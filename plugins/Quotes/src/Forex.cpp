// Forex.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

int hLangpack;
HINSTANCE g_hInstance = nullptr;
HANDLE g_hEventWorkThreadStop;
//int g_nStatus = ID_STATUS_OFFLINE;
bool g_bAutoUpdate = true;
HGENMENU g_hMenuEditSettings = nullptr;
HGENMENU g_hMenuOpenLogFile = nullptr;
#ifdef CHART_IMPLEMENT
HGENMENU g_hMenuChart = nullptr;
#endif
HGENMENU g_hMenuRefresh = nullptr, g_hMenuRoot = nullptr;

#define DB_STR_AUTO_UPDATE "AutoUpdate"

typedef std::vector<HANDLE> THandles;
THandles g_ahThreads;
HGENMENU g_hEnableDisableMenu;
HANDLE g_hTBButton;

LPSTR g_pszAutoUpdateCmd = "Quotes/Enable-Disable Auto Update";
LPSTR g_pszCurrencyConverter = "Quotes/CurrencyConverter";

PLUGININFOEX Global_pluginInfo =
{
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {E882056D-0D1D-4131-9A98-404CBAEA6A9C}
	{ 0xe882056d, 0xd1d, 0x4131, { 0x9a, 0x98, 0x40, 0x4c, 0xba, 0xea, 0x6a, 0x9c } }
};

void UpdateMenu(bool bAutoUpdate)
{
	if (bAutoUpdate) // to enable auto-update
		Menu_ModifyItem(g_hEnableDisableMenu, LPGENW("Auto Update Enabled"), Quotes_GetIconHandle(IDI_ICON_MAIN));
	else // to disable auto-update
		Menu_ModifyItem(g_hEnableDisableMenu, LPGENW("Auto Update Disabled"), Quotes_GetIconHandle(IDI_ICON_DISABLED));

	CallService(MS_TTB_SETBUTTONSTATE, reinterpret_cast<WPARAM>(g_hTBButton), !bAutoUpdate ? TTBST_PUSHED : 0);
}

INT_PTR QuotesMenu_RefreshAll(WPARAM, LPARAM)
{
	const CQuotesProviders::TQuotesProviders& apProviders = CModuleInfo::GetQuoteProvidersPtr()->GetProviders();
	std::for_each(apProviders.begin(), apProviders.end(), boost::bind(&IQuotesProvider::RefreshAllContacts, _1));
	return 0;
}

INT_PTR QuotesMenu_EnableDisable(WPARAM, LPARAM)
{
	g_bAutoUpdate = (g_bAutoUpdate) ? false : true;
	db_set_b(NULL, QUOTES_MODULE_NAME, DB_STR_AUTO_UPDATE, g_bAutoUpdate);

	const CModuleInfo::TQuotesProvidersPtr& pProviders = CModuleInfo::GetQuoteProvidersPtr();
	const CQuotesProviders::TQuotesProviders& rapProviders = pProviders->GetProviders();
	std::for_each(std::begin(rapProviders), std::end(rapProviders), [](const CQuotesProviders::TQuotesProviderPtr& pProvider) {
		pProvider->RefreshSettings();
		if (g_bAutoUpdate)
			pProvider->RefreshAllContacts();
	});
	UpdateMenu(g_bAutoUpdate);

	return 0;
}

void InitMenu()
{
	CMenuItem mi;
	mi.flags = CMIF_UNICODE;
	mi.root = Menu_CreateRoot(MO_MAIN, LPGENW("Quotes"), 0, Quotes_GetIconHandle(IDI_ICON_MAIN));
	Menu_ConfigureItem(mi.root, MCI_OPT_UID, "B474F556-22B6-42A1-A91E-22FE4F671388");

	SET_UID(mi, 0x9de6716, 0x3591, 0x48c4, 0x9f, 0x64, 0x1b, 0xfd, 0xc6, 0xd1, 0x34, 0x97);
	mi.name.w = LPGENW("Enable/Disable Auto Update");
	mi.position = 10100001;
	mi.hIcolibItem = Quotes_GetIconHandle(IDI_ICON_MAIN);
	mi.pszService = g_pszAutoUpdateCmd;
	g_hEnableDisableMenu = Menu_AddMainMenuItem(&mi);
	CreateServiceFunction(mi.pszService, QuotesMenu_EnableDisable);
	UpdateMenu(g_bAutoUpdate);

	SET_UID(mi, 0x91cbabf6, 0x5073, 0x4a78, 0x84, 0x8, 0x34, 0x61, 0xc1, 0x8a, 0x34, 0xd9);
	mi.name.w = LPGENW("Refresh All Quotes\\Rates");
	mi.position = 20100001;
	mi.hIcolibItem = Quotes_GetIconHandle(IDI_ICON_MAIN);
	mi.pszService = "Quotes/RefreshAll";
	Menu_AddMainMenuItem(&mi);
	CreateServiceFunction(mi.pszService, QuotesMenu_RefreshAll);

	SET_UID(mi, 0x3663409c, 0xbd36, 0x473b, 0x9b, 0x4f, 0xff, 0x80, 0xf6, 0x2c, 0xdf, 0x9b);
	mi.name.w = LPGENW("Currency Converter...");
	mi.position = 20100002;
	mi.hIcolibItem = Quotes_GetIconHandle(IDI_ICON_CURRENCY_CONVERTER);
	mi.pszService = g_pszCurrencyConverter;
	Menu_AddMainMenuItem(&mi);
	CreateServiceFunction(mi.pszService, QuotesMenu_CurrencyConverter);

	SET_UID(mi, 0x7cca4fd9, 0x903f, 0x4b7d, 0x93, 0x7a, 0x18, 0x63, 0x23, 0xd4, 0xa9, 0xa9);
	mi.name.w = LPGENW("Export All Quotes");
	mi.hIcolibItem = Quotes_GetIconHandle(IDI_ICON_EXPORT);
	mi.pszService = "Quotes/ExportAll";
	mi.position = 20100003;
	Menu_AddMainMenuItem(&mi);
	CreateServiceFunction(mi.pszService, QuotesMenu_ExportAll);

	SET_UID(mi, 0xa994d3b, 0x77c2, 0x4612, 0x8d, 0x5, 0x6a, 0xae, 0x8c, 0x21, 0xbd, 0xc9);
	mi.name.w = LPGENW("Import All Quotes");
	mi.hIcolibItem = Quotes_GetIconHandle(IDI_ICON_IMPORT);
	mi.pszService = "Quotes/ImportAll";
	mi.position = 20100004;
	Menu_AddMainMenuItem(&mi);
	CreateServiceFunction(mi.pszService, QuotesMenu_ImportAll);

	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, Quotes_PrebuildContactMenu);

	g_hMenuRoot = mi.root = Menu_CreateRoot(MO_CONTACT, _T(QUOTES_PROTOCOL_NAME), 0, Quotes_GetIconHandle(IDI_ICON_MAIN));
	Menu_ConfigureItem(mi.root, MCI_OPT_UID, "C259BE01-642C-461E-997D-0E756B2A3AD6");

	SET_UID(mi, 0xb9812194, 0x3235, 0x4e76, 0xa3, 0xa4, 0x73, 0x32, 0x96, 0x1c, 0x1c, 0xf4);
	mi.name.w = LPGENW("Refresh");
	mi.hIcolibItem = Quotes_GetIconHandle(IDI_ICON_REFRESH);
	mi.pszService = "Quotes/RefreshContact";
	g_hMenuRefresh = Menu_AddContactMenuItem(&mi, QUOTES_PROTOCOL_NAME);
	Menu_ConfigureItem(g_hMenuRefresh, MCI_OPT_EXECPARAM, INT_PTR(0));
	CreateServiceFunction(mi.pszService, QuotesMenu_RefreshContact);

	SET_UID(mi, 0x19a16fa2, 0xf370, 0x4201, 0x92, 0x9, 0x25, 0xde, 0x4e, 0x55, 0xf9, 0x1a);
	mi.name.w = LPGENW("Open Log File...");
	mi.hIcolibItem = nullptr;
	mi.pszService = "Quotes/OpenLogFile";
	g_hMenuOpenLogFile = Menu_AddContactMenuItem(&mi, QUOTES_PROTOCOL_NAME);
	Menu_ConfigureItem(g_hMenuOpenLogFile, MCI_OPT_EXECPARAM, 1);
	CreateServiceFunction(mi.pszService, QuotesMenu_OpenLogFile);

#ifdef CHART_IMPLEMENT
	SET_UID(mi, 0x65da7256, 0x43a2, 0x4857, 0xac, 0x52, 0x1c, 0xb7, 0xff, 0xd7, 0x96, 0xfa);
	mi.name.w = LPGENW("Chart...");
	mi.hIcolibItem = nullptr;
	mi.pszService = "Quotes/Chart";
	g_hMenuChart = Menu_AddContactMenuItem(&mi, QUOTES_PROTOCOL_NAME);
	CreateServiceFunction(mi.pszService, QuotesMenu_Chart);
#endif

	SET_UID(mi, 0xac5fc17, 0x5640, 0x4f81, 0xa3, 0x44, 0x8c, 0xb6, 0x9a, 0x5c, 0x98, 0xf);
	mi.name.w = LPGENW("Edit Settings...");
	mi.hIcolibItem = nullptr;
	mi.pszService = "Quotes/EditSettings";
	g_hMenuEditSettings = Menu_AddContactMenuItem(&mi, QUOTES_PROTOCOL_NAME);
#ifdef CHART_IMPLEMENT
	Menu_ConfigureItem(g_hMenuEditSettings, MCI_OPT_EXECPARAM, 3);
#else
	Menu_ConfigureItem(g_hMenuEditSettings, MCI_OPT_EXECPARAM, 2);
#endif
	CreateServiceFunction(mi.pszService, QuotesMenu_EditSettings);
}

int Quotes_OnToolbarLoaded(WPARAM, LPARAM)
{
	TTBButton ttb = {};
	ttb.name = LPGEN("Enable/Disable Quotes Auto Update");
	ttb.pszService = g_pszAutoUpdateCmd;
	ttb.pszTooltipUp = LPGEN("Quotes Auto Update Enabled");
	ttb.pszTooltipDn = LPGEN("Quotes Auto Update Disabled");
	ttb.hIconHandleUp = Quotes_GetIconHandle(IDI_ICON_MAIN);
	ttb.hIconHandleDn = Quotes_GetIconHandle(IDI_ICON_DISABLED);
	ttb.dwFlags = ((g_bAutoUpdate) ? 0 : TTBBF_PUSHED) | TTBBF_ASPUSHBUTTON | TTBBF_VISIBLE;
	g_hTBButton = TopToolbar_AddButton(&ttb);

	ttb.name = LPGEN("Currency Converter");
	ttb.pszService = g_pszCurrencyConverter;
	ttb.pszTooltipUp = LPGEN("Currency Converter");
	ttb.pszTooltipDn = LPGEN("Currency Converter");
	ttb.hIconHandleUp = Quotes_GetIconHandle(IDI_ICON_CURRENCY_CONVERTER);
	ttb.hIconHandleDn = Quotes_GetIconHandle(IDI_ICON_CURRENCY_CONVERTER);
	ttb.dwFlags = TTBBF_VISIBLE;
	TopToolbar_AddButton(&ttb);

	return 0;
}

static void WorkingThread(void *pParam)
{
	IQuotesProvider *pProvider = reinterpret_cast<IQuotesProvider*>(pParam);
	assert(pProvider);

	if (pProvider)
		pProvider->Run();
}

int QuotesEventFunc_OnModulesLoaded(WPARAM, LPARAM)
{
	CHTTPSession::Init();

	g_hEventWorkThreadStop = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	HookEvent(ME_USERINFO_INITIALISE, QuotesEventFunc_OnUserInfoInit);

	HookEvent(ME_CLIST_DOUBLECLICKED, Quotes_OnContactDoubleClick);

	HookEvent(ME_TTB_MODULELOADED, Quotes_OnToolbarLoaded);

	g_bAutoUpdate = 1 == db_get_b(NULL, QUOTES_MODULE_NAME, DB_STR_AUTO_UPDATE, 1);

	InitMenu();

	BOOL b = ::ResetEvent(g_hEventWorkThreadStop);
	assert(b && "Failed to reset event");

	const CModuleInfo::TQuotesProvidersPtr& pProviders = CModuleInfo::GetQuoteProvidersPtr();
	const CQuotesProviders::TQuotesProviders& rapProviders = pProviders->GetProviders();
	for (CQuotesProviders::TQuotesProviders::const_iterator i = rapProviders.begin(); i != rapProviders.end(); ++i) {
		const CQuotesProviders::TQuotesProviderPtr& pProvider = *i;
		g_ahThreads.push_back(mir_forkthread(WorkingThread, pProvider.get()));
	}

	return 0;
}

int QuotesEventFunc_OnContactDeleted(WPARAM wParam, LPARAM)
{
	MCONTACT hContact = MCONTACT(wParam);

	const CModuleInfo::TQuotesProvidersPtr& pProviders = CModuleInfo::GetQuoteProvidersPtr();
	CQuotesProviders::TQuotesProviderPtr pProvider = pProviders->GetContactProviderPtr(hContact);
	if (pProvider)
		pProvider->DeleteContact(hContact);
	return 0;
}

INT_PTR QuoteProtoFunc_GetStatus(WPARAM, LPARAM)
{
	return g_bAutoUpdate ? ID_STATUS_ONLINE : ID_STATUS_OFFLINE;
}

void WaitForWorkingThreads()
{
	size_t cThreads = g_ahThreads.size();
	if (cThreads > 0) {
		HANDLE* paHandles = &*(g_ahThreads.begin());
		::WaitForMultipleObjects((DWORD)cThreads, paHandles, TRUE, INFINITE);
	}
}


int QuotesEventFunc_PreShutdown(WPARAM, LPARAM)
{
	::SetEvent(g_hEventWorkThreadStop);

	CModuleInfo::GetInstance().OnMirandaShutdown();
	return 0;
}

int QuotesEventFunc_OptInitialise(WPARAM wp, LPARAM/* lp*/)
{
	const CModuleInfo::TQuotesProvidersPtr& pProviders = CModuleInfo::GetQuoteProvidersPtr();
	const CQuotesProviders::TQuotesProviders& rapProviders = pProviders->GetProviders();

	OPTIONSDIALOGPAGE odp = { 0 };
	odp.position = 910000000;
	odp.hInstance = g_hInstance;
	odp.szTitle.w = _T(QUOTES_PROTOCOL_NAME);
	odp.szGroup.w = LPGENW("Network");
	odp.flags = ODPF_USERINFOTAB | ODPF_UNICODE;

	std::for_each(rapProviders.begin(), rapProviders.end(), boost::bind(&IQuotesProvider::ShowPropertyPage, _1, wp, boost::ref(odp)));
	return 0;
}

inline int Quotes_UnhookEvent(HANDLE h)
{
	return UnhookEvent(h);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD, LPVOID)
{
	g_hInstance = hinstDLL;
	return TRUE;
}

EXTERN_C __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD)
{
	return &Global_pluginInfo;
}

EXTERN_C int __declspec(dllexport) Load(void)
{
	mir_getLP(&Global_pluginInfo);

	if (false == CModuleInfo::Verify())
		return 1;

	Quotes_IconsInit();
	Quotes_InitExtraIcons();

	PROTOCOLDESCRIPTOR pd = { 0 };
	pd.cbSize = PROTOCOLDESCRIPTOR_V3_SIZE;
	pd.szName = QUOTES_PROTOCOL_NAME;
	pd.type = PROTOTYPE_VIRTUAL;
	Proto_RegisterModule(&pd);

	CreateProtoServiceFunction(QUOTES_PROTOCOL_NAME, PS_GETSTATUS, QuoteProtoFunc_GetStatus);

	HookEvent(ME_SYSTEM_MODULESLOADED, QuotesEventFunc_OnModulesLoaded);
	HookEvent(ME_DB_CONTACT_DELETED, QuotesEventFunc_OnContactDeleted);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, QuotesEventFunc_PreShutdown);
	HookEvent(ME_OPT_INITIALISE, QuotesEventFunc_OptInitialise);

	CreateServiceFunction(MS_QUOTES_EXPORT, Quotes_Export);
	CreateServiceFunction(MS_QUOTES_IMPORT, Quotes_Import);

	return 0;
}

EXTERN_C __declspec(dllexport) int Unload(void)
{
	WaitForWorkingThreads();

	::CloseHandle(g_hEventWorkThreadStop);

	return 0;
}
