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

#include "Banner.h"
#include "CameraFilesExtractor.h"
#include "ClientDataStores.h"
#include "ClientStorage.h"
#include "DataStoresExtractor.h"
#include "Definitions.h"
#include "ExtractorOptions.h"
#include "GameTablesExtractor.h"
#include "GitRevision.h"
#include "Log.h"
#include "MapDataExtractor.h"
#include "Optional.h"

// for isatty
#if TRINITY_PLATFORM == TRINITY_PLATFORM_WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif

void SetupLogging()
{
    sLog->CreateAppenderFromConfigLine("Appender.Console", "1,3,0");    // APPENDER_CONSOLE | LOG_LEVEL_INFO | APPENDER_FLAGS_NONE

    sLog->CreateLoggerFromConfigLine("Logger.root", "3,Console");       // LOG_LEVEL_INFO | Console appender
    sLog->CreateLoggerFromConfigLine("Logger.extractor", "3,Console");  // LOG_LEVEL_INFO | Console appender

    // Disable .progress loggers when not running in console (output redirected to file)
    if (!isatty(fileno(stdout)))
    {
        sLog->CreateLoggerFromConfigLine("Logger.extractor.cameras.progress", "0,Console");     // LOG_LEVEL_DISABLED | Console appender
        sLog->CreateLoggerFromConfigLine("Logger.extractor.datastores.progress", "0,Console");  // LOG_LEVEL_DISABLED | Console appender
        sLog->CreateLoggerFromConfigLine("Logger.extractor.gametables.progress", "0,Console");  // LOG_LEVEL_DISABLED | Console appender
        sLog->CreateLoggerFromConfigLine("Logger.extractor.maps.progress", "0,Console");        // LOG_LEVEL_DISABLED | Console appender
    }

    // Used loggers
    // * extractor
    // * extractor.cameras
    // * extractor.datastores
    // * extractor.gametables
    // * extractor.maps
    // * extractor.maps.progress
    // * extractor.options
    // * extractor.storage
}

int main(int argc, char const* argv[])
{
    using namespace Trinity;

    SetupLogging();

    ExtractorOptions options;

    if (!options.Parse(argc, argv))
    {
        options.PrintHelp();
        return 1;
    }

    if (options.ShowVersion)
    {
        TC_LOG_INFO("extractor", "%s (Client data extractor)", GitRevision::GetFullVersion());
        return 0;
    }

    Banner::Show("Client data extractor", [](char const* text) { TC_LOG_INFO("extractor", "%s", text); }, nullptr);

    if (options.ShowHelp)
    {
        options.PrintHelp();
        return 0;
    }

    TC_LOG_INFO("extractor", "Extractor configuration:");
    TC_LOG_INFO("extractor", "%s", options.ToString().c_str());

    uint32 installedLocalesMask = GetInstalledLocalesMask(options.InputPath, options.Product);
    Optional<LocaleConstant> firstInstalledLocale;
    uint32 build = 0;

    ClientStorage::StaticInterface const* storageInterface = GetClientStorageStaticInterface(options.InputPath);
    if (!storageInterface)
    {
        TC_LOG_ERROR("extractor", "Current client installation uses unsupported storage type!");
        return 1;
    }

    for (LocaleConstant i = LOCALE_enUS; i < TOTAL_LOCALES; i = LocaleConstant(i + 1))
    {
        if (options.Locale && !(*options.Locale & (1 << i)))
            continue;

        if (i == LOCALE_none)
            continue;

        if (!(installedLocalesMask & storageInterface->ConvertWowLocaleToStorageLocaleMask(i)))
            continue;

        std::shared_ptr<ClientStorage::Storage> storage(OpenClientStorage(options.InputPath, options.Product, i));
        if (!storage)
            continue;

        if (!options.ExtractDB2)
        {
            firstInstalledLocale = i;
            build = storage->GetBuildNumber();
            if (!build)
                continue;

            TC_LOG_INFO("extractor", "Detected client build: %u\n", build);
            break;
        }

        //Extract DBC files
        uint32 tempBuild = storage->GetBuildNumber();
        if (!tempBuild)
            continue;

        TC_LOG_INFO("extractor", "Detected client build %u for locale %s\n", tempBuild, localeNames[i]);
        DataStoresExtractor(storage, options.OutputPath, i).Run();

        if (!firstInstalledLocale)
        {
            firstInstalledLocale = i;
            build = tempBuild;
        }
    }

    if (!firstInstalledLocale)
    {
        TC_LOG_ERROR("extractor", "No locales detected");
        return 1;
    }

    if (options.ExtractMaps || options.ExtractCameras || options.ExtractGameTables || options.ExtractVmaps)
    {
        std::shared_ptr<ClientStorage::Storage> storage(OpenClientStorage(options.InputPath, options.Product, *firstInstalledLocale));

        Trinity::DataStores::Load(storage);

        if (options.ExtractCameras)
            CameraFilesExtractor(storage, options.OutputPath).Run();

        if (options.ExtractGameTables)
            GameTablesExtractor(storage, options.OutputPath).Run();

        if (options.ExtractMaps || options.ExtractVmaps)
        {
            // Start ADT/WDT parser
            MapDataExtractor mapDataExtractor(storage, options.OutputPath, build,
                options.ExtractMaps, options.Map.AllowHeightCompression,
                options.ExtractVmaps, options.Vmap.IncludeAllVertices);

            mapDataExtractor.Run();
        }
    }

    return 0;
}
