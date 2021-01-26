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

/*
 *  C Implementation: depdb
 *
 * Description: 
 *
 * Author: tyzam,<tyzam@foxmail.com>,, (C) 2008
 *
 * Copyright: See COPYING file that comes with this distribution
 *
 */

#include <pspiofilemgr.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include "depdb.h"
#include "dbg.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

bool m_littlendian;

Word swap_Word(Word r)
{
	if (m_littlendian)
		return (r >> 8) | (r << 8);
	else
		return r;
}

DWord swap_DWord(DWord r)
{
	if (m_littlendian)
		return ((r >> 24) & 0x00FF) | (r << 24) | ((r >> 8) & 0xFF00) | ((r << 8) & 0xFF0000);
	else
		return r;
}

#define GET_Word(fd,n)   { sceIoRead(fd, &n, 2); n = swap_Word ( n ); }
#define GET_DWord(fd,n)  { sceIoRead(fd, &n, 4); n = swap_DWord( n ); }
#define GET_DDWord(fd,n)  { int a; sceIoRead(fd, &n, 4); n = swap_DWord( n ); sceIoRead(fd, &a, 4);}

static inline void _zero_fill(u8 * p, int len)
{
	while (len-- > 0)
		*p++ = '\0';
}

void selectSwap()
{
	/*union { char c[2];  Word n; }  w;
	   strncpy(  w.c, "\1\2",     2 );

	   if ( w.n == 0x0201 )
	   m_littlendian = true;
	   else
	   m_littlendian = false; */
	m_littlendian = true;
}

DWord decompress_huge(buffer * m_buf, buffer ** puzbuf)
{
	DWord i, j;
	u8 c;
	int di, n;
	unsigned int temp_c;
	char *pbuf = m_buf->ptr;
	char *puz = (*puzbuf)->ptr;

	for (i = j = 0; i < m_buf->used;) {

		c = pbuf[i++];

		if (c >= 1 && c <= 8)
			while (c--)
				puz[j++] = pbuf[i++];

		else if (c > 0x7F) {
			temp_c = c;
			temp_c = (temp_c << 8);
			temp_c = temp_c + (u8) pbuf[i++];
			di = (temp_c & 0x3FFF) >> COUNT_BITS;
			n = (temp_c & ((1 << COUNT_BITS) - 1)) + 3;
			for (; n--; ++j)
				puz[j] = puz[j - di];
			temp_c = 0;
		} else
			puz[j++] = c;
	}
	if (i < m_buf->used) {
		m_buf->used = m_buf->used - i + 2;
		memcpy(m_buf->ptr, m_buf->ptr + i - 2, m_buf->used);
	} else
		m_buf->used = 0;
	(*puzbuf)->used = j;
	printf("unzip ret:%d\n", j);
	return j;
}

long read_pdb_data(const char *pdbfile, buffer ** pbuf)
{
	size_t ret = -1;
	SceUID fd = -1;
	SceIoStat sta;
	bool bCompressed = false;
	buffer *pzbuf = NULL;
	DWord *p_records_offset = NULL;

	if (!pdbfile || !pbuf || !(*pbuf) || !(*pbuf)->ptr)
		return ret;

	selectSwap();
	do {
		pdb_header m_header;
		DWord file_size, offset, cur_size;
		doc_record0 m_rec0;
		int num_records;
		int i = 0;

		if (sceIoGetstat(pdbfile, &sta) < 0) {
			//printf("stat file:%s error:%s\n",pdbfile,strerror(errno));
			break;
		}
		if ((fd = sceIoOpen(pdbfile, PSP_O_RDONLY, 0777)) < 0) {
			printf("Could not open file :%s\n", pdbfile);
			break;
		}
		if (sceIoRead(fd, &m_header, PDB_HEADER_SIZE) < 0) {
			//dbg_printf(d, "%s read umd file chunk error,ret:%d!",__func__,p->used);
			break;
		}
		if (strncmp(m_header.type, DOC_TYPE, sizeof(m_header.type)) || strncmp(m_header.creator, DOC_CREATOR, sizeof(m_header.creator))) {
			printf("type:%s,creator:%s,This file is not recognized as a PDB document\n", m_header.type, m_header.creator);
			break;
		}
		// progressbar
		num_records = swap_Word(m_header.numRecords) - 1;

		printf("total %d records\n", num_records);
		GET_DDWord(fd, offset);
		//printf("offset %d\n",offset);
		p_records_offset = (DWord *) calloc(num_records, sizeof(DWord));

		for (i = 0; i < num_records; i++)
			GET_DDWord(fd, p_records_offset[i]);
		if (sceIoRead(fd, &m_rec0, sizeof(m_rec0)) < 0) {
			//dbg_printf(d, "%s read umd file chunk error,ret:%d!",__func__,p->used);
			break;
		}

		if (swap_Word(m_rec0.version) == 2)
			bCompressed = true;

		file_size = sta.st_size;
		buffer_prepare_copy(*pbuf, file_size * 2);
		//buffer_prepare_copy(*pbuf,file_size);

		if (!p_records_offset)
			break;
		pzbuf = buffer_init();
		if (!pzbuf)
			break;
		cur_size = file_size - offset;
		buffer_prepare_copy(pzbuf, cur_size * 2);
		if ((cur_size = sceIoRead(fd, pzbuf->ptr, cur_size)) < 0) {
			//dbg_printf(d, "%s read umd file chunk error,ret:%d!",__func__,p->used);
			break;
		}
		pzbuf->used = cur_size;
		if (bCompressed) {
			decompress_huge(pzbuf, pbuf);
			//printf("\n\nlast:%s\n",(*pbuf)->ptr);
			//buffer_append_string_buffer(*pbuf,puzbuf);
		} else
			buffer_append_string_buffer(*pbuf, pzbuf);
		ret = (*pbuf)->used;
	} while (false);

	printf("parse done,ret:%d!\n", ret);
	if (pzbuf)
		buffer_free(pzbuf);
	if (p_records_offset)
		free(p_records_offset);
	if (fd > -1)
		sceIoClose(fd);
	if (ret < 0)
		buffer_free(*pbuf);
	return ret;
}
