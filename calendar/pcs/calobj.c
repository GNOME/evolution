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

	/* Alarms */
	g_free (ico->dalarm.data);
	g_free (ico->palarm.data);
	g_free (ico->malarm.data);
	g_free (ico->aalarm.data);

	g_free (ico);
}

static GList *
set_list (char *str, char *sc)
{
	GList *list = 0;
	char *s;
	
	for (s = strtok (str, sc); s; s = strtok (NULL, sc))
		list = g_list_prepend (list, g_strdup (s));
	
	return list;
}

static GList *
set_date_list (char *str)
{
	GList *list = 0;
	char *s;

	for (s = strtok (str, ";"); s; s = strtok (NULL, ";")){
		time_t *t = g_new (time_t, 1);

		*t = time_from_isodate (s);
		list = g_list_prepend (list, t);
	}
	return list;
}

static void
ignore_space(char **str)
{
	while (**str && isspace (**str))
		str++;
}

static void
skip_numbers (char **str)
{
	while (**str){
		ignore_space (str);
		if (!isdigit (**str))
			return;
		while (**str && isdigit (**str))
			;
	}
}

static void
weekdaylist (iCalObject *o, char **str)
{
	int i;
	struct {
		char first_letter, second_letter;
		int  index;
	} days [] = {
		{ 'S', 'U', 0 },
		{ 'M', 'O', 1 },
		{ 'T', 'U', 2 },
		{ 'W', 'E', 3 },
		{ 'T', 'H', 4 },
		{ 'F', 'R', 5 },
		{ 'S', 'A', 6 }
	};

	ignore_space (str);
	do {
		for (i = 0; i < 7; i++){
			if (**str == days [i].first_letter && *(*str+1) == days [i].second_letter){
				o->recur->weekday |= 1 << i;
				*str += 2;
				if (**str == ' ')
					(*str)++;
			}
		}
	} while (isalpha (**str));
}

static void
ocurrencelist (iCalObject *o, char **str)
{
	char *p;
	
	ignore_space (str);
	p = *str;
	if (!isdigit (*p))
		return;
	
	if (!(*p >= '1' && *p <= '5'))
		return;

	if (!(*(p+1) == '+' || *(p+1) == '-'))
		return;
	
	o->recur->u.month_pos = (*p-'0') * (*(p+1) == '+' ? 1 : -1);
	*str += 2;
}

static void
daynumber (iCalObject *o, char **str)
{
	int val = 0;
	char *p = *str;

	ignore_space (str);
	if (strcmp (p, "LD")){
		o->recur->u.month_day = DAY_LASTDAY;
		*str += 2;
		return;
	}
	
	if (!(isdigit (*p)))
		return;

	while (**str && isdigit (**str)){
		val = val * 10 + (**str - '0');
		str++;
	}

	if (**str == '+')
		str++;

	if (**str == '-')
		val *= -1;
	o->recur->u.month_day = val;
}

static void
daynumberlist (iCalObject *o, char **str)
{
	int first = 0;
	int val = 0;
		
	ignore_space (str);

	while (**str){
		if (!isdigit (**str))
			return;
		while (**str && isdigit (**str))
			val = 10 * val + (**str - '0');
		if (!first){
			o->recur->u.month_day = val;
			first = 1;
			val = 0;
		}
	}
}

static void
load_recur_weekly (iCalObject *o, char **str)
{
	weekdaylist (o, str);
}

static void
load_recur_monthly_pos (iCalObject *o, char **str)
{
	ocurrencelist (o, str);
	weekdaylist (o, str);
}

static void
load_recur_monthly_day (iCalObject *o, char **str)
{
	daynumberlist (o, str);
}

static void
load_recur_yearly_month (iCalObject *o, char **str)
{
	/* Skip as we do not support multiple months and we do expect
	 * the dtstart to agree with the value on this field
	 */
	skip_numbers (str);
}

static void
load_recur_yearly_day (iCalObject *o, char **str)
{
	/* Skip as we do not support multiple days and we do expect
	 * the dtstart to agree with the value on this field
	 */
	skip_numbers (str);
}

static void
duration (iCalObject *o, char **str)
{
	int duration = 0;
	
	ignore_space (str);
	if (**str != '#')
		return;
	while (**str && isdigit (**str))
		duration = duration * 10 + (**str - '0');
	
	o->recur->temp_duration = duration;
}

static void
enddate (iCalObject *o, char **str)
{
	ignore_space (str);
	if (isdigit (**str)){
		o->recur->enddate = time_from_isodate (*str);
		*str += 16;
	}
}

static int
load_recurrence (iCalObject *o, char *str)
{
	enum RecurType type;
	int  interval = 0;
	
	type = -1;
	switch (*str++){
	case 'D':
		type = RECUR_DAILY;
		break;
		
	case 'W':
		type = RECUR_WEEKLY;
		break;
		
	case 'M':
		if (*str == 'P')
			type = RECUR_MONTHLY_BY_POS;
		else if (*str == 'D')
			type = RECUR_MONTHLY_BY_DAY;
		str++;
		break;
		
	case 'Y':
		if (*str == 'M')
			type = RECUR_YEARLY_BY_MONTH;
		else if (*str == 'D')
			type = RECUR_YEARLY_BY_DAY;
		str++;
		break;
	}
	if (type == -1)
		return 0;

	o->recur = g_new0 (Recurrence, 1);
	o->recur->type = type;
	ignore_space (&str);

	/* Get the interval */
	while (*str && isdigit (*str))
		interval = interval * 10 + (*str-'0');
	o->recur->interval = interval;
	
	ignore_space (&str);
	
	switch (type){
	case RECUR_WEEKLY:
		load_recur_weekly (o, &str);
		break;
	case RECUR_MONTHLY_BY_POS:
		load_recur_monthly_pos (o, &str);
		break;
	case RECUR_MONTHLY_BY_DAY:
		load_recur_monthly_day (o, &str);
		break;
	case RECUR_YEARLY_BY_MONTH:
		load_recur_yearly_month (o, &str);
		break;
	case RECUR_YEARLY_BY_DAY:
		load_recur_yearly_day (o, &str);
		break;
	default:
		g_warning ("Unimplemented recurrence type %d", (int) type);
		break;
	}
	duration (o, &str);
	enddate (o, &str);

	return 1;
}

#define is_a_prop_of(obj,prop) isAPropertyOf (obj,prop)
#define str_val(obj) the_str = fakeCString (vObjectUStringZValue (obj))
#define has(obj,prop) (vo = isAPropertyOf (obj, prop))

/* FIXME: we need to load the recurrence properties */
iCalObject *
ical_object_create_from_vobject (VObject *o, const char *object_name)
{
	time_t  now = time (NULL);
	iCalObject *ical;
	VObject *vo;
	VObjectIterator i;
	char *the_str;

	ical = g_new0 (iCalObject, 1);
	
	if (strcmp (object_name, VCEventProp) == 0)
		ical->type = ICAL_EVENT;
	else if (strcmp (object_name, VCTodoProp) == 0)
		ical->type = ICAL_TODO;
	else
		return 0;

	/* uid */
	if (has (o, VCUniqueStringProp)){
		ical->uid = g_strdup (str_val (vo));
		free (the_str);
	}
	
	/* seq */
	if (has (o, VCSequenceProp)){
		ical->seq = atoi (str_val (vo));
		free (the_str);
	} else
		ical->seq = 0;
	
	/* dtstart */
	if (has (o, VCDTstartProp)){
		ical->dtstart = time_from_isodate (str_val (vo));
		free (the_str);
	} else
		ical->dtstart = 0;

	/* dtend */
	if (has (o, VCDTendProp)){
		ical->dtend = time_from_isodate (str_val (vo));
		free (the_str);
	} else
		ical->dtend = 0;

	/* dcreated */
	if (has (o, VCDCreatedProp)){
		ical->created = time_from_isodate (str_val (vo));
		free (the_str);
	}
	
	/* completed */
	if (has (o, VCCompletedProp)){
		ical->completed = time_from_isodate (str_val (vo));
		free (the_str);
	}
	
	/* last_mod */
	if (has (o, VCLastModifiedProp)){
		ical->last_mod = time_from_isodate (str_val (vo));
		free (the_str);
	} else
		ical->last_mod = now;

	/* exdate */
	if (has (o, VCExpDateProp)){
		ical->exdate = set_date_list (str_val (vo));
		free (the_str);
	}

	/* description/comment */
	if (has (o, VCDescriptionProp)){
		ical->comment = g_strdup (str_val (vo));
		free (the_str);
	}
	
	/* summary */
	if (has (o, VCSummaryProp)){
		ical->summary = g_strdup (str_val (vo));
		free (the_str);
	} else
		ical->summary = g_strdup ("");

	/* status */
	if (has (o, VCStatusProp)){
		ical->status = g_strdup (str_val (vo));
		free (the_str);
	} else
		ical->status = g_strdup ("NEEDS ACTION");

	if (has (o, VCClassProp)){
		ical->class = g_strdup (str_val (vo));
		free (the_str);
	} else
		ical->class = "PUBLIC";

	/* categories */
	if (has (o, VCCategoriesProp)){
		ical->categories = set_list (str_val (vo), ",");
		free (the_str);
	}
	
	/* resources */
	if (has (o, VCResourcesProp)){
		ical->resources = set_list (str_val (vo), ";");
		free (the_str);
	}
	
	/* priority */
	if (has (o, VCPriorityProp)){
		ical->priority = atoi (str_val (vo));
		free (the_str);
	}
	
	/* tranparency */
	if (has (o, VCTranspProp)){
		ical->transp = atoi (str_val (vo)) ? ICAL_TRANSPARENT : ICAL_OPAQUE;
		free (the_str);
	}

	/* related */
	if (has (o, VCRelatedToProp)){
		ical->related = set_list (str_val (vo), ";");
		free (the_str);
	}
	
	/* attach */
	initPropIterator (&i, o);
	while (moreIteration (&i)){
		vo = nextVObject (&i);
		if (strcmp (vObjectName (vo), VCAttachProp) == 0){
			ical->attach = g_list_prepend (ical->attach, g_strdup (str_val (vo)));
			free (the_str);
		}
	}

	/* url */
	if (has (o, VCURLProp)){
		ical->url = g_strdup (str_val (vo));
		free (the_str);
	}
	
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

	/* FIXME: rrule */
	if (has (o, VCRRuleProp)){
		if (!load_recurrence (ical, str_val (vo))) {
			ical_object_destroy (ical);
			return NULL;
		}
		free (the_str);
	}
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
		store_list (o, VCCategoriesProp, ical->resources, ';');

	/* priority */
	addPropValue (o, VCPriorityProp, to_str (ical->priority));

	/* transparency */
	addPropValue (o, VCTranspProp, to_str (ical->transp));

	/* related */
	store_list (o, VCRelatedToProp, ical->related, ';');

	/* attach */
	for (l = ical->attach; l; l = l->next)
		addPropValue (o, VCAttachProp, l->data);

	/* url */
	if (ical->url)
		addPropValue (o, VCURLProp, ical->url);

	/* FIXME: alarms */
	return o;
}

void
ical_foreach (GList *events, iCalObjectFn fn, void *closure)
{
	for (; events; events = events->next){
		iCalObject *ical = events->data;
		
		(*fn) (ical, ical->dtstart, ical->dtend, closure);
	}
}
