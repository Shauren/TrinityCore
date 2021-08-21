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

#include "CameraFilesExtractor.h"
#include "ClientDataStores.h"
#include "Log.h"
#include <boost/filesystem/operations.hpp>
#include <unordered_set>

namespace Trinity
{
void CameraFilesExtractor::Run()
{
    TC_LOG_INFO("extractor.cameras", "Extracting camera files...");

    boost::filesystem::path outputPath = _outputPath / "cameras";

    CreateDir(outputPath);

    TC_LOG_INFO("extractor.cameras", "Cameras output path %s", outputPath.string().c_str());

    // extract M2s
    uint32 count = 0;
    for (std::pair<uint32 const, DataStores::CinematicCamera> const& camera : DataStores::CinematicCameras)
    {
        std::unique_ptr<ClientStorage::File> cameraFile(_storage->OpenFile(camera.second.FileDataID, _storage->NoneLocaleMask()));
        if (cameraFile)
        {
            boost::filesystem::path filePath = outputPath / Trinity::StringFormat("FILE%08X.xxx", camera.second.FileDataID);

            if (!boost::filesystem::exists(filePath))
                if (ExtractFile(cameraFile.get(), filePath.string()))
                    ++count;
        }
        else
            TC_LOG_ERROR("extractor.cameras", "Unable to open file %u in the archive: %s", camera.second.FileDataID, _storage->GetLastErrorText());
    }

    TC_LOG_INFO("extractor.cameras", "Extracted %u camera files", count);
}
}
