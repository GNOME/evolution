#include <config.h>

#include <bonobo/bonobo-moniker-simple.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include "wombat-moniker.h"

#define DEFAULT_DB_URL "xmldb:/tmp/wombat-default-config.xmldb"
#define USER_DB_URL "xmldb:~/evolution/config.xmldb"

#define DB_URL (DEFAULT_DB_URL "#" USER_DB_URL)

static CORBA_Object
wombat_lookup_db (CORBA_Environment *ev)
{
	static CORBA_Object db = CORBA_OBJECT_NIL;

	if (db == CORBA_OBJECT_NIL)
		db = bonobo_get_object (DB_URL, 
					"IDL:Bonobo/ConfigDatabase:1.0", ev);

	return db;
}

Bonobo_Unknown 
wombat_moniker_resolve (BonoboMoniker               *moniker,
			const Bonobo_ResolveOptions *options,
			const CORBA_char            *interface,
			CORBA_Environment           *ev)
{
	CORBA_Object    db;
	Bonobo_Moniker  parent;
	const gchar    *name;

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

		/* fixme: */
	}

	if (!strcmp (interface, "IDL:Bonobo/Stream:1.0")) {

		/* fixme: */
	}

	if (!strcmp (interface, "IDL:Bonobo/ConfigDatabase:1.0")) {

		if (strcmp (name, ""))
			g_warning ("wombat: unused moniker name");

		if ((db = wombat_lookup_db (ev)) != CORBA_OBJECT_NIL)
			return db;
	}

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

