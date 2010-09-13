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
#error This file cannot be built with C++ compiler
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define INITGUID
#include <windows.h>
#include <rpc.h>

#ifdef HAVE_EVENTSYS_H
#include <eventsys.h>
#else

/* Extract relevant typedefs from mingw-w64 headers */

typedef struct IEnumEventObject IEnumEventObject;

const IID IID_IEnumEventObject;
typedef struct IEnumEventObjectVtbl {
	BEGIN_INTERFACE
		HRESULT (WINAPI *QueryInterface)(IEnumEventObject *This,REFIID riid,PVOID *ppvObject);
		ULONG (WINAPI *AddRef)(IEnumEventObject *This);
		ULONG (WINAPI *Release)(IEnumEventObject *This);
		HRESULT (WINAPI *Clone)(IEnumEventObject *This,IEnumEventObject **ppInterface);
		HRESULT (WINAPI *Next)(IEnumEventObject *This,ULONG cReqElem,IUnknown **ppInterface,ULONG *cRetElem);
		HRESULT (WINAPI *Reset)(IEnumEventObject *This);
		HRESULT (WINAPI *Skip)(IEnumEventObject *This,ULONG cSkipElem);
	END_INTERFACE
} IEnumEventObjectVtbl;
struct IEnumEventObject {
	CONST_VTBL struct IEnumEventObjectVtbl *lpVtbl;
};

typedef struct IEventObjectCollection IEventObjectCollection;

const IID IID_IEventObjectCollection;
typedef struct IEventObjectCollectionVtbl {
	BEGIN_INTERFACE
		HRESULT (WINAPI *QueryInterface)(IEventObjectCollection *This,REFIID riid,PVOID *ppvObject);
		ULONG (WINAPI *AddRef)(IEventObjectCollection *This);
		ULONG (WINAPI *Release)(IEventObjectCollection *This);
		HRESULT (WINAPI *GetTypeInfoCount)(IEventObjectCollection *This,UINT *pctinfo);
		HRESULT (WINAPI *GetTypeInfo)(IEventObjectCollection *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
		HRESULT (WINAPI *GetIDsOfNames)(IEventObjectCollection *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
		HRESULT (WINAPI *Invoke)(IEventObjectCollection *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
		HRESULT (WINAPI *get__NewEnum)(IEventObjectCollection *This,IUnknown **ppUnkEnum);
		HRESULT (WINAPI *get_Item)(IEventObjectCollection *This,BSTR objectID,VARIANT *pItem);
		HRESULT (WINAPI *get_NewEnum)(IEventObjectCollection *This,IEnumEventObject **ppEnum);
		HRESULT (WINAPI *get_Count)(IEventObjectCollection *This,long *pCount);
		HRESULT (WINAPI *Add)(IEventObjectCollection *This,VARIANT *item,BSTR objectID);
		HRESULT (WINAPI *Remove)(IEventObjectCollection *This,BSTR objectID);
	END_INTERFACE
} IEventObjectCollectionVtbl;
struct IEventObjectCollection {
	CONST_VTBL struct IEventObjectCollectionVtbl *lpVtbl;
};

typedef struct IEventSystem IEventSystem;

const IID IID_IEventSystem;
typedef struct IEventSystemVtbl {
	BEGIN_INTERFACE
		HRESULT (WINAPI *QueryInterface)(IEventSystem *This,REFIID riid,PVOID *ppvObject);
		ULONG (WINAPI *AddRef)(IEventSystem *This);
		ULONG (WINAPI *Release)(IEventSystem *This);
		HRESULT (WINAPI *GetTypeInfoCount)(IEventSystem *This,UINT *pctinfo);
		HRESULT (WINAPI *GetTypeInfo)(IEventSystem *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
		HRESULT (WINAPI *GetIDsOfNames)(IEventSystem *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
		HRESULT (WINAPI *Invoke)(IEventSystem *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
		HRESULT (WINAPI *Query)(IEventSystem *This,BSTR progID,BSTR queryCriteria,int *errorIndex,IUnknown **ppInterface);
		HRESULT (WINAPI *Store)(IEventSystem *This,BSTR ProgID,IUnknown *pInterface);
		HRESULT (WINAPI *Remove)(IEventSystem *This,BSTR progID,BSTR queryCriteria,int *errorIndex);
		HRESULT (WINAPI *get_EventObjectChangeEventClassID)(IEventSystem *This,BSTR *pbstrEventClassID);
		HRESULT (WINAPI *QueryS)(IEventSystem *This,BSTR progID,BSTR queryCriteria,IUnknown **ppInterface);
		HRESULT (WINAPI *RemoveS)(IEventSystem *This,BSTR progID,BSTR queryCriteria);
	END_INTERFACE
} IEventSystemVtbl;
struct IEventSystem {
	CONST_VTBL struct IEventSystemVtbl *lpVtbl;
};

typedef struct IEventSubscription IEventSubscription;

const IID IID_IEventSubscription;
typedef struct IEventSubscriptionVtbl {
	BEGIN_INTERFACE
		HRESULT (WINAPI *QueryInterface)(IEventSubscription *This,REFIID riid,PVOID *ppvObject);
		ULONG (WINAPI *AddRef)(IEventSubscription *This);
		ULONG (WINAPI *Release)(IEventSubscription *This);
		HRESULT (WINAPI *GetTypeInfoCount)(IEventSubscription *This,UINT *pctinfo);
		HRESULT (WINAPI *GetTypeInfo)(IEventSubscription *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
		HRESULT (WINAPI *GetIDsOfNames)(IEventSubscription *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
		HRESULT (WINAPI *Invoke)(IEventSubscription *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
		HRESULT (WINAPI *get_SubscriptionID)(IEventSubscription *This,BSTR *pbstrSubscriptionID);
		HRESULT (WINAPI *put_SubscriptionID)(IEventSubscription *This,BSTR bstrSubscriptionID);
		HRESULT (WINAPI *get_SubscriptionName)(IEventSubscription *This,BSTR *pbstrSubscriptionName);
		HRESULT (WINAPI *put_SubscriptionName)(IEventSubscription *This,BSTR bstrSubscriptionName);
		HRESULT (WINAPI *get_PublisherID)(IEventSubscription *This,BSTR *pbstrPublisherID);
		HRESULT (WINAPI *put_PublisherID)(IEventSubscription *This,BSTR bstrPublisherID);
		HRESULT (WINAPI *get_EventClassID)(IEventSubscription *This,BSTR *pbstrEventClassID);
		HRESULT (WINAPI *put_EventClassID)(IEventSubscription *This,BSTR bstrEventClassID);
		HRESULT (WINAPI *get_MethodName)(IEventSubscription *This,BSTR *pbstrMethodName);
		HRESULT (WINAPI *put_MethodName)(IEventSubscription *This,BSTR bstrMethodName);
		HRESULT (WINAPI *get_SubscriberCLSID)(IEventSubscription *This,BSTR *pbstrSubscriberCLSID);
		HRESULT (WINAPI *put_SubscriberCLSID)(IEventSubscription *This,BSTR bstrSubscriberCLSID);
		HRESULT (WINAPI *get_SubscriberInterface)(IEventSubscription *This,IUnknown **ppSubscriberInterface);
		HRESULT (WINAPI *put_SubscriberInterface)(IEventSubscription *This,IUnknown *pSubscriberInterface);
		HRESULT (WINAPI *get_PerUser)(IEventSubscription *This,WINBOOL *pfPerUser);
		HRESULT (WINAPI *put_PerUser)(IEventSubscription *This,WINBOOL fPerUser);
		HRESULT (WINAPI *get_OwnerSID)(IEventSubscription *This,BSTR *pbstrOwnerSID);
		HRESULT (WINAPI *put_OwnerSID)(IEventSubscription *This,BSTR bstrOwnerSID);
		HRESULT (WINAPI *get_Enabled)(IEventSubscription *This,WINBOOL *pfEnabled);
		HRESULT (WINAPI *put_Enabled)(IEventSubscription *This,WINBOOL fEnabled);
		HRESULT (WINAPI *get_Description)(IEventSubscription *This,BSTR *pbstrDescription);
		HRESULT (WINAPI *put_Description)(IEventSubscription *This,BSTR bstrDescription);
		HRESULT (WINAPI *get_MachineName)(IEventSubscription *This,BSTR *pbstrMachineName);
		HRESULT (WINAPI *put_MachineName)(IEventSubscription *This,BSTR bstrMachineName);
		HRESULT (WINAPI *GetPublisherProperty)(IEventSubscription *This,BSTR bstrPropertyName,VARIANT *propertyValue);
		HRESULT (WINAPI *PutPublisherProperty)(IEventSubscription *This,BSTR bstrPropertyName,VARIANT *propertyValue);
		HRESULT (WINAPI *RemovePublisherProperty)(IEventSubscription *This,BSTR bstrPropertyName);
		HRESULT (WINAPI *GetPublisherPropertyCollection)(IEventSubscription *This,IEventObjectCollection **collection);
		HRESULT (WINAPI *GetSubscriberProperty)(IEventSubscription *This,BSTR bstrPropertyName,VARIANT *propertyValue);
		HRESULT (WINAPI *PutSubscriberProperty)(IEventSubscription *This,BSTR bstrPropertyName,VARIANT *propertyValue);
		HRESULT (WINAPI *RemoveSubscriberProperty)(IEventSubscription *This,BSTR bstrPropertyName);
		HRESULT (WINAPI *GetSubscriberPropertyCollection)(IEventSubscription *This,IEventObjectCollection **collection);
		HRESULT (WINAPI *get_InterfaceID)(IEventSubscription *This,BSTR *pbstrInterfaceID);
		HRESULT (WINAPI *put_InterfaceID)(IEventSubscription *This,BSTR bstrInterfaceID);
	END_INTERFACE
} IEventSubscriptionVtbl;
struct IEventSubscription {
	CONST_VTBL struct IEventSubscriptionVtbl *lpVtbl;
};

#define PROGID_EventSubscription OLESTR("EventSystem.EventSubscription")

#endif

#ifdef HAVE_SENSEVTS_H
#include <sensevts.h>
#else

/* Extract relevant typedefs from mingw-w64 headers */

typedef struct {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwOutSpeed;
	DWORD dwInSpeed;
} *LPSENS_QOCINFO;

typedef struct ISensNetwork ISensNetwork;

const IID IID_ISensNetwork;
typedef struct ISensNetworkVtbl {
	BEGIN_INTERFACE
		HRESULT (WINAPI *QueryInterface)(ISensNetwork *This,REFIID riid,PVOID *ppvObject);
		ULONG (WINAPI *AddRef)(ISensNetwork *This);
		ULONG (WINAPI *Release)(ISensNetwork *This);
		HRESULT (WINAPI *GetTypeInfoCount)(ISensNetwork *This,UINT *pctinfo);
		HRESULT (WINAPI *GetTypeInfo)(ISensNetwork *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
		HRESULT (WINAPI *GetIDsOfNames)(ISensNetwork *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
		HRESULT (WINAPI *Invoke)(ISensNetwork *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
		HRESULT (WINAPI *ConnectionMade)(ISensNetwork *This,BSTR bstrConnection,ULONG ulType,LPSENS_QOCINFO lpQOCInfo);
		HRESULT (WINAPI *ConnectionMadeNoQOCInfo)(ISensNetwork *This,BSTR bstrConnection,ULONG ulType);
		HRESULT (WINAPI *ConnectionLost)(ISensNetwork *This,BSTR bstrConnection,ULONG ulType);
		HRESULT (WINAPI *DestinationReachable)(ISensNetwork *This,BSTR bstrDestination,BSTR bstrConnection,ULONG ulType,LPSENS_QOCINFO lpQOCInfo);
		HRESULT (WINAPI *DestinationReachableNoQOCInfo)(ISensNetwork *This,BSTR bstrDestination,BSTR bstrConnection,ULONG ulType);
	END_INTERFACE
} ISensNetworkVtbl;
struct ISensNetwork {
	CONST_VTBL struct ISensNetworkVtbl *lpVtbl;
};

#endif

#include <shell/e-shell.h>
#include <e-util/e-extension.h>

/* 4E14FB9F-2E22-11D1-9964-00C04FBBB345 */
DEFINE_GUID (IID_IEventSystem, 0x4E14FB9F, 0x2E22, 0x11D1, 0x99, 0x64, 0x00, 0xC0, 0x4F, 0xBB, 0xB3, 0x45);

/* 4A6B0E15-2E38-11D1-9965-00C04FBBB345 */
DEFINE_GUID (IID_IEventSubscription, 0x4A6B0E15, 0x2E38, 0x11D1, 0x99, 0x65, 0x00, 0xC0, 0x4F, 0xBB, 0xB3, 0x45);

/* d597bab1-5b9f-11d1-8dd2-00aa004abd5e */
DEFINE_GUID (IID_ISensNetwork, 0xd597bab1, 0x5b9f, 0x11d1, 0x8d, 0xd2, 0x00, 0xaa, 0x00, 0x4a, 0xbd, 0x5e);

/* 4E14FBA2-2E22-11D1-9964-00C04FBBB345 */
DEFINE_GUID (CLSID_CEventSystem, 0x4E14FBA2, 0x2E22, 0x11D1, 0x99, 0x64, 0x00, 0xC0, 0x4F, 0xBB, 0xB3, 0x45);

/* 7542e960-79c7-11d1-88f9-0080c7d771bf */
DEFINE_GUID (CLSID_CEventSubscription, 0x7542e960, 0x79c7, 0x11d1, 0x88, 0xf9, 0x00, 0x80, 0xc7, 0xd7, 0x71, 0xbf);

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

static void e_sens_network_listener_init (ESensNetworkListener**,EWindowsSENS*);

/* Functions to implement ISensNetwork interface */

static HRESULT WINAPI e_sens_network_listener_queryinterface (ISensNetwork*,REFIID,PVOID *);
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
		((LPUNKNOWN)*ppv)->lpVtbl->AddRef ((LPUNKNOWN)*ppv);
		return S_OK;
	}
	*ppv = NULL;
	return E_NOINTERFACE;
}

static ULONG WINAPI
e_sens_network_listener_addref (ISensNetwork *This)
{
	ESensNetworkListener *esnl_ptr=(ESensNetworkListener*)This;
	return InterlockedIncrement (&(esnl_ptr->ref));
}

static ULONG WINAPI
e_sens_network_listener_release (ISensNetwork *This)
{
	ESensNetworkListener *esnl_ptr=(ESensNetworkListener*)This;
	ULONG tmp = InterlockedDecrement (&(esnl_ptr->ref));
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
		g_usleep (G_USEC_PER_SEC);
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
		g_usleep (G_USEC_PER_SEC);
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
e_sens_network_listener_init (ESensNetworkListener **esnl_ptr,
                             EWindowsSENS          *ews)
{
	(*esnl_ptr) = g_new0 (ESensNetworkListener,1);
	(*esnl_ptr)->lpVtbl = &ESensNetworkListenerVtbl;
	(*esnl_ptr)->ews_ptr = ews;
	(*esnl_ptr)->ref = 1;
}

static BSTR
_mb2wchar (const gchar * a)
{
	static WCHAR b[64];
	MultiByteToWideChar (0, 0, a, -1, b, 64);
	return b;
}

static const gchar * add_curly_braces_to_uuid (const gchar * string_uuid)
{
	static gchar curly_braced_uuid_string[64];
	gint i;
	if (!string_uuid)
		return NULL;
	lstrcpy(curly_braced_uuid_string,"{");
	i = strlen (curly_braced_uuid_string);
	lstrcat (curly_braced_uuid_string+i,string_uuid);
	i = strlen (curly_braced_uuid_string);
	lstrcat(curly_braced_uuid_string+i,"}");
	return curly_braced_uuid_string;
}

#define SENSAPI_DLL "sensapi.dll"

static void
windows_sens_constructed (GObject *object)
{
	HRESULT res;
	static IEventSystem *pEventSystem =0;
	static IEventSubscription* pEventSubscription = 0;
	static ESensNetworkListener *pESensNetworkListener = 0;
	static const gchar * eventclassid="{D5978620-5B9F-11D1-8DD2-00AA004ABD5E}";
	static const gchar * methods[]={
		"ConnectionMade",
		"ConnectionMadeNoQOCInfo",
		"ConnectionLost",
		"DestinationReachable",
		"DestinationReachableNoQOCInfo"
	};
	static const gchar * names[]={
		"EWS_ConnectionMade",
		"EWS_ConnectionMadeNoQOCInfo",
		"EWS_ConnectionLost",
		"EWS_DestinationReachable",
		"EWS_DestinationReachableNoQOCInfo"
	};
	guchar * subids[] = { 0, 0, 0, 0, 0 };

	EWindowsSENS *extension = (E_WINDOWS_SENS (object));
	e_sens_network_listener_init (&pESensNetworkListener, extension);

	CoInitialize (0);

	res=CoCreateInstance (&CLSID_CEventSystem, 0,CLSCTX_SERVER,&IID_IEventSystem,(LPVOID*)&pEventSystem);

	if (res == S_OK && pEventSystem) {

		unsigned i;

		for (i=0; i<G_N_ELEMENTS (methods); i++) {

			res=CoCreateInstance (&CLSID_CEventSubscription, 0, CLSCTX_SERVER, &IID_IEventSubscription, (LPVOID*)&pEventSubscription);

			if (res == S_OK && pEventSubscription) {
				UUID tmp_uuid;
				UuidCreate (&tmp_uuid);
				UuidToString (&tmp_uuid, &subids[i]);
				res=pEventSubscription->lpVtbl->put_SubscriptionID (pEventSubscription, _mb2wchar (add_curly_braces_to_uuid ((gchar *)subids[i])));
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
			pEventSubscription->lpVtbl->Release (pEventSubscription);
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

		char *buf = NULL;
		char dummy;
		int n, k;
		HMODULE hDLL = NULL;

		n = GetSystemDirectory (&dummy, 0);

		if (n <= 0)
			goto cleanup;

		buf = g_malloc (n + 1 + strlen (SENSAPI_DLL));
		k = GetSystemDirectory (buf, n);
  
		if (k == 0 || k > n)
			goto cleanup;

		if (!G_IS_DIR_SEPARATOR (buf[strlen (buf) -1]))
			strcat (buf, G_DIR_SEPARATOR_S);
		strcat (buf, SENSAPI_DLL);

		hDLL=LoadLibrary (buf);

		if ((pIsNetworkAlive=(IsNetworkAlive_t) GetProcAddress (hDLL, "IsNetworkAlive"))) {
			DWORD Network;
			alive=pIsNetworkAlive (&Network);
		}

		FreeLibrary (hDLL);

		e_shell_set_network_available (shell, alive);

cleanup:
		g_free (buf);
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

