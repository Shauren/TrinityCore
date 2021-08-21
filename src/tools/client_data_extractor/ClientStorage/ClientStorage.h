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

#ifndef ClientStorage_h__
#define ClientStorage_h__

#include "Common.h"
#include <boost/variant/variant.hpp>

namespace boost
{
    namespace filesystem
    {
        class path;
    }
}

namespace Trinity
{
    class ClientStorage
    {
    public:
        class Storage;

        class StaticInterface
        {
        public:
            char const* (*Name)();
            uint32(*ConvertWowLocaleToStorageLocaleMask)(LocaleConstant locale);
            uint32 (*NoneLocaleMask)();
            uint32 (*AnyLocaleMask)();
            Storage* (*Open)(boost::filesystem::path const& path, uint32 localeMask, char const* product);
            bool (*Supports)(boost::filesystem::path const& gameInstallPath);
        };

        class File;
        struct FileId;

        class Storage
        {
        public:
            Storage(StaticInterface const* staticInterface);
            virtual ~Storage() = default;

            virtual uint32 GetBuildNumber() const = 0;
            virtual uint32 GetInstalledLocalesMask() const = 0;
            virtual bool HasTactKey(uint64 keyLookup) const = 0;

            virtual File* OpenFile(FileId const& id, uint32 localeMask, bool printErrors = false, bool zerofillEncryptedParts = false) const = 0;

            virtual char const* GetLastErrorText() const = 0;
            uint32 NoneLocaleMask() const { return _staticInterface->NoneLocaleMask(); }
            uint32 AnyLocaleMask() const { return _staticInterface->AnyLocaleMask(); }

        private:
            StaticInterface const* _staticInterface;
        };

        struct FileId : boost::variant<uint32, std::string>
        {
            using Base = boost::variant<uint32, std::string>;

            using Base::Base;
            using Base::operator=;

            operator uint32 const*() const;
            operator std::string const*() const;

            std::string ToString() const;
        };

        class File
        {
        public:
            virtual ~File() = default;

            virtual uint32 GetId() const = 0;
            virtual int64 GetSize() const = 0;
            virtual int64 GetPointer() const = 0;
            virtual bool SetPointer(int64 position) = 0;
            virtual bool ReadFile(void* buffer, uint32 bytes, uint32* bytesRead) = 0;
        };
    };

    ClientStorage::StaticInterface const* GetClientStorageStaticInterface(boost::filesystem::path const& gameInstallPath);

    ClientStorage::Storage* OpenClientStorage(boost::filesystem::path const& gameInstallPath, std::string const& product, LocaleConstant locale);

    uint32 GetInstalledLocalesMask(boost::filesystem::path const& gameInstallPath, std::string const& product);
}

#endif // ClientStorage_h__
