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

/* ScriptData
Name: instance_commandscript
%Complete: 100
Comment: All instance related commands
Category: commandscripts
EndScriptData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceLockMgr.h"
#include "InstanceScript.h"
#include "Language.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RBAC.h"
#include "Util.h"
#include "World.h"
#include "WorldSession.h"
#include <sstream>

class instance_commandscript : public CommandScript
{
public:
    instance_commandscript() : CommandScript("instance_commandscript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> instanceCommandTable =
        {
            { "listbinds",    rbac::RBAC_PERM_COMMAND_INSTANCE_LISTBINDS,     false, &HandleInstanceListBindsCommand,    "" },
            { "unbind",       rbac::RBAC_PERM_COMMAND_INSTANCE_UNBIND,        false, &HandleInstanceUnbindCommand,       "" },
            { "stats",        rbac::RBAC_PERM_COMMAND_INSTANCE_STATS,          true, &HandleInstanceStatsCommand,        "" },
            { "setbossstate", rbac::RBAC_PERM_COMMAND_INSTANCE_SET_BOSS_STATE, true, &HandleInstanceSetBossStateCommand, "" },
            { "getbossstate", rbac::RBAC_PERM_COMMAND_INSTANCE_GET_BOSS_STATE, true, &HandleInstanceGetBossStateCommand, "" },
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "instance", rbac::RBAC_PERM_COMMAND_INSTANCE,  true, nullptr, "", instanceCommandTable },
        };

        return commandTable;
    }

    static bool HandleInstanceListBindsCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->getSelectedPlayer();
        if (!player)
            player = handler->GetSession()->GetPlayer();

        InstanceResetTimePoint now = GameTime::GetGameTimeSystemPoint();
        std::vector<InstanceLock const*> instanceLocks = sInstanceLockMgr.GetInstanceLocksForPlayer(player->GetGUID());
        for (InstanceLock const* instanceLock : instanceLocks)
        {
            MapDb2Entries entries{ instanceLock->GetMapId(), instanceLock->GetDifficultyId() };
            std::string timeleft = !instanceLock->IsExpired() ? secsToTimeString(std::chrono::duration_cast<Seconds>(instanceLock->GetEffectiveExpiryTime() - now).count()) : "-";
            handler->PSendSysMessage(LANG_COMMAND_LIST_BIND_INFO,
                entries.Map->ID, entries.Map->MapName[sWorld->GetDefaultDbcLocale()],
                uint32(entries.MapDifficulty->DifficultyID), sDifficultyStore.AssertEntry(entries.MapDifficulty->DifficultyID)->Name[sWorld->GetDefaultDbcLocale()],
                instanceLock->GetInstanceId(),
                handler->GetTrinityString(instanceLock->IsExpired() ? LANG_YES : LANG_NO),
                handler->GetTrinityString(instanceLock->IsExtended() ? LANG_YES : LANG_NO),
                timeleft.c_str());
        }

        handler->PSendSysMessage(LANG_COMMAND_LIST_BIND_PLAYER_BINDS, uint32(instanceLocks.size()));
        return true;
    }

    static bool HandleInstanceUnbindCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        Player* player = handler->getSelectedPlayer();
        if (!player)
            player = handler->GetSession()->GetPlayer();

        char const* mapStr = strtok((char*)args, " ");
        char const* difficultyStr = strtok(nullptr, " ");

        Optional<uint32> mapId;
        Optional<Difficulty> difficulty;

        if (difficultyStr)
            difficulty = Difficulty(atoul(difficultyStr));

        if (strcmp(mapStr, "all") != 0)
        {
            mapId = uint32(atoul(mapStr));
            if (!mapId)
                return false;
        }

        std::vector<InstanceLock const*> locksReset;
        std::vector<InstanceLock const*> locksNotReset;

        sInstanceLockMgr.ResetInstanceLocksForPlayer(player->GetGUID(), mapId, difficulty, &locksReset, &locksNotReset);

        InstanceResetTimePoint now = GameTime::GetGameTimeSystemPoint();
        for (InstanceLock const* instanceLock : locksReset)
        {
            MapDb2Entries entries{ instanceLock->GetMapId(), instanceLock->GetDifficultyId() };
            std::string timeleft = !instanceLock->IsExpired() ? secsToTimeString(std::chrono::duration_cast<Seconds>(instanceLock->GetEffectiveExpiryTime() - now).count()) : "-";
            handler->PSendSysMessage(LANG_COMMAND_INST_UNBIND_UNBINDING,
                entries.Map->ID, entries.Map->MapName[sWorld->GetDefaultDbcLocale()],
                uint32(entries.MapDifficulty->DifficultyID), sDifficultyStore.AssertEntry(entries.MapDifficulty->DifficultyID)->Name[sWorld->GetDefaultDbcLocale()],
                instanceLock->GetInstanceId(),
                handler->GetTrinityString(instanceLock->IsExpired() ? LANG_YES : LANG_NO),
                handler->GetTrinityString(instanceLock->IsExtended() ? LANG_YES : LANG_NO),
                timeleft.c_str());
        }

        handler->PSendSysMessage(LANG_COMMAND_INST_UNBIND_UNBOUND, uint32(locksReset.size()));

        for (InstanceLock const* instanceLock : locksNotReset)
        {
            MapDb2Entries entries{ instanceLock->GetMapId(), instanceLock->GetDifficultyId() };
            std::string timeleft = !instanceLock->IsExpired() ? secsToTimeString(std::chrono::duration_cast<Seconds>(instanceLock->GetEffectiveExpiryTime() - now).count()) : "-";
            handler->PSendSysMessage(LANG_COMMAND_INST_UNBIND_FAILED,
                entries.Map->ID, entries.Map->MapName[sWorld->GetDefaultDbcLocale()],
                uint32(entries.MapDifficulty->DifficultyID), sDifficultyStore.AssertEntry(entries.MapDifficulty->DifficultyID)->Name[sWorld->GetDefaultDbcLocale()],
                instanceLock->GetInstanceId(),
                handler->GetTrinityString(instanceLock->IsExpired() ? LANG_YES : LANG_NO),
                handler->GetTrinityString(instanceLock->IsExtended() ? LANG_YES : LANG_NO),
                timeleft.c_str());
        }

        player->SendRaidInfo();

        return true;
    }

    static bool HandleInstanceStatsCommand(ChatHandler* handler, char const* /*args*/)
    {
        handler->PSendSysMessage(LANG_COMMAND_INST_STAT_LOADED_INST, sMapMgr->GetNumInstances());
        handler->PSendSysMessage(LANG_COMMAND_INST_STAT_PLAYERS_IN, sMapMgr->GetNumPlayersInInstances());

        InstanceLocksStatistics statistics = sInstanceLockMgr.GetStatistics();

        handler->PSendSysMessage(LANG_COMMAND_INST_STAT_SAVES, statistics.InstanceCount);
        handler->PSendSysMessage(LANG_COMMAND_INST_STAT_PLAYERSBOUND, statistics.PlayerCount);

        return true;
    }

    static bool HandleInstanceSetBossStateCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char* param1 = strtok((char*)args, " ");
        char* param2 = strtok(nullptr, " ");
        char* param3 = strtok(nullptr, " ");
        uint32 encounterId = 0;
        int32 state = 0;
        Player* player = nullptr;
        std::string playerName;

        // Character name must be provided when using this from console.
        if (!param2 || (!param3 && !handler->GetSession()))
        {
            handler->PSendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!param3)
            player = handler->GetSession()->GetPlayer();
        else
        {
            playerName = param3;
            if (normalizePlayerName(playerName))
                player = ObjectAccessor::FindPlayerByName(playerName);
        }

        if (!player)
        {
            handler->PSendSysMessage(LANG_PLAYER_NOT_FOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        InstanceMap* map = player->GetMap()->ToInstanceMap();
        if (!map)
        {
            handler->PSendSysMessage(LANG_NOT_DUNGEON);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!map->GetInstanceScript())
        {
            handler->PSendSysMessage(LANG_NO_INSTANCE_DATA);
            handler->SetSentErrorMessage(true);
            return false;
        }

        encounterId = atoul(param1);
        state = atoi(param2);

        // Reject improper values.
        if (state > TO_BE_DECIDED || encounterId > map->GetInstanceScript()->GetEncounterCount())
        {
            handler->PSendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        map->GetInstanceScript()->SetBossState(encounterId, EncounterState(state));
        char const* stateName = InstanceScript::GetBossStateName(EncounterState(state));
        handler->PSendSysMessage(LANG_COMMAND_INST_SET_BOSS_STATE, encounterId, state, stateName);
        return true;
    }

    static bool HandleInstanceGetBossStateCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
            return false;

        char* param1 = strtok((char*)args, " ");
        char* param2 = strtok(nullptr, " ");
        uint32 encounterId = 0;
        Player* player = nullptr;
        std::string playerName;

        // Character name must be provided when using this from console.
        if (!param1 || (!param2 && !handler->GetSession()))
        {
            handler->PSendSysMessage(LANG_CMD_SYNTAX);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!param2)
            player = handler->GetSession()->GetPlayer();
        else
        {
            playerName = param2;
            if (normalizePlayerName(playerName))
                player = ObjectAccessor::FindPlayerByName(playerName);
        }

        if (!player)
        {
            handler->PSendSysMessage(LANG_PLAYER_NOT_FOUND);
            handler->SetSentErrorMessage(true);
            return false;
        }

        InstanceMap* map = player->GetMap()->ToInstanceMap();
        if (!map)
        {
            handler->PSendSysMessage(LANG_NOT_DUNGEON);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!map->GetInstanceScript())
        {
            handler->PSendSysMessage(LANG_NO_INSTANCE_DATA);
            handler->SetSentErrorMessage(true);
            return false;
        }

        encounterId = atoul(param1);

        if (encounterId > map->GetInstanceScript()->GetEncounterCount())
        {
            handler->PSendSysMessage(LANG_BAD_VALUE);
            handler->SetSentErrorMessage(true);
            return false;
        }

        int32 state = map->GetInstanceScript()->GetBossState(encounterId);
        char const* stateName = InstanceScript::GetBossStateName(EncounterState(state));
        handler->PSendSysMessage(LANG_COMMAND_INST_GET_BOSS_STATE, encounterId, state, stateName);
        return true;
    }
};

void AddSC_instance_commandscript()
{
    new instance_commandscript();
}
