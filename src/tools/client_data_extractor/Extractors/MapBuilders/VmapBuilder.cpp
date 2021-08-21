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

#include "VmapBuilder.h"

namespace Trinity
{
MapDataExtractor::BaseBuilder::Task VmapBuilder::CreateWDTTask(MapDataExtractor::Context& /*context*/, MapDataExtractor::WDTData* /*wdtData*/)
{
    if (_includeAllVmapVertices)
        ;
    return nullptr;
}

MapDataExtractor::BaseBuilder::Task VmapBuilder::CreateADTTask(MapDataExtractor::Context& /*context*/, MapDataExtractor::WDTData* /*wdtData*/, int32 /*x*/, int32 /*y*/)
{
    return nullptr;
}

MapDataExtractor::BaseBuilder::Task VmapBuilder::CreateWDTFinalizeTask(MapDataExtractor::Context& /*context*/, MapDataExtractor::WDTData* /*wdtData*/)
{
    return nullptr;
}
}
