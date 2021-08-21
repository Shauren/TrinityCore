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

#ifndef Definitions_h__
#define Definitions_h__

#include "Define.h"
#include "EnumFlag.h"
#include <G3D/AABox.h>
#include <G3D/Sphere.h>
#include <G3D/Vector3.h>
#include <string>

namespace Trinity
{
namespace WDT
{
    int32 constexpr ADTS_PER_WDT = 64;
}

namespace ADT
{
    int32 constexpr CELLS_PER_GRID = 16;
    int32 constexpr CELL_SIZE = 8;
    int32 constexpr GRID_SIZE = (CELLS_PER_GRID * CELL_SIZE);
}

#pragma pack(push,1)

namespace Chunks
{
    // WDT
    enum class MPHD_Flag : uint32
    {
        HasMAID = 0x200
    };

    DEFINE_ENUM_FLAG(MPHD_Flag);

    struct MPHD
    {
        EnumFlag<MPHD_Flag> Flags;
        uint32 LgtFileDataID;
        uint32 OccFileDataID;
        uint32 FogsFileDataID;
        uint32 MpvFileDataID;
        uint32 TexFileDataID;
        uint32 WdlFileDataID;
        uint32 Pd4FileDataID;
    };

    enum class MAIN_Flag : uint32
    {
        HasADT      = 0x1,
        AllWater    = 0x2
    };

    DEFINE_ENUM_FLAG(MAIN_Flag);

    struct MAIN
    {
        struct SMAreaInfo
        {
            EnumFlag<MAIN_Flag> Flag;
            uint32 AsyncId;
        } Data[WDT::ADTS_PER_WDT][WDT::ADTS_PER_WDT];
    };

    struct MAID
    {
        struct SMAreaFileIDs
        {
            uint32 RootADT;         // FileDataID of mapname_xx_yy.adt
            uint32 Obj0ADT;         // FileDataID of mapname_xx_yy_obj0.adt
            uint32 Obj1ADT;         // FileDataID of mapname_xx_yy_obj1.adt
            uint32 Tex0ADT;         // FileDataID of mapname_xx_yy_tex0.adt
            uint32 LodADT;          // FileDataID of mapname_xx_yy_lod.adt
            uint32 MapTexture;      // FileDataID of mapname_xx_yy.blp
            uint32 MapTextureN;     // FileDataID of mapname_xx_yy_n.blp
            uint32 MinimapTexture;  // FileDataID of mapxx_yy.blp
        } Data[WDT::ADTS_PER_WDT][WDT::ADTS_PER_WDT];
    };

    // WDT & ADT
    struct MWMO
    {
        // Actual size is Header::Size
        char FileNames[1];
    };

    struct MODF
    {
        uint32 Id;
        uint32 UniqueId;
        G3D::Vector3 Position;
        G3D::Vector3 Rotation;
        G3D::AABox Bounds;
        uint16 Flags;
        uint16 DoodadSet;   // can be larger than number of doodad sets in WMO
        uint16 NameSet;
        uint16 Scale;
    };

    // ADT
    enum class MCNK_Flag : uint32
    {
        HasMCSH             = 0x00001,
        Impass              = 0x00002,
        RiverMCLQ           = 0x00004,
        OceanMCLQ           = 0x00008,
        MagmaMCLQ           = 0x00010,
        SlimeMCLQ           = 0x00020,
        HasMCCV             = 0x00040,
        DoNotFixAlphaMap    = 0x08000,
        HasHighResHoles     = 0x10000,
    };

    DEFINE_ENUM_FLAG(MCNK_Flag);

    struct MCNK
    {
        EnumFlag<MCNK_Flag> Flags;
        uint32 IndexX;
        uint32 IndexY;
        uint32 nLayers;
        uint32 nDoodadRefs;
        union
        {
            struct
            {
                uint32 offsMCVT;        // height map
                uint32 offsMCNR;        // Normal vectors for each vertex
            } offsets;
            uint8 HighResHoles[8];
        } union_5_3_0;
        uint32 offsMCLY;        // Texture layer definitions
        uint32 offsMCRF;        // A list of indices into the parent file's MDDF chunk
        uint32 offsMCAL;        // Alpha maps for additional texture layers
        uint32 sizeMCAL;
        uint32 offsMCSH;        // Shadow map for static shadows on the terrain
        uint32 sizeMCSH;
        uint32 areaid;
        uint32 nMapObjRefs;
        uint16 LowResHoles;
        uint16 pad;
        uint64 predTex[2];
        uint64 noEffectDoodad;  // doodads disabled if 1 (array of 64 bits)
        uint32 offsMCSE;
        uint32 nSndEmitters;
        uint32 offsMCLQ;        // Liqid level (old)
        uint32 sizeMCLQ;        //
        G3D::Vector3 position;
        uint32 offsMCCV;        // offsColorValues in WotLK
        uint32 offsMCLV;
        uint32 unused;
    };

    struct MCVT
    {
        float Height[(ADT::CELL_SIZE + 1) * (ADT::CELL_SIZE + 1) + ADT::CELL_SIZE * ADT::CELL_SIZE];
    };

    struct MCLQ
    {
        float height1;
        float height2;
        struct liquid_data
        {
            uint32 light;
            float  height;
        } liquid[ADT::CELL_SIZE + 1][ADT::CELL_SIZE + 1];

        // 1<<0 - ocean
        // 1<<1 - lava/slime
        // 1<<2 - water
        // 1<<6 - all water
        // 1<<7 - dark water
        // == 0x0F - not show liquid
        uint8 flags[ADT::CELL_SIZE][ADT::CELL_SIZE];
        uint32_t nFlowvs;
        struct SWFlowv
        {
            G3D::Sphere sphere;
            G3D::Vector3 dir;
            float velocity;
            float amplitude;
            float frequency;
        } flowvs[2]; // always 2 in file, independent on nFlowvs.
    };

    struct MH2O
    {
        enum class LiquidVertexFormatType
        {
            HeightDepth             = 0,
            HeightTextureCoord      = 1,
            Depth                   = 2,
            HeightDepthTextureCoord = 3,
            Unk4                    = 4,
            Unk5                    = 5
        };

        struct Instance
        {
            uint16 LiquidType;              // Index from LiquidType.db2
            uint16 LiquidVertexFormat;      // Id from LiquidObject.db2 if >= 42
            float MinHeightLevel;
            float MaxHeightLevel;
            uint8 OffsetX;
            uint8 OffsetY;
            uint8 Width;
            uint8 Height;
            uint32 OffsetExistsBitmap;
            uint32 OffsetVertexData;

            uint8 GetOffsetX() const { return LiquidVertexFormat < 42 ? OffsetX : 0; }
            uint8 GetOffsetY() const { return LiquidVertexFormat < 42 ? OffsetY : 0; }
            uint8 GetWidth() const { return LiquidVertexFormat < 42 ? Width : 8; }
            uint8 GetHeight() const { return LiquidVertexFormat < 42 ? Height : 8; }
        };

        struct Attributes
        {
            uint64 Fishable;
            uint64 Deep;
        };

        struct SMLiquidChunk
        {
            uint32 OffsetInstances;
            uint32 LayerCount;
            uint32 OffsetAttributes;
        } chunks[ADT::CELLS_PER_GRID][ADT::CELLS_PER_GRID];

        Instance const* GetLiquidInstance(uint32 layer, int32 x, int32 y) const;
        Attributes GetLiquidAttributes(int32 x, int32 y) const;
        uint16 GetLiquidType(Instance const* h) const;
        float GetLiquidHeight(Instance const* h, int32 pos) const;
        int8 GetLiquidDepth(Instance const* h, int32 pos) const;
        uint16 const* GetLiquidTextureCoordMap(Instance const* h, int32 pos) const;
        uint64 GetLiquidExistsBitmap(Instance const* h) const;
        LiquidVertexFormatType GetLiquidVertexFormat(Instance const* liquidInstance) const;
    };

    struct MFBO
    {
        struct plane
        {
            int16 coords[9];
        };
        plane max;
        plane min;
    };

    struct MMDX
    {
        // Actual size is Header::Size
        char FileNames[1];
    };

    struct MDDF
    {
        uint32 Id;
        uint32 UniqueId;
        G3D::Vector3 Position;
        G3D::Vector3 Rotation;
        uint16 Scale;
        uint16 Flags;
    };

    namespace Traits
    {
        template<typename T>
        struct HasSubchunks : std::false_type { };

        template<typename T>
        struct HasMultipleCopiesUnderOneHeader : std::false_type { };

        // Specializations
        template<>
        struct HasSubchunks<MCNK> : std::true_type { };

        template<>
        struct HasMultipleCopiesUnderOneHeader<MODF> : std::true_type { };

        template<>
        struct HasMultipleCopiesUnderOneHeader<MDDF> : std::true_type { };
    }
}

#pragma pack(pop)
}

#endif // Definitions_h__
