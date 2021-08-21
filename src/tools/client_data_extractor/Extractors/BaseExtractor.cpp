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

#include "BaseExtractor.h"
#include "Log.h"
#include <boost/filesystem/operations.hpp>

namespace Trinity
{
void BaseExtractor::CreateDir(boost::filesystem::path const& path)
{
    if (!boost::filesystem::exists(path))
        if (!boost::filesystem::create_directory(path))
            throw std::runtime_error("Unable to create directory" + path.string());
}

bool BaseExtractor::ExtractFile(ClientStorage::File* fileInArchive, std::string const& filename)
{
    int64 fileSize = fileInArchive->GetSize();
    if (fileSize == -1)
    {
        TC_LOG_ERROR("extractor", "Can't read file size of '%s'", filename.c_str());
        return false;
    }

    FILE* output = fopen(filename.c_str(), "wb");
    if (!output)
    {
        TC_LOG_ERROR("extractor", "Can't create the output file '%s'", filename.c_str());
        return false;
    }

    char  buffer[0x10000];
    uint32 readBytes;

    do
    {
        readBytes = 0;
        if (!fileInArchive->ReadFile(buffer, std::min<uint32>(fileSize, sizeof(buffer)), &readBytes))
        {
            TC_LOG_ERROR("extractor", "Can't read file '%s'", filename.c_str());
            fclose(output);
            boost::filesystem::remove(filename);
            return false;
        }

        if (!readBytes)
            break;

        fwrite(buffer, 1, readBytes, output);
        fileSize -= readBytes;
        if (!fileSize) // now we have read entire file
            break;

    } while (true);

    fclose(output);
    return true;
}
}
