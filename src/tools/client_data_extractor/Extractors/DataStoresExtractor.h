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

#ifndef DataStoresExtractor_h__
#define DataStoresExtractor_h__

#include "BaseExtractor.h"

namespace Trinity
{
    class DataStoresExtractor : public BaseExtractor
    {
    public:
        DataStoresExtractor(std::shared_ptr<ClientStorage::Storage> storage, boost::filesystem::path outputPath, LocaleConstant locale)
            : BaseExtractor(std::move(storage), std::move(outputPath)), _locale(locale) { }

        void Run();

    private:
        bool ExtractFile(ClientStorage::FileId const& fileId, boost::filesystem::path const& outputPath);

        LocaleConstant _locale;
    };
}

#endif // DataStoresExtractor_h__
