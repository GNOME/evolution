/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Calendar objects implementations.
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors:
 *   Miguel de Icaza (miguel@gnu.org)
 *   Federico Mena (quartic@gimp.org)
 */
#include <config.h>
#include <string.h>
#include <glib.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include "calobj.h"
#include "timeutil.h"
#include "libversit/vcc.h"
#include "icalendar-save.h"
#include "icalendar.h"



/* VCalendar product ID */
#define PRODID "-//Helix Code//NONSGML Evolution Calendar//EN"

static gint compare_exdates (gconstpointer a, gconstpointer b);
static void ical_object_normalize_summary (iCalObject *ico);



char *
ical_gen_uid (void)
{
	static char *hostname;
	time_t t = time (NULL);
	static int serial;

	if (!hostname){
		char buffer [128];

		if ((gethostname (buffer, sizeof (buffer)-1) == 0) &&
		    (buffer [0] != 0))
			hostname = g_strdup (buffer);
		else
			hostname = g_strdup ("localhost");
	}

	return g_strdup_printf (
		"%s-%d-%d-%d-%d@%s",
		isodate_from_time_t (t),
		getpid (),
		getgid (),
		getppid (),
		serial++,
		hostname);
}

iCalObject *
ical_object_new (void)
{
	iCalObject *ico;

	ico = g_new0 (iCalObject, 1);

	ico->seq = -1;
	ico->dtstamp = time (NULL);
	ico->uid = ical_gen_uid ();

	ico->pilot_id = 0;
	ico->pilot_status = ICAL_PILOT_SYNC_MOD;

	ico->ref_count = 1;

	return ico;
}

iCalObject *
ical_new (char *comment, char *organizer, char *summary)
{
	iCalObject *ico;

	ico = ical_object_new ();

	ico->comment   = g_strdup (comment);
	ico->organizer = g_new0 (iCalPerson, 1);
	ico->organizer->addr = g_strdup (organizer);
	ico->summary   = g_strdup (summary);
	ico->class     = g_strdup ("PUBLIC");
	ico->status    = g_strdup ("NEEDS ACTION");

	ico->dalarm.type = ALARM_DISPLAY;
	ico->palarm.type = ALARM_PROGRAM;
	ico->malarm.type = ALARM_MAIL;
	ico->aalarm.type = ALARM_AUDIO;

	ical_object_normalize_summary (ico);

	return ico;
}


void
ical_object_ref (iCalObject *ico)
{
	ico->ref_count++;
}


#define free_if_defined(x) if (x){ g_free (x); x = 0; }
#define lfree_if_defined(x) if (x){ list_free (x); x = 0; }
static void
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
	free_if_defined  (ico->recur);

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

void
ical_object_unref (iCalObject *ico)
{
	ico->ref_count--;
	if (ico->ref_count == 0)
		ical_object_destroy (ico);
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

/* This resets any recurrence rules of the iCalObject. */
void
ical_object_reset_recurrence (iCalObject *ico)
{
	free_if_defined  (ico->recur);
	lfree_if_defined (ico->exdate);
}

static GList *
set_list (char *str)
{
	GList *list = 0;
	char *s;

	for (s = strtok (str, ";"); s; s = strtok (NULL, ";"))
		list = g_list_prepend (list, g_strdup (s));

	return list;
}

static GList *
set_date_list (char *str)
{
	GList *list = 0;
	char *s;

	for (s = strtok (str, ";,"); s; s = strtok (NULL, ";,")){
		time_t *t = g_new (time_t, 1);

		while (*s && isspace (*s))
			s++;
		*t = time_from_isodate (s);
		list = g_list_prepend (list, t);
	}
	return list;
}

void
ical_object_add_exdate (iCalObject *o, time_t t)
{
	time_t *pt = g_new (time_t, 1);

	*pt = t;
	o->exdate = g_list_prepend (o->exdate, pt);
}

static void
ignore_space(char **str)
{
	while (**str && isspace (**str))
		(*str)++;
}

static void
skip_numbers (char **str)
{
	while (**str){
		ignore_space (str);
		if (!isdigit (**str))
			return;
		while (**str && isdigit (**str))
			(*str)++;
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

	if (o->recur->weekday == 0){
		struct tm tm = *localtime (&o->dtstart);

		o->recur->weekday = 1 << tm.tm_wday;
	}
}

static void
weekdaynum (iCalObject *o, char **str)
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
				o->recur->weekday = i;
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

#if 0

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
		(*str)++;
	}

	if (**str == '+')
		(*str)++;

	if (**str == '-')
		val *= -1;
	o->recur->u.month_day = val;
}

#endif

static void
daynumberlist (iCalObject *o, char **str)
{
	int first = 0;
	int val = 0;

	ignore_space (str);

	while (**str){
		if (!isdigit (**str))
			return;
		while (**str && isdigit (**str)){
			val = 10 * val + (**str - '0');
			(*str)++;
		}
		if (!first){
			/*
			 * Some broken applications set this to zero
			 */
			if (val == 0){
				struct tm day = *localtime (&o->dtstart);

				val = day.tm_mday;
			}
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
	weekdaynum (o, str);
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
	 *
	 * FIXME: we should support every-n-years
	 */
	skip_numbers (str);
}

static void
duration (iCalObject *o, char **str)
{
	unsigned int duration = 0;

	ignore_space (str);
	if (**str != '#')
		return;
	(*str)++;
	while (**str && isdigit (**str)){
		duration = duration * 10 + (**str - '0');
		(*str)++;
	}
	o->recur->duration = duration;
}

static void
enddate (iCalObject *o, char **str)
{
	ignore_space (str);
	if (isdigit (**str)){
		o->recur->_enddate = time_from_isodate (*str);
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
	for (;*str && isdigit (*str);str++)
		interval = interval * 10 + (*str-'0');

	if (interval == 0)
		interval = 1;

	o->recur->interval = interval;

	/* this is the default per the spec */
	o->recur->duration = 2;

	ignore_space (&str);

	switch (type){
	case RECUR_DAILY:
		break;
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

	/* Compute the enddate */
	if (o->recur->_enddate == 0){
		if (o->recur->duration != 0){
			ical_object_compute_end (o);
		} else
			o->recur->enddate = 0;
	} else {
		o->recur->enddate = o->recur->_enddate;
	}
	return 1;
}

#define is_a_prop_of(obj,prop) isAPropertyOf (obj,prop)
#define str_val(obj) the_str = fakeCString (vObjectUStringZValue (obj))
#define has(obj,prop) (vo = isAPropertyOf (obj, prop))

/*
 * FIXME: This is loosing precission.  Enhanec the thresholds
 */
#define HOURS(n) (n*(60*60))

static void
setup_alarm_at (iCalObject *ico, CalendarAlarm *alarm, char *iso_time, VObject *vo)
{
	time_t alarm_time = time_from_isodate (iso_time);
	time_t base = ico->dtstart;
	int d = difftime (base, alarm_time);
	VObject *a;
	char *the_str;

	alarm->enabled = 1;
	if (d > HOURS (2)){
		if (d > HOURS (48)){
			alarm->count = d / HOURS (24);
			alarm->units = ALARM_DAYS;
		} else {
			alarm->count = d / (60*60);
			alarm->units = ALARM_HOURS;
		}
	} else {
		alarm->count = d / 60;
		alarm->units = ALARM_MINUTES;
	}

	if ((a = is_a_prop_of (vo, VCSnoozeTimeProp))){
		alarm->snooze_secs = isodiff_to_secs (str_val (a));
		free (the_str);
	}

	if ((a = is_a_prop_of (vo, VCRepeatCountProp))){
		alarm->snooze_repeat = atoi (str_val (a));
		free (the_str);
	}
}

/*
 * Duplicates an iCalObject.  Implementation is a grand hack.
 * If you need the new ICalObject to have a new uid, free the current one,
 * and call ical_gen_uid() to generate a new one.
 */
iCalObject *
ical_object_duplicate (iCalObject *o)
{
	VObject *vo;
	iCalObject *new;

	vo = ical_object_to_vobject (o);
	switch (o->type){
	case ICAL_EVENT:
		new = ical_object_create_from_vobject (vo, VCEventProp);
		break;
	case ICAL_TODO:
		new = ical_object_create_from_vobject (vo, VCTodoProp);
		break;
	default:
		new = NULL;
	}

	cleanVObject (vo);
	return new;
}

/* FIXME: we need to load the recurrence properties */
iCalObject *
ical_object_create_from_vobject (VObject *o, const char *object_name)
{
	time_t  now = time (NULL);
	iCalObject *ical;
	VObject *vo, *a;
	VObjectIterator i;
	char *the_str;

	ical = g_new0 (iCalObject, 1);

	if (strcmp (object_name, VCEventProp) == 0)
		ical->type = ICAL_EVENT;
	else if (strcmp (object_name, VCTodoProp) == 0)
		ical->type = ICAL_TODO;
	else {
		g_free (ical);
		return 0;
	}

	ical->ref_count = 1;

	/* uid */
	if (has (o, VCUniqueStringProp)){
		ical->uid = g_strdup (str_val (vo));
		free (the_str);
	} else {
		ical->uid = ical_gen_uid ();
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
	ical->dtend = 0;	/* default value */
	if (ical->type == ICAL_EVENT){
		if (has (o, VCDTendProp)){
			ical->dtend = time_from_isodate (str_val (vo));
			free (the_str);
		}
	} else if (ical->type == ICAL_TODO){
		if (has (o, VCDueProp)){
			ical->dtend = time_from_isodate (str_val (vo));
			free (the_str);
		}
	}

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

		/* Convert any CR/LF/CRLF sequences in the summary field to
		   spaces so we just have a one-line field. */
		ical_object_normalize_summary (ical);
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
		ical->class = g_strdup ("PUBLIC");

	/* categories */
	if (has (o, VCCategoriesProp)){
		ical->categories = set_list (str_val (vo));
		free (the_str);
	}

	/* resources */
	if (has (o, VCResourcesProp)){
		ical->resources = set_list (str_val (vo));
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

	/* Organizer */
	if (has (o, VCOrgNameProp)){
		ical->organizer = g_new0 (iCalPerson, 1);
		ical->organizer->addr = g_strdup (str_val (vo));
		free (the_str);
	}

	/* related */
	if (has (o, VCRelatedToProp)){
		char *str;
		char *s;
		iCalRelation *rel;
		str = str_val (vo);
		for (s = strtok (str, ";"); s; s = strtok (NULL, ";")) {
			rel = g_new0 (iCalRelation, 1);
			rel->uid = g_strdup (s);
			rel->reltype = g_strdup ("PARENT");
			ical->related = g_list_prepend (ical->related, rel);
		}
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

	/* dalarm */
	ical->dalarm.type = ALARM_DISPLAY;
	ical->dalarm.enabled = 0;
	if (has (o, VCDAlarmProp)){
		if ((a = is_a_prop_of (vo, VCRunTimeProp))){
			setup_alarm_at (ical, &ical->dalarm, str_val (a), vo);
			free (the_str);
		}
	}

	/* aalarm */
	ical->aalarm.type = ALARM_AUDIO;
	ical->aalarm.enabled = 0;
	if (has (o, VCAAlarmProp)){
		if ((a = is_a_prop_of (vo, VCRunTimeProp))){
			setup_alarm_at (ical, &ical->aalarm, str_val (a), vo);
			free (the_str);
		}
	}

	/* palarm */
	ical->palarm.type = ALARM_PROGRAM;
	ical->palarm.enabled = 0;
	if (has (o, VCPAlarmProp)){
		ical->palarm.type = ALARM_PROGRAM;
		if ((a = is_a_prop_of (vo, VCRunTimeProp))){
			setup_alarm_at (ical, &ical->palarm, str_val (a), vo);
			free (the_str);

			if ((a = is_a_prop_of (vo, VCProcedureNameProp))){
				ical->palarm.data = g_strdup (str_val (a));
				free (the_str);
			} else
				ical->palarm.data = g_strdup ("");
		}
	}

	/* malarm */
	ical->malarm.type = ALARM_MAIL;
	ical->malarm.enabled = 0;
	if (has (o, VCMAlarmProp)){
		ical->malarm.type = ALARM_MAIL;
		if ((a = is_a_prop_of (vo, VCRunTimeProp))){
			setup_alarm_at (ical, &ical->malarm, str_val (a), vo);
			free (the_str);

			if ((a = is_a_prop_of (vo, VCEmailAddressProp))){
				ical->malarm.data = g_strdup (str_val (a));
				free (the_str);
			} else
				ical->malarm.data = g_strdup ("");
		}
	}

	/* rrule */
	if (has (o, VCRRuleProp)){
		if (!load_recurrence (ical, str_val (vo))) {
			ical_object_unref (ical);
			return NULL;
		}
		free (the_str);
	}

	/*
	 * Pilot
	 */
	if (has (o, XPilotIdProp)){
		ical->pilot_id = atoi (str_val (vo));
		free (the_str);
	} else
		ical->pilot_id = 0;

	if (has (o, XPilotStatusProp)){
		ical->pilot_status = atoi (str_val (vo));
		free (the_str);
	} else
		ical->pilot_status = ICAL_PILOT_SYNC_MOD;

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
 * stores a GList in the property.
 */
static void
store_list (VObject *o, char *prop, GList *values)
{
	GList *l;
	int len;
	char *result, *p;

	for (len = 0, l = values; l; l = l->next)
		len += strlen (l->data) + 1;

	result = g_malloc (len);

	for (p = result, l = values; l; l = l->next) {
		int len = strlen (l->data);

		strcpy (p, l->data);

		if (l->next) {
			p [len] = ';';
			p += len+1;
		} else
			p += len;
	}

	*p = 0;

	addPropValue (o, prop, result);
	g_free (result);
}

static void
store_rel_list (VObject *o, char *prop, GList *values)
{
	GList *l;
	int len;
	char *result, *p;
	
	for (len = 0, l = values; l; l = l->next)
		len += strlen (((iCalRelation*)(l->data))->uid) + 1;

	result = g_malloc (len);

	for (p = result, l = values; l; l = l->next) {
		int len = strlen (((iCalRelation*)(l->data))->uid);
		
		strcpy (p, ((iCalRelation*)(l->data))->uid);

		if (l->next) {
			p [len] = ';';
			p += len+1;
		} else
			p += len;
	}

	*p = 0;

	addPropValue (o, prop, result);
	g_free (result);
}

static void
store_date_list (VObject *o, char *prop, GList *values)
{
	GList *l;
	int   size, len;
	char  *s, *p;

	size = g_list_length (values);
	s = p = g_malloc ((size * 17 + 1) * sizeof (char));

	for (l = values; l; l = l->next){
		strcpy (s, isodate_from_time_t (*(time_t *)l->data));
		len = strlen (s);
		s [len] = ',';
		s += len + 1;
	}
	s--;
	*s = 0;
	addPropValue (o, prop, p);
	g_free (p);
}

static char *recur_type_name [] = { "D", "W", "MP", "MD", "YM", "YD" };
static char *recur_day_list  [] = { "SU", "MO", "TU","WE", "TH", "FR", "SA" };
static char *alarm_names [] = { VCMAlarmProp, VCPAlarmProp, VCDAlarmProp, VCAAlarmProp };

static VObject *
save_alarm (VObject *o, CalendarAlarm *alarm, iCalObject *ical)
{
	VObject *alarm_object;
	struct tm tm;
	time_t alarm_time;

	if (!alarm->enabled)
		return NULL;
	tm = *localtime (&ical->dtstart);
	switch (alarm->units){
	case ALARM_MINUTES:
		tm.tm_min -= alarm->count;
		break;

	case ALARM_HOURS:
		tm.tm_hour -= alarm->count;
		break;

	case ALARM_DAYS:
		tm.tm_mday -= alarm->count;
		break;
	}

	alarm_time = mktime (&tm);
	alarm_object = addProp (o, alarm_names [alarm->type]);
	addPropValue (alarm_object, VCRunTimeProp, isodate_from_time_t (alarm_time));

	if (alarm->snooze_secs)
		addPropValue (alarm_object, VCSnoozeTimeProp, isodiff_from_secs (alarm->snooze_secs));
	else
		addPropValue (alarm_object, VCSnoozeTimeProp, "");

	if (alarm->snooze_repeat){
		char buf [20];

		sprintf (buf, "%d", alarm->snooze_repeat);
		addPropValue (alarm_object, VCRepeatCountProp, buf);
	} else
		addPropValue (alarm_object, VCRepeatCountProp, "");
	return alarm_object;
}

VObject *
ical_object_to_vobject (iCalObject *ical)
{
	VObject *o, *alarm, *s;
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
	if (ical->type == ICAL_EVENT){
		addPropValue (o, VCDTendProp, isodate_from_time_t (ical->dtend));
	} else if (ical->type == ICAL_TODO){
		addPropValue (o, VCDueProp, isodate_from_time_t (ical->dtend));
	}

	/* dcreated */
	addPropValue (o, VCDCreatedProp, isodate_from_time_t (ical->created));

	/* completed */
	if (ical->completed)
		addPropValue (o, VCDTendProp, isodate_from_time_t (ical->completed));

	/* last_mod */
	addPropValue (o, VCLastModifiedProp, isodate_from_time_t (ical->last_mod));

	/* exdate */
	if (ical->exdate)
		store_date_list (o, VCExpDateProp, ical->exdate);

	/* description/comment */
	if (ical->comment && strlen (ical->comment)){
		s = addPropValue (o, VCDescriptionProp, ical->comment);
		if (strchr (ical->comment, '\n'))
			addProp (s, VCQuotedPrintableProp);
	}

	/* summary */
	if (strlen (ical->summary)){
		s = addPropValue (o, VCSummaryProp, ical->summary);
		if (strchr (ical->summary, '\n'))
			addProp (s, VCQuotedPrintableProp);
	}
		
	/* status */
	addPropValue (o, VCStatusProp, ical->status);

	/* class */
	addPropValue (o, VCClassProp, ical->class);

	/* categories */
	if (ical->categories)
		store_list (o, VCCategoriesProp, ical->categories);

	/* resources */
	if (ical->resources)
		store_list (o, VCCategoriesProp, ical->resources);

	/* priority */
	addPropValue (o, VCPriorityProp, to_str (ical->priority));

	/* transparency */
	addPropValue (o, VCTranspProp, to_str (ical->transp));

	/* Owner/organizer */
	if (ical->organizer && ical->organizer->addr)
		addPropValue (o, VCOrgNameProp, ical->organizer->addr);

	/* related */
	if (ical->related)
		store_rel_list (o, VCRelatedToProp, ical->related);

	/* attach */
	for (l = ical->attach; l; l = l->next)
		addPropValue (o, VCAttachProp, l->data);

	/* url */
	if (ical->url)
		addPropValue (o, VCURLProp, ical->url);

	if (ical->recur){
		char result [256];
		char buffer [80];
		int i;

		sprintf (result, "%s%d ", recur_type_name [ical->recur->type], ical->recur->interval);
		switch (ical->recur->type){
		case RECUR_DAILY:
			break;

		case RECUR_WEEKLY:
			for (i = 0; i < 7; i++){
				if (ical->recur->weekday & (1 << i)){
					sprintf (buffer, "%s ", recur_day_list [i]);
					strcat (result, buffer);
				}
			}
			break;

		case RECUR_MONTHLY_BY_POS: {
			int nega = ical->recur->u.month_pos < 0;

			sprintf (buffer, "%d%s ", nega ? -ical->recur->u.month_pos : ical->recur->u.month_pos,
				 nega ? "-" : "+");
			strcat (result, buffer);
			/* the gui is set up for a single day, not a set here in this case */
			sprintf (buffer, "%s ", recur_day_list [ical->recur->weekday]);
			strcat (result, buffer);
		}
		break;

		case RECUR_MONTHLY_BY_DAY:
			sprintf (buffer, "%d ", ical->recur->u.month_pos);
			strcat (result, buffer);
			break;

		case RECUR_YEARLY_BY_MONTH:
			break;

		case RECUR_YEARLY_BY_DAY:
			break;
		}
		if (ical->recur->_enddate == 0)
			sprintf (buffer, "#%d ",ical->recur->duration);
		else
			sprintf (buffer, "%s ", isodate_from_time_t (ical->recur->_enddate));
		strcat (result, buffer);
		addPropValue (o, VCRRuleProp, result);
	}

	save_alarm (o, &ical->aalarm, ical);
	save_alarm (o, &ical->dalarm, ical);

	if ((alarm = save_alarm (o, &ical->palarm, ical)))
		addPropValue (alarm, VCProcedureNameProp, ical->palarm.data);
	if ((alarm = save_alarm (o, &ical->malarm, ical)))
		addPropValue (alarm, VCEmailAddressProp, ical->malarm.data);

	/* Pilot */
	{
		char buffer [20];

		sprintf (buffer, "%d", ical->pilot_id);
		addPropValue (o, XPilotIdProp, buffer);
		sprintf (buffer, "%d", ical->pilot_status);
		addPropValue (o, XPilotStatusProp, buffer);
	}

	return o;
}

void
ical_foreach (GList *events, calendarfn fn, void *closure)
{
	for (; events; events = events->next){
		iCalObject *ical = events->data;

		(*fn) (ical, ical->dtstart, ical->dtend, closure);
	}
}

static int
is_date_in_list (GList *list, struct tm *date)
{
	struct tm tm;

	for (; list; list = list->next){
		time_t *timep = list->data;

		tm = *localtime (timep);
		if (date->tm_mday == tm.tm_mday &&
		    date->tm_mon  == tm.tm_mon &&
		    date->tm_year == tm.tm_year){
			return 1;
		}
	}
	return 0;
}

/* Generates an event instance based on the reference time */
static gboolean
generate (iCalObject *ico, time_t reference, calendarfn cb, void *closure)
{
	time_t offset;
	struct tm tm_start, ref;
	time_t start, end;

	offset = ico->dtend - ico->dtstart;

	tm_start = *localtime (&ico->dtstart);
	ref      = *localtime (&reference);

	tm_start.tm_mday = ref.tm_mday;
	tm_start.tm_mon  = ref.tm_mon;
	tm_start.tm_year = ref.tm_year;

	start = mktime (&tm_start);
	if (start == -1) {
		g_message ("generate(): Produced invalid start date!");
		return FALSE;
	}

	end = start + offset;

#if 0
	/* FIXME: I think this is not needed, since we are offsetting by full day values,
	 * and the times should remain the same --- if you have a daily appointment
	 * at 18:00, it is always at 18:00 even during daylight savings.
	 *
	 * However, what should happen on the exact change-of-savings day with
	 * appointments in the early morning hours?
	 */

	if (ref.tm_isdst > tm_start.tm_isdst) {
		tm_start.tm_hour--;
		tm_end.tm_hour--;
	} else if (ref.tm_isdst < tm_start.tm_isdst) {
		tm_start.tm_hour++;
		tm_end.tm_hour++;
	}
#endif

	if (ico->exdate && is_date_in_list (ico->exdate, &tm_start))
		return TRUE;

	return (*cb) (ico, start, end, closure);
}

int
ical_object_get_first_weekday (int weekday_mask)
{
	int i;

	for (i = 0; i < 7; i++)
		if (weekday_mask & (1 << i))
			return i;

	return -1;
}

#define time_in_range(t, a, b) ((t >= a) && (b ? (t < b) : 1))
#define recur_in_range(t, r) (r->enddate ? (t < r->enddate) : 1)

/*
 * Generate every possible event.  Invokes the callback routine for
 * every occurrence of the event in the [START, END] time interval.
 *
 * If END is zero, the event is generated forever.
 * The callback routine is expected to return 0 when no further event
 * generation is requested.
 */
void
ical_object_generate_events (iCalObject *ico, time_t start, time_t end, calendarfn cb, void *closure)
{
	time_t current;
	int first_week_day;

	/* If there is no recurrence, just check ranges */

	if (!ico->recur) {
		if ((end && (ico->dtstart < end) && (ico->dtend > start))
		    || ((end == 0) && (ico->dtend > start))) {
			/* The new calendar views expect the times to not be
			   clipped, so they can show that it continues past
			   the end of the viewable area. */
#if 0
			time_t ev_s, ev_e;

			/* Clip range */

			ev_s = MAX (ico->dtstart, start);
			ev_e = MIN (ico->dtend, end);

			(* cb) (ico, ev_s, ev_e, closure);
#else
			(* cb) (ico, ico->dtstart, ico->dtend, closure);
#endif
		}
		return;
	}

	/* The event has a recurrence rule -- check that we will generate at least one instance */

	if (end != 0) {
		if (ico->dtstart > end)
			return;

		if (!IS_INFINITE (ico->recur) && (ico->recur->enddate < start))
			return;
	}

	/* Generate the instances */

	current = ico->dtstart;

	switch (ico->recur->type) {
	case RECUR_DAILY:
		do {
			if (time_in_range (current, start, end) && recur_in_range (current, ico->recur))
				if (!generate (ico, current, cb, closure))
					return;

			/* Advance */

			current = time_add_day (current, ico->recur->interval);

			if (current == -1) {
				g_warning ("RECUR_DAILY: time_add_day() returned invalid time");
				return;
			}
		} while ((current < end) || (end == 0));

		break;

	case RECUR_WEEKLY:
		do {
			struct tm tm;

			tm = *localtime (&current);

			if (time_in_range (current, start, end) && recur_in_range (current, ico->recur)) {
				/* Weekdays to recur on are specified as a bitmask */
			  if (ico->recur->weekday & (1 << tm.tm_wday)) {
					if (!generate (ico, current, cb, closure))
						return;
			  }
			}

			/* Advance by day for scanning the week or by interval at week end */

			if (tm.tm_wday == 6)
				current = time_add_day (current, (ico->recur->interval - 1) * 7 + 1);
			else
				current = time_add_day (current, 1);

			if (current == -1) {
				g_warning ("RECUR_WEEKLY: time_add_day() returned invalid time\n");
				return;
			}
		} while (current < end || (end == 0));

		break;

	case RECUR_MONTHLY_BY_POS:
		/* FIXME: We only deal with positives now */
		if (ico->recur->u.month_pos < 0) {
			g_warning ("RECUR_MONTHLY_BY_POS does not support negative positions yet");
			return;
		}

		if (ico->recur->u.month_pos == 0)
			return;

		first_week_day = /* ical_object_get_first_weekday (ico->recur->weekday);  */
			ico->recur->weekday; /* the i/f only lets you choose a single day of the week! */

		/* This should not happen, but take it into account */
		if (first_week_day == -1) {
			g_warning ("ical_object_get_first_weekday() returned -1");
			return;
		}

		do {
			struct tm tm;
			time_t t;
			int    week_day_start;

			tm = *localtime (&current);
			tm.tm_mday = 1;
			t = mktime (&tm);
			tm = *localtime (&t);
			week_day_start = tm.tm_wday;

			tm.tm_mday = (7 * (ico->recur->u.month_pos - ((week_day_start <= first_week_day ) ? 1 : 0))
				      - (week_day_start - first_week_day) + 1);
			if( tm.tm_mday > 31 )
			{
				tm.tm_mday = 1;
				tm.tm_mon += ico->recur->interval;
				current = mktime (&tm);
				continue;
			}

			switch( tm.tm_mon )
			{
			case 3:
			case 5:
			case 8:
			case 10:
				if( tm.tm_mday > 30 )
				{
					tm.tm_mday = 1;
					tm.tm_mon += ico->recur->interval;
					current = mktime (&tm);
					continue;
				}
				break;
			case 1:
				if( ((tm.tm_year+1900)%4) == 0
					&& ((tm.tm_year+1900)%400) != 100
					&& ((tm.tm_year+1900)%400) != 200
					&& ((tm.tm_year+1900)%400) != 300 )
				{

					if( tm.tm_mday > 29 )
					{
						tm.tm_mday = 1;
						tm.tm_mon += ico->recur->interval;
						current = mktime (&tm);
						continue;
					}
				}
				else
				{
					if( tm.tm_mday > 28 )
					{
						tm.tm_mday = 1;
						tm.tm_mon += ico->recur->interval;
						current = mktime (&tm);
						continue;
					}
				}
				break;
			}

			t = mktime (&tm);

			if (time_in_range (t, start, end) && recur_in_range (current, ico->recur))
				if (!generate (ico, t, cb, closure))
					return;

			/* Advance by the appropriate number of months */

			current = mktime (&tm);

			tm.tm_mday = 1;
			tm.tm_mon += ico->recur->interval;
			current = mktime (&tm);

			if (current == -1) {
				g_warning ("RECUR_MONTHLY_BY_DAY: mktime error\n");
				return;
			}
		} while ((current < end) || (end == 0));

		break;

	case RECUR_MONTHLY_BY_DAY:
		do {
			struct tm tm;
			time_t t;
			int    p;

			tm = *localtime (&current);

			p = tm.tm_mday;
			tm.tm_mday = ico->recur->u.month_day;
			t = mktime (&tm);
			if (time_in_range (t, start, end) && recur_in_range (current, ico->recur))
				if (!generate (ico, t, cb, closure))
					return;

			/* Advance by the appropriate number of months */

			tm.tm_mday = p;
			tm.tm_mon += ico->recur->interval;
			current = mktime (&tm);

			if (current == -1) {
				g_warning ("RECUR_MONTHLY_BY_DAY: mktime error\n");
				return;
			}
		} while (current < end || (end == 0));

		break;

	case RECUR_YEARLY_BY_MONTH:
	case RECUR_YEARLY_BY_DAY:
		do {
			if (time_in_range (current, start, end) && recur_in_range (current, ico->recur))
				if (!generate (ico, current, cb, closure))
					return;

			/* Advance */

			current = time_add_year (current, ico->recur->interval);
		} while (current < end || (end == 0));

		break;

	default:
		g_assert_not_reached ();
	}
}

static int
duration_callback (iCalObject *ico, time_t start, time_t end, void *closure)
{
	int *count = closure;
	struct tm tm;

	tm = *localtime (&start);

	(*count)++;
	if (ico->recur->duration == *count) {
		ico->recur->enddate = time_day_end (end);
		return 0;
	}
	return 1;
}

/* Computes ico->recur->enddate from ico->recur->duration */
void
ical_object_compute_end (iCalObject *ico)
{
	int count = 0;

	g_return_if_fail (ico->recur != NULL);

	ico->recur->_enddate = 0;
	ico->recur->enddate = 0;
	ical_object_generate_events (ico, ico->dtstart, 0, duration_callback, &count);
}

int
alarm_compute_offset (CalendarAlarm *a)
{
	if (!a->enabled)
		return -1;
	switch (a->units){
	case ALARM_MINUTES:
		a->offset = a->count * 60;
		break;
	case ALARM_HOURS:
		a->offset = a->count * 3600;
		break;
	case ALARM_DAYS:
		a->offset = a->count * 24 * 3600;
	}
	return a->offset;
}

/**
 * ical_object_find_in_string:
 * @uid: Unique identifier of the sought object.
 * @vcalobj: String representation of a complete calendar object.
 * @ico: The resulting #iCalObject is stored here.
 *
 * Parses a complete vCalendar object string and tries to find the calendar
 * object that matches the specified @uid.  If found, it stores the resulting
 * #iCalObject in the @ico parameter.
 *
 * Return value: A result code depending on whether the parse and search were
 * successful.
 **/
CalObjFindStatus
ical_object_find_in_string (const char *uid, const char *vcalobj, iCalObject **ico)
{
#if 1
	icalcomponent* comp = NULL;
	icalcomponent *subcomp;
	iCalObject    *ical;

	g_return_val_if_fail (vcalobj != NULL, CAL_OBJ_FIND_NOT_FOUND);

	comp = icalparser_parse_string (vcalobj);

	if (!comp) {
		printf ("CAL_OBJ_FIND_SYNTAX_ERROR #1\n");
		return CAL_OBJ_FIND_SYNTAX_ERROR;
	}

	subcomp = icalcomponent_get_first_component (comp,
						     ICAL_ANY_COMPONENT);
	if (!subcomp) {
		printf ("CAL_OBJ_FIND_SYNTAX_ERROR #2\n");
		return CAL_OBJ_FIND_SYNTAX_ERROR;
	}

	while (subcomp) {
		ical = ical_object_create_from_icalcomponent (subcomp);
		if (ical->type != ICAL_EVENT && 
		    ical->type != ICAL_TODO  &&
		    ical->type != ICAL_JOURNAL) {
			g_warning ("Skipping unsupported iCalendar component");
		} else {
			if (strcasecmp (ical->uid, uid) == 0) {
				(*ico) = ical;
				(*ico)->ref_count = 1;
				printf ("CAL_OBJ_FIND_SUCCESS\n");

	printf ("ical_object_find_in_string:\n");
	printf ("-----------------------------------------------------\n");
	dump_icalobject (*ico);
	printf ("-----------------------------------------------------\n");


				return CAL_OBJ_FIND_SUCCESS;
			}
		}
		subcomp = icalcomponent_get_next_component (comp,
							   ICAL_ANY_COMPONENT);
	}

	printf ("CAL_OBJ_FIND_NOT_FOUND\n");
	return CAL_OBJ_FIND_NOT_FOUND;

#else /* 0 */
	VObject *vcal;
	VObjectIterator i;
	CalObjFindStatus status;

	g_return_val_if_fail (uid != NULL, CAL_OBJ_FIND_SYNTAX_ERROR);
	g_return_val_if_fail (vcalobj != NULL, CAL_OBJ_FIND_SYNTAX_ERROR);
	g_return_val_if_fail (ico != NULL, CAL_OBJ_FIND_SYNTAX_ERROR);

	*ico = NULL;
	status = CAL_OBJ_FIND_NOT_FOUND;

	vcal = Parse_MIME (vcalobj, strlen (vcalobj));

	if (!vcal)
		return CAL_OBJ_FIND_SYNTAX_ERROR;

	initPropIterator (&i, vcal);

	while (moreIteration (&i)) {
		VObject *vobj;
		VObject *uid_prop;
		char *the_str;

		vobj = nextVObject (&i);

		uid_prop = isAPropertyOf (vobj, VCUniqueStringProp);
		if (!uid_prop)
			continue;

		/* str_val() sets the_str to the string representation of the
		 * property.
		 */
		str_val (uid_prop);

		if (strcmp (the_str, uid) == 0) {
			const char *object_name;

			object_name = vObjectName (vobj);
			*ico = ical_object_create_from_vobject (vobj, object_name);

			if (*ico)
				status = CAL_OBJ_FIND_SUCCESS;
		}

		free (the_str);

		if (status == CAL_OBJ_FIND_SUCCESS)
			break;
	}

	cleanVObject (vcal);
	cleanStrTbl ();

	return status;
#endif /* 0 */
}

/* Creates a VObject with the base information of a calendar */
static VObject *
get_calendar_base_vobject (void)
{
	VObject *vobj;
	time_t now;
	struct tm tm;

	/* We call localtime for the side effect of setting tzname */

	now = time (NULL);
	tm = *localtime (&now);

	vobj = newVObject (VCCalProp);

	addPropValue (vobj, VCProdIdProp, PRODID);

#if defined (HAVE_TM_ZONE)
	addPropValue (vobj, VCTimeZoneProp, tm.tm_zone);
#elif defined (HAVE_TZNAME)
	addPropValue (vobj, VCTimeZoneProp, tzname[0]);
#endif

	/* Per the vCalendar spec, this must be "1.0" */
	addPropValue (vobj, VCVersionProp, "1.0");

	return vobj;
}

/**
 * ical_object_to_string:
 * @ico: A calendar object.
 * 
 * Converts a vCalendar object to its string representation.  It is wrapped
 * inside a complete VCALENDAR object because other auxiliary information such
 * as timezones may appear there.
 * 
 * Return value: String representation of the object.
 **/
char *
ical_object_to_string (iCalObject *ico)
{
#if 1
	icalcomponent *top = icalcomponent_new (ICAL_VCALENDAR_COMPONENT);
	char *out_cal_string;
	icalcomponent *comp;

	printf ("ical_object_to_string:\n");
	printf ("-----------------------------------------------------\n");
	dump_icalobject (ico);
	printf ("-----------------------------------------------------\n");

	comp = icalcomponent_create_from_ical_object (ico);
	icalcomponent_add_component (top, comp);
	out_cal_string = icalcomponent_as_ical_string (top);
	return g_strdup (out_cal_string);

#else /* 0 */
	VObject *vcalobj, *vobj;
	char *buf, *gbuf;

	vcalobj = get_calendar_base_vobject ();
	vobj = ical_object_to_vobject (ico);
	addVObjectProp (vcalobj, vobj);

	buf = writeMemVObject (NULL, NULL, vcalobj);

	cleanVObject (vcalobj);
	cleanStrTbl ();

	/* We have to g_strdup() it because libversit uses malloc()/realloc(),
	 * and we want clients to be able to use g_free().  Sigh.
	 */
	gbuf = g_strdup (buf);
	free (buf);

	return gbuf;
#endif /* 0 */
}


/**
 * ical_object_compare_dates:
 * @ico1: A calendar event.
 * @ico2: A calendar event to compare with @ico1.
 * 
 * Returns TRUE if the dates of both objects match, including any recurrence
 * rules. Both calendar objects must have a type of ICAL_EVENT.
 * 
 * Return value: TRUE if both calendar objects have the same dates.
 **/
gboolean
ical_object_compare_dates (iCalObject *ico1,
			   iCalObject *ico2)
{
	Recurrence *recur1, *recur2;
	gint num_exdates;
	GList *elem1, *elem2;
	time_t *time1, *time2;

	g_return_val_if_fail (ico1 != NULL, FALSE);
	g_return_val_if_fail (ico2 != NULL, FALSE);
	g_return_val_if_fail (ico1->type == ICAL_EVENT, FALSE);
	g_return_val_if_fail (ico2->type == ICAL_EVENT, FALSE);

	/* First check the base dates. */
	if (ico1->dtstart != ico2->dtstart
	    || ico1->dtend != ico2->dtend)
		return FALSE;

	recur1 = ico1->recur;
	recur2 = ico2->recur;

	/* If the event doesn't recur, we already know it matches. */
	if (!recur1 && !recur2)
		return TRUE;

	/* Check that both recur. */
	if (!(recur1 && recur2))
		return FALSE;

	/* Now we need to see if the recurrence rules are the same. */
	if (recur1->type != recur2->type
	    || recur1->interval != recur2->interval
	    || recur1->enddate != recur2->enddate
	    || recur1->weekday != recur2->weekday
	    || recur1->duration != recur2->duration
	    || recur1->_enddate != recur2->_enddate
	    || recur1->__count != recur2->__count)
		return FALSE;

	switch (recur1->type) {
	case RECUR_MONTHLY_BY_POS:
		if (recur1->u.month_pos != recur2->u.month_pos)
			return FALSE;
		break;
	case RECUR_MONTHLY_BY_DAY:
		if (recur1->u.month_day != recur2->u.month_day)
			return FALSE;
		break;
	default:
		break;
	}

	/* Now check if the excluded dates match. */
	num_exdates = g_list_length (ico1->exdate);
	if (g_list_length (ico2->exdate) != num_exdates)
		return FALSE;
	if (num_exdates == 0)
		return TRUE;

	ico1->exdate = g_list_sort (ico1->exdate, compare_exdates);
	ico2->exdate = g_list_sort (ico2->exdate, compare_exdates);

	elem1 = ico1->exdate;
	elem2 = ico2->exdate;
	while (elem1) {
		time1 = (time_t*) elem1->data;
		time2 = (time_t*) elem2->data;

		if (*time1 != *time2)
			return FALSE;

		elem1 = elem1->next;
		elem2 = elem2->next;
	}

	return TRUE;
}


static gint
compare_exdates (gconstpointer a, gconstpointer b)
{
	const time_t *ca = a, *cb = b;
	time_t diff = *ca - *cb;
	return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
}


/* Converts any CR/LF sequences in the summary field to spaces so we just
   have a one-line field. The iCalObjects summary field is changed. */
static void
ical_object_normalize_summary (iCalObject *ico)
{
	gchar *src, *dest, ch;
	gboolean just_output_space = FALSE;

	src = dest = ico->summary;
	while ((ch = *src++)) {
		if (ch == '\n' || ch == '\r') {
			/* We only output 1 space for each sequence of CR & LF
			   characters. */
			if (!just_output_space) {
				*dest++ = ' ';
				just_output_space = TRUE;
			}
		} else {
			*dest++ = ch;
			just_output_space = FALSE;
		}
	}
	*dest = '\0';
}


void dump_icalobject (iCalObject *ico)
{
	if (!ico) {
		printf ("<<NULL>>\n");
		return;
	}

	printf ("type ");
	switch (ico->type) {
	case ICAL_EVENT: printf ("event"); break;
	case ICAL_TODO: printf ("todo"); break;
	case ICAL_JOURNAL: printf ("journal"); break;
	case ICAL_FBREQUEST: printf ("fbrequest"); break;
	case ICAL_FBREPLY: printf ("fbreply"); break;
	case ICAL_BUSYTIME: printf ("busytime"); break;
	case ICAL_TIMEZONE: printf ("timezone"); break;
	}
	printf ("\n");

	printf ("attach-length %d\n", g_list_length (ico->attach));

	printf ("attendee-length %d\n", g_list_length (ico->attendee));

	printf ("catagories-length %d\n", g_list_length (ico->categories));

	printf ("class '%s'\n", ico->class ? ico->class : "NULL");

	printf ("comment '%s'\n", ico->comment ? ico->comment : "NULL");

	printf ("completed %ld=%s",
		ico->completed, ctime (&ico->completed));

	printf ("created %ld=%s", ico->created, ctime (&ico->created));

	printf ("contact-length %d\n", g_list_length (ico->contact));

	printf ("desc '%s'\n", ico->desc ? ico->desc : "NULL");

	printf ("dtstamp %ld=%s", ico->dtstamp, ctime (&ico->dtstamp));

	printf ("dtstart %ld=%s", ico->dtstart, ctime (&ico->dtstart));

	printf ("dtend %ld=%s", ico->dtend, ctime (&ico->dtend));

	printf ("date_only %d\n", ico->date_only);

	printf ("exdate-length %d\n", g_list_length (ico->exdate));

	printf ("exrule-length %d\n", g_list_length (ico->exrule));

	printf ("iCalGeo %d %f %f\n",
		ico->geo.valid, ico->geo.latitude, ico->geo.longitude);

	printf ("last_mod %ld=%s", ico->last_mod, ctime (&ico->last_mod));

	printf ("location '%s'\n", ico->location ? ico->location : "NULL");

	printf ("organizer %p\n", ico->organizer);

	printf ("percent %d\n", ico->percent);

	printf ("priority %d\n", ico->priority);

	printf ("rstatus '%s'\n", ico->rstatus ? ico->rstatus : "NULL");

	printf ("related-length %d\n", g_list_length (ico->related));

	printf ("resources-length %d\n", g_list_length (ico->resources));

	printf ("rdate-length %d\n", g_list_length (ico->rdate));

	printf ("rrule-length %d\n", g_list_length (ico->rrule));

	printf ("seq %d\n", ico->seq);

	printf ("status '%s'\n", ico->status ? ico->status : "NULL");

	printf ("summary '%s'\n", ico->summary ? ico->summary : "NULL");

	printf ("transp ");
	switch (ico->transp) {
	case ICAL_OPAQUE: printf ("opaque"); break;
	case ICAL_TRANSPARENT: printf ("transparent"); break;
	}
	printf ("\n");

	printf ("uid '%s'\n", ico->uid ? ico->uid : "NULL");

	printf ("url '%s'\n", ico->url ? ico->url : "NULL");

	printf ("recurid %ld=%s", ico->recurid, ctime (&ico->recurid));

	printf ("dalarm %d\n", ico->dalarm.enabled);

	printf ("aalarm %d\n", ico->aalarm.enabled);

	printf ("palarm %d\n", ico->palarm.enabled);

	printf ("malarm %d\n", ico->malarm.enabled);

	printf ("alarms-length %d\n", g_list_length (ico->alarms));

	printf ("recur %p\n", ico->recur);

	printf ("new %d\n", ico->new);

	printf ("user_data %p\n", ico->user_data);

	printf ("ref_count %d\n", ico->ref_count);
}
