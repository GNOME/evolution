/*
 * Calendar objects implementations.
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors:
 *   Miguel de Icaza (miguel@gnu.org)
 *   Federico Mena (federico@gimp.org)
 */
#include <string.h>
#include <glib.h>
#include "calobj.h"
#include "timeutil.h"
#include "versit/vcc.h"

iCalObject *
ical_object_new (void)
{
	iCalObject *ico;

	ico = g_new0 (iCalObject, 1);
	
	ico->seq = -1;
	ico->dtstamp = time (NULL);

	return ico;
}

static void
default_alarm (iCalObject *ical, CalendarAlarm *alarm, char *def_mail, enum AlarmType type)
{
	alarm->enabled = 0;
	alarm->type    = type;

	if (type != ALARM_MAIL){
		alarm->count   = 15;
		alarm->units   = ALARM_MINUTES;
	} else {
		printf ("uno!\n");
		alarm->count   = 1;
		alarm->units   = ALARM_DAYS;
	}

	if (type == ALARM_MAIL)
		alarm->data = g_strdup (def_mail);
	else
		alarm->data = g_strdup ("");
}

iCalObject *
ical_new (char *comment, char *organizer, char *summary)
{
	iCalObject *ico;

	ico = ical_object_new ();

	ico->comment   = g_strdup (comment);
	ico->organizer = g_strdup (organizer);
	ico->summary   = g_strdup (summary);
	ico->class     = g_strdup ("PUBLIC");

	default_alarm  (ico, &ico->dalarm, organizer, ALARM_DISPLAY);
	default_alarm  (ico, &ico->palarm, organizer, ALARM_PROGRAM);
	default_alarm  (ico, &ico->malarm, organizer, ALARM_MAIL);
	default_alarm  (ico, &ico->aalarm, organizer, ALARM_AUDIO);
	
	return ico;
}

static void
my_free (gpointer data, gpointer user_dat_ignored)
{
	g_free (data);
}

static void
list_free (GList *list)
{
	g_list_foreach (list, my_free, 0);
	g_list_free (list);
}

#define free_if_defined(x) if (x){ g_free (x); x = 0; }
#define lfree_if_defined(x) if (x){ list_free (x); x = 0; }
void
ical_object_destroy (iCalObject *ico)
{
	/* Regular strings */
	free_if_defined  (ico->comment);
	free_if_defined  (ico->organizer);
	free_if_defined  (ico->summary);
	free_if_defined  (ico->uid);
	free_if_defined  (ico->status);
	free_if_defined  (ico->class);
	free_if_defined  (ico->url);

	/* Lists */
	lfree_if_defined (ico->exdate);
	lfree_if_defined (ico->categories);
	lfree_if_defined (ico->resources);
	lfree_if_defined (ico->related);
	lfree_if_defined (ico->attach);
	
	g_free (ico);
}

GList *
set_list (char *str, char *sc)
{
	GList *list = 0;
	char *s;
	
	for (s = strtok (str, sc); s; s = strtok (NULL, sc))
		list = g_list_prepend (list, g_strdup (s));
	
	return list;
}

#define is_a_prop_of(obj,prop) isAPropertyOf (obj,prop)
#define str_val(obj) fakeCString (vObjectUStringZValue (obj))
#define has(obj,prop) (vo = isAPropertyOf (obj, prop))

/* FIXME: we need to load the recurrence properties */
iCalObject *
ical_object_create_from_vobject (VObject *o, const char *object_name)
{
	time_t  now = time (NULL);
	iCalObject *ical;
	VObject *vo;
	VObjectIterator i;

	ical = g_new0 (iCalObject, 1);
	
	if (strcmp (object_name, VCEventProp) == 0)
		ical->type = ICAL_EVENT;
	else if (strcmp (object_name, VCTodoProp) == 0)
		ical->type = ICAL_TODO;
	else
		return 0;

	/* uid */
	if (has (o, VCUniqueStringProp))
		ical->uid = g_strdup (str_val (vo));

	/* seq */
	if (has (o, VCSequenceProp))
		ical->seq = atoi (str_val (vo));
	else
		ical->seq = 0;
	
	/* dtstart */
	if (has (o, VCDTstartProp))
		ical->dtstart = time_from_isodate (str_val (vo));
	else
		ical->dtstart = 0;

	/* dtend */
	if (has (o, VCDTendProp))
		ical->dtend = time_from_isodate (str_val (vo));
	else
		ical->dtend = 0;

	/* dcreated */
	if (has (o, VCDCreatedProp))
		ical->created = time_from_isodate (str_val (vo));
	
	/* completed */
	if (has (o, VCCompletedProp))
		ical->completed = time_from_isodate (str_val (vo));
	
	/* last_mod */
	if (has (o, VCLastModifiedProp))
		ical->last_mod = time_from_isodate (str_val (vo));
	else
		ical->last_mod = now;

	/* exdate */
	if (has (o, VCExpDateProp))
		ical->exdate = set_list (str_val (vo), ",");

	/* description/comment */
	if (has (o, VCDescriptionProp))
		ical->comment = g_strdup (str_val (vo));
	
	/* summary */
	if (has (o, VCSummaryProp))
		ical->summary = g_strdup (str_val (vo));
	else
		ical->summary = g_strdup ("");

	/* status */
	if (has (o, VCStatusProp))
		ical->status = g_strdup (str_val (vo));
	else
		ical->status = g_strdup ("NEEDS ACTION");

	if (has (o, VCClassProp))
		ical->class = g_strdup (str_val (vo));
	else
		ical->class = "PUBLIC";

	/* categories */
	if (has (o, VCCategoriesProp))
		ical->categories = set_list (str_val (vo), ",");

	/* resources */
	if (has (o, VCResourcesProp))
		ical->resources = set_list (str_val (vo), ";");
	
	/* priority */
	if (has (o, VCPriorityProp))
		ical->priority = atoi (str_val (vo));

	/* tranparency */
	if (has (o, VCTranspProp))
		ical->transp = atoi (str_val (vo)) ? ICAL_TRANSPARENT : ICAL_OPAQUE;

	/* related */
	if (has (o, VCRelatedToProp))
		ical->related = set_list (str_val (vo), ";");

	/* attach */
	initPropIterator (&i, o);
	while (moreIteration (&i)){
		vo = nextVObject (&i);
		if (strcmp (vObjectName (vo), VCAttachProp) == 0)
			ical->attach = g_list_prepend (ical->attach, g_strdup (str_val (vo)));
	}

	/* url */
	if (has (o, VCURLProp))
		ical->url = g_strdup (str_val (vo));
	
	/* FIXME: dalarm */
	if (has (o, VCDAlarmProp))
		;
	
	/* FIXME: aalarm */
	if (has (o, VCAAlarmProp))
		;

	/* FIXME: palarm */
	if (has (o, VCPAlarmProp))
		;

	/* FIXME: malarm */
	if (has (o, VCMAlarmProp))
		;

	/* FIXME: rdate */
	if (has (o, VCRDateProp))
		;

	/* FIXME: rrule */
	if (has (o, VCRRuleProp))
		;
		
	return ical;
}

static char *
to_str (int num)
{
	static char buf [40];

	sprintf (buf, "%d", num);
	return buf;
}

/*
 * stores a GList in the property, using SEP as the value separator
 */
static void
store_list (VObject *o, char *prop, GList *values, char sep)
{
	GList *l;
	int len;
	char *result, *p;
	
	for (len = 0, l = values; l; l = l->next)
		len += strlen (l->data) + 1;

	result = g_malloc (len);
	for (p = result, l = values; l; l = l->next){
		int len = strlen (l->data);
		
		strcpy (p, l->data);
		p [len] = sep;
		p += len+1;
	}
	addPropValue (o, prop, result);
	g_free (p);
}

VObject *
ical_object_to_vobject (iCalObject *ical)
{
	VObject *o;
	GList *l;
	
	if (ical->type == ICAL_EVENT)
		o = newVObject (VCEventProp);
	else
		o = newVObject (VCTodoProp);

	/* uid */
	if (ical->uid)
		addPropValue (o, VCUniqueStringProp, ical->uid);

	/* seq */
	addPropValue (o, VCSequenceProp, to_str (ical->seq));

	/* dtstart */
	addPropValue (o, VCDTstartProp, isodate_from_time_t (ical->dtstart));

	/* dtend */
	addPropValue (o, VCDTendProp, isodate_from_time_t (ical->dtend));

	/* dcreated */
	addPropValue (o, VCDTendProp, isodate_from_time_t (ical->created));

	/* completed */
	if (ical->completed)
		addPropValue (o, VCDTendProp, isodate_from_time_t (ical->completed));

	/* last_mod */
	addPropValue (o, VCLastModifiedProp, isodate_from_time_t (ical->last_mod));

	/* exdate */
	if (ical->exdate)
		store_list (o, VCExpDateProp, ical->exdate, ',');

	/* description/comment */
	if (ical->comment)
		addPropValue (o, VCDescriptionProp, ical->comment);

	/* summary */
	if (ical->summary)
		addPropValue (o, VCSummaryProp, ical->summary);
	
	/* status */
	addPropValue (o, VCStatusProp, ical->status);

	/* class */
	addPropValue (o, VCClassProp, ical->class);

	/* categories */
	if (ical->categories)
		store_list (o, VCCategoriesProp, ical->categories, ',');

	/* resources */
	if (ical->categories)
		store_list (o, VCCategoriesProp, ical->resources, ";");

	/* priority */
	addPropValue (o, VCPriorityProp, to_str (ical->priority));

	/* transparency */
	addPropValue (o, VCTranspProp, to_str (ical->transp));

	/* related */
	store_list (o, VCRelatedToProp, ical->related, ";");

	/* attach */
	for (l = ical->attach; l; l = l->next)
		addPropValue (o, VCAttachProp, l->data);

	/* url */
	if (ical->url)
		addPropValue (o, VCURLProp, ical->url);

	/* FIXME: alarms */
	return o;
}
	
