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
/*#include "calendar.h" DELETE */
#include "gnome-cal.h"
#include "alarm.h"
#include <cal-util/timeutil.h>
#include "libversit/vcc.h"
#include <libgnorba/gnome-factory.h>
#include "GnomeCal.h"
#include "corba-cal-factory.h"
#include "corba-cal.h"

static iCalObject *
calendar_object_find_by_pilot (GnomeCalendar *cal, int pilot_id);


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
		     const CORBA_char  *uid,
		     CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;
	char *obj_string;
	char *buffer;
	CORBA_char *ret;
	
	/*obj = calendar_object_find_event (gcal->cal, uid); DELETE */
	obj_string = cal_client_get_object (gcal->calc, uid);
	obj = string_to_ical_object (obj_string);
	free (obj_string);

	if (obj == NULL){
	        GNOME_Calendar_Repository_NotFound *exn;

		exn = GNOME_Calendar_Repository_NotFound__alloc();
		CORBA_exception_set (
			ev,
			CORBA_USER_EXCEPTION,
			ex_GNOME_Calendar_Repository_NotFound,
			exn);
		return NULL;
	}

	/* buffer = calendar_string_from_object (obj); DELETE */
	buffer = ical_object_to_string (obj);
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

	obj = calendar_object_find_by_pilot (gcal, pilot_id);
	if (obj == NULL){
	        GNOME_Calendar_Repository_NotFound *exn;

		exn = GNOME_Calendar_Repository_NotFound__alloc();
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Calendar_Repository_NotFound, exn);
		return NULL;
	}

	/* buffer = calendar_string_from_object (obj); DELETE */
	buffer = ical_object_to_string (obj);
	ret = CORBA_string_dup (buffer);
	free (buffer);

	return ret;
	
}

/* where should this go? FIX ME */
static iCalObject *
calendar_object_find_by_pilot (GnomeCalendar *cal, int pilot_id)
{
	GList *l, *uids;
	
	g_return_val_if_fail (cal != NULL, NULL);

	uids = cal_client_get_uids (cal->calc,
				    CALOBJ_TYPE_EVENT |
				    CALOBJ_TYPE_TODO |
				    CALOBJ_TYPE_JOURNAL |
				    CALOBJ_TYPE_OTHER |
				    CALOBJ_TYPE_ANY);
	for (l = uids; l; l = l->next){
		char *obj_string = cal_client_get_object (cal->calc, l->data);
		iCalObject *obj = string_to_ical_object (obj_string);

		if (obj->pilot_id == pilot_id)
			return obj;
	}

	/* DELETE
	for (l = cal->todo; l; l = l->next){
		iCalObject *obj = l->data;

		if (obj->pilot_id == pilot_id)
			return obj;
	}
	*/

	return NULL;
}

static CORBA_char *
cal_repo_get_id_from_pilot_id (PortableServer_Servant servant,
			       CORBA_long  pilot_id,
			       CORBA_Environment  *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;
	
	obj = calendar_object_find_by_pilot (gcal, pilot_id);
	if (obj == NULL){
	        GNOME_Calendar_Repository_NotFound *exn;

		exn = GNOME_Calendar_Repository_NotFound__alloc();
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Calendar_Repository_NotFound, exn);
		return NULL;
	}

	return CORBA_string_dup (obj->uid);
}

static void
cal_repo_delete_object (PortableServer_Servant servant,
			const CORBA_char  *uid,
			CORBA_Environment  *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;
	char *obj_string;

	/* obj = calendar_object_find_event (gcal->cal, uid); */
	obj_string = cal_client_get_object (gcal->calc, uid);
	obj = string_to_ical_object (obj_string);
	free (obj_string);

	if (obj == NULL){
	        GNOME_Calendar_Repository_NotFound *exn;

		exn = GNOME_Calendar_Repository_NotFound__alloc();
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Calendar_Repository_NotFound,
				     exn);
		return;
	}

	gnome_calendar_remove_object (gcal, obj);
}

static void
cal_repo_update_object (PortableServer_Servant servant,
			const CORBA_char *uid,
			const CORBA_char *vcalendar_object,
			CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	/* iCalObject *obj; char *obj_string; */
	iCalObject *new_object;
	char *new_object_string;
	
	new_object = ical_object_new_from_string (vcalendar_object);

#if 0  /* it looks like this is taken care of in cal_client_update_object? */
	/* DELETE */
	/* obj = calendar_object_find_event (gcal->cal, uid); DELETE */
	obj_string = cal_client_get_object (gcal->calc, uid);
	obj = string_to_ical_object (obj_string);
	free (obj_string);

	if (obj != NULL){
		/* calendar_remove_object (gcal->cal, obj); DELETE */
		cal_client_remove_object (gcal->calc, uid);
	} 
#endif /* 0 */

	/* calendar_add_object (gcal->cal, new_object); DELETE */
	new_object_string = ical_object_to_string (new_object);
	cal_client_update_object (gcal->calc, uid, new_object_string);
	free (new_object_string);
}

static void
cal_repo_update_pilot_id (PortableServer_Servant servant,
			  const CORBA_char *uid,
			  const CORBA_long pilot_id,
			  const CORBA_long pilot_status,
			  CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	iCalObject *obj;
	char *obj_string;
	
	/* obj = calendar_object_find_event (gcal->cal, uid); DELETE */
	obj_string = cal_client_get_object (gcal->calc, uid);
	obj = string_to_ical_object (obj_string);
	free (obj_string);

	if (obj == NULL){
	        GNOME_Calendar_Repository_NotFound *exn;

		exn = GNOME_Calendar_Repository_NotFound__alloc();
		CORBA_exception_set (
			ev,
			CORBA_USER_EXCEPTION,
			ex_GNOME_Calendar_Repository_NotFound,
			exn);
		return;
	}

	obj->pilot_id = pilot_id;
	obj->pilot_status = pilot_status;
}


static void list_free_string (gpointer data, gpointer user_data)
{
	free (data);
}


static CORBA_long
cal_repo_get_number_of_objects (PortableServer_Servant servant,
				GNOME_Calendar_Repository_RecordStatus record_status,
				CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	CORBA_long res;
	iCalPilotState real_record_status;
	GList *l, *uids;

	if (record_status == GNOME_Calendar_Repository_ANY) {
		/* return g_list_length(gcal->cal->events); DELETE */
                GList *uids = cal_client_get_uids (gcal->calc,
						   CALOBJ_TYPE_EVENT);
                res = g_list_length (uids);
                g_list_foreach (uids, list_free_string, NULL);
                g_list_free (uids);
                return res;
	}

	switch (record_status) {
	case GNOME_Calendar_Repository_NEW:
		real_record_status = ICAL_PILOT_SYNC_MOD;
		break;
	case GNOME_Calendar_Repository_MODIFIED:
		real_record_status = ICAL_PILOT_SYNC_MOD;
		break;
	case GNOME_Calendar_Repository_DELETED:
		real_record_status = ICAL_PILOT_SYNC_DEL;
		break;
	}

	res = 0;

	uids = cal_client_get_uids (gcal->calc, CALOBJ_TYPE_EVENT);
	for (l = uids; l; l = l->next){
		char *obj_string = cal_client_get_object (gcal->calc, l->data);
		iCalObject *obj = string_to_ical_object (obj_string);

		if (obj->pilot_status == real_record_status)
			res ++;
		g_free (l->data);
	}
	g_list_free (uids);

	return res;
}

static GNOME_Calendar_Repository_String_Sequence*
cal_repo_get_object_id_list(PortableServer_Servant servant,
			    CORBA_Environment *ev)
{
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	GList *l, *uids;
	GNOME_Calendar_Repository_String_Sequence *result;
	int counter;

	uids = cal_client_get_uids (gcal->calc, CALOBJ_TYPE_EVENT);
	
	result = GNOME_Calendar_Repository_String_Sequence__alloc();
	result->_length = g_list_length (uids);
	result->_buffer =
	  CORBA_sequence_CORBA_string_allocbuf(result->_length);

	counter = 0;
	for (l = uids; l; l = l->next){
		result->_buffer[counter] = CORBA_string_dup(l->data);
		counter++;
		g_free (l->data);
	}
	g_list_free (uids);

	return result;
}

static CORBA_char *
cal_repo_get_updated_objects (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	/* FIX ME -- this might be wrong */
	GnomeCalendar *gcal = gnomecal_from_servant (servant);
	/* Calendar *dirty_cal; DELETE */
	VObject *vcalobj, *vobj;
	GList *l, *uids;
	CORBA_char *res;
	char *str;

	/* dirty_cal = calendar_new ("Temporal",CALENDAR_INIT_NIL); DELETE */
	vcalobj = newVObject (VCCalProp);
	
	uids = cal_client_get_uids (gcal->calc, CALOBJ_TYPE_EVENT);
	for (l = uids; l; l = l->next){
		char *obj_string = cal_client_get_object (gcal->calc, l->data);
		iCalObject *obj = string_to_ical_object (obj_string);

		if (obj->pilot_status != ICAL_PILOT_SYNC_MOD)
			continue;

		/* calendar_add_object (dirty_cal, obj); DELETE */
		vobj = ical_object_to_vobject (obj);
		addVObjectProp (vcalobj, vobj);
		g_free (l->data);
	}
	g_list_free (uids);

#       if 0
	/* DELETE */
	str = calendar_get_as_vcal_string (dirty_cal);
	res = CORBA_string_dup (str);
	free (str); /* calendar_get_as_vcal_string() uses writeMemVObject(), which uses realloc() */
	calendar_destroy (dirty_cal);
#       endif /* 0 */

	str = writeMemVObject (NULL, NULL, vcalobj);
	res = CORBA_string_dup (str);
	free (str); /* calendar_get_as_vcal_string() uses writeMemVObject(),
		       which uses realloc() */
	cleanVObject (vcalobj);
	cleanStrTbl ();

	return res;
}

static void
cal_repo_done (PortableServer_Servant servant,
	       CORBA_Environment *ev)
{
	/* GnomeCalendar *gcal = gnomecal_from_servant (servant); */

	/* calendar_save (gcal->cal, NULL); DELETE */
	/* FIX ME -- i dont know what to do here */
}

static void
init_calendar_repo_class (void)
{
	calendar_repository_epv.get_object = cal_repo_get_object;
	calendar_repository_epv.get_object_by_pilot_id = cal_repo_get_object_by_pilot_id;
	calendar_repository_epv.get_id_from_pilot_id = cal_repo_get_id_from_pilot_id;
	calendar_repository_epv.delete_object = cal_repo_delete_object;
	calendar_repository_epv.update_object = cal_repo_update_object;
	calendar_repository_epv.get_number_of_objects = cal_repo_get_number_of_objects;
	calendar_repository_epv.get_updated_objects = cal_repo_get_updated_objects;
	calendar_repository_epv.update_pilot_id = cal_repo_update_pilot_id;
	calendar_repository_epv.get_object_id_list = cal_repo_get_object_id_list;
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
	calendar->calc->corba_server =
	  PortableServer_POA_servant_to_reference (poa, calendar_servant, &ev);

	CORBA_exception_free (&ev);
}
