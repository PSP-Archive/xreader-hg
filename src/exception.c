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

#include <pspkernel.h>
#include <pspsdk.h>
#include <pspctrl.h>
#include <psprtc.h>
#include <pspsysmem_kernel.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "version.h"
#include "strsafe.h"
#include "kubridge.h"
#include "scene.h"

#define MAX_BACKTRACE_NUM 10

PspDebugRegBlock exception_regs;

extern SceModule module_info;
extern int _ftext;

static const char *codeTxt[32] = {
	"Interrupt", "TLB modification", "TLB load/inst fetch", "TLB store",
	"Address load/inst fetch", "Address store", "Bus error (instr)",
	"Bus error (data)", "Syscall", "Breakpoint", "Reserved instruction",
	"Coprocessor unusable", "Arithmetic overflow", "Unknown 14",
	"Unknown 15", "Unknown 16", "Unknown 17", "Unknown 18", "Unknown 19",
	"Unknown 20", "Unknown 21", "Unknown 22", "Unknown 23", "Unknown 24",
	"Unknown 25", "Unknown 26", "Unknown 27", "Unknown 28", "Unknown 29",
	"Unknown 31"
};

static const unsigned char regName[32][5] = {
	"zr", "at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

void ExceptionHandler(PspDebugRegBlock * regs)
{
	int i, found;
	SceCtrlData pad;
	PspDebugStackTrace traces[MAX_BACKTRACE_NUM];

	pspDebugScreenInit();
	pspDebugScreenSetBackColor(0x00FF0000);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenClear();
	pspDebugScreenPrintf("%-21s %s has just crashed!\n", XREADER_VERSION_LONG,
#ifdef ENABLE_LITE
						 "lite"
#else
						 ""
#endif
		);

	pspDebugScreenPrintf("%-21s: %08X\r\n", "PSP firmware version", psp_fw_version);
	pspDebugScreenPrintf("%-21s: 0%dg\r\n\n", "PSP type", psp_model + 1);
	pspDebugScreenPrintf("Exception details:\n\n");

	pspDebugScreenPrintf("Exception - %s\n", codeTxt[(regs->cause >> 2) & 31]);
	pspDebugScreenPrintf("EPC       - %08X / %s.text + %08X\n", (int) regs->epc, module_info.modname, (unsigned int) (regs->epc - (int) &_ftext));
	pspDebugScreenPrintf("Cause     - %08X\n", (int) regs->cause);
	pspDebugScreenPrintf("Status    - %08X\n", (int) regs->status);
	pspDebugScreenPrintf("BadVAddr  - %08X\n", (int) regs->badvaddr);
	for (i = 0; i < 32; i += 4)
		pspDebugScreenPrintf("%s:%08X %s:%08X %s:%08X %s:%08X\n", regName[i],
							 (int) regs->r[i], regName[i + 1],
							 (int) regs->r[i + 1], regName[i + 2], (int) regs->r[i + 2], regName[i + 3], (int) regs->r[i + 3]);

	pspDebugScreenPrintf("\n");

	found = pspDebugGetStackTrace2(regs, traces, MAX_BACKTRACE_NUM);

	pspDebugScreenPrintf("Call Trace:\n");
	for (i = 0; i < found; ++i) {
		pspDebugScreenPrintf("\t%d: caller %08X(%08X) func %08X(%08X)\n", i,
							 (unsigned int) traces[i].call_addr,
							 (unsigned int) traces[i].call_addr -
							 (unsigned int) &_ftext, (unsigned int) traces[i].func_addr, (unsigned int) traces[i].func_addr - (unsigned int) &_ftext);
	}

	sceKernelDelayThread(1000000);
	pspDebugScreenPrintf("\n\nPress O to dump information on file exception.log and quit");
	pspDebugScreenPrintf("\nPress X to restart");

	for (;;) {
		sceCtrlReadBufferPositive(&pad, 1);
		if (pad.Buttons & PSP_CTRL_CIRCLE) {
			char testo[512];
			char timestr[80];
			FILE *log = fopen("exception.log", "w");
			pspTime tm;

			if (log == NULL) {
				break;
			}

			SPRINTF_S(testo, "%-21s: %s %s\r\n", "xReader version", XREADER_VERSION_LONG,
#ifdef ENABLE_LITE
					  "lite"
#else
					  ""
#endif
				);
			fwrite(testo, 1, strlen(testo), log);
			SPRINTF_S(testo, "%-21s: %08X\r\n", "PSP firmware version", psp_fw_version);
			fwrite(testo, 1, strlen(testo), log);
			SPRINTF_S(testo, "%-21s: 0%dg\r\n", "PSP type", psp_model + 1);
			fwrite(testo, 1, strlen(testo), log);

			sceRtcGetCurrentClockLocalTime(&tm);
			SPRINTF_S(timestr, "%u-%u-%u %02u:%02u:%02u", tm.year, tm.month, tm.day, tm.hour, tm.minutes, tm.seconds);

			SPRINTF_S(testo, "%-21s: %s\r\n", "Crash time", timestr);
			fwrite(testo, 1, strlen(testo), log);

			SPRINTF_S(testo, "\r\n");
			fwrite(testo, 1, strlen(testo), log);

			SPRINTF_S(testo, "Exception details:\r\n\r\n");
			fwrite(testo, 1, strlen(testo), log);
			SPRINTF_S(testo, "Exception - %s\r\n", codeTxt[(regs->cause >> 2) & 31]);
			fwrite(testo, 1, strlen(testo), log);
			SPRINTF_S(testo, "EPC       - %08X / %s.text + %08X\r\n", (int) regs->epc, module_info.modname, (unsigned int) (regs->epc - (int) &_ftext));
			fwrite(testo, 1, strlen(testo), log);
			SPRINTF_S(testo, "Cause     - %08X\r\n", (int) regs->cause);
			fwrite(testo, 1, strlen(testo), log);
			SPRINTF_S(testo, "Status    - %08X\r\n", (int) regs->status);
			fwrite(testo, 1, strlen(testo), log);
			SPRINTF_S(testo, "BadVAddr  - %08X\r\n", (int) regs->badvaddr);
			fwrite(testo, 1, strlen(testo), log);
			for (i = 0; i < 32; i += 4) {
				SPRINTF_S(testo, "%s:%08X %s:%08X %s:%08X %s:%08X\r\n",
						  regName[i], (int) regs->r[i], regName[i + 1],
						  (int) regs->r[i + 1], regName[i + 2], (int) regs->r[i + 2], regName[i + 3], (int) regs->r[i + 3]);
				fwrite(testo, 1, strlen(testo), log);
			}
			SPRINTF_S(testo, "\r\n");
			fwrite(testo, 1, strlen(testo), log);
			SPRINTF_S(testo, "Call Trace:\r\n");
			fwrite(testo, 1, strlen(testo), log);
			for (i = 0; i < found; ++i) {
				SPRINTF_S(testo,
						  "\t%d: caller %08X(%08X) func %08X(%08X)\r\n", i,
						  (unsigned int) traces[i].call_addr,
						  (unsigned int) traces[i].call_addr -
						  (unsigned int) &_ftext, (unsigned int) traces[i].func_addr, (unsigned int) traces[i].func_addr - (unsigned int) &_ftext);
				fwrite(testo, 1, strlen(testo), log);
			}
			fclose(log);
			break;
		} else if (pad.Buttons & PSP_CTRL_CROSS) {
			break;
		}
		sceKernelDelayThread(100000);
	}
	sceKernelExitGame();
}

int initExceptionHandler(const char *path)
{
	SceKernelLMOption option;
	int args[2], fd, modid;

	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.mpidtext = PSP_MEMORY_PARTITION_KERNEL;
	option.mpiddata = PSP_MEMORY_PARTITION_KERNEL;
	option.position = 0;
	option.access = 1;

	if ((modid = sceKernelLoadModule(path, 0, &option)) >= 0) {
		args[0] = (int) ExceptionHandler;
		args[1] = (int) &exception_regs;
		sceKernelStartModule(modid, 8, args, &fd, NULL);
		return 0;
	}

	return -1;
}
