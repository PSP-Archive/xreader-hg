/*
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

/* vim:set ts=4 sw=4 cindent ignorecase enc=gbk: */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#endif
#ifdef PSP
#include <pspsdk.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <psptypes.h>
#include "common/utils.h"
#include "buffer.h"
#endif
#include <time.h>
#include "dbg.h"
#include "strsafe.h"

#ifdef _MSC_VER
#define vsnprintf _vsnprintf
#define snprintf  _snprintf
#endif

#define ASCII_LINELENGTH 300
#define HEXDUMP_BYTES_PER_LINE 16
#define HEXDUMP_SHORTS_PER_LINE (HEXDUMP_BYTES_PER_LINE / 2)
#define HEXDUMP_HEXSTUFF_PER_SHORT 5	/* 4 hex digits and a space */
#define HEXDUMP_HEXSTUFF_PER_LINE \
    (HEXDUMP_HEXSTUFF_PER_SHORT * HEXDUMP_SHORTS_PER_LINE)

int conv_gbk_to_current_locale(const char *gbkstr, size_t size, char **ptr,
							   int *outputsize);

static void dbg_set_handle(dbg_handle * p, void (*init) (void *),
						   dbg_func writer, void (*cleanup) (void *), void *arg)
{
	p->init = init;
	p->write = writer;
	p->arg = arg;
	p->cleanup = cleanup;
}

static int dbg_add_handle(DBG * d, void (*init) (void *), dbg_func writer,
						  void (*cleanup) (void *), void *arg)
{
	if (!d)
		return -1;
	if (d->otsize) {
		d->ot =
			(dbg_handle *) realloc(d->ot, sizeof(dbg_handle) * (d->otsize + 1));
	} else {
		d->ot = (dbg_handle *) malloc(sizeof(dbg_handle));
	}
	dbg_set_handle(d->ot + d->otsize, init, writer, cleanup, arg);
	d->otsize++;
	// Init now
	if (init)
		(*init) (arg);
	return (int) d->otsize;
}

DBG *dbg_init()
{
	DBG *d = (DBG *) malloc(sizeof(DBG));

	d->ot = 0;
	d->otsize = 0;
	d->on = 1;

	return d;
}

int dbg_close(DBG * d)
{
	size_t i;

	if (!d)
		return -1;
	for (i = 0; i < d->otsize; ++i) {
		if (d->ot[i].cleanup)
			(*d->ot[i].cleanup) (d->ot[i].arg);
	}
	free(d->ot);
	free(d);
	return 0;
}

int dbg_gethandle_count(DBG * d)
{
	if (!d)
		return -1;
	return d->otsize;
}

dbg_handle *dbg_gethandle(DBG * d, size_t index)
{
	if (!d)
		return 0;
	if (index >= d->otsize)
		return 0;
	return &d->ot[index];
}

int dbg_close_handle(DBG * d, size_t index)
{
	dbg_handle *newot = 0;

	if (!d || index >= d->otsize)
		return -1;
	newot = (dbg_handle *) malloc(sizeof(dbg_handle) * (d->otsize - 1));
	if (!newot)
		return -1;

	if (index > 0) {
		memcpy(newot, d->ot, sizeof(dbg_handle) * index);
		memcpy(newot + index, d->ot + index + 1,
			   sizeof(dbg_handle) * (d->otsize - index - 1));
	} else {
		memcpy(newot, d->ot + 1, sizeof(dbg_handle) * (d->otsize - 1));
	}

	if ((*d->ot[index].cleanup))
		(*d->ot[index].cleanup) (d->ot[index].arg);
	free(d->ot);
	d->ot = newot;
	d->otsize--;

	return d->otsize;
}

/*
int dbg_open_console(DBG *d, HANDLE console)
{
	extern void dbg_write_console(void *arg, const char* str);
	extern void dbg_close_console(void* arg);
	return dbg_add_handle(d, 0, dbg_write_console, dbg_close_console, (void*)console);
}
*/

#ifdef WIN32
void dbg_write_console(void *arg, const char *str)
{
	DWORD len;

	WriteConsole(arg, str, (DWORD) strlen(str), &len, 0);
}

void dbg_close_console(void *arg)
{
	CloseHandle((HANDLE) arg);
}
#endif

int dbg_open_dummy(DBG * d)
{
	return dbg_add_handle(d, 0, 0, 0, 0);
}

char *dbg_memorylog = 0;

#ifdef PSP
buffer *dbg_memory_buffer = 0;

void dbg_write_memorylog(void *arg, const char *str)
{
	if (!dbg_memory_buffer)
		return;
	buffer_append_string(dbg_memory_buffer, str);
}

void dbg_close_memorylog(void *arg)
{
	buffer_free(dbg_memory_buffer);
	dbg_memory_buffer = 0;
}

int dbg_open_memorylog(DBG * d)
{
	dbg_memory_buffer = buffer_init();
	if (!dbg_memory_buffer)
		return 0;
	return dbg_add_handle(d, 0, dbg_write_memorylog, dbg_close_memorylog, 0);
}

const char *dbg_get_memorylog(void)
{
	return dbg_memorylog;
}
#endif

#ifdef PSP
void dbg_write_psp(void *arg, const char *str);

int dbg_open_psp(DBG * d)
{
	return dbg_add_handle(d, 0, dbg_write_psp, 0, 0);
}

void dbg_write_psp(void *arg, const char *str)
{
	pspDebugScreenPrintf(str);
}

int dbg_open_psp_logfile(DBG * d, const char *logfile)
{
	extern void dbg_write_psp_logfile(void *arg, const char *str);
	extern void dbg_close_psp_logfile(void *arg);
	SceUID fd =
		sceIoOpen(logfile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
	if (fd < 0) {
		return -1;
	}
	return dbg_add_handle(d, 0, dbg_write_psp_logfile, dbg_close_psp_logfile,
						  (void *) fd);
}

void dbg_write_psp_logfile(void *arg, const char *str)
{
	SceUID fd = (SceUID) arg;
	int l = strlen(str);
	char *newstr = (char *) malloc(l + 2);

	strcpy_s(newstr, l + 2, str);
	if (newstr[l - 1] == '\n') {
		newstr[l - 1] = '\r';
		newstr[l] = '\n';
		newstr[l + 1] = '\0';
	}
	sceIoWrite(fd, newstr, l + 1);
	free(newstr);
}

void dbg_close_psp_logfile(void *arg)
{
	SceUID fd = (SceUID) arg;

	sceIoClose(fd);
}
#endif

int dbg_open_stream(DBG * d, FILE * stream)
{
	extern void dbg_write_stream(void *arg, const char *str);
	extern void dbg_close_stream(void *arg);

	return dbg_add_handle(d, 0, dbg_write_stream, dbg_close_stream, stream);
}

void dbg_write_stream(void *arg, const char *str)
{
	fwrite(str, strlen(str), 1, (FILE *) arg);
}

void dbg_close_stream(void *arg)
{
	fclose((FILE *) arg);
}

int dbg_open_file(DBG * d, const char *fn)
{
	FILE *fp;

	if (!d)
		return -1;
	fp = fopen(fn, "a");
	if (!fp)
		return -1;
	return dbg_open_stream(d, fp);
}

#ifdef WIN32
void dbg_close_net(void *arg)
{
	closesocket((int) arg);
}

int dbg_open_net(DBG * d, const char *hostname, int port)
{
	extern void dbg_write_net(void *arg, const char *str);
	int sock;
	struct hostent *host;
	struct sockaddr_in servaddr;
	int socklen = sizeof(struct sockaddr);

	host = gethostbyname(hostname);
	if (!host)
		return -1;
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return -1;

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(sock, (struct sockaddr *) &servaddr, socklen) < 0)
		return -1;

	return dbg_add_handle(d, 0, dbg_write_net, dbg_close_net, (void *) sock);
}

void dbg_write_net(void *arg, const char *str)
{
	if ((int) arg != INVALID_SOCKET)
		send((int) arg, str, strlen(str), 0);
}
#endif

int dbg_open_custom(DBG * d, void (*writer) (const char *))
{
	extern void dbg_write_custom(void *arg, const char *str);

	return dbg_add_handle(d, 0, dbg_write_custom, 0, writer);
}

void dbg_write_custom(void *arg, const char *str)
{
	if (arg)
		(*(void (*)(const char *)) (arg)) (str);
}

void dbg_assert(DBG * d, char *info, int test, const char *func,
				const char *file, int line)
{
	if (!test) {
		// assertion "0" failed: file "test.c", function "main", line 6
		dbg_printf_raw(d,
					   "assertion \"%s\" failed: file \"%s\", function \"%s\", line %d",
					   info, file, func, line);
	}
}

int dbg_printf(DBG * d, const char *fmt, ...)
{
	char *buf;
	va_list ap;
	int l, size;
	size_t i;
	char timestr[80];

	if (!d)
		return -1;
	if (!d->on)
		return 0;
	va_start(ap, fmt);

#ifdef PSP
	pspTime tm;

	sceRtcGetCurrentClockLocalTime(&tm);

	SPRINTF_S(timestr, "%u-%u-%u %02u:%02u:%02u", tm.year, tm.month, tm.day,
			  tm.hour, tm.minutes, tm.seconds);
#else
	time_t timep;

	time(&timep);
	struct tm *p;

	p = localtime(&timep);
	/*
	   strcpy(timestr, asctime(localtime(&timep)));
	   if(timestr[strlen(timestr)-1] == '\n')
	   timestr[strlen(timestr)-1] = '\0';
	 */
	SPRINTF_S(timestr, "%u-%u-%u %02u:%02u:%02u", 1900 + p->tm_year,
			  1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
#endif

	int timelen = strlen(timestr);

	size = DBG_BUFSIZE;
	buf = (char *) malloc(size + timelen + 2);
	strcpy_s(buf, size + timelen + 2, timestr);
	strcat_s(buf, size + timelen + 2, ": ");
	l = vsnprintf(buf + timelen + 2, size, fmt, ap);
	buf[size + timelen + 2 - 1] = '\0';
	while (strlen(buf) == size - 1) {
		size *= 2 + 16;
		buf = (char *) realloc(buf, size + timelen + 2);
		if (!buf)
			return 0;
		strcpy_s(buf, size + timelen + 2, timestr);
		strcat_s(buf, size + timelen + 2, ": ");
		l = vsnprintf(buf + timelen + 2, size, fmt, ap);
		buf[size + timelen + 2 - 1] = '\0';
	}
	strcat_s(buf, size + timelen + 2, "\n");
	va_end(ap);

	int tsize;
	char *t = NULL;

	conv_gbk_to_current_locale(buf, strlen(buf), &t, &tsize);

	if (t != NULL) {
		for (i = 0; i < d->otsize; ++i) {
			if (d->ot[i].write)
				(*d->ot[i].write) (d->ot[i].arg, t);
		}
	}

	free(t);
	free(buf);
	return l;
}

int dbg_printf_raw(DBG * d, const char *fmt, ...)
{
	char *buf;
	va_list ap;
	int l, size;
	size_t i;

	if (!d)
		return -1;
	if (!d->on)
		return 0;
	va_start(ap, fmt);
	size = DBG_BUFSIZE;
	buf = (char *) malloc(size);
	l = vsnprintf(buf, size, fmt, ap);
	buf[size - 1] = '\0';
	while (strlen(buf) == size - 1) {
		size *= 2 + 16;
		buf = (char *) realloc(buf, size);
		l = vsnprintf(buf, size, fmt, ap);
		buf[size - 1] = '\0';
	}
	strcat_s(buf, size, "\n");
	va_end(ap);

	int tsize;
	char *t = NULL;

	conv_gbk_to_current_locale(buf, strlen(buf), &t, &tsize);

	if (t != NULL) {
		for (i = 0; i < d->otsize; ++i) {
			if (d->ot[i].write)
				(*d->ot[i].write) (d->ot[i].arg, t);
		}
	}

	free(t);
	free(buf);
	return l;
}

void
hex_and_ascii_print_with_offset(DBG * d, register const char *ident,
								register const u_char * cp,
								register u_int length, register u_int oset)
{
	register u_int i;
	register int s1, s2;
	register int nshorts;
	char hexstuff[HEXDUMP_SHORTS_PER_LINE * HEXDUMP_HEXSTUFF_PER_SHORT + 1],
		*hsp;
	char asciistuff[ASCII_LINELENGTH + 1], *asp;

	nshorts = length / sizeof(u_short);
	i = 0;
	hsp = hexstuff;
	asp = asciistuff;
	while (--nshorts >= 0) {
		s1 = *cp++;
		s2 = *cp++;
		(void) snprintf(hsp, sizeof(hexstuff) - (hsp - hexstuff),
						" %02x%02x", s1, s2);
		hsp += HEXDUMP_HEXSTUFF_PER_SHORT;
		*(asp++) = (isgraph(s1) ? s1 : '.');
		*(asp++) = (isgraph(s2) ? s2 : '.');
		i++;
		if (i >= HEXDUMP_SHORTS_PER_LINE) {
			*hsp = *asp = '\0';
			(void) dbg_printf_raw(d, "%s0x%04x: %-*s  %s",
								  ident, oset, HEXDUMP_HEXSTUFF_PER_LINE,
								  hexstuff, asciistuff);
			i = 0;
			hsp = hexstuff;
			asp = asciistuff;
			oset += HEXDUMP_BYTES_PER_LINE;
		}
	}
	if (length & 1) {
		s1 = *cp++;
		(void) snprintf(hsp, sizeof(hexstuff) - (hsp - hexstuff), " %02x", s1);
		hsp += 3;
		*(asp++) = (isgraph(s1) ? s1 : '.');
		++i;
	}
	if (i > 0) {
		*hsp = *asp = '\0';
		(void) dbg_printf_raw(d, "%s0x%04x: %-*s  %s",
							  ident, oset, HEXDUMP_HEXSTUFF_PER_LINE,
							  hexstuff, asciistuff);
	}
}

static void
hex_and_ascii_print(DBG * d, register const char *ident,
					register const u_char * cp, register u_int length)
{
	hex_and_ascii_print_with_offset(d, ident, cp, length, 0);
}

static void
hex_print_with_offset(DBG * d, register const char *ident,
					  register const u_char * cp, register u_int length,
					  register u_int oset)
{
	register u_int i;
	register int s1, s2;
	register int nshorts;
	char hexstuff[HEXDUMP_SHORTS_PER_LINE * HEXDUMP_HEXSTUFF_PER_SHORT + 1],
		*hsp;

	nshorts = length / sizeof(u_short);
	i = 0;
	hsp = hexstuff;
	while (--nshorts >= 0) {
		s1 = *cp++;
		s2 = *cp++;
		(void) snprintf(hsp, sizeof(hexstuff) - (hsp - hexstuff),
						" %02x%02x", s1, s2);
		hsp += HEXDUMP_HEXSTUFF_PER_SHORT;
		i++;
		if (i >= HEXDUMP_SHORTS_PER_LINE) {
			*hsp = '\0';
			(void) dbg_printf_raw(d, "%s0x%04x: %-*s",
								  ident, oset, HEXDUMP_HEXSTUFF_PER_LINE,
								  hexstuff);
			i = 0;
			hsp = hexstuff;
			oset += HEXDUMP_BYTES_PER_LINE;
		}
	}
	if (length & 1) {
		s1 = *cp++;
		(void) snprintf(hsp, sizeof(hexstuff) - (hsp - hexstuff), " %02x", s1);
		hsp += 3;
		++i;
	}
	if (i > 0) {
		*hsp = '\0';
		(void) dbg_printf_raw(d, "%s0x%04x: %-*s",
							  ident, oset, HEXDUMP_HEXSTUFF_PER_LINE, hexstuff);
	}
}

/*
 * just for completeness
 */
static void
hex_print(DBG * d, register const char *ident, register const u_char * cp,
		  register u_int length)
{
	hex_print_with_offset(d, ident, cp, length, 0);
}

int dbg_hexdump(DBG * d, const unsigned char *data, size_t len)
{
	if (!d)
		return -1;
	if (!d->on)
		return 0;
	hex_print(d, "", data, len);
	return 0;
}

int dbg_hexdump_ascii(DBG * d, const unsigned char *data, size_t len)
{
	if (!d)
		return -1;
	if (!d->on)
		return 0;
	hex_and_ascii_print(d, "", data, len);
	return 0;
}

void dbg_switch(DBG * d, short on)
{
	d->on = on;
}

#ifdef PSP
double pspDiffTime(u64 * t1, u64 * t2)
{
	if (!t1 || !t2)
		return 0.0;
	double d = (*t1 - *t2) * 1.0 / sceRtcGetTickResolution();

	return d;
}
#endif

#ifdef WIN32
void WriteToMessageBox(const char *pszText)
{
	MessageBox(0, pszText, "", 0);
}
#endif

#ifdef TEST_DBG
int main()
{
	WSADATA data;
	int index;
	DBG *d = dbg_init();

	WSAStartup(MAKEWORD(2, 2), &data);
	dbg_open_stream(d, stderr);
	dbg_open_stream(d, stdout);
	dbg_open_dummy(d);
	index = dbg_open_net(d, "localhost", 8888);
	dbg_open_file(d, "log.txt");

	dbg_open_stream(d, stderr);

	dbg_open_custom(d, WriteToMessageBox);
	dbg_printf_raw(d, "Hello %s", "World");
	dbg_hexdump_ascii(d, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
					  strlen("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
	dbg_switch(d, 0);
	dbg_hexdump(d, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
				strlen("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
	dbg_switch(d, 1);
	dbg_hexdump_ascii(d, (const unsigned char *) d, sizeof(*d));
	dbg_close(d);
	return 0;
}
#endif
