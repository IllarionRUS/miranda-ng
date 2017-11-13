/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (�) 2012-17 Miranda NG project (https://miranda-ng.org),
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

#include "stdafx.h"

#define UPDATEANIMFRAMES 20

int DetailsInit(WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK DlgProcDetails(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static MWindowList hWindowList = nullptr;
static HANDLE hDetailsInitEvent;

struct DetailsPageInit
{
	int pageCount;
	OPTIONSDIALOGPAGE *odp;
};

struct DetailsPageData
{
	DLGTEMPLATE *pTemplate;
	HINSTANCE hInst;
	DLGPROC dlgProc;
	LPARAM  dlgParam;
	HWND hwnd;
	HTREEITEM hItem;
	int changed, hLangpack;
	wchar_t *ptszTitle, *ptszTab;
};

struct DetailsData
{
	MCONTACT hContact;
	HANDLE hProtoAckEvent;
	HINSTANCE hInstIcmp;
	HFONT hBoldFont;
	int pageCount;
	int currentPage;
	DetailsPageData *opd;
	RECT rcDisplay,  rcDisplayTab;
	int updateAnimFrame;
	wchar_t szUpdating[64];
	int *infosUpdated;
};

wchar_t* getTitle(OPTIONSDIALOGPAGE *p)
{
	return (p->flags & ODPF_DONTTRANSLATE) ? p->szTitle.w : TranslateW_LP(p->szTitle.w, p->hLangpack);
}

wchar_t* getTab(OPTIONSDIALOGPAGE *p)
{
	return (p->flags & ODPF_DONTTRANSLATE) ? p->szTab.w : TranslateW_LP(p->szTab.w, p->hLangpack);
}

static int PageSortProc(OPTIONSDIALOGPAGE *item1, OPTIONSDIALOGPAGE *item2)
{
	int res;
	wchar_t *s1 = getTitle(item1), *s2 = getTitle(item2);
	if (!mir_wstrcmp(s1, TranslateT("Summary"))) return -1;
	if (!mir_wstrcmp(s2, TranslateT("Summary"))) return 1;
	if (res = mir_wstrcmp(s1, s2)) return res;

	s1 = getTab(item1), s2 = getTab(item2);
	if (s1 && !s2) return -1;
	if (!s1 && s2) return 1;
	if (!s1 && !s2) return 0;

	if (s1 && !mir_wstrcmp(s1, TranslateT("General"))) return -1;
	if (s2 && !mir_wstrcmp(s2, TranslateT("General"))) return 1;
	return mir_wstrcmp(s1, s2);
}

static INT_PTR ShowDetailsDialogCommand(WPARAM wParam, LPARAM)
{
	HWND hwnd;
	if (hwnd = WindowList_Find(hWindowList, wParam)) {
		SetForegroundWindow(hwnd);
		SetFocus(hwnd);
		return 0;
	}

	DetailsPageInit opi = { 0 };
	opi.pageCount = 0;
	opi.odp = nullptr;
	NotifyEventHooks(hDetailsInitEvent, (WPARAM)&opi, wParam);
	if (opi.pageCount == 0)
		return 0;

	qsort(opi.odp, opi.pageCount, sizeof(OPTIONSDIALOGPAGE), (int(*)(const void*, const void*))PageSortProc);

	PROPSHEETHEADER psh = { sizeof(psh) };
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
	psh.hwndParent = nullptr;
	psh.nPages = opi.pageCount;
	psh.pStartPage = nullptr;
	psh.pszCaption = (wchar_t*)wParam;	  //more abuses of structure: this is hContact
	psh.ppsp = (PROPSHEETPAGE*)opi.odp;		  //blatent misuse of the structure, but what the hell

	CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DETAILS), NULL, DlgProcDetails, (LPARAM)&psh);
	for (int i = 0; i < opi.pageCount; i++) {
		//cleanup moved to WM_DESTROY
		if (opi.odp[i].szGroup.w != nullptr)
			mir_free(opi.odp[i].szGroup.a);
		if ((DWORD_PTR)opi.odp[i].pszTemplate & 0xFFFF0000)
			mir_free((char*)opi.odp[i].pszTemplate);
	}
	mir_free(opi.odp);
	return 0;
}

static INT_PTR AddDetailsPage(WPARAM wParam, LPARAM lParam)
{
	OPTIONSDIALOGPAGE *odp = (OPTIONSDIALOGPAGE*)lParam;
	struct DetailsPageInit *opi = (struct DetailsPageInit*)wParam;

	if (odp == nullptr || opi == nullptr)
		return 1;

	opi->odp = (OPTIONSDIALOGPAGE*)mir_realloc(opi->odp, sizeof(OPTIONSDIALOGPAGE)*(opi->pageCount + 1));
	OPTIONSDIALOGPAGE *dst = opi->odp + opi->pageCount;
	memset(dst, 0, sizeof(OPTIONSDIALOGPAGE));
	dst->hInstance = odp->hInstance;
	dst->pfnDlgProc = odp->pfnDlgProc;
	dst->position = odp->position;
	dst->pszTemplate = ((DWORD_PTR)odp->pszTemplate & 0xFFFF0000) ? mir_strdup(odp->pszTemplate) : odp->pszTemplate;

	if (odp->flags & ODPF_UNICODE) {
		dst->szTitle.w = (odp->szTitle.w == nullptr) ? nullptr : mir_wstrdup(odp->szTitle.w);
		dst->szTab.w = (odp->flags & ODPF_USERINFOTAB) ? mir_wstrdup(odp->szTab.w) : nullptr;
	}
	else {
		dst->szTitle.w = mir_a2u(odp->szTitle.a);
		dst->szTab.w = (odp->flags & ODPF_USERINFOTAB) ? mir_a2u(odp->szTab.a) : nullptr;
	}

	dst->hLangpack = odp->hLangpack;
	dst->flags = odp->flags;
	dst->dwInitParam = odp->dwInitParam;
	opi->pageCount++;
	return 0;
}

static void ThemeDialogBackground(HWND hwnd)
{
	EnableThemeDialogTexture(hwnd, ETDT_ENABLETAB);
}

static void CreateDetailsTabs(HWND hwndDlg, DetailsData *dat, DetailsPageData *ppg)
{
	HWND hwndTab = GetDlgItem(hwndDlg, IDC_TABS);
	int sel = 0, pages = 0;
	TCITEM tie;
	tie.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_PARAM;
	tie.iImage = -1;
	TabCtrl_DeleteAllItems(hwndTab);
	for (int i = 0; i < dat->pageCount; i++) {
		DetailsPageData &odp = dat->opd[i];
		if (!odp.ptszTab || mir_wstrcmp(odp.ptszTitle, ppg->ptszTitle))
			continue;

		tie.pszText = TranslateW_LP(odp.ptszTab, odp.hLangpack);
		tie.lParam = i;
		TabCtrl_InsertItem(hwndTab, pages, &tie);
		if (!mir_wstrcmp(odp.ptszTab, ppg->ptszTab))
			sel = pages;
		pages++;
	}
	TabCtrl_SetCurSel(hwndTab, sel);

	LONG style = GetWindowLongPtr(hwndTab, GWL_STYLE);
	SetWindowLongPtr(hwndTab, GWL_STYLE, pages > 1 ? style | WS_TABSTOP : style & ~WS_TABSTOP);
}

static void CreateDetailsPageWindow(HWND hwndDlg, DetailsData *dat, DetailsPageData *ppg)
{
	RECT *rc = ppg->ptszTab ? &dat->rcDisplayTab : &dat->rcDisplay;
	ppg->hwnd = CreateDialogIndirectParam(ppg->hInst, ppg->pTemplate, hwndDlg, ppg->dlgProc, (LPARAM)dat->hContact);
	ThemeDialogBackground(ppg->hwnd);
	SetWindowPos(ppg->hwnd, HWND_TOP, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, 0);
	SetWindowPos(ppg->hwnd, HWND_TOP, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, 0);

	PSHNOTIFY pshn;
	pshn.hdr.code = PSN_PARAMCHANGED;
	pshn.hdr.hwndFrom = ppg->hwnd;
	pshn.hdr.idFrom = 0;
	pshn.lParam = (LPARAM)ppg->dlgParam;
	SendMessage(ppg->hwnd, WM_NOTIFY, 0, (LPARAM)&pshn);

	pshn.hdr.code = PSN_INFOCHANGED;
	pshn.hdr.hwndFrom = ppg->hwnd;
	pshn.hdr.idFrom = 0;
	pshn.lParam = (LPARAM)dat->hContact;
	SendMessage(ppg->hwnd, WM_NOTIFY, 0, (LPARAM)&pshn);
}

static int UserInfoContactDelete(WPARAM wParam, LPARAM)
{
	HWND hwnd = WindowList_Find(hWindowList, wParam);
	if (hwnd != nullptr)
		DestroyWindow(hwnd);
	return 0;
}

#define HM_PROTOACK   (WM_USER+10)
#define M_CHECKONLINE (WM_USER+11)
static INT_PTR CALLBACK DlgProcDetails(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PSHNOTIFY pshn;
	int i;

	DetailsData *dat = (DetailsData*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		Window_SetSkinIcon_IcoLib(hwndDlg, SKINICON_OTHER_USERDETAILS);
		{
			PROPSHEETHEADER *psh = (PROPSHEETHEADER*)lParam;
			dat = (DetailsData*)mir_calloc(sizeof(DetailsData));
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)dat);
			dat->hContact = (UINT_PTR)psh->pszCaption;
			dat->hProtoAckEvent = HookEventMessage(ME_PROTO_ACK, hwndDlg, HM_PROTOACK);
			WindowList_Add(hWindowList, hwndDlg, dat->hContact);

			//////////////////////////////////////////////////////////////////////
			wchar_t *name, oldTitle[256], newTitle[256];
			if (dat->hContact == NULL)
				name = TranslateT("Owner");
			else
				name = pcli->pfnGetContactDisplayName(dat->hContact, 0);

			GetWindowText(hwndDlg, oldTitle, _countof(oldTitle));
			mir_snwprintf(newTitle, oldTitle, name);
			SetWindowText(hwndDlg, newTitle);

			//////////////////////////////////////////////////////////////////////
			LOGFONT lf;
			HFONT hNormalFont = (HFONT)SendDlgItemMessage(hwndDlg, IDC_NAME, WM_GETFONT, 0, 0);
			GetObject(hNormalFont, sizeof(lf), &lf);
			lf.lfWeight = FW_BOLD;
			dat->hBoldFont = CreateFontIndirect(&lf);
			SendDlgItemMessage(hwndDlg, IDC_NAME, WM_SETFONT, (WPARAM)dat->hBoldFont, 0);

			LPTSTR ptszLastTab;
			DBVARIANT dbv;
			if (!db_get_ws(NULL, "UserInfo", "LastTab", &dbv)) {
				ptszLastTab = NEWWSTR_ALLOCA(dbv.ptszVal);
				db_free(&dbv);
			}
			else ptszLastTab = nullptr;

			dat->currentPage = 0;
			dat->pageCount = psh->nPages;
			dat->opd = (DetailsPageData*)mir_calloc(sizeof(DetailsPageData) * dat->pageCount);
			OPTIONSDIALOGPAGE *odp = (OPTIONSDIALOGPAGE*)psh->ppsp;

			//////////////////////////////////////////////////////////////////////
			HWND hwndTree = GetDlgItem(hwndDlg, IDC_PAGETREE);
			for (i = 0; i < dat->pageCount; i++) {
				DetailsPageData &p = dat->opd[i];
				p.pTemplate = (LPDLGTEMPLATE)LockResource(LoadResource(odp[i].hInstance,
					FindResourceA(odp[i].hInstance, odp[i].pszTemplate, MAKEINTRESOURCEA(5))));
				p.dlgProc = odp[i].pfnDlgProc;
				p.dlgParam = odp[i].dwInitParam;
				p.hInst = odp[i].hInstance;

				p.ptszTitle = odp[i].szTitle.w;
				p.ptszTab = odp[i].szTab.w;
				p.hLangpack = odp[i].hLangpack;

				if (i && p.ptszTab && !mir_wstrcmp(dat->opd[i - 1].ptszTitle, p.ptszTitle)) {
					p.hItem = dat->opd[i - 1].hItem;
					continue;
				}

				TVINSERTSTRUCT tvis;
				tvis.hParent = nullptr;
				tvis.hInsertAfter = TVI_LAST;
				tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
				tvis.item.lParam = (LPARAM)i;
				if (odp[i].flags & ODPF_DONTTRANSLATE)
					tvis.item.pszText = p.ptszTitle;
				else
					tvis.item.pszText = TranslateW_LP(p.ptszTitle, p.hLangpack);
				if (ptszLastTab && !mir_wstrcmp(tvis.item.pszText, ptszLastTab))
					dat->currentPage = i;
				p.hItem = TreeView_InsertItem(hwndTree, &tvis);
			}

			//////////////////////////////////////////////////////////////////////
			HWND hwndTab = GetDlgItem(hwndDlg, IDC_TABS);

			TCITEM tci;
			tci.mask = TCIF_TEXT | TCIF_IMAGE;
			tci.iImage = -1;
			tci.pszText = L"X";
			TabCtrl_InsertItem(hwndTab, 0, &tci);

			GetWindowRect(hwndTab, &dat->rcDisplayTab);
			TabCtrl_AdjustRect(hwndTab, FALSE, &dat->rcDisplayTab);

			POINT pt = { 0, 0 };
			ClientToScreen(hwndDlg, &pt);
			OffsetRect(&dat->rcDisplayTab, -pt.x, -pt.y);

			TabCtrl_DeleteAllItems(hwndTab);

			GetWindowRect(hwndTab, &dat->rcDisplay);
			TabCtrl_AdjustRect(hwndTab, FALSE, &dat->rcDisplay);

			memset(&pt, 0, sizeof(pt));
			ClientToScreen(hwndDlg, &pt);
			OffsetRect(&dat->rcDisplay, -pt.x, -pt.y);

			TreeView_SelectItem(GetDlgItem(hwndDlg, IDC_PAGETREE), dat->opd[dat->currentPage].hItem);

			//////////////////////////////////////////////////////////////////////
			dat->updateAnimFrame = 0;
			GetDlgItemText(hwndDlg, IDC_UPDATING, dat->szUpdating, _countof(dat->szUpdating));
			SendMessage(hwndDlg, M_CHECKONLINE, 0, 0);
			if (!ProtoChainSend(dat->hContact, PSS_GETINFO, SGIF_ONOPEN, 0)) {
				EnableWindow(GetDlgItem(hwndDlg, IDC_UPDATE), FALSE);
				SetTimer(hwndDlg, 1, 100, nullptr);
			}
			else ShowWindow(GetDlgItem(hwndDlg, IDC_UPDATING), SW_HIDE);

			SetFocus(GetDlgItem(hwndDlg, IDC_PAGETREE));
		}
		return TRUE;

	case WM_TIMER:
		wchar_t str[128];
		mir_snwprintf(str, L"%.*s%s%.*s", dat->updateAnimFrame % 10, L".........", dat->szUpdating, dat->updateAnimFrame % 10, L".........");
		SetDlgItemText(hwndDlg, IDC_UPDATING, str);
		if (++dat->updateAnimFrame == UPDATEANIMFRAMES)
			dat->updateAnimFrame = 0;
		break;

	case WM_CTLCOLORSTATIC:
		switch (GetDlgCtrlID((HWND)lParam)) {
		case IDC_WHITERECT:
			SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
			return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);

		case IDC_UPDATING:
			{
				COLORREF textCol = GetSysColor(COLOR_BTNTEXT);
				COLORREF bgCol = GetSysColor(COLOR_3DFACE);
				int      ratio = abs(UPDATEANIMFRAMES / 2 - dat->updateAnimFrame) * 510 / UPDATEANIMFRAMES;
				COLORREF newCol = RGB(GetRValue(bgCol) + (GetRValue(textCol) - GetRValue(bgCol))*ratio / 256,
					GetGValue(bgCol) + (GetGValue(textCol) - GetGValue(bgCol))*ratio / 256,
					GetBValue(bgCol) + (GetBValue(textCol) - GetBValue(bgCol))*ratio / 256);
				SetTextColor((HDC)wParam, newCol);
				SetBkColor((HDC)wParam, GetSysColor(COLOR_3DFACE));
			}
			return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);

		default:
			SetBkMode((HDC)wParam, TRANSPARENT);
			return (INT_PTR)GetStockObject(NULL_BRUSH);
		}
		break;

	case PSM_CHANGED:
		dat->opd[dat->currentPage].changed = 1;
		return TRUE;

	case PSM_FORCECHANGED:
		pshn.hdr.code = PSN_INFOCHANGED;
		pshn.hdr.idFrom = 0;
		pshn.lParam = (LPARAM)dat->hContact;
		for (i = 0; i < dat->pageCount; i++) {
			DetailsPageData &odp = dat->opd[i];
			pshn.hdr.hwndFrom = odp.hwnd;
			if (odp.hwnd != nullptr)
				SendMessage(odp.hwnd, WM_NOTIFY, 0, (LPARAM)&pshn);
		}
		break;

	case M_CHECKONLINE:
		if (dat->hContact != NULL) {
			char *szProto = GetContactProto(dat->hContact);
			if (szProto == nullptr)
				EnableWindow(GetDlgItem(hwndDlg, IDC_UPDATE), FALSE);
			else {
				if (CallProtoService(szProto, PS_GETSTATUS, 0, 0) < ID_STATUS_ONLINE)
					EnableWindow(GetDlgItem(hwndDlg, IDC_UPDATE), FALSE);
				else
					EnableWindow(GetDlgItem(hwndDlg, IDC_UPDATE), !IsWindowVisible(GetDlgItem(hwndDlg, IDC_UPDATING)));
			}
		}
		break;

	case HM_PROTOACK:
		{
			ACKDATA *ack = (ACKDATA*)lParam;
			if (ack->hContact == NULL && ack->type == ACKTYPE_STATUS) {
				SendMessage(hwndDlg, M_CHECKONLINE, 0, 0);
				break;
			}
			if (ack->hContact != dat->hContact || ack->type != ACKTYPE_GETINFO)
				break;

			SendMessage(hwndDlg, PSM_FORCECHANGED, 0, 0);
			/* if they're not gonna send any more ACK's don't let that mean we should crash */
			if (!ack->hProcess && !ack->lParam) {
				ShowWindow(GetDlgItem(hwndDlg, IDC_UPDATING), SW_HIDE);
				KillTimer(hwndDlg, 1);
				SendMessage(hwndDlg, M_CHECKONLINE, 0, 0);
				break;
			} //if
			if (dat->infosUpdated == nullptr)
				dat->infosUpdated = (int*)mir_calloc(sizeof(int)*(INT_PTR)ack->hProcess);
			if (ack->result == ACKRESULT_SUCCESS || ack->result == ACKRESULT_FAILED)
				dat->infosUpdated[ack->lParam] = 1;

			for (i = 0; i < (INT_PTR)ack->hProcess; i++)
				if (dat->infosUpdated[i] == 0)
					break;
			
			if (i == (INT_PTR)ack->hProcess) {
				ShowWindow(GetDlgItem(hwndDlg, IDC_UPDATING), SW_HIDE);
				KillTimer(hwndDlg, 1);
				SendMessage(hwndDlg, M_CHECKONLINE, 0, 0);
			}
		}
		break;

	case WM_NOTIFY:
		switch (wParam) {
		case IDC_TABS:
		case IDC_PAGETREE:
			switch (((LPNMHDR)lParam)->code) {
			case TCN_SELCHANGING:
			case TVN_SELCHANGING:
				if (dat->currentPage != -1 && dat->opd[dat->currentPage].hwnd != NULL) {
					pshn.hdr.code = PSN_KILLACTIVE;
					pshn.hdr.hwndFrom = dat->opd[dat->currentPage].hwnd;
					pshn.hdr.idFrom = 0;
					pshn.lParam = (LPARAM)dat->hContact;
					if (SendMessage(dat->opd[dat->currentPage].hwnd, WM_NOTIFY, 0, (LPARAM)&pshn)) {
						SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, TRUE);
						return TRUE;
					}
				}
				break;

			case TCN_SELCHANGE:
				if (dat->currentPage != -1 && dat->opd[dat->currentPage].hwnd != NULL) {
					HWND hwndTab = GetDlgItem(hwndDlg, IDC_TABS);
					ShowWindow(dat->opd[dat->currentPage].hwnd, SW_HIDE);

					TCITEM tie;
					TVITEM tvi;

					tie.mask = TCIF_PARAM;
					TabCtrl_GetItem(hwndTab, TabCtrl_GetCurSel(hwndTab), &tie);
					dat->currentPage = tie.lParam;

					tvi.hItem = TreeView_GetNextItem(GetDlgItem(hwndDlg, IDC_PAGETREE), NULL, TVGN_CARET);
					tvi.mask = TVIF_PARAM;
					tvi.lParam = dat->currentPage;
					TreeView_SetItem(GetDlgItem(hwndDlg, IDC_PAGETREE), &tvi);

					if (dat->currentPage != -1) {
						if (dat->opd[dat->currentPage].hwnd == NULL)
							CreateDetailsPageWindow(hwndDlg, dat, &dat->opd[dat->currentPage]);
						ShowWindow(dat->opd[dat->currentPage].hwnd, SW_SHOWNA);
					}
				}
				break;

			case TVN_SELCHANGED:
				if (dat->currentPage != -1 && dat->opd[dat->currentPage].hwnd != NULL)
					ShowWindow(dat->opd[dat->currentPage].hwnd, SW_HIDE);
				{
					LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
					TVITEM tvi = pnmtv->itemNew;
					dat->currentPage = tvi.lParam;

					if (dat->currentPage != -1) {
						CreateDetailsTabs(hwndDlg, dat, &dat->opd[dat->currentPage]);
						if (dat->opd[dat->currentPage].hwnd == NULL)
							CreateDetailsPageWindow(hwndDlg, dat, &dat->opd[dat->currentPage]);
						ShowWindow(dat->opd[dat->currentPage].hwnd, SW_SHOWNA);

					}
				}
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			pshn.hdr.idFrom = 0;
			pshn.hdr.code = PSN_RESET;
			pshn.lParam = (LPARAM)dat->hContact;

			for (i = 0; i < dat->pageCount; i++) {
				DetailsPageData &odp = dat->opd[i];
				if (odp.hwnd == nullptr || !odp.changed)
					continue;
				pshn.hdr.hwndFrom = odp.hwnd;
				SendMessage(odp.hwnd, WM_NOTIFY, 0, (LPARAM)&pshn);
			}
			DestroyWindow(hwndDlg);
			break;

		case IDOK:
			pshn.hdr.idFrom = 0;
			pshn.lParam = (LPARAM)dat->hContact;
			if (dat->currentPage != -1) {
				pshn.hdr.code = PSN_KILLACTIVE;
				pshn.hdr.hwndFrom = dat->opd[dat->currentPage].hwnd;
				if (SendMessage(dat->opd[dat->currentPage].hwnd, WM_NOTIFY, 0, (LPARAM)&pshn))
					break;
			}

			pshn.hdr.code = PSN_APPLY;
			for (i = 0; i < dat->pageCount; i++) {
				DetailsPageData &odp = dat->opd[i];
				if (odp.hwnd == nullptr || !odp.changed)
					continue;
				pshn.hdr.hwndFrom = odp.hwnd;
				if (SendMessage(odp.hwnd, WM_NOTIFY, 0, (LPARAM)&pshn) == PSNRET_INVALID_NOCHANGEPAGE) {
					TreeView_Select(GetDlgItem(hwndDlg, IDC_PAGETREE), odp.hItem, TVGN_CARET);
					if (dat->currentPage != -1) ShowWindow(dat->opd[dat->currentPage].hwnd, SW_HIDE);
					dat->currentPage = i;
					ShowWindow(dat->opd[dat->currentPage].hwnd, SW_SHOW);
					return 0;
				}
			}
			DestroyWindow(hwndDlg);
			break;

		case IDC_UPDATE:
			if (dat->infosUpdated != NULL) {
				mir_free(dat->infosUpdated);
				dat->infosUpdated = NULL;
			}
			if (dat->hContact != NULL) {
				if (!ProtoChainSend(dat->hContact, PSS_GETINFO, 0, 0)) {
					EnableWindow(GetDlgItem(hwndDlg, IDC_UPDATE), FALSE);
					ShowWindow(GetDlgItem(hwndDlg, IDC_UPDATING), SW_SHOW);
					SetTimer(hwndDlg, 1, 100, nullptr);
				}
			}
			break;
		}
		break;

	case WM_CLOSE:
		SendMessage(hwndDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(hwndDlg, IDOK));
		break;

	case WM_DESTROY:
		wchar_t name[128];
		TVITEM tvi;
		tvi.mask = TVIF_TEXT;
		tvi.hItem = dat->opd[dat->currentPage].hItem;
		tvi.pszText = name;
		tvi.cchTextMax = _countof(name);
		TreeView_GetItem(GetDlgItem(hwndDlg, IDC_PAGETREE), &tvi);
		db_set_ws(NULL, "UserInfo", "LastTab", name);

		Window_FreeIcon_IcoLib(hwndDlg);
		SendDlgItemMessage(hwndDlg, IDC_NAME, WM_SETFONT, SendDlgItemMessage(hwndDlg, IDC_WHITERECT, WM_GETFONT, 0, 0), 0);
		DeleteObject(dat->hBoldFont);
		WindowList_Remove(hWindowList, hwndDlg);
		UnhookEvent(dat->hProtoAckEvent);

		for (i = 0; i < dat->pageCount; i++) {
			DetailsPageData &odp = dat->opd[i];
			if (odp.hwnd != nullptr)
				DestroyWindow(odp.hwnd);
			mir_free(odp.ptszTitle);
			mir_free(odp.ptszTab);
		}

		mir_free(dat->infosUpdated);
		mir_free(dat->opd);
		mir_free(dat);
		break;
	}
	return FALSE;
}

static int ShutdownUserInfo(WPARAM, LPARAM)
{
	WindowList_Broadcast(hWindowList, WM_DESTROY, 0, 0);
	WindowList_Destroy(hWindowList);
	return 0;
}

int LoadUserInfoModule(void)
{
	CreateServiceFunction("UserInfo/AddPage", AddDetailsPage);
	CreateServiceFunction(MS_USERINFO_SHOWDIALOG, ShowDetailsDialogCommand);

	hDetailsInitEvent = CreateHookableEvent(ME_USERINFO_INITIALISE);

	HookEvent(ME_USERINFO_INITIALISE, DetailsInit);
	HookEvent(ME_DB_CONTACT_DELETED, UserInfoContactDelete);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, ShutdownUserInfo);

	CMenuItem mi;
	SET_UID(mi, 0xe8731d53, 0x95af, 0x42cf, 0xae, 0x27, 0xc7, 0xa7, 0xa, 0xbf, 0x14, 0x1c);
	mi.position = 1000050000;
	mi.hIcolibItem = Skin_GetIconHandle(SKINICON_OTHER_USERDETAILS);
	mi.name.a = LPGEN("User &details");
	mi.pszService = MS_USERINFO_SHOWDIALOG;
	Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0x42852ca4, 0x4941, 0x4219, 0x8b, 0x98, 0x33, 0x46, 0x8c, 0x32, 0xd8, 0xb8);
	mi.position = 500050000;
	mi.name.a = LPGEN("View/change my &details...");
	Menu_AddMainMenuItem(&mi);

	hWindowList = WindowList_Create();
	return 0;
}
