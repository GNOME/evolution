#include <config.h>

#include <bonobo/bonobo-moniker-simple.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-storage.h>

#include "wombat-moniker.h"

#include "wombat-interface-check.h"

#define DEFAULT_DB_URL "xmldb:" EVOLUTION_DATADIR "/evolution/config.xmldb"
#define USER_DB_URL "xmldb:~/evolution/config.xmldb"

#define DB_URL (DEFAULT_DB_URL "#" USER_DB_URL)

static Bonobo_Storage
wombat_root_storage (CORBA_Environment *ev)
{
	static BonoboStorage *root = NULL;
	char *path;

	if (!root) {
		path = g_strconcat (g_get_home_dir (), "/evolution/config",
				    NULL);

		root = bonobo_storage_open_full (BONOBO_IO_DRIVER_FS, path,
						 Bonobo_Storage_CREATE, 0664, 
						 ev);
		
		g_free (path);

		if (BONOBO_EX (ev) || !root)
			return CORBA_OBJECT_NIL;
	}

	return BONOBO_OBJREF (root);
}

static Bonobo_Storage
wombat_lookup_storage (const char        *name, 
		       CORBA_Environment *ev)
{
	Bonobo_Storage root;

	if ((root = wombat_root_storage (ev)) == CORBA_OBJECT_NIL)
		return CORBA_OBJECT_NIL;		
	
	if (!strcmp (name, ""))
		return bonobo_object_dup_ref (root, ev);

	return Bonobo_Storage_openStorage (root, name, Bonobo_Storage_CREATE, 
					   ev);
}

static Bonobo_Storage
wombat_lookup_stream (const char        *name, 
		      CORBA_Environment *ev)
{
	Bonobo_Storage root;

	if (!strcmp (name, "")) {
		bonobo_exception_set (ev, ex_Bonobo_Storage_NotFound);
		return CORBA_OBJECT_NIL;
	}

	if ((root = wombat_root_storage (ev)) == CORBA_OBJECT_NIL)
		return CORBA_OBJECT_NIL;		
	

	return Bonobo_Storage_openStream (root, name, Bonobo_Storage_CREATE, 
					  ev);
}

static CORBA_Object
wombat_lookup_db (CORBA_Environment *ev)
{
	static CORBA_Object db = CORBA_OBJECT_NIL;

	if (db == CORBA_OBJECT_NIL)
		db = bonobo_get_object (DB_URL, 
					"IDL:Bonobo/ConfigDatabase:1.0", ev);

	bonobo_object_dup_ref (db, ev);

	return db;
}

static CORBA_Object
wombat_lookup_interface_check (void)
{
	static WombatInterfaceCheck *object = NULL;
	CORBA_Environment ev;
	CORBA_Object corba_objref;

	if (object == NULL)
		object = wombat_interface_check_new ();

	bonobo_object_ref (BONOBO_OBJECT (object));

	CORBA_exception_init (&ev);
	corba_objref = CORBA_Object_duplicate (BONOBO_OBJREF (object), &ev);
	CORBA_exception_free (&ev);

	return corba_objref;
}

static Bonobo_Unknown 
wombat_moniker_resolve (BonoboMoniker               *moniker,
			const Bonobo_ResolveOptions *options,
			const CORBA_char            *interface,
			CORBA_Environment           *ev)
{
	CORBA_Object    db;
	Bonobo_Moniker  parent;
	const gchar    *name;
	Bonobo_Storage  storage;
	Bonobo_Stream   stream;

	parent = bonobo_moniker_get_parent (moniker, ev);
	if (BONOBO_EX (ev))
		return CORBA_OBJECT_NIL;

	name = bonobo_moniker_get_name (moniker);

	if (parent != CORBA_OBJECT_NIL) {
		
		g_warning ("wombat: parent moniker are not supproted");
		
		bonobo_object_release_unref (parent, ev);

		bonobo_exception_set (ev, ex_Bonobo_Moniker_InterfaceNotFound);

		return CORBA_OBJECT_NIL;
	}
	
	if (!strcmp (interface, "IDL:Bonobo/Storage:1.0")) {

		storage = wombat_lookup_storage (name, ev);
		
		return storage;
	}

	if (!strcmp (interface, "IDL:Bonobo/Stream:1.0")) {
		
		stream = wombat_lookup_stream (name, ev);
		
		return stream;
	}

	if (!strcmp (interface, "IDL:Bonobo/ConfigDatabase:1.0")) {

		if (strcmp (name, ""))
			g_warning ("wombat: unused moniker name");

		if ((db = wombat_lookup_db (ev)) != CORBA_OBJECT_NIL)
			return db;
	}

	if (!strcmp (interface, "IDL:GNOME/Evolution/WombatInterfaceCheck:1.0"))
		return wombat_lookup_interface_check ();

	bonobo_exception_set (ev, ex_Bonobo_Moniker_InterfaceNotFound);
	return CORBA_OBJECT_NIL;
}

BonoboObject *
wombat_moniker_factory (BonoboGenericFactory *this,
			const char           *object_id,
			void                 *data)
{
	g_return_val_if_fail (object_id != NULL, NULL);

	if (!strcmp (object_id, "OAFIID:Bonobo_Moniker_wombat"))

		return BONOBO_OBJECT (bonobo_moniker_simple_new (
			"wombat:", wombat_moniker_resolve));

	else
		g_warning ("Failing to manufacture a '%s'", object_id);

	return NULL;	
}

