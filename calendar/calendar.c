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
 *   Federico Mena (quartic@gimp.org)
 *
 */

#include <config.h>
#include <unistd.h>
#include <sys/stat.h>
#include "calendar.h"
#include "alarm.h"
#include "timeutil.h"
#include "../libversit/vcc.h"

#ifdef HAVE_TZNAME
extern char *tzname[2];
#endif

/* Our day range */
time_t calendar_day_begin, calendar_day_end;

static void calendar_init_alarms (Calendar *cal);

Calendar *
calendar_new (char *title)
{
	Calendar *cal;

	cal = g_new0 (Calendar, 1);

	cal->title = g_strdup (title);

	calendar_init_alarms (cal);
	
	return cal;
}

static void
try_add (iCalObject *ico, CalendarAlarm *alarm, time_t start, time_t end)
{
	alarm->trigger = start-alarm->offset;
	
	if (alarm->trigger < calendar_day_begin)
		return;
	if (alarm->trigger > calendar_day_end)
		return;
	alarm_add (alarm, &calendar_notify, ico);
}

static int
add_object_alarms (iCalObject *obj, time_t start, time_t end, void *closure)
{
	if (obj->aalarm.enabled)
		try_add (obj, &obj->aalarm, start, end);
	if (obj->dalarm.enabled)
		try_add (obj, &obj->dalarm, start, end);
	if (obj->palarm.enabled)
		try_add (obj,&obj->palarm, start, end);
	if (obj->malarm.enabled)
		try_add (obj, &obj->malarm, start, end);
	
	return TRUE;
}

#define max(a,b) ((a > b) ? a : b)

void
ical_object_try_alarms (iCalObject *obj)
{
	int ao, po, od, mo;
	int max_o;
	
	ao = alarm_compute_offset (&obj->aalarm);
	po = alarm_compute_offset (&obj->palarm);
	od = alarm_compute_offset (&obj->dalarm);
	mo = alarm_compute_offset (&obj->malarm);
	
	max_o = max (ao, max (po, max (od, mo)));
	if (max_o == -1)
		return;
	
	ical_object_generate_events (obj, calendar_day_begin, calendar_day_end + max_o, add_object_alarms, obj);
}

void
calendar_add_object (Calendar *cal, iCalObject *obj)
{
	obj->new = 0;
	switch (obj->type){
	case ICAL_EVENT:
		cal->events = g_list_prepend (cal->events, obj);
		ical_object_try_alarms (obj);
#ifdef DEBUGGING_MAIL_ALARM
		obj->malarm.trigger = 0;
		calendar_notify (0, obj);
#endif
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

	if (!obj->uid){
		char buffer [80];

		snprintf (buffer, sizeof (buffer), "GnomeCalendar-%ld\n", time (NULL));
		obj->uid = g_strdup (buffer);
	}
	
	cal->modified = TRUE;

	obj->last_mod = time (NULL);
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
	struct tm tm;

	tm = *localtime (&t);
	sprintf (buffer, "%d/%d/%d", tm.tm_mday, tm.tm_mon, tm.tm_year);
	return buffer;
}

void
calendar_iterate_on_objects (GList *objects, time_t start, time_t end, calendarfn cb, void *closure)
{
	for (; objects; objects = objects->next){
		iCalObject *object = objects->data;

		ical_object_generate_events (object, start, end, cb, closure);
	}
}

void
calendar_iterate (Calendar *cal, time_t start, time_t end, calendarfn cb, void *closure)
{
	calendar_iterate_on_objects (cal->events, start, end, cb, closure);
}

GList *
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

	/* Put the list in increasing order if no sort function was specified */

	if (!sort_func)
		new_events = g_list_reverse (new_events);

	return new_events;
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
	VObjectIterator i;
	
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

		if (strcmp (object_name, VCTimeZoneProp) == 0)
			continue; /* FIXME: implement */
		
		ical = ical_object_create_from_vobject (this, object_name);

		if (ical)
			calendar_add_object (cal, ical);
	}
}

static void
calendar_set_day (void)
{
	time_t calendar_today;
	
	calendar_today     = time (NULL);
	calendar_day_begin = time_day_begin (calendar_today);
	calendar_day_end   = time_day_end (calendar_today);
}

/* Loads a calendar from a file */
char *
calendar_load (Calendar *cal, char *fname)
{
	VObject *vcal;
	struct stat s;
	
	if (cal->filename){
		g_warning ("Calendar load called again\n");
		return "Internal error";
	}

	cal->filename = g_strdup (fname);
	vcal = Parse_MIME_FromFileName (fname);
	if (!vcal)
		return "Could not load the calendar";

	stat (fname, &s);
	cal->file_time = s.st_mtime;

	calendar_set_day ();
		
	calendar_load_from_vobject (cal, vcal);
	cleanVObject (vcal);
	cleanStrTbl ();
	return NULL;
}

void
calendar_save (Calendar *cal, char *fname)
{
	VObject *vcal;
	GList   *l;
	time_t  now = time (NULL);
	struct  stat s;
	struct tm tm;
	
	if (fname == NULL)
		fname = cal->filename;

	/* We call localtime for the side effect of setting tzname */
	tm = *localtime (&now);
	
	vcal = newVObject (VCCalProp);
	addPropValue (vcal, VCProdIdProp, "-//GNOME//NONSGML GnomeCalendar//EN");
#if defined(HAVE_TM_ZONE)
	addPropValue (vcal, VCTimeZoneProp, tm.tm_zone);
#elif defined(HAVE_TZNAME)
	addPropValue (vcal, VCTimeZoneProp, tzname[0]);
#endif
	addPropValue (vcal, VCVersionProp, VERSION);
	cal->temp = vcal;

	/* Events */

	for (l = cal->events; l; l = l->next) {
		VObject *obj;
			
		obj = ical_object_to_vobject ((iCalObject *) l->data);
		addVObjectProp (vcal, obj);
	}

	/* To-do entries */

	for (l = cal->todo; l; l = l->next) {
		VObject *obj;

		obj = ical_object_to_vobject ((iCalObject *) l->data);
		addVObjectProp (vcal, obj);
	}

	if (g_file_exists (fname)){
		char *backup_name = g_strconcat (fname, "~", NULL);

		if (g_file_exists (backup_name)){
			unlink (backup_name);
		}
		rename (fname, backup_name);
		g_free (backup_name);
	}
	writeVObjectToFile (fname, vcal);

	stat (fname, &s);
	cal->file_time = s.st_mtime;
	
	cleanVObject (vcal);
	cleanStrTbl ();
}

static gint
calendar_object_compare_by_start (gconstpointer a, gconstpointer b)
{
	const CalendarObject *ca = a;
	const CalendarObject *cb = b;
	time_t diff;
	
	diff = ca->ev_start - cb->ev_start;
	return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
}

static int
assemble_event_list (iCalObject *obj, time_t start, time_t end, void *c)
{
	CalendarObject *co;
	GList **l = c;
	
	co = g_new (CalendarObject, 1);

	co->ev_start = start;
	co->ev_end   = end;
	co->ico      = obj;
	*l = g_list_insert_sorted (*l, co, calendar_object_compare_by_start);

	return 1;
}

void
calendar_destroy_event_list (GList *l)
{
	GList *p;

	for (p = l; p; p = p->next)
		g_free (p->data);
	g_list_free (l);
}

GList *
calendar_get_events_in_range (Calendar *cal, time_t start, time_t end)
{
	GList *l = 0;
	
	calendar_iterate (cal, start, end, assemble_event_list, &l);
	return l;
}

void
calendar_object_changed (Calendar *cal, iCalObject *obj, int flags)
{
	obj->last_mod = time (NULL);
	
	if (!(flags & CHANGE_DATES))
		return;
	
	/* Remove any alarms on the alarm list for this object */
	while (alarm_kill (obj))
		;
	ical_object_try_alarms (obj);
}

static void
calendar_day_change (time_t time, CalendarAlarm *which, void *closure)
{
	GList *events;
	Calendar *cal = closure;
	
	calendar_set_day ();

	for (events = cal->events; events; events = events->next){
		iCalObject *obj = events->data;

		ical_object_try_alarms (obj);
	}
}

static void
calendar_init_alarms (Calendar *cal)
{
	CalendarAlarm day_change_alarm;

	day_change_alarm.trigger = calendar_day_end;
	alarm_add (&day_change_alarm, calendar_day_change, cal);
}

