/*
 * corba-cal.c: Service that provides access to the calendar repository
 *
 * Author:
 *   Miguel de Icaza (miguel@gnu.org)
 */

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "calendar.h"
#include "gnome-cal.h"
#include "alarm.h"
#include "timeutil.h"
#include "../libversit/vcc.h"
#include <libgnorba/gnome-factory.h>
#include "GnomeCal.h"
#include "corba-cal-factory.h"
#include "corba-cal.h"

typedef struct {
	POA_GNOME_Calendar_Repository servant;	
	GnomeCalendar *calendar;
} CalendarServant;

/*
 * Vectors
 */
static POA_GNOME_Calendar_Repository__epv  calendar_repository_epv;
static POA_GNOME_Calendar_Repository__vepv calendar_repository_vepv;

/*
 * Servant and Object Factory
 */
static POA_GNOME_Calendar_Repository calendar_repository_servant;

static inline GnomeCalendar *
gnomecal_from_servant (PortableServer_Servant servant)
{
	CalendarServant *cs = (CalendarServant *) servant;

	return cs->calendar;
}

static CORBA_char *
cal_repo_get_object (PortableServer_Servant servant,
		     CORBA_char  *uid,
		     CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;
	char *buffer;
	CORBA_char *ret;
	
	obj = calendar_object_find_event (gcal->cal, uid);
	if (obj == NULL){
		CORBA_exception_set (
			ev,
			CORBA_USER_EXCEPTION,
			ex_GNOME_Calendar_Repository_NotFound,
			"");
		return NULL;
	}

	buffer = calendar_string_from_object (obj);
	ret = CORBA_string_dup (buffer);
	free (buffer);

	return ret;
}

static CORBA_char *
cal_repo_get_object_by_pilot_id (PortableServer_Servant servant,
				 CORBA_long  pilot_id,
				 CORBA_Environment  *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;
	char *buffer;
	CORBA_char *ret;
	
	obj = calendar_object_find_by_pilot (gcal->cal, pilot_id);
	if (obj == NULL){
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Calendar_Repository_NotFound, "");
		return NULL;
	}

	buffer = calendar_string_from_object (obj);
	ret = CORBA_string_dup (buffer);
	free (buffer);

	return ret;
	
}

static CORBA_char *
cal_repo_get_id_from_pilot_id (PortableServer_Servant servant,
			       CORBA_long  pilot_id,
			       CORBA_Environment  *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;
	
	obj = calendar_object_find_by_pilot (gcal->cal, pilot_id);
	if (obj == NULL){
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Calendar_Repository_NotFound, "");
		return NULL;
	}

	return CORBA_string_dup (obj->uid);
}

static void
cal_repo_delete_object (PortableServer_Servant servant,
			CORBA_char  *uid,
			CORBA_Environment  *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;

	obj = calendar_object_find_event (gcal->cal, uid);
	if (obj == NULL){
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Calendar_Repository_NotFound, NULL);
		return;
	}

	gnome_calendar_remove_object (gcal, obj);
}

static void
cal_repo_update_object (PortableServer_Servant servant,
			CORBA_char *uid,
			CORBA_char *vcalendar_object,
			CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;
	iCalObject *new_object;
	
	new_object = ical_object_new_from_string (vcalendar_object);
	
	obj = calendar_object_find_event (gcal->cal, uid);
	if (obj != NULL){
		calendar_remove_object (gcal->cal, obj);
	} 

	calendar_add_object (gcal->cal, new_object);
}

static void
cal_repo_update_pilot_id (PortableServer_Servant servant,
			  CORBA_char *uid,
			  CORBA_long pilot_id,
			  CORBA_long pilot_status,
			  CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;
	
	obj = calendar_object_find_event (gcal->cal, uid);
	if (obj == NULL){
		CORBA_exception_set (
			ev,
			CORBA_USER_EXCEPTION,
			ex_GNOME_Calendar_Repository_NotFound,
			"");
		return;
	}

	obj->pilot_id = pilot_id;
	obj->pilot_status = pilot_status;
}

static CORBA_char *
cal_repo_get_objects (PortableServer_Servant servant,
		      CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	Calendar *dirty_cal;
	GList *l;
	char *str;
	CORBA_char *res;
	
	dirty_cal = calendar_new ("Temporal");
	
	for (l = gcal->cal->events; l; l = l->next){
		iCalObject *obj = l->data;

		obj = ical_object_duplicate (l->data);

		calendar_add_object (dirty_cal, obj);
	}
	str = calendar_get_as_vcal_string (dirty_cal);
	res = CORBA_string_dup (str);
	g_free (str);
	calendar_destroy (dirty_cal);

	return res;
}

static CORBA_char *
cal_repo_get_updated_objects (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	Calendar *dirty_cal;
	GList *l;
	CORBA_char *res;
	char *str;

	dirty_cal = calendar_new ("Temporal");
	
	for (l = gcal->cal->events; l; l = l->next){
		iCalObject *obj = l->data;

		if (obj->pilot_status != ICAL_PILOT_SYNC_MOD)
			continue;

		obj = ical_object_duplicate (l->data);

		calendar_add_object (dirty_cal, obj);
	}
	str = calendar_get_as_vcal_string (dirty_cal);
	res = CORBA_string_dup (str);
	g_free (str);
	calendar_destroy (dirty_cal);

	return res;
}

static void
cal_repo_done (PortableServer_Servant servant,
	       CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);

	calendar_save (gcal->cal, NULL);
}

static void
init_calendar_repo_class (void)
{
	calendar_repository_epv.get_object = cal_repo_get_object;
	calendar_repository_epv.get_object_by_pilot_id = cal_repo_get_object_by_pilot_id;
	calendar_repository_epv.get_id_from_pilot_id = cal_repo_get_id_from_pilot_id;
	calendar_repository_epv.delete_object = cal_repo_delete_object;
	calendar_repository_epv.update_object = cal_repo_update_object;
	calendar_repository_epv.get_objects = cal_repo_get_objects;
	calendar_repository_epv.get_updated_objects = cal_repo_get_updated_objects;
	calendar_repository_epv.update_pilot_id = cal_repo_update_pilot_id;
	
	calendar_repository_epv.done = cal_repo_done;
	
	calendar_repository_vepv.GNOME_Calendar_Repository_epv =
		&calendar_repository_epv;

	calendar_repository_servant.vepv = &calendar_repository_vepv;
}

/*
 * Initializes the CORBA parts of the @calendar object
 */
void
gnome_calendar_create_corba_server (GnomeCalendar *calendar)
{
	static gboolean class_inited = FALSE;
	CalendarServant *calendar_servant;
	CORBA_Environment ev;
	
	if (!class_inited){
		init_calendar_repo_class ();
		class_inited = TRUE;
	}

	calendar_servant = g_new0 (CalendarServant, 1);
	calendar_servant->servant.vepv = &calendar_repository_vepv;
	calendar_servant->calendar = calendar;

	CORBA_exception_init (&ev);
	POA_GNOME_Calendar_Repository__init ((PortableServer_Servant) calendar_servant, &ev);
	CORBA_free (
		PortableServer_POA_activate_object (poa, calendar_servant, &ev));
	calendar->cal->corba_server = PortableServer_POA_servant_to_reference (
		poa, calendar_servant, &ev);
	CORBA_exception_free (&ev);
}
