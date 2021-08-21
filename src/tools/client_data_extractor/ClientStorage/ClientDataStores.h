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

#ifndef ClientDataStores_h__
#define ClientDataStores_h__

#include "ClientStorage.h"
#include <map>

class DB2Record;

namespace Trinity
{
    namespace DataStores
    {
        struct CinematicCamera
        {
            uint32 FileDataID = 0;

            void FillFrom(DB2Record& record);
        };

        struct GameobjectDisplayInfo
        {
            uint32 FileDataID = 0;

            void FillFrom(DB2Record& record);
        };

        struct LiquidMaterial
        {
            int8 LVF = 0;

            void FillFrom(DB2Record& record);
        };

        struct LiquidObject
        {
            int16 LiquidTypeID = 0;

            void FillFrom(DB2Record& record);
        };

        enum class LiquidTypeSoundBank : uint8
        {
            Water   = 0,
            Ocean   = 1,
            Magma   = 2,
            Slime   = 3
        };

        struct LiquidType
        {
            LiquidTypeSoundBank SoundBank = LiquidTypeSoundBank::Water;
            uint8 MaterialID = 0;

            void FillFrom(DB2Record& record);
        };

        struct Map
        {
            int32 WdtFileDataID = 0;
            int16 ParentMapID = 0;
            std::string Name;
            std::string Directory;

            void FillFrom(DB2Record& record);
        };

        extern std::map<uint32, Trinity::DataStores::CinematicCamera> CinematicCameras;
        extern std::map<uint32, Trinity::DataStores::GameobjectDisplayInfo> GameobjectDisplays;
        extern std::map<uint32, Trinity::DataStores::LiquidMaterial> LiquidMaterials;
        extern std::map<uint32, Trinity::DataStores::LiquidObject> LiquidObjects;
        extern std::map<uint32, Trinity::DataStores::LiquidType> LiquidTypes;
        extern std::map<uint32, Trinity::DataStores::Map> Maps;

        void Load(std::shared_ptr<ClientStorage::Storage> storage);
    }
}

#endif // ClientDataStores_h__
