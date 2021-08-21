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

#include "GameTablesExtractor.h"
#include "DBFilesClientList.h"
#include "Log.h"
#include <boost/filesystem/operations.hpp>

namespace Trinity
{
void GameTablesExtractor::Run()
{
    TC_LOG_INFO("extractor.gametables", "Extracting game tables...");

    boost::filesystem::path outputPath = _outputPath / "gt";

    CreateDir(outputPath);

    TC_LOG_INFO("extractor.gametables", "output path %s", outputPath.string().c_str());

    DB2FileInfo GameTables[] =
    {
        { 1582086, "ArtifactKnowledgeMultiplier.txt" },
        { 1391662, "ArtifactLevelXP.txt" },
        { 1391663, "BarberShopCostBase.txt" },
        { 1391664, "BaseMp.txt" },
        { 1391665, "BattlePetTypeDamageMod.txt" },
        { 1391666, "BattlePetXP.txt" },
        { 1391667, "ChallengeModeDamage.txt" },
        { 1391668, "ChallengeModeHealth.txt" },
        { 1391669, "CombatRatings.txt" },
        { 1391670, "CombatRatingsMultByILvl.txt" },
        { 1391671, "HonorLevel.txt" },
        { 1391642, "HpPerSta.txt" },
        { 2012881, "ItemLevelByLevel.txt" },
        { 1726830, "ItemLevelSquish.txt" },
        { 1391643, "ItemSocketCostPerLevel.txt" },
        { 1391651, "NPCManaCostScaler.txt" },
        { 1391659, "SandboxScaling.txt" },
        { 1391660, "SpellScaling.txt" },
        { 1980632, "StaminaMultByILvl.txt" },
        { 1391661, "xp.txt" }
    };

    uint32 count = 0;
    for (DB2FileInfo const& gt : GameTables)
    {
        std::unique_ptr<ClientStorage::File> file(_storage->OpenFile(gt.FileDataId, _storage->NoneLocaleMask()));
        if (file)
        {
            boost::filesystem::path filePath = outputPath / gt.Name;

            if (!boost::filesystem::exists(filePath))
                if (ExtractFile(file.get(), filePath.string()))
                    ++count;
        }
        else
            TC_LOG_ERROR("extractor.gametables", "Unable to open file %s in the archive: %s", gt.Name, _storage->GetLastErrorText());
    }

    TC_LOG_INFO("extractor.gametables", "Extracted %u files\n", count);
}
}
