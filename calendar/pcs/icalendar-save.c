/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <config.h>
#include <unistd.h>
#include <sys/stat.h>
#include "icalendar-save.h"


static void unparse_person (iCalPerson *person, icalproperty *person_prop);
static struct icaltimetype timet_to_icaltime (time_t tt);
static icalproperty *unparse_related (iCalRelation *rel);
static icalcomponent *unparse_alarm (CalendarAlarm *alarm);


icalcomponent*
icalcomponent_create_from_ical_object (iCalObject *ical)
{
	icalcomponent_kind kind;
	icalcomponent *comp;
	icalproperty *prop;

	switch (ical->type) {
	case ICAL_EVENT: kind = ICAL_VEVENT_COMPONENT; break;
	case ICAL_TODO: kind = ICAL_VTODO_COMPONENT; break;
	case ICAL_JOURNAL: kind = ICAL_VJOURNAL_COMPONENT; break;
	case ICAL_FBREQUEST: kind = ICAL_VFREEBUSY_COMPONENT; break;
	case ICAL_TIMEZONE: kind = ICAL_VTIMEZONE_COMPONENT; break;
	default:
		kind = ICAL_NO_COMPONENT; break;
	}

	comp = icalcomponent_new (kind);

	/*** calscale ***/
	prop = icalproperty_new_calscale ("GREGORIAN");
	icalcomponent_add_property (comp, prop);

	/*** catagories ***/
	if (ical->categories) {		
		/* ical->categories is a GList of (char *) */
		GList *cur;
		for (cur = ical->categories; cur; cur = cur->next) {
			prop = icalproperty_new_categories ((char *) cur);
			icalcomponent_add_property (comp, prop);
		}
	}

	/*** class ***/
	if (ical->class) {
		prop = icalproperty_new_class (ical->class);
		icalcomponent_add_property (comp, prop);
	}

	/*** comment ***/
	if (ical->comment) {
		prop = icalproperty_new_comment (ical->comment);
		icalcomponent_add_property (comp, prop);
	}

	/*** description ***/
	if (ical->desc) {
		prop = icalproperty_new_description (ical->desc);
		icalcomponent_add_property (comp, prop);
	}

	/*** geo ***/
	if (ical->geo.valid) {
		struct icalgeotype v;
		v.lat = ical->geo.latitude;
		v.lon = ical->geo.longitude;
		prop = icalproperty_new_geo (v);
		icalcomponent_add_property (comp, prop);
	}

	/*** location ***/
	if (ical->location) {
		prop = icalproperty_new_location (ical->location);
		icalcomponent_add_property (comp, prop);
	}

	/*** percentcomplete ***/
	prop = icalproperty_new_percentcomplete (ical->percent);
	icalcomponent_add_property (comp, prop);

	/*** priority ***/
	if (ical->priority) {
		prop = icalproperty_new_priority (ical->priority);
		icalcomponent_add_property (comp, prop);
	}

	/*** resources ***/
	if (ical->resources) {
		/* ical->resources is a GList of (char *) */
		GList *cur;
		for (cur = ical->resources; cur; cur = cur->next) {
			prop = icalproperty_new_resources ((char *) cur);
			icalcomponent_add_property (comp, prop);
		}
	}

	/*** status ***/
	if (ical->status) {
		prop = icalproperty_new_status (ical->status);
		icalcomponent_add_property (comp, prop);
	}

	/*** summary ***/
	if (ical->summary) {
		prop = icalproperty_new_summary (ical->summary);
		icalcomponent_add_property (comp, prop);
	}

	/*** completed ***/
	if (ical->completed) {
		struct icaltimetype ictime;
		ictime = timet_to_icaltime (ical->completed);
		prop = icalproperty_new_completed (ictime);
		icalcomponent_add_property (comp, prop);
	}

	/*** dtend ***/  /*** due ***/
	if (ical->dtend) {
		/* FIXME: We should handle timezone specifiers */
		struct icaltimetype ictime;
		ictime = timet_to_icaltime (ical->dtend);
		if (ical->type == ICAL_TODO)
			prop = icalproperty_new_due (ictime);
		else
			prop = icalproperty_new_dtend (ictime);
		if (ical->date_only) {
			icalparameter *param;
			param = icalparameter_new (ICAL_VALUE_PARAMETER);
			icalparameter_set_value (param, ICAL_VALUE_DATE);
			icalproperty_add_parameter (prop, param);
		}
		icalcomponent_add_property (comp, prop);
	}

	/*** dtstart ***/
	if (ical->dtstart) {
		/* FIXME: We should handle timezone specifiers */
		struct icaltimetype ictime;
		ictime = timet_to_icaltime (ical->dtstart);
		prop = icalproperty_new_dtstart (ictime);
		if (ical->date_only) {
			icalparameter *param;
			param = icalparameter_new (ICAL_VALUE_PARAMETER);
			icalparameter_set_value (param, ICAL_VALUE_DATE);
			icalproperty_add_parameter (prop, param);
		}
		icalcomponent_add_property (comp, prop);
	}

	/*** duration ***/
	{
		/* FIX ME */
	}

	/*** freebusy ***/
	{
		/* FIX ME */
	}

	/*** transp ***/
	{
		if (ical->transp == ICAL_TRANSP_PROPERTY)
			icalproperty_set_transp (prop, "TRANSPARENT");
		else
			icalproperty_set_transp (prop, "OPAQUE");
		icalcomponent_add_property (comp, prop);
	}

	/*
	  ICAL_TZID_PROPERTY:
	  ICAL_TZNAME_PROPERTY:
	  ICAL_TZOFFSETFROM_PROPERTY:
	  ICAL_TZOFFSETTO_PROPERTY:
	  ICAL_TZURL_PROPERTY:
	*/

	/*** attendee ***/
	if (ical->attendee) {
		/* a list of (iCalPerson *) */
		GList *cur;
		for (cur = ical->attendee; cur; cur = cur->next) {
			iCalPerson *person = (iCalPerson *) cur->data;
			prop = icalproperty_new_attendee ("FIX ME");
			unparse_person (person, prop);
			icalcomponent_add_property (comp, prop);
		}
	}

	/*** contact ***/
	if (ical->contact) {
		/* a list of (iCalPerson *) */
		GList *cur;
		for (cur = ical->contact; cur; cur = cur->next) {
			iCalPerson *person = (iCalPerson *) cur->data;
			prop = icalproperty_new_contact ("FIX ME");
			unparse_person (person, prop);
			icalcomponent_add_property (comp, prop);
		}
	}

	/*** organizer ***/
	if (ical->organizer) {
		prop = icalproperty_new_organizer ("FIX ME");
		unparse_person (ical->organizer, prop);
		icalcomponent_add_property (comp, prop);
	}

	/*** recurrenceid ***/
	if (ical->recurid) {
		struct icaltimetype ictime;
		ictime = timet_to_icaltime (ical->recurid);
		prop = icalproperty_new_recurrenceid (ictime);
	}

	/*** relatedto ***/

	if (ical->related) {
		/* a list of (iCalPerson *) */
		GList *cur;
		for (cur = ical->related; cur; cur = cur->next) {
			iCalRelation *related = (iCalRelation *) cur->data;
			prop = unparse_related (related);
			icalcomponent_add_property (comp, prop);
		}
	}


	/*** url ***/
	if (ical->url) {
		prop = icalproperty_new_url (ical->url);
		icalcomponent_add_property (comp, prop);
	}

	/*** uid ***/
	if (ical->uid) {
		prop = icalproperty_new_uid (ical->uid);
		icalcomponent_add_property (comp, prop);
	}

	/*** exdate ***/
	if (ical->exdate) {
		struct icaltimetype v;
		GList *cur;
		for (cur = ical->exdate; cur; cur = cur->next) {
			time_t t = (time_t) cur->data;
			v = timet_to_icaltime (t);
			prop = icalproperty_new_exdate (v);
			icalcomponent_add_property (comp, prop);
		}
	}

	/*** created ***/
	if (ical->created) {
		struct icaltimetype v;
		v = timet_to_icaltime (ical->created);
		prop = icalproperty_new_created (v);
		icalcomponent_add_property (comp, prop);
	}

	/*** dtstamp ***/
	if (ical->dtstamp) {
		struct icaltimetype v;
		v = timet_to_icaltime (ical->dtstamp);
		prop = icalproperty_new_created (v);
		icalcomponent_add_property (comp, prop);
	}

	/*** lastmodified ***/
	if (ical->last_mod) {
		struct icaltimetype v;
		v = timet_to_icaltime (ical->last_mod);
		prop = icalproperty_new_created (v);
		icalcomponent_add_property (comp, prop);
	}

	/*** sequence ***/
	if (ical->seq) {
		prop = icalproperty_new_sequence (ical->seq);
		icalcomponent_add_property (comp, prop);
	}

	/*** requeststatus ***/
	if (ical->rstatus) {
		prop = icalproperty_new_requeststatus (ical->rstatus);
		icalcomponent_add_property (comp, prop);
	}

	/* if there is a VALARM subcomponent, add it here */

	if (ical->alarms) {
		GList *cur;
		for (cur = ical->alarms; cur; cur = cur->next) {
			CalendarAlarm *alarm = (CalendarAlarm *) cur->data;
			icalcomponent *subcomp = unparse_alarm (alarm);
			icalcomponent_add_component (comp, subcomp);
		}
	}

	return comp;
}


/* FIX ME -- same as icaltimetype_from_timet in icaltypes.c */
static
struct icaltimetype timet_to_icaltime (time_t tt)
{
	extern long timezone;
	struct tm *t;
	struct icaltimetype i;

	t = gmtime (&tt);

	/*return tt - (i->is_utc ? timezone : 0); */
	i.is_utc = 0;

	i.year = t->tm_year + 1900;
	i.month = t->tm_mon + 1;
	i.day = t->tm_mday;

	if (t->tm_hour == 0 && t->tm_min == 0 && t->tm_sec == 0) {
		i.is_date = 1;
		i.hour = 0;
		i.minute = 0;
		i.second = 0;
	} else {
		i.is_date = 0;
		i.hour = t->tm_hour;
		i.minute = t->tm_min;
		i.second = t->tm_sec;
	}

	return i;
}


/* fills in "person_prop" with information from "person" */

static
void unparse_person (iCalPerson *person, icalproperty *person_prop)
{
	icalparameter *param;
	GList *cur;

	/* convert iCalPerson to an icalproperty */

	param = icalparameter_new_cn (person->name);
	icalproperty_add_parameter (person_prop, param);

	if (g_strcasecmp (person->role, "CHAIR") == 0)
		param = icalparameter_new_role (ICAL_ROLE_CHAIR);
	else if (g_strcasecmp (person->role, "REQPARTICIPANT") == 0)
		param = icalparameter_new_role (ICAL_ROLE_REQPARTICIPANT);
	else if (g_strcasecmp (person->role, "OPTPARTICIPANT") == 0)
		param = icalparameter_new_role (ICAL_ROLE_OPTPARTICIPANT);
	else if (g_strcasecmp (person->role, "NONPARTICIPANT") == 0)
		param = icalparameter_new_role (ICAL_ROLE_NONPARTICIPANT);
	else
		param = icalparameter_new_role (ICAL_ROLE_XNAME);
	icalproperty_add_parameter (person_prop, param);

	if (g_strcasecmp (person->partstat, "NEEDSACTION") == 0)
		param = icalparameter_new_partstat (ICAL_PARTSTAT_NEEDSACTION);
	else if (g_strcasecmp (person->partstat, "ACCEPTED") == 0)
		param = icalparameter_new_partstat (ICAL_PARTSTAT_ACCEPTED);
	else if (g_strcasecmp (person->partstat, "DECLINED") == 0)
		param = icalparameter_new_partstat (ICAL_PARTSTAT_DECLINED);
	else if (g_strcasecmp (person->partstat, "TENTATIVE") == 0)
		param = icalparameter_new_partstat (ICAL_PARTSTAT_TENTATIVE);
	else if (g_strcasecmp (person->partstat, "DELEGATED") == 0)
		param = icalparameter_new_partstat (ICAL_PARTSTAT_DELEGATED);
	else if (g_strcasecmp (person->partstat, "COMPLETED") == 0)
		param = icalparameter_new_partstat (ICAL_PARTSTAT_COMPLETED);
	else if (g_strcasecmp (person->partstat, "INPROCESS") == 0)
		param = icalparameter_new_partstat (ICAL_PARTSTAT_INPROCESS);
	else /* FIX ME, NEEDSACTION instead? */
		param = icalparameter_new_partstat (ICAL_PARTSTAT_XNAME);
	icalproperty_add_parameter (person_prop, param);

	if (person->rsvp != FALSE) {
		param = icalparameter_new_rsvp (TRUE);
		icalproperty_add_parameter (person_prop, param);
	}

	if (g_strcasecmp (person->cutype, "INDIVIDUAL") == 0)
		param = icalparameter_new_cutype (ICAL_CUTYPE_INDIVIDUAL);
	else if (g_strcasecmp (person->cutype, "GROUP") == 0)
		param = icalparameter_new_cutype (ICAL_CUTYPE_GROUP);
	else if (g_strcasecmp (person->cutype, "RESOURCE") == 0)
		param = icalparameter_new_cutype (ICAL_CUTYPE_RESOURCE);
	else if (g_strcasecmp (person->cutype, "ROOM") == 0)
		param = icalparameter_new_cutype (ICAL_CUTYPE_ROOM);
	else /* FIX ME, INDIVIDUAL instead? */
		param = icalparameter_new_cutype (ICAL_CUTYPE_UNKNOWN);
	icalproperty_add_parameter (person_prop, param);

	/* person->member is a list of ICAL_MEMBER_PARAMETER */
	for (cur = person->member; cur; cur = cur->next) {
		gchar *member = (gchar *) cur->data;
		param = icalparameter_new_member (member);
		icalproperty_add_parameter (person_prop, param);
	}

	/* person->deleg_to is a list of ICAL_DELEGATEDTO_PARAMETER */
	for (cur = person->deleg_to; cur; cur = cur->next) {
		gchar *deleg_to = (gchar *) cur->data;
		param = icalparameter_new_delegatedto (deleg_to);
		icalproperty_add_parameter (person_prop, param);
	}

	/* ret->deleg_from is a list of ICAL_DELEGATEDFROM_PARAMETER */
	for (cur = person->deleg_from; cur; cur = cur->next) {
		gchar *deleg_from = (gchar *) cur->data;
		param = icalparameter_new_delegatedfrom (deleg_from);
		icalproperty_add_parameter (person_prop, param);
	}

	param = icalparameter_new_sentby (person->sent_by);

	/* ret->deleg_to is a list of ICAL_DIR_PARAMETER */
	/* FIX ME ... */
}


static
icalproperty *unparse_related (iCalRelation *rel)
{
	icalproperty *prop;

	prop = icalproperty_new_relatedto (rel->reltype);

	icalproperty_set_relatedto (prop, rel->uid);

	/* FIX ME  RELTYPE_XNAME ? */
	
	return prop;
}


static
icalcomponent *unparse_alarm (CalendarAlarm *alarm)
{
	icalcomponent *comp = icalcomponent_new (ICAL_VALARM_COMPONENT);
	icalproperty *prop;

	prop = NULL;
	switch (alarm->type){
	case ALARM_AUDIO:
		prop = icalproperty_new_action ("AUDIO");
		break;
	case ALARM_DISPLAY:
		prop = icalproperty_new_action ("DISPLAY");
		break;
	case ALARM_MAIL:
		prop = icalproperty_new_action ("EMAIL");
		break;
	case ALARM_PROGRAM:
		prop = icalproperty_new_action ("PROCEDURE");
		break;
	default:
		g_warning ("Unsupported alarm type!");
		break;
	}
	if (prop)
		icalcomponent_add_property (comp, prop);

	if (alarm->snooze_repeat)
		prop = icalproperty_new_repeat (alarm->snooze_repeat);

	if (alarm->snooze_secs) {
		struct icaldurationtype dur;
		dur = icaldurationtype_from_timet (alarm->snooze_secs);
		prop = icalproperty_new_duration (dur);
		icalcomponent_add_property (comp, prop);
	}

	if (alarm->attach) {
		struct icalattachtype *attach;
		attach = icalattachtype_new ();
		icalattachtype_set_url (attach, alarm->attach);
		prop = icalproperty_new_attach (*attach);
		icalattachtype_free (attach);
		icalcomponent_add_property (comp, prop);
	}

	if (alarm->desc) {
		prop = icalproperty_new_description (alarm->desc);
		icalcomponent_add_property (comp, prop);
	}

	if (alarm->summary) {
		prop = icalproperty_new_summary (alarm->summary);
		icalcomponent_add_property (comp, prop);
	}

	if (alarm->attendee) {
		icalproperty_new_attendee (alarm->attendee);
		icalcomponent_add_property (comp, prop);
	}

	return comp;
}
