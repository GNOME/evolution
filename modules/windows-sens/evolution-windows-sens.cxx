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
#include <stdio.h>


#include <shell/e-shell.h>
#include <e-util/e-extension.h>

#define NUM_ELEMENTS(x) (sizeof((x)) / sizeof((x)[0]))

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

class MySensNetwork : public ISensNetwork
{
private:
    long ref;
    EWindowsSENS *mpEWS;

public:
    MySensNetwork (EWindowsSENS *ews) :
		ref(1),
		mpEWS(ews)
    {}

    HRESULT WINAPI QueryInterface (REFIID iid, void ** ppv)
    {
        if (IsEqualIID (iid, IID_IUnknown) || IsEqualIID (iid, IID_IDispatch) || IsEqualIID (iid, IID_ISensNetwork)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    ULONG WINAPI AddRef ()
    {
        return InterlockedIncrement (&ref);
    }

    ULONG WINAPI Release ()
    {
        int tmp = InterlockedDecrement (&ref);
        return tmp;
    }

    HRESULT WINAPI GetTypeInfoCount (unsigned FAR*)
    {
        return E_NOTIMPL;
    }

    HRESULT WINAPI GetTypeInfo (unsigned, LCID, ITypeInfo FAR* FAR*)
    {
        return E_NOTIMPL;
    }

    HRESULT WINAPI GetIDsOfNames (REFIID, OLECHAR FAR* FAR*, unsigned, LCID, DISPID FAR*)
    {
        return E_NOTIMPL;
    }

    HRESULT WINAPI Invoke (DISPID, REFIID, LCID, WORD, DISPPARAMS FAR*, VARIANT FAR*, EXCEPINFO FAR*, unsigned FAR*)
    {
        return E_NOTIMPL;
    }

// ISensNetwork methods:
    virtual HRESULT WINAPI ConnectionMade (BSTR, ULONG ulType, LPSENS_QOCINFO)
    {
		if (ulType) {
			EShell *shell = windows_sens_get_shell (mpEWS);
			e_shell_set_network_available (shell, TRUE);
		}
		return S_OK;
	}

    virtual HRESULT WINAPI ConnectionMadeNoQOCInfo (BSTR, ULONG)
    {
		//Always followed by ConnectionMade
		return S_OK;
	}

    virtual HRESULT WINAPI ConnectionLost (BSTR, ULONG ulType)
    {
		if (ulType) {
			EShell *shell = windows_sens_get_shell (mpEWS);
			e_shell_set_network_available (shell, FALSE);
		}
		return S_OK;
	}

    virtual HRESULT WINAPI DestinationReachable(BSTR, BSTR , ULONG ulType, LPSENS_QOCINFO)
    {
		if (ulType) {
			EShell *shell = windows_sens_get_shell (mpEWS);
			e_shell_set_network_available (shell, TRUE);
		}
		return S_OK;
    }

    virtual HRESULT WINAPI DestinationReachableNoQOCInfo(BSTR bstrDestination,BSTR bstrConnection,ULONG ulType)
    {
        return S_OK;
    }
};

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

static void
windows_sens_constructed (GObject *object)
{
	static IEventSystem *pIEventSystem =0;
	static IEventSubscription* pIEventSubscription = 0;
	WCHAR buffer[64];
	static const char* eventclassid="{D5978620-5B9F-11D1-8DD2-00AA004ABD5E}";
    static const char* methods[]={"ConnectionMade","ConnectionMadeNoQOCInfo","ConnectionLost","DestinationReachable","DestinationReachableNoQOCInfo"};
    static const char* names[]={"EWS_ConnectionMade","EWS_ConnectionMadeNoQOCInfo","EWS_ConnectionLost","EWS_DestinationReachable","EWS_DestinationReachableNoQOCInfo"};
    static const char* subids[]={"{cd1dcbd6-a14d-4823-a0d2-8473afde360f}","{a82f0e80-1305-400c-ba56-375ae04264a1}","{45233130-b6c3-44fb-a6af-487c47cee611}",
								 "{51377df7-1d29-49eb-af32-4fff77b059fb}","{d16830d3-7a3a-4240-994b-a1fa344385dd}"};

	EWindowsSENS *extension = (E_WINDOWS_SENS (object));
	static MySensNetwork *pISensNetwork = new MySensNetwork (extension);

	CoInitialize(0);

	HRESULT res=CoCreateInstance (CLSID_CEventSystem, 0,CLSCTX_SERVER,IID_IEventSystem,(void**)&pIEventSystem);

	for (unsigned i=0; i<NUM_ELEMENTS(methods); i++)
	{
		res=CoCreateInstance (CLSID_CEventSubscription, 0, CLSCTX_SERVER, IID_IEventSubscription, (LPVOID*)&pIEventSubscription);
		MultiByteToWideChar (0, 0, eventclassid, -1, buffer, 64);
		res=pIEventSubscription->put_EventClassID (buffer);
		res=pIEventSubscription->put_SubscriberInterface ((IUnknown*)pISensNetwork);
		MultiByteToWideChar (0, 0, methods[i], -1, buffer, 64);
		res=pIEventSubscription->put_MethodName (buffer);
		MultiByteToWideChar (0, 0, names[i], -1, buffer, 64);
		res=pIEventSubscription->put_SubscriptionName (buffer);
		MultiByteToWideChar (0, 0, subids[i], -1, buffer, 64);
		res=pIEventSubscription->put_SubscriptionID (buffer);

		/* Make the subscription receive the event only if the ownerof the subscription
		 * is logged on to the same computer as the publisher. This makes this module
		 * work on Windows Vista and Windows 7 with normal user account.
		 */
		res=pIEventSubscription->put_PerUser(TRUE);

		res=pIEventSystem->Store (PROGID_EventSubscription, (IUnknown*)pIEventSubscription);
		pIEventSubscription->Release ();
		pIEventSubscription=0;
	}

	typedef BOOL (WINAPI* IsNetworkAlive_t) (LPDWORD);

	IsNetworkAlive_t pIsNetworkAlive = NULL;

	HMODULE hDLL=LoadLibrary ("sensapi.dll");

	BOOL alive = TRUE;
	if ((pIsNetworkAlive=(IsNetworkAlive_t) GetProcAddress (hDLL, "IsNetworkAlive"))) {
		DWORD Network;
		alive=pIsNetworkAlive (&Network);
	}

	FreeLibrary(hDLL);

	EShell *shell = windows_sens_get_shell (extension);

	e_shell_set_network_available (shell, alive);
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
	CoUninitialize();
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
