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

#include "ClientDataStores.h"
#include "ClientStorage.h"
#include "ClientDataStoreFileSource.h"
#include "Containers.h"
#include "DB2FileLoader.h"
#include "ExtractorDB2LoadInfo.h"
#include "Log.h"

namespace Trinity
{
namespace DataStores
{
    std::map<uint32, Trinity::DataStores::CinematicCamera> CinematicCameras;
    std::map<uint32, Trinity::DataStores::GameobjectDisplayInfo> GameobjectDisplays;
    std::map<uint32, Trinity::DataStores::LiquidMaterial> LiquidMaterials;
    std::map<uint32, Trinity::DataStores::LiquidObject> LiquidObjects;
    std::map<uint32, Trinity::DataStores::LiquidType> LiquidTypes;
    std::map<uint32, Trinity::DataStores::Map> Maps;

    void CinematicCamera::FillFrom(DB2Record& record)
    {
        FileDataID = record.GetUInt32("FileDataID");
    }

    void GameobjectDisplayInfo::FillFrom(DB2Record& record)
    {
        FileDataID = record.GetUInt32("FileDataID");
    }

    void LiquidMaterial::FillFrom(DB2Record& record)
    {
        LVF = int8(record.GetUInt8("LVF"));
    }

    void LiquidObject::FillFrom(DB2Record& record)
    {
        LiquidTypeID = int16(record.GetUInt16("LiquidTypeID"));
    }

    void LiquidType::FillFrom(DB2Record& record)
    {
        SoundBank = LiquidTypeSoundBank(record.GetUInt8("SoundBank"));
        MaterialID = record.GetUInt8("MaterialID");
    }

    void Map::FillFrom(DB2Record& record)
    {
        WdtFileDataID = record.GetInt32("WdtFileDataID");
        ParentMapID = int16(record.GetUInt16("ParentMapID"));
        Name = record.GetString("MapName");
        Directory = record.GetString("Directory");

        if (ParentMapID < 0)
            ParentMapID = int16(record.GetUInt16("CosmeticParentMapID"));
    }

    template<typename T>
    void LoadStore(std::shared_ptr<ClientStorage::Storage> storage, std::string const& name, DB2FileLoadInfo const* loadInfo, std::map<uint32, T>& store)
    {
        TC_LOG_INFO("extractor.datastores", "Loading %s...", name.c_str());

        ClientDataStoreFileSource source(storage, loadInfo->Meta->FileDataId);
        DB2FileLoader loader;
        try
        {
            loader.Load(&source, loadInfo);
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("extractor.datastores", "Fatal error: Invalid %s file format! %s", name.c_str(), storage->GetLastErrorText());
            TC_LOG_ERROR("extractor.datastores", "%s\n", e.what());
            throw e;
        }

        for (uint32 i = 0; i < loader.GetRecordCount(); ++i)
            if (DB2Record record = loader.GetRecord(i))
                store[record.GetId()].FillFrom(record);

        for (uint32 i = 0; i < loader.GetRecordCopyCount(); ++i)
        {
            DB2RecordCopy copy = loader.GetRecordCopy(i);
            if (T const* original = Containers::MapGetValuePtr(store, copy.SourceRowId))
                store[copy.NewRowId] = *original;
        }

        TC_LOG_INFO("extractor.datastores", "Done! (" SZFMTD " rows loaded)\n", store.size());
    }

    void Load(std::shared_ptr<ClientStorage::Storage> storage)
    {
        LoadStore(storage, "CinematicCamera.db2", CinematicCameraLoadInfo::Instance(), CinematicCameras);
        LoadStore(storage, "GameObjectDisplayInfo.db2", GameobjectDisplayInfoLoadInfo::Instance(), GameobjectDisplays);
        LoadStore(storage, "LiquidMaterial.db2", LiquidMaterialLoadInfo::Instance(), LiquidMaterials);
        LoadStore(storage, "LiquidObjects.db2", LiquidObjectLoadInfo::Instance(), LiquidObjects);
        LoadStore(storage, "LiquidTypes.db2", LiquidTypeLoadInfo::Instance(), LiquidTypes);
        LoadStore(storage, "Map.db2", MapLoadInfo::Instance(), Maps);
    }
}
}
