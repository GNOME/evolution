/* Evolution calendar - iCalendar component object
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include "cal-component.h"
#include "timeutil.h"



/* Private part of the CalComponent structure */
struct _CalComponentPrivate {
	/* The icalcomponent we wrap */
	icalcomponent *icalcomp;

	/* Properties */

	icalproperty *uid;

	struct categories {
		icalproperty *prop;
	};
	GSList *categories_list; /* list of struct categories */

	icalproperty *classification;

	struct text {
		icalproperty *prop;
		icalparameter *altrep_param;
	};

	GSList *comment_list; /* list of struct text */

	icalproperty *completed;
	icalproperty *created;

	GSList *description_list; /* list of struct text */

	struct datetime {
		icalproperty *prop;
		icalparameter *tzid_param;
	};

	struct datetime dtstart;
	struct datetime dtend;

	icalproperty *dtstamp;

	struct datetime due;

	GSList *exdate_list; /* list of icalproperty objects */
	GSList *exrule_list; /* list of icalproperty objects */

	icalproperty *geo;
	icalproperty *last_modified;
	icalproperty *percent;
	icalproperty *priority;

	struct period {
		icalproperty *prop;
		icalparameter *value_param;
	};

	GSList *rdate_list; /* list of struct period */

	GSList *rrule_list; /* list of icalproperty objects */

	icalproperty *sequence;

	struct {
		icalproperty *prop;
		icalparameter *altrep_param;
	} summary;

	icalproperty *transparency;
	icalproperty *url;

	/* Whether we should increment the sequence number when piping the
	 * object over the wire.
	 */
	guint need_sequence_inc : 1;
};

/* Private structure for alarms */
struct _CalComponentAlarm {
	/* Our parent component */
	CalComponent *parent;

	/* Alarm icalcomponent we wrap */
	icalcomponent *icalcomp;

	/* Properties */

	icalproperty *action;
	icalproperty *trigger;
};



static void cal_component_class_init (CalComponentClass *class);
static void cal_component_init (CalComponent *comp);
static void cal_component_destroy (GtkObject *object);

static GtkObjectClass *parent_class;



/**
 * cal_component_get_type:
 *
 * Registers the #CalComponent class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: The type ID of the #CalComponent class.
 **/
GtkType
cal_component_get_type (void)
{
	static GtkType cal_component_type = 0;

	if (!cal_component_type) {
		static const GtkTypeInfo cal_component_info = {
			"CalComponent",
			sizeof (CalComponent),
			sizeof (CalComponentClass),
			(GtkClassInitFunc) cal_component_class_init,
			(GtkObjectInitFunc) cal_component_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		cal_component_type = gtk_type_unique (GTK_TYPE_OBJECT, &cal_component_info);
	}

	return cal_component_type;
}

/* Class initialization function for the calendar component object */
static void
cal_component_class_init (CalComponentClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = cal_component_destroy;
}

/* Object initialization function for the calendar component object */
static void
cal_component_init (CalComponent *comp)
{
	CalComponentPrivate *priv;

	priv = g_new0 (CalComponentPrivate, 1);
	comp->priv = priv;
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

/* Frees the internal icalcomponent only if it does not have a parent.  If it
 * does, it means we don't own it and we shouldn't free it.
 */
static void
free_icalcomponent (CalComponent *comp)
{
	CalComponentPrivate *priv;

	priv = comp->priv;

	if (!priv->icalcomp)
		return;

	/* Free the icalcomponent */

	if (icalcomponent_get_parent (priv->icalcomp) != NULL)
		icalcomponent_free (priv->icalcomp);

	priv->icalcomp = NULL;

	/* Free the mappings */

	priv->uid = NULL;

	priv->categories_list = free_slist (priv->categories_list);

	priv->classification = NULL;
	priv->comment_list = NULL;
	priv->completed = NULL;
	priv->created = NULL;

	priv->description_list = free_slist (priv->description_list);

	priv->dtend.prop = NULL;
	priv->dtend.tzid_param = NULL;

	priv->dtstamp = NULL;

	priv->dtstart.prop = NULL;
	priv->dtstart.tzid_param = NULL;

	priv->due.prop = NULL;
	priv->due.tzid_param = NULL;

	g_slist_free (priv->exdate_list);
	priv->exdate_list = NULL;

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

	/* Clean up */

	priv->need_sequence_inc = FALSE;
}

/* Destroy handler for the calendar component object */
static void
cal_component_destroy (GtkObject *object)
{
	CalComponent *comp;
	CalComponentPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (object));

	comp = CAL_COMPONENT (object);
	priv = comp->priv;

	free_icalcomponent (comp);

	g_free (priv);
	comp->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
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

	return g_strdup_printf (
		"%s-%d-%d-%d-%d@%s",
		isodate_from_time_t (t),
		getpid (),
		getgid (),
		getppid (),
		serial++,
		hostname);
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
	return CAL_COMPONENT (gtk_type_new (CAL_COMPONENT_TYPE));
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

/* Scans the categories property */
static void
scan_categories (CalComponent *comp, icalproperty *prop)
{
	CalComponentPrivate *priv;
	struct categories *categ;

	priv = comp->priv;

	categ = g_new (struct categories, 1);
	categ->prop = prop;

	priv->categories_list = g_slist_append (priv->categories_list, categ);
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

	priv = comp->priv;
	priv->exdate_list = g_slist_append (priv->exdate_list, prop);
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
	case ICAL_CATEGORIES_PROPERTY:
		scan_categories (comp, prop);
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

	case ICAL_PERCENTCOMPLETE_PROPERTY:
		priv->percent = prop;
		break;

	case ICAL_PRIORITY_PROPERTY:
		priv->priority = prop;
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

	default:
		break;
	}
}

/* Scans an icalcomponent for its properties so that we can provide
 * random-access to them.
 */
static void
scan_icalcomponent (CalComponent *comp)
{
	CalComponentPrivate *priv;
	icalproperty *prop;

	priv = comp->priv;

	g_assert (priv->icalcomp != NULL);

	for (prop = icalcomponent_get_first_property (priv->icalcomp, ICAL_ANY_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (priv->icalcomp, ICAL_ANY_PROPERTY))
		scan_property (comp, prop);

	/* We don't scan for alarm subcomponents since they can be iterated
	 * through using cal_component_get_{first,next}_alarm().
	 */
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
		time_t tim;
		struct icaltimetype t;

		tim = time (NULL);
		t = icaltimetype_from_timet (tim, FALSE);

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

	free_icalcomponent (comp);

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

	free_icalcomponent (comp);

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

	if (!priv->categories_list) {
		*categ_list = NULL;
		return;
	}

	categories = icalproperty_get_categories (priv->categories_list);
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
	struct categories *cat;
	char *categories_str;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	/* Free the old list */

	if (!categ_list) {
		if (priv->categories_list) {
			GSList *l;

			for (l = priv->categories_list; l; l = l->next) {
				struct categories *c;

				c = l->data;
				icalcomponent_remove_property (priv->icalcomp, c->prop);
				icalproperty_free (c->prop);

				g_free (c);
			}

			g_slist_free (priv->categories_list);
			priv->categories_list = NULL;
		}

		return;
	}

	/* Create a single string of categories */

	categories_str = stringify_categories (categ_list);

	/* Set the categories */

	cat = g_new (struct categories, 1);
	cat->prop = icalproperty_new_categories (categories_str);
	g_free (categories_str);

	icalcomponent_add_property (priv->icalcomp, cat->prop);
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
	const char *class;

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

	if (strcasecmp (class, "PUBLIC") == 0)
		*classif = CAL_COMPONENT_CLASS_PUBLIC;
	else if (strcasecmp (class, "PRIVATE") == 0)
		*classif = CAL_COMPONENT_CLASS_PRIVATE;
	else if (strcasecmp (class, "CONFIDENTIAL") == 0)
		*classif = CAL_COMPONENT_CLASS_CONFIDENTIAL;
	else
		*classif = CAL_COMPONENT_CLASS_UNKNOWN;
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
	char *str;

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
		str = "PUBLIC";
		break;

	case CAL_COMPONENT_CLASS_PRIVATE:
		str = "PRIVATE";
		break;

	case CAL_COMPONENT_CLASS_CONFIDENTIAL:
		str = "CONFIDENTIAL";
		break;

	default:
		g_assert_not_reached ();
		str = NULL;
	}

	if (priv->classification)
		icalproperty_set_class (priv->classification, str);
	else {
		priv->classification = icalproperty_new_class (str);
		icalcomponent_add_property (priv->icalcomp, priv->classification);
	}
}

/* Gets a text list value */
static void
get_text_list (GSList *text_list,
	       char *(* get_prop_func) (icalproperty *prop),
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
	       icalproperty *(* new_prop_func) (char *value),
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

/* Gets a struct icaltimetype value */
static void
get_icaltimetype (icalproperty *prop,
		  struct icaltimetype (* get_prop_func) (icalproperty *prop),
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
	      struct icaltimetype (* get_prop_func) (icalproperty *prop),
	      CalComponentDateTime *dt)
{
	if (datetime->prop) {
		dt->value = g_new (struct icaltimetype, 1);
		*dt->value = (* get_prop_func) (datetime->prop);
	} else
		dt->value = NULL;

	if (datetime->tzid_param)
		dt->tzid = icalparameter_get_tzid (datetime->tzid_param);
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

	if (datetime->prop)
		(* prop_set_func) (datetime->prop, *dt->value);
	else {
		datetime->prop = (* prop_new_func) (*dt->value);
		icalcomponent_add_property (priv->icalcomp, datetime->prop);
	}

	if (dt->tzid) {
		g_assert (datetime->prop != NULL);

		if (datetime->tzid_param)
			icalparameter_set_tzid (datetime->tzid_param, (char *) dt->tzid);
		else {
			datetime->tzid_param = icalparameter_new_tzid ((char *) dt->tzid);
			icalproperty_add_parameter (datetime->prop, datetime->tzid_param);
		}
	} else if (datetime->tzid_param) {
#if 0
		/* FIXME: this fucking routine will assert(0) since it is not implemented */
		icalproperty_remove_parameter (datetime->prop, ICAL_TZID_PARAMETER);
		icalparameter_free (datetime->tzid_param);
#endif
		datetime->tzid_param = NULL;
	}
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

	priv->need_sequence_inc = TRUE;
}

/**
 * cal_component_get_dtstamp:
 * @comp: A calendar component object.
 * @t: Return value for the date/timestamp.
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

	priv->need_sequence_inc = TRUE;
}

/* Builds a list of CalComponentPeriod structures based on a list of icalproperties */
static void
get_period_list (GSList *period_list,
		 struct icalperiodtype (* get_prop_func) (icalproperty *prop),
		 GSList **list)
{
	GSList *l;

	*list = NULL;

	if (!period_list)
		return;

	for (l = period_list; l; l = l->next) {
		struct period *period;
		CalComponentPeriod *p;
		struct icalperiodtype ip;

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

		p->start = ip.start;

		if (p->type == CAL_COMPONENT_PERIOD_DATETIME)
			p->u.end = ip.end;
		else if (p->type == CAL_COMPONENT_PERIOD_DURATION)
			p->u.duration = ip.duration;
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
		 icalproperty *(* new_prop_func) (struct icalperiodtype period),
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
		struct icalperiodtype ip;
		icalparameter_value value_type;

		g_assert (l->data != NULL);
		p = l->data;

		/* Create libical value */

		ip.start = p->start;

		if (p->type == CAL_COMPONENT_PERIOD_DATETIME) {
			value_type = ICAL_VALUE_DATETIME;
			ip.end = p->u.end;
		} else if (p->type == CAL_COMPONENT_PERIOD_DURATION) {
			value_type = ICAL_VALUE_DURATION;
			ip.duration = p->u.duration;
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
 * struct #icaltimetype structures.  This should be freed using the
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
		icalproperty *prop;
		struct icaltimetype *t;

		prop = l->data;

		t = g_new (struct icaltimetype, 1);
		*t = icalproperty_get_exdate (prop);

		*exdate_list = g_slist_prepend (*exdate_list, t);
	}

	*exdate_list = g_slist_reverse (*exdate_list);
}

/**
 * cal_component_set_exdate_list:
 * @comp: A calendar component object.
 * @exdate_list: List of struct #icaltimetype structures.
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
		icalproperty *prop;

		prop = l->data;
		icalcomponent_remove_property (priv->icalcomp, prop);
		icalproperty_free (prop);
	}

	g_slist_free (priv->exdate_list);
	priv->exdate_list = NULL;

	/* Add in new exception dates */

	for (l = exdate_list; l; l = l->next) {
		icalproperty *prop;
		struct icaltimetype *t;

		g_assert (l->data != NULL);
		t = l->data;

		prop = icalproperty_new_exdate (*t);
		icalcomponent_add_property (priv->icalcomp, prop);

		priv->exdate_list = g_slist_prepend (priv->exdate_list, prop);
	}

	priv->exdate_list = g_slist_reverse (priv->exdate_list);

	priv->need_sequence_inc = TRUE;
}

/* Gets a list of recurrence rules */
static void
get_recur_list (GSList *recur_list,
		struct icalrecurrencetype (* get_prop_func) (icalproperty *prop),
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
	g_return_if_fail (period_list != NULL);

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
#if 0
		/* FIXME: this fucking routine will assert(0) since it is not implemented */
		icalproperty_remove_parameter (priv->summary.prop, ICAL_ALTREP_PARAMETER);
		icalparameter_free (priv->summary.altrep_param);
#endif
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
	const char *val;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (transp != NULL);

	priv = comp->priv;
	g_return_if_fail (priv->icalcomp != NULL);

	if (!priv->transparency) {
		*transp = CAL_COMPONENT_TRANSP_NONE;
		return;
	}

	val = icalproperty_get_transp (priv->transparency);

	if (strcasecmp (val, "TRANSPARENT"))
		*transp = CAL_COMPONENT_TRANSP_TRANSPARENT;
	else if (strcasecmp (val, "OPAQUE"))
		*transp = CAL_COMPONENT_TRANSP_OPAQUE;
	else
		*transp = CAL_COMPONENT_TRANSP_UNKNOWN;
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
	char *str;

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
		str = "TRANSPARENT";
		break;

	case CAL_COMPONENT_TRANSP_OPAQUE:
		str = "OPAQUE";
		break;

	default:
		g_assert_not_reached ();
		str = NULL;
	}

	if (priv->transparency)
		icalproperty_set_transp (priv->transparency, str);
	else {
		priv->transparency = icalproperty_new_transp (str);
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

	if (!url) {
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

/**
 * cal_component_get_pilot_id:
 * @comp: A calendar component object.
 * @pilot_id: Return value for the pilot ID.
 *
 * Queries the pilot id of a calendar component object, if any.
 **/
void
cal_component_get_pilot_id (CalComponent *comp, unsigned long *pilot_id)
{
	/* pilot_id maybe should be recordid_t */
	/* FIX ME */
	*pilot_id = 0;
}

/**
 * cal_component_set_pilot_id:
 * @comp: A calendar component object.
 * @pilot_id: Return value for the pilot ID.
 *
 * Sets the pilot id of a clanedar component object.
 **/
void
cal_component_set_pilot_id (CalComponent *comp, unsigned long pilot_id)
{
	/* pilot_id maybe should be recordid_t */
	/* FIX ME */
}

/**
 * cal_component_get_pilot_status:
 * @comp: A calendar component object.
 * @pilot_status: Return value for the pilot sync status.
 *
 * Queries the pilot status of a calendar component object, if any.
 **/
void
cal_component_get_pilot_status (CalComponent *comp, unsigned long *pilot_status)
{
	/* FIX ME */
	/* pilot_status should be iCalPilotState ? */
	*pilot_status = 0;
}

/**
 * cal_component_set_pilot_id:
 * @comp: A calendar component object.
 * @pilot_id: Return value for the pilot ID.
 *
 * Sets the pilot id of a clanedar component object.
 **/
void
cal_component_set_pilot_status (CalComponent *comp, unsigned long pilot_status)
{
	/* pilot_id maybe should be recordid_t */
	/* FIX ME */
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

	if (dt->value)
		g_free (dt->value);
}

/**
 * cal_component_free_exdate_list:
 * @exdate_list: List of struct #icaltimetype structures.
 * 
 * Frees a list of struct #icaltimetype structures.
 **/
void
cal_component_free_exdate_list (GSList *exdate_list)
{
	GSList *l;

	for (l = exdate_list; l; l = l->next) {
		struct icaltimetype *t;

		g_assert (l->data != NULL);
		t = l->data;

		g_free (t);
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

		g_free (l);
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



/* Scans an icalproperty from a calendar component and adds its mapping to our
 * own alarm structure.
 */
static void
scan_alarm_property (CalComponentAlarm *alarm, icalproperty *prop)
{
	icalproperty_kind kind;

	kind = icalproperty_isa (prop);

	switch (kind) {
	case ICAL_ACTION_PROPERTY:
		alarm->action = prop;
		break;

	case ICAL_TRIGGER_PROPERTY:
		alarm->trigger = prop;
		break;

	default:
		break;
	}
}

/* Creates a CalComponentAlarm from a libical alarm subcomponent */
static CalComponentAlarm *
make_alarm (CalComponent *comp, icalcomponent *subcomp)
{
	CalComponentAlarm *alarm;
	icalproperty *prop;

	alarm = g_new (CalComponentAlarm, 1);

	alarm->parent = comp;
	alarm->icalcomp = subcomp;

	for (prop = icalcomponent_get_first_property (subcomp, ICAL_ANY_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (subcomp, ICAL_ANY_PROPERTY))
		scan_alarm_property (alarm, prop);

	return alarm;
}

/**
 * cal_component_get_first_alarm:
 * @comp: A calendar component object.
 *
 * Starts an iterator for the alarms in a calendar component object.  Subsequent
 * alarms can be obtained with the cal_component_get_next_alarm() function.
 *
 * Return value: The first alarm in the component, or NULL if the component has
 * no alarms.  This should be freed using the cal_component_alarm_free()
 * function.
 **/
CalComponentAlarm *
cal_component_get_first_alarm (CalComponent *comp)
{
	CalComponentPrivate *priv;
	icalcomponent *subcomp;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), NULL);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	subcomp = icalcomponent_get_first_component (priv->icalcomp, ICAL_VALARM_COMPONENT);
	if (!subcomp)
		return NULL;

	return make_alarm (comp, subcomp);
}

/**
 * cal_component_get_next_alarm:
 * @comp: A calendar component object.
 * 
 * Gets the next alarm on a calendar component object.  This should be used as
 * an iterator function after calling cal_component_get_first_alarm().
 * 
 * Return value: The next alarm in the component, or NULL if the component has
 * no more alarms.  This should be freed using the cal_component_alarm_free()
 * function.
 **/
CalComponentAlarm *
cal_component_get_next_alarm (CalComponent *comp)
{
	CalComponentPrivate *priv;
	icalcomponent *subcomp;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), NULL);

	priv = comp->priv;
	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

	subcomp = icalcomponent_get_next_component (priv->icalcomp, ICAL_VALARM_COMPONENT);
	if (!subcomp)
		return NULL;

	return make_alarm (comp, subcomp);
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
	g_return_if_fail (alarm != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (icalcomponent_get_parent (alarm->icalcomp) != NULL)
		icalcomponent_free (alarm->icalcomp);

	alarm->icalcomp = NULL;

	alarm->parent = NULL;
	alarm->action = NULL;

	g_free (alarm);
}

/**
 * cal_component_alarm_get_action:
 * @alarm: An alarm.
 * @action: Return value for the alarm's action type.
 *
 * Queries the action type of an alarm.
 **/
void
cal_component_alarm_get_action (CalComponentAlarm *alarm, CalComponentAlarmAction *action)
{
	const char *str;

	g_return_if_fail (alarm != NULL);
	g_return_if_fail (action != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (!alarm->action) {
		*action = CAL_COMPONENT_ALARM_NONE;
		return;
	}

	str = icalproperty_get_action (alarm->action);

	if (strcasecmp (str, "AUDIO") == 0)
		*action = CAL_COMPONENT_ALARM_AUDIO;
	else if (strcasecmp (str, "DISPLAY") == 0)
		*action = CAL_COMPONENT_ALARM_DISPLAY;
	else if (strcasecmp (str, "EMAIL") == 0)
		*action = CAL_COMPONENT_ALARM_EMAIL;
	else if (strcasecmp (str, "PROCEDURE") == 0)
		*action = CAL_COMPONENT_ALARM_PROCEDURE;
	else
		*action = CAL_COMPONENT_ALARM_UNKNOWN;
}

/**
 * cal_component_alarm_set_action:
 * @alarm: An alarm.
 * @action: Action type.
 *
 * Sets the action type for an alarm.
 **/
void
cal_component_alarm_set_action (CalComponentAlarm *alarm, CalComponentAlarmAction action)
{
	char *str;

	g_return_if_fail (alarm != NULL);
	g_return_if_fail (action != CAL_COMPONENT_ALARM_NONE);
	g_return_if_fail (action != CAL_COMPONENT_ALARM_UNKNOWN);

	g_assert (alarm->icalcomp != NULL);

	switch (action) {
	case CAL_COMPONENT_ALARM_AUDIO:
		str = "AUDIO";
		break;

	case CAL_COMPONENT_ALARM_DISPLAY:
		str = "DISPLAY";
		break;

	case CAL_COMPONENT_ALARM_EMAIL:
		str = "EMAIL";
		break;

	case CAL_COMPONENT_ALARM_PROCEDURE:
		str = "PROCEDURE";
		break;

	default:
		g_assert_not_reached ();
		str = NULL;
	}

	if (alarm->action)
		icalproperty_set_action (alarm->action, str);
	else {
		alarm->action = icalproperty_new_action (str);
		icalcomponent_add_property (alarm->icalcomp, alarm->action);
	}
}

/**
 * cal_component_alarm_get_trigger:
 * @alarm: An alarm.
 * @trigger: Return value for the trigger time.  This should be freed using the
 * cal_component_alarm_free_trigger() function.
 *
 * Queries the trigger time for an alarm.
 **/
void
cal_component_alarm_get_trigger (CalComponentAlarm *alarm, CalComponentAlarmTrigger **trigger)
{
	icalparameter *param;
	union icaltriggertype t;

	g_return_if_fail (alarm != NULL);
	g_return_if_fail (trigger != NULL);

	g_assert (alarm->icalcomp != NULL);

	if (!alarm->trigger) {
		*trigger = NULL;
		return;
	}

	*trigger = g_new (CalComponentAlarmTrigger, 1);

	/* Get trigger type */

	param = icalproperty_get_first_parameter (alarm->trigger, ICAL_VALUE_PARAMETER);

	if (param) {
		icalparameter_value value;

		value = icalparameter_get_value (param);

		switch (value) {
		case ICAL_VALUE_DURATION:
			(*trigger)->type = CAL_COMPONENT_ALARM_TRIGGER_RELATIVE;
			break;

		case ICAL_VALUE_DATETIME:
			(*trigger)->type = CAL_COMPONENT_ALARM_TRIGGER_ABSOLUTE;
			break;

		default:
			g_message ("cal_component_alarm_get_trigger(): Unknown value for trigger "
				   "value %d; using RELATIVE", value);

			(*trigger)->type = CAL_COMPONENT_ALARM_TRIGGER_RELATIVE;
			break;
		}
	} else
		(*trigger)->type = CAL_COMPONENT_ALARM_TRIGGER_RELATIVE;

	/* Get trigger value and the RELATED parameter */

	t = icalproperty_get_trigger (alarm->trigger);

	switch ((*trigger)->type) {
	case CAL_COMPONENT_ALARM_TRIGGER_RELATIVE:
		(*trigger)->u.relative.duration = t.duration;

		param = icalproperty_get_first_parameter (alarm->trigger, ICAL_RELATED_PARAMETER);
		if (param) {
			icalparameter_related rel;

			rel = icalparameter_get_related (param);

			switch (rel) {
			case ICAL_RELATED_START:
				(*trigger)->u.relative.related =
					CAL_COMPONENT_ALARM_TRIGGER_RELATED_START;
				break;

			case ICAL_RELATED_END:
				(*trigger)->u.relative.related =
					CAL_COMPONENT_ALARM_TRIGGER_RELATED_END;
				break;

			default:
				g_assert_not_reached ();
			}
		} else
			(*trigger)->u.relative.related = CAL_COMPONENT_ALARM_TRIGGER_RELATED_START;

		break;

	case CAL_COMPONENT_ALARM_TRIGGER_ABSOLUTE:
		(*trigger)->u.absolute = t.time;
		break;

	default:
		g_assert_not_reached ();
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
cal_component_alarm_set_trigger (CalComponentAlarm *alarm, CalComponentAlarmTrigger *trigger)
{
	union icaltriggertype t;
	icalparameter *param;
	icalparameter_value value_type;
	icalparameter_related related;

	g_return_if_fail (alarm != NULL);
	g_return_if_fail (trigger != NULL);

	g_assert (alarm->icalcomp != NULL);

	/* Delete old trigger */

	if (alarm->trigger) {
		icalcomponent_remove_property (alarm->icalcomp, alarm->trigger);
		icalproperty_free (alarm->trigger);
		alarm->trigger = NULL;
	}

	/* Set the value */

	value_type = ICAL_DURATION_VALUE; /* Keep GCC happy */
	related = ICAL_RELATED_START; /* Ditto */

	switch (trigger->type) {
	case CAL_COMPONENT_ALARM_TRIGGER_RELATIVE:
		t.duration = trigger->u.relative.duration;
		value_type = ICAL_DURATION_VALUE;

		switch (trigger->u.relative.related) {
		case CAL_COMPONENT_ALARM_TRIGGER_RELATED_START:
			related = ICAL_RELATED_START;
			break;

		case CAL_COMPONENT_ALARM_TRIGGER_RELATED_END:
			related = ICAL_RELATED_END;
			break;

		default:
			g_assert_not_reached ();
			return;
		}

		break;

	case CAL_COMPONENT_ALARM_TRIGGER_ABSOLUTE:
		t.time = trigger->u.absolute;
		value_type = ICAL_DATETIME_VALUE;
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

	if (trigger->type == CAL_COMPONENT_ALARM_TRIGGER_RELATIVE) {
		param = icalproperty_get_first_parameter (alarm->trigger, ICAL_RELATED_PARAMETER);

		if (param)
			icalparameter_set_related (param, related);
		else {
			param = icalparameter_new_related (related);
			icalproperty_add_parameter (alarm->trigger, param);
		}
	}
}

/**
 * cal_component_alarm_free_trigger:
 * @trigger: A #CalComponentAlarmTrigger structure.
 *
 * Frees a #CalComponentAlarmTrigger structure.
 **/
void
cal_component_alarm_free_trigger (CalComponentAlarmTrigger *trigger)
{
	g_return_if_fail (trigger != NULL);

	g_free (trigger);
}
