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

#ifndef ExtractorOptions_h__
#define ExtractorOptions_h__

#include "Common.h"
#include "Optional.h"
#include <boost/filesystem/path.hpp>
#include <memory>

namespace boost
{
    namespace program_options
    {
        class options_description;
    }
}

namespace Trinity
{
    struct ExtractorOptions
    {
    public:
        ExtractorOptions();
        ~ExtractorOptions();

        bool Parse(int argc, char const* argv[]);
        void PrintHelp();
        std::string ToString() const;

        // General options
        boost::filesystem::path InputPath;
        boost::filesystem::path OutputPath;
        std::string Product;
        Optional<LocaleConstant> Locale;
        bool ExtractMaps = false;
        bool ExtractDB2 = false;
        bool ExtractCameras = false;
        bool ExtractGameTables = false;
        bool ExtractVmaps = false;

        // Map options
        struct
        {
            bool AllowHeightCompression = false;
        } Map;

        // VMap options
        struct
        {
            bool IncludeAllVertices = false;
        } Vmap;

        // Help options
        bool ShowHelp = false;
        bool ShowVersion = false;

    private:
        std::unique_ptr<boost::program_options::options_description> _genericOptions;
        std::unique_ptr<boost::program_options::options_description> _mapOptions;
        std::unique_ptr<boost::program_options::options_description> _vmapOptions;
    };
}

#endif // ExtractorOptions_h__
