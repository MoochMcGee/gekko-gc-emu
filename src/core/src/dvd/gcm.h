/*!
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * \file    gcm.h
 * \author  Lightning
 * \date    2006-01-01
 * \brief   Interface for loading a GCM GameCube DVD image
 *
 * \section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#ifndef CORE_DVD_GCM_H_
#define CORE_DVD_GCM_H_

#include "common.h"

#define GCMFILEID	                    0x314D4347
#define SIZE_OF_GCM_HEADER				0x440
#define SIZE_OF_GCM_BANNER				0x1960

#define DVD_BANNER_FILENAME				"opening.bnr"

#define DVD_BANNER_WIDTH				96
#define DVD_BANNER_HEIGHT				32

#define DVD_BANNER_ID					'BNR1'  // JP/US
#define DVD_BANNER_ID2					'BNR2'  // EU

namespace dvd {

//info about the files in the GCM
typedef struct {
    u32		NameOffset;
    u32		DiskAddr;
    u32		FileSize;

} GCMFST;

typedef struct {
    u32		Offset;
    u32		Size;
    u32		MaxSize;
    u32		MemLocation;
} GCMFSTHeader;

/// Our data about the FST
typedef struct _GCMFileData {
    char *			Filename;
    u32				DiskAddr;
    u32				FileSize;			//size of a file
    u32				FileCount;			//file count in a directory
    u32				IsDirectory;
    _GCMFileData *	FileList;
    _GCMFileData *	Parent;
} GCMFileData;

/// Info about the pointer passed in
typedef struct _GCMFileInfo {
    u32					ID;				//should be "GCM1", GCMFILEID
    GCMFileData *		FileData;		//file data
    u32					CurPos;			//current pointer in the GCM
    _GCMFileInfo *		PrevPtr;		//prev and next used only during cleanup
    _GCMFileInfo *		NextPtr;		//of memory
} GCMFileInfo;

// JAP/USA
typedef struct {
    u32     id;		// 'BNR1'											
    u32     padding[7];
    u8      image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT];	
    u8      shortTitle[32];										
    u8      shortMaker[32];										
    u8      longTitle[64];										
    u8      longMaker[64];										
    u8      comment[128];										
} DVDBanner;

// EUR
typedef struct {
    u32     id;		// 'BNR2'
    u32     padding[7];
    u8      image[2 * DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT];

    struct
    {
        u8  shortTitle[32];
        u8  shortMaker[32];
        u8  longTitle[64];
        u8  longMaker[64];
        u8  comment[128];
    } comments[6];
} DVDBanner2;

//all filenames in the FST
extern char *FileNames;

//poitner to the FST
extern GCMFileData *	FST;

//pointers to open files
extern GCMFileInfo *	FilePtrs;
extern GCMFileInfo *	LowLevelPtr;
extern GCMFileData *	GCMCurDir;

extern char	g_current_game_name[992];

u32 AdjustFSTCounts(GCMFST *CurGCMFSTData, u32 *CurIndex, u32 LastIndex);
void ParseFSTTree(GCMFileData *FSTEntry, GCMFileData *RootFSTEntry, GCMFST **CurEntry, char *Filenames, u32 *Count, GCMFileData *ParentFST);
GCMFileData *FindFSTEntry(GCMFileData *CurEntry, char *Filename);
GCMFileData *ChangeDirEntry(GCMFileData *CurEntry, char *Filename);

} // namespace

#endif // CORE_DVD_GCM_H_