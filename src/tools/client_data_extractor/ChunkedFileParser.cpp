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

#include "ChunkedFileParser.h"
#include "Definitions.h"

namespace Trinity
{
namespace
{
    template<typename T>
    std::enable_if_t<!Chunks::Traits::HasSubchunks<T>::value> ParseSubchunks(std::vector<Chunk<T>>& /*chunks*/, IteratorPair<std::vector<uint8>::iterator> /*dataRange*/, std::unordered_set<uint32> const& /*parsedChunks*/)
    {
    }

    template<typename T>
    std::enable_if_t<Chunks::Traits::HasSubchunks<T>::value> ParseSubchunks(std::vector<Chunk<T>>& chunks, IteratorPair<std::vector<uint8>::iterator> dataRange, std::unordered_set<uint32> const& parsedChunks)
    {
        chunks.back().Subchunks.ParseRange(dataRange, parsedChunks);
    }

    template<typename T>
    std::enable_if_t<!Chunks::Traits::HasMultipleCopiesUnderOneHeader<T>::value, std::vector<uint8>::iterator>
        ParseMultiple(std::vector<Chunk<T>>& chunks, Chunks::Header const* header, IteratorPair<std::vector<uint8>::iterator> dataRange)
    {
        T const* asT = reinterpret_cast<T const*>(&*dataRange.begin());
        chunks.emplace_back(header, asT);
        return dataRange.begin() + sizeof(T);
    }

    template<typename T>
    std::enable_if_t<Chunks::Traits::HasMultipleCopiesUnderOneHeader<T>::value, std::vector<uint8>::iterator>
        ParseMultiple(std::vector<Chunk<T>>& chunks, Chunks::Header const* header, IteratorPair<std::vector<uint8>::iterator> dataRange)
    {
        auto itr = dataRange.begin();
        do
        {
            T const* asT = reinterpret_cast<T const*>(&*itr);
            chunks.emplace_back(header, asT);
            itr += sizeof(T);

        } while (itr < dataRange.end());

        return itr;
    }

    template<typename T>
    void HandleChunk(IteratorPair<std::vector<uint8>::iterator> dataRange, std::unordered_set<uint32> const& parsedChunks, boost::any& container)
    {
        if (container.empty())
            container = std::vector<Chunk<T>>();

        std::vector<Chunk<T>>& chunks = boost::any_cast<std::vector<Chunk<T>>&>(container);
        Chunks::Header const* header = reinterpret_cast<Chunks::Header const*>(&*dataRange.begin());

        auto posAfterSelf = ParseMultiple(chunks, header, Containers::MakeIteratorPair(dataRange.begin() + sizeof(Chunks::Header), dataRange.end()));

        // Start parsing for subchunks after self-header
        ParseSubchunks(chunks, Containers::MakeIteratorPair(posAfterSelf, dataRange.end()), parsedChunks);
    }

    using ChunkHandler = void(*)(IteratorPair<std::vector<uint8>::iterator> dataRange, std::unordered_set<uint32> const& parsedChunks, boost::any& container);
    using ChunkHandlers = std::unordered_map<uint32, std::pair<ChunkHandler, std::type_index>>;

    uint32 ConvertChunkMagicToInt(char const (&name)[5])
    {
        return (uint32((name[0]) << 24)) | (uint32((name[1]) << 16)) | (uint32((name[2]) << 8)) | uint32((name[3]));
    }

    uint32 ConvertChunkMagicToInt(std::string const& name)
    {
        char chars[5];
        strncpy(chars, name.c_str(), 4);
        chars[4] = '\0';
        return ConvertChunkMagicToInt(chars);
    }

    template<typename T>
    void RegisterChunk(char const (&name)[5], ChunkHandlers& handlers)
    {
        handlers.emplace(ConvertChunkMagicToInt(name), std::make_pair(&HandleChunk<T>, std::type_index(typeid(T))));
    }

    ChunkHandlers RegisterChunks()
    {
        using namespace Chunks;

        ChunkHandlers result;

        // WDT
        RegisterChunk<MPHD>("MPHD", result);
        RegisterChunk<MAIN>("MAIN", result);
        RegisterChunk<MAID>("MAID", result);

        // WDT & ADT
        RegisterChunk<MWMO>("MWMO", result);
        RegisterChunk<MODF>("MODF", result);

        // ADT
        RegisterChunk<MCNK>("MCNK", result);
        RegisterChunk<MCVT>("MCVT", result);
        RegisterChunk<MH2O>("MH2O", result);
        RegisterChunk<MFBO>("MFBO", result);
        RegisterChunk<MMDX>("MMDX", result);
        RegisterChunk<MDDF>("MDDF", result);

        return result;
    }

    ChunkHandlers Handlers = RegisterChunks();
}

void ChunkParser::ParseRange(IteratorPair<std::vector<uint8>::iterator> sourceRange,
    std::unordered_set<uint32> const& parsedChunks)
{
    // Make sure there's enough data to read u_map_fcc struct and the uint32 size after it
    auto itr = sourceRange.begin();
    auto end = sourceRange.end() - sizeof(Chunks::Header);
    while (itr <= end)
    {
        Chunks::Header const* header = reinterpret_cast<Chunks::Header const*>(&*itr);
        auto chunkEnd = itr + sizeof(Chunks::Header) + header->Size;

        if (parsedChunks.count(header->Type) && header->Size)
            if (ChunkHandlers::mapped_type const* handler = Containers::MapGetValuePtr(Handlers, header->Type))
                handler->first(Containers::MakeIteratorPair(itr, chunkEnd), parsedChunks, _chunks[handler->second]);

        // move to next chunk
        itr = chunkEnd;
    }
}

bool ChunkedFileParser::ParseFile(std::shared_ptr<ClientStorage::Storage> storage, ClientStorage::FileId const& id,
    std::unordered_set<std::string> const& parsedChunks, bool log /*= true*/)
{
    std::unique_ptr<ClientStorage::File> file(storage->OpenFile(id, storage->AnyLocaleMask(), log));
    if (!file)
        return false;

    int64 fileSize = file->GetSize();
    if (fileSize == -1)
        return false;

    std::vector<uint8> fileData(fileSize);
    uint32 bytesRead = 0;
    if (!file->ReadFile(fileData.data(), fileSize, &bytesRead) || bytesRead != fileSize)
        return false;

    std::unordered_set<uint32> parsedChunkTypes;
    std::transform(parsedChunks.begin(), parsedChunks.end(), std::inserter(parsedChunkTypes, parsedChunkTypes.end()), [](std::string const& chunkName)
    {
        return ConvertChunkMagicToInt(chunkName);
    });
    _fileData.push_front(std::move(fileData));
    ParseRange(Containers::MakeIteratorPair(_fileData.front().begin(), _fileData.front().end()), parsedChunkTypes);
    return true;
}
}
