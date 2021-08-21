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

#ifndef CascHandles_h__
#define CascHandles_h__

#include "Common.h"
#include "ClientStorage.h"
#include <CascPort.h>

namespace boost
{
    namespace filesystem
    {
        class path;
    }
}

namespace Trinity
{
namespace CASC
{
    class Storage : public ClientStorage::Storage
    {
    public:
        static char const* Name();
        static uint32 ConvertWowLocaleToStorageLocaleMask(LocaleConstant locale);
        static uint32 NoneLocaleMask();
        static uint32 AnyLocaleMask();
        static bool Supports(boost::filesystem::path const& gameInstallPath);

        ~Storage();

        static ClientStorage::Storage* Open(boost::filesystem::path const& gameInstallPath, uint32 localeMask, char const* product);

        uint32 GetBuildNumber() const override;
        uint32 GetInstalledLocalesMask() const override;
        bool HasTactKey(uint64 keyLookup) const override;

        ClientStorage::File* OpenFile(ClientStorage::FileId const& id, uint32 localeMask, bool printErrors = false, bool zerofillEncryptedParts = false) const override;

        char const* GetLastErrorText() const override;

    private:
        Storage(ClientStorage::StaticInterface const* staticInterface, HANDLE handle);

        bool LoadOnlineTactKeys();

        HANDLE _handle;
    };

    class File : public ClientStorage::File
    {
        friend ClientStorage::File* Storage::OpenFile(ClientStorage::FileId const& id, uint32 localeMask, bool printErrors, bool zerofillEncryptedParts) const;

    public:
        ~File();

        uint32 GetId() const override;
        int64 GetSize() const override;
        int64 GetPointer() const override;
        bool SetPointer(int64 position) override;
        bool ReadFile(void* buffer, uint32 bytes, uint32* bytesRead) override;

    private:
        File(HANDLE handle);

        HANDLE _handle;
    };
}
}

#endif // CascHandles_h__
