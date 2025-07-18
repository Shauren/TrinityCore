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

#include "SFMTRand.h"
#include <algorithm>
#include <array>
#include <functional>
#include <random>
#include <ctime>

SFMTRand::SFMTRand() noexcept
{
    std::random_device dev;
    if (dev.entropy() > 0)
    {
        std::array<uint32, SFMT_N32> seed;
        std::ranges::generate(seed, std::ref(dev));

        sfmt_init_by_array(&_state, seed.data(), seed.size());
    }
    else
        sfmt_init_gen_rand(&_state, uint32(time(nullptr)));
}

uint32 SFMTRand::RandomUInt32() noexcept                            // Output random bits
{
    return sfmt_genrand_uint32(&_state);
}
