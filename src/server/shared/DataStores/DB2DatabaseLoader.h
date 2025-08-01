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

#ifndef DB2_DATABASE_LOADER_H
#define DB2_DATABASE_LOADER_H

#include "DB2FileLoader.h"
#include <string_view>
#include <vector>

enum HotfixDatabaseStatements : uint32;

struct TC_SHARED_API DB2LoadInfo : public DB2FileLoadInfo
{
    constexpr explicit DB2LoadInfo(DB2FieldMeta const* fields, std::size_t fieldCount, DB2Meta const* meta, HotfixDatabaseStatements statement)
        : DB2FileLoadInfo(fields, fieldCount, meta), Statement(statement) { }

    HotfixDatabaseStatements Statement;
};

class TC_SHARED_API DB2DatabaseLoader
{
public:
    DB2DatabaseLoader(std::string_view storageName, DB2LoadInfo const* loadInfo) : _storageName(storageName), _loadInfo(loadInfo) { }

    char* Load(bool custom, uint32& records, char**& indexTable, std::vector<char*>& stringPool, uint32& minId);
    void LoadStrings(bool custom, LocaleConstant locale, uint32 records, char** indexTable, std::vector<char*>& stringPool);
    static char* AddString(char const** holder, std::string_view value);

private:
    std::string_view _storageName;
    DB2LoadInfo const* _loadInfo;
};

#endif
