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
#include <unistd.h>
#include "cal-component.h"
#include "timeutil.h"



/* Private part of the CalComponent structure */
typedef struct {
	/* The icalcomponent we wrap */
	icalcomponent *icalcomp;

	/* Properties */

	icalproperty *uid_prop;

	struct {
		icalproperty *prop;
		icalparameter *altrep_param;
	} summary;
} CalComponentPrivate;



static void cal_component_class_init (CalComponentClass *class);
static void cal_component_init (CalComponent *comp);
static void cal_component_destroy (GtkObject *object);

static GtkObjectClass *parent_class;



/**
 * cal_component_get_type:
 * @void: 
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

	priv->uid_prop = cal_component_gen_uid ();
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

	/* FIXME: remove the mappings! */

	if (icalcomponent_get_parent (priv->icalcomp) != NULL)
		icalcomponent_free (priv->icalcomp);

	priv->icalcomp = NULL;
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
 * @void: 
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
 * @void:
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

/* Scans the summary property */
static void
scan_summary (CalComponent *comp, icalproperty *prop)
{
	CalComponentPrivate *priv;
	icalparameter *param;

	priv = comp->priv;

	priv->summary.prop = prop;

	for (param = icalproperty_get_first_parameter (prop, ICAL_ANY_PARAMETER);
	     param;
	     param = icalproperty_get_next_parameter (prop, ICAL_ANY_PARAMETER)) {
		icalparameter_kind kind;
		
		kind = icalparameter_isa (param);
		
		switch (kind) {
		case ICAL_ALTREP_PARAMETER:
			priv->summary.altrep_param = param;
			break;

		default:
			break;
		}
	}
}

/* Scans an icalproperty and adds its mapping to the component */
static void
scan_property (CalComponent *comp, icalproperty *prop)
{
	CalComponentPrivate *priv;
	icalproperty_kind kind;

	priv = comp->priv;

	switch (kind) {
	case ICAL_SUMMARY_PROPERTY:
		scan_summary (comp, prop);

	case ICAL_UID_PROPERTY:
		priv->uid_prop = prop;

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

	/* FIXME: parse ALARM subcomponents */
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
	char *uid;
	icalproperty *prop;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;

	free_icalcomponent (comp);

	if (type == CAL_COMPONENT_NO_TYPE)
		return;

	/* Figure out the kind */

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

	/* Create an UID */

	icalcomp = icalcomponent_new (kind);
	if (!icalcomp) {
		g_message ("cal_component_set_new_vtype(): Could not create the icalcomponent!");
		return;
	}

	uid = cal_component_gen_uid ();
	prop = icalproperty_new_uid (uid);
	g_free (uid);

	if (!prop) {
		icalcomponent_free (icalcomp);
		g_message ("cal_component_set_new_vtype(): Could not create the UID property!");
		return;
	}

	icalcomponent_add_property (icalcomp, prop);

	/* Scan the component to build our mapping table */

	priv->icalcomp = icalcomp;
	scan_icalcomponent (comp);
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
 **/
void
cal_component_set_icalcomponent (CalComponent *comp, icalcomponent *icalcomp)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;

	if (priv->icalcomp == icalcomp)
		return;

	free_icalcomponent (comp);

	priv->icalcomp = icalcomp;

	if (priv->icalcomp)
		scan_icalcomponent (comp);
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

	if (!priv->icalcomp)
		return CAL_COMPONENT_NO_TYPE;

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
 * cal_component_get_uid:
 * @comp: A calendar component object.
 * 
 * Queries the unique identifier of a calendar component object.
 * 
 * Return value: The unique identifier string.
 **/
const char *
cal_component_get_uid (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), NULL);

	priv = comp->priv;

	/* This MUST exist, since we ensured that it did */
	g_assert (priv->uid_prop != NULL);

	return icalproperty_get_uid (priv->uid_prop);
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

	/* This MUST exist, since we ensured that it did */
	g_assert (priv->uid_prop != NULL);

	icalproperty_set_uid (priv->uid_prop, (char *) uid);
}

/**
 * cal_component_get_summary:
 * @comp: A calendar component object.
 * @summary: Return value for the summary string.
 * @altrep: Return value for the alternate representation string.
 * 
 * Queries the summary of a calendar component object.
 **/
void
cal_component_get_summary (CalComponent *comp, const char **summary, const char **altrep)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;

	if (summary) {
		if (priv->summary.prop)
			*summary = icalproperty_get_summary (priv->summary.prop);
		else
			*summary = NULL;
	}

	if (altrep) {
		if (priv->summary.altrep_param)
			*altrep = icalparameter_get_altrep (priv->summary.altrep_param);
		else
			*altrep = NULL;
	}
}

/**
 * cal_component_set_summary:
 * @comp: A calendar component object.
 * @summary: Summary string.
 * 
 * Sets the summary of a calendar component object.
 **/
void
cal_component_set_summary (CalComponent *comp, const char *summary, const char *altrep)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;

	g_return_if_fail (priv->icalcomp != NULL);

	if (!summary)
		g_return_if_fail (altrep == NULL);

	if (summary) {
		if (priv->summary.prop)
			icalproperty_set_summary (priv->summary.prop, (char *) summary);
		else {
			priv->summary.prop = icalproperty_new_summary ((char *) summary);
			icalcomponent_add_property (priv->icalcomp, priv->summary.prop);
		}
	} else if (priv->summary.prop) {
		icalcomponent_remove_property (priv->icalcomp, priv->summary.prop);
		icalproperty_free (priv->summary.prop);

		priv->summary.prop = NULL;
		priv->summary.altrep_param = NULL;
	}

	if (altrep) {
		g_assert (priv->summary.prop != NULL);

		if (priv->summary.altrep_param)
			icalparameter_set_altrep (priv->summary.altrep_param, (char *) altrep);
		else {
			priv->summary.altrep_param = icalparameter_new_altrep ((char *) altrep);
			icalproperty_add_parameter (priv->summary.prop, priv->summary.altrep_param);
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
