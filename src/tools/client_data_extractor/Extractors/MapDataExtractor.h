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

#ifndef MapDataExtractor_h__
#define MapDataExtractor_h__

#include "BaseExtractor.h"
#include "ChunkedFileParser.h"
#include "Optional.h"
#include <atomic>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

namespace Trinity
{
    namespace DataStores
    {
        struct Map;
    }

    class MapDataExtractor : public BaseExtractor
    {
    public:
        struct Context
        {
            Context() : TotalTasks(0), ProcessedTasks(0) { }

            std::atomic<uint32> TotalTasks;
            std::atomic<uint32> ProcessedTasks;
        };

        struct ADTData;

        struct WDTData
        {
            uint32 MapId = 0;
            DataStores::Map const* MapInfo;
            std::shared_ptr<ClientStorage::Storage> Storage;
            ChunkedFileParser FileData;

            // ADT cache, enabled only if map is used as parent for another map
            Optional<std::array<std::array<std::shared_ptr<ADTData>, 64>, 64>> CachedADTs;

            // Task specific data
            std::unordered_map<std::type_index, boost::any> TaskData;

            WDTData(uint32 mapId, DataStores::Map const* mapInfo) : MapId(mapId), MapInfo(mapInfo) { }
            bool LoadFile(std::shared_ptr<ClientStorage::Storage> storage, ClientStorage::FileId const& id);
            std::shared_ptr<ADTData> GetADT(int32 x, int32 y);
        };

        struct ADTData
        {
            ChunkedFileParser FileData;

            // ADT cache, enabled only if map is used as parent for another map - copy of output
            Optional<std::vector<uint8>> CachedVmapOutput;

            // Task specific data
            std::unordered_map<std::type_index, boost::any> TaskData;

            bool LoadFile(std::shared_ptr<ClientStorage::Storage> storage, ClientStorage::FileId const& id, std::unordered_set<std::string> const& parsedChunks);
        };

        MapDataExtractor(std::shared_ptr<ClientStorage::Storage> storage, boost::filesystem::path outputPath, uint32 build,
            bool extractMaps, bool allowMapHeightCompression, bool extractVmaps, bool includeAllVmapVertices)
            : BaseExtractor(std::move(storage), std::move(outputPath)), _build(build),
            _extractMaps(extractMaps), _allowMapHeightCompression(allowMapHeightCompression),
            _extractVmaps(extractVmaps), _includeAllVmapVertices(includeAllVmapVertices) { }

        void Run();

        class BaseBuilder
        {
        public:
            using Task = std::function<void()>;

            virtual ~BaseBuilder() = default;

            virtual Task CreateWDTTask(Context& context, WDTData* wdtData) = 0;
            virtual Task CreateADTTask(Context& context, WDTData* wdtData, int32 x, int32 y) = 0;
            virtual Task CreateWDTFinalizeTask(Context& context, WDTData* wdtData) = 0;

        protected:
            BaseBuilder(boost::filesystem::path outputPath) : _outputPath(std::move(outputPath)) { }
            boost::filesystem::path _outputPath;
        };

    private:
        struct MapDependencyInfo;
        std::map<uint32, std::vector<MapDependencyInfo>> PrepareExtractionSteps();

        uint32 _build;

        bool _extractMaps;
        bool _allowMapHeightCompression;

        bool _extractVmaps;
        bool _includeAllVmapVertices;
    };
}

#endif // MapDataExtractor_h__
