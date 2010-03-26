/*
 * evolution-windows-sens.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#define INITGUID
#include <windows.h>
#include <eventsys.h>
#include <sensevts.h>
#include <rpc.h>

#include <shell/e-shell.h>
#include <e-util/e-extension.h>

#define NUM_ELEMENTS(x) (sizeof((x)) / sizeof((x)[0]))

/* 4E14FB9F-2E22-11D1-9964-00C04FBBB345 */
DEFINE_GUID(IID_IEventSystem, 0x4E14FB9F, 0x2E22, 0x11D1, 0x99, 0x64, 0x00, 0xC0, 0x4F, 0xBB, 0xB3, 0x45);

/* 4A6B0E15-2E38-11D1-9965-00C04FBBB345 */
DEFINE_GUID(IID_IEventSubscription, 0x4A6B0E15, 0x2E38, 0x11D1, 0x99, 0x65, 0x00, 0xC0, 0x4F, 0xBB, 0xB3, 0x45);

/* d597bab1-5b9f-11d1-8dd2-00aa004abd5e */
DEFINE_GUID(IID_ISensNetwork, 0xd597bab1, 0x5b9f, 0x11d1, 0x8d, 0xd2, 0x00, 0xaa, 0x00, 0x4a, 0xbd, 0x5e);

/* 4E14FBA2-2E22-11D1-9964-00C04FBBB345 */
DEFINE_GUID(CLSID_CEventSystem, 0x4E14FBA2, 0x2E22, 0x11D1, 0x99, 0x64, 0x00, 0xC0, 0x4F, 0xBB, 0xB3, 0x45);

/* 7542e960-79c7-11d1-88f9-0080c7d771bf */
DEFINE_GUID(CLSID_CEventSubscription, 0x7542e960, 0x79c7, 0x11d1, 0x88, 0xf9, 0x00, 0x80, 0xc7, 0xd7, 0x71, 0xbf);


/* Standard GObject macros */
#define E_TYPE_WINDOWS_SENS \
	(e_windows_sens_get_type ())
#define E_WINDOWS_SENS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WINDOWS_SENS, EWindowsSENS))

typedef struct _EWindowsSENS EWindowsSENS;
typedef struct _EWindowsSENSClass EWindowsSENSClass;

struct _EWindowsSENS {
	EExtension parent;
};

struct _EWindowsSENSClass {
	EExtensionClass parent_class;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_windows_sens_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EWindowsSENS, e_windows_sens, E_TYPE_EXTENSION)

static EShell *
windows_sens_get_shell (EWindowsSENS *extension)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (extension));

	return E_SHELL (extensible);
}

/* Object to receive the ISensNetwork events */

typedef struct ESensNetworkListener {
	ISensNetworkVtbl *lpVtbl;
	long ref;
	EWindowsSENS *ews_ptr;
} ESensNetworkListener;

static void e_sens_network_listener_init(ESensNetworkListener**,EWindowsSENS*);

/* Functions to implement ISensNetwork interface */

static HRESULT WINAPI e_sens_network_listener_queryinterface (ISensNetwork*,REFIID,void**);
static ULONG WINAPI e_sens_network_listener_addref (ISensNetwork*);
static ULONG WINAPI e_sens_network_listener_release (ISensNetwork*);
static HRESULT WINAPI e_sens_network_listener_gettypeinfocount (ISensNetwork*, UINT*);
static HRESULT WINAPI e_sens_network_listener_gettypeinfo (ISensNetwork*,UINT,LCID,ITypeInfo**);
static HRESULT WINAPI e_sens_network_listener_getidsofnames (ISensNetwork*,REFIID,LPOLESTR*,UINT,LCID, DISPID*);
static HRESULT WINAPI e_sens_network_listener_invoke (ISensNetwork*,DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*);
static HRESULT WINAPI e_sens_network_listener_connectionmade (ISensNetwork*,BSTR,ULONG,LPSENS_QOCINFO);
static HRESULT WINAPI e_sens_network_listener_connectionmadenoqocinfo (ISensNetwork*,BSTR,ULONG);
static HRESULT WINAPI e_sens_network_listener_connectionlost (ISensNetwork*,BSTR,ULONG);
static HRESULT WINAPI e_sens_network_listener_destinationreachable (ISensNetwork*,BSTR,BSTR,ULONG,LPSENS_QOCINFO);
static HRESULT WINAPI e_sens_network_listener_destinationreachablenoqocinfo (ISensNetwork*,BSTR,BSTR,ULONG);

/* Initializing the VTable of our ESensNetworkListener object */

static ISensNetworkVtbl ESensNetworkListenerVtbl = {
	e_sens_network_listener_queryinterface,
	e_sens_network_listener_addref,
	e_sens_network_listener_release,
	e_sens_network_listener_gettypeinfocount,
	e_sens_network_listener_gettypeinfo,
	e_sens_network_listener_getidsofnames,
	e_sens_network_listener_invoke,
	e_sens_network_listener_connectionmade,
	e_sens_network_listener_connectionmadenoqocinfo,
	e_sens_network_listener_connectionlost,
	e_sens_network_listener_destinationreachable,
	e_sens_network_listener_destinationreachablenoqocinfo
};


static HRESULT WINAPI
e_sens_network_listener_queryinterface (ISensNetwork *This,
                                        REFIID        iid,
                                        void        **ppv)
{
	if (IsEqualIID (iid, &IID_IUnknown) || IsEqualIID (iid, &IID_IDispatch) || IsEqualIID (iid, &IID_ISensNetwork)) {
		*ppv = This;
		((LPUNKNOWN)*ppv)->lpVtbl->AddRef((LPUNKNOWN)*ppv);
		return S_OK;
	}
	*ppv = NULL;
	return E_NOINTERFACE;
}

static ULONG WINAPI
e_sens_network_listener_addref (ISensNetwork *This)
{
	ESensNetworkListener *esnl_ptr=(ESensNetworkListener*)This;
	return InterlockedIncrement(&(esnl_ptr->ref));
}

static ULONG WINAPI
e_sens_network_listener_release (ISensNetwork *This)
{
	ESensNetworkListener *esnl_ptr=(ESensNetworkListener*)This;
	ULONG tmp = InterlockedDecrement(&(esnl_ptr->ref));
	return tmp;
}

static HRESULT WINAPI
e_sens_network_listener_gettypeinfocount (ISensNetwork *This,
                                          UINT         *pctinfo)
{
	return E_NOTIMPL;
}

static HRESULT WINAPI
e_sens_network_listener_gettypeinfo (ISensNetwork *This,
                                     UINT          iTInfo,
                                     LCID          lcid,
                                     ITypeInfo   **ppTInfo)
{
	return E_NOTIMPL;
}

static HRESULT WINAPI
e_sens_network_listener_getidsofnames (ISensNetwork *This,
                                       REFIID        riid,
                                       LPOLESTR     *rgszNames,
                                       UINT          cNames,
                                       LCID          lcid,
                                       DISPID       *rgDispId)
{
	return E_NOTIMPL;
}

static HRESULT WINAPI
e_sens_network_listener_invoke (ISensNetwork *This,
								DISPID        dispIdMember,
								REFIID        riid,
								LCID          lcid,
								WORD          wFlags,
								DISPPARAMS   *pDispParams,
								VARIANT      *pVarResult,
								EXCEPINFO    *pExcepInfo,
								UINT         *puArgErr)
{
	return E_NOTIMPL;
}

static HRESULT WINAPI
e_sens_network_listener_connectionmade (ISensNetwork  *This,
                                        BSTR           bstrConnection,
                                        ULONG          ulType,
                                        LPSENS_QOCINFO lpQOCInfo)
{
	if (ulType) {
		ESensNetworkListener *esnl_ptr=(ESensNetworkListener*)This;
		EShell *shell = windows_sens_get_shell (esnl_ptr->ews_ptr);
		/* Wait a second so that the connection stabilizes */
		g_usleep(G_USEC_PER_SEC);
		e_shell_set_network_available (shell, TRUE);
	}
	return S_OK;
}

static HRESULT WINAPI
e_sens_network_listener_connectionmadenoqocinfo (ISensNetwork *This, 
                                                 BSTR          bstrConnection,
                                                 ULONG         ulType)
{
	//Always followed by ConnectionMade
	return S_OK;
}

static HRESULT WINAPI
e_sens_network_listener_connectionlost (ISensNetwork *This,
                                        BSTR          bstrConnection,
                                        ULONG         ulType)
{
	if (ulType) {
		ESensNetworkListener *esnl_ptr=(ESensNetworkListener*)This;
		EShell *shell = windows_sens_get_shell (esnl_ptr->ews_ptr);
		e_shell_set_network_available (shell, FALSE);
	}
	return S_OK;
}

static HRESULT WINAPI
e_sens_network_listener_destinationreachable (ISensNetwork  *This,
                                              BSTR           bstrDestination,
                                              BSTR           bstrConnection,
                                              ULONG          ulType,
                                              LPSENS_QOCINFO lpQOCInfo)
{
	if (ulType) {
		ESensNetworkListener *esnl_ptr=(ESensNetworkListener*)This;
		EShell *shell = windows_sens_get_shell (esnl_ptr->ews_ptr);
		/* Wait a second so that the connection stabilizes */
		g_usleep(G_USEC_PER_SEC);
		e_shell_set_network_available (shell, TRUE);
	}
	return S_OK;
}

static HRESULT WINAPI
e_sens_network_listener_destinationreachablenoqocinfo (ISensNetwork *This,
                                                       BSTR          bstrDestination,
                                                       BSTR          bstrConnection,
                                                       ULONG         ulType)
{
	return S_OK;
}

static void
e_sens_network_listener_init(ESensNetworkListener **esnl_ptr,
                             EWindowsSENS          *ews)
{
	(*esnl_ptr) = g_new0(ESensNetworkListener,1);
	(*esnl_ptr)->lpVtbl = &ESensNetworkListenerVtbl;
	(*esnl_ptr)->ews_ptr = ews;
	(*esnl_ptr)->ref = 1;
}


static BSTR
_mb2wchar (const char* a)
{
	static WCHAR b[64];
	MultiByteToWideChar (0, 0, a, -1, b, 64);
	return b;
}

static const char* add_curly_braces_to_uuid (const char* string_uuid)
{
	static char curly_braced_uuid_string[64];
	int i;
	if (!string_uuid)
		return NULL;
	lstrcpy(curly_braced_uuid_string,"{");
	i = strlen(curly_braced_uuid_string);
	lstrcat(curly_braced_uuid_string+i,string_uuid);
	i = strlen(curly_braced_uuid_string);
	lstrcat(curly_braced_uuid_string+i,"}");
	return curly_braced_uuid_string;
}	
	
static void
windows_sens_constructed (GObject *object)
{
	HRESULT res;
	static IEventSystem *pEventSystem =0;
	static IEventSubscription* pEventSubscription = 0;
	static ESensNetworkListener *pESensNetworkListener = 0;
	static const char* eventclassid="{D5978620-5B9F-11D1-8DD2-00AA004ABD5E}";
	static const char* methods[]={
		"ConnectionMade",
		"ConnectionMadeNoQOCInfo",
		"ConnectionLost",
		"DestinationReachable",
		"DestinationReachableNoQOCInfo"
	};
	static const char* names[]={
		"EWS_ConnectionMade",
		"EWS_ConnectionMadeNoQOCInfo",
		"EWS_ConnectionLost",
		"EWS_DestinationReachable",
		"EWS_DestinationReachableNoQOCInfo"
	};
	unsigned char* subids[] = { 0, 0, 0, 0, 0 };

	EWindowsSENS *extension = (E_WINDOWS_SENS (object));
	e_sens_network_listener_init(&pESensNetworkListener, extension);

	CoInitialize(0);

	res=CoCreateInstance (&CLSID_CEventSystem, 0,CLSCTX_SERVER,&IID_IEventSystem,(LPVOID*)&pEventSystem);

	if (res == S_OK && pEventSystem) {

		unsigned i;

		for (i=0; i<NUM_ELEMENTS(methods); i++) {

			res=CoCreateInstance (&CLSID_CEventSubscription, 0, CLSCTX_SERVER, &IID_IEventSubscription, (LPVOID*)&pEventSubscription);

			if (res == S_OK && pEventSubscription) {
				UUID tmp_uuid;
				UuidCreate(&tmp_uuid);
				UuidToString(&tmp_uuid, &subids[i]);
				res=pEventSubscription->lpVtbl->put_SubscriptionID (pEventSubscription, _mb2wchar (add_curly_braces_to_uuid ((char*)subids[i])));
				if (res) {
					RpcStringFree (&subids[i]);
					break;
				}
				RpcStringFree (&subids[i]);
				res=pEventSubscription->lpVtbl->put_SubscriptionName (pEventSubscription, _mb2wchar (names[i]));
				if (res)
					break;
				res=pEventSubscription->lpVtbl->put_MethodName (pEventSubscription, _mb2wchar (methods[i]));
				if (res)
					break;
				res=pEventSubscription->lpVtbl->put_EventClassID (pEventSubscription, _mb2wchar (eventclassid));
				if (res)
					break;
				res=pEventSubscription->lpVtbl->put_SubscriberInterface (pEventSubscription, (IUnknown*)pESensNetworkListener);
				if (res)
					break;
				/* Make the subscription receive the event only if the owner of the subscription
				 * is logged on to the same computer as the publisher. This makes this module
				 * work with normal user account without administrative privileges.
				 */
				res=pEventSubscription->lpVtbl->put_PerUser (pEventSubscription, TRUE);
				if (res)
					break;

				res=pEventSystem->lpVtbl->Store (pEventSystem, (BSTR)PROGID_EventSubscription, (IUnknown*)pEventSubscription);
				if (res)
					break;
				pEventSubscription->lpVtbl->Release (pEventSubscription);
				pEventSubscription=0;
			}
		}
		if (pEventSubscription)
			pEventSubscription->lpVtbl->Release(pEventSubscription);
	}
	
	/* Do not try to get initial state when we are sure we will not get system events.
	 * Like that we don't get stuck with Disconnected status if we were disconnected
	 * on start.
	 */
	if (res == S_OK) {

		typedef BOOL (WINAPI* IsNetworkAlive_t) (LPDWORD);
		BOOL alive = TRUE;
		EShell *shell = windows_sens_get_shell (extension);

		IsNetworkAlive_t pIsNetworkAlive = NULL;

		HMODULE hDLL=LoadLibrary ("sensapi.dll");

		if ((pIsNetworkAlive=(IsNetworkAlive_t) GetProcAddress (hDLL, "IsNetworkAlive"))) {
			DWORD Network;
			alive=pIsNetworkAlive (&Network);
		}

		FreeLibrary(hDLL);

		e_shell_set_network_available (shell, alive);
	}
}

static void
e_windows_sens_class_init (EWindowsSENSClass *_class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (_class);
	object_class->constructed = windows_sens_constructed;

	extension_class = E_EXTENSION_CLASS (_class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_windows_sens_class_finalize (EWindowsSENSClass *_class)
{
}

static void
e_windows_sens_init (EWindowsSENS *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_windows_sens_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

#ifdef __cplusplus
}
#endif
