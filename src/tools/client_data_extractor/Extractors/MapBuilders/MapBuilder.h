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

#ifndef MapBuilder_h__
#define MapBuilder_h__

#include "MapDataExtractor.h"
#include "Definitions.h"
#include "MapDefines.h"

namespace Trinity
{
    class MapBuilder : public MapDataExtractor::BaseBuilder
    {
    public:
        MapBuilder(boost::filesystem::path outputPath, uint32 build, bool allowMapHeightCompression)
            : MapDataExtractor::BaseBuilder(std::move(outputPath)), _build(build),
            _allowMapHeightCompression(allowMapHeightCompression) { }

        Task CreateWDTTask(MapDataExtractor::Context& context, MapDataExtractor::WDTData* wdtData) override;
        Task CreateADTTask(MapDataExtractor::Context& context, MapDataExtractor::WDTData* wdtData, int32 x, int32 y) override;
        Task CreateWDTFinalizeTask(MapDataExtractor::Context& context, MapDataExtractor::WDTData* wdtData) override;

    private:
        bool ProcessADT(std::shared_ptr<MapDataExtractor::ADTData> adtData, boost::filesystem::path const& outputPath, bool ignoreDeepWater) const;

        void FillHeightData(Chunk<Chunks::MCNK> const& mcnkChunk,
            float (&V9)[ADT::GRID_SIZE + 1][ADT::GRID_SIZE + 1],
            float (&V8)[ADT::GRID_SIZE][ADT::GRID_SIZE]) const;

        void FillLegacyLiquidData(Chunk<Chunks::MCNK> const& mcnkChunk, bool ignoreDeepWater, boost::filesystem::path const& outputPath,
            bool (&liquidShow)[ADT::GRID_SIZE][ADT::GRID_SIZE],
            uint16 (&liquidId)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID],
            map_liquidHeaderTypeFlags (&liquidFlags)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID],
            float (&liquidHeight)[ADT::GRID_SIZE + 1][ADT::GRID_SIZE + 1]) const;

        void FillLiquidData(Chunks::MH2O const* mh2o, bool ignoreDeepWater, boost::filesystem::path const& outputPath,
            bool (&liquidShow)[ADT::GRID_SIZE][ADT::GRID_SIZE],
            uint16 (&liquidId)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID],
            map_liquidHeaderTypeFlags (&liquidFlags)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID],
            float (&liquidHeight)[ADT::GRID_SIZE + 1][ADT::GRID_SIZE + 1]) const;

        void FillLiquidData() const;

        bool FillHoleData(Chunks::MCNK const* mcnk, uint8 (&holes)[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID][8]) const;

        bool FillMapBoundsData(Chunks::MFBO const* mfbo, int16 (&flightBoxMax)[3][3], int16 (&flightBoxMin)[3][3]) const;

        static bool IsDeepWaterIgnored(uint32 mapId, int32 x, int32 y);
        static bool TransformToHighRes(uint16 lowResHoles, uint8 (&hiResHoles)[8]);
        static float SelectUInt8StepStore(float maxDiff);
        static float SelectUInt16StepStore(float maxDiff);

        uint32 _build;
        bool _allowMapHeightCompression;
    };
}

#endif // MapBuilder_h__
