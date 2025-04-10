/*****************************************************************************/
/* CascCommon.h                           Copyright (c) Ladislav Zezula 2014 */
/*---------------------------------------------------------------------------*/
/* Common functions for CascLib                                              */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 29.04.14  1.00  Lad  The first version of CascCommon.h                    */
/*****************************************************************************/

#ifndef __CASCCOMMON_H__
#define __CASCCOMMON_H__

//-----------------------------------------------------------------------------
// Compression support

// Include functions from zlib
#ifndef CASC_USE_SYSTEM_ZLIB
    #include "zlib/zlib.h"
#else
    #include <zlib.h>
#endif

#if defined(_DEBUG) && !defined(CASCLIB_NODEBUG)
#define CASCLIB_DEBUG
//#define CASCLIB_WRITE_VERIFIED_FILENAMES            // If defined, TRootHandler_WoW will save all files whose hashes are confirmed
#endif

#include "CascPort.h"
#include "common/Common.h"
#include "common/Array.h"
#include "common/ArraySparse.h"
#include "common/Map.h"
#include "common/FileTree.h"
#include "common/FileStream.h"
#include "common/Directory.h"
#include "common/ListFile.h"
#include "common/Csv.h"
#include "common/Mime.h"
#include "common/Path.h"
#include "common/RootHandler.h"
#include "common/Sockets.h"

// Headers for hashes used in CascLib
#include "hashes/md5.h"
#include "hashes/sha1.h"

// For HashStringJenkins
#include "jenkins/lookup.h"

// On-disk CASC structures
#include "CascStructs.h"

//-----------------------------------------------------------------------------
// CascLib private defines

#if defined(CASCLIB_DEBUG) && defined(CASCLIB_DEV)
#define BREAK_ON_XKEY3(CKey, v0, v1, v2) if(CKey[0] == v0 && CKey[1] == v1 && CKey[2] == v2) { __debugbreak(); }
#define BREAKIF(condition)               if(condition)  { __debugbreak(); }
#else
#define BREAK_ON_XKEY3(CKey, v0, v1, v2) { /* NOTHING */ }
#define BREAKIF(condition)               { /* NOTHING */ }
#endif

#define CASC_MAGIC_STORAGE  0x524F545343534143      // 'CASCSTOR'
#define CASC_MAGIC_FILE     0x454C494643534143      // 'CASCFILE'
#define CASC_MAGIC_FIND     0x444E494643534143      // 'CASCFIND'

// The maximum size of an online file
#define CASC_MAX_ONLINE_FILE_SIZE   0x40000000

//-----------------------------------------------------------------------------
// In-memory structures

typedef enum _CBLD_TYPE
{
    CascBuildNone = 0,                              // No build type found
    CascBuildDb,                                    // .build.db (older storages)
    CascBuildInfo,                                  // .build.info
    CascVersions                                    // versions (cached or online)
} CBLD_TYPE, *PCBLD_TYPE;

typedef enum _CPATH_TYPE
{
    PathTypeConfig,                                 // The "config" subfolder
    PathTypeData,                                   // The "data" subfolder
    PathTypePatch                                   // The "patch" subfolder
} CPATH_TYPE, *PCPATH_TYPE;

typedef enum _CSTRTG
{
    CascCacheInvalid,                               // Do not cache anything. Used as invalid value
    CascCacheNothing,                               // Do not cache anything. Used on internal files, where the content is loaded directly to the user buffer
    CascCacheLastFrame,                             // Only cache one file frame
} CSTRTG, *PCSTRTG;

// Tag file entry, loaded from the DOWNLOAD file
typedef struct _CASC_TAG_ENTRY
{
    USHORT HashType;                                // Hash type
    char TagName[1];                                // Tag name. Variable length.
} CASC_TAG_ENTRY, *PCASC_TAG_ENTRY;

// Information about CASC main file
typedef struct _CASC_BUILD_FILE
{
    LPCTSTR szPlainName;                            // Plain file name
    CBLD_TYPE BuildFileType;                        // Build file type
    TCHAR   szFullPath[MAX_PATH];                   // Full file name
} CASC_BUILD_FILE, *PCASC_BUILD_FILE;

// Information about index file
typedef struct _CASC_INDEX
{
    CASC_BLOB FileData;
    LPTSTR szFileName;                              // Full name of the index file
    DWORD NewSubIndex;                              // New subindex
    DWORD OldSubIndex;                              // Old subindex
} CASC_INDEX, *PCASC_INDEX;

// Normalized header of the index files.
// Both version 1 and version 2 are converted to this structure
typedef struct _CASC_INDEX_HEADER
{
    USHORT IndexVersion;                            // 5 for index v 1.0, 7 for index version 2.0
    BYTE   BucketIndex;                             // Should be the same as the first byte of the hex filename.
    BYTE   StorageOffsetLength;                     // Length, in bytes, of the StorageOffset field in the EKey entry
    BYTE   EncodedSizeLength;                       // Length, in bytes, of the EncodedSize in the EKey entry
    BYTE   EKeyLength;                              // Length, in bytes, of the (trimmed) EKey in the EKey entry
    BYTE   FileOffsetBits;                          // Number of bits of the archive file offset in StorageOffset field. Rest is data segment index
    BYTE   Alignment;
    ULONGLONG SegmentSize;                          // Size of one data segment (aka data.### file)
    size_t HeaderLength;                            // Length of the on-disk header structure, in bytes
    size_t HeaderPadding;                           // Length of padding after the header
    size_t EntryLength;                             // Length of the on-disk EKey entry structure, in bytes
    size_t EKeyCount;                               // Number of EKey entries. Only supplied for index files version 1.

} CASC_INDEX_HEADER, *PCASC_INDEX_HEADER;

// Normalized footer of the archive index files (md5.index)
typedef struct _CASC_ARCINDEX_FOOTER
{
    BYTE   TocHash[MD5_HASH_SIZE];                  // Hash of the structure
    BYTE   FooterHash[MD5_HASH_SIZE];               // MD5 hash of the footer, possibly shortened
    BYTE   Version;                                 // Version of the index footer
    BYTE   OffsetBytes;                             // Length, in bytes, of the file offset field
    BYTE   SizeBytes;                               // Length, in bytes, of the file size field
    BYTE   EKeyLength;                              // Length, in bytes, of the EKey field
    BYTE   FooterHashBytes;                         // Length, in bytes, of the hash and checksum
    BYTE   Alignment[3];
    size_t ElementCount;                            // total number of elements in the index file
    size_t PageLength;                              // Length, in bytes, of the index page
    size_t ItemLength;                              // Length, in bytes, of the single item
    size_t FooterLength;                            // Length, in bytes, of the index footer structure

} CASC_ARCINDEX_FOOTER, *PCASC_ARCINDEX_FOOTER;

// Normalized header of the ENCODING file
typedef struct _CASC_ENCODING_HEADER
{
    USHORT Magic;                                   // FILE_MAGIC_ENCODING ('EN')
    USHORT Version;                                 // Expected to be 1 by CascLib
    BYTE   CKeyLength;                              // The content key length in ENCODING file. Usually 0x10
    BYTE   EKeyLength;                              // The encoded key length in ENCODING file. Usually 0x10
    DWORD  CKeyPageSize;                            // Size of the CKey page in bytes
    DWORD  EKeyPageSize;                            // Size of the CKey page in bytes
    DWORD  CKeyPageCount;                           // Number of CKey pages in the page table
    DWORD  EKeyPageCount;                           // Number of EKey pages in the page table
    DWORD  ESpecBlockSize;                          // Size of the ESpec string block, in bytes
} CASC_ENCODING_HEADER, *PCASC_ENCODING_HEADER;

typedef struct _CASC_DOWNLOAD_HEADER
{
    USHORT Magic;                                   // FILE_MAGIC_DOWNLOAD ('DL')
    USHORT Version;                                 // Version
    USHORT EKeyLength;                              // Length of the EKey
    USHORT EntryHasChecksum;                        // If nonzero, then the entry has checksum
    DWORD  EntryCount;
    DWORD  TagCount;
    USHORT FlagByteSize;
    USHORT BasePriority;

    size_t HeaderLength;                            // Length of the on-disk header, in bytes
    size_t EntryLength;                             // Length of the on-disk entry, in bytes

} CASC_DOWNLOAD_HEADER, *PCASC_DOWNLOAD_HEADER;

typedef struct _CASC_DOWNLOAD_ENTRY
{
    BYTE EKey[MD5_HASH_SIZE];
    ULONGLONG EncodedSize;
    DWORD Checksum;
    DWORD Flags;
    BYTE Priority;
} CASC_DOWNLOAD_ENTRY, *PCASC_DOWNLOAD_ENTRY;

// Capturable tag structure for loading from DOWNLOAD manifest
typedef struct _CASC_TAG_ENTRY1
{
    const char * szTagName;                         // Tag name
    LPBYTE Bitmap;                                  // Bitmap
    size_t BitmapLength;                            // Length of the bitmap, in bytes
    size_t NameLength;                              // Length of the tag name, in bytes, not including '\0'
    size_t TagLength;                               // Length of the on-disk tag, in bytes
    DWORD TagValue;                                 // Tag value
} CASC_TAG_ENTRY1, *PCASC_TAG_ENTRY1;

// Tag structure for storing in arrays
typedef struct _CASC_TAG_ENTRY2
{
    size_t NameLength;                              // Length of the on-disk tag, in bytes
    DWORD TagValue;                                 // Tag value
    char szTagName[0x08];                           // Tag string. This member can be longer than declared. Aligned to 8 bytes.
} CASC_TAG_ENTRY2, *PCASC_TAG_ENTRY2;

typedef struct _CASC_INSTALL_HEADER
{
    USHORT Magic;                                   // FILE_MAGIC_DOWNLOAD ('DL')
    BYTE   Version;                                 // Version
    BYTE   EKeyLength;                              // Length of the EKey
    DWORD  EntryCount;
    DWORD  TagCount;

    size_t HeaderLength;                            // Length of the on-disk header, in bytes
} CASC_INSTALL_HEADER, *PCASC_INSTALL_HEADER;

typedef struct _CASC_FILE_FRAME
{
    CONTENT_KEY FrameHash;                          // MD5 hash of the file frame
    ULONGLONG StartOffset;                          // Starting offset of the file span
    ULONGLONG EndOffset;                            // Ending offset of the file span
    ULONGLONG DataFileOffset;                       // Offset in the data file (data.###)
    DWORD EncodedSize;                              // Encoded size of the frame
    DWORD ContentSize;                              // Content size of the frame
} CASC_FILE_FRAME, *PCASC_FILE_FRAME;

typedef struct _CASC_FILE_SPAN
{
    ULONGLONG StartOffset;                          // Starting offset of the file span
    ULONGLONG EndOffset;                            // Ending offset of the file span
    PCASC_FILE_FRAME pFrames;
    TFileStream * pStream;                          // [Opened] stream for the file span
    DWORD ArchiveIndex;                             // Index of the archive
    DWORD ArchiveOffs;                              // Offset in the archive
    DWORD HeaderSize;                               // Size of encoded frame headers
    DWORD FrameCount;                               // Number of frames in this span

} CASC_FILE_SPAN, *PCASC_FILE_SPAN;

// Archive information for a remote file
typedef struct _CASC_ARCHIVE_INFO
{
    DWORD ArchiveIndex;                             // This is the index of the archive
    DWORD ArchiveOffs;                              // The file is within an archive at this offset
    DWORD EncodedSize;                              // The size of the file within the archive
    BYTE ArchiveKey[MD5_HASH_SIZE];                // This is the CKey of the archive

} CASC_ARCHIVE_INFO, *PCASC_ARCHIVE_INFO;

//-----------------------------------------------------------------------------
// Structures for CASC storage and CASC file

struct TCascStorage
{
    TCascStorage();
    ~TCascStorage();

    TCascStorage * AddRef();
    TCascStorage * Release();

    static TCascStorage * IsValid(HANDLE hStorage)
    {
        TCascStorage * hs = (TCascStorage *)hStorage;

        return (hs != INVALID_HANDLE_VALUE &&
                hs != NULL &&
                hs->ClassName == CASC_MAGIC_STORAGE) ? hs : NULL;
    }

    DWORD SetProductCodeName(LPCSTR szNewCodeName, size_t nLength = 0)
    {
        if(szCodeName == NULL && szNewCodeName != NULL)
        {
            // Make sure we have the length
            if(nLength == 0)
                nLength = strlen(szNewCodeName);

            // Allocate the code name buffer and copy from ANSI string
            if((szCodeName = CASC_ALLOC<TCHAR>(nLength + 1)) == NULL)
                return ERROR_NOT_ENOUGH_MEMORY;
            CascStrCopy(szCodeName, nLength + 1, szNewCodeName, nLength);
        }
        return ERROR_SUCCESS;
    }

    // Class recognizer. Has constant value of 'CASCSTOR' (CASC_MAGIC_STORAGE)
    ULONGLONG ClassName;

    // Class members
    PCASC_OPEN_STORAGE_ARGS pArgs;                  // Open storage arguments. Only valid during opening the storage
    CASC_LOCK StorageLock;                          // Lock for multi-threaded operations

    LPCTSTR szIndexFormat;                          // Format of the index file name
    LPTSTR  szCodeName;                             // On local storage, this select a product in a multi-product storage. For online storage, this selects a product
    LPTSTR  szRootPath;                             // Path where the build file is
    LPTSTR  szDataPath;                             // The directory where data files are
    LPTSTR  szIndexPath;                            // The directory where index files are
    LPTSTR  szFilesPath;                            // The directory where raw files are
    LPTSTR  szConfigPath;                           // The directory with configs
    LPTSTR  szMainFile;                             // Main storage file (".build.info", ".build.db", "versions")
    LPTSTR  szCdnHostUrl;                           // URL of the ribbit/http server where to download the "versions" and "cdns" files
    LPTSTR  szCdnServers;                           // List of CDN servers, separated by space
    LPTSTR  szCdnPath;                              // Remote CDN sub path for the product
    LPSTR   szRegion;                               // Product region. Only when "versions" is used as storage root file
    LPSTR   szBuildKey;                             // Product build key, aka MD5 of the build file
    DWORD dwDefaultLocale;                          // Mask of installed localles
    DWORD dwBuildNumber;                            // Product build number
    DWORD dwRefCount;                               // Number of references
    DWORD dwFeatures;                               // List of CASC features. See CASC_FEATURE_XXX

    CBLD_TYPE BuildFileType;                        // Type of the build file

    CASC_BLOB CdnConfigKey;                         // Currently selected CDN config file. Points to "config\%02X\%02X\%s
    CASC_BLOB CdnBuildKey;                          // Currently selected CDN build file. Points to "config\%02X\%02X\%s

    CASC_BLOB ArchiveGroup;                         // Key array of the "archive-group"
    CASC_BLOB ArchivesKey;                          // Key array of the "archives"
    CASC_BLOB PatchArchivesKey;                     // Key array of the "patch-archives"
    CASC_BLOB PatchArchivesGroup;                   // Key array of the "patch-archive-group"
    CASC_BLOB BuildFiles;                           // List of supported build files

    TFileStream * DataFiles[CASC_MAX_DATA_FILES];   // Array of open data files
    CASC_INDEX IndexFiles[CASC_INDEX_COUNT];        // Array of found index files
    CASC_MAP IndexEKeyMap;

    CASC_CKEY_ENTRY EncodingCKey;                   // Information about ENCODING file
    CASC_CKEY_ENTRY DownloadCKey;                   // Information about DOWNLOAD file
    CASC_CKEY_ENTRY InstallCKey;                    // Information about INSTALL file
    CASC_CKEY_ENTRY PatchFile;                      // Information about PATCH file
    CASC_CKEY_ENTRY RootFile;                       // Information about ROOT file
    CASC_CKEY_ENTRY SizeFile;                       // Information about SIZE file
    CASC_CKEY_ENTRY VfsRoot;                        // The main VFS root file
    CASC_ARRAY VfsRootList;                         // List of CASC_EKEY_ENTRY for each TVFS sub-root

    TRootHandler * pRootHandler;                    // Common handler for various ROOT file formats
    CASC_ARRAY IndexArray;                          // Array of CASC_EKEY_ENTRY, loaded from online indexes
    CASC_ARRAY CKeyArray;                           // Array of CASC_CKEY_ENTRY, loaded from ENCODING file
    CASC_ARRAY TagsArray;                           // Array of CASC_DOWNLOAD_TAG2
    CASC_MAP IndexMap;                              // Map of EKey -> IndexArray (for online archives)
    CASC_MAP CKeyMap;                               // Map of CKey -> CKeyArray
    CASC_MAP EKeyMap;                               // Map of EKey -> CKeyArray
    size_t LocalFiles;                              // Number of files that are present locally
    size_t TotalFiles;                              // Total number of files in the storage, some may not be present locally
    size_t EKeyEntries;                             // Number of CKeyEntry-ies loaded from text build file
    size_t EKeyLength;                              // EKey length from the index files
    DWORD FileOffsetBits;                           // Number of bits in the storage offset which mean data segent offset

    CASC_KEY_MAP KeyMap;                            // Growable map of encryption keys
    ULONGLONG  LastFailKeyName;                     // The value of the encryption key that recently was NOT found.
};

struct TCascFile
{
    TCascFile(TCascStorage * hs, PCASC_CKEY_ENTRY pCKeyEntry);
    ~TCascFile();

    DWORD OpenFileSpans(LPCTSTR szSpanList);
    void InitFileSpans(PCASC_FILE_SPAN pSpans, DWORD dwSpanCount);
    void InitCacheStrategy();

    static TCascFile * IsValid(HANDLE hFile)
    {
        TCascFile * hf = (TCascFile *)hFile;

        return (hf != INVALID_HANDLE_VALUE &&
                hf != NULL &&
                hf->ClassName == CASC_MAGIC_FILE &&
                hf->pCKeyEntry != NULL) ? hf : NULL;
    }

    // Class recognizer. Has constant value of 'CASCFILE' (CASC_MAGIC_FILE)
    ULONGLONG ClassName;

    // Class members
    TCascStorage * hs;                              // Pointer to storage structure

    PCASC_CKEY_ENTRY pCKeyEntry;                    // Pointer to the first CKey entry. Each entry describes one file span
    PCASC_FILE_SPAN pFileSpan;                      // Pointer to the first file span entry
    ULONGLONG ContentSize;                          // Content size. This is the summed content size of all file spans
    ULONGLONG EncodedSize;                          // Encoded size. This is the summed encoded size of all file spans
    ULONGLONG FilePointer;                          // Current file pointer
    DWORD SpanCount;                                // Number of file spans. There is one CKey entry for each file span
    DWORD bVerifyIntegrity:1;                       // If true, then the data are validated more strictly when read
    DWORD bDownloadFileIf:1;                        // If true, then the data will be downloaded from the online storage if missing
    DWORD bCloseFileStream:1;                       // If true, file stream needs to be closed during CascCloseFile
    DWORD bOvercomeEncrypted:1;                     // If true, then CascReadFile will fill the part that is encrypted (and key was not found) with zeros
    DWORD bFreeCKeyEntries:1;                       // If true, dectructor will free the array of CKey entries

    ULONGLONG FileCacheStart;                       // Starting offset of the file cached area
    ULONGLONG FileCacheEnd;                         // Ending offset of the file cached area
    LPBYTE pbFileCache;                             // Pointer to file cached area
    CSTRTG CacheStrategy;                           // Caching strategy. See CSTRTG enum for more info
};

struct TCascSearch
{
    TCascSearch(TCascStorage * ahs, LPCTSTR aszListFile, const char * aszMask)
    {
        // Init the class
        ClassName = CASC_MAGIC_FIND;
        hs = (ahs != NULL) ? ahs->AddRef() : NULL;

        // Init provider-specific data
        pCache = NULL;
        nFileIndex = 0;
        nSearchState = 0;
        bListFileUsed = false;

        // Allocate mask
        szListFile = CascNewStr(aszListFile);
        szMask = CascNewStr((aszMask != NULL) ? aszMask : "*");
    }

    ~TCascSearch()
    {
        // Dereference the CASC storage
        if(hs != NULL)
            hs = hs->Release();
        ClassName = 0;

        // Free the rest of the members
        CASC_FREE(szMask);
        CASC_FREE(szListFile);
        CASC_FREE(pCache);
    }

    static TCascSearch * IsValid(HANDLE hFind)
    {
        TCascSearch * pSearch = (TCascSearch *)hFind;

        return (hFind != INVALID_HANDLE_VALUE &&
                hFind != NULL &&
                pSearch->ClassName == CASC_MAGIC_FIND &&
                pSearch->szMask != NULL) ? pSearch : NULL;
    }

    ULONGLONG ClassName;                            // Contains 'CASCFIND'

    TCascStorage * hs;                              // Pointer to the storage handle
    LPTSTR szListFile;                              // Name of the listfile
    void * pCache;                                  // Listfile cache
    char * szMask;                                  // Search mask

    // Provider-specific data
    size_t nFileIndex;                              // Root-specific search context
    DWORD nSearchState:8;                           // The current search state (0 = listfile, 1 = nameless, 2 = done)
    DWORD bListFileUsed:1;                          // TRUE: The listfile has already been loaded
};

//-----------------------------------------------------------------------------
// Common functions (CascCommon.cpp)

inline void FreeCascBlob(PCASC_BLOB pBlob)
{
    if(pBlob != NULL)
    {
        CASC_FREE(pBlob->pbData);
        pBlob->cbData = 0;
    }
}

//-----------------------------------------------------------------------------
// Text file parsing (CascFiles.cpp)

bool  InvokeProgressCallback(TCascStorage * hs, CASC_PROGRESS_MSG Message, LPCSTR szObject, DWORD CurrentValue, DWORD TotalValue);
DWORD GetFileSpanInfo(PCASC_CKEY_ENTRY pCKeyEntry, PULONGLONG PtrContentSize, PULONGLONG PtrEncodedSize = NULL);
DWORD FetchCascFile(TCascStorage * hs, CPATH_TYPE PathType, LPBYTE pbEKey, LPCTSTR szExtension, CASC_PATH<TCHAR> & LocalPath, PCASC_ARCHIVE_INFO pArchiveInfo = NULL);
DWORD CheckCascBuildFileExact(CASC_BUILD_FILE & BuildFile, LPCTSTR szLocalPath);
DWORD CheckCascBuildFileDirs(CASC_BUILD_FILE & BuildFile, LPCTSTR szLocalPath);
DWORD CheckOnlineStorage(PCASC_OPEN_STORAGE_ARGS pArgs, CASC_BUILD_FILE & BuildFile, bool bOnlineStorage);
DWORD CheckArchiveFilesDirectories(TCascStorage * hs);
DWORD CheckDataFilesDirectory(TCascStorage * hs);
DWORD LoadMainFile(TCascStorage * hs);
DWORD LoadCdnConfigFile(TCascStorage * hs);
DWORD LoadCdnBuildFile(TCascStorage * hs);

DWORD LoadInternalFileToMemory(TCascStorage * hs, PCASC_CKEY_ENTRY pCKeyEntry, CASC_BLOB & FileData);
DWORD LoadFileToMemory(LPCTSTR szFileName, CASC_BLOB & FileData);
bool OpenFileByCKeyEntry(TCascStorage * hs, PCASC_CKEY_ENTRY pCKeyEntry, DWORD dwOpenFlags, HANDLE * PtrFileHandle);
bool SetCacheStrategy(HANDLE hFile, CSTRTG CacheStrategy);

//-----------------------------------------------------------------------------
// Internal file functions

void * ProbeOutputBuffer(void * pvBuffer, size_t cbLength, size_t cbMinLength, size_t * pcbLengthNeeded);

PCASC_CKEY_ENTRY FindCKeyEntry_CKey(TCascStorage * hs, LPBYTE pbCKey, PDWORD PtrIndex = NULL);
PCASC_CKEY_ENTRY FindCKeyEntry_EKey(TCascStorage * hs, LPBYTE pbEKey, PDWORD PtrIndex = NULL);

size_t GetTagBitmapLength(LPBYTE pbFilePtr, LPBYTE pbFileEnd, DWORD EntryCount);

DWORD CascDecompress(LPBYTE pvOutBuffer, PDWORD pcbOutBuffer, LPBYTE pvInBuffer, DWORD cbInBuffer);
DWORD CascDirectCopy(LPBYTE pbOutBuffer, PDWORD pcbOutBuffer, LPBYTE pbInBuffer, DWORD cbInBuffer);

DWORD CascLoadEncryptionKeys(TCascStorage * hs);
DWORD CascDecrypt(TCascStorage * hs, LPBYTE pbOutBuffer, PDWORD pcbOutBuffer, LPBYTE pbInBuffer, DWORD cbInBuffer, DWORD dwFrameIndex);

//-----------------------------------------------------------------------------
// Support for index files

bool CopyEKeyEntry(TCascStorage * hs, PCASC_CKEY_ENTRY pCKeyEntry);

DWORD LoadIndexFiles(TCascStorage * hs);
void  FreeIndexFiles(TCascStorage * hs);

//-----------------------------------------------------------------------------
// Support for ROOT file

DWORD RootHandler_CreateMNDX(TCascStorage * hs, CASC_BLOB & RootFile);
DWORD RootHandler_CreateTVFS(TCascStorage * hs, CASC_BLOB & RootFile);
DWORD RootHandler_CreateDiablo3(TCascStorage * hs, CASC_BLOB & RootFile);
DWORD RootHandler_CreateWoW(TCascStorage * hs, CASC_BLOB & RootFile, DWORD dwLocaleMask);
DWORD RootHandler_CreateOverwatch(TCascStorage * hs, CASC_BLOB & RootFile);
DWORD RootHandler_CreateStarcraft1(TCascStorage * hs, CASC_BLOB & RootFile);
DWORD RootHandler_CreateInstall(TCascStorage * hs, CASC_BLOB & InstallFile);

//-----------------------------------------------------------------------------
// Dumpers (CascDumpData.cpp)

#ifdef CASCLIB_DEBUG
void CascDumpData(LPCSTR szFileName, const void * pvData, size_t cbData);
void CascDumpFile(HANDLE hFile, const char * szDumpFile = NULL);
void CascDumpStorage(HANDLE hStorage, const char * szDumpFile = NULL);
#endif

#endif // __CASCCOMMON_H__
