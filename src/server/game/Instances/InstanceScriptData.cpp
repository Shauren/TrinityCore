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

#include "InstanceScriptData.h"
#include "Containers.h"
#include "DB2Stores.h"
#include "InstanceScript.h"
#include "Log.h"
#include "Map.h"
#include "World.h"
#include <rapidjson/error/en.h>
#include <rapidjson/pointer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace
{
    std::string const HeadersKey = "Header";

    std::string const BossStatesSaveDataKey = "BossStates";
    std::string const BossStateStateKey = "State";

    std::string const MoreSaveDataKey = "AdditionalData";
}

InstanceScriptDataReader::Result InstanceScriptDataReader::Load(char const* data)
{
    /*
       Expected JSON

        {
            "Header": "HEADER_STRING_SET_BY_SCRIPT",
            "BossStates": {
                "0": {
                    "State": "DONE"
                },
                "1": {
                    "State": "NOT_STARTED"
                }
                ...
            },
            "AdditionalData: { // optional
                "ExtraKey1": 123
                "AnotherExtraKey": 2.0
            }
        }
    */
    if (_doc.Parse(data).HasParseError())
    {
        TC_LOG_ERROR("scripts.data.load", "JSON parser error %s at " SZFMTD " while loading data for instance %u [%u-%s | %u-%s]",
            rapidjson::GetParseError_En(_doc.GetParseError()), _doc.GetErrorOffset(),
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::MalformedJson;
    }

    if (!_doc.IsObject())
    {
        TC_LOG_ERROR("scripts.data.load", "Root JSON value is not an object for instance %u [%u-%s | %u-%s]",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::RootIsNotAnObject;
    }

    Result result = ParseHeader();
    if (result != Result::Ok)
        return result;

    result = ParseBossStates();
    if (result != Result::Ok)
        return result;

    result = ParseAdditionalData();
    if (result != Result::Ok)
        return result;

    return Result::Ok;
}

InstanceScriptDataReader::Result InstanceScriptDataReader::ParseHeader()
{
    auto headerItr = _doc.FindMember(HeadersKey);
    if (headerItr == _doc.MemberEnd())
    {
        TC_LOG_ERROR("scripts.data.load", "Missing data header for instance %u [%u-%s | %u-%s]",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::MissingHeader;
    }

    if (headerItr->value != _instance.GetHeader())
    {
        TC_LOG_ERROR("scripts.data.load", "Incorrect data header for instance %u [%u-%s | %u-%s], expected \"%s\" got \"%s\"",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName(),
            _instance.GetHeader().c_str(), headerItr->value.IsString() ? headerItr->value.GetString() : "");
        return Result::UnexpectedHeader;
    }

    return Result::Ok;
}

InstanceScriptDataReader::Result InstanceScriptDataReader::ParseBossStates()
{
    auto bossStatesItr = _doc.FindMember(BossStatesSaveDataKey);
    if (bossStatesItr == _doc.MemberEnd())
    {
        TC_LOG_ERROR("scripts.data.load", "Missing boss states for instance %u [%u-%s | %u-%s]",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::MissingBossStates;
    }

    if (!bossStatesItr->value.IsObject())
    {
        TC_LOG_ERROR("scripts.data.load", "Boss states is not an object for instance %u [%u-%s | %u-%s]",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::BossStatesIsNotAnObject;
    }

    for (auto bossItr = bossStatesItr->value.MemberBegin(); bossItr != bossStatesItr->value.MemberEnd(); ++bossItr)
    {
        uint32 bossId = atoul(bossItr->name.GetString());
        if (bossId >= _instance.GetEncounterCount())
        {
            TC_LOG_ERROR("scripts.data.load", "Boss states has entry for boss with higher id (%u) than number of bosses (%u) for instance %u [%u-%s | %u-%s]",
                bossId, _instance.GetEncounterCount(), GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
            return Result::UnknownBoss;
        }

        if (!bossItr->value.IsObject())
        {
            TC_LOG_ERROR("scripts.data.load", "Boss state for boss (%u) is not an object for instance %u [%u-%s | %u-%s]",
                bossId, GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
            return Result::BossStateIsNotAnObject;
        }

        auto stateItr = bossItr->value.FindMember(BossStateStateKey);
        if (stateItr == bossItr->value.MemberEnd())
        {
            TC_LOG_ERROR("scripts.data.load", "Boss state for boss (%u) is missing state value for instance %u [%u-%s | %u-%s]",
                bossId, GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
            return Result::MissingBossState;
        }

        if (!stateItr->value.IsString())
        {
            TC_LOG_ERROR("scripts.data.load", "Boss state for boss (%u) state value is not a string for instance %u [%u-%s | %u-%s]",
                bossId, GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
            return Result::BossStateValueIsNotANumber;
        }

        EncounterState state = InstanceScript::GetBossStateFromName(stateItr->value.GetString());
        if (state == IN_PROGRESS || state == FAIL || state == SPECIAL)
            state = NOT_STARTED;

        if (state < TO_BE_DECIDED)
            _instance.SetBossState(bossId, state);
    }

    return Result::Ok;
}

InstanceScriptDataReader::Result InstanceScriptDataReader::ParseAdditionalData()
{
    auto moreDataItr = _doc.FindMember(MoreSaveDataKey);
    if (moreDataItr == _doc.MemberEnd())
        return Result::Ok;

    if (!moreDataItr->value.IsObject())
    {
        TC_LOG_ERROR("scripts.data.load", "Additional data is not an object for instance %u [%u-%s | %u-%s]",
            GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
        return Result::AdditionalDataIsNotAnObject;
    }

    for (PersistentInstanceScriptValueBase* value : _instance.GetPersistentScriptValues())
    {
        auto valueItr = moreDataItr->value.FindMember(value->GetName());
        if (valueItr != moreDataItr->value.MemberEnd() && !valueItr->value.IsNull())
        {
            switch (value->GetType())
            {
                case UpdateAdditionalSaveDataEvent::ValueType::Int32:
                    if (!valueItr->value.IsNumber())
                    {
                        TC_LOG_ERROR("scripts.data.load", "Additional data value for key %s is not a number for instance %u [%u-%s | %u-%s]",
                            value->GetName(), GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
                        return Result::AdditionalDataUnexpectedValueType;
                    }
                    static_cast<PersistentInstanceScriptValue<int32>*>(value)->LoadValue(valueItr->value.GetInt());
                    break;
                case UpdateAdditionalSaveDataEvent::ValueType::UInt32:
                    if (!valueItr->value.IsNumber())
                    {
                        TC_LOG_ERROR("scripts.data.load", "Additional data value for key %s is not a number for instance %u [%u-%s | %u-%s]",
                            value->GetName(), GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
                        return Result::AdditionalDataUnexpectedValueType;
                    }
                    static_cast<PersistentInstanceScriptValue<uint32>*>(value)->LoadValue(valueItr->value.GetUint());
                    break;
                case UpdateAdditionalSaveDataEvent::ValueType::Float:
                    if (!valueItr->value.IsNumber())
                    {
                        TC_LOG_ERROR("scripts.data.load", "Additional data value for key %s is not a number for instance %u [%u-%s | %u-%s]",
                            value->GetName(), GetInstanceId(), GetMapId(), GetMapName(), GetDifficultyId(), GetDifficultyName());
                        return Result::AdditionalDataUnexpectedValueType;
                    }
                    static_cast<PersistentInstanceScriptValue<float>*>(value)->LoadValue(valueItr->value.GetFloat());
                    break;
                default:
                    break;
            }
        }
    }

    return Result::Ok;
}

uint32 InstanceScriptDataReader::GetInstanceId() const { return _instance.instance->GetInstanceId(); }
uint32 InstanceScriptDataReader::GetMapId() const { return _instance.instance->GetId(); }
char const* InstanceScriptDataReader::GetMapName() const { return _instance.instance->GetMapName(); }
uint32 InstanceScriptDataReader::GetDifficultyId() const { return uint32(_instance.instance->GetDifficultyID()); }
char const* InstanceScriptDataReader::GetDifficultyName() const { return sDifficultyStore.AssertEntry(_instance.instance->GetDifficultyID())->Name[sWorld->GetDefaultDbcLocale()]; }

std::string InstanceScriptDataWriter::GetString()
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    _doc.Accept(writer);

    return std::string(buffer.GetString(), buffer.GetSize());
}

void InstanceScriptDataWriter::FillData(bool withValues)
{
    _doc.SetObject();
    _doc.AddMember(rapidjson::StringRef(HeadersKey), _instance.GetHeader(), _doc.GetAllocator());

    rapidjson::Value bossStates(rapidjson::kObjectType);
    for (uint32 bossId = 0; bossId < _instance.GetEncounterCount(); ++bossId)
    {
        rapidjson::Value bossObject(rapidjson::kObjectType);
        bossObject.AddMember(rapidjson::StringRef(BossStateStateKey),
            rapidjson::Value(rapidjson::StringRef(InstanceScript::GetBossStateName(withValues ? _instance.GetBossState(bossId) : NOT_STARTED))),
            _doc.GetAllocator());
        bossStates.AddMember(rapidjson::Value(Trinity::StringFormat("%u", bossId), _doc.GetAllocator()), bossObject.Move(), _doc.GetAllocator());
    }
    _doc.AddMember(rapidjson::StringRef(BossStatesSaveDataKey), bossStates.Move(), _doc.GetAllocator());

    if (!_instance.GetPersistentScriptValues().empty())
    {
        rapidjson::Value moreData(rapidjson::kObjectType);
        for (PersistentInstanceScriptValueBase* additionalValue : _instance.GetPersistentScriptValues())
        {
            if (withValues)
            {
                UpdateAdditionalSaveDataEvent data = additionalValue->CreateEvent();
                switch (data.Type)
                {
                    case UpdateAdditionalSaveDataEvent::ValueType::Int32:
                        moreData.AddMember(rapidjson::StringRef(data.Key), rapidjson::Value(data.Int32Value), _doc.GetAllocator());
                        break;
                    case UpdateAdditionalSaveDataEvent::ValueType::UInt32:
                        moreData.AddMember(rapidjson::StringRef(data.Key), rapidjson::Value(data.UInt32Value), _doc.GetAllocator());
                        break;
                    case UpdateAdditionalSaveDataEvent::ValueType::Float:
                        moreData.AddMember(rapidjson::StringRef(data.Key), rapidjson::Value(data.FloatValue), _doc.GetAllocator());
                        break;
                    default:
                        break;
                }
            }
            else
                moreData.AddMember(rapidjson::StringRef(additionalValue->GetName()), rapidjson::Value(), _doc.GetAllocator());
        }

        _doc.AddMember(rapidjson::StringRef(MoreSaveDataKey), moreData.Move(), _doc.GetAllocator());
    }
}

void InstanceScriptDataWriter::FillDataFrom(std::string const& data)
{
    if (_doc.Parse(data).HasParseError())
        FillData(false);
}

void InstanceScriptDataWriter::SetBossState(UpdateBossStateSaveDataEvent const& data)
{
    std::string bossIdKey = Trinity::StringFormat("%u", data.BossId);

    rapidjson::Pointer::Token tokens[] =
    {
        { BossStatesSaveDataKey.c_str(), uint32(BossStatesSaveDataKey.length()), rapidjson::kPointerInvalidIndex },
        { bossIdKey.c_str(), uint32(bossIdKey.length()), rapidjson::kPointerInvalidIndex },
        { BossStateStateKey.c_str(), uint32(BossStateStateKey.length()), rapidjson::kPointerInvalidIndex }
    };
    rapidjson::Pointer ptr(tokens, Trinity::Containers::Size(tokens));

    // jsonptr("/BossStates/BossId/State")
    rapidjson::SetValueByPointer(_doc, ptr, rapidjson::StringRef(InstanceScript::GetBossStateName(data.NewState)));
}

void InstanceScriptDataWriter::SetAdditionalData(UpdateAdditionalSaveDataEvent const& data)
{
    rapidjson::Pointer::Token tokens[] =
    {
        { MoreSaveDataKey.c_str(), uint32(MoreSaveDataKey.length()), rapidjson::kPointerInvalidIndex },
        { data.Key, uint32(strlen(data.Key)), rapidjson::kPointerInvalidIndex }
    };
    rapidjson::Pointer ptr(tokens, Trinity::Containers::Size(tokens));

    // jsonptr("/AdditionalData/CustomValueName")
    switch (data.Type)
    {
        case UpdateAdditionalSaveDataEvent::ValueType::Int32:
            rapidjson::SetValueByPointer(_doc, ptr, data.Int32Value);
            break;
        case UpdateAdditionalSaveDataEvent::ValueType::UInt32:
            rapidjson::SetValueByPointer(_doc, ptr, data.UInt32Value);
            break;
        case UpdateAdditionalSaveDataEvent::ValueType::Float:
            rapidjson::SetValueByPointer(_doc, ptr, data.FloatValue);
            break;
        default:
            break;
    }
}
