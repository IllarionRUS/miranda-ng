/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (c) 2012-18 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <winsock2.h>
#include <shlobj.h>
#include <commctrl.h>
#include <vssym32.h>

#include <stdio.h>
#include <time.h>
#include <stddef.h>
#include <process.h>
#include <io.h>
#include <limits.h>
#include <string.h>
#include <locale.h>
#include <direct.h>
#include <malloc.h>

#include <win2k.h>

#include <m_system.h>
#include <newpluginapi.h>
#include <m_utils.h>
#include <m_netlib.h>
#include <m_langpack.h>
#include <m_clist.h>
#include <m_button.h>
#include <m_protosvc.h>
#include <m_protocols.h>
#include <m_options.h>
#include <m_skin.h>
#include <m_gui.h>
#include <m_contacts.h>
#include <m_message.h>
#include <m_userinfo.h>
#include <m_findadd.h>
#include <m_idle.h>
#include <m_icolib.h>
#include <m_timezones.h>

#include "version.h"

#include "../../mir_app/src/resource.h"

#define IDLEMOD "Idle"

extern HINSTANCE hInst;

struct Settings
{
	Settings() :
		bIdleCheck(IDLEMOD, "UserIdleCheck", 0),
		bIdleMethod(IDLEMOD, "IdleMethod", 0),
		bIdleOnSaver(IDLEMOD, "IdleOnSaver", 0),
		bIdleOnFullScr(IDLEMOD, "IdleOnFullScr", 0),
		bIdleOnLock(IDLEMOD, "IdleOnLock", 0),
		bIdlePrivate(IDLEMOD, "IdlePrivate", 0),
		bIdleSoundsOff(IDLEMOD, "IdleSoundsOff", 1),
		bIdleOnTerminal(IDLEMOD, "IdleOnTerminalDisconnect", 0),
		bIdleStatusLock(IDLEMOD, "IdleStatusLock", 0),
		bAAEnable(IDLEMOD, "AAEnable", 0),
		bAAStatus(IDLEMOD, "AAStatus", 0),
		iIdleTime1st(IDLEMOD, "IdleTime1st", 10)
	{}

	CMOption<BYTE> bIdleCheck, bIdleMethod, bIdleOnSaver, bIdleOnFullScr, bIdleOnLock;
	CMOption<BYTE> bIdlePrivate, bIdleSoundsOff, bIdleOnTerminal, bIdleStatusLock;
	CMOption<BYTE> bAAEnable;
	CMOption<WORD> bAAStatus;
	CMOption<DWORD> iIdleTime1st;
};

extern Settings S;

void IdleObject_Destroy();
void IdleObject_Create();
