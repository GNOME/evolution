/*
 * Calendar manager object
 *
 * This keeps track of a given calendar.  Eventually this will abtract everything
 * related to getting calendars/saving calendars locally or to a remote Calendar Service
 *
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors:
 *   Miguel de Icaza (miguel@gnu.org)
 *   Federico Mena (federico@gimp.org)
 *
 */

#include <config.h>

#include "calendar.h"
#include "timeutil.h"
#include "versit/vcc.h"

Calendar *
calendar_new (char *title)
{
	Calendar *cal;

	cal = g_new0 (Calendar, 1);
	cal->title = g_strdup (title);

	return cal;
}

void
calendar_add_object (Calendar *cal, iCalObject *obj)
{
	printf ("Adding object\n");
	switch (obj->type){
	case ICAL_EVENT:
		cal->events = g_list_prepend (cal->events, obj);
		break;

	case ICAL_TODO:
		cal->todo = g_list_prepend (cal->todo, obj);
		break;

	case ICAL_JOURNAL:
		cal->journal = g_list_prepend (cal->journal, obj);
		break;
	default:
		g_assert_not_reached ();
	}

	cal->modified = TRUE;

	/* FIXME: do we have to set the last_mod field in the object? */
}

void
calendar_remove_object (Calendar *cal, iCalObject *obj)
{
	switch (obj->type){
	case ICAL_EVENT:
		cal->events = g_list_remove (cal->events, obj);
		break;

	case ICAL_TODO:
		cal->todo = g_list_remove (cal->todo, obj);
		break;

	case ICAL_JOURNAL:
		cal->journal = g_list_remove (cal->journal, obj);
		break;
	default:
		g_assert_not_reached ();
	}

	cal->modified = TRUE;
}

void
calendar_destroy (Calendar *cal)
{
	g_list_foreach (cal->events, (GFunc) ical_object_destroy, NULL);
	g_list_free (cal->events);
	
	g_list_foreach (cal->todo, (GFunc) ical_object_destroy, NULL);
	g_list_free (cal->todo);
	
	g_list_foreach (cal->journal, (GFunc) ical_object_destroy, NULL);
	g_list_free (cal->journal);

	if (cal->title)
		g_free (cal->title);
	if (cal->filename)
		g_free (cal->filename);
	
	g_free (cal);
}

char *
ice (time_t t)
{
	static char buffer [100];
	struct tm *tm;

	tm = localtime (&t);
	sprintf (buffer, "%d/%d/%d", tm->tm_mday, tm->tm_mon, tm->tm_year);
	return buffer;
}

static GList *
calendar_get_objects_in_range (GList *objects, time_t start, time_t end, GCompareFunc sort_func)
{
	GList *new_events = 0;

	for (; objects; objects = objects->next){
		iCalObject *object = objects->data;

		if ((start <= object->dtstart) && (object->dtend <= end)){
			if (sort_func)
				new_events = g_list_insert_sorted (new_events, object, sort_func);
			else
				new_events = g_list_prepend (new_events, object);
		}
	}

	return new_events;
}

GList *
calendar_get_events_in_range (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func)
{
	return calendar_get_objects_in_range (cal->events, start, end, sort_func);
}

GList *
calendar_get_todo_in_range (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func)
{
	return calendar_get_objects_in_range (cal->todo, start, end, sort_func);
}
GList *
calendar_get_journal_in_range (Calendar *cal, time_t start, time_t end, GCompareFunc sort_func)
{
	return calendar_get_objects_in_range (cal->journal, start, end, sort_func);
}

gint
calendar_compare_by_dtstart (gpointer a, gpointer b)
{
	iCalObject *obj1, *obj2;
	time_t diff;

	obj1 = a;
	obj2 = b;

	diff = obj1->dtstart - obj2->dtstart;

	return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
}

#define str_val(obj) (char *) vObjectUStringZValue (obj)

/* Loads our calendar contents from a vObject */
void
calendar_load_from_vobject (Calendar *cal, VObject *vcal)
{
	VObjectIterator i;;
	
	initPropIterator (&i, vcal);

	while (moreIteration (&i)){
		VObject *this = nextVObject (&i);
		iCalObject *ical;
		const char *object_name = vObjectName (this);

		if (strcmp (object_name, VCDCreatedProp) == 0){
			cal->created = time_from_isodate (str_val (this));
			continue;
		}
		
		if (strcmp (object_name, VCLocationProp) == 0)
			continue; /* FIXME: imlement */

		if (strcmp (object_name, VCProdIdProp) == 0)
			continue; /* FIXME: implement */

		if (strcmp (object_name, VCVersionProp) == 0)
			continue; /* FIXME: implement */
		
		ical = ical_object_create_from_vobject (this, object_name);

		if (ical)
			calendar_add_object (cal, ical);
	}
}

/* Loads a calendar from a file */
void
calendar_load (Calendar *cal, char *fname)
{
	VObject *vcal;
	
	if (cal->filename){
		g_warning ("Calendar load called again\n");
		return;
	}

	cal->filename = g_strdup (fname);
	vcal = Parse_MIME_FromFileName (fname);
	calendar_load_from_vobject (cal, vcal);
	cleanVObject (vcal);
	cleanStrTbl ();
}

void
calendar_save (Calendar *cal, char *fname)
{
	VObject *vcal;
	GList   *l;

	if (fname == NULL)
		fname = cal->filename;
	
	vcal = newVObject (VCCalProp);
	addPropValue (vcal, VCProdIdProp, "-//GNOME//NONSGML GnomeCalendar//EN");
	addPropValue (vcal, VCTimeZoneProp, "NONE");
	addPropValue (vcal, VCVersionProp, VERSION);
	cal->temp = vcal;

	for (l = cal->events; l; l = l->next){
		VObject *obj;
			
		obj = ical_object_to_vobject ((iCalObject *) l->data);
		addVObjectProp (vcal, obj);
	}
	writeVObjectToFile (fname, vcal);
	cleanVObject (vcal);
	cleanStrTbl ();
}

