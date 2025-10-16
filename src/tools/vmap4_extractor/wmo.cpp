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

#include "vmapexport.h"
#include "adtfile.h"
#include "cascfile.h"
#include "Errors.h"
#include "StringFormat.h"
#include "vec3d.h"
#include "VMapDefinitions.h"
#include "wmo.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <cstdio>
#include <cstdlib>

WMORoot::WMORoot(std::string const& filename)
    : filename(filename), color(0), nTextures(0), nGroups(0), nPortals(0), nLights(0),
    nDoodadNames(0), nDoodadDefs(0), nDoodadSets(0), RootWMOID(0), flags(0), numLod(0)
{
    memset(bbcorn1, 0, sizeof(bbcorn1));
    memset(bbcorn2, 0, sizeof(bbcorn2));
}

extern std::shared_ptr<CASC::Storage> CascStorage;

bool WMORoot::open()
{
    CASCFile f(CascStorage, filename.c_str());
    if(f.isEof ())
    {
        printf("No such file.\n");
        return false;
    }

    uint32 size;
    char fourcc[5];

    while (!f.isEof())
    {
        f.read(fourcc,4);
        f.read(&size, 4);

        flipcc(fourcc);
        fourcc[4] = 0;

        size_t nextpos = f.getPos() + size;

        if (!strcmp(fourcc,"MOHD")) // header
        {
            f.read(&nTextures, 4);
            f.read(&nGroups, 4);
            f.read(&nPortals, 4);
            f.read(&nLights, 4);
            f.read(&nDoodadNames, 4);
            f.read(&nDoodadDefs, 4);
            f.read(&nDoodadSets, 4);
            f.read(&color, 4);
            f.read(&RootWMOID, 4);
            f.read(bbcorn1, 12);
            f.read(bbcorn2, 12);
            f.read(&flags, 2);
            f.read(&numLod, 2);
        }
        else if (!strcmp(fourcc, "MODS"))
        {
            DoodadData.Sets.resize(size / sizeof(WMO::MODS));
            f.read(DoodadData.Sets.data(), size);
        }
        else if (!strcmp(fourcc,"MODN"))
        {
            ASSERT(!DoodadData.FileDataIds);

            char* ptr = f.getPointer();
            char* end = ptr + size;
            DoodadData.Paths = std::make_unique<char[]>(size);
            memcpy(DoodadData.Paths.get(), ptr, size);
            while (ptr < end)
            {
                std::string path = ptr;

                char* s = GetPlainName(ptr);
                NormalizeFileName(s, strlen(s));

                uint32 doodadNameIndex = ptr - f.getPointer();
                ptr += path.length() + 1;

                if (ExtractSingleModel(path))
                    ValidDoodadNames.insert(doodadNameIndex);
            }
        }
        else if (!strcmp(fourcc, "MODI"))
        {
            ASSERT(!DoodadData.Paths);

            uint32 fileDataIdCount = size / sizeof(uint32);
            DoodadData.FileDataIds = std::make_unique<uint32[]>(fileDataIdCount);
            f.read(DoodadData.FileDataIds.get(), size);
            for (uint32 i = 0; i < fileDataIdCount; ++i)
            {
                if (!DoodadData.FileDataIds[i])
                    continue;

                std::string path = Trinity::StringFormat("FILE{:08X}.xxx", DoodadData.FileDataIds[i]);
                if (ExtractSingleModel(path))
                    ValidDoodadNames.insert(i);
            }
        }
        else if (!strcmp(fourcc,"MODD"))
        {
            DoodadData.Spawns.resize(size / sizeof(WMO::MODD));
            f.read(DoodadData.Spawns.data(), size);
        }
        else if (!strcmp(fourcc, "MOGN"))
        {
            GroupNames.resize(size);
            f.read(GroupNames.data(), size);
        }
        else if (!strcmp(fourcc, "GFID"))
        {
            // full LOD reading code for reference
            // commented out as we are not interested in any of them beyond first, most detailed

            //uint16 lodCount = 1;
            //if (flags & 0x10)
            //{
            //    if (numLod)
            //        lodCount = numLod;
            //    else
            //        lodCount = 3;
            //}

            //for (uint32 lod = 0; lod < lodCount; ++lod)
            //{
                for (uint32 gp = 0; gp < nGroups; ++gp)
                {
                    uint32 fileDataId;
                    f.read(&fileDataId, 4);
                    if (fileDataId)
                        groupFileDataIDs.push_back(fileDataId);
                }
            //}
        }
        /*
        else if (!strcmp(fourcc,"MOTX"))
        {
        }
        else if (!strcmp(fourcc,"MOMT"))
        {
        }
        else if (!strcmp(fourcc,"MOGI"))
        {
        }
        else if (!strcmp(fourcc,"MOLT"))
        {
        }
        else if (!strcmp(fourcc,"MOSB"))
        {
        }
        else if (!strcmp(fourcc,"MOPV"))
        {
        }
        else if (!strcmp(fourcc,"MOPT"))
        {
        }
        else if (!strcmp(fourcc,"MOPR"))
        {
        }
        else if (!strcmp(fourcc,"MFOG"))
        {
        }
        */
        f.seek((int)nextpos);
    }
    f.close ();
    return true;
}

bool WMORoot::ConvertToVMAPRootWmo(FILE* pOutfile)
{
    //printf("Convert RootWmo...\n");

    fwrite(VMAP::RAW_VMAP_MAGIC, 1, 8, pOutfile);
    unsigned int nVectors = 0;
    fwrite(&nVectors,sizeof(nVectors), 1, pOutfile); // will be filled later
    fwrite(&nGroups, 4, 1, pOutfile);
    fwrite(&RootWMOID, 4, 1, pOutfile);
    ModelFlags tcFlags = ModelFlags::None;
    fwrite(&tcFlags, sizeof(ModelFlags), 1, pOutfile);
    return true;
}

WMOGroup::WMOGroup(const std::string &filename) :
    filename(filename), MPY2(nullptr), MOVX(nullptr), MOVT(nullptr), MOBA(nullptr), MobaEx(nullptr),
    hlq(nullptr), LiquEx(nullptr), LiquBytes(nullptr), groupName(0), descGroupName(0), mogpFlags(0),
    moprIdx(0), moprNItems(0), nBatchA(0), nBatchB(0), nBatchC(0), fogIdx(0),
    groupLiquid(0), groupWMOID(0), moba_size(0), LiquEx_size(0),
    nVertices(0), nTriangles(0), liquflags(0)
{
    memset(bbcorn1, 0, sizeof(bbcorn1));
    memset(bbcorn2, 0, sizeof(bbcorn2));
}

bool WMOGroup::open(WMORoot* rootWMO)
{
    CASCFile f(CascStorage, filename.c_str());
    if(f.isEof ())
    {
        printf("No such file.\n");
        return false;
    }
    uint32 size;
    char fourcc[5] = { };
    while (!f.isEof())
    {
        f.read(fourcc,4);
        f.read(&size, 4);
        flipcc(fourcc);
        if (!strcmp(fourcc,"MOGP")) //size specified in MOGP chunk is all the other chunks combined, adjust to read MOGP-only
            size = 68;

        size_t nextpos = f.getPos() + size;
        if (!strcmp(fourcc,"MOGP"))//header
        {
            f.read(&groupName, 4);
            f.read(&descGroupName, 4);
            f.read(&mogpFlags, 4);
            f.read(bbcorn1, 12);
            f.read(bbcorn2, 12);
            f.read(&moprIdx, 2);
            f.read(&moprNItems, 2);
            f.read(&nBatchA, 2);
            f.read(&nBatchB, 2);
            f.read(&nBatchC, 4);
            f.read(&fogIdx, 4);
            f.read(&groupLiquid, 4);
            f.read(&groupWMOID, 4);
            f.read(&mogpFlags2, 4);
            f.read(&parentOrFirstChildSplitGroupIndex, 2);
            f.read(&nextSplitChildGroupIndex, 2);

            // according to WoW.Dev Wiki:
            if (rootWMO->flags & 4)
                groupLiquid = GetLiquidTypeId(groupLiquid);
            else if (groupLiquid == 15)
                groupLiquid = 0;
            else
                groupLiquid = GetLiquidTypeId(groupLiquid + 1);

            if (groupLiquid && !IsLiquidIgnored(groupLiquid))
                liquflags |= 2;
        }
        else if (!strcmp(fourcc,"MOPY"))
        {
            MPY2 = std::make_unique<uint16[]>(size);
            std::unique_ptr<uint8[]> MOPY = std::make_unique<uint8[]>(size);
            nTriangles = (int)size / 2;
            f.read(MOPY.get(), size);
            std::copy_n(MOPY.get(), size, MPY2.get());
        }
        else if (!strcmp(fourcc,"MPY2"))
        {
            MPY2 = std::make_unique<uint16[]>(size / 2);
            nTriangles = (int)size / 4;
            f.read(MPY2.get(), size);
        }
        else if (!strcmp(fourcc,"MOVI"))
        {
            MOVX = std::make_unique<uint32[]>(size / 2);
            std::unique_ptr<uint16[]> MOVI = std::make_unique<uint16[]>(size / 2);
            f.read(MOVI.get(), size);
            std::copy_n(MOVI.get(), size / 2, MOVX.get());
        }
        else if (!strcmp(fourcc,"MOVX"))
        {
            MOVX = std::make_unique<uint32[]>(size / 4);
            f.read(MOVX.get(), size);
        }
        else if (!strcmp(fourcc,"MOVT"))
        {
            MOVT = new float[size/4];
            f.read(MOVT, size);
            nVertices = (int)size / 12;
        }
        else if (!strcmp(fourcc,"MONR"))
        {
        }
        else if (!strcmp(fourcc,"MOTV"))
        {
        }
        else if (!strcmp(fourcc,"MOBA"))
        {
            MOBA = new uint16[size/2];
            moba_size = size/2;
            f.read(MOBA, size);
        }
        else if (!strcmp(fourcc,"MODR"))
        {
            DoodadReferences.resize(size / sizeof(uint16));
            f.read(DoodadReferences.data(), size);
        }
        else if (!strcmp(fourcc,"MLIQ"))
        {
            liquflags |= 1;
            hlq = new WMOLiquidHeader();
            f.read(hlq, sizeof(WMOLiquidHeader));
            LiquEx_size = sizeof(WMOLiquidVert) * hlq->xverts * hlq->yverts;
            LiquEx = new WMOLiquidVert[hlq->xverts * hlq->yverts];
            f.read(LiquEx, LiquEx_size);
            int nLiquBytes = hlq->xtiles * hlq->ytiles;
            LiquBytes = new char[nLiquBytes];
            f.read(LiquBytes, nLiquBytes);

            // Determine legacy liquid type
            if (!groupLiquid)
            {
                for (int i = 0; i < nLiquBytes; ++i)
                {
                    if ((LiquBytes[i] & 0xF) != 15)
                    {
                        groupLiquid = GetLiquidTypeId((LiquBytes[i] & 0xF) + 1);
                        break;
                    }
                }
            }

            if (IsLiquidIgnored(groupLiquid))
                liquflags = 0;

            /* std::ofstream llog("Buildings/liquid.log", ios_base::out | ios_base::app);
            llog << filename;
            llog << "\nbbox: " << bbcorn1[0] << ", " << bbcorn1[1] << ", " << bbcorn1[2] << " | " << bbcorn2[0] << ", " << bbcorn2[1] << ", " << bbcorn2[2];
            llog << "\nlpos: " << hlq->pos_x << ", " << hlq->pos_y << ", " << hlq->pos_z;
            llog << "\nx-/yvert: " << hlq->xverts << "/" << hlq->yverts << " size: " << size << " expected size: " << 30 + hlq->xverts*hlq->yverts*8 + hlq->xtiles*hlq->ytiles << std::endl;
            llog.close(); */
        }
        f.seek((int)nextpos);
    }
    f.close();
    return true;
}

int WMOGroup::ConvertToVMAPGroupWmo(FILE* output, bool preciseVectorData)
{
    fwrite(&mogpFlags,sizeof(uint32),1,output);
    fwrite(&groupWMOID,sizeof(uint32),1,output);
    // group bound
    fwrite(bbcorn1, sizeof(float), 3, output);
    fwrite(bbcorn2, sizeof(float), 3, output);
    fwrite(&liquflags,sizeof(uint32),1,output);
    int nColTriangles = 0;
    if (preciseVectorData)
    {
        char GRP[] = "GRP ";
        fwrite(GRP,1,4,output);

        int k = 0;
        int moba_batch = moba_size/12;
        MobaEx = new int[moba_batch*4];
        for(int i=8; i<moba_size; i+=12)
        {
            MobaEx[k++] = MOBA[i];
        }
        int moba_size_grp = moba_batch*4+4;
        fwrite(&moba_size_grp,4,1,output);
        fwrite(&moba_batch,4,1,output);
        fwrite(MobaEx,4,k,output);
        delete [] MobaEx;

        uint32 nIdexes = nTriangles * 3;

        if(fwrite("INDX",4, 1, output) != 1)
        {
            printf("Error while writing file nbraches ID");
            exit(0);
        }
        int wsize = sizeof(uint32) + sizeof(unsigned short) * nIdexes;
        if(fwrite(&wsize, sizeof(int), 1, output) != 1)
        {
            printf("Error while writing file wsize");
            // no need to exit?
        }
        if(fwrite(&nIdexes, sizeof(uint32), 1, output) != 1)
        {
            printf("Error while writing file nIndexes");
            exit(0);
        }
        if(nIdexes >0)
        {
            if (fwrite(MOVX.get(), sizeof(uint32), nIdexes, output) != nIdexes)
            {
                printf("Error while writing file indexarray");
                exit(0);
            }
        }

        if(fwrite("VERT",4, 1, output) != 1)
        {
            printf("Error while writing file nbraches ID");
            exit(0);
        }
        wsize = sizeof(int) + sizeof(float) * 3 * nVertices;
        if(fwrite(&wsize, sizeof(int), 1, output) != 1)
        {
            printf("Error while writing file wsize");
            // no need to exit?
        }
        if(fwrite(&nVertices, sizeof(int), 1, output) != 1)
        {
            printf("Error while writing file nVertices");
            exit(0);
        }
        if(nVertices >0)
        {
            if(fwrite(MOVT, sizeof(float)*3, nVertices, output) != nVertices)
            {
                printf("Error while writing file vectors");
                exit(0);
            }
        }

        nColTriangles = nTriangles;
    }
    else
    {
        char GRP[] = "GRP ";
        fwrite(GRP,1,4,output);
        int k = 0;
        int moba_batch = moba_size/12;
        MobaEx = new int[moba_batch*4];
        for(int i=8; i<moba_size; i+=12)
        {
            MobaEx[k++] = MOBA[i];
        }

        int moba_size_grp = moba_batch*4+4;
        fwrite(&moba_size_grp,4,1,output);
        fwrite(&moba_batch,4,1,output);
        fwrite(MobaEx,4,k,output);
        delete [] MobaEx;

        //-------INDX------------------------------------
        //-------MOPY/MPY2--------
        std::unique_ptr<uint32[]> MovxEx = std::make_unique<uint32[]>(nTriangles*3); // "worst case" size...
        std::unique_ptr<int32[]> IndexRenum = std::make_unique<int32[]>(nVertices);
        std::fill_n(IndexRenum.get(), nVertices, -1);
        for (int i=0; i<nTriangles; ++i)
        {
            // Skip no collision triangles
            bool isRenderFace = (MPY2[2 * i] & WMO_MATERIAL_RENDER) && !(MPY2[2 * i] & WMO_MATERIAL_DETAIL);
            bool isCollision = MPY2[2 * i] & WMO_MATERIAL_COLLISION || isRenderFace;

            if (!isCollision)
                continue;

            // Use this triangle
            for (int j=0; j<3; ++j)
            {
                IndexRenum[MOVX[3*i + j]] = 1;
                MovxEx[3*nColTriangles + j] = MOVX[3*i + j];
            }
            ++nColTriangles;
        }

        // assign new vertex index numbers
        uint32 nColVertices = 0;
        for (uint32 i=0; i<nVertices; ++i)
        {
            if (IndexRenum[i] == 1)
            {
                IndexRenum[i] = nColVertices;
                ++nColVertices;
            }
        }

        // translate triangle indices to new numbers
        for (int i=0; i<3*nColTriangles; ++i)
        {
            ASSERT(MovxEx[i] < nVertices);
            MovxEx[i] = IndexRenum[MovxEx[i]];
        }

        // write triangle indices
        int INDX[] = {0x58444E49, nColTriangles*6+4, nColTriangles*3};
        fwrite(INDX,4,3,output);
        fwrite(MovxEx.get(),4,nColTriangles*3,output);

        // write vertices
        uint32 VERT[] = {0x54524556u, nColVertices*3*static_cast<uint32>(sizeof(float))+4, nColVertices};// "VERT"
        int check = 3*nColVertices;
        fwrite(VERT,4,3,output);
        for (uint32 i=0; i<nVertices; ++i)
            if(IndexRenum[i] >= 0)
                check -= fwrite(MOVT+3*i, sizeof(float), 3, output);

        ASSERT(check==0);
    }

    //------LIQU------------------------
    if (liquflags & 3)
    {
        int LIQU_totalSize = sizeof(uint32);
        if (liquflags & 1)
        {
            LIQU_totalSize += sizeof(WMOLiquidHeader);
            LIQU_totalSize += LiquEx_size / sizeof(WMOLiquidVert) * sizeof(float);
            LIQU_totalSize += hlq->xtiles * hlq->ytiles;
        }
        int LIQU_h[] = { 0x5551494C, LIQU_totalSize };// "LIQU"
        fwrite(LIQU_h, 4, 2, output);

        /* std::ofstream llog("Buildings/liquid.log", ios_base::out | ios_base::app);
        llog << filename;
        llog << ":\nliquidEntry: " << liquidEntry << " type: " << hlq->type << " (root:" << rootWMO->flags << " group:" << flags << ")\n";
        llog.close(); */

        fwrite(&groupLiquid, sizeof(uint32), 1, output);
        if (liquflags & 1)
        {
            fwrite(hlq, sizeof(WMOLiquidHeader), 1, output);
            // only need height values, the other values are unknown anyway
            for (uint32 i = 0; i < LiquEx_size / sizeof(WMOLiquidVert); ++i)
                fwrite(&LiquEx[i].height, sizeof(float), 1, output);
            // todo: compress to bit field
            fwrite(LiquBytes, 1, hlq->xtiles * hlq->ytiles, output);
        }
    }

    return nColTriangles;
}

uint32 WMOGroup::GetLiquidTypeId(uint32 liquidTypeId)
{
    if (liquidTypeId < 21 && liquidTypeId)
    {
        switch (((static_cast<uint8>(liquidTypeId) - 1) & 3))
        {
            case 0: return ((mogpFlags & 0x80000) != 0) + 13;
            case 1: return 14;
            case 2: return 19;
            case 3: return 20;
            default: break;
        }
    }
    return liquidTypeId;
}

bool WMOGroup::ShouldSkip(WMORoot const* root) const
{
    // skip unreachable
    if (mogpFlags & 0x80)
        return true;

    // skip antiportals
    if (mogpFlags & 0x4000000)
        return true;

    if (groupName < int32(root->GroupNames.size()) && !strcmp(&root->GroupNames[groupName], "antiportal"))
        return true;

    return false;
}

WMOGroup::~WMOGroup()
{
    delete [] MOVT;
    delete [] MOBA;
    delete hlq;
    delete [] LiquEx;
    delete [] LiquBytes;
}

bool MapObject::ShouldExtract(ADT::MODF& mapObjDef, std::string& fileName, [[maybe_unused]] uint32 mapId)
{
    if ((mapObjDef.Flags & 0x1) != 0)
    {
        //if (FILE* destro = fopen("Buildings/destructible.log", "a"))
        //{
        //    fprintf(destro, R"(  { fileName: "%s", fileDataID: %u, mapId: %u, uniqueId: %u, pos: { x: %f, y: %f, z: %f } },)" "\n",
        //        fileName.c_str(), mapObjDef.Id, mapId, mapObjDef.UniqueId, 533.33333f * 32 - mapObjDef.Position.z, 533.33333f * 32 - mapObjDef.Position.x, mapObjDef.Position.y);
        //    fclose(destro);
        //}

        // keep objects whose purpose is to be ground and only skip obstacles (like walls)
        switch (mapObjDef.UniqueId)
        {
            // 571 - Northrend
            case 1894698: // nexus_floating_platform01.wmo
            case 2713315: // dsexcavationplatform.wmo
            case 2422013: // wg_siege01.wmo
            case 2468284: // wg_siege01.wmo
            case 2422014: // wg_siege01.wmo
            case 2422015: // wg_siege01.wmo
            case 2422012: // wg_siege01.wmo
            case 2453481: // wg_siege01.wmo
            case 2339089: // wg_tower01.wmo
            case 2339090: // wg_tower01.wmo
            case 2339091: // wg_tower01.wmo
            case 2339092: // wg_tower01.wmo
            case 2381624: // wg_tower02.wmo
            case 2381625: // wg_tower02.wmo
            case 2381628: // wg_tower02.wmo
            case 2709387: // nd_human_tower_open.wmo
            case 2705797: // nd_human_wall_end_small02.wmo
            case 2705802: // nd_human_wall_end_small02.wmo
            case 2705805: // nd_human_wall_end_small02.wmo
            case 2705808: // nd_human_wall_end_small02.wmo
            case 2706003: // nd_human_wall_end_small02.wmo
            case 2706004: // nd_human_wall_end_small02.wmo
            case 2706005: // nd_human_wall_end_small02.wmo
            case 2706006: // nd_human_wall_end_small02.wmo
            case 2705798: // nd_human_wall_small02.wmo
            case 2705801: // nd_human_wall_small02.wmo
            case 2705803: // nd_human_wall_small02.wmo
            case 2705807: // nd_human_wall_small02.wmo
            case 2706001: // nd_human_wall_small02.wmo
            case 2706002: // nd_human_wall_small02.wmo
            case 2706007: // nd_human_wall_small02.wmo
            case 2706008: // nd_human_wall_small02.wmo
            case 2707736: // stormpeaks_irongiant_01.wmo
            // 603 - Ulduar
            case 3018484: // ulduar_tower01.wmo
            case 4013183: // ulduar_building01.wmo
            case 4013193: // ulduar_building01.wmo
            case 4013195: // ulduar_building01.wmo
            case 4013204: // ulduar_building01.wmo
            case 4027460: // ulduar_building01.wmo
            case 4013202: // ulduar_tower01.wmo
            case 4013219: // ulduar_building01.wmo
            case 4013222: // ulduar_building01.wmo
            case 4013231: // ulduar_building01.wmo
            case 4013234: // ulduar_building01.wmo
            case 4013196: // ulduar_building01.wmo
            case 4013199: // ulduar_tower01.wmo
            case 4013205: // ulduar_building01.wmo
            case 4013215: // ulduar_building01.wmo
            case 4013218: // ulduar_building01.wmo
            case 4013223: // ulduar_building01.wmo
            case 4013226: // ulduar_tower01.wmo
            case 4013228: // ulduar_building01.wmo
            case 4013229: // ulduar_building01.wmo
            case 4013232: // ulduar_building01.wmo
            case 4013235: // ulduar_building01.wmo
            case 4013236: // ulduar_building01.wmo
            // 609 - Ebon Hold
            case 2559451: // guardtower.wmo
            case 2616079: // pirateship.wmo
            // 616 - The Eye of Eternity
            case 2709658: // nexus_raid_floating_platform.wmo
            // 631 - Icecrown Citadel
            case 5535469: // icecrownraid_arthas_precipice_phase0.wmo
            // 649 - Trial of the Crusader
            case 4313980: // coliseum_intact_floor.wmo
                break;
            // 720 - Firelands
            case 6316308: // firelands_raid_ragnaros_platform_phase0.wmo
                fileName = "FILE0008176F.xxx"; // replace with firelands_raid_ragnaros_platform_phase2.wmo for pathfinding reasons
                mapObjDef.DoodadSet = 1;
                break;
            case 6227023: // firelands_volcano.wmo
            case 6346397: // firelands_lavaboss_bridge_phase0.wmo
            case 6225885: // firelands_mainbridge.wmo
            case 6238854: // firelands_phoenixshell.wmo
            // 732 - Tol Barad
            case 5839901: // tb_lighthouse.wmo
            case 5839913: // tb_lighthouse.wmo
            case 5839951: // tb_lighthouse.wmo
            // 754 - Throne of the Four Winds
            case 5973793: // kl_skywall_raid.wmo
                break;
            // 755 - Lost City of the Tol'vir
            case 5994039:
                fileName = "FILE0006CFCB.xxx"; // replace with tolvir_central_building_01_c.wmo for pathfinding reasons
                mapObjDef.DoodadSet = 1;
                break;
            // 938 - End Time
            case 6718596: // firelands_lavaboss_platform.wmo
            case 6718597: // firelands_lavaboss_platform.wmo
            case 6718598: // firelands_lavaboss_platform.wmo
            case 6718599: // firelands_lavaboss_platform.wmo
            // 962 - Gate of the Setting Sun
            case 7688762: // pa_greatwall_corner_03.wmo
            case 7688763: // pa_greatwall_corner_02.wmo
            case 7688764: // pa_greatwall_corner_01.wmo
            // 967 - Dragon Soul
            case 7006169: // nd_alliancegunship_dragonsoul.wmo
            // 1064 - Mogu Island Daily Area
            case 8542587: // mini_sunwell_a.wmo
            case 8270873: // pa_dalaran_tower_complete01.wmo
            // 1126 - Mogu Island Progression Events
            case 8542608: // mini_sunwell_a.wmo
            case 8527321: // pa_dalaran_tower_complete01.wmo
            // 1205 - Blackrock Foundry
            case 9623865: // 6du_bkfoundry_blackhand_a.wmo
            // 1220 - Broken Isles
            case 17283359: // 7bs_nightelf_moonwell_phase01.wmo
            case 17578766: // 7bs_nightelf_commandbuilding_phase01.wmo
            case 17283357: // 7bs_nightelf_towertall_phase01.wmo
            // 1265 - Tanaan Jungle Intro
            case 9915502: // 6ih_ironhorde_dam.wmo
            case 10311435: // 6ih_ironhorde_supertank_tilted.wmo
            case 10192067: // 6ih_ironhorde_ship01.wmo
            case 10179933: // 6ih_ironhorde_ship01.wmo
            case 10616762: // 6tj_darkportal.wmo
            case 9973026: // 6ih_ironhorde_bridged.wmo
            // 1492 - Maw of Souls
            case 12907468: // 7du_helheim_ghostship.wmo
            // 1530 - The Nighthold
            case 15382672: // 7du_suramarraid_topa.wmo
            // 1579 - Ulduar (scenario)
            case 23240511: // ulduar_building01.wmo
            case 23240528: // ulduar_building01.wmo
            case 23240555: // ulduar_building01.wmo
            case 23240556: // ulduar_building01.wmo
            case 23240557: // ulduar_building01.wmo
            case 23240560: // ulduar_tower01.wmo
            case 23240514: // ulduar_tower01.wmo
            case 23240531: // ulduar_building01.wmo
            case 23240532: // ulduar_building01.wmo
            case 23240534: // ulduar_building01.wmo
            case 23240554: // ulduar_building01.wmo
            case 23240527: // ulduar_building01.wmo
            case 23240530: // ulduar_building01.wmo
            case 23240559: // ulduar_tower01.wmo
            case 23240512: // ulduar_building01.wmo
            case 23240513: // ulduar_building01.wmo
            case 23240518: // ulduar_building01.wmo
            case 23240521: // ulduar_building01.wmo
            case 23240522: // ulduar_building01.wmo
            case 23240523: // ulduar_building01.wmo
            case 23240525: // ulduar_tower01.wmo
            case 23240533: // ulduar_building01.wmo
            case 23240558: // ulduar_building01.wmo
            // 1676 - Tomb of Sargeras
            case 17003270: // 7du_tombofsargeras_avatarfloor15.wmo
            case 17003271: // 7du_tombofsargeras_avatarfloor19.wmo
            case 17003272: // 7du_tombofsargeras_avatarfloor25.wmo
            case 17003273: // 7du_tombofsargeras_avatarfloor22.wmo
            case 17003274: // 7du_tombofsargeras_avatarfloor23.wmo
            case 17003275: // 7du_tombofsargeras_avatarfloor20.wmo
            case 17003276: // 7du_tombofsargeras_avatarfloor21.wmo
            case 17003277: // 7du_tombofsargeras_avatarfloor24.wmo
            case 17003278: // 7du_tombofsargeras_avatarfloor07.wmo
            case 17003279: // 7du_tombofsargeras_avatarfloor18.wmo
            case 17003280: // 7du_tombofsargeras_avatarfloor05.wmo
            case 17003281: // 7du_tombofsargeras_avatarfloor16.wmo
            case 17003282: // 7du_tombofsargeras_avatarfloor08.wmo
            case 17003283: // 7du_tombofsargeras_avatarfloor06.wmo
            case 17003284: // 7du_tombofsargeras_avatarfloor13.wmo
            case 17003285: // 7du_tombofsargeras_avatarfloor10.wmo
            case 17003286: // 7du_tombofsargeras_avatarfloor11.wmo
            case 17003287: // 7du_tombofsargeras_avatarfloor12.wmo
            case 17003288: // 7du_tombofsargeras_avatarfloor01.wmo
            case 17003289: // 7du_tombofsargeras_avatarfloor14.wmo
            case 17003290: // 7du_tombofsargeras_avatarfloor03.wmo
            case 17003291: // 7du_tombofsargeras_avatarfloor02.wmo
            case 17003292: // 7du_tombofsargeras_avatarfloor09.wmo
            case 17003293: // 7du_tombofsargeras_avatarfloor04.wmo
            case 17003294: // 7du_tombofsargeras_avatarfloor17.wmo
            case 17066842: // 7du_tombofsargeras_upperavatarfloor01.wmo
            // 1704 - Legion Ship - Horizontal - Valsharah
            case 16960691: // 7lg_legion_commandcenter01_horizontal.wmo
            // 1705 - Legion Ship - Horizontal - Azsuna
            case 17175153: // 7lg_legion_commandcenter01_horizontal.wmo
            // 1706 - Legion Ship - Vertical - HighMountain
            case 17224635: // 7lg_legion_commandcenter01.wmo
            // 1707 - Legion Ship - Vertical - Stormheim
            case 17019460: // 7lg_legion_commandcenter01.wmo
            // 1734 - Throne of the Four Winds - Shaman Class Mount
            case 17213860: // kl_skywall_raid.wmo
            // 2076 - Firelands - Dark Iron Dwarf
            case 21753085: // firelands_raid_ragnaros_platform_phase0.wmo
            case 21753055: // firelands_volcano.wmo
            case 21753065: // firelands_lavaboss_bridge_phase0.wmo
            case 21753072: // firelands_mainbridge.wmo
            case 21753087: // firelands_phoenixshell.wmo
            // 2118 - Battle for Wintergrasp
            case 23091378: // wg_siege01.wmo
            case 23091529: // wg_siege01.wmo
            case 23089816: // wg_siege01.wmo
            case 23091562: // wg_siege01.wmo
            case 23091391: // wg_siege01.wmo
            case 23091511: // wg_siege01.wmo
            case 23089846: // wg_tower01.wmo
            case 23089847: // wg_tower01.wmo
            case 23091372: // wg_tower01.wmo
            case 23091414: // wg_tower01.wmo
            case 23091540: // wg_tower02.wmo
            case 23091491: // wg_tower02.wmo
            case 23091413: // wg_tower02.wmo
            // 2235 - Caverns of Time - Anniversary
            case 26230342: // firelands_raid_ragnaros_platform_phase0.wmo
            case 26030340: // icecrownraid_arthas_precipice_phase0.wmo
            // 2654 - 10.2. Nighthold
            case 44122627: // 7du_suramarraid_topa.wmo
            // 2657 - Nerub-ar Palace
            case 46861327: // 11nb_nerubian_platform_boss01.wmo
            case 48344925: // 11nb_nerubian_platform_boss01.wmo
            // 2819 - 11.1.7 Icecrown Citadel
            case 52962111: // icecrownraid_arthas_precipice_phase0.wmo
                break;
            default:
                return false;
        }
    }

    return true;
}

void MapObject::Extract(ADT::MODF const& mapObjDef, char const* WmoInstName, bool isGlobalWmo, uint32 mapID, uint32 originalMapId, FILE* pDirfile, std::vector<ADTOutputCache>* dirfileCache)
{
    //-----------add_in _dir_file----------------

    std::string tempname = Trinity::StringFormat("{}/{}", szWorkDirWmo, WmoInstName);
    FILE* input = fopen(tempname.c_str(), "r+b");

    if (!input)
    {
        printf("WMOInstance::WMOInstance: couldn't open %s\n", tempname.c_str());
        return;
    }

    fseek(input, 8, SEEK_SET); // get the correct no of vertices
    int nVertices;
    int count = fread(&nVertices, sizeof(int), 1, input);
    fclose(input);

    if (count != 1 || nVertices == 0)
        return;

    Vec3D position = fixCoords(mapObjDef.Position);
    AaBox3D bounds;
    bounds.min = fixCoords(mapObjDef.Bounds.min);
    bounds.max = fixCoords(mapObjDef.Bounds.max);

    if (isGlobalWmo)
    {
        position += Vec3D(533.33333f * 32, 533.33333f * 32, 0.0f);
        bounds += Vec3D(533.33333f * 32, 533.33333f * 32, 0.0f);
    }

    float scale = 1.0f;
    if (mapObjDef.Flags & 0x4)
        scale = mapObjDef.Scale / 1024.0f;
    uint32 uniqueId = GenerateUniqueObjectId(mapObjDef.UniqueId, 0, true);
    uint8 flags = MOD_HAS_BOUND;
    uint8 nameSet = mapObjDef.NameSet;
    if (mapID != originalMapId)
        flags |= MOD_PARENT_SPAWN;
    if (mapObjDef.Flags & 0x1)
        flags |= MOD_PATH_ONLY;

    //write Flags, NameSet, UniqueId, Pos, Rot, Scale, Bound_lo, Bound_hi, name
    fwrite(&flags, sizeof(uint8), 1, pDirfile);
    fwrite(&nameSet, sizeof(uint8), 1, pDirfile);
    fwrite(&uniqueId, sizeof(uint32), 1, pDirfile);
    fwrite(&position, sizeof(Vec3D), 1, pDirfile);
    fwrite(&mapObjDef.Rotation, sizeof(Vec3D), 1, pDirfile);
    fwrite(&scale, sizeof(float), 1, pDirfile);
    fwrite(&bounds, sizeof(AaBox3D), 1, pDirfile);
    uint32 nlen = strlen(WmoInstName);
    fwrite(&nlen, sizeof(uint32), 1, pDirfile);
    fwrite(WmoInstName, sizeof(char), nlen, pDirfile);

    if (dirfileCache)
    {
        dirfileCache->emplace_back();
        ADTOutputCache& cacheModelData = dirfileCache->back();
        cacheModelData.Flags = flags & ~MOD_PARENT_SPAWN;
        cacheModelData.Data.resize(
            sizeof(uint8) +     // nameSet
            sizeof(uint32) +    // uniqueId
            sizeof(Vec3D) +     // position
            sizeof(Vec3D) +     // mapObjDef.Rotation
            sizeof(float) +     // scale
            sizeof(AaBox3D) +   // bounds
            sizeof(uint32) +    // nlen
            nlen);              // WmoInstName

        uint8* cacheData = cacheModelData.Data.data();
#define CACHE_WRITE(value, size, count, dest) memcpy(dest, value, size * count); dest += size * count;

        CACHE_WRITE(&nameSet, sizeof(uint8), 1, cacheData);
        CACHE_WRITE(&uniqueId, sizeof(uint32), 1, cacheData);
        CACHE_WRITE(&position, sizeof(Vec3D), 1, cacheData);
        CACHE_WRITE(&mapObjDef.Rotation, sizeof(Vec3D), 1, cacheData);
        CACHE_WRITE(&scale, sizeof(float), 1, cacheData);
        CACHE_WRITE(&bounds, sizeof(AaBox3D), 1, cacheData);
        CACHE_WRITE(&nlen, sizeof(uint32), 1, cacheData);
        CACHE_WRITE(WmoInstName, sizeof(char), nlen, cacheData);

#undef CACHE_WRITE
    }
}
