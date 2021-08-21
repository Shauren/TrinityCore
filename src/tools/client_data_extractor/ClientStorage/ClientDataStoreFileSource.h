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

#ifndef DB2CascFileSource_h__
#define DB2CascFileSource_h__

#include "DB2FileLoader.h"
#include "ClientStorage.h"
#include <string>

namespace Trinity
{
struct ClientDataStoreFileSource : public DB2FileSource
{
    ClientDataStoreFileSource(std::shared_ptr<ClientStorage::Storage const> storage, ClientStorage::FileId const& fileId, bool printErrors = true);
    bool IsOpen() const override;
    bool Read(void* buffer, std::size_t numBytes) override;
    int64 GetPosition() const override;
    bool SetPosition(int64 position) override;
    int64 GetFileSize() const override;
    ClientStorage::File* GetNativeHandle() const;
    char const* GetFileName() const override;
    DB2EncryptedSectionHandling HandleEncryptedSection(DB2SectionHeader const& sectionHeader) const override;

private:
    std::weak_ptr<ClientStorage::Storage const> _storageHandle;
    std::unique_ptr<ClientStorage::File> _fileHandle;
    std::string _fileName;
};
}

#endif // DB2CascFile_h__
