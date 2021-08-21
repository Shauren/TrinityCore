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

#ifndef ChunkedFileParser_h__
#define ChunkedFileParser_h__

#include "ClientStorage.h"
#include "Containers.h"
#include "IteratorPair.h"
#include <boost/any.hpp>
#include <forward_list>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Trinity
{
    template<typename T>
    struct Chunk;

    namespace Chunks
    {
        struct Header
        {
            uint32 Type;
            uint32 Size;
        };

        namespace Traits
        {
            template<typename T>
            struct HasSubchunks;
        }
    }

    class ChunkParser
    {
    public:
        void ParseRange(IteratorPair<std::vector<uint8>::iterator> sourceRange, std::unordered_set<uint32> const& parsedChunks);

        template<typename T>
        std::vector<Chunk<T>> const* GetChunks() const
        {
            if (boost::any const* container = Containers::MapGetValuePtr(_chunks, typeid(T)))
                return &boost::any_cast<std::vector<Chunk<T>> const&>(*container);

            return nullptr;
        }

        template<typename T>
        Chunk<T> const* GetSingleChunk() const
        {
            if (std::vector<Chunk<T>> const* chunks = GetChunks<T>())
                if (chunks->size() == 1)
                    return &(*chunks)[0];

            return nullptr;
        }

    protected:
        std::unordered_map<std::type_index, boost::any /*std::vector<Chunk<T>>*/> _chunks;
    };

    struct SubchunkHolder
    {
        ChunkParser Subchunks;
    };

    template<typename T>
    struct Chunk : std::conditional_t<Chunks::Traits::HasSubchunks<T>::value, SubchunkHolder, std::tuple<>>
    {
        Chunk(Chunks::Header const* header, T const* data) : Header(header), Data(data) { }

        Chunks::Header const* Header;
        T const* Data;
    };

    class ChunkedFileParser : public ChunkParser
    {
    public:
        bool ParseFile(std::shared_ptr<ClientStorage::Storage> storage, ClientStorage::FileId const& id, std::unordered_set<std::string> const& parsedChunks, bool log = true);

    private:
        std::forward_list<std::vector<uint8>> _fileData;
    };

}

#endif // ChunkedFileParser_h__
