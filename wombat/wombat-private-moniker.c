#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <bonobo/bonobo-moniker-simple.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-storage.h>
#include "wombat-private-moniker.h"

#define USER_DB_URL "xmldb:~/evolution/private/config.xmldb"

#define DB_URL (USER_DB_URL)

static Bonobo_Storage
wombat_private_root_storage (CORBA_Environment *ev)
{
	static BonoboStorage *root = NULL;
	char *path;

	if (!root) {
		path = g_strconcat (g_get_home_dir (), "/evolution/private/config",
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
wombat_private_lookup_storage (const char        *name, 
			       CORBA_Environment *ev)
{
	Bonobo_Storage root;

	if ((root = wombat_private_root_storage (ev)) == CORBA_OBJECT_NIL)
		return CORBA_OBJECT_NIL;		
	
	if (!strcmp (name, ""))
		return bonobo_object_dup_ref (root, ev);

	return Bonobo_Storage_openStorage (root, name, Bonobo_Storage_CREATE, 
					   ev);
}

static Bonobo_Storage
wombat_private_lookup_stream (const char        *name, 
		      CORBA_Environment *ev)
{
	Bonobo_Storage root;

	if (!strcmp (name, "")) {
		bonobo_exception_set (ev, ex_Bonobo_Storage_NotFound);
		return CORBA_OBJECT_NIL;
	}

	if ((root = wombat_private_root_storage (ev)) == CORBA_OBJECT_NIL)
		return CORBA_OBJECT_NIL;		
	

	return Bonobo_Storage_openStream (root, name, Bonobo_Storage_CREATE, 
					  ev);
}

static CORBA_Object
wombat_private_lookup_db (CORBA_Environment *ev)
{
	static CORBA_Object db = CORBA_OBJECT_NIL;

	/* kinda crappy, but make sure permissions for the directory
           are correct whenever someone new opens the db. */
	char *private_dir_path;

	private_dir_path = g_strconcat (g_get_home_dir (),
					"/evolution/private",
					NULL);
	chmod (private_dir_path, 0700);
	g_free (private_dir_path);

	if (db == CORBA_OBJECT_NIL)
		db = bonobo_get_object (DB_URL, 
					"IDL:Bonobo/ConfigDatabase:1.0", ev);

	bonobo_object_dup_ref (db, ev);

	return db;
}

static Bonobo_Unknown 
wombat_private_moniker_resolve (BonoboMoniker               *moniker,
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

		storage = wombat_private_lookup_storage (name, ev);
		
		return storage;
	}

	if (!strcmp (interface, "IDL:Bonobo/Stream:1.0")) {
		
		stream = wombat_private_lookup_stream (name, ev);
		
		return stream;
	}

	if (!strcmp (interface, "IDL:Bonobo/ConfigDatabase:1.0")) {

		if (strcmp (name, ""))
			g_warning ("wombat: unused moniker name");

		if ((db = wombat_private_lookup_db (ev)) != CORBA_OBJECT_NIL)
			return db;
	}

	bonobo_exception_set (ev, ex_Bonobo_Moniker_InterfaceNotFound);
	return CORBA_OBJECT_NIL;
}

BonoboObject *
wombat_private_moniker_factory (BonoboGenericFactory *this,
			const char           *object_id,
			void                 *data)
{
	g_return_val_if_fail (object_id != NULL, NULL);

	if (!strcmp (object_id, "OAFIID:Bonobo_Moniker_wombat_private"))

		return BONOBO_OBJECT (bonobo_moniker_simple_new (
			"wombat-private:", wombat_private_moniker_resolve));

	else
		g_warning ("Failing to manufacture a '%s'", object_id);

	return NULL;	
}

