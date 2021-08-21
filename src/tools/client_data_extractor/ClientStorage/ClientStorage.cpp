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

#include "ClientStorage.h"
#include "CascHandles.h"
#include "Errors.h"
#include "Log.h"
#include <boost/filesystem/operations.hpp>
#include <boost/variant/get.hpp>

namespace Trinity
{
    namespace
    {
        template<typename T>
        ClientStorage::StaticInterface MakeStorageInfo()
        {
            return { &T::Name, &T::ConvertWowLocaleToStorageLocaleMask, &T::NoneLocaleMask, &T::AnyLocaleMask, &T::Open, &T::Supports };
        }

        std::vector<ClientStorage::StaticInterface> StorageTypes =
        {
            MakeStorageInfo<CASC::Storage>()
        };
    }

    ClientStorage::FileId::operator uint32 const*() const
    {
        return boost::get<uint32>(this);
    }

    ClientStorage::FileId::operator std::string const*() const
    {
        return boost::get<std::string>(this);
    }

    std::string ClientStorage::FileId::ToString() const
    {
        if (uint32 const* fileDataId = *this)
            return Trinity::StringFormat("FileDataId %u", *fileDataId);
        else if (std::string const* fileName = *this)
            return *fileName;

        return "";
    }

    ClientStorage::Storage::Storage(StaticInterface const* staticInterface) : _staticInterface(staticInterface)
    {
    }

    ClientStorage::StaticInterface const* GetClientStorageStaticInterface(boost::filesystem::path const& gameInstallPath)
    {
        auto itr = std::find_if(StorageTypes.begin(), StorageTypes.end(), [&](ClientStorage::StaticInterface const& storage)
        {
            return storage.Supports(gameInstallPath);
        });
        return itr != StorageTypes.end() ? &*itr : nullptr;
    }

    ClientStorage::Storage* OpenClientStorage(boost::filesystem::path const& gameInstallPath, std::string const& product, LocaleConstant locale)
    {
        ClientStorage::StaticInterface const* storageInterface = ASSERT_NOTNULL(GetClientStorageStaticInterface(gameInstallPath));
        try
        {
            ClientStorage::Storage* storage = storageInterface->Open(gameInstallPath, storageInterface->ConvertWowLocaleToStorageLocaleMask(locale), product.c_str());
            if (!storage)
            {
                TC_LOG_ERROR("extractor.storage", "Error opening %s storage '%s' locale %s", storageInterface->Name(), gameInstallPath.string().c_str(), localeNames[locale]);
                return nullptr;
            }

            return storage;
        }
        catch (boost::filesystem::filesystem_error const& error)
        {
            TC_LOG_ERROR("extractor.storage", "Error opening %s storage: %s", storageInterface->Name(), error.what());
            return nullptr;
        }
    }

    uint32 GetInstalledLocalesMask(boost::filesystem::path const& gameInstallPath, std::string const& product)
    {
        try
        {
            ClientStorage::StaticInterface const* storageInterface = ASSERT_NOTNULL(GetClientStorageStaticInterface(gameInstallPath));
            std::unique_ptr<ClientStorage::Storage> storage(storageInterface->Open(gameInstallPath, storageInterface->AnyLocaleMask(), product.c_str()));
            if (!storage)
                return false;

            return storage->GetInstalledLocalesMask();
        }
        catch (boost::filesystem::filesystem_error const& error)
        {
            TC_LOG_ERROR("extractor.storage", "Unable to determine installed locales mask: %s", error.what());
        }

        return 0;
    }
}
