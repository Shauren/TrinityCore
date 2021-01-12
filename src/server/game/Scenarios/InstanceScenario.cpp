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

#include "InstanceScenario.h"
#include "Containers.h"
#include "DB2Stores.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScenarioMgr.h"

InstanceScenario::InstanceScenario(InstanceMap const* map, ScenarioData const* scenarioData) : Scenario(scenarioData), _map(map)
{
    ASSERT(_map);
    LoadInstanceData();

    Map::PlayerList const& players = map->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        if (Player* player = itr->GetSource()->ToPlayer())
            SendScenarioState(player);
}

void InstanceScenario::LoadInstanceData()
{
    InstanceScript const* instanceScript = _map->GetInstanceScript();
    if (!instanceScript)
        return;

    std::vector<CriteriaTree const*> criteriaTrees;

    if (CriteriaList const* criterias = sCriteriaMgr->GetScenarioCriteriaByTypeAndScenario(CRITERIA_TYPE_KILL_CREATURE, _data->Entry->ID))
    {
        if (std::vector<InstanceSpawnGroupInfo> const* spawnGroups = sObjectMgr->GetSpawnGroupsForInstance(_map->GetId()))
        {
            std::unordered_map<uint32, uint64> despawnedCreatureCountsById;
            for (InstanceSpawnGroupInfo const& spawnGroup : *spawnGroups)
            {
                if (instanceScript->GetBossState(spawnGroup.BossStateId) != DONE)
                    continue;

                bool isDespawned = !((1 << DONE) & spawnGroup.BossStates) || (spawnGroup.Flags & InstanceSpawnGroupInfo::FLAG_BLOCK_SPAWN);
                if (isDespawned)
                    for (auto&& spawnObject : sObjectMgr->GetSpawnDataForGroup(spawnGroup.SpawnGroupId))
                        ++despawnedCreatureCountsById[spawnObject.second->id];
            }

            for (Criteria const* criteria : *criterias)
            {
                // count creatures in despawned spawn groups
                if (uint64* progress = Trinity::Containers::MapGetValuePtr(despawnedCreatureCountsById, criteria->Entry->Asset.CreatureID))
                {
                    SetCriteriaProgress(criteria, *progress, nullptr, PROGRESS_SET);

                    if (CriteriaTreeList const* trees = sCriteriaMgr->GetCriteriaTreesByCriteria(criteria->ID))
                        for (CriteriaTree const* tree : *trees)
                            criteriaTrees.push_back(tree);
                }
            }
        }
    }

    if (CriteriaList const* criterias = sCriteriaMgr->GetScenarioCriteriaByTypeAndScenario(CRITERIA_TYPE_COMPLETE_DUNGEON_ENCOUNTER, _data->Entry->ID))
    {
        for (Criteria const* criteria : *criterias)
        {
            if (!instanceScript->IsEncounterCompleted(criteria->Entry->Asset.DungeonEncounterID))
                continue;

            SetCriteriaProgress(criteria, 1, nullptr, PROGRESS_SET);

            if (CriteriaTreeList const* trees = sCriteriaMgr->GetCriteriaTreesByCriteria(criteria->ID))
                for (CriteriaTree const* tree : *trees)
                    criteriaTrees.push_back(tree);
        }
    }

    for (CriteriaTree const* tree : criteriaTrees)
    {
        ScenarioStepEntry const* step = tree->ScenarioStep;
        if (!step)
            continue;

        if (IsCompletedCriteriaTree(tree))
            SetStepState(step, SCENARIO_STEP_DONE);
    }
}

std::string InstanceScenario::GetOwnerInfo() const
{
    return Trinity::StringFormat("Instance ID %u", _map->GetInstanceId());
}

void InstanceScenario::SendPacket(WorldPacket const* data) const
{
    _map->SendToPlayers(data);
}
