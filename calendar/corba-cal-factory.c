/*
 * corba-cal-factory.c: Service that provides access to the calendar repositories.
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 */

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "gnome-cal.h"
#include "main.h"
#include "alarm.h"
#include "timeutil.h"
#include "../libversit/vcc.h"
#include <libgnorba/gnome-factory.h>
#include <libgnorba/gnorba.h>
#include "GnomeCal.h"
#include "corba-cal-factory.h"
#include "corba-cal.h"

CORBA_ORB                 orb;
PortableServer_POA        poa;
PortableServer_POAManager poa_manager;

static POA_GNOME_GenericFactory__epv  calendar_epv;
static POA_GNOME_GenericFactory__vepv calendar_vepv;

/*
 * Servant and Object Factory
 */
static POA_GNOME_GenericFactory calendar_servant;
static GNOME_GenericFactory     calendar_factory;

static CORBA_boolean
calendar_supports (PortableServer_Servant servant,
		   CORBA_char * obj_goad_id,
		   CORBA_Environment * ev)
{
        if (strcmp (obj_goad_id, "IDL:GNOME:Calendar:Repository:1.0") == 0)
                return CORBA_TRUE;
        else
                return CORBA_FALSE;
}

static CORBA_Object
calendar_create_object (PortableServer_Servant servant,
			CORBA_char *goad_id,
			const GNOME_stringlist *params,
			CORBA_Environment *ev)
{
	GnomeCalendar *gcal;
	struct stat s;
	char *name;

	if (params->_length == 1)
		name = params->_buffer [0];
	else
		name = NULL;
		
	if (strcmp (goad_id, "IDL:GNOME:Calendar:Repository:1.0") != 0){
                CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
                                     ex_GNOME_GenericFactory_CannotActivate,
                                     NULL);
		return CORBA_OBJECT_NIL;
	}

	gcal = gnome_calendar_locate (name);
	if (gcal != NULL)
		return CORBA_Object_duplicate (gcal->cal->corba_server, ev);
	
	if (stat (name, &s) != 0){
                CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
                                     ex_GNOME_GenericFactory_CannotActivate,
                                     NULL);
		return CORBA_OBJECT_NIL;
	}

	gcal = new_calendar ("", name, NULL, NULL, FALSE);

	return CORBA_Object_duplicate (gcal->cal->corba_server, ev);
}

void
init_corba_server (void)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	poa_manager = PortableServer_POA__get_the_POAManager (poa, &ev);
	if (ev._major != CORBA_NO_EXCEPTION){
		g_warning ("Can not get the POA manager");
		CORBA_exception_free (&ev);
		return;
	}

	PortableServer_POAManager_activate (poa_manager, &ev);

	/* First create the locator for the repositories as a factory object */
	calendar_vepv.GNOME_GenericFactory_epv = &calendar_epv;
	calendar_epv.supports = calendar_supports;
	calendar_epv.create_object = calendar_create_object;

	calendar_servant.vepv = &calendar_vepv;
	POA_GNOME_GenericFactory__init ((PortableServer_Servant) &calendar_servant, &ev);
	CORBA_free (PortableServer_POA_activate_object (
		poa, (PortableServer_Servant)&calendar_servant, &ev));

	calendar_factory = PortableServer_POA_servant_to_reference (
		poa, (PortableServer_Servant) &calendar_servant, &ev);

	goad_server_register (
		CORBA_OBJECT_NIL, calendar_factory,
		"IDL:GNOME:Calendar:RepositoryLocator:1.0", "object", &ev);
	CORBA_exception_free (&ev);
}

void
unregister_calendar_services (void)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	goad_server_unregister (
		CORBA_OBJECT_NIL,
		"IDL:GNOME:Calendar:RepositoryLocator:1.0", "object", &ev);
	CORBA_exception_free (&ev);
}
