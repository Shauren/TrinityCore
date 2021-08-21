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

#include "ExtractorOptions.h"
#include "Log.h"
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>
#include <fmt/ostream.h>

#include "Hacks/boost_program_options_with_filesystem_path.h"

namespace
{
    enum LegacyExtractOption
    {
        EXTRACT_MAP     = 0x1,
        EXTRACT_DBC     = 0x2,
        EXTRACT_CAMERA  = 0x4,
        EXTRACT_GT      = 0x8,
        EXTRACT_VMAP    = 0x10,
    };

    std::pair<std::string, std::string> ExtraOptionParser(std::string const& option)
    {
        // Hidden alias -? for -h
        if (option == "-?")
            return { "-h", "" };

        return {};
    }
}

// in global namespace for ADL
static void validate(boost::any& v, std::vector<std::string> const& values, LocaleConstant* /*target_type_tag*/, int)
{
    using namespace boost::program_options;

    validators::check_first_occurrence(v);
    std::string const& s = validators::get_single_string(values);
    LocaleConstant locale = GetLocaleByName(s);
    if (locale == TOTAL_LOCALES)
        throw validation_error(invalid_option_value(s));

    v = locale;
}

namespace Trinity
{
    ExtractorOptions::ExtractorOptions() = default;

    ExtractorOptions::~ExtractorOptions() = default;

    bool ExtractorOptions::Parse(int argc, char const* argv[])
    {
        using namespace boost::program_options;

        try
        {
            options_description allOptions;
            _genericOptions = std::make_unique<options_description>("Allowed options");
            _mapOptions = std::make_unique<options_description>("Map extractor specific options");
            _vmapOptions = std::make_unique<options_description>("VMap extractor specific options");
            options_description hiddenOptions;

            _genericOptions->add_options()
                ("input,i", value<>(&InputPath)->value_name("directory")->default_value(boost::filesystem::current_path()), "Input path (game install location)")
                ("output,o", value<>(&OutputPath)->value_name("directory")->default_value(boost::filesystem::current_path()), "Output path (server files location)")
                ("product,p", value<>(&Product)->value_name("product")->default_value("wow"), "Selects which installed product to open if multiple are available (wow/wowt/wow_beta)")
                ("locale,l", value<>(&Locale)->value_name("locale"), "DB2 locale to extract (will extract all installed if not set)")
                ("maps", value<>(&ExtractMaps)->implicit_value(true, "")->default_value(true, "true")->value_name("true/false"), "Extract maps")
                ("db2", value<>(&ExtractDB2)->implicit_value(true, "")->default_value(true, "true")->value_name("true/false"), "Extract db2 files")
                ("cameras", value<>(&ExtractCameras)->implicit_value(true, "")->default_value(true, "true")->value_name("true/false"), "Extract cameras")
                ("gt", value<>(&ExtractGameTables)->implicit_value(true, "")->default_value(true, "true")->value_name("true/false"), "Extract game tables (gt)")
                ("vmaps", value<>(&ExtractVmaps)->implicit_value(true, "")->default_value(true, "true")->value_name("true/false"), "Extract vmaps")
                ("help,h", bool_switch(&ShowHelp), "Print usage message")
                ("version,v", bool_switch(&ShowVersion), "Print version build info");

            _mapOptions->add_options()
                ("allow-height-compression,f", value<>(&Map.AllowHeightCompression)->default_value(true, "true")->value_name("true/false"), "Turns height data compression on/off");

            hiddenOptions.add_options()
                ("extract,e", value<int32>()->value_name("directory"), "Legacy extract mask")
                ("vmap-include-all-vertices", bool_switch(&Vmap.IncludeAllVertices), "Include all vertices in vmap output, even those that shouldn't cause collision");

            allOptions
                .add(*_genericOptions)
                .add(*_mapOptions)
                .add(*_vmapOptions)
                .add(hiddenOptions);

            variables_map variablesMap;
            store(parse_command_line(argc, argv, allOptions, command_line_style::unix_style & ~command_line_style::allow_guessing, ExtraOptionParser), variablesMap);

            if (variablesMap.count("extract"))
            {
                // Convert legacy extract mask
                std::vector<std::string> legacyConverted;
                int32 extractMask = variablesMap["extract"].as<int32>();
                legacyConverted.push_back(Trinity::StringFormat("--maps=%s", (extractMask & EXTRACT_MAP) != 0 ? "true" : "false"));
                legacyConverted.push_back(Trinity::StringFormat("--db2=%s", (extractMask & EXTRACT_DBC) != 0 ? "true" : "false"));
                legacyConverted.push_back(Trinity::StringFormat("--cameras=%s", (extractMask & EXTRACT_CAMERA) != 0 ? "true" : "false"));
                legacyConverted.push_back(Trinity::StringFormat("--gt=%s", (extractMask & EXTRACT_GT) != 0 ? "true" : "false"));
                legacyConverted.push_back(Trinity::StringFormat("--vmaps=%s", (extractMask & EXTRACT_VMAP) != 0 ? "true" : "false"));

                store(command_line_parser(legacyConverted).options(*_genericOptions).extra_parser(ExtraOptionParser).run(), variablesMap);
            }

            notify(variablesMap);
            return true;
        }
        catch (std::exception const& e)
        {
            TC_LOG_ERROR("extractor.options", "%s", e.what());
            return false;
        }
    }

    void ExtractorOptions::PrintHelp()
    {
        if (!_genericOptions->options().empty())
            TC_LOG_INFO("extractor.options", "%s", fmt::format("{}", *_genericOptions).c_str());

        if (!_mapOptions->options().empty())
            TC_LOG_INFO("extractor.options", "%s", fmt::format("{}", *_mapOptions).c_str());

        if (!_vmapOptions->options().empty())
            TC_LOG_INFO("extractor.options", "%s", fmt::format("{}", *_vmapOptions).c_str());
    }

    std::string ExtractorOptions::ToString() const
    {
        return Trinity::StringFormat(R"(InputPath = %s
OutputPath = %s
Product = %s
Locale = %s
ExtractMaps = %s
ExtractDB2 = %s
ExtractCameras = %s
ExtractGameTables = %s
ExtractVmaps = %s
Map.AllowHeightCompression = %s
ShowHelp = %s
ShowVersion = %s
)",
            InputPath.string().c_str(), OutputPath.string().c_str(), Product.c_str(), Locale.map([](LocaleConstant l){ return localeNames[l]; }).value_or("all"),
            ExtractMaps ? "true" : "false", ExtractDB2 ? "true" : "false", ExtractCameras ? "true" : "false", ExtractGameTables ? "true" : "false",
            ExtractVmaps ? "true" : "false", Map.AllowHeightCompression ? "true" : "false", ShowHelp ? "true" : "false", ShowVersion ? "true" : "false");
    }
}
