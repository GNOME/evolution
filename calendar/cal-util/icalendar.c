/*
 * icalendar server for gnomecal
 * 
 * This module interfaces between libical and the gnomecal internal
 * representation
 *
 * Copyright (C) 1999 The Free Software Foundation
 * Authors:
 *   Russell Steinthal (rms39@columbia.edu)
 * 
 */

#include <config.h>
#include <unistd.h>
#include <sys/stat.h>
#include "icalendar.h"

static time_t icaltime_to_timet (struct icaltimetype* i);
static CalendarAlarm* parse_alarm (icalproperty *prop);
static iCalPerson* parse_person (icalproperty *prop, gchar *value);
static iCalRelation* parse_related (icalproperty *prop);

/* Duplicate a string without memory leaks */
static gchar* copy_str (gchar** store, gchar* src)
{
	if (*store)
		g_free (*store);
	return (*store = g_strdup (src));
}

static GList* 
copy_to_list (GList** store, gchar* src)
{
	*store = g_list_prepend (*store, g_strdup (src));
	return *store;
}

	
iCalObject *
ical_object_create_from_icalcomponent (icalcomponent* comp)
{
	iCalObject    *ical = NULL;
	iCalPerson    *person;
	icalcomponent *subcomp;
	icalproperty  *prop;
	icalparameter *param;
	struct icaltimetype ictime;
	time_t *pt;	
	CalendarAlarm *alarm = NULL;
	icalcomponent_kind compType;
	struct icalgeotype geo;
	struct icalperiodtype period; 

	gboolean root = FALSE;
	gboolean attachment = FALSE;

	char *tmpStr;		/* this is a library-owned string */

	ical  = g_new0 (iCalObject, 1);

	compType = icalcomponent_isa (comp);
	
	switch (compType) {
	case ICAL_XROOT_COMPONENT:
	     root = TRUE;
	     break;
	case ICAL_XATTACH_COMPONENT:
	     attachment = TRUE;
	     break;
	case ICAL_VEVENT_COMPONENT:
	     ical->type = ICAL_EVENT;
	     break;
	case ICAL_VTODO_COMPONENT:
	     ical->type = ICAL_TODO;
	     break;
	case ICAL_VJOURNAL_COMPONENT:
	     ical->type = ICAL_JOURNAL;
	     break;
	case ICAL_VCALENDAR_COMPONENT:
	     /* FIXME: what does this mean? */
	     break;
	case ICAL_VFREEBUSY_COMPONENT:
	     ical->type = ICAL_FBREQUEST;
	     /* NOTE: This is not conclusive- you need to analyze
		properties to determine whether this is an
		FBREQUEST or an FBREPLY */
	     break;
	case ICAL_VTIMEZONE_COMPONENT:
	     ical->type = ICAL_TIMEZONE;
	     break;
	case ICAL_VALARM_COMPONENT:
	case ICAL_XAUDIOALARM_COMPONENT:
	case ICAL_XDISPLAYALARM_COMPONENT:
	case ICAL_XEMAILALARM_COMPONENT:
	case ICAL_XPROCEDUREALARM_COMPONENT:
	     /* this should not be reached, since this loop should
		only be processing first level components */
	     break;
	case ICAL_XSTANDARD_COMPONENT:
	     /* FIXME: what does this mean? */
	     break;
	case ICAL_XDAYLIGHT_COMPONENT:
	     /* FIXME: what does this mean? */
	     break;
	case ICAL_X_COMPONENT:
	     /* FIXME: what does this mean? */
	     break;
	case ICAL_VSCHEDULE_COMPONENT:
	     /* FIXME: what does this mean? */
	     break;
	case ICAL_XLICINVALID_COMPONENT:
	     /* FIXME: what does this mean? */
	     break;
	case ICAL_NO_COMPONENT:
	case ICAL_ANY_COMPONENT:
	     /* should not occur */
	     break;
	case ICAL_VQUERY_COMPONENT:
	case ICAL_VCAR_COMPONENT:
	case ICAL_VCOMMAND_COMPONENT:
	     /* FIXME: what does this mean? */
	     break;
	}
	
	prop = icalcomponent_get_first_property (comp, ICAL_ANY_PROPERTY);
	while (prop) {
		switch (icalproperty_isa (prop)) {
		case ICAL_CALSCALE_PROPERTY:
			if (g_strcasecmp (icalproperty_get_calscale (prop),
					  "GREGORIAN"))
				g_warning ("Unknown calendar format.");
			break;
		case ICAL_METHOD_PROPERTY:
			/* FIXME: implement something here */
			break;
		case ICAL_ATTACH_PROPERTY:
			/* FIXME: not yet implemented */
			break;
		case ICAL_CATEGORIES_PROPERTY:
			copy_to_list (&ical->categories,
				      icalproperty_get_categories (prop));
			break;
		case ICAL_CLASS_PROPERTY:
			copy_str (&ical->class, icalproperty_get_class (prop));
			break;
		case ICAL_COMMENT_PROPERTY:
			/*tmpStr = icalproperty_get_comment (prop);*/
			tmpStr = g_strconcat (icalproperty_get_comment (prop),
					      ical->comment,
					      NULL);
			if (ical->comment)
				g_free (ical->comment);
			ical->comment = tmpStr;
			break;
		case ICAL_DESCRIPTION_PROPERTY:
			copy_str (&ical->desc, 
				  icalproperty_get_description (prop));
			break;
		case ICAL_GEO_PROPERTY:
			geo = icalproperty_get_geo (prop);
			ical->geo.latitude = geo.lat;
			ical->geo.longitude = geo.lon;
			ical->geo.valid = TRUE;
			break;
		case ICAL_LOCATION_PROPERTY:
			copy_str (&ical->location,
				  icalproperty_get_location (prop));
			break;
		case ICAL_PERCENTCOMPLETE_PROPERTY:
			ical->percent = icalproperty_get_percentcomplete (prop);
			break;
		case ICAL_PRIORITY_PROPERTY:
			ical->priority = icalproperty_get_priority (prop);
			if (ical->priority < 0 || ical->priority > 9)
				g_warning ("Priority out-of-range (see RFC2445)");
			break;
		case ICAL_RESOURCES_PROPERTY:
			copy_to_list (&ical->resources,
				      icalproperty_get_resources (prop));
			break;
		case ICAL_STATUS_PROPERTY:
			copy_str (&ical->status,
				  icalproperty_get_status (prop));
			break;
		case ICAL_SUMMARY_PROPERTY:
			copy_str (&ical->summary,
				  icalproperty_get_summary (prop));
			break;
		case ICAL_COMPLETED_PROPERTY:
			ictime = icalproperty_get_completed (prop);
			ical->completed = icaltime_to_timet (&ictime);
			break;
		case ICAL_DTEND_PROPERTY:
			ictime = icalproperty_get_dtend (prop);
			ical->dtend = icaltime_to_timet (&ictime);
			param = icalproperty_get_first_parameter (prop,
								  ICAL_VALUE_PARAMETER);
			ical->date_only = (icalparameter_get_value (param) == 
					   ICAL_VALUE_DATE);
			/* FIXME: We should handle timezone specifiers */
			break;
		case ICAL_DUE_PROPERTY:
			ictime = icalproperty_get_due (prop);
			ical->dtend = icaltime_to_timet (&ictime);
			param = icalproperty_get_first_parameter (prop,
								  ICAL_VALUE_PARAMETER);
			ical->date_only = (icalparameter_get_value (param) == 
					   ICAL_VALUE_DATE);
			/* FIXME: We should handle timezone specifiers */
			break;
		case ICAL_DTSTART_PROPERTY:
			ictime = icalproperty_get_dtstart (prop);
			ical->dtstart = icaltime_to_timet (&ictime);
			param = icalproperty_get_first_parameter (prop,
								  ICAL_VALUE_PARAMETER);
			ical->date_only = (icalparameter_get_value (param) == 
					   ICAL_VALUE_DATE);
			/* FIXME: We should handle timezone specifiers */
			break;
		case ICAL_DURATION_PROPERTY:
			/* FIXME: I don't see the necessary libical function */
			break;
		case ICAL_FREEBUSY_PROPERTY:
			period = icalproperty_get_freebusy (prop);
			ical->dtstart = icaltime_to_timet (&(period.start));
			/* FIXME: period.end is specified as being relative to start, so 
this may not be correct */
			ical->dtend   = icaltime_to_timet (&(period.end));
			break;
		case ICAL_TRANSP_PROPERTY:
			tmpStr = icalproperty_get_transp (prop);
			/* do not i18n the following string constant! */
			if (!g_strcasecmp (tmpStr, "TRANSPARENT"))
				ical->transp = ICAL_TRANSPARENT;
			else
				ical->transp = ICAL_OPAQUE;
			break;
		case ICAL_TZID_PROPERTY:
		case ICAL_TZNAME_PROPERTY:
		case ICAL_TZOFFSETFROM_PROPERTY:
		case ICAL_TZOFFSETTO_PROPERTY:
		case ICAL_TZURL_PROPERTY:
			/* no implementation for now */
			break;
		case ICAL_ATTENDEE_PROPERTY:
			tmpStr = icalproperty_get_attendee (prop);
			person  = parse_person (prop, tmpStr);
			ical->attendee = g_list_prepend (ical->attendee, 
							 person);
			break;
		case ICAL_CONTACT_PROPERTY:
			tmpStr = icalproperty_get_contact (prop);
			person  = parse_person (prop, tmpStr);
			ical->contact = g_list_prepend (ical->contact, person);
			break;
		case ICAL_ORGANIZER_PROPERTY:
			tmpStr = icalproperty_get_organizer (prop);
			person  = parse_person (prop, tmpStr);
			if (ical->organizer)
				g_free (ical->organizer);
			ical->organizer = person;
			break;
		case ICAL_RECURRENCEID_PROPERTY:
			ictime = icalproperty_get_recurrenceid (prop);
			ical->recurid = icaltime_to_timet (&ictime);
			/* FIXME: Range parameter not implemented */
			break;
		case ICAL_RELATEDTO_PROPERTY:
			ical->related = g_list_prepend (ical->related,
							parse_related (prop));
			break;
		case ICAL_URL_PROPERTY:
			copy_str (&ical->url, 
				  icalproperty_get_url (prop));
			break;
		case ICAL_UID_PROPERTY:
			copy_str (&ical->uid,
				  icalproperty_get_uid (prop));
			break;
		case ICAL_EXDATE_PROPERTY:
			/* FIXME: This does not appear to parse
                           multiple exdate values in one property, as
                           allowed by the RFC; needs a libical fix */
			ictime = icalproperty_get_exdate (prop);
			pt = g_new0 (time_t, 1);
			*pt = icaltime_to_timet (&ictime);
			ical->exdate = g_list_prepend (ical->exdate, pt);
			break;
		case ICAL_EXRULE_PROPERTY:
		case ICAL_RDATE_PROPERTY:
		case ICAL_RRULE_PROPERTY:
			/* FIXME: need recursion processing */
			break;
		case ICAL_ACTION_PROPERTY:
		case ICAL_REPEAT_PROPERTY:
		case ICAL_TRIGGER_PROPERTY:
			/* should only occur in VALARM's, handled below */
			g_assert_not_reached();
			break;
		case ICAL_CREATED_PROPERTY:
			ictime = icalproperty_get_created (prop);
			ical->created = icaltime_to_timet (&ictime);
			break;
		case ICAL_DTSTAMP_PROPERTY:
			ictime = icalproperty_get_dtstamp (prop);
			ical->dtstamp = icaltime_to_timet (&ictime);
			break;
		case ICAL_LASTMODIFIED_PROPERTY:
			ictime = icalproperty_get_lastmodified (prop);
			ical->last_mod = icaltime_to_timet (&ictime);
			break;
		case ICAL_SEQUENCE_PROPERTY:
			ical->seq = icalproperty_get_sequence (prop);
			break;
		case ICAL_REQUESTSTATUS_PROPERTY:
			copy_str (&ical->rstatus,
				  icalproperty_get_requeststatus (prop));
			break;
		case ICAL_X_PROPERTY:
			g_warning ("Unsupported X-property: %s",
				   icalproperty_as_ical_string (prop));
			break;
		case ICAL_XLICERROR_PROPERTY:
			g_warning ("Unsupported property: %s",
				   icalproperty_get_xlicerror (prop));
			break;
		case ICAL_PRODID_PROPERTY:
		case ICAL_VERSION_PROPERTY:
			/* nothing to do for this property */
			break;
		default:
			g_warning ("Unsupported property: %s", icalproperty_as_ical_string 
(prop));
			break;
			
		}
		
		prop = icalcomponent_get_next_property (comp,
							ICAL_ANY_PROPERTY);
	}
	
	/* now parse subcomponents --- should only be VALARM's */
	subcomp = icalcomponent_get_first_component (comp,
						     ICAL_ANY_COMPONENT);
	while (subcomp) {
		compType = icalcomponent_isa (subcomp);
		switch (compType) {
		case ICAL_VALARM_COMPONENT:
			alarm = parse_alarm (subcomp);
			if (alarm)
				ical->alarms = g_list_prepend (ical->alarms,
							       alarm);
			break;
		default:
			g_warning ("Only nested VALARM components are supported.");
		}

		subcomp = icalcomponent_get_next_component (comp,
							    ICAL_ANY_COMPONENT);
	}

	return ical;
}


static time_t icaltime_to_timet (struct icaltimetype* i)
{
	struct tm t;
	time_t ret;

	t.tm_year = i->year - 1900;
	t.tm_mon  = i->month - 1;
	t.tm_mday = i->day;
	if (!i->is_date) {
		t.tm_hour = i->hour;
		t.tm_min  = i->minute;
		t.tm_sec  = i->second;
	} else {
		t.tm_hour = 0;
		t.tm_min  = 0;
		t.tm_sec  = 0;
	}

	ret = mktime(&t);

	if (i->is_utc) {
#ifdef HAVE_TIMEZONE
	  	extern long timezone;
		ret -= timezone;
#else
		struct tm *tmp;
		time_t tod = time(NULL);
		tmp = localtime (&tod);
		ret += tmp->tm_gmtoff;
#endif
	} 

	return ret;
}
	
static iCalPerson*
parse_person (icalproperty* prop, gchar* value)
{
	icalparameter* param;
	icalparameter_role role;
	icalparameter_partstat partstat;
	icalparameter_cutype cutype;

	iCalPerson* ret;
	
	ret = g_new0 (iCalPerson, 1);

	ret->addr = g_strdup (value);

	param = icalproperty_get_first_parameter (prop,
						  ICAL_CN_PARAMETER);
	ret->name = g_strdup (icalparameter_get_cn (param));
	
	param = icalproperty_get_first_parameter (prop,
						  ICAL_ROLE_PARAMETER);
	if (param) {
		role  = icalparameter_get_role (param);
		switch (role) {
		case ICAL_ROLE_CHAIR:
			ret->role = g_strdup ("CHAIR");
			break;
		case ICAL_ROLE_REQPARTICIPANT:
			ret->role = g_strdup ("REQPARTICIPANT");
			break;
		case ICAL_ROLE_OPTPARTICIPANT:
			ret->role = g_strdup ("OPTPARTICIPANT");
			break;
		case ICAL_ROLE_NONPARTICIPANT:
			ret->role = g_strdup ("NONPARTICIPANT");
			break;
		case ICAL_ROLE_XNAME:
		default:
			ret->role = g_strdup ("UNKNOWN");
			break;
		}
	} else
		ret->role = g_strdup ("REQPARTICIPANT");
	
	param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
	if (param) {
		partstat = icalparameter_get_partstat (param);
		switch (partstat) {
		case ICAL_PARTSTAT_NEEDSACTION:
			ret->partstat = g_strdup ("NEEDSACTION");
			break;
		case ICAL_PARTSTAT_ACCEPTED:
			ret->partstat = g_strdup ("ACCEPTED");
			break;
		case ICAL_PARTSTAT_DECLINED:
			ret->partstat = g_strdup ("DECLINED");
			break;
		case ICAL_PARTSTAT_TENTATIVE:
			ret->partstat = g_strdup ("TENTATIVE");
			break;
		case ICAL_PARTSTAT_DELEGATED:
			ret->partstat = g_strdup ("DELEGATED");
			break;
		case ICAL_PARTSTAT_COMPLETED:
			ret->partstat = g_strdup ("COMPLETED");
			break;
		case ICAL_PARTSTAT_INPROCESS:
			ret->partstat = g_strdup ("INPROCESS");
			break;
		case ICAL_PARTSTAT_XNAME:
			ret->partstat = g_strdup (icalparameter_get_xvalue (param));
			break;
		default:
			ret->partstat = g_strdup ("UNKNOWN");
			break;
		}
	} else
		ret->partstat = g_strdup ("NEEDSACTION");

	param = icalproperty_get_first_parameter (prop, ICAL_RSVP_PARAMETER);
	if (param)
		ret->rsvp = icalparameter_get_rsvp (param);
	else
		ret->rsvp = FALSE;

	param = icalproperty_get_first_parameter (prop, ICAL_CUTYPE_PARAMETER
);
	if (param) {
		cutype = icalparameter_get_cutype (param);
		switch (cutype) {
		case ICAL_CUTYPE_INDIVIDUAL:
			ret->cutype = g_strdup ("INDIVIDUAL");
			break;
		case ICAL_CUTYPE_GROUP:
			ret->cutype = g_strdup ("GROUP");
			break;
		case ICAL_CUTYPE_RESOURCE:
			ret->cutype = g_strdup ("RESOURCE");
			break;
		case ICAL_CUTYPE_ROOM:
			ret->cutype = g_strdup ("ROOM");
			break;
		case ICAL_CUTYPE_UNKNOWN:
		case ICAL_CUTYPE_XNAME:
		default:
			ret->cutype = g_strdup ("UNKNOWN");
			break;
		}
	} else 
		ret->cutype = g_strdup ("INDIVIDUAL");
	
	param = icalproperty_get_first_parameter (prop, ICAL_MEMBER_PARAMETER
);
	while (param) {
		copy_to_list (&ret->member, icalparameter_get_member (param));
		param = icalproperty_get_next_parameter (prop, 
							 ICAL_MEMBER_PARAMETER);
	}
	
	param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDTO_PARAMETER);
	while (param) {
		copy_to_list (&ret->deleg_to, 
			      icalparameter_get_delegatedto (param));
		param = icalproperty_get_next_parameter (prop,
							 ICAL_DELEGATEDTO_PARAMETER);
	}

	param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDFROM_PARAMETER);
	while (param) {
		copy_to_list (&ret->deleg_from, 
			      icalparameter_get_delegatedfrom (param));
		param = icalproperty_get_next_parameter (prop,
							 ICAL_DELEGATEDFROM_PARAMETER);
	}

	param = icalproperty_get_first_parameter (prop, ICAL_SENTBY_PARAMETER
);
	copy_str (&ret->sent_by, 
		  icalparameter_get_sentby (param));

	param = icalproperty_get_first_parameter (prop, ICAL_DIR_PARAMETER);
	while (param) {
		copy_to_list (&ret->deleg_to, 
			      icalparameter_get_delegatedto (param));
		param = icalproperty_get_next_parameter (prop,
							 ICAL_DIR_PARAMETER);
	}

	return ret;
}

static iCalRelation* 
parse_related (icalproperty* prop)
{
	iCalRelation* rel;
	icalparameter* param;
	icalparameter_reltype type;

	rel = g_new0 (iCalRelation, 1);
	rel->uid = g_strdup (icalproperty_get_relatedto (prop));
	
	param = icalproperty_get_first_parameter (prop, 
						  ICAL_RELTYPE_PARAMETER);
	if (param) {
		type = icalparameter_get_reltype (param);
		switch (type) {
		case ICAL_RELTYPE_PARENT:
			rel->reltype = g_strdup ("PARENT");
			break;
		case ICAL_RELTYPE_CHILD:
			rel->reltype = g_strdup ("CHILD");
			break;
		case ICAL_RELTYPE_SIBLING:
			rel->reltype = g_strdup ("SIBLING");
			break;
		case ICAL_RELTYPE_XNAME:
			rel->reltype = g_strdup (icalparameter_get_xvalue (param));
			break;
		default:
			rel->reltype = g_strdup ("UNKNOWN");
			break;
		}
	} else
		rel->reltype = g_strdup ("PARENT");

	return rel;
}
	
#ifdef TEST

int main(int argc, char* argv[])
{
	icalcomponent* comp;
	comp = icalendar_parse_file (argv[1]);
	printf ("%s\n", icalcomponent_as_ical_string (comp));
	return 0;
}

#endif
	
	
static CalendarAlarm*
parse_alarm (icalcomponent* comp)
{
	CalendarAlarm *alarm;
	icalproperty  *prop;
	char          *tmpStr;
	struct icaldurationtype dur;
	struct icalattachtype attach;

	g_return_val_if_fail (comp != NULL, NULL);
	
	alarm = g_new0 (CalendarAlarm, 1);
	
	prop = icalcomponent_get_first_property (comp, ICAL_ANY_PROPERTY);
	while (prop) {
		switch (icalproperty_isa (prop)) {
		case ICAL_ACTION_PROPERTY:
			tmpStr = icalproperty_get_action (prop);
			if (!g_strcasecmp (tmpStr, "AUDIO"))
				alarm->type = ALARM_AUDIO;
			else if (!g_strcasecmp (tmpStr, "DISPLAY"))
				alarm->type = ALARM_DISPLAY;
			else if (!g_strcasecmp (tmpStr, "EMAIL"))
				alarm->type = ALARM_MAIL;
			else if (!g_strcasecmp (tmpStr, "PROCEDURE"))
				alarm->type = ALARM_PROGRAM;
			else
				g_warning ("Unsupported alarm type!");
			break;
		case ICAL_TRIGGER_PROPERTY:
			/* FIXME: waiting on proper libical support */
			break;
		case ICAL_REPEAT_PROPERTY:
			alarm->snooze_repeat = icalproperty_get_repeat (prop);
			break;
		case ICAL_DURATION_PROPERTY:
			dur = icalproperty_get_duration (prop);
			alarm->snooze_secs = icaldurationtype_as_timet (dur);
			break;
		case ICAL_ATTACH_PROPERTY:
			attach = icalproperty_get_attach (prop);
			copy_str (&alarm->attach,
				  icalattachtype_get_url (&attach));
			break;
		case ICAL_DESCRIPTION_PROPERTY:
			copy_str (&alarm->desc,
				  icalproperty_get_description (prop));
			break;
		case ICAL_SUMMARY_PROPERTY:
			copy_str (&alarm->summary,
				  icalproperty_get_summary (prop));
			break;
		case ICAL_ATTENDEE_PROPERTY:
			copy_str (&alarm->attendee,
				  icalproperty_get_attendee (prop));
			break;
		default:
			g_warning ("Unsupported alarm property: %s",
				   icalproperty_as_ical_string (prop));
			break;
		}
		
		prop = icalcomponent_get_next_property (comp, 
							ICAL_ANY_PROPERTY);
	}

	return alarm;
}
