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
#include "calobj.h"
#include "calendar.h"

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
}

void
calendar_destroy (Calendar *cal)
{
	g_list_foreach (cal->events, ical_object_destroy, NULL);
	g_list_free (cal->events);
	
	g_list_foreach (cal->todo, ical_object_destroy, NULL);
	g_list_free (cal->todo);
	
	g_list_foreach (cal->journal, ical_object_destroy, NULL);
	g_list_free (cal->journal);

	if (cal->title)
		g_free (cal->title);
	if (cal->filename)
		g_free (cal->filename);
	
	g_free (cal);
}

static GList *
calendar_get_objects_in_range (GList *objects, time_t start, time_t end)
{
	GList *new_events = 0;

	for (; objects; objects = objects->next){
		iCalObject *object = objects->data;
		
		if ((start <= object->dtstart) && (end >= object->dtend))
			new_events = g_list_prepend (new_events, object);
	}
}

GList *
calendar_get_events_in_range (Calendar *cal, time_t start, time_t end)
{
	calendar_get_objects_in_range (cal->events, start, end);
}

GList *
calendar_get_todo_in_range (Calendar *cal, time_t start, time_t end)
{
	calendar_get_objects_in_range (cal->todo, start, end);
}
GList *
calendar_get_journal_in_range (Calendar *cal, time_t start, time_t end)
{
	calendar_get_objects_in_range (cal->journal, start, end);
}
