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

#include "InstanceScript.h"
#include "AreaBoundary.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "Group.h"
#include "InstancePackets.h"
#include "InstanceScenario.h"
#include "InstanceScriptData.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "PhasingHandler.h"
#include "Player.h"
#include "RBAC.h"
#include "ScriptMgr.h"
#include "ScriptReloadMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldSession.h"
#include <cstdarg>

BossBoundaryData::~BossBoundaryData()
{
    for (const_iterator it = begin(); it != end(); ++it)
        delete it->Boundary;
}

DungeonEncounterEntry const* BossInfo::GetDungeonEncounterForDifficulty(Difficulty difficulty) const
{
    auto itr = std::find_if(DungeonEncounters.begin(), DungeonEncounters.end(), [difficulty](DungeonEncounterEntry const* dungeonEncounter)
    {
        return dungeonEncounter && (dungeonEncounter->DifficultyID == 0 || Difficulty(dungeonEncounter->DifficultyID) == difficulty);
    });

    return itr != DungeonEncounters.end() ? *itr : nullptr;
}

InstanceScript::InstanceScript(InstanceMap* map) : instance(map), completedEncounters(0), _instanceSpawnGroups(sObjectMgr->GetSpawnGroupsForInstance(map->GetId())),
_entranceId(0), _temporaryEntranceId(0), _combatResurrectionTimer(0), _combatResurrectionCharges(0), _combatResurrectionTimerStarted(false)
{
#ifdef TRINITY_API_USE_DYNAMIC_LINKING
    uint32 scriptId = sObjectMgr->GetInstanceTemplate(map->GetId())->ScriptId;
    auto const scriptname = sObjectMgr->GetScriptName(scriptId);
    ASSERT(!scriptname.empty());
   // Acquire a strong reference from the script module
   // to keep it loaded until this object is destroyed.
    module_reference = sScriptMgr->AcquireModuleReferenceOfScriptName(scriptname);
#endif // #ifndef TRINITY_API_USE_DYNAMIC_LINKING
}

bool InstanceScript::IsEncounterInProgress() const
{
    for (std::vector<BossInfo>::const_iterator itr = bosses.begin(); itr != bosses.end(); ++itr)
        if (itr->state == IN_PROGRESS)
            return true;

    return false;
}

void InstanceScript::OnCreatureCreate(Creature* creature)
{
    AddObject(creature, true);
    AddMinion(creature, true);
}

void InstanceScript::OnCreatureRemove(Creature* creature)
{
    AddObject(creature, false);
    AddMinion(creature, false);
}

void InstanceScript::OnGameObjectCreate(GameObject* go)
{
    AddObject(go, true);
    AddDoor(go, true);
}

void InstanceScript::OnGameObjectRemove(GameObject* go)
{
    AddObject(go, false);
    AddDoor(go, false);
}

ObjectGuid InstanceScript::GetObjectGuid(uint32 type) const
{
    ObjectGuidMap::const_iterator i = _objectGuids.find(type);
    if (i != _objectGuids.end())
        return i->second;
    return ObjectGuid::Empty;
}

ObjectGuid InstanceScript::GetGuidData(uint32 type) const
{
    return GetObjectGuid(type);
}

Creature* InstanceScript::GetCreature(uint32 type)
{
    return instance->GetCreature(GetObjectGuid(type));
}

GameObject* InstanceScript::GetGameObject(uint32 type)
{
    return instance->GetGameObject(GetObjectGuid(type));
}

void InstanceScript::SetHeaders(std::string const& dataHeaders)
{
    headers = dataHeaders;
}

void InstanceScript::LoadBossBoundaries(BossBoundaryData const& data)
{
    for (BossBoundaryEntry const& entry : data)
        if (entry.BossId < bosses.size())
            bosses[entry.BossId].boundary.push_back(entry.Boundary);
}

void InstanceScript::LoadMinionData(MinionData const* data)
{
    while (data->entry)
    {
        if (data->bossId < bosses.size())
            minions.insert(std::make_pair(data->entry, MinionInfo(&bosses[data->bossId])));

        ++data;
    }
    TC_LOG_DEBUG("scripts", "InstanceScript::LoadMinionData: " UI64FMTD " minions loaded.", uint64(minions.size()));
}

void InstanceScript::LoadDoorData(DoorData const* data)
{
    while (data->entry)
    {
        if (data->bossId < bosses.size())
            doors.insert(std::make_pair(data->entry, DoorInfo(&bosses[data->bossId], data->type)));

        ++data;
    }
    TC_LOG_DEBUG("scripts", "InstanceScript::LoadDoorData: " UI64FMTD " doors loaded.", uint64(doors.size()));
}

void InstanceScript::LoadObjectData(ObjectData const* creatureData, ObjectData const* gameObjectData)
{
    if (creatureData)
        LoadObjectData(creatureData, _creatureInfo);

    if (gameObjectData)
        LoadObjectData(gameObjectData, _gameObjectInfo);

    TC_LOG_DEBUG("scripts", "InstanceScript::LoadObjectData: " SZFMTD " objects loaded.", _creatureInfo.size() + _gameObjectInfo.size());
}

void InstanceScript::LoadObjectData(ObjectData const* data, ObjectInfoMap& objectInfo)
{
    while (data->entry)
    {
        ASSERT(objectInfo.find(data->entry) == objectInfo.end());
        objectInfo[data->entry] = data->type;
        ++data;
    }
}

void InstanceScript::LoadDungeonEncounterData(uint32 bossId, std::array<uint32, MAX_DUNGEON_ENCOUNTERS_PER_BOSS> const& dungeonEncounterIds)
{
    if (bossId < bosses.size())
        for (std::size_t i = 0; i < MAX_DUNGEON_ENCOUNTERS_PER_BOSS; ++i)
            bosses[bossId].DungeonEncounters[i] = sDungeonEncounterStore.LookupEntry(dungeonEncounterIds[i]);
}

void InstanceScript::UpdateDoorState(GameObject* door)
{
    DoorInfoMapBounds range = doors.equal_range(door->GetEntry());
    if (range.first == range.second)
        return;

    bool open = true;
    for (; range.first != range.second && open; ++range.first)
    {
        DoorInfo const& info = range.first->second;
        switch (info.type)
        {
            case DOOR_TYPE_ROOM:
                open = (info.bossInfo->state != IN_PROGRESS);
                break;
            case DOOR_TYPE_PASSAGE:
                open = (info.bossInfo->state == DONE);
                break;
            case DOOR_TYPE_SPAWN_HOLE:
                open = (info.bossInfo->state == IN_PROGRESS);
                break;
            default:
                break;
        }
    }

    door->SetGoState(open ? GO_STATE_ACTIVE : GO_STATE_READY);
}

void InstanceScript::UpdateMinionState(Creature* minion, EncounterState state)
{
    switch (state)
    {
        case NOT_STARTED:
            if (!minion->IsAlive())
                minion->Respawn();
            else if (minion->IsInCombat())
                minion->AI()->EnterEvadeMode();
            break;
        case IN_PROGRESS:
            if (!minion->IsAlive())
                minion->Respawn();
            else if (!minion->GetVictim())
                minion->AI()->DoZoneInCombat();
            break;
        default:
            break;
    }
}

void InstanceScript::UpdateSpawnGroups()
{
    if (!_instanceSpawnGroups)
        return;
    enum states { BLOCK, SPAWN, FORCEBLOCK };
    std::unordered_map<uint32, states> newStates;
    for (auto it = _instanceSpawnGroups->begin(), end = _instanceSpawnGroups->end(); it != end; ++it)
    {
        InstanceSpawnGroupInfo const& info = *it;
        states& curValue = newStates[info.SpawnGroupId]; // makes sure there's a BLOCK value in the map
        if (curValue == FORCEBLOCK) // nothing will change this
            continue;
        if (!((1 << GetBossState(info.BossStateId)) & info.BossStates))
            continue;
        if (info.Flags & InstanceSpawnGroupInfo::FLAG_BLOCK_SPAWN)
            curValue = FORCEBLOCK;
        else if (info.Flags & InstanceSpawnGroupInfo::FLAG_ACTIVATE_SPAWN)
            curValue = SPAWN;
    }
    for (auto const& pair : newStates)
    {
        uint32 const groupId = pair.first;
        bool const doSpawn = (pair.second == SPAWN);
        if (instance->IsSpawnGroupActive(groupId) == doSpawn)
            continue; // nothing to do here
        // if we should spawn group, then spawn it...
        if (doSpawn)
            instance->SpawnGroupSpawn(groupId);
        else // otherwise, set it as inactive so it no longer respawns (but don't despawn it)
            instance->SetSpawnGroupActive(groupId, false);
    }
}

BossInfo* InstanceScript::GetBossInfo(uint32 id)
{
    ASSERT(id < bosses.size());
    return &bosses[id];
}

void InstanceScript::AddObject(Creature* obj, bool add)
{
    ObjectInfoMap::const_iterator j = _creatureInfo.find(obj->GetEntry());
    if (j != _creatureInfo.end())
        AddObject(obj, j->second, add);
}

void InstanceScript::AddObject(GameObject* obj, bool add)
{
    ObjectInfoMap::const_iterator j = _gameObjectInfo.find(obj->GetEntry());
    if (j != _gameObjectInfo.end())
        AddObject(obj, j->second, add);
}

void InstanceScript::AddObject(WorldObject* obj, uint32 type, bool add)
{
    if (add)
        _objectGuids[type] = obj->GetGUID();
    else
    {
        ObjectGuidMap::iterator i = _objectGuids.find(type);
        if (i != _objectGuids.end() && i->second == obj->GetGUID())
            _objectGuids.erase(i);
    }
}

void InstanceScript::AddDoor(GameObject* door, bool add)
{
    DoorInfoMapBounds range = doors.equal_range(door->GetEntry());
    if (range.first == range.second)
        return;

    for (; range.first != range.second; ++range.first)
    {
        DoorInfo const& data = range.first->second;

        if (add)
        {
            data.bossInfo->door[data.type].insert(door->GetGUID());
        }
        else
            data.bossInfo->door[data.type].erase(door->GetGUID());
    }

    if (add)
        UpdateDoorState(door);
}

void InstanceScript::AddMinion(Creature* minion, bool add)
{
    MinionInfoMap::iterator itr = minions.find(minion->GetEntry());
    if (itr == minions.end())
        return;

    if (add)
        itr->second.bossInfo->minion.insert(minion->GetGUID());
    else
        itr->second.bossInfo->minion.erase(minion->GetGUID());
}

bool InstanceScript::SetBossState(uint32 id, EncounterState state)
{
    if (id < bosses.size())
    {
        BossInfo* bossInfo = &bosses[id];
        if (bossInfo->state == TO_BE_DECIDED) // loading
        {
            bossInfo->state = state;
            TC_LOG_DEBUG("scripts", "InstanceScript: Initialize boss %u state as %s (map %u, %u).", id, GetBossStateName(state), instance->GetId(), instance->GetInstanceId());
            return false;
        }
        else
        {
            if (bossInfo->state == state)
                return false;

            if (bossInfo->state == DONE)
            {
                TC_LOG_ERROR("map", "InstanceScript: Tried to set instance state from %s back to %s for map %u, instance id %u. Blocked!", GetBossStateName(bossInfo->state), GetBossStateName(state), instance->GetId(), instance->GetInstanceId());
                return false;
            }

            if (state == DONE)
                for (GuidSet::iterator i = bossInfo->minion.begin(); i != bossInfo->minion.end(); ++i)
                    if (Creature* minion = instance->GetCreature(*i))
                        if (minion->isWorldBoss() && minion->IsAlive())
                            return false;

            DungeonEncounterEntry const* dungeonEncounter = nullptr;
            switch (state)
            {
                case IN_PROGRESS:
                {
                    uint32 resInterval = GetCombatResurrectionChargeInterval();
                    InitializeCombatResurrections(1, resInterval);
                    SendEncounterStart(1, 9, resInterval, resInterval);

                    Map::PlayerList const &playerList = instance->GetPlayers();
                    if (!playerList.isEmpty())
                        for (Map::PlayerList::const_iterator i = playerList.begin(); i != playerList.end(); ++i)
                            if (Player* player = i->GetSource())
                                if (player->IsAlive())
                                    player->ProcSkillsAndAuras(nullptr, PROC_FLAG_ENCOUNTER_START, PROC_FLAG_NONE, PROC_SPELL_TYPE_MASK_ALL, PROC_SPELL_PHASE_NONE, PROC_HIT_NONE, nullptr, nullptr, nullptr);
                    break;
                }
                case FAIL:
                    ResetCombatResurrections();
                    SendEncounterEnd();
                    break;
                case DONE:
                    ResetCombatResurrections();
                    SendEncounterEnd();
                    dungeonEncounter = bossInfo->GetDungeonEncounterForDifficulty(instance->GetDifficultyID());
                    if (dungeonEncounter)
                    {
                        DoUpdateCriteria(CRITERIA_TYPE_COMPLETE_DUNGEON_ENCOUNTER, dungeonEncounter->ID);
                        SendBossKillCredit(dungeonEncounter->ID);
                    }
                    break;
                default:
                    break;
            }

            bossInfo->state = state;
            if (dungeonEncounter)
                instance->UpdateInstanceLock({ dungeonEncounter, id, state });
        }

        for (uint32 type = 0; type < MAX_DOOR_TYPES; ++type)
            for (GuidSet::iterator i = bossInfo->door[type].begin(); i != bossInfo->door[type].end(); ++i)
                if (GameObject* door = instance->GetGameObject(*i))
                    UpdateDoorState(door);

        GuidSet minions = bossInfo->minion; // Copy to prevent iterator invalidation (minion might be unsummoned in UpdateMinionState)
        for (GuidSet::iterator i = minions.begin(); i != minions.end(); ++i)
            if (Creature* minion = instance->GetCreature(*i))
                UpdateMinionState(minion, state);

        UpdateSpawnGroups();
        return true;
    }
    return false;
}

bool InstanceScript::_SkipCheckRequiredBosses(Player const* player /*= nullptr*/) const
{
    return player && player->GetSession()->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_INSTANCE_REQUIRED_BOSSES);
}

void InstanceScript::Create()
{
    for (size_t i = 0; i < bosses.size(); ++i)
        SetBossState(i, NOT_STARTED);
    UpdateSpawnGroups();
}

void InstanceScript::Load(char const* data)
{
    if (!data)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(data);

    InstanceScriptDataReader reader(*this);
    if (reader.Load(data) == InstanceScriptDataReader::Result::Ok)
    {
        UpdateSpawnGroups();
        AfterDataLoad();
    }
    else
        OUT_LOAD_INST_DATA_FAIL;

    OUT_LOAD_INST_DATA_COMPLETE;
}

std::string InstanceScript::GetSaveData()
{
    OUT_SAVE_INST_DATA;

    InstanceScriptDataWriter writer(*this);

    writer.FillData();

    OUT_SAVE_INST_DATA_COMPLETE;

    return writer.GetString();
}

std::string InstanceScript::UpdateBossStateSaveData(std::string const& oldData, UpdateBossStateSaveDataEvent const& event)
{
    if (!instance->GetMapDifficulty()->IsUsingEncounterLocks())
        return GetSaveData();

    InstanceScriptDataWriter writer(*this);
    writer.FillDataFrom(oldData);
    writer.SetBossState(event);
    return writer.GetString();
}

std::string InstanceScript::UpdateAdditionalSaveData(std::string const& oldData, UpdateAdditionalSaveDataEvent const& event)
{
    if (!instance->GetMapDifficulty()->IsUsingEncounterLocks())
        return GetSaveData();

    InstanceScriptDataWriter writer(*this);
    writer.FillDataFrom(oldData);
    writer.SetAdditionalData(event);
    return writer.GetString();
}

Optional<uint32> InstanceScript::GetEntranceLocationForCompletedEncounters(uint32 completedEncountersMask) const
{
    if (!instance->GetMapDifficulty()->IsUsingEncounterLocks())
        return _entranceId;

    return ComputeEntranceLocationForCompletedEncounters(completedEncountersMask);
}

Optional<uint32> InstanceScript::ComputeEntranceLocationForCompletedEncounters(uint32 /*completedEncountersMask*/) const
{
    return { };
}

void InstanceScript::HandleGameObject(ObjectGuid guid, bool open, GameObject* go /*= nullptr*/)
{
    if (!go)
        go = instance->GetGameObject(guid);
    if (go)
        go->SetGoState(open ? GO_STATE_ACTIVE : GO_STATE_READY);
    else
        TC_LOG_DEBUG("scripts", "InstanceScript: HandleGameObject failed");
}

void InstanceScript::DoUseDoorOrButton(ObjectGuid guid, uint32 withRestoreTime /*= 0*/, bool useAlternativeState /*= false*/)
{
    if (!guid)
        return;

    if (GameObject* go = instance->GetGameObject(guid))
    {
        if (go->GetGoType() == GAMEOBJECT_TYPE_DOOR || go->GetGoType() == GAMEOBJECT_TYPE_BUTTON)
        {
            if (go->getLootState() == GO_READY)
                go->UseDoorOrButton(withRestoreTime, useAlternativeState);
            else if (go->getLootState() == GO_ACTIVATED)
                go->ResetDoorOrButton();
        }
        else
            TC_LOG_ERROR("scripts", "InstanceScript: DoUseDoorOrButton can't use gameobject entry %u, because type is %u.", go->GetEntry(), go->GetGoType());
    }
    else
        TC_LOG_DEBUG("scripts", "InstanceScript: DoUseDoorOrButton failed");
}

void InstanceScript::DoCloseDoorOrButton(ObjectGuid guid)
{
    if (!guid)
        return;

    if (GameObject* go = instance->GetGameObject(guid))
    {
        if (go->GetGoType() == GAMEOBJECT_TYPE_DOOR || go->GetGoType() == GAMEOBJECT_TYPE_BUTTON)
        {
            if (go->getLootState() == GO_ACTIVATED)
                go->ResetDoorOrButton();
        }
        else
            TC_LOG_ERROR("scripts", "InstanceScript: DoCloseDoorOrButton can't use gameobject entry %u, because type is %u.", go->GetEntry(), go->GetGoType());
    }
    else
        TC_LOG_DEBUG("scripts", "InstanceScript: DoCloseDoorOrButton failed");
}

void InstanceScript::DoRespawnGameObject(ObjectGuid guid, uint32 timeToDespawn /*= MINUTE*/)
{
    if (GameObject* go = instance->GetGameObject(guid))
    {
        switch (go->GetGoType())
        {
            case GAMEOBJECT_TYPE_DOOR:
            case GAMEOBJECT_TYPE_BUTTON:
            case GAMEOBJECT_TYPE_TRAP:
            case GAMEOBJECT_TYPE_FISHINGNODE:
                // not expect any of these should ever be handled
                TC_LOG_ERROR("scripts", "InstanceScript: DoRespawnGameObject can't respawn gameobject entry %u, because type is %u.", go->GetEntry(), go->GetGoType());
                return;
            default:
                break;
        }

        if (go->isSpawned())
            return;

        go->SetRespawnTime(timeToDespawn);
    }
    else
        TC_LOG_DEBUG("scripts", "InstanceScript: DoRespawnGameObject failed");
}

void InstanceScript::DoUpdateWorldState(uint32 uiStateId, uint32 uiStateData)
{
    Map::PlayerList const& lPlayers = instance->GetPlayers();

    if (!lPlayers.isEmpty())
    {
        for (Map::PlayerList::const_iterator itr = lPlayers.begin(); itr != lPlayers.end(); ++itr)
            if (Player* player = itr->GetSource())
                player->SendUpdateWorldState(uiStateId, uiStateData);
    }
    else
        TC_LOG_DEBUG("scripts", "DoUpdateWorldState attempt send data but no players in map.");
}

// Send Notify to all players in instance
void InstanceScript::DoSendNotifyToInstance(char const* format, ...)
{
    InstanceMap::PlayerList const& players = instance->GetPlayers();

    if (!players.isEmpty())
    {
        va_list ap;
        va_start(ap, format);
        char buff[1024];
        vsnprintf(buff, 1024, format, ap);
        va_end(ap);
        for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
            if (Player* player = i->GetSource())
                if (WorldSession* session = player->GetSession())
                    session->SendNotification("%s", buff);
    }
}

// Update Achievement Criteria for all players in instance
void InstanceScript::DoUpdateCriteria(CriteriaTypes type, uint32 miscValue1 /*= 0*/, uint32 miscValue2 /*= 0*/, Unit* unit /*= nullptr*/)
{
    Map::PlayerList const& PlayerList = instance->GetPlayers();

    if (!PlayerList.isEmpty())
        for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            if (Player* player = i->GetSource())
                player->UpdateCriteria(type, miscValue1, miscValue2, 0, unit);
}

// Start timed achievement for all players in instance
void InstanceScript::DoStartCriteriaTimer(CriteriaTimedTypes type, uint32 entry)
{
    Map::PlayerList const& PlayerList = instance->GetPlayers();

    if (!PlayerList.isEmpty())
        for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            if (Player* player = i->GetSource())
                player->StartCriteriaTimer(type, entry);
}

// Stop timed achievement for all players in instance
void InstanceScript::DoStopCriteriaTimer(CriteriaTimedTypes type, uint32 entry)
{
    Map::PlayerList const& PlayerList = instance->GetPlayers();

    if (!PlayerList.isEmpty())
        for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            if (Player* player = i->GetSource())
                player->RemoveCriteriaTimer(type, entry);
}

// Remove Auras due to Spell on all players in instance
void InstanceScript::DoRemoveAurasDueToSpellOnPlayers(uint32 spell)
{
    Map::PlayerList const& PlayerList = instance->GetPlayers();
    if (!PlayerList.isEmpty())
    {
        for (Map::PlayerList::const_iterator itr = PlayerList.begin(); itr != PlayerList.end(); ++itr)
        {
            if (Player* player = itr->GetSource())
            {
                player->RemoveAurasDueToSpell(spell);
                if (Pet* pet = player->GetPet())
                    pet->RemoveAurasDueToSpell(spell);
            }
        }
    }
}

// Cast spell on all players in instance
void InstanceScript::DoCastSpellOnPlayers(uint32 spell)
{
    Map::PlayerList const& PlayerList = instance->GetPlayers();

    if (!PlayerList.isEmpty())
        for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
            if (Player* player = i->GetSource())
                player->CastSpell(player, spell, true);
}

bool InstanceScript::ServerAllowsTwoSideGroups()
{
    return sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP);
}

DungeonEncounterEntry const* InstanceScript::GetBossDungeonEncounter(uint32 id) const
{
    return id < bosses.size() ? bosses[id].GetDungeonEncounterForDifficulty(instance->GetDifficultyID()) : nullptr;
}

bool InstanceScript::CheckAchievementCriteriaMeet(uint32 criteria_id, Player const* /*source*/, Unit const* /*target*/ /*= nullptr*/, uint32 /*miscvalue1*/ /*= 0*/)
{
    TC_LOG_ERROR("misc", "Achievement system call InstanceScript::CheckAchievementCriteriaMeet but instance script for map %u not have implementation for achievement criteria %u",
        instance->GetId(), criteria_id);
    return false;
}

bool InstanceScript::IsEncounterCompleted(uint32 dungeonEncounterId) const
{
    for (std::size_t i = 0; i < bosses.size(); ++i)
        for (std::size_t j = 0; j < bosses[i].DungeonEncounters.size(); ++j)
            if (bosses[i].DungeonEncounters[j] && bosses[i].DungeonEncounters[j]->ID == dungeonEncounterId)
                return bosses[i].state == DONE;

    return false;
}

void InstanceScript::SetEntranceLocation(uint32 worldSafeLocationId)
{
    _entranceId = worldSafeLocationId;
    _temporaryEntranceId = 0;
}

void InstanceScript::SendEncounterUnit(uint32 type, Unit* unit /*= nullptr*/, uint8 priority)
{
    switch (type)
    {
        case ENCOUNTER_FRAME_ENGAGE:                    // SMSG_INSTANCE_ENCOUNTER_ENGAGE_UNIT
        {
            if (!unit)
                return;

            WorldPackets::Instance::InstanceEncounterEngageUnit encounterEngageMessage;
            encounterEngageMessage.Unit = unit->GetGUID();
            encounterEngageMessage.TargetFramePriority = priority;
            instance->SendToPlayers(encounterEngageMessage.Write());
            break;
        }
        case ENCOUNTER_FRAME_DISENGAGE:                 // SMSG_INSTANCE_ENCOUNTER_DISENGAGE_UNIT
        {
            if (!unit)
                return;

            WorldPackets::Instance::InstanceEncounterDisengageUnit encounterDisengageMessage;
            encounterDisengageMessage.Unit = unit->GetGUID();
            instance->SendToPlayers(encounterDisengageMessage.Write());
            break;
        }
        case ENCOUNTER_FRAME_UPDATE_PRIORITY:           // SMSG_INSTANCE_ENCOUNTER_CHANGE_PRIORITY
        {
            if (!unit)
                return;

            WorldPackets::Instance::InstanceEncounterChangePriority encounterChangePriorityMessage;
            encounterChangePriorityMessage.Unit = unit->GetGUID();
            encounterChangePriorityMessage.TargetFramePriority = priority;
            instance->SendToPlayers(encounterChangePriorityMessage.Write());
            break;
        }
        default:
            break;
    }
}

void InstanceScript::SendEncounterStart(uint32 inCombatResCount /*= 0*/, uint32 maxInCombatResCount /*= 0*/, uint32 inCombatResChargeRecovery /*= 0*/, uint32 nextCombatResChargeTime /*= 0*/)
{
    WorldPackets::Instance::InstanceEncounterStart encounterStartMessage;
    encounterStartMessage.InCombatResCount = inCombatResCount;
    encounterStartMessage.MaxInCombatResCount = maxInCombatResCount;
    encounterStartMessage.CombatResChargeRecovery = inCombatResChargeRecovery;
    encounterStartMessage.NextCombatResChargeTime = nextCombatResChargeTime;

    instance->SendToPlayers(encounterStartMessage.Write());
}

void InstanceScript::SendEncounterEnd()
{
    WorldPackets::Instance::InstanceEncounterEnd encounterEndMessage;
    instance->SendToPlayers(encounterEndMessage.Write());
}

void InstanceScript::SendBossKillCredit(uint32 encounterId)
{
    WorldPackets::Instance::BossKill bossKillCreditMessage;
    bossKillCreditMessage.DungeonEncounterID = encounterId;

    instance->SendToPlayers(bossKillCreditMessage.Write());
}

void InstanceScript::UpdateEncounterState(EncounterCreditType type, uint32 creditEntry, Unit* /*source*/)
{
    DungeonEncounterList const* encounters = sObjectMgr->GetDungeonEncounterList(instance->GetId(), instance->GetDifficultyID());
    if (!encounters)
        return;

    uint32 dungeonId = 0;

    for (DungeonEncounterList::const_iterator itr = encounters->begin(); itr != encounters->end(); ++itr)
    {
        DungeonEncounter const* encounter = *itr;
        if (encounter->creditType == type && encounter->creditEntry == creditEntry)
        {
            completedEncounters |= 1 << encounter->dbcEntry->Bit;
            if (encounter->lastEncounterDungeon)
            {
                dungeonId = encounter->lastEncounterDungeon;
                TC_LOG_DEBUG("lfg", "UpdateEncounterState: Instance %s (instanceId %u) completed encounter %s. Credit Dungeon: %u",
                    instance->GetMapName(), instance->GetInstanceId(), encounter->dbcEntry->Name[sWorld->GetDefaultDbcLocale()], dungeonId);
                break;
            }
        }
    }

    if (dungeonId)
    {
        Map::PlayerList const& players = instance->GetPlayers();
        for (Map::PlayerList::const_iterator i = players.begin(); i != players.end(); ++i)
        {
            if (Player* player = i->GetSource())
                if (Group* grp = player->GetGroup())
                    if (grp->isLFGGroup())
                    {
                        sLFGMgr->FinishDungeon(grp->GetGUID(), dungeonId, instance);
                        return;
                    }
        }
    }
}

void InstanceScript::UpdateEncounterStateForKilledCreature(uint32 creatureId, Unit* source)
{
    UpdateEncounterState(ENCOUNTER_CREDIT_KILL_CREATURE, creatureId, source);
}

void InstanceScript::UpdateEncounterStateForSpellCast(uint32 spellId, Unit* source)
{
    UpdateEncounterState(ENCOUNTER_CREDIT_CAST_SPELL, spellId, source);
}

void InstanceScript::UpdatePhasing()
{
    Map::PlayerList const& players = instance->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        if (Player* player = itr->GetSource())
            PhasingHandler::SendToPlayer(player);
}

/*static*/ char const* InstanceScript::GetBossStateName(EncounterState state)
{
    // See enum EncounterState in InstanceScript.h
    switch (state)
    {
        case NOT_STARTED:
            return "NOT_STARTED";
        case IN_PROGRESS:
            return "IN_PROGRESS";
        case FAIL:
            return "FAIL";
        case DONE:
            return "DONE";
        case SPECIAL:
            return "SPECIAL";
        case TO_BE_DECIDED:
            return "TO_BE_DECIDED";
        default:
            return "INVALID";
    }
}

EncounterState InstanceScript::GetBossStateFromName(char const* stateName)
{
    if (!strcmp(stateName, "NOT_STARTED"))
        return NOT_STARTED;
    if (!strcmp(stateName, "IN_PROGRESS"))
        return IN_PROGRESS;
    if (!strcmp(stateName, "FAIL"))
        return FAIL;
    if (!strcmp(stateName, "DONE"))
        return DONE;
    if (!strcmp(stateName, "SPECIAL"))
        return SPECIAL;
    if (!strcmp(stateName, "TO_BE_DECIDED"))
        return TO_BE_DECIDED;
    return NOT_STARTED;
}

void InstanceScript::UpdateCombatResurrection(uint32 diff)
{
    if (!_combatResurrectionTimerStarted)
        return;

    if (_combatResurrectionTimer <= diff)
        AddCombatResurrectionCharge();
    else
        _combatResurrectionTimer -= diff;
}

void InstanceScript::InitializeCombatResurrections(uint8 charges /*= 1*/, uint32 interval /*= 0*/)
{
    _combatResurrectionCharges = charges;
    if (!interval)
        return;

    _combatResurrectionTimer = interval;
    _combatResurrectionTimerStarted = true;
}

void InstanceScript::AddCombatResurrectionCharge()
{
    ++_combatResurrectionCharges;
    _combatResurrectionTimer = GetCombatResurrectionChargeInterval();

    WorldPackets::Instance::InstanceEncounterGainCombatResurrectionCharge gainCombatResurrectionCharge;
    gainCombatResurrectionCharge.InCombatResCount = _combatResurrectionCharges;
    gainCombatResurrectionCharge.CombatResChargeRecovery = _combatResurrectionTimer;
    instance->SendToPlayers(gainCombatResurrectionCharge.Write());
}

void InstanceScript::UseCombatResurrection()
{
    --_combatResurrectionCharges;

    instance->SendToPlayers(WorldPackets::Instance::InstanceEncounterInCombatResurrection().Write());
}

void InstanceScript::ResetCombatResurrections()
{
    _combatResurrectionCharges = 0;
    _combatResurrectionTimer = 0;
    _combatResurrectionTimerStarted = false;
}

uint32 InstanceScript::GetCombatResurrectionChargeInterval() const
{
    uint32 interval = 0;
    if (uint32 playerCount = instance->GetPlayers().getSize())
        interval = 90 * MINUTE * IN_MILLISECONDS / playerCount;

    return interval;
}

PersistentInstanceScriptValueBase::PersistentInstanceScriptValueBase(InstanceScript& instance, char const* name)
    : _instance(instance), _name(name)
{
    _instance.RegisterPersistentScriptValue(this);
}

void PersistentInstanceScriptValueBase::NotifyValueChanged()
{
    _instance.instance->UpdateInstanceLock(CreateEvent());
}

bool InstanceHasScript(WorldObject const* obj, char const* scriptName)
{
    if (InstanceMap* instance = obj->GetMap()->ToInstanceMap())
        return instance->GetScriptName() == scriptName;

    return false;
}
