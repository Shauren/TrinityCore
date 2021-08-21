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

#include "MapDataExtractor.h"
#include "ClientDataStores.h"
#include "Containers.h"
#include "Definitions.h"
#include "Errors.h"
#include "Log.h"
#include "MapBuilder.h"
#include "VmapBuilder.h"
#include <queue>

namespace Trinity
{
struct MapDataExtractor::MapDependencyInfo
{
    uint32 MapId = 0;
    DataStores::Map const* MapInfo = nullptr;
    uint32 NumberOfMapsThatDependOnThis = 0;
    uint32 NumberOfMapsThatThisDependsOn = 0;
};

void MapDataExtractor::Run()
{
    if (_extractMaps && _extractVmaps)
        TC_LOG_INFO("extractor.maps", "Extracting map and vmap files...");
    else if (_extractMaps)
        TC_LOG_INFO("extractor.maps", "Extracting map files...");
    else if (_extractVmaps)
        TC_LOG_INFO("extractor.maps", "Extracting vmap files...");

    std::map<uint32, std::vector<MapDependencyInfo>> steps = PrepareExtractionSteps();
    std::unordered_map<uint32, WDTData> wdts;

    // Preload all WDT files
    for (std::pair<uint32 const, std::vector<MapDependencyInfo>> const& step : steps)
    {
        for (MapDependencyInfo const& map : step.second)
        {
            auto itr = wdts.emplace(std::piecewise_construct, std::forward_as_tuple(map.MapId), std::forward_as_tuple(map.MapId, map.MapInfo)).first;
            if (!itr->second.LoadFile(_storage, map.MapInfo->WdtFileDataID))
            {
                wdts.erase(itr);
                continue;
            }

            if (_extractVmaps && map.NumberOfMapsThatDependOnThis)
                itr->second.CachedADTs.emplace(); // Map will be used as parent for another map, enable ADT cache
        }
    }

    std::vector<std::unique_ptr<BaseBuilder>> builders;
    if (_extractMaps)
    {
        CreateDir(_outputPath / "maps");

        builders.emplace_back(new MapBuilder(_outputPath, _build, _allowMapHeightCompression));
    }

    if (_extractVmaps)
    {
        CreateDir(_outputPath / "Buildings");
        CreateDir(_outputPath / "vmaps");

        builders.emplace_back(new VmapBuilder(_outputPath, _includeAllVmapVertices));
    }

    Context context;
    std::map<uint32, std::queue<BaseBuilder::Task>> tasks;
    for (std::pair<uint32 const, std::vector<MapDependencyInfo>> const& step : steps)
    {
        // Prepare all extraction tasks
        for (MapDependencyInfo const& map : step.second)
        {
            WDTData* wdt = Containers::MapGetValuePtr(wdts, map.MapId);
            if (!wdt)
                continue;

            for (std::unique_ptr<BaseBuilder>& builder : builders)
            {
                if (BaseBuilder::Task task = builder->CreateWDTTask(context, wdt))
                {
                    tasks[step.first].push(std::move(task));
                    ++context.TotalTasks;
                }
            }

            Chunks::MAIN const* main = wdt->FileData.GetSingleChunk<Chunks::MAIN>()->Data;
            for (int32 y = 0; y < WDT::ADTS_PER_WDT; ++y)
            {
                for (int32 x = 0; x < WDT::ADTS_PER_WDT; ++x)
                {
                    if (!main || !main->Data[y][x].Flag.HasFlag(Chunks::MAIN_Flag::HasADT))
                        continue;

                    for (std::unique_ptr<BaseBuilder>& builder : builders)
                    {
                        if (BaseBuilder::Task task = builder->CreateADTTask(context, wdt, x, y))
                        {
                            tasks[step.first].push(std::move(task));
                            ++context.TotalTasks;
                        }
                    }
                }
            }

            for (std::unique_ptr<BaseBuilder>& builder : builders)
            {
                if (BaseBuilder::Task task = builder->CreateWDTFinalizeTask(context, wdt))
                {
                    tasks[step.first].push(std::move(task));
                    ++context.TotalTasks;
                }
            }
        }
    }

    for (std::pair<uint32 const, std::queue<BaseBuilder::Task>>& tasksForStep : tasks)
    {
        while (!tasksForStep.second.empty())
        {
            ++context.ProcessedTasks;
            tasksForStep.second.front()();
            tasksForStep.second.pop();
        }
    }
}

// Prepares extraction steps:
// * first maps that have no dependencies
// * then maps that depend on those maps (with one dependency level)
// * then maps that depend on previous maps (two levels of dependencies)
// * ... and so on
std::map<uint32, std::vector<MapDataExtractor::MapDependencyInfo>> MapDataExtractor::PrepareExtractionSteps()
{
    std::unordered_map<uint32 /*mapId*/, MapDependencyInfo> mapDependencies;

    for (std::pair<uint32 const, DataStores::Map> const& map : DataStores::Maps)
    {
        // Skip maps that don't have a WDT file
        if (!map.second.WdtFileDataID)
            continue;

        MapDependencyInfo& mapInfo = mapDependencies[map.first];
        mapInfo.MapId = map.first;
        mapInfo.MapInfo = &map.second;

        int16 parentMapId = map.second.ParentMapID;
        while (parentMapId >= 0)
        {
            ++mapInfo.NumberOfMapsThatThisDependsOn;

            if (DataStores::Map const* parentMap = Containers::MapGetValuePtr(DataStores::Maps, parentMapId))
            {
                ++mapDependencies[parentMapId].NumberOfMapsThatDependOnThis;
                parentMapId = parentMap->ParentMapID;
            }
        }
    }

    std::map<uint32, std::vector<MapDependencyInfo>> steps;
    for (std::pair<uint32 const, MapDependencyInfo> const& mapInfo : mapDependencies)
        steps[mapInfo.second.NumberOfMapsThatThisDependsOn].push_back(mapInfo.second);

    for (std::pair<uint32 const, std::vector<MapDependencyInfo>>& step : steps)
    {
        std::sort(step.second.begin(), step.second.end(), [&](MapDependencyInfo const& map1, MapDependencyInfo const& map2)
        {
            return map1.NumberOfMapsThatDependOnThis > map2.NumberOfMapsThatDependOnThis;
        });
    }

    return steps;
}

bool MapDataExtractor::WDTData::LoadFile(std::shared_ptr<ClientStorage::Storage> storage, ClientStorage::FileId const& id)
{
    Storage = storage;
    return FileData.ParseFile(storage, id, { "MPHD", "MAIN", "MAID", "MWMO", "MODF" });
}

std::shared_ptr<MapDataExtractor::ADTData> MapDataExtractor::WDTData::GetADT(int32 x, int32 y)
{
    using namespace Chunks;

    ASSERT(x >= 0 && x < WDT::ADTS_PER_WDT&& y >= 0 && y < WDT::ADTS_PER_WDT);

    if (CachedADTs && (*CachedADTs)[x][y])
        return (*CachedADTs)[x][y];

    MAIN const* main = FileData.GetSingleChunk<MAIN>()->Data;
    if (!main || !main->Data[y][x].Flag.HasFlag(MAIN_Flag::HasADT))
        return nullptr;

    ClientStorage::FileId fileId;
    if (FileData.GetSingleChunk<MPHD>()->Data->Flags.HasFlag(MPHD_Flag::HasMAID))
        fileId = FileData.GetSingleChunk<MAID>()->Data->Data[y][x].RootADT;
    else
        fileId = Trinity::StringFormat(R"(World\Maps\%s\%s_%d_%d.adt)", MapInfo->Directory.c_str(), MapInfo->Directory.c_str(), x, y);

    std::shared_ptr<ADTData> adt = std::make_shared<ADTData>();
    if (!adt->LoadFile(Storage, fileId, { "MCNK", "MCVT", "MCLQ", "MH2O", "MFBO" }))
        return nullptr;

    if (FileData.GetSingleChunk<MPHD>()->Data->Flags.HasFlag(MPHD_Flag::HasMAID))
        fileId = FileData.GetSingleChunk<MAID>()->Data->Data[y][x].Obj0ADT;
    else
        fileId = Trinity::StringFormat(R"(World\Maps\%s\%s_%d_%d_obj0.adt)", MapInfo->Directory.c_str(), MapInfo->Directory.c_str(), x, y);

    if (!adt->LoadFile(Storage, fileId, { "MMDX", "MDDF", "MWMO", "MODF" }))
        return nullptr;

    if (CachedADTs)
    {
        // Map will be used as parent for another map, enable vmap output cache and store adt for future
        adt->CachedVmapOutput.emplace();
        (*CachedADTs)[x][y] = adt;
    }

    return adt;
}

bool MapDataExtractor::ADTData::LoadFile(std::shared_ptr<ClientStorage::Storage> storage, ClientStorage::FileId const& id, std::unordered_set<std::string> const& parsedChunks)
{
    return FileData.ParseFile(storage, id, parsedChunks);
}

namespace Chunks
{
MH2O::Instance const* MH2O::GetLiquidInstance(uint32 layer, int32 x, int32 y) const
{
    if (layer < chunks[x][y].LayerCount && chunks[x][y].OffsetInstances)
        return &reinterpret_cast<Instance const*>(reinterpret_cast<uint8 const*>(this) + chunks[x][y].OffsetInstances)[layer];

    return nullptr;
}

MH2O::Attributes MH2O::GetLiquidAttributes(int32 x, int32 y) const
{
    if (chunks[x][y].LayerCount > 0)
    {
        if (chunks[x][y].OffsetAttributes)
            return *reinterpret_cast<Attributes const*>(reinterpret_cast<uint8 const*>(this) + chunks[x][y].OffsetAttributes);

        return { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF };
    }

    return { 0, 0 };
}

uint16 MH2O::GetLiquidType(Instance const* h) const
{
    if (GetLiquidVertexFormat(h) == LiquidVertexFormatType::Depth)
        return 2;

    return h->LiquidType;
}

float MH2O::GetLiquidHeight(Instance const* h, int32 pos) const
{
    if (!h->OffsetVertexData)
        return 0.0f;
    if (GetLiquidVertexFormat(h) == LiquidVertexFormatType::Depth)
        return 0.0f;

    switch (GetLiquidVertexFormat(h))
    {
        case LiquidVertexFormatType::HeightDepth:
        case LiquidVertexFormatType::HeightTextureCoord:
        case LiquidVertexFormatType::HeightDepthTextureCoord:
            return reinterpret_cast<float const*>(reinterpret_cast<uint8 const*>(this) + h->OffsetVertexData)[pos];
        case LiquidVertexFormatType::Depth:
            return 0.0f;
        case LiquidVertexFormatType::Unk4:
        case LiquidVertexFormatType::Unk5:
            return reinterpret_cast<float const*>(reinterpret_cast<uint8 const*>(this) + h->OffsetVertexData + 4)[pos * 2];
        default:
            break;
    }

    return 0.0f;
}

int8 MH2O::GetLiquidDepth(Instance const* h, int32 pos) const
{
    if (!h->OffsetVertexData)
        return -1;

    switch (GetLiquidVertexFormat(h))
    {
        case LiquidVertexFormatType::HeightDepth:
            return reinterpret_cast<int8 const*>(reinterpret_cast<uint8 const*>(this) + h->OffsetVertexData + (h->GetWidth() + 1) * (h->GetHeight() + 1) * 4)[pos];
        case LiquidVertexFormatType::HeightTextureCoord:
            return 0;
        case LiquidVertexFormatType::Depth:
            return reinterpret_cast<int8 const*>(reinterpret_cast<uint8 const*>(this) + h->OffsetVertexData)[pos];
        case LiquidVertexFormatType::HeightDepthTextureCoord:
            return reinterpret_cast<int8 const*>(reinterpret_cast<uint8 const*>(this) + h->OffsetVertexData + (h->GetWidth() + 1) * (h->GetHeight() + 1) * 8)[pos];
        case LiquidVertexFormatType::Unk4:
            return reinterpret_cast<int8 const*>(reinterpret_cast<uint8 const*>(this) + h->OffsetVertexData)[pos * 8];
        case LiquidVertexFormatType::Unk5:
            return 0;
        default:
            break;
    }
    return 0;
}

uint16 const* MH2O::GetLiquidTextureCoordMap(Instance const* h, int32 pos) const
{
    if (!h->OffsetVertexData)
        return nullptr;

    switch (GetLiquidVertexFormat(h))
    {
        case LiquidVertexFormatType::HeightDepth:
        case LiquidVertexFormatType::Depth:
        case LiquidVertexFormatType::Unk4:
            return nullptr;
        case LiquidVertexFormatType::HeightTextureCoord:
        case LiquidVertexFormatType::HeightDepthTextureCoord:
            return reinterpret_cast<uint16 const*>(reinterpret_cast<uint8 const*>(this) + h->OffsetVertexData + 4 * ((h->GetWidth() + 1) * (h->GetHeight() + 1) + pos));
        case LiquidVertexFormatType::Unk5:
            return reinterpret_cast<uint16 const*>(reinterpret_cast<uint8 const*>(this) + h->OffsetVertexData + 8 * ((h->GetWidth() + 1) * (h->GetHeight() + 1) + pos));
        default:
            break;
    }
    return nullptr;
}

uint64 MH2O::GetLiquidExistsBitmap(Instance const* h) const
{
    if (h->OffsetExistsBitmap)
        return *reinterpret_cast<uint64 const*>(reinterpret_cast<uint8 const*>(this) + h->OffsetExistsBitmap);

    return 0xFFFFFFFFFFFFFFFFuLL;
}

MH2O::LiquidVertexFormatType MH2O::GetLiquidVertexFormat(Instance const* liquidInstance) const
{
    if (liquidInstance->LiquidVertexFormat < 42)
        return static_cast<LiquidVertexFormatType>(liquidInstance->LiquidVertexFormat);

    if (liquidInstance->LiquidType == 2)
        return LiquidVertexFormatType::Depth;

    auto liquidType = DataStores::LiquidTypes.find(liquidInstance->LiquidType);
    if (liquidType != DataStores::LiquidTypes.end())
    {
        auto liquidMaterial = DataStores::LiquidMaterials.find(liquidType->second.MaterialID);
        if (liquidMaterial != DataStores::LiquidMaterials.end())
            return static_cast<LiquidVertexFormatType>(liquidMaterial->second.LVF);
    }

    return static_cast<LiquidVertexFormatType>(-1);
}
}
}
