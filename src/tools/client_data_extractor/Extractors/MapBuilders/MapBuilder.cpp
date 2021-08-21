/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "MapBuilder.h"
#include "ClientDataStores.h"
#include "Log.h"
#include <fstream>

namespace Trinity
{
namespace
{
    // This allows to limit minimum height to some value (memory saving)
    constexpr bool  ALLOW_HEIGHT_LIMIT = true;
    constexpr float MIN_STORED_HEIGHT = -2000.0f;

    constexpr float FLOAT_TO_INT8_LIMIT  = 2.0f;      // Max accuracy = val/256
    constexpr float FLOAT_TO_INT16_LIMIT = 2048.0f;   // Max accuracy = val/65536
    constexpr float FLAT_HEIGHT_DELTA_LIMIT = 0.005f; // If (maxHeight - minHeight) is lower than this value - surface is treated as flat and heights are not stored
    constexpr float FLAT_LIQUID_DELTA_LIMIT = 0.001f; // If (maxLiquidHeight - minLiquidHeight) is lower than this value - liquid surface is flat and heights are not stored

    struct WDTTaskData
    {
        std::array<char, WDT::ADTS_PER_WDT * WDT::ADTS_PER_WDT> SuccessfullyExtractedTiles = { };

        WDTTaskData()
        {
            SuccessfullyExtractedTiles.fill('0');
        }
    };

    template<typename T>
    struct TerrainHeightData
    {
        T V8[ADT::GRID_SIZE][ADT::GRID_SIZE] = { };
        T V9[ADT::GRID_SIZE + 1][ADT::GRID_SIZE + 1] = { };
    };
}

MapDataExtractor::BaseBuilder::Task MapBuilder::CreateWDTTask(MapDataExtractor::Context& /*context*/, MapDataExtractor::WDTData* wdtData)
{
    wdtData->TaskData[typeid(MapBuilder)] = WDTTaskData();
    return nullptr;
}

MapDataExtractor::BaseBuilder::Task MapBuilder::CreateADTTask(MapDataExtractor::Context& context, MapDataExtractor::WDTData* wdtData, int32 x, int32 y)
{
    return [&context, this, wdtData, x, y]()
    {
        boost::filesystem::path outputPath = Trinity::StringFormat("%s/maps/%04u_%02u_%02u.map", _outputPath.string().c_str(), wdtData->MapId, y, x);

        TC_LOG_INFO("extractor.maps.progress", "[%3u%%] Processing %s", uint32(context.ProcessedTasks) * 100 / uint32(context.TotalTasks), outputPath.filename().string().c_str());

        if (std::shared_ptr<MapDataExtractor::ADTData> adtData = wdtData->GetADT(x, y))
        {
            WDTTaskData& taskData = boost::any_cast<WDTTaskData&>(wdtData->TaskData[typeid(MapBuilder)]);
            bool ignoreDeepWater = IsDeepWaterIgnored(wdtData->MapId, y, x);
            taskData.SuccessfullyExtractedTiles[y * WDT::ADTS_PER_WDT + x] = ProcessADT(adtData, outputPath, ignoreDeepWater) ? '1' : '0';
        }
    };
}

MapDataExtractor::BaseBuilder::Task MapBuilder::CreateWDTFinalizeTask(MapDataExtractor::Context& /*context*/, MapDataExtractor::WDTData* wdtData)
{
    return [this, wdtData]()
    {
        if (FILE* tileList = fopen(Trinity::StringFormat("%s/maps/%04u.tilelist", _outputPath.string().c_str(), wdtData->MapId).c_str(), "wb"))
        {
            WDTTaskData& taskData = boost::any_cast<WDTTaskData&>(wdtData->TaskData[typeid(MapBuilder)]);

            fwrite(MapMagic.data(), 1, MapMagic.size(), tileList);
            fwrite(MapVersionMagic.data(), 1, MapVersionMagic.size(), tileList);
            fwrite(&_build, sizeof(_build), 1, tileList);
            fwrite(taskData.SuccessfullyExtractedTiles.data(), 1, taskData.SuccessfullyExtractedTiles.size(), tileList);
            fclose(tileList);
        }
    };
}

void MapBuilder::FillHeightData(Chunk<Chunks::MCNK> const& mcnkChunk,
    float (&V9)[ADT::GRID_SIZE + 1][ADT::GRID_SIZE + 1],
    float (&V8)[ADT::GRID_SIZE][ADT::GRID_SIZE]) const
{
    using namespace Chunks;

    MCNK const* mcnk = mcnkChunk.Data;

    // Height values for triangles stored in order:
    // 1     2     3     4     5     6     7     8     9
    //    10    11    12    13    14    15    16    17
    // 18    19    20    21    22    23    24    25    26
    //    27    28    29    30    31    32    33    34
    // . . . . . . . .
    // For better get height values merge it to V9 and V8 map
    // V9 height map:
    // 1     2     3     4     5     6     7     8     9
    // 18    19    20    21    22    23    24    25    26
    // . . . . . . . .
    // V8 height map:
    //    10    11    12    13    14    15    16    17
    //    27    28    29    30    31    32    33    34
    // . . . . . . . .

    // Set map height as grid height
    for (uint32 y = 0; y <= ADT::CELL_SIZE; ++y)
    {
        uint32 cy = mcnk->IndexY * ADT::CELL_SIZE + y;
        for (uint32 x = 0; x <= ADT::CELL_SIZE; ++x)
        {
            uint32 cx = mcnk->IndexX * ADT::CELL_SIZE + x;
            V9[cy][cx] = mcnk->position.z;
        }
    }

    for (uint32 y = 0; y < ADT::CELL_SIZE; ++y)
    {
        uint32 cy = mcnk->IndexY * ADT::CELL_SIZE + y;
        for (uint32 x = 0; x < ADT::CELL_SIZE; ++x)
        {
            uint32 cx = mcnk->IndexX * ADT::CELL_SIZE + x;
            V8[cy][cx] = mcnk->position.z;
        }
    }

    // Get custom height
    if (Chunk<MCVT> const* mcvtChunk = mcnkChunk.Subchunks.GetSingleChunk<MCVT>())
    {
        MCVT const* mcvt = mcvtChunk->Data;
        // get V9 height map
        for (uint32 y = 0; y <= ADT::CELL_SIZE; ++y)
        {
            uint32 cy = mcnk->IndexY * ADT::CELL_SIZE + y;
            for (uint32 x = 0; x <= ADT::CELL_SIZE; ++x)
            {
                uint32 cx = mcnk->IndexX * ADT::CELL_SIZE + x;
                V9[cy][cx] += mcvt->Height[y * (ADT::CELL_SIZE * 2 + 1) + x];
            }
        }
        // get V8 height map
        for (uint32 y = 0; y < ADT::CELL_SIZE; ++y)
        {
            uint32 cy = mcnk->IndexY * ADT::CELL_SIZE + y;
            for (uint32 x = 0; x < ADT::CELL_SIZE; ++x)
            {
                uint32 cx = mcnk->IndexX * ADT::CELL_SIZE + x;
                V8[cy][cx] += mcvt->Height[y * (ADT::CELL_SIZE * 2 + 1) + ADT::CELL_SIZE + 1 + x];
            }
        }
    }
}

void MapBuilder::FillLegacyLiquidData(Chunk<Chunks::MCNK> const& mcnkChunk, bool ignoreDeepWater, boost::filesystem::path const& outputPath,
    bool (&liquidShow)[ADT::GRID_SIZE][ADT::GRID_SIZE],
    uint16 (&liquidId)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID],
    map_liquidHeaderTypeFlags (&liquidFlags)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID],
    float (&liquidHeight)[ADT::GRID_SIZE + 1][ADT::GRID_SIZE + 1]) const
{
    using namespace Chunks;

    if (Chunk<MCLQ> const* mclqChunk = mcnkChunk.Subchunks.GetSingleChunk<MCLQ>())
    {
        MCNK const* mcnk = mcnkChunk.Data;
        MCLQ const* liquid = mclqChunk->Data;
        int32 count = 0;
        for (uint32 y = 0; y < ADT::CELL_SIZE; ++y)
        {
            uint32 cy = mcnk->IndexY * ADT::CELL_SIZE + y;
            for (uint32 x = 0; x < ADT::CELL_SIZE; ++x)
            {
                uint32 cx = mcnk->IndexX * ADT::CELL_SIZE + x;
                if (liquid->flags[y][x] != 0x0F)
                {
                    liquidShow[cy][cx] = true;
                    if (!ignoreDeepWater && liquid->flags[y][x] & (1 << 7))
                        liquidFlags[mcnk->IndexY][mcnk->IndexX] |= map_liquidHeaderTypeFlags::DarkWater;
                    ++count;
                }
            }
        }

        if (mcnk->Flags.HasFlag(MCNK_Flag::RiverMCLQ))
        {
            liquidId[mcnk->IndexY][mcnk->IndexX] = 1;
            liquidFlags[mcnk->IndexY][mcnk->IndexX] |= map_liquidHeaderTypeFlags::Water; // water
        }
        if (mcnk->Flags.HasFlag(MCNK_Flag::OceanMCLQ))
        {
            liquidId[mcnk->IndexY][mcnk->IndexX] = 2;
            liquidFlags[mcnk->IndexY][mcnk->IndexX] |= map_liquidHeaderTypeFlags::Ocean; // ocean
        }
        if (mcnk->Flags.HasFlag(MCNK_Flag::MagmaMCLQ))
        {
            liquidId[mcnk->IndexY][mcnk->IndexX] = 3;
            liquidFlags[mcnk->IndexY][mcnk->IndexX] |= map_liquidHeaderTypeFlags::Magma; // magma/slime
        }

        if (!count && liquidFlags[mcnk->IndexY][mcnk->IndexX] != map_liquidHeaderTypeFlags::NoWater)
            TC_LOG_ERROR("extractor.maps", "Failed to detect liquid type from MCLQ chunk when creating %s, chunk %u, %u",
                outputPath.filename().string().c_str(), mcnk->IndexY, mcnk->IndexX);

        for (int32 y = 0; y <= ADT::CELL_SIZE; ++y)
        {
            int32 cy = mcnk->IndexY * ADT::CELL_SIZE + y;
            for (int32 x = 0; x <= ADT::CELL_SIZE; ++x)
            {
                int32 cx = mcnk->IndexX * ADT::CELL_SIZE + x;
                liquidHeight[cy][cx] = liquid->liquid[y][x].height;
            }
        }
    }
}

void MapBuilder::FillLiquidData(Chunks::MH2O const* mh2o, bool ignoreDeepWater, boost::filesystem::path const& outputPath,
    bool (&liquidShow)[ADT::GRID_SIZE][ADT::GRID_SIZE],
    uint16 (&liquidId)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID],
    map_liquidHeaderTypeFlags (&liquidFlags)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID],
    float (&liquidHeight)[ADT::GRID_SIZE + 1][ADT::GRID_SIZE + 1]) const
{
    using namespace Chunks;

    for (int32 i = 0; i < ADT::CELLS_PER_GRID; i++)
    {
        for (int32 j = 0; j < ADT::CELLS_PER_GRID; j++)
        {
            MH2O::Instance const* h = mh2o->GetLiquidInstance(0, i, j);
            if (!h)
                continue;

            MH2O::Attributes attrs = mh2o->GetLiquidAttributes(i, j);

            int32 count = 0;
            uint64 existsMask = mh2o->GetLiquidExistsBitmap(h);
            for (int32 y = 0; y < h->GetHeight(); y++)
            {
                int32 cy = i * ADT::CELL_SIZE + y + h->GetOffsetY();
                for (int32 x = 0; x < h->GetWidth(); x++)
                {
                    int32 cx = j * ADT::CELL_SIZE + x + h->GetOffsetX();
                    if (existsMask & 1)
                    {
                        liquidShow[cy][cx] = true;
                        ++count;
                    }
                    existsMask >>= 1;
                }
            }

            liquidId[i][j] = mh2o->GetLiquidType(h);
            switch (DataStores::LiquidTypes.at(liquidId[i][j]).SoundBank)
            {
                case DataStores::LiquidTypeSoundBank::Water: liquidFlags[i][j] |= map_liquidHeaderTypeFlags::Water; break;
                case DataStores::LiquidTypeSoundBank::Ocean: liquidFlags[i][j] |= map_liquidHeaderTypeFlags::Ocean; if (!ignoreDeepWater && attrs.Deep) liquidFlags[i][j] |= map_liquidHeaderTypeFlags::DarkWater; break;
                case DataStores::LiquidTypeSoundBank::Magma: liquidFlags[i][j] |= map_liquidHeaderTypeFlags::Magma; break;
                case DataStores::LiquidTypeSoundBank::Slime: liquidFlags[i][j] |= map_liquidHeaderTypeFlags::Slime; break;
                default:
                    TC_LOG_ERROR("extractor.maps", "Can't find Liquid type %u for map %s; chunk %d,%d",
                        h->LiquidType, outputPath.filename().string().c_str(), i, j);
                    break;
            }

            if (!count && liquidFlags[i][j] != map_liquidHeaderTypeFlags::NoWater)
                TC_LOG_ERROR("extractor.maps", "Failed to detect liquid type from MH2O chunk when creating %s, chunk %u, %u",
                    outputPath.filename().string().c_str(), i, j);

            int32 pos = 0;
            for (int32 y = 0; y <= h->GetHeight(); y++)
            {
                int32 cy = i * ADT::CELL_SIZE + y + h->GetOffsetY();
                for (int32 x = 0; x <= h->GetWidth(); x++)
                {
                    int32 cx = j * ADT::CELL_SIZE + x + h->GetOffsetX();
                    liquidHeight[cy][cx] = mh2o->GetLiquidHeight(h, pos);
                    pos++;
                }
            }
        }
    }
}

bool MapBuilder::FillHoleData(Chunks::MCNK const* mcnk, uint8 (&holes)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID][8]) const
{
    if (!mcnk->Flags.HasFlag(Chunks::MCNK_Flag::HasHighResHoles))
    {
        if (uint16 hole = mcnk->LowResHoles)
            if (TransformToHighRes(hole, holes[mcnk->IndexY][mcnk->IndexX]))
                return true;
    }
    else
    {
        memcpy(holes[mcnk->IndexY][mcnk->IndexX], mcnk->union_5_3_0.HighResHoles, sizeof(uint64));
        if (*((uint64*)holes[mcnk->IndexY][mcnk->IndexX]) != 0)
            return true;
    }

    return false;
}

bool MapBuilder::FillMapBoundsData(Chunks::MFBO const* mfbo, int16 (&flightBoxMax)[3][3], int16 (&flightBoxMin)[3][3]) const
{
    memcpy(flightBoxMax, &mfbo->max, sizeof(flightBoxMax));
    memcpy(flightBoxMin, &mfbo->min, sizeof(flightBoxMin));
    return true;
}

bool MapBuilder::ProcessADT(std::shared_ptr<MapDataExtractor::ADTData> adtData, boost::filesystem::path const& outputPath, bool ignoreDeepWater) const
{
    using namespace Chunks;

    // Prepare map header
    map_fileheader map = { };
    map.mapMagic = MapMagic;
    map.versionMagic = MapVersionMagic;
    map.buildMagic = _build;

    // Get area flags data
    uint16 areaIds[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID] = { };

    TerrainHeightData<float> terrainHeights;
    std::unique_ptr<TerrainHeightData<uint16>> packedTerrainHeightsAsUInt16;
    std::unique_ptr<TerrainHeightData<uint8>> packedTerrainHeightsAsUInt8;

    bool liquidShow[ADT::GRID_SIZE][ADT::GRID_SIZE] = { };
    uint16 liquidId[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID] = { };
    map_liquidHeaderTypeFlags liquidFlags[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID] = { };
    float liquidHeight[ADT::GRID_SIZE + 1][ADT::GRID_SIZE + 1] = { };

    bool hasHoles = false;
    uint8 holes[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID][8] = { };

    bool hasFlightBox = false;
    int16 flightBoxMax[3][3] = { };
    int16 flightBoxMin[3][3] = { };

    for (Chunk<MCNK> const& mcnkChunk : *adtData->FileData.GetChunks<MCNK>())
    {
        MCNK const* mcnk = mcnkChunk.Data;

        // Area data
        areaIds[mcnk->IndexY][mcnk->IndexX] = mcnk->areaid;

        // Height
        FillHeightData(mcnkChunk, terrainHeights.V9, terrainHeights.V8);

        // Liquid data
        if (mcnk->sizeMCLQ > 8)
            FillLegacyLiquidData(mcnkChunk, ignoreDeepWater, outputPath, liquidShow, liquidId, liquidFlags, liquidHeight);

        // Hole data
        if (FillHoleData(mcnk, holes))
            hasHoles = true;
    }

    // Get liquid map for grid (in WOTLK used MH2O chunk)
    if (Chunk<MH2O> const* mh2oChunk = adtData->FileData.GetSingleChunk<MH2O>())
        FillLiquidData(mh2oChunk->Data, ignoreDeepWater, outputPath, liquidShow, liquidId, liquidFlags, liquidHeight);

    if (Chunk<MFBO> const* mfbo = adtData->FileData.GetSingleChunk<MFBO>())
        hasFlightBox = FillMapBoundsData(mfbo->Data, flightBoxMax, flightBoxMin);

    //============================================
    // Try pack area data
    //============================================
    bool fullAreaData = false;
    uint16 areaId = areaIds[0][0];
    for (int y = 0; y < ADT::CELLS_PER_GRID; ++y)
    {
        for (int x = 0; x < ADT::CELLS_PER_GRID; ++x)
        {
            if (areaIds[y][x] != areaId)
            {
                fullAreaData = true;
                break;
            }
        }
    }

    map.areaMapOffset = sizeof(map);
    map.areaMapSize = sizeof(map_areaHeader);

    map_areaHeader areaHeader;
    areaHeader.areaMagic = MapAreaMagic;
    areaHeader.flags = map_areaHeaderFlags::None;
    if (fullAreaData)
    {
        areaHeader.gridArea = 0;
        map.areaMapSize += sizeof(areaIds);
    }
    else
    {
        areaHeader.flags |= map_areaHeaderFlags::NoArea;
        areaHeader.gridArea = areaId;
    }

    //============================================
    // Try pack height data
    //============================================
    float maxHeight = -20000;
    float minHeight = 20000;
    for (uint32 y = 0; y < ADT::GRID_SIZE; ++y)
    {
        for (uint32 x = 0; x < ADT::GRID_SIZE; ++x)
        {
            float h = terrainHeights.V8[y][x];
            if (maxHeight < h) maxHeight = h;
            if (minHeight > h) minHeight = h;
        }
    }
    for (uint32 y = 0; y <= ADT::GRID_SIZE; ++y)
    {
        for (uint32 x = 0; x <= ADT::GRID_SIZE; ++x)
        {
            float h = terrainHeights.V9[y][x];
            if (maxHeight < h) maxHeight = h;
            if (minHeight > h) minHeight = h;
        }
    }

    // Check for allow limit minimum height (not store height in deep ochean - allow save some memory)
    if (ALLOW_HEIGHT_LIMIT && minHeight < MIN_STORED_HEIGHT)
    {
        for (uint32 y = 0; y < ADT::GRID_SIZE; ++y)
            for (uint32 x = 0; x < ADT::GRID_SIZE; ++x)
                if (terrainHeights.V8[y][x] < MIN_STORED_HEIGHT)
                    terrainHeights.V8[y][x] = MIN_STORED_HEIGHT;

        for (uint32 y = 0; y <= ADT::GRID_SIZE; ++y)
            for (uint32 x = 0; x <= ADT::GRID_SIZE; ++x)
                if (terrainHeights.V9[y][x] < MIN_STORED_HEIGHT)
                    terrainHeights.V9[y][x] = MIN_STORED_HEIGHT;

        if (minHeight < MIN_STORED_HEIGHT)
            minHeight = MIN_STORED_HEIGHT;

        if (maxHeight < MIN_STORED_HEIGHT)
            maxHeight = MIN_STORED_HEIGHT;
    }

    map.heightMapOffset = map.areaMapOffset + map.areaMapSize;
    map.heightMapSize = sizeof(map_heightHeader);

    map_heightHeader heightHeader;
    heightHeader.heightMagic = MapHeightMagic;
    heightHeader.flags = map_heightHeaderFlags::None;
    heightHeader.gridHeight = minHeight;
    heightHeader.gridMaxHeight = maxHeight;

    if (maxHeight == minHeight)
        heightHeader.flags |= map_heightHeaderFlags::NoHeight;

    // Not need store if flat surface
    if (_allowMapHeightCompression && (maxHeight - minHeight) < FLAT_HEIGHT_DELTA_LIMIT)
        heightHeader.flags |= map_heightHeaderFlags::NoHeight;

    if (hasFlightBox)
    {
        heightHeader.flags |= map_heightHeaderFlags::HasFlightBounds;
        map.heightMapSize += sizeof(flightBoxMax) + sizeof(flightBoxMin);
    }

    // Try store as packed in uint16 or uint8 values
    if (!heightHeader.flags.HasFlag(map_heightHeaderFlags::NoHeight))
    {
        float step = 0;
        // Try Store as uint values
        if (_allowMapHeightCompression)
        {
            float diff = maxHeight - minHeight;
            if (diff < FLOAT_TO_INT8_LIMIT)      // As uint8 (max accuracy = CONF_float_to_int8_limit/256)
            {
                heightHeader.flags |= map_heightHeaderFlags::HeightAsInt8;
                step = SelectUInt8StepStore(diff);
            }
            else if (diff < FLOAT_TO_INT16_LIMIT)  // As uint16 (max accuracy = CONF_float_to_int16_limit/65536)
            {
                heightHeader.flags |= map_heightHeaderFlags::HeightAsInt16;
                step = SelectUInt16StepStore(diff);
            }
        }

        // Pack it to int values if need
        if (heightHeader.flags.HasFlag(map_heightHeaderFlags::HeightAsInt8))
        {
            packedTerrainHeightsAsUInt8 = std::make_unique<TerrainHeightData<uint8>>();

            for (int32 y = 0; y < ADT::GRID_SIZE; y++)
                for (int32 x = 0; x < ADT::GRID_SIZE; x++)
                    packedTerrainHeightsAsUInt8->V8[y][x] = uint8((terrainHeights.V8[y][x] - minHeight) * step + 0.5f);

            for (int32 y = 0; y <= ADT::GRID_SIZE; y++)
                for (int32 x = 0; x <= ADT::GRID_SIZE; x++)
                    packedTerrainHeightsAsUInt8->V9[y][x] = uint8((terrainHeights.V9[y][x] - minHeight) * step + 0.5f);

            map.heightMapSize += sizeof(packedTerrainHeightsAsUInt8->V9) + sizeof(packedTerrainHeightsAsUInt8->V8);
        }
        else if (heightHeader.flags.HasFlag(map_heightHeaderFlags::HeightAsInt16))
        {
            packedTerrainHeightsAsUInt16 = std::make_unique<TerrainHeightData<uint16>>();

            for (int32 y = 0; y < ADT::GRID_SIZE; y++)
                for (int32 x = 0; x < ADT::GRID_SIZE; x++)
                    packedTerrainHeightsAsUInt16->V8[y][x] = uint16((terrainHeights.V8[y][x] - minHeight) * step + 0.5f);

            for (int32 y = 0; y <= ADT::GRID_SIZE; y++)
                for (int32 x = 0; x <= ADT::GRID_SIZE; x++)
                    packedTerrainHeightsAsUInt16->V9[y][x] = uint16((terrainHeights.V9[y][x] - minHeight) * step + 0.5f);

            map.heightMapSize += sizeof(packedTerrainHeightsAsUInt16->V9) + sizeof(packedTerrainHeightsAsUInt16->V8);
        }
        else
            map.heightMapSize += sizeof(terrainHeights.V9) + sizeof(terrainHeights.V8);
    }

    //============================================
    // Pack liquid data
    //============================================
    uint16 firstLiquidType = liquidId[0][0];
    map_liquidHeaderTypeFlags firstLiquidFlag = liquidFlags[0][0];
    bool fullType = false;
    for (int y = 0; y < ADT::CELLS_PER_GRID; y++)
    {
        for (int x = 0; x < ADT::CELLS_PER_GRID; x++)
        {
            if (liquidId[y][x] != firstLiquidType || liquidFlags[y][x] != firstLiquidFlag)
            {
                fullType = true;
                y = ADT::CELLS_PER_GRID;
                break;
            }
        }
    }

    map_liquidHeader liquidHeader;

    // no water data (if all grid have 0 liquid type)
    if (firstLiquidFlag == map_liquidHeaderTypeFlags::NoWater && !fullType)
    {
        // No liquid data
        map.liquidMapOffset = 0;
        map.liquidMapSize = 0;
    }
    else
    {
        int32 minX = 255, minY = 255;
        int32 maxX = 0, maxY = 0;
        maxHeight = -20000;
        minHeight = 20000;
        for (int32 y = 0; y < ADT::GRID_SIZE; y++)
        {
            for (int32 x = 0; x < ADT::GRID_SIZE; x++)
            {
                if (liquidShow[y][x])
                {
                    if (minX > x) minX = x;
                    if (maxX < x) maxX = x;
                    if (minY > y) minY = y;
                    if (maxY < y) maxY = y;
                    float h = liquidHeight[y][x];
                    if (maxHeight < h) maxHeight = h;
                    if (minHeight > h) minHeight = h;
                }
                else
                    liquidHeight[y][x] = MIN_STORED_HEIGHT;
            }
        }
        map.liquidMapOffset = map.heightMapOffset + map.heightMapSize;
        map.liquidMapSize = sizeof(map_liquidHeader);
        liquidHeader.liquidMagic = MapLiquidMagic;
        liquidHeader.flags = map_liquidHeaderFlags::None;
        liquidHeader.liquidType = 0;
        liquidHeader.offsetX = minX;
        liquidHeader.offsetY = minY;
        liquidHeader.width = maxX - minX + 1 + 1;
        liquidHeader.height = maxY - minY + 1 + 1;
        liquidHeader.liquidLevel = minHeight;

        if (maxHeight == minHeight)
            liquidHeader.flags |= map_liquidHeaderFlags::NoHeight;

        // Not need store if flat surface
        if (_allowMapHeightCompression && (maxHeight - minHeight) < FLAT_LIQUID_DELTA_LIMIT)
            liquidHeader.flags |= map_liquidHeaderFlags::NoHeight;

        if (!fullType)
            liquidHeader.flags |= map_liquidHeaderFlags::NoType;

        if (liquidHeader.flags.HasFlag(map_liquidHeaderFlags::NoType))
        {
            liquidHeader.liquidFlags = firstLiquidFlag;
            liquidHeader.liquidType = firstLiquidType;
        }
        else
            map.liquidMapSize += sizeof(liquidId) + sizeof(liquidFlags);

        if (!liquidHeader.flags.HasFlag(map_liquidHeaderFlags::NoHeight))
            map.liquidMapSize += sizeof(float) * liquidHeader.width * liquidHeader.height;
    }

    if (hasHoles)
    {
        if (map.liquidMapOffset)
            map.holesOffset = map.liquidMapOffset + map.liquidMapSize;
        else
            map.holesOffset = map.heightMapOffset + map.heightMapSize;

        map.holesSize = sizeof(holes);
    }
    else
    {
        map.holesOffset = 0;
        map.holesSize = 0;
    }

    // Ok all data prepared - store it
    std::ofstream outFile(outputPath.string(), std::ofstream::out | std::ofstream::binary);
    if (!outFile)
    {
        TC_LOG_ERROR("extractor.maps", "Can't create the output file '%s'", outputPath.string().c_str());
        return false;
    }

    outFile.write(reinterpret_cast<char const*>(&map), sizeof(map));
    // Store area data
    outFile.write(reinterpret_cast<char const*>(&areaHeader), sizeof(areaHeader));
    if (!areaHeader.flags.HasFlag(map_areaHeaderFlags::NoArea))
        outFile.write(reinterpret_cast<char const*>(areaIds), sizeof(areaIds));

    // Store height data
    outFile.write(reinterpret_cast<char const*>(&heightHeader), sizeof(heightHeader));
    if (!heightHeader.flags.HasFlag(map_heightHeaderFlags::NoHeight))
    {
        if (heightHeader.flags.HasFlag(map_heightHeaderFlags::HeightAsInt16))
        {
            outFile.write(reinterpret_cast<char const*>(packedTerrainHeightsAsUInt16->V9), sizeof(packedTerrainHeightsAsUInt16->V9));
            outFile.write(reinterpret_cast<char const*>(packedTerrainHeightsAsUInt16->V8), sizeof(packedTerrainHeightsAsUInt16->V8));
        }
        else if (heightHeader.flags.HasFlag(map_heightHeaderFlags::HeightAsInt8))
        {
            outFile.write(reinterpret_cast<char const*>(packedTerrainHeightsAsUInt8->V9), sizeof(packedTerrainHeightsAsUInt8->V9));
            outFile.write(reinterpret_cast<char const*>(packedTerrainHeightsAsUInt8->V8), sizeof(packedTerrainHeightsAsUInt8->V8));
        }
        else
        {
            outFile.write(reinterpret_cast<char const*>(terrainHeights.V9), sizeof(terrainHeights.V9));
            outFile.write(reinterpret_cast<char const*>(terrainHeights.V8), sizeof(terrainHeights.V8));
        }
    }

    if (heightHeader.flags.HasFlag(map_heightHeaderFlags::HasFlightBounds))
    {
        outFile.write(reinterpret_cast<char*>(flightBoxMax), sizeof(flightBoxMax));
        outFile.write(reinterpret_cast<char*>(flightBoxMin), sizeof(flightBoxMin));
    }

    // Store liquid data if need
    if (map.liquidMapOffset)
    {
        outFile.write(reinterpret_cast<char const*>(&liquidHeader), sizeof(liquidHeader));
        if (!liquidHeader.flags.HasFlag(map_liquidHeaderFlags::NoType))
        {
            outFile.write(reinterpret_cast<char const*>(liquidId), sizeof(liquidId));
            outFile.write(reinterpret_cast<char const*>(liquidFlags), sizeof(liquidFlags));
        }

        if (!liquidHeader.flags.HasFlag(map_liquidHeaderFlags::NoHeight))
            for (int32 y = 0; y < liquidHeader.height; y++)
                outFile.write(reinterpret_cast<char const*>(&liquidHeight[y + liquidHeader.offsetY][liquidHeader.offsetX]), sizeof(float) * liquidHeader.width);
    }

    // store hole data
    if (hasHoles)
        outFile.write(reinterpret_cast<char const*>(holes), map.holesSize);

    outFile.close();

    return true;
}

bool MapBuilder::IsDeepWaterIgnored(uint32 mapId, int32 x, int32 y)
{
    if (mapId == 0)
    {
        //                                                                                                GRID(39, 24) || GRID(39, 25) || GRID(39, 26) ||
        //                                                                                                GRID(40, 24) || GRID(40, 25) || GRID(40, 26) ||
        //GRID(41, 18) || GRID(41, 19) || GRID(41, 20) || GRID(41, 21) || GRID(41, 22) || GRID(41, 23) || GRID(41, 24) || GRID(41, 25) || GRID(41, 26) ||
        //GRID(42, 18) || GRID(42, 19) || GRID(42, 20) || GRID(42, 21) || GRID(42, 22) || GRID(42, 23) || GRID(42, 24) || GRID(42, 25) || GRID(42, 26) ||
        //GRID(43, 18) || GRID(43, 19) || GRID(43, 20) || GRID(43, 21) || GRID(43, 22) || GRID(43, 23) || GRID(43, 24) || GRID(43, 25) || GRID(43, 26) ||
        //GRID(44, 18) || GRID(44, 19) || GRID(44, 20) || GRID(44, 21) || GRID(44, 22) || GRID(44, 23) || GRID(44, 24) || GRID(44, 25) || GRID(44, 26) ||
        //GRID(45, 18) || GRID(45, 19) || GRID(45, 20) || GRID(45, 21) || GRID(45, 22) || GRID(45, 23) || GRID(45, 24) || GRID(45, 25) || GRID(45, 26) ||
        //GRID(46, 18) || GRID(46, 19) || GRID(46, 20) || GRID(46, 21) || GRID(46, 22) || GRID(46, 23) || GRID(46, 24) || GRID(46, 25) || GRID(46, 26)

        // Vashj'ir grids completely ignore fatigue
        return (x >= 39 && x <= 40 && y >= 24 && y <= 26) || (x >= 41 && x <= 46 && y >= 18 && y <= 26);
    }

    if (mapId == 1)
    {
        // GRID(43, 39) || GRID(43, 40)
        // Thousand Needles
        return x == 43 && (y == 39 || y == 40);
    }

    return false;
}

bool MapBuilder::TransformToHighRes(uint16 lowResHoles, uint8 (&hiResHoles)[8])
{
    for (uint8 i = 0; i < 8; i++)
    {
        for (uint8 j = 0; j < 8; j++)
        {
            int32 holeIdxL = (i / 2) * 4 + (j / 2);
            if (((lowResHoles >> holeIdxL) & 1) == 1)
                hiResHoles[i] |= (1 << j);
        }
    }

    return *((uint64*)hiResHoles) != 0;
}

float MapBuilder::SelectUInt8StepStore(float maxDiff)
{
    return 255 / maxDiff;
}

float MapBuilder::SelectUInt16StepStore(float maxDiff)
{
    return 65535 / maxDiff;
}
}
