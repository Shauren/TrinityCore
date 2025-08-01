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

#ifndef TRINITYCORE_CONVERSATION_H
#define TRINITYCORE_CONVERSATION_H

#include "Object.h"
#include "GridObject.h"

class ConversationAI;
class Unit;
class SpellInfo;
enum class ConversationActorType : uint32;

class TC_GAME_API Conversation final : public WorldObject, public GridObject<Conversation>
{
    public:
        Conversation();
        ~Conversation();

    protected:
        void BuildValuesCreate(ByteBuffer* data, UF::UpdateFieldFlag flags, Player const* target) const override;
        void BuildValuesUpdate(ByteBuffer* data, UF::UpdateFieldFlag flags, Player const* target) const override;
        void ClearUpdateMask(bool remove) override;

    public:
        void BuildValuesUpdateForPlayerWithMask(UpdateData* data, UF::ObjectData::Mask const& requestedObjectMask,
            UF::ConversationData::Mask const& requestedConversationMask, Player const* target) const;

        struct ValuesUpdateForPlayerWithMaskSender // sender compatible with MessageDistDeliverer
        {
            explicit ValuesUpdateForPlayerWithMaskSender(Conversation const* owner) : Owner(owner) { }

            Conversation const* Owner;
            UF::ObjectData::Base ObjectMask;
            UF::ConversationData::Base ConversationMask;

            void operator()(Player const* player) const;
        };

        void AddToWorld() override;
        void RemoveFromWorld() override;

        void Update(uint32 diff) override;
        void Remove();
        Milliseconds GetDuration() const { return _duration; }
        uint32 GetTextureKitId() const { return _textureKitId; }

        static Conversation* CreateConversation(uint32 conversationEntry, Unit* creator, Position const& pos, ObjectGuid privateObjectOwner, SpellInfo const* spellInfo = nullptr, bool autoStart = true);
        void Create(ObjectGuid::LowType lowGuid, uint32 conversationEntry, Map* map, Unit* creator, Position const& pos, ObjectGuid privateObjectOwner, SpellInfo const* spellInfo = nullptr);
        bool Start();
        void AddActor(int32 actorId, uint32 actorIdx, ObjectGuid const& actorGuid);
        void AddActor(int32 actorId, uint32 actorIdx, ConversationActorType type, uint32 creatureId, uint32 creatureDisplayInfoId);

        ObjectGuid GetCreatorGUID() const override { return _creatorGuid; }
        ObjectGuid GetOwnerGUID() const override { return GetCreatorGUID(); }
        uint32 GetFaction() const override { return 0; }

        Position const& GetStationaryPosition() const override { return _stationaryPosition; }
        void RelocateStationaryPosition(Position const& pos) { _stationaryPosition.Relocate(pos); }

        Milliseconds const* GetLineStartTime(LocaleConstant locale, int32 lineId) const;
        Milliseconds GetLastLineEndTime(LocaleConstant locale) const;
        static int32 GetLineDuration(LocaleConstant locale, int32 lineId);
        Milliseconds GetLineEndTime(LocaleConstant locale, int32 lineId) const;

        LocaleConstant GetPrivateObjectOwnerLocale() const;
        Unit* GetActorUnit(uint32 actorIdx) const;
        Creature* GetActorCreature(uint32 actorIdx) const;

        void AI_Initialize();
        void AI_Destroy();

        ConversationAI* AI() { return _ai.get(); }
        uint32 GetScriptId() const;

        UF::UpdateField<UF::ConversationData, int32(WowCS::EntityFragment::CGObject), TYPEID_CONVERSATION> m_conversationData;

    private:
        Position _stationaryPosition;
        ObjectGuid _creatorGuid;
        Milliseconds _duration;
        uint32 _textureKitId;

        std::unordered_map<int32 /*lineId*/, std::array<Milliseconds, TOTAL_LOCALES> /*startTime*/> _lineStartTimes;
        std::array<Milliseconds /*endTime*/, TOTAL_LOCALES> _lastLineEndTimes;

        std::unique_ptr<ConversationAI> _ai;
};

#endif // TRINITYCORE_CONVERSATION_H
