/*
 * This file is part of xReader.
 *
 * Copyright (C) 2008 hrimfaxi (outmatch@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

//
// C++ Interface: depdb
//
// Description: 
//
//
// Author: tyzam, <tyzam@foxmail.com>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//
/*
For general Scribus (>=1.3.2) copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Scribus 1.3.2
for which a new license (GPL+exception) is in place.
*/

#ifndef _DEPDB_H_
#define _DEPDB_H_
#include "common/datatype.h"
#include "buffer.h"
typedef unsigned int DWord;
typedef unsigned short Word;

#define RECORD_SIZE_MAX 4096
#define BUFFER_SIZE 4096
#define COUNT_BITS 3
#define DISP_BITS 11
#define DOC_CREATOR "REAd"
#define DOC_TYPE "TEXt"
#define dmDBNameLength 32

typedef struct
{
	char name[dmDBNameLength];
	Word attributes;
	Word version;
	DWord create_time;
	DWord modify_time;
	DWord backup_time;
	DWord modificationNumber;
	DWord appInfoID;
	DWord sortInfoID;
	char type[4];
	char creator[4];
	DWord id_seed;
	DWord nextRecordList;
	Word numRecords;
} __attribute__ ((packed)) pdb_header;

#define PDB_HEADER_SIZE 78
#define PDB_RECORD_HEADER_SIZE 8

typedef struct
{
	Word version;				/* 1 = plain text, 2 = compressed */
	Word reserved1;
	DWord doc_size;				/* in bytes, when uncompressed */
	Word numRecords;			/* text rec's only; = pdb_header.numRecords-1 */
	Word rec_size;				/* usually RECORD_SIZE_MAX */
	DWord reserved2;
} __attribute__ ((packed)) doc_record0;

typedef struct
{
	u8 buf[BUFFER_SIZE];
	DWord len;
	DWord position;
} __attribute__ ((packed)) pdbbuffer;

extern long read_pdb_data(const char *pdbfile, buffer ** pbuf);

#endif
