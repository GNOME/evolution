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

iCalObject *
ical_new (char *comment, char *organizer, char *summary)
{
	iCalObject *ico;

	ico = ical_object_new ();

	ico->comment   = g_strdup (comment);
	ico->organizer = g_strdup (organizer);
	ico->summary   = g_strdup (summary);

	return ico;
}

#define free_if_defined(x) if (x){ g_free (x); x = 0; }

void
ical_object_destroy (iCalObject *ico)
{
	free_if_defined (ico->comment);
	free_if_defined (ico->organizer);
	free_if_defined (ico->summary);
	
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
#define str_val(obj) (char *) vObjectUStringZValue (obj)
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

	/* description */
	if (has (o, VCDescriptionProp))
		ical->description = g_strdup (str_val (vo));
	
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
		ical->status = g_strdup ("PUBLIC");

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

void
ical_object_save (iCalObject *ical)
{
	VObject *o;

	if (ical->type == ICAL_EVENT)
		o = newVObject (VCEventProp);
	else
		o = newVObject (VCTodoProp);
}
	
