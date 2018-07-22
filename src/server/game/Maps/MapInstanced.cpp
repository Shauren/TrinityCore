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

#include "MapInstanced.h"
#include "Battleground.h"
#include "DB2Stores.h"
#include "GarrisonMap.h"
#include "Group.h"
#include "InstanceLockMgr.h"
#include "InstanceSaveMgr.h"
#include "Log.h"
#include "MapManager.h"
#include "MMapFactory.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScenarioMgr.h"
#include "VMapFactory.h"
#include "World.h"

MapInstanced::MapInstanced(uint32 id, time_t expiry) : Map(id, expiry, 0, DIFFICULTY_NORMAL)
{
}

void MapInstanced::InitVisibilityDistance()
{
    if (m_InstancedMaps.empty())
        return;
    //initialize visibility distances for all instance copies
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
    {
        (*i).second->InitVisibilityDistance();
    }
}

void MapInstanced::Update(uint32 t)
{
    // take care of loaded GridMaps (when unused, unload it!)
    Map::Update(t);

    // update the instanced maps
    InstancedMaps::iterator i = m_InstancedMaps.begin();

    while (i != m_InstancedMaps.end())
    {
        if (i->second->CanUnload(t))
        {
            if (!DestroyInstance(i))                             // iterator incremented
            {
                //m_unloadTimer
            }
        }
        else
        {
            // update only here, because it may schedule some bad things before delete
            if (sMapMgr->GetMapUpdater()->activated())
                sMapMgr->GetMapUpdater()->schedule_update(*i->second, t);
            else
                i->second->Update(t);
            ++i;
        }
    }
}

void MapInstanced::DelayedUpdate(uint32 diff)
{
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
        i->second->DelayedUpdate(diff);

    Map::DelayedUpdate(diff); // this may be removed
}

/*
void MapInstanced::RelocationNotify()
{
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
        i->second->RelocationNotify();
}
*/

void MapInstanced::UnloadAll()
{
    // Unload instanced maps
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
        i->second->UnloadAll();

    // Delete the maps only after everything is unloaded to prevent crashes
    for (InstancedMaps::iterator i = m_InstancedMaps.begin(); i != m_InstancedMaps.end(); ++i)
        delete i->second;

    m_InstancedMaps.clear();

    // Unload own grids (just dummy(placeholder) grids, neccesary to unload GridMaps!)
    Map::UnloadAll();
}

/*
- return the right instance for the object, based on its InstanceId
- create the instance if it's not created already
- the player is not actually added to the instance (only in InstanceMap::Add)
*/
Map* MapInstanced::CreateInstanceForPlayer(Player* player)
{
    if (!player)
        return nullptr;

    Map* map = nullptr;
    uint32 newInstanceId = 0;                       // instanceId of the resulting map

    if (IsBattlegroundOrArena())
    {
        // instantiate or find existing bg map for player
        // the instance id is set in battlegroundid
        newInstanceId = player->GetBattlegroundId();
        if (!newInstanceId)
            return nullptr;

        map = FindInstanceMap(newInstanceId);
        if (!map)
        {
            if (Battleground* bg = player->GetBattleground())
                map = CreateBattleground(newInstanceId, bg);
            else
            {
                player->TeleportToBGEntryPoint();
                return nullptr;
            }
        }
    }
    else if (!IsGarrison())
    {
        Group* group = player->GetGroup();
        Difficulty difficulty = group ? group->GetDifficultyID(GetEntry()) : player->GetDifficultyID(GetEntry());
        MapDb2Entries entries{ GetEntry(), sDB2Manager.GetDownscaledMapDifficultyData(GetId(), difficulty) };
        ObjectGuid instanceOwnerGuid = group ? group->GetRecentInstanceOwner(GetId()) : player->GetGUID();
        InstanceLock* instanceLock = sInstanceLockMgr.FindActiveInstanceLock(instanceOwnerGuid, entries);
        if (instanceLock)
        {
            newInstanceId = instanceLock->GetInstanceId();

            // Reset difficulty to the one used in instance lock
            if (!entries.Map->IsFlexLocking())
                difficulty = instanceLock->GetDifficultyId();
        }
        else
        {
            // Try finding instance id for normal dungeon
            if (!entries.MapDifficulty->HasResetSchedule())
                newInstanceId = group ? group->GetRecentInstanceId(GetId()) : player->GetRecentInstanceId(GetId());

            // If not found or instance is not a normal dungeon, generate new one
            if (!newInstanceId)
                newInstanceId = sMapMgr->GenerateInstanceId();

            instanceLock = sInstanceLockMgr.CreateInstanceLockForNewInstance(instanceOwnerGuid, entries, newInstanceId);
        }

        // it is possible that the save exists but the map doesn't
        map = FindInstanceMap(newInstanceId);

        // is is also possible that instance id is already in use by another group for boss-based locks
        if (!entries.IsInstanceIdBound() && instanceLock && map && map->ToInstanceMap()->GetInstanceLock() != instanceLock)
        {
            newInstanceId = sMapMgr->GenerateInstanceId();
            instanceLock->SetInstanceId(newInstanceId);
            map = nullptr;
        }

        if (!map)
        {
            map = CreateInstance(newInstanceId, instanceLock, difficulty, player->GetTeamId(), group);
            if (group)
                group->SetRecentInstance(GetId(), instanceOwnerGuid, newInstanceId);
            else
                player->SetRecentInstance(GetId(), newInstanceId);
        }
    }
    else
    {
        newInstanceId = player->GetGUID().GetCounter();
        map = FindInstanceMap(newInstanceId);
        if (!map)
            map = CreateGarrison(newInstanceId, player);
    }

    return map;
}

InstanceMap* MapInstanced::CreateInstance(uint32 instanceId, InstanceLock* instanceLock, Difficulty difficulty, TeamId team, Group* group)
{
    // load/create a map
    std::lock_guard<std::mutex> lock(_mapLock);

    if (!sObjectMgr->GetInstanceTemplate(GetId()))
    {
        TC_LOG_ERROR("maps", "CreateInstance: no instance template for map %d", GetId());
        ABORT();
    }

    TC_LOG_DEBUG("maps", "MapInstanced::CreateInstance: %smap instance %d for %d created with difficulty %s",
        instanceLock && instanceLock->GetInstanceId() ? "" : "new ", instanceId, GetId(), sDifficultyStore.AssertEntry(difficulty)->Name[sWorld->GetDefaultDbcLocale()]);

    InstanceMap* map = new InstanceMap(GetId(), GetGridExpiry(), instanceId, difficulty, instanceLock, this);
    ASSERT(map->IsDungeon());

    map->LoadRespawnTimes();
    map->LoadCorpseData();
    map->CreateInstanceData();
    map->SetInstanceScenario(sScenarioMgr->CreateInstanceScenario(map, team));

    if (sWorld->getBoolConfig(CONFIG_INSTANCEMAP_LOAD_GRIDS))
        map->LoadAllCells();

    m_InstancedMaps[instanceId] = map;
    return map;
}

BattlegroundMap* MapInstanced::CreateBattleground(uint32 InstanceId, Battleground* bg)
{
    // load/create a map
    std::lock_guard<std::mutex> lock(_mapLock);

    TC_LOG_DEBUG("maps", "MapInstanced::CreateBattleground: map bg %d for %d created.", InstanceId, GetId());

    BattlegroundMap* map = new BattlegroundMap(GetId(), GetGridExpiry(), InstanceId, this, DIFFICULTY_NONE);
    ASSERT(map->IsBattlegroundOrArena());
    map->SetBG(bg);
    bg->SetBgMap(map);

    m_InstancedMaps[InstanceId] = map;
    return map;
}

GarrisonMap* MapInstanced::CreateGarrison(uint32 instanceId, Player* owner)
{
    std::lock_guard<std::mutex> lock(_mapLock);

    GarrisonMap* map = new GarrisonMap(GetId(), GetGridExpiry(), instanceId, this, owner->GetGUID());
    ASSERT(map->IsGarrison());

    m_InstancedMaps[instanceId] = map;
    return map;
}

// increments the iterator after erase
bool MapInstanced::DestroyInstance(InstancedMaps::iterator &itr)
{
    itr->second->RemoveAllPlayers();
    if (itr->second->HavePlayers())
    {
        ++itr;
        return false;
    }

    itr->second->UnloadAll();
    // should only unload VMaps if this is the last instance and grid unloading is enabled
    if (m_InstancedMaps.size() <= 1 && sWorld->getBoolConfig(CONFIG_GRID_UNLOAD))
    {
        VMAP::VMapFactory::createOrGetVMapManager()->unloadMap(itr->second->GetId());
        MMAP::MMapFactory::createOrGetMMapManager()->unloadMap(itr->second->GetId());
        // in that case, unload grids of the base map, too
        // so in the next map creation, (EnsureGridCreated actually) VMaps will be reloaded
        Map::UnloadAll();
    }

    // Free up the instance id and allow it to be reused for bgs and arenas (other instances are handled in the InstanceSaveMgr)
    if (itr->second->IsBattlegroundOrArena())
        sMapMgr->FreeInstanceId(itr->second->GetInstanceId());

    // erase map
    delete itr->second;
    m_InstancedMaps.erase(itr++);

    return true;
}

Map::EnterState MapInstanced::CannotEnter(Player* /*player*/)
{
    //ABORT();
    return CAN_ENTER;
}
