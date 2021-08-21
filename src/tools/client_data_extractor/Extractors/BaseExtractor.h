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

#ifndef BaseExtractor_h__
#define BaseExtractor_h__

#include "ClientStorage.h"
#include <boost/filesystem/path.hpp>

namespace Trinity
{
class BaseExtractor
{
public:
    BaseExtractor(std::shared_ptr<ClientStorage::Storage> storage, boost::filesystem::path outputPath) : _storage(storage), _outputPath(std::move(outputPath)) { }
    virtual ~BaseExtractor() = default;

    void CreateDir(boost::filesystem::path const& path);
    bool ExtractFile(ClientStorage::File* fileInArchive, std::string const& filename);

protected:
    std::shared_ptr<ClientStorage::Storage> _storage;
    boost::filesystem::path _outputPath;
};
}
#endif // BaseExtractor_h__
