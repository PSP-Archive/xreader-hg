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

#include <pspiofilemgr.h>
#include <string.h>
#include <malloc.h>
#include "unumd.h"
#include "zlib.h"
#include "dbg.h"
#include "charsets.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif

enum umd_cmd
{
	CMD_ID_VERSION = 1,
	CMD_ID_TITLE = 2,
	CMD_ID_AUTHOR = 3,
	CMD_ID_YEAR = 4,
	CMD_ID_MONTH = 5,
	CMD_ID_DAY = 6,
	CMD_ID_GENDER = 7,
	CMD_ID_PUBLISHER = 8,
	CMD_ID_VENDOR = 9,
	CMD_ID_CONTENT_ID = 0x0a,
	CMD_ID_FILE_LENGTH = 0x0b,
	CMD_ID_FIXED_LEN = 0x0c,
	CMD_ID_COMIC = 0x0e,
	CMD_ID_FIXIMG = 0x0f,
	CMD_ID_REF_CONTENT = 0x81,
	CMD_ID_COVER_PAGE = 0x82,
	CMD_ID_CHAP_OFF = 0x83,
	CMD_ID_CHAP_STR = 0x84,
	CMD_ID_PAGE_OFFSET = 0x87,
	CMD_ID_CDS_KEY = 0xf0,
	CMD_ID_LICENSE_KEY = 0xf1
};

enum umd_type
{
	UMD_TYPE_TXT = 1,
	UMD_TYPE_COMIC,
	UMD_TYPE_FIX
};

enum umd_cover
{
	COVER_TYPE_BMP = 0,
	COVER_TYPE_JPG = 1,
	COVER_TYPE_GIF = 2
};

static size_t buf_offset = 0;
static size_t umdfile_offset = 0;
static size_t umdfile_remain = 0;

p_umd_chapter umd_chapter_init()
{
	p_umd_chapter pchap = (p_umd_chapter) calloc(1, sizeof(t_umd_chapter));

	if (!pchap)
		return NULL;
	if (!(pchap->umdfile = buffer_init())) {
		free(pchap);
		return NULL;
	}
	return pchap;
}

void umd_chapter_reset(p_umd_chapter pchap)
{
	u_int i = 0;
	buffer *p = NULL;

	if (!pchap)
		return;
	if (pchap->umdfile)
		buffer_free(pchap->umdfile);

	for (i = 0; i < pchap->chapter_count; i++) {
		if (pchap->pchapters[i].name) {
			p = pchap->pchapters[i].name;
			/*if(pchap->pchapters[i].name->ptr)
			   free(pchap->pchapters[i].name->ptr);
			   free(pchap->pchapters[i].name); */
			buffer_free((buffer *) (pchap->pchapters[i].name));
		}
	}
	if (i > 0 && pchap->pchapters)
		free(pchap->pchapters);
	memset(pchap, 0, sizeof(*pchap));
}

void umd_chapter_free(p_umd_chapter pchap)
{
	u_int i = 0;

	if (!pchap)
		return;
	if (pchap->umdfile)
		buffer_free(pchap->umdfile);

	for (i = 0; i < pchap->chapter_count; i++) {
		if (pchap->pchapters[i].name)
			buffer_free(pchap->pchapters[i].name);
	}
	if (i > 0 && pchap->pchapters)
		free(pchap->pchapters);
	free(pchap);
	pchap = NULL;
}

int umd_inflate(Byte * compr, Byte * uncompr, uLong comprLen, uLong uncomprLen)
{
	int err;
	static z_stream d_stream;	/* decompression stream */
	uLong l;

	d_stream.zalloc = (alloc_func) 0;
	d_stream.zfree = (free_func) 0;
	d_stream.opaque = (voidpf) 0;

	d_stream.next_in = compr;
	d_stream.next_out = uncompr;
	d_stream.avail_in = (uInt) comprLen;
	d_stream.avail_out = (uInt) uncomprLen;

	err = inflateInit(&d_stream);
	if (err != Z_OK) {
		//CHECK_ERR(err, "inflateInit");
		return -1;
	}

	while (1) {
		err = inflate(&d_stream, Z_FINISH);
		if (err == Z_STREAM_END || err == Z_BUF_ERROR)
			break;
		if (err != Z_OK) {
			printf("inflate fail,err:%d\n", err);
			inflateEnd(&d_stream);
			return -2;
		}
	}

	l = d_stream.total_out;

	err = inflateEnd(&d_stream);
	if (err != Z_OK)
		return -3;
	//      printf("inflate():length:%d %s\n",l, (char *)uncompr);

	return l;

}

//extern DBG *d;
int read_umd_buf(SceUID fd, buffer ** pbuf)
{
	buffer *p;

	if (fd < 0 || !pbuf || !(*pbuf) || !(*pbuf)->ptr || (*pbuf)->used < 0)
		return -1;

	p = *pbuf;

	if ((p->used = sceIoRead(fd, p->ptr, p->used)) < 0) {
		dbg_printf(d, "%s read umd file chunk error,ret:%d!", __func__, p->used);
		buffer_free(*pbuf);
		return -1;
	}
	p->ptr[p->used] = 0;
	return p->used;
}

int get_chunk_buf(SceUID fd, buffer ** pbuf, char **pchunk, size_t stwant)
{
	buffer *p;
	size_t stremain;

	if (fd < 0 || !pbuf || !(*pbuf) || !(*pbuf)->ptr || !pchunk || !(*pchunk)
		|| buf_offset < 0 || umdfile_remain < 0)
		return -1;

	p = *pbuf;
	stremain = p->size - buf_offset;

	if (stremain < 0)
		return -2;
	if (stremain < stwant) {
		if (stwant - stremain > umdfile_remain)
			return -3;
		memmove(p->ptr, p->ptr + buf_offset, stremain);
		//size_t stbuf_want = (p->size - stremain > file_remain) ? file_remain : (p->size - stremain);
		if (stwant > p->size) {
			(*pbuf)->used = stremain;
			if (0 > buffer_prepare_append(*pbuf, stwant - stremain)) {
				//dbg_printf(d, "%s resize to new size:%d error!",__func__,stwant + (*pbuf)->size - stremain);
				return -4;
			}
			p = *pbuf;
			//dbg_printf(d, "%s new ptr:%p,%x%x%x!",__func__,p->ptr,*p->ptr,*(p->ptr+1),*(p->ptr+2));
		}
		if (sceIoRead(fd, p->ptr + stremain, p->size - stremain) < 0) {
			//dbg_printf(d, "%s read umd file chunk error,ret:%d!",__func__,p->used);
			buffer_free(*pbuf);
			return -1;
		}
		//p->ptr[p->used] = 0;
		p->used = p->size;
		*pchunk = p->ptr;
		buf_offset = stwant;
		umdfile_remain -= (p->size - stremain);
	} else {
		*pchunk = p->ptr + buf_offset;
		buf_offset += stwant;
	}
	umdfile_offset += stwant;
	return stwant;
}

int get_offset_chunk_buf(SceUID fd, buffer ** pbuf, char **pchunk, size_t stpre_offset, size_t stwant)
{
	buffer *p;
	size_t stremain;

	if (fd < 0 || !pbuf || !(*pbuf) || !(*pbuf)->ptr || !pchunk || !(*pchunk)
		|| buf_offset < 0 || umdfile_remain < 0)
		return -1;

	p = *pbuf;
	stremain = p->size - buf_offset;

	if (stremain < 0 || stpre_offset > umdfile_remain || stwant > umdfile_remain || (stpre_offset + stwant) > umdfile_remain)
		return -2;
	if (stremain < (stpre_offset + stwant)) {
		if (stremain < stpre_offset) {
			stremain = stpre_offset - stremain;
			while (stremain > p->size) {
				if (sceIoRead(fd, p->ptr, p->size) < 0) {
					//dbg_printf(d, "%s read umd file pre offset chunk error!",__func__);
					buffer_free(*pbuf);
					return -3;
				}
				umdfile_remain -= p->size;
				stremain -= p->size;
			}
			if (sceIoRead(fd, p->ptr, stremain) < 0) {
				//dbg_printf(d, "%s read umd file pre offset chunk error!",__func__);
				buffer_free(*pbuf);
				return -4;
			}
			umdfile_remain -= stremain;
			stremain = 0;
		} else {
			stremain -= stpre_offset;
			memmove(p->ptr, p->ptr + buf_offset + stpre_offset, stremain);
		}

		if (stwant > p->size) {
			(*pbuf)->used = stremain;
			if ((stwant > umdfile_remain)
				|| 0 > buffer_prepare_append(*pbuf, stwant)) {
				//dbg_printf(d, "%s resize to new size:%d error!",__func__,stwant + (*pbuf)->size - stremain);
				return -5;
			}
			p = *pbuf;
			//dbg_printf(d, "%s new ptr:%p,%x%x%x!",__func__,p->ptr,*p->ptr,*(p->ptr+1),*(p->ptr+2));
		}
		if (sceIoRead(fd, p->ptr + stremain, p->size - stremain) < 0) {
			//dbg_printf(d, "%s read umd file chunk error,ret:%d!",__func__,p->used);
			buffer_free(*pbuf);
			return -1;
		}
		//p->ptr[p->used] = 0;
		p->used = p->size;
		*pchunk = p->ptr;
		buf_offset = stwant;
		umdfile_remain -= (p->size - stremain);
	} else {
		buf_offset += stpre_offset;
		*pchunk = p->ptr + buf_offset;
		buf_offset += stwant;
	}
	umdfile_offset += (stpre_offset + stwant);
	return stwant;
}

int ReadSection(char **pSe, buffer ** buf, u_short * pType, u_short * phdType)
{
	struct UMDHeaderData *pHead;

	if (!pSe || !(*pSe) || !phdType)
		return -1;

	pHead = (struct UMDHeaderData *) (*pSe);

	if (!pHead || pHead->Length < 5) {
		//cout << "invalid length:" << pHead->Length << endl;
		return -2;
	}
	*phdType = pHead->hdType;
	if (0x01 == *phdType)
		memcpy(pType, *pSe + 5, 1);
	else
		memcpy(pType, *pSe + 5, 1);

	/*  str_cpy(strIn,p + 5,pHead->Length - 5);
	   str_clr(strOut,(pHead->Length -5) * 2);

	   if(hdType > 1 && hdType < 0x0b)
	   printencode("ucs2","utf-8",strIn.pbuf,strIn.stUsedLen,strOut.pbuf,strOut.stUsedLen);

	   switch(hdType)
	   {
	   case 0x01:
	   printf("book type:%d\n", *(Byte*)(p + 5));
	   break;
	   case 0x02:
	   printf("flag:%d,title(%d):%s\n", pHead->Flag,pHead->Length,strOut.pbuf);
	   break;
	   case 0x03:
	   printf("flag:%d,author(%d):%s\n", pHead->Flag,pHead->Length,strOut.pbuf);
	   break;
	   case 0x04:
	   printf("flag:%d,year(%d):%s\n", pHead->Flag,pHead->Length,strOut.pbuf);
	   break;
	   case 0x05:
	   printf("flag:%d,month(%d):%s\n", pHead->Flag,pHead->Length,strOut.pbuf);
	   break;
	   case 0x06:
	   printf("flag:%d,day(%d):%s\n", pHead->Flag,pHead->Length,strOut.pbuf);
	   break;
	   case 0x07:
	   printf("flag:%d,bookkind(%d):%s\n", pHead->Flag,pHead->Length,strOut.pbuf);
	   break;
	   case 0x08:
	   printf("flag:%d,publisher(%d):%s\n", pHead->Flag,pHead->Length,strOut.pbuf);
	   break;
	   case 0x09:
	   printf("flag:%d,vendor(%d):%s\n", pHead->Flag,pHead->Length,strOut.pbuf);
	   break;
	   case 0x0b:
	   printf("umd content length:%d\n", *(int*)strIn.pbuf);
	   break;
	   case 0x0e://??
	   printf("umd img type:%d\n", *(int*)strIn.pbuf);
	   break;
	   case 0x0f:
	   printf("umd img type:%d\n", *(int*)strIn.pbuf);
	   break;
	   case 0x83:
	   printf("umd chapter offset random key:%d\n", *(int*)strIn.pbuf);
	   break;
	   case 0x84:
	   printf("umd chapter title random key:%d\n", *(int*)strIn.pbuf);
	   break;
	   default:
	   //  printf("index:0x%02x,flag:0x%x,data length:%d,data:%s\n", pHead->Type,pHead->Flag,pHead->Length - 5,strOut.pbuf);
	   break;
	   }
	 */
	*pSe += pHead->Length;
	return pHead->Length;
}

int ReadAdditionalSession(char **pSe, buffer ** buf, p_umd_chapter * pchapter, const u_short hdType, bool b_get_chapter)
{
	u_int ulTotalLength = 0;
	struct UMDHeaderDataEx *pHead;
	u_int i = 0;

	if (!pSe || !(*pSe))
		return -1;
	if (b_get_chapter && (!pchapter || !(*pchapter)))
		return -2;

	pHead = (struct UMDHeaderDataEx *) (*pSe);

	if (!pHead || pHead->Length < 9) {
		//cout << "invalid additional length:" << pHead->Length << endl;
		return -2;
	}

	pHead->Length -= 9;

	switch (hdType) {
		case 14:
		case 15:
			{
				/*if((14 == hdType && 2 != (*pchapter)->umd_mode) || (15 == hdType && 3 != (*pchapter)->umd_mode) || *(*pSe) != '$')
				   {
				   //dbg_printf(d,"%s umd mode:%d not fit for hdType %d or not start from $\n",__func__,(*pchapter)->umd_mode,hdType);
				   break;
				   } */
				buffer *pRaw = (buffer *) calloc(1, sizeof(*pRaw));

				if (pRaw == NULL)
					return -1;
				while (*(*pSe) == '$') {
					struct UMDHeaderDataEx *pEx = (struct UMDHeaderDataEx *) (*pSe);
					u_int ZipLength = pEx->Length;
					u_int len;

					if (ZipLength < 9) {
						//dbg_printf(d,"%s zipLength %d  < 9",__func__,ZipLength);
						break;
					}

					(*pSe) += 9;
					len = ZipLength - 9;
					buffer_append_memory(*buf, *pSe, len);
					(*pSe) += len;
					ulTotalLength += len;

					if (**pSe == '#') {
						struct UMDHeaderData *ps = (struct UMDHeaderData *) (*pSe);
						if (ps->hdType == 10)
							(*pSe) += ps->Length;
						else if (ps->hdType == 0x81) {
							buffer_free(pRaw);
							return 1;
						}
					}
				}
				buffer_free(pRaw);
				break;
			}

			/*  case 130:
			   break;
			   case 0x81:
			   pHead->Length /= 4;
			   //cout << "total " << pHead->Length << " blocks" << endl;
			   for(i = 0;i < pHead->Length;i++)
			   {
			   //cout << "block " << i << " is " << *(u_int*)(*pSe) << endl;
			   (*pSe) += 4;
			   }
			   break; */
		case 0x83:
			(*pSe) += 9;
			pHead->Length /= 4;
			//cout << "total " << pHead->Length << " chapters" << endl;
			if (b_get_chapter) {
				struct t_chapter *p;

				(*pchapter)->chapter_count = pHead->Length;
				(*pchapter)->pchapters = (struct t_chapter *) calloc(pHead->Length, sizeof(struct t_chapter));
				if (!(*pchapter)->pchapters)
					return -4;
				p = (*pchapter)->pchapters;

				for (i = 0; i < pHead->Length; i++) {
					//cout << "chapter " << i << " is " << *(u_int*)(*pSe) << endl;
					if (!(p[i].name = buffer_init()))
						return -4;
					memcpy(&p[i].chunk_pos, *pSe, 4);
					(*pSe) += 4;
				}
			} else {
				for (i = 0; i < pHead->Length; i++) {
					//cout << "chapter " << i << " is " << *(u_int*)(*pSe) << endl;
					(*pSe) += 4;
				}
			}
			break;
		case 0x84:
			{
				(*pSe) += 9;
				if (b_get_chapter) {
					//cout << "total " << pHead->Length << " chapters" << endl;
					size_t stlen = (*pchapter)->chapter_count;
					size_t chapter_len = 0;
					struct t_chapter *p = (*pchapter)->pchapters;

					for (i = 0; i < stlen; i++) {
						chapter_len = *(u8 *) (*pSe);
						(*pSe) += 1;
						buffer_copy_string_len(p[i].name, *pSe, (chapter_len > 256) ? 256 : chapter_len);
						//dbg_printf(d,"%dth chapter name:%s\n",i,p[i].name->ptr);
						(*pSe) += chapter_len;
					}
					return stlen;
				} else {
					size_t stUnzipSize = 0;
					Byte *p = NULL;
					buffer *pRaw;

					(*pSe) = (*pSe) + pHead->Length;
					if (pchapter && 1 != (*pchapter)->umd_mode)
						break;
					if (*(*pSe) != '$') {
						//printlog("ms0:/PSP/GAME371/xReader/xlog.txt","not $\n");
						break;
					}

					pRaw = buffer_init();

					if (pRaw == NULL)
						return -1;
					while (*(*pSe) == '$') {
						struct UMDHeaderDataEx *pEx = (struct UMDHeaderDataEx *) (*pSe);
						u_int len;
						u_int outlen;
						u_int ZipLength = pEx->Length;

						if (ZipLength < 9) {
							//printlog("ms0:/PSP/GAME371/xReader/xlog.txt","\nzipLength  < 9");
							//printlog("ms0:/PSP/GAME371/xReader/xlog.txt",&ZipLength);
							break;
						}

						(*pSe) += 9;
						len = ZipLength - 9;
						outlen = len * 2;

						buffer_prepare_copy(pRaw, outlen);
						stUnzipSize = umd_inflate((Byte *) ((*pSe)), (Byte *) pRaw->ptr, len, outlen);
						if (stUnzipSize < 0) {
							buffer_free(pRaw);
							return -1;
						}
						p = (Byte *) pRaw->ptr;
						buffer_append_memory(*buf, pRaw->ptr, stUnzipSize);
						(*pSe) += len;
						ulTotalLength += outlen;
						if (**pSe == '#') {
							struct UMDHeaderData *ps = (struct UMDHeaderData *) (*pSe);
							if (ps->hdType == 0xf1 || ps->hdType == 10)
								(*pSe) += ps->Length;
							else if (ps->hdType == 0x81) {
								buffer_free(pRaw);
								return 1;
							}
						}
					}
					buffer_free(pRaw);
				}
			}
			break;
		default:
			(*pSe) += 9;
			(*pSe) += pHead->Length;
			break;
	}
	return 1;
}

int umd_readdata(char **pf, buffer ** buf)
{
	u_short hdType = 0;
	u_short hdUmdMode = 0;
	int nRet = -1;
	p_umd_chapter *pchapter = NULL;

	if (!pf || !(*pf) || !buf || !(*buf))
		return -1;

	while (**pf == '#') {
		if ((nRet = ReadSection(pf, buf, &hdUmdMode, &hdType)) < 0)
			break;
		while (**pf == '$') {
			if ((nRet = ReadAdditionalSession(pf, buf, pchapter, hdType, false)) < 0)
				return nRet;
		}
	}
	return nRet;
}

int locate_umd_img1(const char *umdfile, size_t file_offset, SceUID * pfd)
{
	int ret = -1;
	char buf[9] = { 0 };
	size_t stread = 0;

	if (!umdfile || !pfd || file_offset < 0)
		return -1;
	*pfd = -1;
	do {
		struct UMDHeaderDataEx *pEx;

		if ((*pfd = sceIoOpen(umdfile, PSP_O_RDONLY, 0777)) < 0) {
			return -2;
		}
		if (0 < sceIoLseek(*pfd, file_offset, SEEK_SET))
			return -3;
		if ((stread = sceIoRead(*pfd, buf, 9)) < 0) {
			dbg_printf(d, "%s read umd file head chunk error!", __func__);
			break;
		}

		pEx = (struct UMDHeaderDataEx *) &buf;

		if (!pEx || pEx->Mark != '$' || pEx->Length < 9)
			break;
		return pEx->Length - 9;
	} while (false);
	if (*pfd) {
		sceIoClose(*pfd);
		*pfd = -1;
	}
	return ret;
}

int locate_umd_img(const char *umdfile, size_t file_offset, FILE ** fp)
{
	int ret = -1;
	char buf[9] = { 0 };
	size_t stread = 0;

	if (!umdfile || !fp || file_offset < 0)
		return -1;
	*fp = NULL;
	do {
		struct UMDHeaderDataEx *pEx;

		if (!(*fp = fopen(umdfile, "r")))
			return -2;
		if (0 != fseek(*fp, file_offset, SEEK_SET))
			return -3;
		if ((stread = fread(buf, 1, 9, *fp)) < 0) {
			//dbg_printf(d, "%s read umd file head chunk error!",__func__);
			break;
		}

		pEx = (struct UMDHeaderDataEx *) &buf;

		if (!pEx || pEx->Mark != '$' || pEx->Length < 9)
			break;
		return pEx->Length - 9;
	} while (false);
	if (*fp) {
		fclose(*fp);
		*fp = NULL;
	}
	return ret;
}

int read_umd_chapter_content(const char *umdfile, u_int index, p_umd_chapter pchapter
							 /*,size_t file_offset,size_t length */ ,
							 buffer ** pbuf)
{
	int ret = -1;
	SceUID fd = -1;
	SceIoStat sta;
	char buf[9] = { 0 };
	size_t stlen = 0;
	size_t stoutlen = 0;
	size_t stUnzipSize = 0;
	buffer *pzbuf = NULL;
	buffer *puzbuf = NULL;
	bool bok = false;
	struct t_chapter *pchap;
	size_t chunk_pos;
	size_t chunk_offset;
	size_t length;

	if (!umdfile || !pchapter || index + 1 > pchapter->chapter_count || !pchapter->pchapters || !(pchapter->pchapters + index) || !pbuf || !(*pbuf))
		return -1;

	pchap = pchapter->pchapters + index;
	chunk_pos = pchap->chunk_pos;
	chunk_offset = pchap->chunk_offset;
	length = pchap->length;

	do {
		char *p;
		struct UMDHeaderDataEx *pEx;

		if (sceIoGetstat(umdfile, &sta) < 0 || sta.st_size < chunk_pos) {
			return -1;
		}
		if ((fd = sceIoOpen(umdfile, PSP_O_RDONLY, 0777)) < 0) {
			return -2;
		}
		if (sceIoLseek(fd, chunk_pos, PSP_SEEK_SET) < 0) {
			break;
		}
		if ((stlen = sceIoRead(fd, buf, 9)) < 0) {
			//dbg_printf(d, "%s read umd file head chunk error!",__func__);
			break;
		}

		p = &buf[0];

		pzbuf = buffer_init();

		if (pzbuf == NULL)
			break;

		puzbuf = buffer_init();

		if (puzbuf == NULL)
			break;

		pEx = (struct UMDHeaderDataEx *) p;

		if (!pEx || pEx->Mark != '$' || pEx->Length < 9)
			break;
		stlen = pEx->Length - 9;
		stoutlen = stlen * 2;
		buf_offset = 0;
		umdfile_offset = chunk_pos + 9;
		umdfile_remain = sta.st_size - umdfile_offset;
		if (stlen < length + chunk_offset) {
			bool bfirst_chunk = true;
			size_t stHeadSize = sizeof(struct UMDHeaderData);
			size_t stHeadExSize = sizeof(struct UMDHeaderDataEx);

			//buffer_prepare_copy(pzbuf,stlen);
			buffer_prepare_copy(pzbuf, umdfile_remain);
			pzbuf->used = pzbuf->size;
			if (0 > read_umd_buf(fd, &pzbuf)) {
				//dbg_printf(d, "%s not start with 0xde9a9b89,that umd must be corrupted!",__func__);
				break;
			}
			buffer_prepare_copy(*pbuf, length);

			while (*p == '$') {
				bok = false;
				pEx = (struct UMDHeaderDataEx *) p;
				stlen = pEx->Length;
				if (stlen < 9) {
					////dbg_printf(d,"%s zipLength %d  < 9",__func__,ZipLength);
					break;
				}
				//pchap[i++].pos = umdfile_offset - 9;
				stlen -= 9;
				stoutlen = stlen * 2;
				buffer_prepare_copy(puzbuf, stoutlen);
				if (0 > get_chunk_buf(fd, &pzbuf, &p, stlen))
					break;
				stUnzipSize = umd_inflate((Byte *) p, (Byte *) puzbuf->ptr, stlen, stoutlen);
				if (stUnzipSize < 0 || stUnzipSize > stoutlen) {
					printf("stUnzipSize %d not in limit size:%d", stUnzipSize, stoutlen);
					break;
				}
				if (bfirst_chunk) {
					if (length <= stUnzipSize - chunk_offset) {
						buffer_append_memory(*pbuf, puzbuf->ptr + chunk_offset, length);
						ret = length;
						bok = true;
						break;
					}
					buffer_append_memory(*pbuf, puzbuf->ptr + chunk_offset, stUnzipSize - chunk_offset);
					length -= (stUnzipSize - chunk_offset);
					chunk_offset = 0;
					bfirst_chunk = false;
				} else if (length > stUnzipSize) {
					buffer_append_memory(*pbuf, puzbuf->ptr, stUnzipSize);
					length -= stUnzipSize;
				} else {
					buffer_append_memory(*pbuf, puzbuf->ptr + chunk_offset, stUnzipSize - chunk_offset);
					ret = pchap->length;
					bok = true;
					break;
				}

				if (*(pzbuf->ptr + buf_offset) == '#') {
					bok = true;
					while (*(pzbuf->ptr + buf_offset) == '#') {
						struct UMDHeaderData *pHead;

						if (0 > get_chunk_buf(fd, &pzbuf, &p, stHeadSize)) {
							bok = false;
							break;
						}

						pHead = (struct UMDHeaderData *) p;

						if (pHead->hdType == 0xf1 || pHead->hdType == 10) {
							if (*(pzbuf->ptr + buf_offset + pHead->Length - stHeadSize) == '#') {
								if (0 > get_chunk_buf(fd, &pzbuf, &p, pHead->Length - stHeadSize)) {
									bok = false;
									break;
								}
							} else {
								if (0 > get_offset_chunk_buf(fd, &pzbuf, &p, pHead->Length - stHeadSize, stHeadExSize))
									bok = false;
								break;
							}
						} else if (pHead->hdType == 0x81) {
							printf("you should never come here fileoffset:%d\n", umdfile_offset);
							if (pzbuf)
								buffer_free(pzbuf);
							if (puzbuf)
								buffer_free(puzbuf);
							sceIoClose(fd);
							return 0;
						}
					}
					if (!bok)
						break;
				} else {
					if (0 > get_chunk_buf(fd, &pzbuf, &p, stHeadExSize))
						break;
				}
			}
		} else {
			buffer_prepare_copy(pzbuf, stlen);
			pzbuf->used = pzbuf->size;
			if (0 > read_umd_buf(fd, &pzbuf)) {
				//dbg_printf(d, "%s not start with 0xde9a9b89,that umd must be corrupted!",__func__);
				break;
			}
			if (0 > get_chunk_buf(fd, &pzbuf, &p, stlen))
				break;
			buffer_prepare_copy(puzbuf, stoutlen);
			stUnzipSize = umd_inflate((Byte *) p, (Byte *) puzbuf->ptr, stlen, stoutlen);
			if (stUnzipSize < 0 || stUnzipSize > stoutlen) {
				printf("stUnzipSize %d not in limit size:%d", stUnzipSize, stoutlen);
				break;
			}
			buffer_copy_string_len(*pbuf, puzbuf->ptr + chunk_offset, length);
			ret = length;
		}
	} while (false);
	if (pzbuf)
		buffer_free(pzbuf);
	if (puzbuf)
		buffer_free(puzbuf);
	if (fd)
		sceIoClose(fd);
	return ret;
}

int parse_umd_chapters(const char *umdfile, p_umd_chapter * pchapter)
{
	//int ret = -1;
	SceUID fd = -1;
	SceIoStat sta;
	buffer *pRaw = NULL;
	buffer *pzbuf = NULL;

	do {
		size_t stHeadSize = sizeof(struct UMDHeaderData);
		size_t stHeadExSize = sizeof(struct UMDHeaderDataEx);
		char *p;
		int Reserve;
		struct UMDHeaderData *pHead;
		struct UMDHeaderDataEx *pHeadEx;
		u_int i;
		size_t stlen;

		if (!umdfile || !pchapter || !(*pchapter))
			break;
		if (sceIoGetstat(umdfile, &sta) < 0) {
			return -1;
		}
		if ((fd = sceIoOpen(umdfile, PSP_O_RDONLY, 0777)) < 0) {
			return -2;
		}

		pRaw = buffer_init();

		if (pRaw == NULL)
			break;

		buffer_prepare_copy(pRaw, (10240 > sta.st_size) ? sta.st_size : 10240);

		pRaw->used = pRaw->size;
		if (0 > read_umd_buf(fd, &pRaw)) {
			//dbg_printf(d, "%s not start with 0xde9a9b89,that umd must be corrupted!",__func__);
			break;
		}

		p = pRaw->ptr;
		Reserve = *(int *) p;

		if (Reserve != (int) 0xde9a9b89 || *(p + sizeof(int)) != '#') {
			//dbg_printf(d, "%s not start with 0xde9a9b89,or not start with '#',that umd must be corrupted!",__func__);
			buffer_free(pRaw);
			break;
		}
		(*pchapter)->filesize = sta.st_size;
		buf_offset = sizeof(int);
		umdfile_offset = buf_offset;
		umdfile_remain = sta.st_size - buf_offset;
		pHead = NULL;
		pHeadEx = NULL;

		////dbg_printf(d,"%s start to parse\n",__func__);
		while (umdfile_remain > 0) {
			if (0 > get_chunk_buf(fd, &pRaw, &p, stHeadSize))
				break;
			pHead = (struct UMDHeaderData *) p;
			if (pHead->Mark != '#' || pHead->Length < stHeadSize) {
				//dbg_printf(d, "%s head corrupted,flag:%c,length:%d!",__func__,pHead->Flag,pHead->Length);
				break;
			}
			if (pHead->hdType == 0x0e || pHead->hdType == 0x0f) {
				size_t stChapterCount;
				bool bchapterless;	//no chapters listed
				struct t_chapter *pchap;

				/*if((14 == hdType && 2 != (*pchapter)->umd_mode) || (15 == hdType && 3 != (*pchapter)->umd_mode) || *(*pSe) != '$')
				   {
				   ////dbg_printf(d,"%s umd mode:%d not fit for hdType %d or not start from $\n",__func__,(*pchapter)->umd_mode,hdType);
				   break;
				   } */
				if (0 > get_chunk_buf(fd, &pRaw, &p, pHead->Length - stHeadSize))
					break;
				memcpy(&(*pchapter)->umd_mode, p, 1);
				//cout << "umd mode:" << (*pchapter)->umd_mode << endl;
				if (0 > get_chunk_buf(fd, &pRaw, &p, stHeadExSize))
					break;
				stChapterCount = (*pchapter)->chapter_count;
				bchapterless = false;	//no chapters listed
				pchap = (*pchapter)->pchapters;

				if (1 > stChapterCount) {
					bchapterless = true;
					stChapterCount = 16;
					(*pchapter)->pchapters = (struct t_chapter *) calloc(stChapterCount, sizeof(struct t_chapter));
					if (!(*pchapter)->pchapters)
						break;
					pchap = (*pchapter)->pchapters;
					for (i = 0; i < stChapterCount; i++) {
						if (!(pchap[i].name = buffer_init()))
							return -4;
						pchap[i].length = 1;
					}
				} else if (!(*pchapter)->pchapters)
					break;
				if (*p == '$')
					(*pchapter)->content_pos = umdfile_offset - 9;
				i = 0;
				while (*p == '$') {
					if (bchapterless) {
						if (i >= stChapterCount) {
							stChapterCount += 16;
							(*pchapter)->pchapters = (struct t_chapter *)
								realloc((*pchapter)->pchapters, stChapterCount * sizeof(struct t_chapter));
							if (!(*pchapter)->pchapters)
								break;
							pchap = (*pchapter)->pchapters;
						}
						if (!(pchap[i].name = buffer_init()))
							break;
						buffer_prepare_copy(pchap[i].name, 7);
						memcpy(pchap[i].name->ptr, "i\0m\0g\0", 6);
						pchap[i].name->ptr[6] = '\0';
						pchap[i].name->used = 7;
					} else if (i >= stChapterCount) {
						//dbg_printf(d,"%s get img chapters %d more than list %d",__func__,i,stChapterCount);
						buffer_free(pRaw);
						sceIoClose(fd);
						return i + 1;
					}
					pHeadEx = (struct UMDHeaderDataEx *) p;
					stlen = pHeadEx->Length;
					if (stlen < 9) {
						////dbg_printf(d,"%s zipLength %d  < 9",__func__,ZipLength);
						break;
					}
					printf("img:%d pos:%d\n", i, umdfile_offset - 9);
					pchap[i++].chunk_pos = umdfile_offset - 9;
					stlen -= 9;
					if (0 > get_chunk_buf(fd, &pRaw, &p, stlen))
						break;
					//cout << "img:" << i++ << " len:" << stlen << endl;
					if (*(pRaw->ptr + buf_offset) == '#') {
						if (0 > get_chunk_buf(fd, &pRaw, &p, stHeadSize))
							break;
						pHead = (struct UMDHeaderData *) p;
						if (pHead->hdType == 10) {
							if (0 > get_offset_chunk_buf(fd, &pRaw, &p, pHead->Length - stHeadSize, stHeadExSize))
								break;
							//cout << "skip chunk" << endl;
						} else if (pHead->hdType == 0x81) {
							if (bchapterless && i < stChapterCount) {
								stChapterCount = i;
								(*pchapter)->pchapters = (struct t_chapter *)
									realloc((*pchapter)->pchapters, stChapterCount * sizeof(struct t_chapter));
								if (!(*pchapter)->pchapters)
									break;
								(*pchapter)->chapter_count = i;
							}
							//cout << "parse done" << endl;
							buffer_free(pRaw);
							sceIoClose(fd);
							return i + 1;
						}
					} else {
						if (0 > get_chunk_buf(fd, &pRaw, &p, stHeadExSize))
							break;
					}
				}
				buffer_free(pRaw);
				sceIoClose(fd);
				return -1;
			} else if (pHead->hdType == 0x83) {
				size_t stChapterCount;

				if (0 > get_offset_chunk_buf(fd, &pRaw, &p, pHead->Length - stHeadSize, stHeadExSize))
					break;
				pHeadEx = (struct UMDHeaderDataEx *) p;
				if (pHeadEx->Length < stHeadExSize)
					break;
				stChapterCount = (pHeadEx->Length - stHeadExSize) / 4;

				if (0 < stChapterCount) {
					struct t_chapter *pchap;

					if (0 > get_chunk_buf(fd, &pRaw, &p, pHeadEx->Length - stHeadExSize))
						break;
					(*pchapter)->chapter_count = stChapterCount;
					(*pchapter)->pchapters = (struct t_chapter *) calloc(stChapterCount, sizeof(struct t_chapter));
					if (!(*pchapter)->pchapters)
						break;

					pchap = (*pchapter)->pchapters;

					for (i = 0; i < stChapterCount; i++) {
						if (!(pchap[i].name = buffer_init()))
							return -4;
						memcpy(&pchap[i].length, p, sizeof(u_int));
						p += 4;
					}
				} else
					continue;
			} else if (pHead->hdType == 0x84) {
				struct t_chapter *pchap;
				size_t stChapterCount;
				size_t stcontent_length = 0;
				bool bok = true;
				size_t stoutlen = 0;
				size_t stUnzipSize = 0;

				if (0 > get_offset_chunk_buf(fd, &pRaw, &p, pHead->Length - stHeadSize, stHeadExSize))
					break;
				pHeadEx = (struct UMDHeaderDataEx *) p;
				if (pHeadEx->Length < stHeadExSize)
					break;
				if (0 > get_chunk_buf(fd, &pRaw, &p, pHeadEx->Length - stHeadExSize))
					break;

				pchap = (*pchapter)->pchapters;
				stChapterCount = (*pchapter)->chapter_count;

				if (0 < stChapterCount) {
					for (i = 0; i < stChapterCount; i++) {
						stlen = *(u8 *) p;
						p += 1;
						buffer_copy_string_len(pchap[i].name, p, stlen);
						////dbg_printf(d,"%dth chapter name:%s\n",i,pchap[i].name->ptr);
						p += stlen;
					}
				}

				if (0x01 != (*pchapter)->umd_type)
					continue;
				if (0 > get_chunk_buf(fd, &pRaw, &p, stHeadExSize))
					break;
				i = 0;

				if (*p == '$')
					(*pchapter)->content_pos = umdfile_offset - 9;

				pchap = (*pchapter)->pchapters;
				pzbuf = buffer_init();

				if (!pzbuf)
					break;
				while (*p == '$') {
					bok = false;
					pHeadEx = (struct UMDHeaderDataEx *) p;
					stlen = pHeadEx->Length;
					if (stlen < 9) {
						////dbg_printf(d,"%s zipLength %d  < 9",__func__,ZipLength);
						break;
					}
					//pchap[i++].pos = umdfile_offset - 9;
					stlen -= 9;
					stoutlen = stlen * 2;
					buffer_prepare_copy(pzbuf, stoutlen);
					if (0 > get_chunk_buf(fd, &pRaw, &p, stlen))
						break;
					stUnzipSize = umd_inflate((Byte *) p, (Byte *) pzbuf->ptr, stlen, stoutlen);
					if (stUnzipSize < 0 || stUnzipSize > stoutlen) {
						printf("stUnzipSize %d not in limit size:%d", stUnzipSize, stoutlen);
						break;
					}
					//stcontent_length += stlen;
					//for(;i < stChapterCount;i++)
					while (i < stChapterCount && pchap[i].length <= stcontent_length + stUnzipSize) {
						pchap[i].chunk_pos = umdfile_offset - 9 - stlen;
						pchap[i].chunk_offset = pchap[i].length - stcontent_length;
						if (i > 0)
							pchap[i - 1].length = pchap[i].length - pchap[i - 1].length;
						printf("%dth pos:%d offset:%d,cur len:%d\n", i, pchap[i].chunk_pos, pchap[i].length, stcontent_length);
						++i;
					}
					stcontent_length += stUnzipSize;
					if (i >= stChapterCount) {
						pchap[stChapterCount - 1].length = (*pchapter)->filesize - pchap[stChapterCount - 1].length;
						printf("x total %d pos fileoffset:%d\n", i, umdfile_offset);
						if (pzbuf)
							buffer_free(pzbuf);
						buffer_free(pRaw);
						sceIoClose(fd);
						return stChapterCount + 1;
					}
					if (*(pRaw->ptr + buf_offset) == '#') {
						bok = true;
						while (*(pRaw->ptr + buf_offset) == '#') {
							if (0 > get_chunk_buf(fd, &pRaw, &p, stHeadSize)) {
								bok = false;
								break;
							}
							pHead = (struct UMDHeaderData *) p;
							if (pHead->hdType == 0xf1 || pHead->hdType == 10) {
								if (*(pRaw->ptr + buf_offset + pHead->Length - stHeadSize) == '#') {
									if (0 > get_chunk_buf(fd, &pRaw, &p, pHead->Length - stHeadSize)) {
										bok = false;
										break;
									}
								} else {
									if (0 > get_offset_chunk_buf(fd, &pRaw, &p, pHead->Length - stHeadSize, stHeadExSize))
										bok = false;
									break;
								}
							} else if (pHead->hdType == 0x81) {
								printf("total %d pos fileoffset:%d\n", i, umdfile_offset);
								if (pzbuf)
									buffer_free(pzbuf);
								buffer_free(pRaw);
								sceIoClose(fd);
								return stChapterCount + 1;
							}
						}
						if (!bok)
							break;
					} else {
						if (0 > get_chunk_buf(fd, &pRaw, &p, stHeadExSize))
							break;
					}
				}

			} else {
				if (0 > get_chunk_buf(fd, &pRaw, &p, pHead->Length - stHeadSize))
					break;
				if (0x01 == pHead->hdType)
					memcpy(&(*pchapter)->umd_type, p, 1);
				else if (0x0b == pHead->hdType)
					memcpy(&(*pchapter)->filesize, p, 4);
			}
			if (*p == '$') {
				//stbuf_offset += stHeadSize;
				//pRaw->used = 4;
				if (0 > get_chunk_buf(fd, &pRaw, &p, 4))
					break;
				//pHeadEx->Length = *(u_int*)p;
				//memcpy((char*)&pHeadEx->Length ,p,sizeof(u_int));
				memcpy((char *) &i, p, sizeof(u_int));
				if (9 > i || 0 > get_chunk_buf(fd, &pRaw, &p, i - stHeadExSize))
					break;
			}
		}

	} while (false);
	if (pRaw)
		buffer_free(pRaw);
	if (fd > 0)
		sceIoClose(fd);
	return 1;
}
