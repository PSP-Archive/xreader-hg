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

#include "config.h"

#ifdef ENABLE_USB

#include <pspkernel.h>
#include <pspusb.h>
#include <psppower.h>
#include <pspusbstor.h>
#include <kubridge.h>
#include <win.h>
#include "usb.h"
#include "conf.h"

static bool is_usb_inited = false;

extern bool usb_open(void)
{
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/chkreg.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/mgr.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/npdrm.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/semawm.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/usbstor.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/usbstormgr.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/usbstorms.prx", 0, NULL), 0, NULL, 0, NULL);
	sceKernelStartModule(kuKernelLoadModule("flash0:/kd/usbstorboot.prx", 0, NULL), 0, NULL, 0, NULL);

	if (sceUsbStart(PSP_USBBUS_DRIVERNAME, 0, 0) != 0)
		return false;

	if (sceUsbStart(PSP_USBSTOR_DRIVERNAME, 0, 0) != 0)
		return false;

	if (sceUsbstorBootSetCapacity(0x800000) != 0)
		return false;

	is_usb_inited = true;

	return true;
}

extern void usb_close(void)
{
	if (!is_usb_inited) {
		usb_open();
	}
	if (usb_isactive())
		usb_deactivate();

	sceUsbStop(PSP_USBSTOR_DRIVERNAME, 0, 0);

	sceUsbStop(PSP_USBBUS_DRIVERNAME, 0, 0);
}

static bool have_prompt = false;

extern bool usb_activate(void)
{
	if (!is_usb_inited) {
		usb_open();
	}
	if (scePowerGetBusClockFrequency() < 66 && !have_prompt) {
		win_msg("USB转输时请提高总线频率以免传输失败(本提示不再提示)", COLOR_WHITE, COLOR_WHITE, config.msgbcolor);
		have_prompt = true;
	}
	if (usb_isactive())
		return false;

	return (sceUsbActivate(0x1c8) == 0);
}

extern bool usb_deactivate(void)
{
	if (!is_usb_inited) {
		usb_open();
	}
	if (usb_isactive())
		return (sceUsbDeactivate(0x1c8) == 0);

	return false;
}

extern bool usb_isactive(void)
{
	if (!is_usb_inited) {
		usb_open();
	}

	return (sceUsbGetState() & PSP_USB_ACTIVATED);
}

extern bool usb_cableconnected(void)
{
	if (!is_usb_inited) {
		usb_open();
	}

	return (sceUsbGetState() & PSP_USB_CABLE_CONNECTED);
}

extern bool usb_connectionestablished(void)
{
	if (!is_usb_inited) {
		usb_open();
	}

	return (sceUsbGetState() & PSP_USB_CONNECTION_ESTABLISHED);
}

#endif
