/* Evolution calendar - iCalendar component object
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include "cal-component.h"
#include "timeutil.h"



/* Extension property for alarm components so that we can reference them by UID */
#define EVOLUTION_ALARM_UID_PROPERTY "X-EVOLUTION-ALARM-UID"


struct attendee {
	icalproperty *prop;
	icalparameter *cutype_param;
	icalparameter *member_param;
	icalparameter *role_param;
	icalparameter *partstat_param;
	icalparameter *rsvp_param;
	icalparameter *delto_param;
	icalparameter *delfrom_param;
	icalparameter *sentby_param;
	icalparameter *cn_param;
	icalparameter *language_param;
};

/* Private part of the CalComponent structure */
struct _CalComponentPrivate {
	/* The icalcomponent we wrap */
	icalcomponent *icalcomp;

	/* Properties */

	icalproperty *uid;

	icalproperty *status;
	GSList *attendee_list;
	
	icalproperty *categories;

	icalproperty *classification;

	struct text {
		icalproperty *prop;
		icalparameter *altrep_param;
	};

	GSList *comment_list; /* list of struct text */

	icalproperty *completed;

	GSList *contact_list; /* list of struct text */

	icalproperty *created;

	GSList *description_list; /* list of struct text */

	struct datetime {
		icalproperty *prop;
		icalparameter *tzid_param;
	};

	struct datetime dtstart;
	struct datetime dtend;

	icalproperty *dtstamp;

	/* The DURATION property can be used instead of the VEVENT DTEND or
	   the VTODO DUE dates. We do not use it directly ourselves, but we
	   must be able to handle it from incoming data. If a DTEND or DUE
	   is requested, we convert the DURATION if necessary. If DTEND or
	   DUE is set, we remove any DURATION. */
	icalproperty *duration;

	struct datetime due;

	GSList *exdate_list; /* list of struct datetime */
	GSList *exrule_list; /* list of icalproperty objects */

	struct organizer {
		icalproperty *prop;
		icalparameter *sentby_param;
		icalparameter *cn_param;
		icalparameter *language_param;
	};

	struct organizer organizer;
	
	icalproperty *geo;
	icalproperty *last_modified;
	icalproperty *percent;
	icalproperty *priority;

	struct period {
		icalproperty *prop;
		icalparameter *value_param;
	};

	struct recur_id {
		struct datetime recur_time;
		
		icalparameter *range_param;
	};

	struct recur_id recur_id;
	
	GSList *rdate_list; /* list of struct period */

	GSList *rrule_list; /* list of icalproperty objects */

	icalproperty *sequence;

	struct {
		icalproperty *prop;
		icalparameter *altrep_param;
	} summary;

	icalproperty *transparency;
	icalproperty *url;
	icalproperty *location;

	/* Subcomponents */

	GHashTable *alarm_uid_hash;

	/* Whether we should increment the sequence number when piping the
	 * object over the wire.
	 */
	guint need_sequence_inc : 1;
};

/* Private structure for alarms */
struct _CalComponentAlarm {
	/* Alarm icalcomponent we wrap */
	icalcomponent *icalcomp;

	/* Our extension UID property */
	icalproperty *uid;

	/* Properties */

	icalproperty *action;
	icalproperty *attach; /* FIXME: see scan_alarm_property() below */

	struct {
		icalproperty *prop;
		icalparameter *altrep_param;
	} description;

	icalproperty *duration;
	icalproperty *repeat;
	icalproperty *trigger;

	GSList *attendee_list;
};



static void cal_component_class_init (CalComponentClass *klass);
static void cal_component_init (CalComponent *comp, CalComponentClass *klass);
static void cal_component_finalize (GObject *object);

static GObjectClass *parent_class;



/**
 * cal_component_get_type:
 *
 * Registers the #CalComponent class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalComponent class.
 **/
GType
cal_component_get_type (void)
{
	static GType cal_component_type = 0;

	if (!cal_component_type) {
		static GTypeInfo info = {
                        sizeof (CalComponentClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) cal_component_class_init,
                        NULL, NULL,
                        sizeof (CalComponent),
                        0,
                        (GInstanceInitFunc) cal_component_init
                };
		cal_component_type = g_type_register_static (G_TYPE_OBJECT, "CalComponent", &info, 0);
	}

	return cal_component_type;
}

/* Class initialization function for the calendar component object */
static void
cal_component_class_init (CalComponentClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = cal_component_finalize;
}

/* Object initialization function for the calendar component object */
static void
cal_component_init (CalComponent *comp, CalComponentClass *klass)
{
	CalComponentPrivate *priv;

	priv = g_new0 (CalComponentPrivate, 1);
	comp->priv = priv;

	priv->alarm_uid_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

/* Does a simple g_free() of the elements of a GSList and then frees the list
 * itself.  Returns NULL.
 */
static GSList *
free_slist (GSList *slist)
{
	GSList *l;

	for (l = slist; l; l = l->next)
		g_free (l->data);

	g_slist_free (slist);
	return NULL;
}

/* Used from g_hash_table_foreach_remove() to free the alarm UIDs hash table.
 * We do not need to do anything to individual elements since we were storing
 * the UID pointers inside the icalproperties themselves.
 */
static gboolean
free_alarm_cb (gpointer key, gpointer value, gpointer data)
{
	return TRUE;
}

/* Frees the internal icalcomponent only if it does not have a parent.  If it
 * does, it means we don't own it and we shouldn't free it.
 */
static void
free_icalcomponent (CalComponent *comp, gboolean free)
{
	CalComponentPrivate *priv;
	GSList *l;
	
	priv = comp->priv;

	if (!priv->icalcomp)
		return;

	/* Free the icalcomponent */

	if (free && icalcomponent_get_parent (priv->icalcomp) == NULL) {
		icalcomponent_free (priv->icalcomp);
		priv->icalcomp = NULL;
	}
	
	/* Free the mappings */

	priv->uid = NULL;

	priv->status = NULL;

	for (l = priv->attendee_list; l != NULL; l = l->next)
		g_free (l->data);
	g_slist_free (priv->attendee_list);
	priv->attendee_list = NULL;
	
	priv->categories = NULL;

	priv->classification = NULL;
	priv->comment_list = NULL;
	priv->completed = NULL;
	priv->contact_list = NULL;
	priv->created = NULL;

	priv->description_list = free_slist (priv->description_list);

	priv->dtend.prop = NULL;
	priv->dtend.tzid_param = NULL;

	priv->dtstamp = NULL;

	priv->dtstart.prop = NULL;
	priv->dtstart.tzid_param = NULL;

	priv->due.prop = NULL;
	priv->due.tzid_param = NULL;

	priv->duration = NULL;

	priv->exdate_list = free_slist (priv->exdate_list);

	g_slist_free (priv->exrule_list);
	priv->exrule_list = NULL;

	priv->geo = NULL;
	priv->last_modified = NULL;
	priv->percent = NULL;
	priv->priority = NULL;

	priv->rdate_list = free_slist (priv->rdate_list);

	g_slist_free (priv->rrule_list);
	priv->rrule_list = NULL;

	priv->sequence = NULL;

	priv->summary.prop = NULL;
	priv->summary.altrep_param = NULL;

	priv->transparency = NULL;
	priv->url = NULL;
	priv->location = NULL;

	/* Free the subcomponents */

	g_hash_table_foreach_remove (priv->alarm_uid_hash, free_alarm_cb, NULL);

	/* Clean up */

	priv->need_sequence_inc = FALSE;
}

/* Finalize handler for the calendar component object */
static void
cal_component_finalize (GObject *object)
{
	CalComponent *comp;
	CalComponentPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (object));

	comp = CAL_COMPONENT (object);
	priv = comp->priv;

	free_icalcomponent (comp, TRUE);
	g_hash_table_destroy (priv->alarm_uid_hash);
	priv->alarm_uid_hash = NULL;

	g_free (priv);
	comp->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/**
 * cal_component_gen_uid:
 *
 * Generates a unique identifier suitable for calendar components.
 *
 * Return value: A unique identifier string.  Every time this function is called
 * a different string is returned.
 **/
char *
cal_component_gen_uid (void)
{
        char *iso, *ret;
	static char *hostname;
	time_t t = time (NULL);
	static int serial;

	if (!hostname) {
		static char buffer [512];

		if ((gethostname (buffer, sizeof (buffer) - 1) == 0) &&
		    (buffer [0] != 0))
			hostname = buffer;
		else
			hostname = "localhost";
	}

	iso = isodate_from_time_t (t);
	ret = g_strdup_printf ("%s-%d-%d-%d-%d@%s",
			       iso,
			       getpid (),
			       getgid (),
			       getppid (),
			       serial++,
			       hostname);
	g_free (iso);

	return ret;
}

/**
 * cal_component_new:
 *
 * Creates a new empty calendar component object.  You should set it from an
 * #icalcomponent structure by using cal_component_set_icalcomponent() or with a
 * new empty component type by using cal_component_set_new_vtype().
 *
 * Return value: A newly-created calendar component object.
 **/
CalComponent *
cal_component_new (void)
{
	return CAL_COMPONENT (g_object_new (CAL_COMPONENT_TYPE, NULL));
}

/**
 * cal_component_clone:
 * @comp: A calendar component object.
 *
 * Creates a new calendar component object by copying the information from
 * another one.
 *
 * Return value: A newly-created calendar component with the same values as the
 * original one.
 **/
CalComponent *
cal_component_clone (CalComponent *comp)
{
	CalComponentPrivate *priv;
	CalComponent *new_comp;
	icalcomponent *new_icalcomp;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), NULL);

	priv = comp->priv;
	g_return_val_if_fail (priv->need_sequence_inc == FALSE, NULL);

	new_comp = cal_component_new ();

	if (priv->icalcomp) {
		new_icalcomp = icalcomponent_new_clone (priv->icalcomp);
		cal_component_set_icalcomponent (new_comp, new_icalcomp);
	}

	return new_comp;
}

/* Scans an attendee property */
static void
scan_attendee (GSList **attendee_list, icalproperty *prop) 
{
	struct attendee *attendee;
	
	attendee = g_new (struct attendee, 1);
	attendee->prop = prop;

	attendee->cutype_param = icalproperty_get_first_parameter (prop, ICAL_CUTYPE_PARAMETER);
	attendee->member_param = icalproperty_get_first_parameter (prop, ICAL_MEMBER_PARAMETER);
	attendee->role_param = icalproperty_get_first_parameter (prop, ICAL_ROLE_PARAMETER);
	attendee->partstat_param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
	attendee->rsvp_param = icalproperty_get_first_parameter (prop, ICAL_RSVP_PARAMETER);
	attendee->delto_param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDTO_PARAMETER);
	attendee->delfrom_param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDFROM_PARAMETER);
	attendee->sentby_param = icalproperty_get_first_parameter (prop, ICAL_SENTBY_PARAMETER);
	attendee->cn_param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
	attendee->language_param = icalproperty_get_first_parameter (prop, ICAL_LANGUAGE_PARAMETER);
	
	*attendee_list = g_slist_append (*attendee_list, attendee);
}

/* Scans a date/time and timezone pair property */
static void
scan_datetime (CalComponent *comp, struct datetime *datetime, icalproperty *prop)
{
	CalComponentPrivate *priv;

	priv = comp->priv;

	datetime->prop = prop;
	datetime->tzid_param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);
}

/* Scans an exception date property */
static void
scan_exdate (CalComponent *comp, icalproperty *prop)
{
	CalComponentPrivate *priv;
	struct datetime *dt;

	priv = comp->priv;

	dt = g_new (struct datetime, 1);
	dt->prop = prop;
	dt->tzid_param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);

	priv->exdate_list = g_slist_append (priv->exdate_list, dt);
}

/* Scans and attendee property */
static void
scan_organizer (CalComponent *comp, struct organizer *organizer, icalproperty *prop) 
{
	organizer->prop = prop;

	organizer->sentby_param = icalproperty_get_first_parameter (prop, ICAL_SENTBY_PARAMETER);
	organizer->cn_param = icalproperty_get_first_parameter (prop, ICAL_CN_PARAMETER);
	organizer->language_param = icalproperty_get_first_parameter (prop, ICAL_LANGUAGE_PARAMETER);
}

/* Scans an icalperiodtype property */
static void
scan_period (CalComponent *comp, GSList **list, icalproperty *prop)
{
	struct period *period;

	period = g_new (struct period, 1);
	period->prop = prop;
	period->value_param = icalproperty_get_first_parameter (prop, ICAL_VALUE_PARAMETER);

	*list = g_slist_append (*list, period);
}


/* Scans an icalrecurtype property */
static void
scan_recur_id (CalComponent *comp, struct recur_id *recur_id, icalproperty *prop)
{
	scan_datetime (comp, &recur_id->recur_time, prop);

	recur_id->range_param = icalproperty_get_first_parameter (prop, ICAL_RANGE_PARAMETER);
}

/* Scans an icalrecurtype property */
static void
scan_recur (CalComponent *comp, GSList **list, icalproperty *prop)
{
	*list = g_slist_append (*list, prop);
}

/* Scans the summary property */
static void
scan_summary (CalComponent *comp, icalproperty *prop)
{
	CalComponentPrivate *priv;

	priv = comp->priv;

	priv->summary.prop = prop;
	priv->summary.altrep_param = icalproperty_get_first_parameter (prop, ICAL_ALTREP_PARAMETER);
}

/* Scans a text (i.e. text + altrep) property */
static void
scan_text (CalComponent *comp, GSList **text_list, icalproperty *prop)
{
	struct text *text;

	text = g_new (struct text, 1);
	text->prop = prop;
	text->altrep_param = icalproperty_get_first_parameter (prop, ICAL_ALTREP_PARAMETER);

	*text_list = g_slist_append (*text_list, text);
}

/* Scans an icalproperty and adds its mapping to the component */
static void
scan_property (CalComponent *comp, icalproperty *prop)
{
	CalComponentPrivate *priv;
	icalproperty_kind kind;

	priv = comp->priv;

	kind = icalproperty_isa (prop);

	switch (kind) {
	case ICAL_STATUS_PROPERTY:
		priv->status = prop;
		break;

	case ICAL_ATTENDEE_PROPERTY:
		scan_attendee (&priv->attendee_list, prop);
		break;

	case ICAL_CATEGORIES_PROPERTY:
		priv->categories = prop;
		break;

	case ICAL_CLASS_PROPERTY:
		priv->classification = prop;
		break;

	case ICAL_COMMENT_PROPERTY:
		scan_text (comp, &priv->comment_list, prop);
		break;

	case ICAL_COMPLETED_PROPERTY:
		priv->completed = prop;
		break;

	case ICAL_CONTACT_PROPERTY:
		scan_text (comp, &priv->contact_list, prop);
		break;

	case ICAL_CREATED_PROPERTY:
		priv->created = prop;
		break;

	case ICAL_DESCRIPTION_PROPERTY:
		scan_text (comp, &priv->description_list, prop);
		break;

	case ICAL_DTEND_PROPERTY:
		scan_datetime (comp, &priv->dtend, prop);
		break;

	case ICAL_DTSTAMP_PROPERTY:
		priv->dtstamp = prop;
		break;

	case ICAL_DTSTART_PROPERTY:
		scan_datetime (comp, &priv->dtstart, prop);
		break;

	case ICAL_DUE_PROPERTY:
		scan_datetime (comp, &priv->due, prop);
		break;

	case ICAL_DURATION_PROPERTY:
		priv->duration = prop;
		break;

	case ICAL_EXDATE_PROPERTY:
		scan_exdate (comp, prop);
		break;

	case ICAL_EXRULE_PROPERTY:
		scan_recur (comp, &priv->exrule_list, prop);
		break;

	case ICAL_GEO_PROPERTY:
		priv->geo = prop;
		break;

	case ICAL_LASTMODIFIED_PROPERTY:
		priv->last_modified = prop;
		break;

	case ICAL_ORGANIZER_PROPERTY:
		scan_organizer (comp, &priv->organizer, prop);
		break;

	case ICAL_PERCENTCOMPLETE_PROPERTY:
		priv->percent = prop;
		break;

	case ICAL_PRIORITY_PROPERTY:
		priv->priority = prop;
		break;

	case ICAL_RECURRENCEID_PROPERTY:
		scan_recur_id (comp, &priv->recur_id, prop);
		break;

	case ICAL_RDATE_PROPERTY:
		scan_period (comp, &priv->rdate_list, prop);
		break;

	case ICAL_RRULE_PROPERTY:
		scan_recur (comp, &priv->rrule_list, prop);
		break;

	case ICAL_SEQUENCE_PROPERTY:
		priv->sequence = prop;
		break;

	case ICAL_SUMMARY_PROPERTY:
		scan_summary (comp, prop);
		break;

	case ICAL_TRANSP_PROPERTY:
		priv->transparency = prop;
		break;

	case ICAL_UID_PROPERTY:
		priv->uid = prop;
		break;

	case ICAL_URL_PROPERTY:
		priv->url = prop;
		break;

	case ICAL_LOCATION_PROPERTY :
		priv->location = prop;
		break;

	default:
		break;
	}
}

/* Gets our alarm UID string from a property that is known to contain it */
static const char *
alarm_uid_from_prop (icalproperty *prop)
{
	const char *xstr;

	g_assert (icalproperty_isa (prop) == ICAL_X_PROPERTY);

	xstr = icalproperty_get_x (prop);
	g_assert (xstr != NULL);

	return xstr;
}

/* Sets our alarm UID extension property on an alarm component.  Returns a
 * pointer to the UID string inside the property itself.
 */
static const char *
set_alarm_uid (icalcomponent *alarm, const char *auid)
{
	icalproperty *prop;
	const char *inprop_auid;

	/* Create the new property */

	prop = icalproperty_new_x ((char *) auid);
	icalproperty_set_x_name (prop, EVOLUTION_ALARM_UID_PROPERTY);

	icalcomponent_add_property (alarm, prop);

	inprop_auid = alarm_uid_from_prop (prop);
	return inprop_auid;
}

/* Removes any alarm UID extension properties from an alarm subcomponent */
static void
remove_alarm_uid (icalcomponent *alarm)
{
	icalproperty *prop;
	GSList *list, *l;

	list = NULL;

	for (prop = icalcomponent_get_first_property (alarm, ICAL_X_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (alarm, ICAL_X_PROPERTY)) {
		const char *xname;

		xname = icalproperty_get_x_name (prop);
		g_assert (xname != NULL);

		if (strcmp (xname, EVOLUTION_ALARM_UID_PROPERTY) == 0)
			list = g_slist_prepend (list, prop);
	}

	for (l = list; l; l = l->next) {
		prop = l->data;
		icalcomponent_remove_property (alarm, prop);
		icalproperty_free (prop);
	}

	g_slist_free (list);
}

/* Adds an alarm subcomponent to the calendar component's mapping table.  The
 * actual UID with which it gets added may not be the same as the specified one;
 * this function will change it if the table already had an alarm subcomponent
 * with the specified UID.  Returns the actual UID used.
 */
static const char *
add_alarm (CalComponent *comp, icalcomponent *alarm, const char *auid)
{
	CalComponentPrivate *priv;
	icalcomponent *old_alarm;

	priv = comp->priv;

	/* First we see if we already have an alarm with the requested UID.  In
	 * that case, we need to change the new UID to something else.  This
	 * should never happen, but who knows.
	 */

	old_alarm = g_hash_table_lookup (priv->alarm_uid_hash, auid);
	if (old_alarm != NULL) {
		char *new_auid;

		g_message ("add_alarm(): Got alarm with duplicated UID `%s', changing it...", auid);

		remove_alarm_uid (alarm);

		new_auid = cal_component_gen_uid ();
		auid = set_alarm_uid (alarm, new_auid);
		g_free (new_auid);
	}

	g_hash_table_insert (priv->alarm_uid_hash, (char *) auid, alarm);
	return auid;
}

/* Scans an alarm subcomponent, adds an UID extension property to it (so that we
 * can reference alarms by unique IDs), and adds its mapping to the component.  */
static void
scan_alarm (CalComponent *comp, icalcomponent *alarm)
{
	CalComponentPrivate *priv;
	icalproperty *prop;
	const char *auid;
	char *new_auid;

	priv = comp->priv;

	for (prop = icalcomponent_get_first_property (alarm, ICAL_X_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (alarm, ICAL_X_PROPERTY)) {
		const char *xname;

		xname = icalproperty_get_x_name (prop);
		g_assert (xname != NULL);

		if (strcmp (xname, EVOLUTION_ALARM_UID_PROPERTY) == 0) {
			auid = alarm_uid_from_prop (prop);
			add_alarm (comp, alarm, auid);
			return;
		}
	}

	/* The component has no alarm UID property, so we create one. */

	new_auid = cal_component_gen_uid ();
	auid = set_alarm_uid (alarm, new_auid);
	g_free (new_auid);

	add_alarm (comp, alarm, auid);
}

/* Scans an icalcomponent for its properties so that we can provide
 * random-access to them.  It also builds a hash table of the component's alarm
 * subcomponents.
 */
static void
scan_icalcomponent (CalComponent *comp)
{
	CalComponentPrivate *priv;
	icalproperty *prop;
	icalcompiter iter;

	priv = comp->priv;

	g_assert (priv->icalcomp != NULL);

	/* Scan properties */

	for (prop = icalcomponent_get_first_property (priv->icalcomp, ICAL_ANY_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (priv->icalcomp, ICAL_ANY_PROPERTY))
		scan_property (comp, prop);

	/* Scan subcomponents */

	for (iter = icalcomponent_begin_component (priv->icalcomp, ICAL_VALARM_COMPONENT);
	     icalcompiter_deref (&iter) != NULL;
	     icalcompiter_next (&iter)) {
		icalcomponent *subcomp;

		subcomp = icalcompiter_deref (&iter);
		scan_alarm (comp, subcomp);
	}
}

/* Ensures that the mandatory calendar component properties (uid, dtstamp) do
 * exist.  If they don't exist, it creates them automatically.
 */
static void
ensure_mandatory_properties (CalComponent *comp)
{
	CalComponentPrivate *priv;

	priv = comp->priv;
	g_assert (priv->icalcomp != NULL);

	if (!priv->uid) {
		char *uid;

		uid = cal_component_gen_uid ();
		priv->uid = icalproperty_new_uid (uid);
		g_free (uid);

		icalcomponent_add_property (priv->icalcomp, priv->uid);
	}

	if (!priv->dtstamp) {
		struct icaltimetype t;

		t = icaltime_current_time_with_zone (icaltimezone_get_utc_timezone ());

		priv->dtstamp = icalproperty_new_dtstamp (t);
		icalcomponent_add_property (priv->icalcomp, priv->dtstamp);
	}
}

/**
 * cal_component_set_new_vtype:
 * @comp: A calendar component object.
 * @type: Type of calendar component to create.
 *
 * Clears any existing component data from a calendar component object and
 * creates a new #icalcomponent of the specified type for it.  The only property
 * that will be set in the new component will be its unique identifier.
 **/
void
cal_component_set_new_vtype (CalComponent *comp, CalComponentVType type)
{
	CalComponentPrivate *priv;
	icalcomponent *icalcomp;
	icalcomponent_kind kind;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;

	free_icalcomponent (comp, TRUE);

	if (type == CAL_COMPONENT_NO_TYPE)
		return;

	/* Figure out the kind and create the icalcomponent */

	switch (type) {
	case CAL_COMPONENT_EVENT:
		kind = ICAL_VEVENT_COMPONENT;
		break;

	case CAL_COMPONENT_TODO:
		kind = ICAL_VTODO_COMPONENT;
		break;

	case CAL_COMPONENT_JOURNAL:
		kind = ICAL_VJOURNAL_COMPONENT;
		break;

	case CAL_COMPONENT_FREEBUSY:
		kind = ICAL_VFREEBUSY_COMPONENT;
		break;

	case CAL_COMPONENT_TIMEZONE:
		kind = ICAL_VTIMEZONE_COMPONENT;
		break;

	default:
		g_assert_not_reached ();
		kind = ICAL_NO_COMPONENT;
	}

	icalcomp = icalcomponent_new (kind);
	if (!icalcomp) {
		g_message ("cal_component_set_new_vtype(): Could not create the icalcomponent!");
		return;
	}

	/* Scan the component to build our mapping table */

	priv->icalcomp = icalcomp;
	scan_icalcomponent (comp);

	/* Add missing stuff */

	ensure_mandatory_properties (comp);
}

/**
 * cal_component_set_icalcomponent:
 * @comp: A calendar component object.
 * @icalcomp: An #icalcomponent.
 *
 * Sets the contents of a calendar component object from an #icalcomponent
 * structure.  If the @comp already had an #icalcomponent set into it, it will
 * will be freed automatically if the #icalcomponent does not have a parent
 * component itself.
 *
 * Supported component types are VEVENT, VTODO, VJOURNAL, VFREEBUSY, and VTIMEZONE.
 *
 * Return value: TRUE on success, FALSE if @icalcomp is an unsupported component
 * type.
 **/
gboolean
cal_component_set_icalcomponent (CalComponent *comp, icalcomponent *icalcomp)
{
	CalComponentPrivate *priv;
	icalcomponent_kind kind;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);

	priv = comp->priv;

	if (priv->icalcomp == icalcomp)
		return TRUE;

	free_icalcomponent (comp, TRUE);

	if (!icalcomp) {
		priv->icalcomp = NULL;
		return TRUE;
	}

	kind = icalcomponent_isa (icalcomp);

	if (!(kind == ICAL_VEVENT_COMPONENT
	      || kind == ICAL_VTODO_COMPONENT
	      || kind == ICAL_VJOURNAL_COMPONENT
	      || kind == ICAL_VFREEBUSY_COMPONENT
	      || kind == ICAL_VTIMEZONE_COMPONENT))
		return FALSE;

	priv->icalcomp = icalcomp;

	scan_icalcomponent (comp);
	ensure_mandatory_properties (comp);

	return TRUE;
}

/**
 * cal_component_get_icalcomponent:
 * @comp: A calendar component object.
 *
 * Queries the #icalcomponent structure that a calendar component object is
 * wrapping.
 *
 * Return value: An #icalcomponent structure, or NULL if the @comp has no
 * #icalcomponent set to it.
 **/
icalcomponent *
cal_component_get_icalcomponent (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), NULL);

	priv = comp->priv;
	g_return_val_if_fail (priv->need_sequence_inc == FALSE, NULL);

	return priv->icalcomp;
}

void
cal_component_rescan (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;

	/* Clear everything out */
	free_icalcomponent (comp, FALSE);

	/* Rescan */
	scan_icalcomponent (comp);
	ensure_mandatory_properties (comp);
}

void
cal_component_strip_errors (CalComponent *comp)
{
	CalComponentPrivate *priv;
	
	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;

	icalcomponent_strip_errors (priv->icalcomp);
}

/**
 * cal_component_get_vtype:
 * @comp: A calendar component object.
 *
 * Queries the type of a calendar component object.
 *
 * Return value: The type of the component, as defined by RFC 2445.
 **/
CalComponentVType
cal_component_get_vtype (CalComponent *comp)
{
	CalComponentPrivate *priv;
	icalcomponent_kind kind;

	g_return_val_if_fail (comp != NULL, CAL_COMPONENT_NO_TYPE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), CAL_COMPONENT_NO_TYPE);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, CAL_COMPONENT_NO_TYPE);

	kind = icalcomponent_isa (priv->icalcomp);
	switch (kind) {
	case ICAL_VEVENT_COMPONENT:
		return CAL_COMPONENT_EVENT;

	case ICAL_VTODO_COMPONENT:
		return CAL_COMPONENT_TODO;

	case ICAL_VJOURNAL_COMPONENT:
		return CAL_COMPONENT_JOURNAL;

	case ICAL_VFREEBUSY_COMPONENT:
		return CAL_COMPONENT_FREEBUSY;

	case ICAL_VTIMEZONE_COMPONENT:
		return CAL_COMPONENT_TIMEZONE;

	default:
		/* We should have been loaded with a supported type! */
		g_assert_not_reached ();
		return CAL_COMPONENT_NO_TYPE;
	}
}

/**
 * cal_component_get_as_string:
 * @comp: A calendar component.
 *
 * Gets the iCalendar string representation of a calendar component.  You should
 * call cal_component_commit_sequence() before this function to ensure that the
 * component's sequence number is consistent with the state of the object.
 *
 * Return value: String representation of the calendar component according to
 * RFC 2445.
 **/
char *
cal_component_get_as_string (CalComponent *comp)
{
	CalComponentPrivate *priv;
	char *str, *buf;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), NULL);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	/* Ensure that the user has committed the new SEQUENCE */
	g_return_val_if_fail (priv->need_sequence_inc == FALSE, NULL);

	/* We dup the string; libical owns that memory */

	str = icalcomponent_as_ical_string (priv->icalcomp);

	if (str)
		buf = g_strdup (str);
	else
		buf = NULL;

	return buf;
}

/* Used from g_hash_table_foreach(); ensures that an alarm subcomponent
 * has the mandatory properties it needs.
 */
static void
ensure_alarm_properties_cb (gpointer key, gpointer value, gpointer data)
{
	CalComponent *comp;
	CalComponentPrivate *priv;
	icalcomponent *alarm;
	icalproperty *prop;
	enum icalproperty_action action;
	const char *str;

	alarm = value;

	comp = CAL_COMPONENT (data);
	priv = comp->priv;

	prop = icalcomponent_get_first_property (alarm, ICAL_ACTION_PROPERTY);
	if (!prop)
		return;

	action = icalproperty_get_action (prop);

	switch (action) {
	case ICAL_ACTION_DISPLAY:
		/* Ensure we have a DESCRIPTION property */
		prop = icalcomponent_get_first_property (alarm, ICAL_DESCRIPTION_PROPERTY);
		if (prop)
			break;

		if (!priv->summary.prop)
			str = _("Untitled appointment");
		else
			str = icalproperty_get_summary (priv->summary.prop);

		prop = icalproperty_new_description (str);
		icalcomponent_add_property (alarm, prop);

		break;

	default:
		break;
		/* FIXME: add other action types here */
	}
}

/* Ensures that alarm subcomponents have the mandatory properties they need,
 * even when clients may not have set them properly.
 */
static void
ensure_alarm_properties (CalComponent *comp)
{
	CalComponentPrivate *priv;

	priv = comp->priv;

	g_hash_table_foreach (priv->alarm_uid_hash, ensure_alarm_properties_cb, comp);
}

/**
 * cal_component_commit_sequence:
 * @comp:
 *
 * Increments the sequence number property in a calendar component object if it
 * needs it.  This needs to be done when any of a number of properties listed in
 * RFC 2445 change values, such as the start and end dates of a component.
 *
 * This function must be called before calling cal_component_get_as_string() to
 * ensure that the component is fully consistent.
 **/
void
cal_component_commit_sequence (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	ensure_alarm_properties (comp);

	if (!priv->need_sequence_inc)
		return;

	if (priv->sequence) {
		int seq;

		seq = icalproperty_get_sequence (priv->sequence);
		icalproperty_set_sequence (priv->sequence, seq + 1);
	} else {
		/* The component had no SEQUENCE property, so assume that the
		 * default would have been zero.  Since it needed incrementing
		 * anyways, we use a value of 1 here.
		 */
		priv->sequence = icalproperty_new_sequence (1);
		icalcomponent_add_property (priv->icalcomp, priv->sequence);
	}

	priv->need_sequence_inc = FALSE;
}

void
cal_component_abort_sequence (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;

	priv->need_sequence_inc = FALSE;
}

/**
 * cal_component_get_uid:
 * @comp: A calendar component object.
 * @uid: Return value for the UID string.
 *
 * Queries the unique identifier of a calendar component object.
 **/
void
cal_component_get_uid (CalComponent *comp, const char **uid)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (uid != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	/* This MUST exist, since we ensured that it did */
	g_assert (priv->uid != NULL);

	*uid = icalproperty_get_uid (priv->uid);
}

/**
 * cal_component_set_uid:
 * @comp: A calendar component object.
 * @uid: Unique identifier.
 *
 * Sets the unique identifier string of a calendar component object.
 **/
void
cal_component_set_uid (CalComponent *comp, const char *uid)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (uid != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	/* This MUST exist, since we ensured that it did */
	g_assert (priv->uid != NULL);

	icalproperty_set_uid (priv->uid, (char *) uid);
}

/**
 * cal_component_get_categories:
 * @comp: A calendar component object.
 * @categories:
 *
 *
 **/
void
cal_component_get_categories (CalComponent *comp, const char **categories)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (categories != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (priv->categories)
		*categories = icalproperty_get_categories (priv->categories);
	else
		*categories = NULL;
}

/**
 * cal_component_set_categories:
 * @comp: A calendar component object.
 * @categories:
 *
 *
 **/
void
cal_component_set_categories (CalComponent *comp, const char *categories)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!categories || !(*categories)) {
		if (priv->categories) {
			icalcomponent_remove_property (priv->icalcomp, priv->categories);
			icalproperty_free (priv->categories);
			priv->url = NULL;
		}

		return;
	}

	if (priv->categories)
		icalproperty_set_categories (priv->categories, (char *) categories);
	else {
		priv->categories = icalproperty_new_categories ((char *) categories);
		icalcomponent_add_property (priv->icalcomp, priv->categories);
	}
}


/**
 * cal_component_get_categories_list:
 * @comp: A calendar component object.
 * @categ_list: Return value for the list of strings, where each string is a
 * category.  This should be freed using cal_component_free_categories_list().
 *
 * Queries the list of categories of a calendar component object.  Each element
 * in the returned categ_list is a string with the corresponding category.
 **/
void
cal_component_get_categories_list (CalComponent *comp, GSList **categ_list)
{
	CalComponentPrivate *priv;
	const char *categories;
	const char *p;
	const char *cat_start;
	char *str;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (categ_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!priv->categories) {
		*categ_list = NULL;
		return;
	}

	categories = icalproperty_get_categories (priv->categories);
	g_assert (categories != NULL);

	cat_start = categories;
	*categ_list = NULL;

	for (p = categories; *p; p++)
		if (*p == ',') {
			str = g_strndup (cat_start, p - cat_start);
			*categ_list = g_slist_prepend (*categ_list, str);

			cat_start = p + 1;
		}

	str = g_strndup (cat_start, p - cat_start);
	*categ_list = g_slist_prepend (*categ_list, str);

	*categ_list = g_slist_reverse (*categ_list);
}

/* Creates a comma-delimited string of categories */
static char *
stringify_categories (GSList *categ_list)
{
	GString *s;
	GSList *l;
	char *str;

	s = g_string_new (NULL);

	for (l = categ_list; l; l = l->next) {
		g_string_append (s, l->data);

		if (l->next != NULL)
			g_string_append (s, ",");
	}

	str = s->str;
	g_string_free (s, FALSE);

	return str;
}

/**
 * cal_component_set_categories_list:
 * @comp: A calendar component object.
 * @categ_list: List of strings, one for each category.
 *
 * Sets the list of categories of a calendar component object.
 **/
void
cal_component_set_categories_list (CalComponent *comp, GSList *categ_list)
{
	CalComponentPrivate *priv;
	char *categories_str;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!categ_list) {
		if (priv->categories) {
			icalcomponent_remove_property (priv->icalcomp, priv->categories);
			icalproperty_free (priv->categories);
		}

		return;
	}

	/* Create a single string of categories */
	categories_str = stringify_categories (categ_list);

	/* Set the categories */
	priv->categories = icalproperty_new_categories (categories_str);
	g_free (categories_str);

	icalcomponent_add_property (priv->icalcomp, priv->categories);
}

/**
 * cal_component_get_classification:
 * @comp: A calendar component object.
 * @classif: Return value for the classification.
 *
 * Queries the classification of a calendar component object.  If the
 * classification property is not set on this component, this function returns
 * #CAL_COMPONENT_CLASS_NONE.
 **/
void
cal_component_get_classification (CalComponent *comp, CalComponentClassification *classif)
{
	CalComponentPrivate *priv;
	icalproperty_class class;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (classif != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!priv->classification) {
		*classif = CAL_COMPONENT_CLASS_NONE;
		return;
	}

	class = icalproperty_get_class (priv->classification);

	switch (class)
	{
	case ICAL_CLASS_PUBLIC:
	  *classif = CAL_COMPONENT_CLASS_PUBLIC;
	  break;
	case ICAL_CLASS_PRIVATE:
	  *classif = CAL_COMPONENT_CLASS_PRIVATE;
	  break;
	case ICAL_CLASS_CONFIDENTIAL:
	  *classif = CAL_COMPONENT_CLASS_CONFIDENTIAL;
	  break;
	default:
	  *classif = CAL_COMPONENT_CLASS_UNKNOWN;
	  break;
	}
}

/**
 * cal_component_set_classification:
 * @comp: A calendar component object.
 * @classif: Classification to use.
 *
 * Sets the classification property of a calendar component object.  To unset
 * the property, specify CAL_COMPONENT_CLASS_NONE for @classif.
 **/
void
cal_component_set_classification (CalComponent *comp, CalComponentClassification classif)
{
	CalComponentPrivate *priv;
	icalproperty_class class;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (classif != CAL_COMPONENT_CLASS_UNKNOWN);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (classif == CAL_COMPONENT_CLASS_NONE) {
		if (priv->classification) {
			icalcomponent_remove_property (priv->icalcomp, priv->classification);
			icalproperty_free (priv->classification);
			priv->classification = NULL;
		}

		return;
	}

	switch (classif) {
	case CAL_COMPONENT_CLASS_PUBLIC:
	  class = ICAL_CLASS_PUBLIC;
		break;

	case CAL_COMPONENT_CLASS_PRIVATE:
	  class = ICAL_CLASS_PRIVATE;
		break;

	case CAL_COMPONENT_CLASS_CONFIDENTIAL:
	  class = ICAL_CLASS_CONFIDENTIAL;
		break;

	default:
		g_assert_not_reached ();
		class = ICAL_CLASS_NONE;
	}

	if (priv->classification)
		icalproperty_set_class (priv->classification, class);
	else {
		priv->classification = icalproperty_new_class (class);
		icalcomponent_add_property (priv->icalcomp, priv->classification);
	}
}

/* Gets a text list value */
static void
get_text_list (GSList *text_list,
	       const char *(* get_prop_func) (const icalproperty *prop),
	       GSList **tl)
{
	GSList *l;

	*tl = NULL;

	if (!text_list)
		return;

	for (l = text_list; l; l = l->next) {
		struct text *text;
		CalComponentText *t;

		text = l->data;
		g_assert (text->prop != NULL);

		t = g_new (CalComponentText, 1);
		t->value = (* get_prop_func) (text->prop);

		if (text->altrep_param)
			t->altrep = icalparameter_get_altrep (text->altrep_param);
		else
			t->altrep = NULL;

		*tl = g_slist_prepend (*tl, t);
	}

	*tl = g_slist_reverse (*tl);
}

/* Sets a text list value */
static void
set_text_list (CalComponent *comp,
	       icalproperty *(* new_prop_func) (const char *value),
	       GSList **text_list,
	       GSList *tl)
{
	CalComponentPrivate *priv;
	GSList *l;

	priv = comp->priv;

	/* Remove old texts */

	for (l = *text_list; l; l = l->next) {
		struct text *text;

		text = l->data;
		g_assert (text->prop != NULL);

		icalcomponent_remove_property (priv->icalcomp, text->prop);
		icalproperty_free (text->prop);
		g_free (text);
	}

	g_slist_free (*text_list);
	*text_list = NULL;

	/* Add in new texts */

	for (l = tl; l; l = l->next) {
		CalComponentText *t;
		struct text *text;

		t = l->data;
		g_return_if_fail (t->value != NULL);

		text = g_new (struct text, 1);

		text->prop = (* new_prop_func) ((char *) t->value);
		icalcomponent_add_property (priv->icalcomp, text->prop);

		if (t->altrep) {
			text->altrep_param = icalparameter_new_altrep ((char *) t->altrep);
			icalproperty_add_parameter (text->prop, text->altrep_param);
		} else
			text->altrep_param = NULL;

		*text_list = g_slist_prepend (*text_list, text);
	}

	*text_list = g_slist_reverse (*text_list);
}

/**
 * cal_component_get_comment_list:
 * @comp: A calendar component object.
 * @text_list: Return value for the comment properties and their parameters, as
 * a list of #CalComponentText structures.  This should be freed using the
 * cal_component_free_text_list() function.
 *
 * Queries the comment of a calendar component object.  The comment property can
 * appear several times inside a calendar component, and so a list of
 * #CalComponentText is returned.
 **/
void
cal_component_get_comment_list (CalComponent *comp, GSList **text_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (text_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_text_list (priv->comment_list, icalproperty_get_comment, text_list);
}

/**
 * cal_component_set_comment_list:
 * @comp: A calendar component object.
 * @text_list: List of #CalComponentText structures.
 *
 * Sets the comment of a calendar component object.  The comment property can
 * appear several times inside a calendar component, and so a list of
 * #CalComponentText structures is used.
 **/
void
cal_component_set_comment_list (CalComponent *comp, GSList *text_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_text_list (comp, icalproperty_new_comment, &priv->comment_list, text_list);
}

/**
 * cal_component_get_contact_list:
 * @comp: A calendar component object.
 * @text_list: Return value for the contact properties and their parameters, as
 * a list of #CalComponentText structures.  This should be freed using the
 * cal_component_free_text_list() function.
 *
 * Queries the contact of a calendar component object.  The contact property can
 * appear several times inside a calendar component, and so a list of
 * #CalComponentText is returned.
 **/
void
cal_component_get_contact_list (CalComponent *comp, GSList **text_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (text_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_text_list (priv->contact_list, icalproperty_get_contact, text_list);
}

/**
 * cal_component_set_contact_list:
 * @comp: A calendar component object.
 * @text_list: List of #CalComponentText structures.
 *
 * Sets the contact of a calendar component object.  The contact property can
 * appear several times inside a calendar component, and so a list of
 * #CalComponentText structures is used.
 **/
void
cal_component_set_contact_list (CalComponent *comp, GSList *text_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_text_list (comp, icalproperty_new_contact, &priv->contact_list, text_list);
}

/* Gets a struct icaltimetype value */
static void
get_icaltimetype (icalproperty *prop,
		  struct icaltimetype (* get_prop_func) (const icalproperty *prop),
		  struct icaltimetype **t)
{
	if (!prop) {
		*t = NULL;
		return;
	}

	*t = g_new (struct icaltimetype, 1);
	**t = (* get_prop_func) (prop);
}

/* Sets a struct icaltimetype value */
static void
set_icaltimetype (CalComponent *comp, icalproperty **prop,
		  icalproperty *(* prop_new_func) (struct icaltimetype v),
		  void (* prop_set_func) (icalproperty *prop, struct icaltimetype v),
		  struct icaltimetype *t)
{
	CalComponentPrivate *priv;

	priv = comp->priv;

	if (!t) {
		if (*prop) {
			icalcomponent_remove_property (priv->icalcomp, *prop);
			icalproperty_free (*prop);
			*prop = NULL;
		}

		return;
	}

	if (*prop)
		(* prop_set_func) (*prop, *t);
	else {
		*prop = (* prop_new_func) (*t);
		icalcomponent_add_property (priv->icalcomp, *prop);
	}
}

/**
 * cal_component_get_completed:
 * @comp: A calendar component object.
 * @t: Return value for the completion date.  This should be freed using the
 * cal_component_free_icaltimetype() function.
 *
 * Queries the date at which a calendar compoment object was completed.
 **/
void
cal_component_get_completed (CalComponent *comp, struct icaltimetype **t)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (t != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_icaltimetype (priv->completed, icalproperty_get_completed, t);
}

/**
 * cal_component_set_completed:
 * @comp: A calendar component object.
 * @t: Value for the completion date.
 *
 * Sets the date at which a calendar component object was completed.
 **/
void
cal_component_set_completed (CalComponent *comp, struct icaltimetype *t)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_icaltimetype (comp, &priv->completed,
			  icalproperty_new_completed,
			  icalproperty_set_completed,
			  t);
}


/**
 * cal_component_get_created:
 * @comp: A calendar component object.
 * @t: Return value for the creation date.  This should be freed using the
 * cal_component_free_icaltimetype() function.
 *
 * Queries the date in which a calendar component object was created in the
 * calendar store.
 **/
void
cal_component_get_created (CalComponent *comp, struct icaltimetype **t)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (t != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_icaltimetype (priv->created, icalproperty_get_created, t);
}

/**
 * cal_component_set_created:
 * @comp: A calendar component object.
 * @t: Value for the creation date.
 *
 * Sets the date in which a calendar component object is created in the calendar
 * store.  This should only be used inside a calendar store application, i.e.
 * not by calendar user agents.
 **/
void
cal_component_set_created (CalComponent *comp, struct icaltimetype *t)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_icaltimetype (comp, &priv->created,
			  icalproperty_new_created,
			  icalproperty_set_created,
			  t);
}

/**
 * cal_component_get_description_list:
 * @comp: A calendar component object.
 * @text_list: Return value for the description properties and their parameters,
 * as a list of #CalComponentText structures.  This should be freed using the
 * cal_component_free_text_list() function.
 *
 * Queries the description of a calendar component object.  Journal components
 * may have more than one description, and as such this function returns a list
 * of #CalComponentText structures.  All other types of components can have at
 * most one description.
 **/
void
cal_component_get_description_list (CalComponent *comp, GSList **text_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (text_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_text_list (priv->description_list, icalproperty_get_description, text_list);
}

/**
 * cal_component_set_description_list:
 * @comp: A calendar component object.
 * @text_list: List of #CalComponentSummary structures.
 *
 * Sets the description of a calendar component object.  Journal components may
 * have more than one description, and as such this function takes in a list of
 * #CalComponentDescription structures.  All other types of components can have
 * at most one description.
 **/
void
cal_component_set_description_list (CalComponent *comp, GSList *text_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_text_list (comp, icalproperty_new_description, &priv->description_list, text_list);
}

/* Gets a date/time and timezone pair */
static void
get_datetime (struct datetime *datetime,
	      struct icaltimetype (* get_prop_func) (const icalproperty *prop),
	      CalComponentDateTime *dt)
{
	if (datetime->prop) {
		dt->value = g_new (struct icaltimetype, 1);
		*dt->value = (* get_prop_func) (datetime->prop);
	} else
		dt->value = NULL;

	/* If the icaltimetype has is_utc set, we set "UTC" as the TZID.
	   This makes the timezone code simpler. */
	if (datetime->tzid_param)
		dt->tzid = g_strdup (icalparameter_get_tzid (datetime->tzid_param));
	else if (dt->value && dt->value->is_utc)
		dt->tzid = g_strdup ("UTC");
	else
		dt->tzid = NULL;
}

/* Sets a date/time and timezone pair */
static void
set_datetime (CalComponent *comp, struct datetime *datetime,
	      icalproperty *(* prop_new_func) (struct icaltimetype v),
	      void (* prop_set_func) (icalproperty * prop, struct icaltimetype v),
	      CalComponentDateTime *dt)
{
	CalComponentPrivate *priv;

	priv = comp->priv;

	/* If we are setting the property to NULL (i.e. removing it), then
	   we remove it if it exists. */
	if (!dt) {
		if (datetime->prop) {
			icalcomponent_remove_property (priv->icalcomp, datetime->prop);
			icalproperty_free (datetime->prop);

			datetime->prop = NULL;
			datetime->tzid_param = NULL;
		}

		return;
	}

	g_return_if_fail (dt->value != NULL);

	/* If the TZID is set to "UTC", we set the is_utc flag. */
	if (dt->tzid && !strcmp (dt->tzid, "UTC"))
		dt->value->is_utc = 1;
	else
		dt->value->is_utc = 0;

	if (datetime->prop) {
		(* prop_set_func) (datetime->prop, *dt->value);
	} else {
		datetime->prop = (* prop_new_func) (*dt->value);
		icalcomponent_add_property (priv->icalcomp, datetime->prop);
	}

	/* If the TZID is set to "UTC", we don't want to save the TZID. */
	if (dt->tzid && strcmp (dt->tzid, "UTC")) {
		g_assert (datetime->prop != NULL);

		if (datetime->tzid_param) {
			icalparameter_set_tzid (datetime->tzid_param, (char *) dt->tzid);
		} else {
			datetime->tzid_param = icalparameter_new_tzid ((char *) dt->tzid);
			icalproperty_add_parameter (datetime->prop, datetime->tzid_param);
		}
	} else if (datetime->tzid_param) {
		icalproperty_remove_parameter (datetime->prop, ICAL_TZID_PARAMETER);
		icalparameter_free (datetime->tzid_param);
		datetime->tzid_param = NULL;
	}
}


/* This tries to get the DTSTART + DURATION for a VEVENT or VTODO. In a
   VEVENT this is used for the DTEND if no DTEND exists, In a VTOTO it is
   used for the DUE date if DUE doesn't exist. */
static void
cal_component_get_start_plus_duration (CalComponent *comp,
				       CalComponentDateTime *dt)
{
	CalComponentPrivate *priv;
	struct icaldurationtype duration;

	priv = comp->priv;

	if (!priv->duration)
		return;

	/* Get the DTSTART time. */
	get_datetime (&priv->dtstart, icalproperty_get_dtstart, dt);
	if (!dt->value)
		return;

	duration = icalproperty_get_duration (priv->duration);

	/* The DURATION shouldn't be negative, but just return DTSTART if it
	   is, i.e. assume it is 0. */
	if (duration.is_neg)
		return;

	/* If DTSTART is a DATE value, then we need to check if the DURATION
	   includes any hours, minutes or seconds. If it does, we need to
	   make the DTEND/DUE a DATE-TIME value. */
	duration.days += duration.weeks * 7;
	if (dt->value->is_date) {
		if (duration.hours != 0 || duration.minutes != 0
		    || duration.seconds != 0) {
			dt->value->is_date = 0;
		}
	}

	/* Add on the DURATION. */
	icaltime_adjust (dt->value, duration.days, duration.hours,
			 duration.minutes, duration.seconds);
}


/**
 * cal_component_get_dtend:
 * @comp: A calendar component object.
 * @dt: Return value for the date/time end.  This should be freed with the
 * cal_component_free_datetime() function.
 *
 * Queries the date/time end of a calendar component object.
 **/
void
cal_component_get_dtend (CalComponent *comp, CalComponentDateTime *dt)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (dt != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_datetime (&priv->dtend, icalproperty_get_dtend, dt);

	/* If we don't have a DTEND property, then we try to get DTSTART
	   + DURATION. */
	if (!dt->value)
		cal_component_get_start_plus_duration (comp, dt);
}

/**
 * cal_component_set_dtend:
 * @comp: A calendar component object.
 * @dt: End date/time.
 *
 * Sets the date/time end property of a calendar component object.
 **/
void
cal_component_set_dtend (CalComponent *comp, CalComponentDateTime *dt)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_datetime (comp, &priv->dtend,
		      icalproperty_new_dtend,
		      icalproperty_set_dtend,
		      dt);

	/* Make sure we remove any existing DURATION property, as it can't be
	   used with a DTEND. If DTEND is set to NULL, i.e. removed, we also
	   want to remove any DURATION. */
	if (priv->duration) {
		icalcomponent_remove_property (priv->icalcomp, priv->duration);
		icalproperty_free (priv->duration);
		priv->duration = NULL;
	}

	priv->need_sequence_inc = TRUE;
}

/**
 * cal_component_get_dtstamp:
 * @comp: A calendar component object.
 * @t: A value for the date/timestamp.
 *
 * Queries the date/timestamp property of a calendar component object, which is
 * the last time at which the object was modified by a calendar user agent.
 **/
void
cal_component_get_dtstamp (CalComponent *comp, struct icaltimetype *t)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (t != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	/* This MUST exist, since we ensured that it did */
	g_assert (priv->dtstamp != NULL);

	*t = icalproperty_get_dtstamp (priv->dtstamp);
}

/**
 * cal_component_set_dtstamp:
 * @comp: A calendar component object.
 * @t: Date/timestamp value.
 *
 * Sets the date/timestamp of a calendar component object.  This should be
 * called whenever a calendar user agent makes a change to a component's
 * properties.
 **/
void
cal_component_set_dtstamp (CalComponent *comp, struct icaltimetype *t)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (t != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	/* This MUST exist, since we ensured that it did */
	g_assert (priv->dtstamp != NULL);

	icalproperty_set_dtstamp (priv->dtstamp, *t);
}

/**
 * cal_component_get_dtstart:
 * @comp: A calendar component object.
 * @dt: Return value for the date/time start.  This should be freed with the
 * cal_component_free_datetime() function.
 *
 * Queries the date/time start of a calendar component object.
 **/
void
cal_component_get_dtstart (CalComponent *comp, CalComponentDateTime *dt)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (dt != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_datetime (&priv->dtstart, icalproperty_get_dtstart, dt);
}

/**
 * cal_component_set_dtstart:
 * @comp: A calendar component object.
 * @dt: Start date/time.
 *
 * Sets the date/time start property of a calendar component object.
 **/
void
cal_component_set_dtstart (CalComponent *comp, CalComponentDateTime *dt)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_datetime (comp, &priv->dtstart,
		      icalproperty_new_dtstart,
		      icalproperty_set_dtstart,
		      dt);

	priv->need_sequence_inc = TRUE;
}

/**
 * cal_component_get_due:
 * @comp: A calendar component object.
 * @dt: Return value for the due date/time.  This should be freed with the
 * cal_component_free_datetime() function.
 *
 * Queries the due date/time of a calendar component object.
 **/
void
cal_component_get_due (CalComponent *comp, CalComponentDateTime *dt)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (dt != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_datetime (&priv->due, icalproperty_get_due, dt);

	/* If we don't have a DTEND property, then we try to get DTSTART
	   + DURATION. */
	if (!dt->value)
		cal_component_get_start_plus_duration (comp, dt);
}

/**
 * cal_component_set_due:
 * @comp: A calendar component object.
 * @dt: End date/time.
 *
 * Sets the due date/time property of a calendar component object.
 **/
void
cal_component_set_due (CalComponent *comp, CalComponentDateTime *dt)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_datetime (comp, &priv->due,
		      icalproperty_new_due,
		      icalproperty_set_due,
		      dt);

	/* Make sure we remove any existing DURATION property, as it can't be
	   used with a DTEND. If DTEND is set to NULL, i.e. removed, we also
	   want to remove any DURATION. */
	if (priv->duration) {
		icalcomponent_remove_property (priv->icalcomp, priv->duration);
		icalproperty_free (priv->duration);
		priv->duration = NULL;
	}

	priv->need_sequence_inc = TRUE;
}

/* Builds a list of CalComponentPeriod structures based on a list of icalproperties */
static void
get_period_list (GSList *period_list,
		 struct icaldatetimeperiodtype (* get_prop_func) (const icalproperty *prop),
		 GSList **list)
{
	GSList *l;

	*list = NULL;

	if (!period_list)
		return;

	for (l = period_list; l; l = l->next) {
		struct period *period;
		CalComponentPeriod *p;
		struct icaldatetimeperiodtype ip;

		period = l->data;
		g_assert (period->prop != NULL);

		p = g_new (CalComponentPeriod, 1);

		/* Get value parameter */

		if (period->value_param) {
			icalparameter_value value_type;

			value_type = icalparameter_get_value (period->value_param);

			if (value_type == ICAL_VALUE_DATE || value_type == ICAL_VALUE_DATETIME)
				p->type = CAL_COMPONENT_PERIOD_DATETIME;
			else if (value_type == ICAL_VALUE_DURATION)
				p->type = CAL_COMPONENT_PERIOD_DURATION;
			else {
				g_message ("get_period_list(): Unknown value for period %d; "
					   "using DATETIME", value_type);
				p->type = CAL_COMPONENT_PERIOD_DATETIME;
			}
		} else
			p->type = CAL_COMPONENT_PERIOD_DATETIME;

		/* Get start and end/duration */

		ip = (* get_prop_func) (period->prop);

		p->start = ip.period.start;

		if (p->type == CAL_COMPONENT_PERIOD_DATETIME)
			p->u.end = ip.period.end;
		else if (p->type == CAL_COMPONENT_PERIOD_DURATION)
			p->u.duration = ip.period.duration;
		else
			g_assert_not_reached ();

		/* Put in list */

		*list = g_slist_prepend (*list, p);
	}

	*list = g_slist_reverse (*list);
}

/* Sets a period list value */
static void
set_period_list (CalComponent *comp,
		 icalproperty *(* new_prop_func) (struct icaldatetimeperiodtype period),
		 GSList **period_list,
		 GSList *pl)
{
	CalComponentPrivate *priv;
	GSList *l;

	priv = comp->priv;

	/* Remove old periods */

	for (l = *period_list; l; l = l->next) {
		struct period *period;

		period = l->data;
		g_assert (period->prop != NULL);

		icalcomponent_remove_property (priv->icalcomp, period->prop);
		icalproperty_free (period->prop);
		g_free (period);
	}

	g_slist_free (*period_list);
	*period_list = NULL;

	/* Add in new periods */

	for (l = pl; l; l = l->next) {
		CalComponentPeriod *p;
		struct period *period;
		struct icaldatetimeperiodtype ip;
		icalparameter_value value_type;

		g_assert (l->data != NULL);
		p = l->data;

		/* Create libical value */

		ip.period.start = p->start;

		if (p->type == CAL_COMPONENT_PERIOD_DATETIME) {
			value_type = ICAL_VALUE_DATETIME;
			ip.period.end = p->u.end;
		} else if (p->type == CAL_COMPONENT_PERIOD_DURATION) {
			value_type = ICAL_VALUE_DURATION;
			ip.period.duration = p->u.duration;
		} else {
			g_assert_not_reached ();
			return;
		}

		/* Create property */

		period = g_new (struct period, 1);

		period->prop = (* new_prop_func) (ip);
		period->value_param = icalparameter_new_value (value_type);
		icalproperty_add_parameter (period->prop, period->value_param);

		/* Add to list */

		*period_list = g_slist_prepend (*period_list, period);
	}

	*period_list = g_slist_reverse (*period_list);
}

/**
 * cal_component_get_exdate_list:
 * @comp: A calendar component object.
 * @exdate_list: Return value for the list of exception dates, as a list of
 * #CalComponentDateTime structures.  This should be freed using the
 * cal_component_free_exdate_list() function.
 *
 * Queries the list of exception date properties in a calendar component object.
 **/
void
cal_component_get_exdate_list (CalComponent *comp, GSList **exdate_list)
{
	CalComponentPrivate *priv;
	GSList *l;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (exdate_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	*exdate_list = NULL;

	for (l = priv->exdate_list; l; l = l->next) {
		struct datetime *dt;
		CalComponentDateTime *cdt;

		dt = l->data;

		cdt = g_new (CalComponentDateTime, 1);
		cdt->value = g_new (struct icaltimetype, 1);

		*cdt->value = icalproperty_get_exdate (dt->prop);

		if (dt->tzid_param)
			cdt->tzid = g_strdup (icalparameter_get_tzid (dt->tzid_param));
		else
			cdt->tzid = NULL;

		*exdate_list = g_slist_prepend (*exdate_list, cdt);
	}

	*exdate_list = g_slist_reverse (*exdate_list);
}

/**
 * cal_component_set_exdate_list:
 * @comp: A calendar component object.
 * @exdate_list: List of #CalComponentDateTime structures.
 *
 * Sets the list of exception dates in a calendar component object.
 **/
void
cal_component_set_exdate_list (CalComponent *comp, GSList *exdate_list)
{
	CalComponentPrivate *priv;
	GSList *l;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	/* Remove old exception dates */

	for (l = priv->exdate_list; l; l = l->next) {
		struct datetime *dt;

		dt = l->data;

		/* Removing the DATE or DATE-TIME property will also remove
		   any TZID parameter. */
		icalcomponent_remove_property (priv->icalcomp, dt->prop);
		icalproperty_free (dt->prop);
		g_free (dt);
	}

	g_slist_free (priv->exdate_list);
	priv->exdate_list = NULL;

	/* Add in new exception dates */

	for (l = exdate_list; l; l = l->next) {
		CalComponentDateTime *cdt;
		struct datetime *dt;

		g_assert (l->data != NULL);
		cdt = l->data;

		g_assert (cdt->value != NULL);

		dt = g_new (struct datetime, 1);
		dt->prop = icalproperty_new_exdate (*cdt->value);

		if (cdt->tzid) {
			dt->tzid_param = icalparameter_new_tzid ((char *) cdt->tzid);
			icalproperty_add_parameter (dt->prop, dt->tzid_param);
		} else
			dt->tzid_param = NULL;

		icalcomponent_add_property (priv->icalcomp, dt->prop);
		priv->exdate_list = g_slist_prepend (priv->exdate_list, dt);
	}

	priv->exdate_list = g_slist_reverse (priv->exdate_list);

	priv->need_sequence_inc = TRUE;
}

/**
 * cal_component_has_exdates:
 * @comp: A calendar component object.
 *
 * Queries whether a calendar component object has any exception dates defined
 * for it.
 *
 * Return value: TRUE if the component has exception dates, FALSE otherwise.
 **/
gboolean
cal_component_has_exdates (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	return (priv->exdate_list != NULL);
}

/* Gets a list of recurrence rules */
static void
get_recur_list (GSList *recur_list,
		struct icalrecurrencetype (* get_prop_func) (const icalproperty *prop),
		GSList **list)
{
	GSList *l;

	*list = NULL;

	for (l = recur_list; l; l = l->next) {
		icalproperty *prop;
		struct icalrecurrencetype *r;

		prop = l->data;

		r = g_new (struct icalrecurrencetype, 1);
		*r = (* get_prop_func) (prop);

		*list = g_slist_prepend (*list, r);
	}

	*list = g_slist_reverse (*list);
}

/* Sets a list of recurrence rules */
static void
set_recur_list (CalComponent *comp,
		icalproperty *(* new_prop_func) (struct icalrecurrencetype recur),
		GSList **recur_list,
		GSList *rl)
{
	CalComponentPrivate *priv;
	GSList *l;

	priv = comp->priv;

	/* Remove old recurrences */

	for (l = *recur_list; l; l = l->next) {
		icalproperty *prop;

		prop = l->data;
		icalcomponent_remove_property (priv->icalcomp, prop);
		icalproperty_free (prop);
	}

	g_slist_free (*recur_list);
	*recur_list = NULL;

	/* Add in new recurrences */

	for (l = rl; l; l = l->next) {
		icalproperty *prop;
		struct icalrecurrencetype *recur;

		g_assert (l->data != NULL);
		recur = l->data;

		prop = (* new_prop_func) (*recur);
		icalcomponent_add_property (priv->icalcomp, prop);

		*recur_list = g_slist_prepend (*recur_list, prop);
	}

	*recur_list = g_slist_reverse (*recur_list);
}

/**
 * cal_component_get_exrule_list:
 * @comp: A calendar component object.
 * @recur_list: List of exception rules as struct #icalrecurrencetype
 * structures.  This should be freed using the cal_component_free_recur_list()
 * function.
 *
 * Queries the list of exception rule properties of a calendar component
 * object.
 **/
void
cal_component_get_exrule_list (CalComponent *comp, GSList **recur_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (recur_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_recur_list (priv->exrule_list, icalproperty_get_exrule, recur_list);
}

/**
 * cal_component_get_exrule_property_list:
 * @comp: A calendar component object.
 * @recur_list: Returns a list of exception rule properties.
 *
 * Queries the list of exception rule properties of a calendar component object.
 **/
void
cal_component_get_exrule_property_list (CalComponent *comp, GSList **recur_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (recur_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	*recur_list = priv->exrule_list;
}

/**
 * cal_component_set_exrule_list:
 * @comp: A calendar component object.
 * @recur_list: List of struct #icalrecurrencetype structures.
 *
 * Sets the list of exception rules in a calendar component object.
 **/
void
cal_component_set_exrule_list (CalComponent *comp, GSList *recur_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_recur_list (comp, icalproperty_new_exrule, &priv->exrule_list, recur_list);

	priv->need_sequence_inc = TRUE;
}

/**
 * cal_component_has_exrules:
 * @comp: A calendar component object.
 *
 * Queries whether a calendar component object has any exception rules defined
 * for it.
 *
 * Return value: TRUE if the component has exception rules, FALSE otherwise.
 **/
gboolean
cal_component_has_exrules (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	return (priv->exrule_list != NULL);
}

/**
 * cal_component_has_exceptions:
 * @comp: A calendar component object
 *
 * Queries whether a calendar component object has any exception dates
 * or exception rules.
 *
 * Return value: TRUE if the component has exceptions, FALSE otherwise.
 **/
gboolean
cal_component_has_exceptions (CalComponent *comp)
{
	return cal_component_has_exdates (comp) || cal_component_has_exrules (comp);
}

/**
 * cal_component_get_geo:
 * @comp: A calendar component object.
 * @geo: Return value for the geographic position property.  This should be
 * freed using the cal_component_free_geo() function.
 *
 * Sets the geographic position property of a calendar component object.
 **/
void
cal_component_get_geo (CalComponent *comp, struct icalgeotype **geo)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (geo != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (priv->geo) {
		*geo = g_new (struct icalgeotype, 1);
		**geo = icalproperty_get_geo (priv->geo);
	} else
		*geo = NULL;
}

/**
 * cal_component_set_geo:
 * @comp: A calendar component object.
 * @geo: Value for the geographic position property.
 *
 * Sets the geographic position property on a calendar component object.
 **/
void
cal_component_set_geo (CalComponent *comp, struct icalgeotype *geo)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!geo) {
		if (priv->geo) {
			icalcomponent_remove_property (priv->icalcomp, priv->geo);
			icalproperty_free (priv->geo);
			priv->geo = NULL;
		}

		return;
	}

	if (priv->geo)
		icalproperty_set_geo (priv->geo, *geo);
	else {
		priv->geo = icalproperty_new_geo (*geo);
		icalcomponent_add_property (priv->icalcomp, priv->geo);
	}
}

/**
 * cal_component_get_last_modified:
 * @comp: A calendar component object.
 * @t: Return value for the last modified time value.
 *
 * Queries the time at which a calendar component object was last modified in
 * the calendar store.
 **/
void
cal_component_get_last_modified (CalComponent *comp, struct icaltimetype **t)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (t != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_icaltimetype (priv->last_modified, icalproperty_get_lastmodified, t);
}

/**
 * cal_component_set_last_modified:
 * @comp: A calendar component object.
 * @t: Value for the last time modified.
 *
 * Sets the time at which a calendar component object was last stored in the
 * calendar store.  This should not be called by plain calendar user agents.
 **/
void
cal_component_set_last_modified (CalComponent *comp, struct icaltimetype *t)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_icaltimetype (comp, &priv->last_modified,
			  icalproperty_new_lastmodified,
			  icalproperty_set_lastmodified,
			  t);
}

/**
 * cal_component_get_organizer:
 * @comp:  A calendar component object
 * @organizer: A value for the organizer
 * 
 * Queries the organizer property of a calendar component object
 **/
void
cal_component_get_organizer (CalComponent *comp, CalComponentOrganizer *organizer)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (organizer != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (priv->organizer.prop)
		organizer->value = icalproperty_get_organizer (priv->organizer.prop);
	else
		organizer->value = NULL;

	if (priv->organizer.sentby_param)
		organizer->sentby = icalparameter_get_sentby (priv->organizer.sentby_param);
	else
		organizer->sentby = NULL;

	if (priv->organizer.cn_param)
		organizer->cn = icalparameter_get_sentby (priv->organizer.cn_param);
	else
		organizer->cn = NULL;

	if (priv->organizer.language_param)
		organizer->language = icalparameter_get_sentby (priv->organizer.language_param);
	else
		organizer->language = NULL;

}

/**
 * cal_component_set_organizer:
 * @comp:  A calendar component object.
 * @organizer: Value for the organizer property
 * 
 * Sets the organizer of a calendar component object
 **/
void
cal_component_set_organizer (CalComponent *comp, CalComponentOrganizer *organizer)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!organizer) {
		if (priv->organizer.prop) {
			icalcomponent_remove_property (priv->icalcomp, priv->organizer.prop);
			icalproperty_free (priv->organizer.prop);

			priv->organizer.prop = NULL;
			priv->organizer.sentby_param = NULL;
			priv->organizer.cn_param = NULL;
			priv->organizer.language_param = NULL;
		}

		return;
	}

	g_return_if_fail (organizer->value != NULL);

	if (priv->organizer.prop)
		icalproperty_set_organizer (priv->organizer.prop, (char *) organizer->value);
	else {
		priv->organizer.prop = icalproperty_new_organizer ((char *) organizer->value);
		icalcomponent_add_property (priv->icalcomp, priv->organizer.prop);
	}

	if (organizer->sentby) {
		g_assert (priv->organizer.prop != NULL);

		if (priv->organizer.sentby_param)
			icalparameter_set_sentby (priv->organizer.sentby_param,
						  (char *) organizer->sentby);
		else {
			priv->organizer.sentby_param = icalparameter_new_sentby (
				(char *) organizer->sentby);
			icalproperty_add_parameter (priv->organizer.prop,
						    priv->organizer.sentby_param);
		}
	} else if (priv->organizer.sentby_param) {
		icalproperty_remove_parameter (priv->organizer.prop, ICAL_SENTBY_PARAMETER);
		icalparameter_free (priv->organizer.sentby_param);
		priv->organizer.sentby_param = NULL;
	}

	if (organizer->cn) {
		g_assert (priv->organizer.prop != NULL);

		if (priv->organizer.cn_param)
			icalparameter_set_cn (priv->organizer.cn_param,
						  (char *) organizer->cn);
		else {
			priv->organizer.cn_param = icalparameter_new_cn (
				(char *) organizer->cn);
			icalproperty_add_parameter (priv->organizer.prop,
						    priv->organizer.cn_param);
		}
	} else if (priv->organizer.cn_param) {
		icalproperty_remove_parameter (priv->organizer.prop, ICAL_CN_PARAMETER);
		icalparameter_free (priv->organizer.cn_param);
		priv->organizer.cn_param = NULL;
	}

	if (organizer->language) {
		g_assert (priv->organizer.prop != NULL);

		if (priv->organizer.language_param)
			icalparameter_set_language (priv->organizer.language_param,
						  (char *) organizer->language);
		else {
			priv->organizer.language_param = icalparameter_new_language (
				(char *) organizer->language);
			icalproperty_add_parameter (priv->organizer.prop,
						    priv->organizer.language_param);
		}
	} else if (priv->organizer.language_param) {
		icalproperty_remove_parameter (priv->organizer.prop, ICAL_LANGUAGE_PARAMETER);
		icalparameter_free (priv->organizer.language_param);
		priv->organizer.language_param = NULL;
	}


}


/**
 * cal_component_has_organizer:
 * @comp: 
 * 
 * 
 * 
 * Return value: 
 **/
gboolean
cal_component_has_organizer (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);

	priv = comp->priv;

	return priv->organizer.prop != NULL;
}
		
/**
 * cal_component_get_percent:
 * @comp: A calendar component object.
 * @percent: Return value for the percent-complete property.  This should be
 * freed using the cal_component_free_percent() function.
 *
 * Queries the percent-complete property of a calendar component object.
 **/
void
cal_component_get_percent (CalComponent *comp, int **percent)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (percent != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (priv->percent) {
		*percent = g_new (int, 1);
		**percent = icalproperty_get_percentcomplete (priv->percent);
	} else
		*percent = NULL;
}

/**
 * cal_component_set_percent:
 * @comp: A calendar component object.
 * @percent: Value for the percent-complete property.
 *
 * Sets the percent-complete property of a calendar component object.
 **/
void
cal_component_set_percent (CalComponent *comp, int *percent)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!percent) {
		if (priv->percent) {
			icalcomponent_remove_property (priv->icalcomp, priv->percent);
			icalproperty_free (priv->percent);
			priv->percent = NULL;
		}

		return;
	}

	g_return_if_fail (*percent >= 0 && *percent <= 100);

	if (priv->percent)
		icalproperty_set_percentcomplete (priv->percent, *percent);
	else {
		priv->percent = icalproperty_new_percentcomplete (*percent);
		icalcomponent_add_property (priv->icalcomp, priv->percent);
	}
}

/**
 * cal_component_get_priority:
 * @comp: A calendar component object.
 * @priority: Return value for the priority property.  This should be freed using
 * the cal_component_free_priority() function.
 *
 * Queries the priority property of a calendar component object.
 **/
void
cal_component_get_priority (CalComponent *comp, int **priority)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (priority != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (priv->priority) {
		*priority = g_new (int, 1);
		**priority = icalproperty_get_priority (priv->priority);
	} else
		*priority = NULL;
}

/**
 * cal_component_set_priority:
 * @comp: A calendar component object.
 * @priority: Value for the priority property.
 *
 * Sets the priority property of a calendar component object.
 **/
void
cal_component_set_priority (CalComponent *comp, int *priority)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!priority) {
		if (priv->priority) {
			icalcomponent_remove_property (priv->icalcomp, priv->priority);
			icalproperty_free (priv->priority);
			priv->priority = NULL;
		}

		return;
	}

	g_return_if_fail (*priority >= 0 && *priority <= 9);

	if (priv->priority)
		icalproperty_set_priority (priv->priority, *priority);
	else {
		priv->priority = icalproperty_new_priority (*priority);
		icalcomponent_add_property (priv->icalcomp, priv->priority);
	}
}

/**
 * cal_component_get_recurid:
 * @comp: A calendar component object.
 * @recur_id: Return value for the recurrence id property
 * 
 * Queries the recurrence id property of a calendar component object
 **/
void 
cal_component_get_recurid (CalComponent *comp, CalComponentRange *recur_id)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (recur_id != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_datetime (&priv->recur_id.recur_time, 
		      icalproperty_get_recurrenceid, 
		      &recur_id->datetime);
}

/**
 * cal_component_set_recurid:
 * @comp: A calendar component object.
 * @recur_id: Value for the recurrence id property.
 * 
 * Sets the recurrence id property of a calendar component object.
 **/
void
cal_component_set_recurid (CalComponent *comp, CalComponentRange *recur_id)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);
	
	set_datetime (comp, &priv->recur_id.recur_time,
		      icalproperty_new_recurrenceid,
		      icalproperty_set_recurrenceid,
		      recur_id ? &recur_id->datetime : NULL);
}

/**
 * cal_component_get_rdate_list:
 * @comp: A calendar component object.
 * @period_list: Return value for the list of recurrence dates, as a list of
 * #CalComponentPeriod structures.  This should be freed using the
 * cal_component_free_period_list() function.
 *
 * Queries the list of recurrence date properties in a calendar component
 * object.
 **/
void
cal_component_get_rdate_list (CalComponent *comp, GSList **period_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (period_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_period_list (priv->rdate_list, icalproperty_get_rdate, period_list);
}

/**
 * cal_component_set_rdate_list:
 * @comp: A calendar component object.
 * @period_list: List of #CalComponentPeriod structures.
 *
 * Sets the list of recurrence dates in a calendar component object.
 **/
void
cal_component_set_rdate_list (CalComponent *comp, GSList *period_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_period_list (comp, icalproperty_new_rdate, &priv->rdate_list, period_list);

	priv->need_sequence_inc = TRUE;
}

/**
 * cal_component_has_rdates:
 * @comp: A calendar component object.
 *
 * Queries whether a calendar component object has any recurrence dates defined
 * for it.
 *
 * Return value: TRUE if the component has recurrence dates, FALSE otherwise.
 **/
gboolean
cal_component_has_rdates (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	return (priv->rdate_list != NULL);
}

/**
 * cal_component_get_rrule_list:
 * @comp: A calendar component object.
 * @recur_list: List of recurrence rules as struct #icalrecurrencetype
 * structures.  This should be freed using the cal_component_free_recur_list()
 * function.
 *
 * Queries the list of recurrence rule properties of a calendar component
 * object.
 **/
void
cal_component_get_rrule_list (CalComponent *comp, GSList **recur_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (recur_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_recur_list (priv->rrule_list, icalproperty_get_rrule, recur_list);
}

/**
 * cal_component_get_rrule_property_list:
 * @comp: A calendar component object.
 * @recur_list: Returns a list of recurrence rule properties.
 *
 * Queries a list of recurrence rule properties of a calendar component object.
 **/
void
cal_component_get_rrule_property_list (CalComponent *comp, GSList **recur_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (recur_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	*recur_list = priv->rrule_list;
}

/**
 * cal_component_set_rrule_list:
 * @comp: A calendar component object.
 * @recur_list: List of struct #icalrecurrencetype structures.
 *
 * Sets the list of recurrence rules in a calendar component object.
 **/
void
cal_component_set_rrule_list (CalComponent *comp, GSList *recur_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_recur_list (comp, icalproperty_new_rrule, &priv->rrule_list, recur_list);

	priv->need_sequence_inc = TRUE;
}

/**
 * cal_component_has_rrules:
 * @comp: A calendar component object.
 *
 * Queries whether a calendar component object has any recurrence rules defined
 * for it.
 *
 * Return value: TRUE if the component has recurrence rules, FALSE otherwise.
 **/
gboolean
cal_component_has_rrules (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	return (priv->rrule_list != NULL);
}

/**
 * cal_component_has_recurrences:
 * @comp: A calendar component object
 *
 * Queries whether a calendar component object has any recurrence dates or
 * recurrence rules.
 *
 * Return value: TRUE if the component has recurrences, FALSE otherwise.
 **/
gboolean
cal_component_has_recurrences (CalComponent *comp)
{
	return cal_component_has_rdates (comp) || cal_component_has_rrules (comp);
}

/* Counts the elements in the by_xxx fields of an icalrecurrencetype */
static int
count_by_xxx (short *field, int max_elements)
{
	int i;

	for (i = 0; i < max_elements; i++)
		if (field[i] == ICAL_RECURRENCE_ARRAY_MAX)
			break;

	return i;
}

gboolean
cal_component_has_simple_recurrence (CalComponent *comp)
{
	GSList *rrule_list;
	struct icalrecurrencetype *r;
	int n_by_second, n_by_minute, n_by_hour;
	int n_by_day, n_by_month_day, n_by_year_day;
	int n_by_week_no, n_by_month, n_by_set_pos;
	int len, i;
	gboolean simple = FALSE;

	if (!cal_component_has_recurrences (comp))
		return TRUE;
	
	cal_component_get_rrule_list (comp, &rrule_list);
	len = g_slist_length (rrule_list);
	if (len > 1
	    || cal_component_has_rdates (comp)
	    || cal_component_has_exrules (comp))
		goto cleanup;

	/* Down to one rule, so test that one */
	r = rrule_list->data;

	/* Any funky frequency? */
	if (r->freq == ICAL_SECONDLY_RECURRENCE
	    || r->freq == ICAL_MINUTELY_RECURRENCE
	    || r->freq == ICAL_HOURLY_RECURRENCE)
		goto cleanup;

	/* Any funky BY_* */
#define N_HAS_BY(field) (count_by_xxx (field, sizeof (field) / sizeof (field[0])))

	n_by_second = N_HAS_BY (r->by_second);
	n_by_minute = N_HAS_BY (r->by_minute);
	n_by_hour = N_HAS_BY (r->by_hour);
	n_by_day = N_HAS_BY (r->by_day);
	n_by_month_day = N_HAS_BY (r->by_month_day);
	n_by_year_day = N_HAS_BY (r->by_year_day);
	n_by_week_no = N_HAS_BY (r->by_week_no);
	n_by_month = N_HAS_BY (r->by_month);
	n_by_set_pos = N_HAS_BY (r->by_set_pos);

	if (n_by_second != 0
	    || n_by_minute != 0
	    || n_by_hour != 0)
		goto cleanup;	

	switch (r->freq) {
	case ICAL_DAILY_RECURRENCE:
		if (n_by_day != 0
		    || n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto cleanup;

		simple = TRUE;
		break;

	case ICAL_WEEKLY_RECURRENCE:
		if (n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto cleanup;

		for (i = 0; i < 8 && r->by_day[i] != ICAL_RECURRENCE_ARRAY_MAX; i++) {
			int pos;
			pos = icalrecurrencetype_day_position (r->by_day[i]);

			if (pos != 0)
				goto cleanup;
		}
		
		simple = TRUE;
		break;

	case ICAL_MONTHLY_RECURRENCE:
		if (n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos > 1)
			goto cleanup;

		if (n_by_month_day == 1) {
			int nth;

			if (n_by_set_pos != 0)
				goto cleanup;

			nth = r->by_month_day[0];
			if (nth < 1 && nth != -1)
				goto cleanup;
			
			simple = TRUE;
			
		} else if (n_by_day == 1) {
			enum icalrecurrencetype_weekday weekday;
			int pos;

			/* Outlook 2000 uses BYDAY=TU;BYSETPOS=2, and will not
			   accept BYDAY=2TU. So we now use the same as Outlook
			   by default. */

			weekday = icalrecurrencetype_day_day_of_week (r->by_day[0]);
			pos = icalrecurrencetype_day_position (r->by_day[0]);

			if (pos == 0) {
				if (n_by_set_pos != 1)
					goto cleanup;
				pos = r->by_set_pos[0];
			} else if (pos < 0) {
				goto cleanup;
			}

			switch (weekday) {
			case ICAL_MONDAY_WEEKDAY:
			case ICAL_TUESDAY_WEEKDAY:
			case ICAL_WEDNESDAY_WEEKDAY:
			case ICAL_THURSDAY_WEEKDAY:
			case ICAL_FRIDAY_WEEKDAY:
			case ICAL_SATURDAY_WEEKDAY:
			case ICAL_SUNDAY_WEEKDAY:
				break;

			default:
				goto cleanup;
			}
		} else {
			goto cleanup;
		}
		
		simple = TRUE;
		break;

	case ICAL_YEARLY_RECURRENCE:
		if (n_by_day != 0
		    || n_by_month_day != 0
		    || n_by_year_day != 0
		    || n_by_week_no != 0
		    || n_by_month != 0
		    || n_by_set_pos != 0)
			goto cleanup;
		
		simple = TRUE;
		break;

	default:
		goto cleanup;
	}

 cleanup:
	cal_component_free_recur_list (rrule_list);

	return simple;
}

gboolean 
cal_component_is_instance (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);

	priv = comp->priv;

	return !(priv->recur_id.recur_time.prop == NULL);
}

/**
 * cal_component_get_sequence:
 * @comp: A calendar component object.
 * @sequence: Return value for the sequence number.  This should be freed using
 * cal_component_free_sequence().
 *
 * Queries the sequence number of a calendar component object.
 **/
void
cal_component_get_sequence (CalComponent *comp, int **sequence)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (sequence != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!priv->sequence) {
		*sequence = NULL;
		return;
	}

	*sequence = g_new (int, 1);
	**sequence = icalproperty_get_sequence (priv->sequence);
}

/**
 * cal_component_set_sequence:
 * @comp: A calendar component object.
 * @sequence: Sequence number value.
 *
 * Sets the sequence number of a calendar component object.  Normally this
 * function should not be called, since the sequence number is incremented
 * automatically at the proper times.
 **/
void
cal_component_set_sequence (CalComponent *comp, int *sequence)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	priv->need_sequence_inc = FALSE;

	if (!sequence) {
		if (priv->sequence) {
			icalcomponent_remove_property (priv->icalcomp, priv->sequence);
			icalproperty_free (priv->sequence);
			priv->sequence = NULL;
		}

		return;
	}

	if (priv->sequence)
		icalproperty_set_sequence (priv->sequence, *sequence);
	else {
		priv->sequence = icalproperty_new_sequence (*sequence);
		icalcomponent_add_property (priv->icalcomp, priv->sequence);
	}
}

/**
 * cal_component_get_status:
 * @comp: A calendar component object.
 * @status: Return value for the status value.  It is set to #ICAL_STATUS_NONE
 * if the component has no status property.
 *
 * Queries the status property of a calendar component object.
 **/
void
cal_component_get_status (CalComponent *comp, icalproperty_status *status)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (status != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!priv->status) {
		*status = ICAL_STATUS_NONE;
		return;
	}

	*status = icalproperty_get_status (priv->status);
}

/**
 * cal_component_set_status:
 * @comp: A calendar component object.
 * @status: Status value.  You should use #ICAL_STATUS_NONE if you want to unset
 * this property.
 *
 * Sets the status property of a calendar component object.
 **/
void
cal_component_set_status (CalComponent *comp, icalproperty_status status)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	priv->need_sequence_inc = TRUE;

	if (status == ICAL_STATUS_NONE) {
		if (priv->status) {
			icalcomponent_remove_property (priv->icalcomp, priv->status);
			icalproperty_free (priv->status);
			priv->status = NULL;
		}

		return;
	}

	if (priv->status) {
		icalproperty_set_status (priv->status, status);
	} else {
		priv->status = icalproperty_new_status (status);
		icalcomponent_add_property (priv->icalcomp, priv->status);
	}
}

/**
 * cal_component_get_summary:
 * @comp: A calendar component object.
 * @summary: Return value for the summary property and its parameters.
 *
 * Queries the summary of a calendar component object.
 **/
void
cal_component_get_summary (CalComponent *comp, CalComponentText *summary)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (summary != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (priv->summary.prop)
		summary->value = icalproperty_get_summary (priv->summary.prop);
	else
		summary->value = NULL;

	if (priv->summary.altrep_param)
		summary->altrep = icalparameter_get_altrep (priv->summary.altrep_param);
	else
		summary->altrep = NULL;
}

/**
 * cal_component_set_summary:
 * @comp: A calendar component object.
 * @summary: Summary property and its parameters.
 *
 * Sets the summary of a calendar component object.
 **/
void
cal_component_set_summary (CalComponent *comp, CalComponentText *summary)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!summary) {
		if (priv->summary.prop) {
			icalcomponent_remove_property (priv->icalcomp, priv->summary.prop);
			icalproperty_free (priv->summary.prop);

			priv->summary.prop = NULL;
			priv->summary.altrep_param = NULL;
		}

		return;
	}

	g_return_if_fail (summary->value != NULL);

	if (priv->summary.prop)
		icalproperty_set_summary (priv->summary.prop, (char *) summary->value);
	else {
		priv->summary.prop = icalproperty_new_summary ((char *) summary->value);
		icalcomponent_add_property (priv->icalcomp, priv->summary.prop);
	}

	if (summary->altrep) {
		g_assert (priv->summary.prop != NULL);

		if (priv->summary.altrep_param)
			icalparameter_set_altrep (priv->summary.altrep_param,
						  (char *) summary->altrep);
		else {
			priv->summary.altrep_param = icalparameter_new_altrep (
				(char *) summary->altrep);
			icalproperty_add_parameter (priv->summary.prop,
						    priv->summary.altrep_param);
		}
	} else if (priv->summary.altrep_param) {
		icalproperty_remove_parameter (priv->summary.prop, ICAL_ALTREP_PARAMETER);
		icalparameter_free (priv->summary.altrep_param);
		priv->summary.altrep_param = NULL;
	}
}

/**
 * cal_component_get_transparency:
 * @comp: A calendar component object.
 * @transp: Return value for the time transparency.
 *
 * Queries the time transparency of a calendar component object.
 **/
void
cal_component_get_transparency (CalComponent *comp, CalComponentTransparency *transp)
{
	CalComponentPrivate *priv;
	icalproperty_transp ical_transp;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (transp != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!priv->transparency) {
		*transp = CAL_COMPONENT_TRANSP_NONE;
		return;
	}

	ical_transp = icalproperty_get_transp (priv->transparency);

	switch (ical_transp)
	{
	case ICAL_TRANSP_TRANSPARENT:
	case ICAL_TRANSP_TRANSPARENTNOCONFLICT:
	  *transp = CAL_COMPONENT_TRANSP_TRANSPARENT;
	  break;

	case ICAL_TRANSP_OPAQUE:
	case ICAL_TRANSP_OPAQUENOCONFLICT:
	  *transp = CAL_COMPONENT_TRANSP_OPAQUE;
	  break;

	default:
	  *transp = CAL_COMPONENT_TRANSP_UNKNOWN;
	  break;
	}
}

/**
 * cal_component_set_transparency:
 * @comp: A calendar component object.
 * @transp: Time transparency value.
 *
 * Sets the time transparency of a calendar component object.
 **/
void
cal_component_set_transparency (CalComponent *comp, CalComponentTransparency transp)
{
	CalComponentPrivate *priv;
	icalproperty_transp ical_transp;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (transp != CAL_COMPONENT_TRANSP_UNKNOWN);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);


	if (transp == CAL_COMPONENT_TRANSP_NONE) {
		if (priv->transparency) {
			icalcomponent_remove_property (priv->icalcomp, priv->transparency);
			icalproperty_free (priv->transparency);
			priv->transparency = NULL;
		}

		return;
	}

	switch (transp) {
	case CAL_COMPONENT_TRANSP_TRANSPARENT:
	  ical_transp = ICAL_TRANSP_TRANSPARENT;
		break;

	case CAL_COMPONENT_TRANSP_OPAQUE:
	  ical_transp = ICAL_TRANSP_OPAQUE;
		break;

	default:
		g_assert_not_reached ();
		ical_transp = ICAL_TRANSP_NONE;
	}

	if (priv->transparency)
		icalproperty_set_transp (priv->transparency, ical_transp);
	else {
		priv->transparency = icalproperty_new_transp (ical_transp);
		icalcomponent_add_property (priv->icalcomp, priv->transparency);
	}
}

/**
 * cal_component_get_url:
 * @comp: A calendar component object.
 * @url: Return value for the URL.
 *
 * Queries the uniform resource locator property of a calendar component object.
 **/
void
cal_component_get_url (CalComponent *comp, const char **url)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (url != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (priv->url)
		*url = icalproperty_get_url (priv->url);
	else
		*url = NULL;
}

/**
 * cal_component_set_url:
 * @comp: A calendar component object.
 * @url: URL value.
 *
 * Sets the uniform resource locator property of a calendar component object.
 **/
void
cal_component_set_url (CalComponent *comp, const char *url)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!url || !(*url)) {
		if (priv->url) {
			icalcomponent_remove_property (priv->icalcomp, priv->url);
			icalproperty_free (priv->url);
			priv->url = NULL;
		}

		return;
	}

	if (priv->url)
		icalproperty_set_url (priv->url, (char *) url);
	else {
		priv->url = icalproperty_new_url ((char *) url);
		icalcomponent_add_property (priv->icalcomp, priv->url);
	}
}

/* Gets a text list value */
static void
get_attendee_list (GSList *attendee_list, GSList **al)
{
	GSList *l;

	*al = NULL;

	if (!attendee_list)
		return;

	for (l = attendee_list; l; l = l->next) {
		struct attendee *attendee;
		CalComponentAttendee *a;

		attendee = l->data;
		g_assert (attendee->prop != NULL);

		a = g_new0 (CalComponentAttendee, 1);
		a->value = icalproperty_get_attendee (attendee->prop);

		if (attendee->member_param)
			a->member = icalparameter_get_member (attendee->member_param);		
		if (attendee->cutype_param)
			a->cutype = icalparameter_get_cutype (attendee->cutype_param);			
		if (attendee->role_param)
			a->role = icalparameter_get_role (attendee->role_param);
		if (attendee->partstat_param)
			a->status = icalparameter_get_role (attendee->partstat_param);
		if (attendee->rsvp_param && icalparameter_get_rsvp (attendee->rsvp_param) == ICAL_RSVP_TRUE)
			a->rsvp = TRUE;
		else
			a->rsvp = FALSE;
		if (attendee->delfrom_param)
			a->delfrom = icalparameter_get_sentby (attendee->delfrom_param);
		if (attendee->delto_param)
			a->delto = icalparameter_get_sentby (attendee->delto_param);
		if (attendee->sentby_param)
			a->sentby = icalparameter_get_sentby (attendee->sentby_param);
		if (attendee->cn_param)
			a->cn = icalparameter_get_sentby (attendee->cn_param);
		if (attendee->language_param)
			a->language = icalparameter_get_sentby (attendee->language_param);

		*al = g_slist_prepend (*al, a);
	}

	*al = g_slist_reverse (*al);
}


/* Sets a text list value */
static void
set_attendee_list (icalcomponent *icalcomp,
		   GSList **attendee_list,
		   GSList *al)
{
	GSList *l;

	/* Remove old attendees */

	for (l = *attendee_list; l; l = l->next) {
		struct attendee *attendee;

		attendee = l->data;
		g_assert (attendee->prop != NULL);

		icalcomponent_remove_property (icalcomp, attendee->prop);
		icalproperty_free (attendee->prop);
		g_free (attendee);
	}

	g_slist_free (*attendee_list);
	*attendee_list = NULL;

	/* Add in new attendees */

	for (l = al; l; l = l->next) {
		CalComponentAttendee *a;
		struct attendee *attendee;

		a = l->data;
		g_return_if_fail (a->value != NULL);

		attendee = g_new0 (struct attendee, 1);

		attendee->prop = icalproperty_new_attendee (a->value);
		icalcomponent_add_property (icalcomp, attendee->prop);

		if (a->member) {
			attendee->member_param = icalparameter_new_member (a->member);
			icalproperty_add_parameter (attendee->prop, attendee->member_param);
		}

		attendee->cutype_param = icalparameter_new_cutype (a->cutype);
		icalproperty_add_parameter (attendee->prop, attendee->cutype_param);

		attendee->role_param = icalparameter_new_role (a->role);
		icalproperty_add_parameter (attendee->prop, attendee->role_param);

		attendee->partstat_param = icalparameter_new_partstat (a->status);
		icalproperty_add_parameter (attendee->prop, attendee->partstat_param);

		if (a->rsvp)
			attendee->rsvp_param = icalparameter_new_rsvp (ICAL_RSVP_TRUE);
		else
			attendee->rsvp_param = icalparameter_new_rsvp (ICAL_RSVP_FALSE);
		icalproperty_add_parameter (attendee->prop, attendee->rsvp_param);
	
		if (a->delfrom) {
			attendee->delfrom_param = icalparameter_new_delegatedfrom (a->delfrom);
			icalproperty_add_parameter (attendee->prop, attendee->delfrom_param);
		}
		if (a->delto) {
			attendee->delto_param = icalparameter_new_delegatedto (a->delto);
			icalproperty_add_parameter (attendee->prop, attendee->delto_param);
		}
		if (a->sentby) {
			attendee->sentby_param = icalparameter_new_sentby (a->sentby);
			icalproperty_add_parameter (attendee->prop, attendee->sentby_param);
		}
		if (a->cn) {
			attendee->cn_param = icalparameter_new_cn (a->cn);
			icalproperty_add_parameter (attendee->prop, attendee->cn_param);
		}
		if (a->language) {
			attendee->language_param = icalparameter_new_language (a->language);
			icalproperty_add_parameter (attendee->prop, attendee->language_param);
		}

		*attendee_list = g_slist_prepend (*attendee_list, attendee);
	}

	*attendee_list = g_slist_reverse (*attendee_list);
}

/**
 * cal_component_get_attendee_list: 
 * @comp: A calendar component object. 
 * @attendee_list: Return value for the attendee property.
 * This should be freed using the cal_component_free_attendee_list ()
 * function.
 * 
 * Queries the attendee properties of the calendar component object
 **/
void
cal_component_get_attendee_list (CalComponent *comp, GSList **attendee_list)
{
	CalComponentPrivate *priv;
	
	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (attendee_list != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	get_attendee_list (priv->attendee_list, attendee_list);
}

/**
 * cal_component_set_attendee_list:
 * @comp: A calendar component object. 
 * @attendee_list: Values for attendee properties
 * 
 * Sets the attendees of a calendar component object
 **/
void
cal_component_set_attendee_list (CalComponent *comp, GSList *attendee_list)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	set_attendee_list (priv->icalcomp, &priv->attendee_list, attendee_list);
}

gboolean
cal_component_has_attendees (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);

	priv = comp->priv;

	if (g_slist_length (priv->attendee_list) > 0)
		return TRUE;
	
	return FALSE;
}

/**
 * cal_component_get_location:
 * @comp: A calendar component object.
 * @url: Return value for the location.
 *
 * Queries the location property of a calendar component object.
 **/
void
cal_component_get_location (CalComponent *comp, const char **location)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (location != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (priv->location)
		*location = icalproperty_get_location (priv->location);
	else
		*location = NULL;
}

/**
 * cal_component_set_location:
 * @comp: A calendar component object.
 * @url: Location value.
 *
 * Sets the location property of a calendar component object.
 **/
void
cal_component_set_location (CalComponent *comp, const char *location)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!location || !(*location)) {
		if (priv->location) {
			icalcomponent_remove_property (priv->icalcomp, priv->location);
			icalproperty_free (priv->location);
			priv->location = NULL;
		}

		return;
	}

	if (priv->location)
		icalproperty_set_location (priv->location, (char *) location);
	else {
		priv->location = icalproperty_new_location ((char *) location);
		icalcomponent_add_property (priv->icalcomp, priv->location);
	}
}



/**
 * cal_component_free_categories_list:
 * @categ_list: List of category strings.
 *
 * Frees a list of category strings.
 **/
void
cal_component_free_categories_list (GSList *categ_list)
{
	GSList *l;

	for (l = categ_list; l; l = l->next)
		g_free (l->data);

	g_slist_free (categ_list);
}

/**
 * cal_component_free_datetime:
 * @dt: A date/time structure.
 *
 * Frees a date/time structure.
 **/
void
cal_component_free_datetime (CalComponentDateTime *dt)
{
	g_return_if_fail (dt != NULL);

	g_free (dt->value);
	g_free ((char*)dt->tzid);
}

void 
cal_component_free_range (CalComponentRange *range)
{
	g_return_if_fail (range != NULL);

	cal_component_free_datetime (&range->datetime);
}

/**
 * cal_component_free_exdate_list:
 * @exdate_list: List of #CalComponentDateTime structures.
 *
 * Frees a list of #CalComponentDateTime structures as returned by the
 * cal_component_get_exdate_list() function.
 **/
void
cal_component_free_exdate_list (GSList *exdate_list)
{
	GSList *l;

	for (l = exdate_list; l; l = l->next) {
		CalComponentDateTime *cdt;

		g_assert (l->data != NULL);
		cdt = l->data;

		g_assert (cdt->value != NULL);
		g_free (cdt->value);
		g_free ((char*)cdt->tzid);

		g_free (cdt);
	}

	g_slist_free (exdate_list);
}

/**
 * cal_component_free_geo:
 * @geo: An #icalgeotype structure.
 *
 * Frees a struct #icalgeotype structure as returned by the calendar component
 * functions.
 **/
void
cal_component_free_geo (struct icalgeotype *geo)
{
	g_return_if_fail (geo != NULL);

	g_free (geo);
}

/**
 * cal_component_free_icaltimetype:
 * @t: An #icaltimetype structure.
 *
 * Frees a struct #icaltimetype value as returned by the calendar component
 * functions.
 **/
void
cal_component_free_icaltimetype (struct icaltimetype *t)
{
	g_return_if_fail (t != NULL);

	g_free (t);
}

/**
 * cal_component_free_percent:
 * @percent: Percent value.
 *
 * Frees a percent value as returned by the cal_component_get_percent()
 * function.
 **/
void
cal_component_free_percent (int *percent)
{
	g_return_if_fail (percent != NULL);

	g_free (percent);
}

/**
 * cal_component_free_priority:
 * @priority: Priority value.
 *
 * Frees a priority value as returned by the cal_component_get_priority()
 * function.
 **/
void
cal_component_free_priority (int *priority)
{
	g_return_if_fail (priority != NULL);

	g_free (priority);
}

/**
 * cal_component_free_period_list:
 * @period_list: List of #CalComponentPeriod structures.
 *
 * Frees a list of #CalComponentPeriod structures.
 **/
void
cal_component_free_period_list (GSList *period_list)
{
	GSList *l;

	for (l = period_list; l; l = l->next) {
		CalComponentPeriod *period;

		g_assert (l->data != NULL);

		period = l->data;
		g_free (period);
	}

	g_slist_free (period_list);
}

/**
 * cal_component_free_recur_list:
 * @recur_list: List of struct #icalrecurrencetype structures.
 *
 * Frees a list of struct #icalrecurrencetype structures.
 **/
void
cal_component_free_recur_list (GSList *recur_list)
{
	GSList *l;

	for (l = recur_list; l; l = l->next) {
		struct icalrecurrencetype *r;

		g_assert (l->data != NULL);
		r = l->data;

		g_free (r);
	}

	g_slist_free (recur_list);
}

/**
 * cal_component_free_sequence:
 * @sequence: Sequence number value.
 *
 * Frees a sequence number value.
 **/
void
cal_component_free_sequence (int *sequence)
{
	g_return_if_fail (sequence != NULL);

	g_free (sequence);
}

/**
 * cal_component_free_text_list:
 * @text_list: List of #CalComponentText structures.
 *
 * Frees a list of #CalComponentText structures.  This function should only be
 * used to free lists of text values as returned by the other getter functions
 * of #CalComponent.
 **/
void
cal_component_free_text_list (GSList *text_list)
{
	GSList *l;

	for (l = text_list; l; l = l->next) {
		CalComponentText *text;

		g_assert (l->data != NULL);

		text = l->data;
		g_return_if_fail (text != NULL);
		g_free (text);
	}

	g_slist_free (text_list);
}

/**
 * cal_component_free_attendee_list:
 * @attendee_list: 
 * 
 * 
 **/
void
cal_component_free_attendee_list (GSList *attendee_list)
{
	GSList *l;

	for (l = attendee_list; l; l = l->next) {
		CalComponentAttendee *attendee;

		g_assert (l->data != NULL);

		attendee = l->data;
		g_return_if_fail (attendee != NULL);
		g_free (attendee);
	}

	g_slist_free (attendee_list);
}



/**
 * cal_component_has_alarms:
 * @comp: A calendar component object.
 *
 * Checks whether the component has any alarms.
 *
 * Return value: TRUE if the component has any alarms.
 **/
gboolean
cal_component_has_alarms (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), FALSE);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, FALSE);

	return g_hash_table_size (priv->alarm_uid_hash) != 0;
}

/**
 * cal_component_add_alarm:
 * @comp: A calendar component.
 * @alarm: An alarm.
 * 
 * Adds an alarm subcomponent to a calendar component.  You should have created
 * the @alarm by using cal_component_alarm_new(); it is invalid to use a
 * #CalComponentAlarm structure that came from cal_component_get_alarm().  After
 * adding the alarm, the @alarm structure is no longer valid because the
 * internal structures may change and you should get rid of it by using
 * cal_component_alarm_free().
 **/
void
cal_component_add_alarm (CalComponent *comp, CalComponentAlarm *alarm)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (alarm != NULL);

	priv = comp->priv;

	add_alarm (comp, alarm->icalcomp, icalproperty_get_x (alarm->uid));
	icalcomponent_add_component (priv->icalcomp, alarm->icalcomp);
}

/**
 * cal_component_remove_alarm:
 * @comp: A calendar component.
 * @auid: UID of the alarm to remove.
 * 
 * Removes an alarm subcomponent from a calendar component.  If the alarm that
 * corresponds to the specified @auid had been fetched with
 * cal_component_get_alarm(), then those alarm structures will be invalid; you
 * should get rid of them with cal_component_alarm_free() before using this
 * function.
 **/
void
cal_component_remove_alarm (CalComponent *comp, const char *auid)
{
	CalComponentPrivate *priv;
	icalcomponent *alarm;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (auid != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	alarm = g_hash_table_lookup (priv->alarm_uid_hash, auid);
	if (!alarm)
		return;

	g_hash_table_remove (priv->alarm_uid_hash, auid);
	icalcomponent_remove_component (priv->icalcomp, alarm);
	icalcomponent_free (alarm);
}

static gboolean
for_each_remove_all_alarms (gpointer key, gpointer value, gpointer data)
{
	CalComponent *comp = CAL_COMPONENT (data);
	CalComponentPrivate *priv;
	icalcomponent *alarm = value;
	
	priv = comp->priv;
	
	icalcomponent_remove_component (priv->icalcomp, alarm);
	icalcomponent_free (alarm);

	return TRUE;	
}

/**
 * cal_component_remove_all_alarms:
 * @comp: A calendar component
 * 
 * Remove all alarms from the calendar component
 **/
void
cal_component_remove_all_alarms (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	g_hash_table_foreach_remove (priv->alarm_uid_hash, for_each_remove_all_alarms, comp);
}


/* Scans an icalproperty from a calendar component and adds its mapping to our
 * own alarm structure.
 */
static void
scan_alarm_property (CalComponentAlarm *alarm, icalproperty *prop)
{
	icalproperty_kind kind;
	const char *xname;

	kind = icalproperty_isa (prop);

	switch (kind) {
	case ICAL_ACTION_PROPERTY:
		alarm->action = prop;
		break;

	case ICAL_ATTACH_PROPERTY:
		/* FIXME: mail alarms may have any number of these, not just one */
		alarm->attach = prop;
		break;

	case ICAL_DESCRIPTION_PROPERTY:
		alarm->description.prop = prop;
		alarm->description.altrep_param = icalproperty_get_first_parameter (
			prop, ICAL_ALTREP_PARAMETER);
		break;

	case ICAL_DURATION_PROPERTY:
		alarm->duration = prop;
		break;

	case ICAL_REPEAT_PROPERTY:
		alarm->repeat = prop;
		break;

	case ICAL_TRIGGER_PROPERTY:
		alarm->trigger = prop;
		break;

	case ICAL_ATTENDEE_PROPERTY:
		scan_attendee (&alarm->attendee_list, prop);
		break;
		
	case ICAL_X_PROPERTY:
		xname = icalproperty_get_x_name (prop);
		g_assert (xname != NULL);

		if (strcmp (xname, EVOLUTION_ALARM_UID_PROPERTY) == 0)
			alarm->uid = prop;

		break;

	default:
		break;
	}
}

/* Creates a CalComponentAlarm from a libical alarm subcomponent */
static CalComponentAlarm *
make_alarm (icalcomponent *subcomp)
{
	CalComponentAlarm *alarm;
	icalproperty *prop;

	alarm = g_new (CalComponentAlarm, 1);

	alarm->icalcomp = subcomp;
	alarm->uid = NULL;

	alarm->action = NULL;
	alarm->attach = NULL;
	alarm->description.prop = NULL;
	alarm->description.altrep_param = NULL;
	alarm->duration = NULL;
	alarm->repeat = NULL;
	alarm->trigger = NULL;
	alarm->attendee_list = NULL;
	
	for (prop = icalcomponent_get_first_property (subcomp, ICAL_ANY_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (subcomp, ICAL_ANY_PROPERTY))
		scan_alarm_property (alarm, prop);

	g_assert (alarm->uid != NULL);

	return alarm;
}

/**
 * cal_component_get_alarm_uids:
 * @comp: A calendar component.
 *
 * Builds a list of the unique identifiers of the alarm subcomponents inside a
 * calendar component.
 *
 * Return value: List of unique identifiers for alarms.  This should be freed
 * using cal_obj_uid_list_free().
 **/
GList *
cal_component_get_alarm_uids (CalComponent *comp)
{
	CalComponentPrivate *priv;
	icalcompiter iter;
	GList *l;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), NULL);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	l = NULL;
	for (iter = icalcomponent_begin_component (priv->icalcomp, ICAL_VALARM_COMPONENT);
	     icalcompiter_deref (&iter) != NULL;
	     icalcompiter_next (&iter)) {
		icalcomponent *subcomp;
		icalproperty *prop;
		
		subcomp = icalcompiter_deref (&iter);
		for (prop = icalcomponent_get_first_property (subcomp, ICAL_X_PROPERTY);
		     prop;
		     prop = icalcomponent_get_next_property (subcomp, ICAL_X_PROPERTY)) {
			const char *xname;
			
			xname = icalproperty_get_x_name (prop);
			g_assert (xname != NULL);
			
			if (strcmp (xname, EVOLUTION_ALARM_UID_PROPERTY) == 0) {
				const char *auid;
				
				auid = alarm_uid_from_prop (prop);
				l = g_list_append (l, g_strdup (auid));
			}
		}
	}

	return l;
}

/**
 * cal_component_get_alarm:
 * @comp: A calendar component.
 * @auid: Unique identifier for the sought alarm subcomponent.
 *
 * Queries a particular alarm subcomponent of a calendar component.
 *
 * Return value: The alarm subcomponent that corresponds to the specified @auid,
 * or #NULL if no alarm exists with that UID.  This should be freed using
 * cal_component_alarm_free().
 **/
CalComponentAlarm *
cal_component_get_alarm (CalComponent *comp, const char *auid)
{
	CalComponentPrivate *priv;
	icalcomponent *alarm;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), NULL);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	g_return_val_if_fail (auid != NULL, NULL);

	alarm = g_hash_table_lookup (priv->alarm_uid_hash, auid);

	if (alarm)
		return make_alarm (alarm);
	else
		return NULL;
}

/**
 * cal_component_alarms_free:
 * @alarms: Component alarms structure.
 *
 * Frees a #CalComponentAlarms structure.
 **/
void
cal_component_alarms_free (CalComponentAlarms *alarms)
{
	GSList *l;

	g_return_if_fail (alarms != NULL);

	g_assert (alarms->comp != NULL);
	g_object_unref (G_OBJECT (alarms->comp));

	for (l = alarms->alarms; l; l = l->next) {
		CalAlarmInstance *instance;

		instance = l->data;
		g_assert (instance != NULL);
		g_free (instance);
	}

	g_slist_free (alarms->alarms);
	g_free (alarms);
}

/**
 * cal_component_alarm_new:
 *
 *
 *
 * Return value: a new alarm component
 **/
CalComponentAlarm *
cal_component_alarm_new (void)
{
	CalComponentAlarm *alarm;
	char *new_auid ;

	alarm = g_new (CalComponentAlarm, 1);

	alarm->icalcomp = icalcomponent_new (ICAL_VALARM_COMPONENT);

	new_auid = cal_component_gen_uid ();
	alarm->uid = icalproperty_new_x (new_auid);
	icalproperty_set_x_name (alarm->uid, EVOLUTION_ALARM_UID_PROPERTY);
	icalcomponent_add_property (alarm->icalcomp, alarm->uid);
	g_free (new_auid);

	alarm->action = NULL;
	alarm->attach = NULL;
	alarm->description.prop = NULL;
	alarm->description.altrep_param = NULL;
	alarm->duration = NULL;
	alarm->repeat = NULL;
	alarm->trigger = NULL;
	alarm->attendee_list = NULL;
	
	return alarm;
}

/**
 * cal_component_alarm_clone:
 * @alarm: An alarm subcomponent.
 * 
 * Creates a new alarm subcomponent by copying the information from another one.
 * 
 * Return value: A newly-created alarm subcomponent with the same values as the
 * original one.  Should be freed with cal_component_alarm_free().
 **/
CalComponentAlarm *
cal_component_alarm_clone (CalComponentAlarm *alarm)
{
	icalcomponent *icalcomp;

	g_return_val_if_fail (alarm != NULL, NULL);

	icalcomp = icalcomponent_new_clone (alarm->icalcomp);
	return make_alarm (icalcomp);
}

/**
 * cal_component_alarm_free:
 * @alarm: A calendar alarm.
 *
 * Frees an alarm structure.
 **/
void
cal_component_alarm_free (CalComponentAlarm *alarm)
{
	GSList *l;
	
	g_return_if_fail (alarm != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (icalcomponent_get_parent (alarm->icalcomp) == NULL)
		icalcomponent_free (alarm->icalcomp);

	alarm->icalcomp = NULL;
	alarm->uid = NULL;
	alarm->action = NULL;
	alarm->attach = NULL;
	alarm->description.prop = NULL;
	alarm->description.altrep_param = NULL;
	alarm->duration = NULL;
	alarm->repeat = NULL;
	alarm->trigger = NULL;

	for (l = alarm->attendee_list; l != NULL; l = l->next)
		g_free (l->data);
	g_slist_free (alarm->attendee_list);
	alarm->attendee_list = NULL;
	
	g_free (alarm);
}

/**
 * cal_component_alarm_get_uid:
 * @alarm: An alarm subcomponent.
 *
 * Queries the unique identifier of an alarm subcomponent.
 *
 * Return value: UID of the alarm.
 **/
const char *
cal_component_alarm_get_uid (CalComponentAlarm *alarm)
{
	g_return_val_if_fail (alarm != NULL, NULL);

	return alarm_uid_from_prop (alarm->uid);
}

/**
 * cal_component_alarm_get_action:
 * @alarm: An alarm.
 * @action: Return value for the alarm's action type.
 *
 * Queries the action type of an alarm.
 **/
void
cal_component_alarm_get_action (CalComponentAlarm *alarm, CalAlarmAction *action)
{
	enum icalproperty_action ipa;

	g_return_if_fail (alarm != NULL);
	g_return_if_fail (action != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (!alarm->action) {
		*action = CAL_ALARM_NONE;
		return;
	}

	ipa = icalproperty_get_action (alarm->action);

	switch (ipa) {
	case ICAL_ACTION_AUDIO:
		*action = CAL_ALARM_AUDIO;
		break;

	case ICAL_ACTION_DISPLAY:
		*action = CAL_ALARM_DISPLAY;
		break;

	case ICAL_ACTION_EMAIL:
		*action = CAL_ALARM_EMAIL;
		break;

	case ICAL_ACTION_PROCEDURE:
		*action = CAL_ALARM_PROCEDURE;
		break;

	case ICAL_ACTION_NONE:
		*action = CAL_ALARM_NONE;
		break;

	default:
		*action = CAL_ALARM_UNKNOWN;
	}
}

/**
 * cal_component_alarm_set_action:
 * @alarm: An alarm.
 * @action: Action type.
 *
 * Sets the action type for an alarm.
 **/
void
cal_component_alarm_set_action (CalComponentAlarm *alarm, CalAlarmAction action)
{
	enum icalproperty_action ipa;

	g_return_if_fail (alarm != NULL);
	g_return_if_fail (action != CAL_ALARM_NONE);
	g_return_if_fail (action != CAL_ALARM_UNKNOWN);

	g_assert (alarm->icalcomp != NULL);

	switch (action) {
	case CAL_ALARM_AUDIO:
		ipa = ICAL_ACTION_AUDIO;
		break;

	case CAL_ALARM_DISPLAY:
		ipa = ICAL_ACTION_DISPLAY;
		break;

	case CAL_ALARM_EMAIL:
		ipa = ICAL_ACTION_EMAIL;
		break;

	case CAL_ALARM_PROCEDURE:
		ipa = ICAL_ACTION_PROCEDURE;
		break;

	default:
		g_assert_not_reached ();
		ipa = ICAL_ACTION_NONE;
	}

	if (alarm->action)
		icalproperty_set_action (alarm->action, ipa);
	else {
		alarm->action = icalproperty_new_action (ipa);
		icalcomponent_add_property (alarm->icalcomp, alarm->action);
	}
}

/**
 * cal_component_alarm_get_attach:
 * @alarm: An alarm.
 * @attach: Return value for the attachment; should be freed using icalattach_unref().
 * 
 * Queries the attachment property of an alarm.
 **/
void
cal_component_alarm_get_attach (CalComponentAlarm *alarm, icalattach **attach)
{
	g_return_if_fail (alarm != NULL);
	g_return_if_fail (attach != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (alarm->attach) {
		*attach = icalproperty_get_attach (alarm->attach);
		icalattach_ref (*attach);
	} else
		*attach = NULL;
}

/**
 * cal_component_alarm_set_attach:
 * @alarm: An alarm.
 * @attach: Attachment property or NULL to remove an existing property.
 * 
 * Sets the attachment property of an alarm.
 **/
void
cal_component_alarm_set_attach (CalComponentAlarm *alarm, icalattach *attach)
{
	g_return_if_fail (alarm != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (alarm->attach) {
		icalcomponent_remove_property (alarm->icalcomp, alarm->attach);
		icalproperty_free (alarm->attach);
		alarm->attach = NULL;
	}

	if (attach) {
		alarm->attach = icalproperty_new_attach (attach);
		icalcomponent_add_property (alarm->icalcomp, alarm->attach);
	}
}

/**
 * cal_component_alarm_get_description:
 * @alarm: An alarm.
 * @description: Return value for the description property and its parameters.
 * 
 * Queries the description property of an alarm.
 **/
void
cal_component_alarm_get_description (CalComponentAlarm *alarm, CalComponentText *description)
{
	g_return_if_fail (alarm != NULL);
	g_return_if_fail (description != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (alarm->description.prop)
		description->value = icalproperty_get_description (alarm->description.prop);
	else
		description->value = NULL;

	if (alarm->description.altrep_param)
		description->altrep = icalparameter_get_altrep (alarm->description.altrep_param);
	else
		description->altrep = NULL;
}

/**
 * cal_component_alarm_set_description:
 * @alarm: An alarm.
 * @description: Description property and its parameters, or NULL for no description.
 * 
 * Sets the description property of an alarm.
 **/
void
cal_component_alarm_set_description (CalComponentAlarm *alarm, CalComponentText *description)
{
	g_return_if_fail (alarm != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (alarm->description.prop) {
		icalcomponent_remove_property (alarm->icalcomp, alarm->description.prop);
		icalproperty_free (alarm->description.prop);

		alarm->description.prop = NULL;
		alarm->description.altrep_param = NULL;
	}

	if (!description)
		return;

	g_return_if_fail (description->value != NULL);

	alarm->description.prop = icalproperty_new_description (description->value);
	icalcomponent_add_property (alarm->icalcomp, alarm->description.prop);

	if (description->altrep) {
		alarm->description.altrep_param = icalparameter_new_altrep (
			(char *) description->altrep);
		icalproperty_add_parameter (alarm->description.prop,
					    alarm->description.altrep_param);
	}
}

/**
 * cal_component_alarm_get_repeat:
 * @alarm: An alarm.
 * @repeat: Return value for the repeat/duration properties.
 * 
 * Queries the repeat/duration properties of an alarm.
 **/
void
cal_component_alarm_get_repeat (CalComponentAlarm *alarm, CalAlarmRepeat *repeat)
{
	g_return_if_fail (alarm != NULL);
	g_return_if_fail (repeat != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (!(alarm->repeat && alarm->duration)) {
		repeat->repetitions = 0;
		memset (&repeat->duration, 0, sizeof (repeat->duration));
		return;
	}

	repeat->repetitions = icalproperty_get_repeat (alarm->repeat);
	repeat->duration = icalproperty_get_duration (alarm->duration);
}

/**
 * cal_component_alarm_set_repeat:
 * @alarm: An alarm.
 * @repeat: Repeat/duration values.  To remove any repetitions from the alarm,
 * set the @repeat.repetitions to 0.
 * 
 * Sets the repeat/duration values for an alarm.
 **/
void
cal_component_alarm_set_repeat (CalComponentAlarm *alarm, CalAlarmRepeat repeat)
{
	g_return_if_fail (alarm != NULL);
	g_return_if_fail (repeat.repetitions >= 0);

	g_assert (alarm->icalcomp != NULL);

	/* Delete old properties */

	if (alarm->repeat) {
		icalcomponent_remove_property (alarm->icalcomp, alarm->repeat);
		icalproperty_free (alarm->repeat);
		alarm->repeat = NULL;
	}

	if (alarm->duration) {
		icalcomponent_remove_property (alarm->icalcomp, alarm->duration);
		icalproperty_free (alarm->duration);
		alarm->duration = NULL;
	}

	/* Set the new properties */

	if (repeat.repetitions == 0)
		return; /* For zero extra repetitions the properties should not exist */

	alarm->repeat = icalproperty_new_repeat (repeat.repetitions);
	icalcomponent_add_property (alarm->icalcomp, alarm->repeat);

	alarm->duration = icalproperty_new_duration (repeat.duration);
	icalcomponent_add_property (alarm->icalcomp, alarm->duration);
}

/**
 * cal_component_alarm_get_trigger:
 * @alarm: An alarm.
 * @trigger: Return value for the trigger time.
 *
 * Queries the trigger time for an alarm.
 **/
void
cal_component_alarm_get_trigger (CalComponentAlarm *alarm, CalAlarmTrigger *trigger)
{
	icalparameter *param;
	struct icaltriggertype t;
	gboolean relative;

	g_return_if_fail (alarm != NULL);
	g_return_if_fail (trigger != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (!alarm->trigger) {
		trigger->type = CAL_ALARM_TRIGGER_NONE;
		return;
	}

	/* Get trigger type */

	param = icalproperty_get_first_parameter (alarm->trigger, ICAL_VALUE_PARAMETER);
	if (param) {
		icalparameter_value value;

		value = icalparameter_get_value (param);

		switch (value) {
		case ICAL_VALUE_DURATION:
			relative = TRUE;
			break;

		case ICAL_VALUE_DATETIME:
			relative = FALSE;
			break;

		default:
			g_message ("cal_component_alarm_get_trigger(): Unknown value for trigger "
				   "value %d; using RELATIVE", value);

			relative = TRUE;
			break;
		}
	} else
		relative = TRUE;

	/* Get trigger value and the RELATED parameter */

	t = icalproperty_get_trigger (alarm->trigger);

	if (relative) {
		trigger->u.rel_duration = t.duration;

		param = icalproperty_get_first_parameter (alarm->trigger, ICAL_RELATED_PARAMETER);
		if (param) {
			icalparameter_related rel;

			rel = icalparameter_get_related (param);

			switch (rel) {
			case ICAL_RELATED_START:
				trigger->type = CAL_ALARM_TRIGGER_RELATIVE_START;
				break;

			case ICAL_RELATED_END:
				trigger->type = CAL_ALARM_TRIGGER_RELATIVE_END;
				break;

			default:
				g_assert_not_reached ();
			}
		} else
			trigger->type = CAL_ALARM_TRIGGER_RELATIVE_START;
	} else {
		trigger->u.abs_time = t.time;
		trigger->type = CAL_ALARM_TRIGGER_ABSOLUTE;
	}
}

/**
 * cal_component_alarm_set_trigger:
 * @alarm: An alarm.
 * @trigger: Trigger time structure.
 *
 * Sets the trigger time of an alarm.
 **/
void
cal_component_alarm_set_trigger (CalComponentAlarm *alarm, CalAlarmTrigger trigger)
{
	struct icaltriggertype t;
	icalparameter *param;
	icalparameter_value value_type;
	icalparameter_related related;

	g_return_if_fail (alarm != NULL);
	g_return_if_fail (trigger.type != CAL_ALARM_TRIGGER_NONE);

	g_assert (alarm->icalcomp != NULL);

	/* Delete old trigger */

	if (alarm->trigger) {
		icalcomponent_remove_property (alarm->icalcomp, alarm->trigger);
		icalproperty_free (alarm->trigger);
		alarm->trigger = NULL;
	}

	/* Set the value */

	related = ICAL_RELATED_START; /* Keep GCC happy */

	t.time = icaltime_null_time ();
	t.duration = icaldurationtype_null_duration ();
	switch (trigger.type) {
	case CAL_ALARM_TRIGGER_RELATIVE_START:
		t.duration = trigger.u.rel_duration;
		value_type = ICAL_VALUE_DURATION;
		related = ICAL_RELATED_START;
		break;

	case CAL_ALARM_TRIGGER_RELATIVE_END:
		t.duration = trigger.u.rel_duration;
		value_type = ICAL_VALUE_DURATION;
		related = ICAL_RELATED_END;
		break;

	case CAL_ALARM_TRIGGER_ABSOLUTE:
		t.time = trigger.u.abs_time;
		value_type = ICAL_VALUE_DATETIME;
		break;

	default:
		g_assert_not_reached ();
		return;
	}

	alarm->trigger = icalproperty_new_trigger (t);
	icalcomponent_add_property (alarm->icalcomp, alarm->trigger);

	/* Value parameters */

	param = icalproperty_get_first_parameter (alarm->trigger, ICAL_VALUE_PARAMETER);
	if (param)
		icalparameter_set_value (param, value_type);
	else {
		param = icalparameter_new_value (value_type);
		icalproperty_add_parameter (alarm->trigger, param);
	}

	/* Related parameter */

	if (trigger.type != CAL_ALARM_TRIGGER_ABSOLUTE) {
		param = icalproperty_get_first_parameter (alarm->trigger, ICAL_RELATED_PARAMETER);

		if (param)
			icalparameter_set_related (param, related);
		else {
			param = icalparameter_new_related (related);
			icalproperty_add_parameter (alarm->trigger, param);
		}
	}
}

void 
cal_component_alarm_get_attendee_list (CalComponentAlarm *alarm, GSList **attendee_list)
{
	g_return_if_fail (alarm != NULL);
	
	get_attendee_list (alarm->attendee_list, attendee_list);
}

void 
cal_component_alarm_set_attendee_list (CalComponentAlarm *alarm, GSList *attendee_list)
{
	g_return_if_fail (alarm != NULL);

	set_attendee_list (alarm->icalcomp, &alarm->attendee_list, attendee_list);
}

gboolean 
cal_component_alarm_has_attendees (CalComponentAlarm *alarm)
{

	g_return_val_if_fail (alarm != NULL, FALSE);

	if (g_slist_length (alarm->attendee_list) > 0)
		return TRUE;
	
	return FALSE;	
}


/**
 * cal_component_alarm_get_icalcomponent
 * @alarm: An alarm.
 *
 * Get the icalcomponent associated with the given #CalComponentAlarm.
 *
 * Returns: the icalcomponent.
 */
icalcomponent *
cal_component_alarm_get_icalcomponent (CalComponentAlarm *alarm)
{
	g_return_val_if_fail (alarm != NULL, NULL);
	return alarm->icalcomp;
}

/* Returns TRUE if both strings match, i.e. they are both NULL or the
   strings are equal. */
static gboolean
cal_component_strings_match	(const gchar	*string1,
				 const gchar	*string2)
{
	if (string1 == NULL || string2 == NULL)
		return (string1 == string2) ? TRUE : FALSE;

	if (!strcmp (string1, string2))
		return TRUE;

	return FALSE;
}


/**
 * cal_component_event_dates_match:
 * @comp1: A calendar component object.
 * @comp2: A calendar component object.
 *
 * Checks if the DTSTART and DTEND properties of the 2 components match.
 * Note that the events may have different recurrence properties which are not
 * taken into account here.
 *
 * Returns: TRUE if the DTSTART and DTEND properties of the 2 components match.
 **/
gboolean
cal_component_event_dates_match	(CalComponent *comp1,
				 CalComponent *comp2)
{
	CalComponentDateTime comp1_dtstart, comp1_dtend;
	CalComponentDateTime comp2_dtstart, comp2_dtend;
	gboolean retval = TRUE;

	cal_component_get_dtstart (comp1, &comp1_dtstart);
	cal_component_get_dtend   (comp1, &comp1_dtend);
	cal_component_get_dtstart (comp2, &comp2_dtstart);
	cal_component_get_dtend   (comp2, &comp2_dtend);

	/* If either value is NULL they must both be NULL to match. */
	if (comp1_dtstart.value == NULL || comp2_dtstart.value == NULL) {
		if (comp1_dtstart.value != comp2_dtstart.value) {
			retval = FALSE;
			goto out;
		}
	} else {
		if (icaltime_compare (*comp1_dtstart.value,
				      *comp2_dtstart.value)) {
			retval = FALSE;
			goto out;
		}
	}

	if (comp1_dtend.value == NULL || comp2_dtend.value == NULL) {
		if (comp1_dtend.value != comp2_dtend.value) {
			retval = FALSE;
			goto out;
		}
	} else {
		if (icaltime_compare (*comp1_dtend.value,
				      *comp2_dtend.value)) {
			retval = FALSE;
			goto out;
		}
	}

	/* Now check the timezones. */
	if (!cal_component_strings_match (comp1_dtstart.tzid,
					  comp2_dtstart.tzid)) {
		retval = FALSE;
		goto out;
	}

	if (!cal_component_strings_match (comp1_dtend.tzid,
					  comp2_dtend.tzid)) {
		retval = FALSE;
	}

 out:

	cal_component_free_datetime (&comp1_dtstart);
	cal_component_free_datetime (&comp1_dtend);
	cal_component_free_datetime (&comp2_dtstart);
	cal_component_free_datetime (&comp2_dtend);

	return retval;
}


