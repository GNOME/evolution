/*
 * Calendar objects implementations.
 * Copyright (C) 1998 the Free Software Foundation
 *
 * Authors:
 *   Miguel de Icaza (miguel@gnu.org)
 *   Federico Mena (quartic@gimp.org)
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
	ico->status    = g_strdup ("NEEDS ACTION");

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
	
	if (o->recur->weekday == 0){
		struct tm *tm = localtime (&o->dtstart);

		o->recur->weekday = 1 << tm->tm_wday;
	}
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
		(*str)++;
	}

	if (**str == '+')
		(*str)++;

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
		while (**str && isdigit (**str)){
			val = 10 * val + (**str - '0');
			(*str)++;
		}
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

/*
 * FIXME: This is loosing precission.  Enhanec the thresholds
 */
#define HOURS(n) (n*(60*60))

static void
setup_alarm_at (time_t base, CalendarAlarm *alarm, char *iso_time)
{
	time_t alarm_time = time_from_isodate (iso_time);
	int d = difftime (base, alarm_time);

	if (d > HOURS (2)){
		if (d > HOURS (48)){
			alarm->count = d / HOURS (24);
			alarm->units = ALARM_DAYS;
		} else {
			alarm->count = d / 60*60;
			alarm->units = ALARM_HOURS;
		}
	} else {
		alarm->count = d / 60;
		alarm->units = ALARM_MINUTES;
	}
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
	
	/* dalarm */
	ical->dalarm.type = ALARM_DISPLAY;
	ical->dalarm.enabled = 0;
	if (has (o, VCDAlarmProp)){
		if ((a = is_a_prop_of (o, VCRunTimeProp))){
			ical->dalarm.enabled = 1;
			setup_alarm_at (ical->dtstart, &ical->dalarm, str_val (a));
			free (the_str);
		}
	}
	
	/* aalarm */
	ical->aalarm.type = ALARM_AUDIO;
	ical->aalarm.enabled = 0;
	if (has (o, VCAAlarmProp)){
		if ((a = is_a_prop_of (o, VCRunTimeProp))){
			ical->aalarm.enabled = 1;
			setup_alarm_at (ical->dtstart, &ical->aalarm, str_val (a));
			free (the_str);
		}
	}

	/* palarm */
	ical->palarm.type = ALARM_PROGRAM;
	ical->palarm.enabled = 0;
	if (has (o, VCPAlarmProp)){
		ical->palarm.type = ALARM_PROGRAM;
		if ((a = is_a_prop_of (o, VCRunTimeProp))){
			ical->palarm.enabled = 1;
			setup_alarm_at (ical->dtstart, &ical->palarm, str_val (a));
			free (the_str);

			if ((a = is_a_prop_of (o, VCProcedureNameProp))){
				ical->palarm.data = g_strdup (str_val (a));
				free (the_str);
			}
		}
	}

	/* malarm */
	ical->malarm.type = ALARM_MAIL;
	ical->malarm.enabled = 0;
	if (has (o, VCMAlarmProp)){
		ical->malarm.type = ALARM_MAIL;
		if ((a = is_a_prop_of (o, VCRunTimeProp))){
			ical->malarm.enabled = 1;
			setup_alarm_at (ical->dtstart, &ical->malarm, str_val (a));
			free (the_str);
			
			if ((a = is_a_prop_of (o, VCEmailAddressProp))){
				ical->malarm.data = g_strdup (str_val (a));
				free (the_str);
			}
		}
	}

	/* rrule */
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

	for (p = result, l = values; l; l = l->next) {
		int len = strlen (l->data);
		
		strcpy (p, l->data);

		if (l->next) {
			p [len] = sep;
			p += len+1;
		} else
			p += len;
	}

	*p = 0;

	addPropValue (o, prop, result);
	g_free (result);
}

static char *recur_type_name [] = { "D", "W", "MP", "MD", "YM", "YD" };
static char *recur_day_list  [] = { "SU", "MO", "TU","WE", "TH", "FR", "SA" };
static char *alarm_names [] = { VCMAlarmProp, VCPAlarmProp, VCDAlarmProp, VCAAlarmProp };

static VObject *
save_alarm (VObject *o, CalendarAlarm *alarm, iCalObject *ical)
{
	VObject *alarm_object;
	struct tm *tm;
	time_t alarm_time;
	
	if (!alarm->enabled)
		return NULL;
	tm = localtime (&ical->dtstart);
	switch (alarm->units){
	case ALARM_MINUTES:
		tm->tm_min -= alarm->count;
		break;
		
	case ALARM_HOURS:
		tm->tm_hour -= alarm->count;
		break;
		
	case ALARM_DAYS:
		tm->tm_mday -= alarm->count;
		break;
	}
	
	alarm_time = mktime (tm);
	alarm_object = addProp (o, alarm_names [alarm->type]);
	addPropValue (alarm_object, VCRunTimeProp, isodate_from_time_t (alarm_time));
	addPropValue (alarm_object, VCRepeatCountProp, "1");
	addPropValue (alarm_object, VCDisplayStringProp, "GNOME appointment alarm");

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
	addPropValue (o, VCDTendProp, isodate_from_time_t (ical->dtend));

	/* dcreated */
	addPropValue (o, VCDCreatedProp, isodate_from_time_t (ical->created));

	/* completed */
	if (ical->completed)
		addPropValue (o, VCDTendProp, isodate_from_time_t (ical->completed));

	/* last_mod */
	addPropValue (o, VCLastModifiedProp, isodate_from_time_t (ical->last_mod));

	/* exdate */
	if (ical->exdate)
		store_list (o, VCExpDateProp, ical->exdate, ',');

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
	} else {
		addPropValue (o, VCSummaryProp, _("Appointment"));
	}
	
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
	if (ical->related)
		store_list (o, VCRelatedToProp, ical->related, ';');

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
			for (i = 0; i < 7; i++){
				if (ical->recur->weekday & (1 << i)){
					sprintf (buffer, "%s ", recur_day_list [i]);
					strcat (result, buffer);
				}
			}
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
	struct tm *tm;
	
	for (; list; list = list->next){
		time_t *timep = list->data;
		
		tm = localtime (timep);
		if (date->tm_mday == tm->tm_mday &&
		    date->tm_mon  == tm->tm_mon &&
		    date->tm_year == tm->tm_year)
			return 1;
	}
	return 0;
}

static int
generate (iCalObject *ico, time_t reference, calendarfn cb, void *closure)
{
	struct tm dt_start, dt_end, ref;
	time_t s_t, e_t;

	dt_start = *localtime (&ico->dtstart);
	dt_end   = *localtime (&ico->dtend);
	ref      = *localtime (&reference);

	dt_start.tm_mday = ref.tm_mday;
	dt_start.tm_mon  = ref.tm_mon;
	dt_start.tm_year = ref.tm_year;

	dt_end.tm_mday = ref.tm_mday;
	dt_end.tm_mon  = ref.tm_mon;
	dt_end.tm_year = ref.tm_year;

	s_t = mktime (&dt_start);
	if (ico->exdate && is_date_in_list (ico->exdate, &dt_start))
		return;
	
	e_t = mktime (&dt_end);
	
	if (s_t == -1 || e_t == -1){
		g_warning ("Produced invalid dates!\n");
		return 0;
	}
	return (*cb)(ico, s_t, e_t, closure);
}

#define time_in_range(x,a,b) ((x >= a) && (b ? x <= b : 1))
#define recur_in_range(t,r) (r->enddate ? (t < r->enddate) : 1)

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
	Recurrence *recur = ico->recur;
	time_t current;
	int first_week_day, i;
	
	if (!ico->recur){
		if (time_in_range (ico->dtstart, start, end) ||
		    time_in_range (ico->dtend, start, end)){
			time_t ev_s, ev_e;

			ev_s = ico->dtstart < start ? start : ico->dtstart;
			ev_e = ico->dtend > end ? end : ico->dtend;
			(*cb)(ico, ev_s, ev_e, closure);
		}
		return;
	}

	/* The event has a recurrence rule */
	if (end != 0){
		if (ico->dtstart > end)
			return;
		if (!IS_INFINITE (recur) && recur->enddate < start)
			return;
	}

	current = ico->dtstart;
	switch (recur->type){
	case RECUR_DAILY:
		do {
			if (time_in_range (current, start, end) && recur_in_range (current, recur)){
				if (!generate (ico, current, cb, closure))
					return;
			}

			/* Advance */
			current = time_add_day (current, recur->interval);
			
			if (current == -1){
				g_warning ("RECUR_DAILY: mktime error\n");
				return;
			}
		} while (current < end || (end == 0));
		break;
		
	case RECUR_WEEKLY:
		do {
			struct tm *tm = localtime (&current);
			
			if (time_in_range (current, start, end) && recur_in_range (current, recur)){
				if (recur->weekday & (1 << tm->tm_wday))
					if (!generate (ico, current, cb, closure))
						return;
			}

			/* Advance by day for scanning the week or by interval at week end */
			if (tm->tm_wday == 6)
				current = time_add_day (current, recur->interval);
			else
				current = time_add_day (current, 1);

			if (current == -1){
				g_warning ("RECUR_WEEKLY: mktime error\n");
				return;
			}
		} while (current < end || (end == 0));
		break;
		
	case RECUR_MONTHLY_BY_POS:
		/* FIXME: We only deal with positives now */
		if (recur->u.month_pos < 0)
			return;
		
		if (recur->u.month_pos == 0)
			return;
		
		first_week_day = 7;
		for (i = 6; i >= 0; i--)
			if (recur->weekday & (1 << i))
				first_week_day = i;

		/* This should not happen, but take it into account */
		if (first_week_day == 7)
			return;

		do {
			struct tm tm;
			time_t t;
			int    week_day_start;

			tm = *localtime (&current);
			tm.tm_mday = 1;
			t = mktime (&tm);
			tm = *localtime (&t);
			week_day_start = tm.tm_wday;

			tm.tm_mday = 7 * (recur->u.month_pos -
					  ((week_day_start <= first_week_day ) ? 1 : 0)) -
				(week_day_start - first_week_day) + 1;
			
			t = mktime (&tm);
			
			if (time_in_range (t, start, end) && recur_in_range (current, recur))
				if (!generate (ico, t, cb, closure))
					return;

			/* Advance a month */
			current = mktime (&tm);
			
			tm.tm_mday = 1;
			tm.tm_mon += recur->interval;
			current = mktime (&tm);

			if (current == -1){
				g_warning ("RECUR_MONTHLY_BY_DAY: mktime error\n");
				return;
			}
		} while (current < end || (end == 0));
		break;

	case RECUR_MONTHLY_BY_DAY:
		do {
			struct tm *tm = localtime (&current);
			time_t t;
			int    p;

			p = tm->tm_mday;
			tm->tm_mday = recur->u.month_day;
			t = mktime (tm);
			if (time_in_range (t, start, end) && recur_in_range (current, recur))
				if (!generate (ico, t, cb, closure))
					return;

			/* Advance a month */
			tm->tm_mday = p;
			tm->tm_mon += recur->interval;
			current = mktime (tm);
			if (current == -1){
				g_warning ("RECUR_MONTHLY_BY_DAY: mktime error\n");
				return;
			}
		} while (current < end || (end == 0));
		
	case RECUR_YEARLY_BY_MONTH:
	case RECUR_YEARLY_BY_DAY:
		do {
			if (time_in_range (current, start, end) && recur_in_range (current, recur))
				if (!generate (ico, current, cb, closure))
					return;
			
			/* Advance */
			current = time_add_year (current, recur->interval);
		} while (current < end || (end == 0));
	}
}

static int
duration_callback (iCalObject *ico, time_t start, time_t end, void *closure)
{
	int *count = closure;

	(*count)++;
	if (ico->recur->duration == *count) {
		ico->recur->enddate = time_end_of_day (end);
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
