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

#ifndef ViewerDependentValues_h__
#define ViewerDependentValues_h__

#include "Conversation.h"
#include "Creature.h"
#include "DB2Stores.h"
#include "GameObject.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "World.h"
#include "WorldSession.h"

namespace UF
{
template<typename Tag>
class ViewerDependentValue
{
};

template<>
class ViewerDependentValue<UF::ObjectData::EntryIDTag>
{
public:
    using value_type = UF::ObjectData::EntryIDTag::value_type;

    static value_type GetValue(UF::ObjectData const* objectData, Object const* object, Player const* receiver)
    {
        value_type entryId = objectData->EntryID;

        if (Unit const* unit = object->ToUnit())
            if (TempSummon const* summon = unit->ToTempSummon())
                if (summon->GetSummonerGUID() == receiver->GetGUID() && summon->GetCreatureIdVisibleToSummoner())
                    entryId = *summon->GetCreatureIdVisibleToSummoner();

        return entryId;
    }
};

template<>
class ViewerDependentValue<UF::ObjectData::DynamicFlagsTag>
{
public:
    using value_type = UF::ObjectData::DynamicFlagsTag::value_type;

    static value_type GetValue(UF::ObjectData const* objectData, Object const* object, Player const* receiver)
    {
        value_type dynamicFlags = objectData->DynamicFlags;
        if (Unit const* unit = object->ToUnit())
        {
            if (Creature const* creature = object->ToCreature())
            {
                if (dynamicFlags & UNIT_DYNFLAG_TAPPED && creature->isTappedBy(receiver))
                    dynamicFlags &= ~UNIT_DYNFLAG_TAPPED;

                if (dynamicFlags & UNIT_DYNFLAG_LOOTABLE && !receiver->isAllowedToLoot(creature))
                    dynamicFlags &= ~UNIT_DYNFLAG_LOOTABLE;

                if (dynamicFlags & UNIT_DYNFLAG_CAN_SKIN && creature->IsSkinnedBy(receiver))
                    dynamicFlags &= ~UNIT_DYNFLAG_CAN_SKIN;
            }

            // unit UNIT_DYNFLAG_TRACK_UNIT should only be sent to caster of SPELL_AURA_MOD_STALKED auras
            if (dynamicFlags & UNIT_DYNFLAG_TRACK_UNIT)
                if (!unit->HasAuraTypeWithCaster(SPELL_AURA_MOD_STALKED, receiver->GetGUID()))
                    dynamicFlags &= ~UNIT_DYNFLAG_TRACK_UNIT;
        }
        else if (GameObject const* gameObject = object->ToGameObject())
        {
            uint32 dynFlags = GO_DYNFLAG_LO_STATE_TRANSITION_ANIM_DONE;
            switch (gameObject->GetGoType())
            {
                case GAMEOBJECT_TYPE_BUTTON:
                case GAMEOBJECT_TYPE_GOOBER:
                    if (gameObject->HasConditionalInteraction() && gameObject->CanActivateForPlayer(receiver))
                        if (gameObject->GetGoStateFor(receiver->GetGUID()) != GO_STATE_ACTIVE)
                            dynFlags |= GO_DYNFLAG_LO_ACTIVATE | GO_DYNFLAG_LO_HIGHLIGHT;
                    break;
                case GAMEOBJECT_TYPE_QUESTGIVER:
                    if (gameObject->CanActivateForPlayer(receiver))
                        dynFlags |= GO_DYNFLAG_LO_ACTIVATE;
                    break;
                case GAMEOBJECT_TYPE_CHEST:
                    if (gameObject->HasConditionalInteraction() && gameObject->CanActivateForPlayer(receiver))
                        dynFlags |= GO_DYNFLAG_LO_ACTIVATE | GO_DYNFLAG_LO_SPARKLE | GO_DYNFLAG_LO_HIGHLIGHT;
                    else if (receiver->IsGameMaster())
                        dynFlags |= GO_DYNFLAG_LO_ACTIVATE | GO_DYNFLAG_LO_SPARKLE;
                    break;
                case GAMEOBJECT_TYPE_GENERIC:
                case GAMEOBJECT_TYPE_SPELL_FOCUS:
                    if (gameObject->HasConditionalInteraction() && gameObject->CanActivateForPlayer(receiver))
                        dynFlags |= GO_DYNFLAG_LO_SPARKLE;
                    break;
                case GAMEOBJECT_TYPE_TRANSPORT:
                case GAMEOBJECT_TYPE_MAP_OBJ_TRANSPORT:
                    dynFlags |= dynamicFlags;   // preserve all dynamicflgs
                    break;
                case GAMEOBJECT_TYPE_CAPTURE_POINT:
                    if (!gameObject->CanInteractWithCapturePoint(receiver))
                        dynFlags |= GO_DYNFLAG_LO_NO_INTERACT;
                    else
                        dynFlags &= ~GO_DYNFLAG_LO_NO_INTERACT;
                    break;
                case GAMEOBJECT_TYPE_GATHERING_NODE:
                    if (gameObject->HasConditionalInteraction() && gameObject->CanActivateForPlayer(receiver))
                        dynFlags |= GO_DYNFLAG_LO_ACTIVATE | GO_DYNFLAG_LO_SPARKLE | GO_DYNFLAG_LO_HIGHLIGHT;
                    if (gameObject->GetGoStateFor(receiver->GetGUID()) == GO_STATE_ACTIVE)
                        dynFlags |= GO_DYNFLAG_LO_DEPLETED;
                    break;
                default:
                    break;
            }

            if (!receiver->IsGameMaster())
            {
                // GO_DYNFLAG_LO_INTERACT_COND should be applied to GOs with conditional interaction (without GO_FLAG_INTERACT_COND) to disable interaction
                // (Ignore GAMEOBJECT_TYPE_GATHERING_NODE as some profession-related GOs may include quest loot and can always be interacted with)
                // (Ignore GAMEOBJECT_TYPE_FLAGSTAND as interaction is handled by GO_DYNFLAG_LO_NO_INTERACT)
                // (Ignore GAMEOBJECT_TYPE_SPELLCASTER as interaction is handled by GO_DYNFLAG_LO_NO_INTERACT)
                if (gameObject->GetGoType() != GAMEOBJECT_TYPE_GATHERING_NODE && gameObject->GetGoType() != GAMEOBJECT_TYPE_FLAGSTAND && gameObject->GetGoType() != GAMEOBJECT_TYPE_SPELLCASTER)
                    if (gameObject->HasConditionalInteraction() && !gameObject->HasFlag(GO_FLAG_INTERACT_COND))
                        dynFlags |= GO_DYNFLAG_LO_INTERACT_COND;

                if (!gameObject->MeetsInteractCondition(receiver))
                    dynFlags |= GO_DYNFLAG_LO_NO_INTERACT;

                if (SpawnMetadata const* data = sObjectMgr->GetSpawnMetadata(SPAWN_TYPE_GAMEOBJECT, gameObject->GetSpawnId()))
                    if (data->spawnTrackingData && !data->spawnTrackingQuestObjectives.empty())
                        if (receiver->GetSpawnTrackingStateByObjectives(data->spawnTrackingData->SpawnTrackingId, data->spawnTrackingQuestObjectives) != SpawnTrackingState::Active)
                            dynFlags &= ~GO_DYNFLAG_LO_ACTIVATE;
            }

            dynamicFlags = dynFlags;
        }

        return dynamicFlags;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::DisplayIDTag>
{
public:
    using value_type = UF::UnitData::DisplayIDTag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type displayId = unitData->DisplayID;
        if (unit->IsCreature())
        {
            CreatureTemplate const* cinfo = unit->ToCreature()->GetCreatureTemplate();

            if (TempSummon const* summon = unit->ToTempSummon())
            {
                if (summon->GetSummonerGUID() == receiver->GetGUID())
                {
                    if (summon->GetCreatureIdVisibleToSummoner())
                        cinfo = sObjectMgr->GetCreatureTemplate(*summon->GetCreatureIdVisibleToSummoner());

                    if (summon->GetDisplayIdVisibleToSummoner())
                        displayId = *summon->GetDisplayIdVisibleToSummoner();
                }
            }

            // this also applies for transform auras
            if (SpellInfo const* transform = sSpellMgr->GetSpellInfo(unit->GetTransformSpell(), unit->GetMap()->GetDifficultyID()))
            {
                for (SpellEffectInfo const& spellEffectInfo : transform->GetEffects())
                {
                    if (spellEffectInfo.IsAura(SPELL_AURA_TRANSFORM))
                    {
                        if (CreatureTemplate const* transformInfo = sObjectMgr->GetCreatureTemplate(spellEffectInfo.MiscValue))
                        {
                            cinfo = transformInfo;
                            break;
                        }
                    }
                }
            }

            if (cinfo->flags_extra & CREATURE_FLAG_EXTRA_TRIGGER)
                if (receiver->IsGameMaster())
                    displayId = cinfo->GetFirstVisibleModel()->CreatureDisplayID;
        }

        return displayId;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::StateWorldEffectIDsTag>
{
public:
    using value_type = UF::UnitData::StateWorldEffectIDsTag::value_type const*;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        if (unit->IsCreature())
            if (SpawnTrackingStateData const* spawnTrackingStateData = unit->GetSpawnTrackingStateDataForPlayer(receiver))
                return &spawnTrackingStateData->StateWorldEffects;

        return &*unitData->StateWorldEffectIDs;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::StateSpellVisualIDTag>
{
public:
    using value_type = UF::UnitData::StateSpellVisualIDTag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type stateSpellVisual = unitData->StateSpellVisualID;

        if (unit->IsCreature())
            if (SpawnTrackingStateData const* spawnTrackingStateData = unit->GetSpawnTrackingStateDataForPlayer(receiver))
                stateSpellVisual = spawnTrackingStateData->StateSpellVisualId.value_or(0);

        return stateSpellVisual;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::StateAnimIDTag>
{
public:
    using value_type = UF::UnitData::StateAnimIDTag::value_type;

    static value_type GetValue(UF::UnitData const* /*unitData*/, Unit const* unit, Player const* receiver)
    {
        value_type stateAnimId = sDB2Manager.GetEmptyAnimStateID();

        if (unit->IsCreature())
            if (SpawnTrackingStateData const* spawnTrackingStateData = unit->GetSpawnTrackingStateDataForPlayer(receiver))
                stateAnimId = spawnTrackingStateData->StateAnimId.value_or(stateAnimId);

        return stateAnimId;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::StateAnimKitIDTag>
{
public:
    using value_type = UF::UnitData::StateAnimKitIDTag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type stateAnimKitId = unitData->StateAnimKitID;

        if (unit->IsCreature())
            if (SpawnTrackingStateData const* spawnTrackingStateData = unit->GetSpawnTrackingStateDataForPlayer(receiver))
                stateAnimKitId = spawnTrackingStateData->StateAnimKitId.value_or(0);

        return stateAnimKitId;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::StateWorldEffectsQuestObjectiveIDTag>
{
public:
    using value_type = UF::UnitData::StateWorldEffectsQuestObjectiveIDTag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type stateWorldEffectsQuestObjectiveId = unitData->StateWorldEffectsQuestObjectiveID;

        if (!stateWorldEffectsQuestObjectiveId && unit->IsCreature())
        {
            if (CreatureData const* data = unit->ToCreature()->GetCreatureData())
            {
                auto itr = data->spawnTrackingQuestObjectives.begin();
                auto end = data->spawnTrackingQuestObjectives.end();
                if (itr != end)
                {
                    // If there is no valid objective for player, fill UF with first objective (if any)
                    stateWorldEffectsQuestObjectiveId = *itr;
                    while (++itr != end)
                    {
                        if (receiver->GetSpawnTrackingStateByObjective(data->spawnTrackingData->SpawnTrackingId, *itr) != SpawnTrackingState::Active)
                            continue;

                        stateWorldEffectsQuestObjectiveId = *itr;
                        break;
                    }
                }
            }
        }

        return stateWorldEffectsQuestObjectiveId;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::FactionTemplateTag>
{
public:
    using value_type = UF::UnitData::FactionTemplateTag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type factionTemplate = unitData->FactionTemplate;
        if (unit->IsControlledByPlayer() && receiver != unit && sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP) && unit->IsInRaidWith(receiver))
        {
            FactionTemplateEntry const* ft1 = unit->GetFactionTemplateEntry();
            FactionTemplateEntry const* ft2 = receiver->GetFactionTemplateEntry();
            if (ft1 && ft2 && !ft1->IsFriendlyTo(ft2))
                // pretend that all other HOSTILE players have own faction, to allow follow, heal, rezz (trade wont work)
                factionTemplate = receiver->GetFaction();
        }

        return factionTemplate;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::FlagsTag>
{
public:
    using value_type = UF::UnitData::FlagsTag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* /*unit*/, Player const* receiver)
    {
        value_type flags = unitData->Flags;
        // Gamemasters should be always able to interact with units - remove uninteractible flag
        if (receiver->IsGameMaster())
            flags &= ~UNIT_FLAG_UNINTERACTIBLE;

        return flags;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::Flags2Tag>
{
public:
    using value_type = UF::UnitData::Flags2Tag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* /*unit*/, Player const* receiver)
    {
        value_type flags = unitData->Flags2;
        // Gamemasters should be always able to interact with units - remove uninteractible flag
        if (receiver->IsGameMaster())
            flags &= ~UNIT_FLAG2_UNTARGETABLE_BY_CLIENT;

        return flags;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::Flags3Tag>
{
public:
    using value_type = UF::UnitData::Flags3Tag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type flags = unitData->Flags3;
        if (flags & UNIT_FLAG3_ALREADY_SKINNED && unit->IsCreature() && !unit->ToCreature()->IsSkinnedBy(receiver))
            flags &= ~UNIT_FLAG3_ALREADY_SKINNED;

        return flags;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::Flags4Tag>
{
public:
    using value_type = UF::UnitData::Flags4Tag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* /*unit*/, Player const* /*receiver*/)
    {
        return unitData->Flags4;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::AuraStateTag>
{
public:
    using value_type = UF::UnitData::AuraStateTag::value_type;

    static value_type GetValue(UF::UnitData const* /*unitData*/, Unit const* unit, Player const* receiver)
    {
        // Check per caster aura states to not enable using a spell in client if specified aura is not by target
        return unit->BuildAuraStateUpdateForTarget(receiver);
    }
};

template<>
class ViewerDependentValue<UF::UnitData::PvpFlagsTag>
{
public:
    using value_type = UF::UnitData::PvpFlagsTag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type pvpFlags = unitData->PvpFlags;
        if (unit->IsControlledByPlayer() && receiver != unit && sWorld->getBoolConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GROUP) && unit->IsInRaidWith(receiver))
        {
            FactionTemplateEntry const* ft1 = unit->GetFactionTemplateEntry();
            FactionTemplateEntry const* ft2 = receiver->GetFactionTemplateEntry();
            if (ft1 && ft2 && !ft1->IsFriendlyTo(ft2))
                // Allow targeting opposite faction in party when enabled in config
                pvpFlags &= UNIT_BYTE2_FLAG_SANCTUARY;
        }

        return pvpFlags;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::InteractSpellIDTag>
{
public:
    using value_type = UF::UnitData::InteractSpellIDTag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type interactSpellId = unitData->InteractSpellID;
        if (unitData->NpcFlags & UNIT_NPC_FLAG_SPELLCLICK && !interactSpellId)
        {
            // this field is not set if there are multiple available spellclick spells
            auto clickBounds = sObjectMgr->GetSpellClickInfoMapBounds(unit->GetEntry());
            for (auto const& [creatureId, spellClickInfo] : clickBounds)
            {
                if (!spellClickInfo.IsFitToRequirements(receiver, unit))
                    continue;

                if (!sConditionMgr->IsObjectMeetingSpellClickConditions(unit->GetEntry(), spellClickInfo.spellId, receiver, unit))
                    continue;

                interactSpellId = spellClickInfo.spellId;
                break;
            }

        }
        return interactSpellId;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::NpcFlagsTag>
{
public:
    using value_type = UF::UnitData::NpcFlagsTag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type npcFlag = unitData->NpcFlags;
        if (npcFlag)
        {
            if ((!unit->IsInteractionAllowedInCombat() && unit->IsInCombat())
               || (!unit->IsInteractionAllowedWhileHostile() && unit->IsHostileTo(receiver)))
                npcFlag = 0;
            else if (Creature const* creature = unit->ToCreature())
            {
                if (!receiver->CanSeeGossipOn(creature))
                    npcFlag &= ~(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);

                if (!receiver->CanSeeSpellClickOn(creature))
                    npcFlag &= ~UNIT_NPC_FLAG_SPELLCLICK;
            }
        }
        return npcFlag;
    }
};

template<>
class ViewerDependentValue<UF::UnitData::NpcFlags2Tag>
{
public:
    using value_type = UF::UnitData::NpcFlags2Tag::value_type;

    static value_type GetValue(UF::UnitData const* unitData, Unit const* unit, Player const* receiver)
    {
        value_type npcFlag = unitData->NpcFlags2;
        if (npcFlag)
        {
            if ((!unit->IsInteractionAllowedInCombat() && unit->IsInCombat())
               || (!unit->IsInteractionAllowedWhileHostile() && unit->IsHostileTo(receiver)))
                npcFlag = 0;
        }
        return npcFlag;
    }
};

template<>
class ViewerDependentValue<UF::GameObjectData::StateWorldEffectIDsTag>
{
public:
    using value_type = UF::GameObjectData::StateWorldEffectIDsTag::value_type const*;

    static value_type GetValue(UF::GameObjectData const* gameObjectData, GameObject const* gameObject, Player const* receiver)
    {
        if (SpawnTrackingStateData const* spawnTrackingStateData = gameObject->GetSpawnTrackingStateDataForPlayer(receiver))
            return &spawnTrackingStateData->StateWorldEffects;

        return &*gameObjectData->StateWorldEffectIDs;
    }
};

template<>
class ViewerDependentValue<UF::GameObjectData::StateSpellVisualIDTag>
{
public:
    using value_type = UF::GameObjectData::StateSpellVisualIDTag::value_type;

    static value_type GetValue(UF::GameObjectData const* gameObjectData, GameObject const* gameObject, Player const* receiver)
    {
        value_type stateSpellVisual = gameObjectData->StateSpellVisualID;

        if (SpawnTrackingStateData const* spawnTrackingStateData = gameObject->GetSpawnTrackingStateDataForPlayer(receiver))
            stateSpellVisual = spawnTrackingStateData->StateSpellVisualId.value_or(0);

        return stateSpellVisual;
    }
};

template<>
class ViewerDependentValue<UF::GameObjectData::SpawnTrackingStateAnimIDTag>
{
public:
    using value_type = UF::GameObjectData::SpawnTrackingStateAnimIDTag::value_type;

    static value_type GetValue(UF::GameObjectData const* /*gameObjectData*/, GameObject const* gameObject, Player const* receiver)
    {
        value_type stateAnimId = sDB2Manager.GetEmptyAnimStateID();

        if (SpawnTrackingStateData const* spawnTrackingStateData = gameObject->GetSpawnTrackingStateDataForPlayer(receiver))
            stateAnimId = spawnTrackingStateData->StateAnimId.value_or(stateAnimId);

        return stateAnimId;
    }
};

template<>
class ViewerDependentValue<UF::GameObjectData::SpawnTrackingStateAnimKitIDTag>
{
public:
    using value_type = UF::GameObjectData::SpawnTrackingStateAnimKitIDTag::value_type;

    static value_type GetValue(UF::GameObjectData const* gameObjectData, GameObject const* gameObject, Player const* receiver)
    {
        value_type stateAnimKitId = gameObjectData->SpawnTrackingStateAnimKitID;

        if (SpawnTrackingStateData const* spawnTrackingStateData = gameObject->GetSpawnTrackingStateDataForPlayer(receiver))
            stateAnimKitId = spawnTrackingStateData->StateAnimKitId.value_or(0);

        return stateAnimKitId;
    }
};

template<>
class ViewerDependentValue<UF::GameObjectData::StateWorldEffectsQuestObjectiveIDTag>
{
public:
    using value_type = UF::GameObjectData::StateWorldEffectsQuestObjectiveIDTag::value_type;

    static value_type GetValue(UF::GameObjectData const* gameObjectData, GameObject const* gameObject, Player const* receiver)
    {
        value_type stateWorldEffectsQuestObjectiveId = gameObjectData->StateWorldEffectsQuestObjectiveID;

        if (!stateWorldEffectsQuestObjectiveId)
        {
            if (::GameObjectData const* data = gameObject->GetGameObjectData())
            {
                auto itr = data->spawnTrackingQuestObjectives.begin();
                auto end = data->spawnTrackingQuestObjectives.end();
                if (itr != end)
                {
                    // If there is no valid objective for player, fill UF with first objective (if any)
                    stateWorldEffectsQuestObjectiveId = *itr;
                    while (++itr != end)
                    {
                        if (receiver->GetSpawnTrackingStateByObjective(data->spawnTrackingData->SpawnTrackingId, *itr) != SpawnTrackingState::Active)
                            continue;

                        stateWorldEffectsQuestObjectiveId = *itr;
                        break;
                    }
                }
            }
        }

        return stateWorldEffectsQuestObjectiveId;
    }
};

template<>
class ViewerDependentValue<UF::GameObjectData::FlagsTag>
{
public:
    using value_type = UF::GameObjectData::FlagsTag::value_type;

    static value_type GetValue(UF::GameObjectData const* gameObjectData, GameObject const* gameObject, Player const* receiver)
    {
        value_type flags = gameObjectData->Flags;
        if (gameObject->GetGoType() == GAMEOBJECT_TYPE_CHEST)
            if (gameObject->GetGOInfo()->chest.usegrouplootrules && !gameObject->IsLootAllowedFor(receiver))
                flags |= GO_FLAG_LOCKED | GO_FLAG_NOT_SELECTABLE;

        return flags;
    }
};

template<>
class ViewerDependentValue<UF::GameObjectData::StateTag>
{
public:
    using value_type = UF::GameObjectData::StateTag::value_type;

    static value_type GetValue(UF::GameObjectData const* /*gameObjectData*/, GameObject const* gameObject, Player const* receiver)
    {
        return gameObject->GetGoStateFor(receiver->GetGUID());
    }
};

template<>
class ViewerDependentValue<UF::ConversationData::LastLineEndTimeTag>
{
public:
    using value_type = UF::ConversationData::LastLineEndTimeTag::value_type;

    static value_type GetValue(UF::ConversationData const* /*conversationData*/, Conversation const* conversation, Player const* receiver)
    {
        LocaleConstant locale = receiver->GetSession()->GetSessionDbLocaleIndex();
        return conversation->GetLastLineEndTime(locale).count();
    }
};

template<>
class ViewerDependentValue<UF::ConversationLine::StartTimeTag>
{
public:
    using value_type = UF::ConversationLine::StartTimeTag::value_type;

    static value_type GetValue(UF::ConversationLine const* conversationLineData, Conversation const* conversation, Player const* receiver)
    {
        value_type startTime = conversationLineData->StartTime;
        LocaleConstant locale = receiver->GetSession()->GetSessionDbLocaleIndex();

        if (Milliseconds const* localizedStartTime = conversation->GetLineStartTime(locale, conversationLineData->ConversationLineID))
            startTime = localizedStartTime->count();

        return startTime;
    }
};
}

#endif // ViewerDependentValues_h__
