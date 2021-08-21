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

#include "DataStoresExtractor.h"
#include "ClientDataStoreFileSource.h"
#include "DBFilesClientList.h"
#include "Log.h"
#include <boost/filesystem/operations.hpp>

namespace Trinity
{
void DataStoresExtractor::Run()
{
    TC_LOG_INFO("extractor.datastores", "Extracting db2 files...");

    boost::filesystem::path localePath = _outputPath / "dbc" / localeNames[_locale];

    CreateDir(_outputPath / "dbc");
    CreateDir(localePath);

    TC_LOG_INFO("extractor.datastores", "Locale %s output path %s", localeNames[_locale], localePath.string().c_str());

    uint32 count = 0;
    for (DB2FileInfo const& db2 : DBFilesClientList)
    {
        boost::filesystem::path filePath = localePath / db2.Name;

        if (!boost::filesystem::exists(filePath))
            if (ExtractFile(db2.FileDataId, filePath))
                ++count;

    }

    TC_LOG_INFO("extractor.datastores", "Extracted %u files", count);
}

bool DataStoresExtractor::ExtractFile(ClientStorage::FileId const& fileId, boost::filesystem::path const& outputPath)
{
    ClientDataStoreFileSource source(_storage, fileId, false);
    if (!source.IsOpen())
    {
        TC_LOG_ERROR("extractor.datastores", "Unable to open file %s in the archive for locale %s: %s",
            outputPath.filename().string().c_str(), localeNames[_locale], _storage->GetLastErrorText());
        return false;
    }

    int64 fileSize = source.GetFileSize();
    if (fileSize == -1)
    {
        TC_LOG_ERROR("extractor.datastores", "Can't read file size of '%s'", outputPath.filename().string().c_str());
        return false;
    }

    DB2FileLoader db2;
    try
    {
        db2.LoadHeaders(&source, nullptr);
    }
    catch (std::exception const& e)
    {
        TC_LOG_ERROR("extractor.datastores", "Can't read DB2 headers of '%s': %s", outputPath.filename().string().c_str(), e.what());
        return false;
    }

    std::string outputFileName = outputPath.string();
    FILE* output = fopen(outputFileName.c_str(), "wb");
    if (!output)
    {
        TC_LOG_ERROR("extractor.datastores", "Can't create the output file '%s'", outputFileName.c_str());
        return false;
    }

    DB2Header header = db2.GetHeader();

    int64 posAfterHeaders = 0;
    posAfterHeaders += fwrite(&header, 1, sizeof(header), output);

    // erase TactId from header if key is known
    for (uint32 i = 0; i < header.SectionCount; ++i)
    {
        DB2SectionHeader sectionHeader = db2.GetSectionHeader(i);
        if (sectionHeader.TactId && _storage->HasTactKey(sectionHeader.TactId))
            sectionHeader.TactId = 0;

        posAfterHeaders += fwrite(&sectionHeader, 1, sizeof(sectionHeader), output);
    }

    char buffer[0x10000];
    uint32 readBatchSize = 0x10000;
    uint32 readBytes;
    source.SetPosition(posAfterHeaders);

    do
    {
        readBytes = 0;
        if (!source.GetNativeHandle()->ReadFile(buffer, std::min<uint32>(fileSize, readBatchSize), &readBytes))
        {
            TC_LOG_ERROR("extractor.datastores", "Can't read file '%s'", outputFileName.c_str());
            fclose(output);
            boost::filesystem::remove(outputPath);
            return false;
        }

        if (!readBytes)
            break;

        fwrite(buffer, 1, readBytes, output);
        fileSize -= readBytes;
        readBatchSize = 0x10000;
        if (!fileSize) // now we have read entire file
            break;

    } while (true);

    fclose(output);
    return true;
}
}
