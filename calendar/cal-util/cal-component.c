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
	/* Type of this component */
	CalComponentVType type;

	/* Summary string, optional */
	char *summary;

	/* Unique identifier, MUST be present */
	char *uid;
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

	priv->uid = cal_component_gen_uid ();
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

	if (priv->uid) {
		g_free (priv->uid);
		priv->uid = NULL;
	}

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
 * Creates a new empty calendar component object whose only set field is the
 * unique identifier.  You should set the type of this component as soon as
 * possible by using cal_component_set_vtype().
 * 
 * Return value: A newly-created calendar component object.
 **/
CalComponent *
cal_component_new (void)
{
	return CAL_COMPONENT (gtk_type_new (CAL_COMPONENT_TYPE));
}

/* Parses a property and stores it in the calendar component */
static void
load_property (CalComponent *comp, icalproperty *prop)
{
	icalproperty_kind kind;

	kind = icalproperty_isa (prop);

	/* FIXME */
}

/**
 * cal_component_new_from_icalcomponent:
 * @ical: An #icalcomponent structure with the component to parse.
 * 
 * Creates a new calendar component object from an #icalcomponent from libical.
 * This function only deals with VEVENT, VTODO, VJOURNAL, VFREEBUSY, AND
 * VTIMEZONE components.
 * 
 * Return value: A newly-created calendar component object.
 **/
CalComponent *
cal_component_new_from_icalcomponent (icalcomponent *ical)
{
	CalComponent *comp;
	icalcomponent_kind kind;
	CalComponentVType type;
	icalproperty *prop;

	g_return_val_if_fail (ical != NULL, NULL);

	kind = icalcomponent_isa (ical);

	switch (kind) {
	case ICAL_VEVENT_COMPONENT:
		type = CAL_COMPONENT_EVENT;
		break;

	case ICAL_VTODO_COMPONENT:
		type = CAL_COMPONENT_TODO;
		break;

	case ICAL_VJOURNAL_COMPONENT:
		type = CAL_COMPONENT_JOURNAL;
		break;

	case ICAL_VFREEBUSY_COMPONENT:
		type = CAL_COMPONENT_FREEBUSY;
		break;

	case ICAL_VTIMEZONE_COMPONENT:
		type = CAL_COMPONENT_TIMEZONE;
		break;

	default:
		g_message ("cal_component_new_from_icalcomponent(): Unsupported component type %d",
			   kind);
		return NULL;
	}

	comp = cal_component_new ();
	cal_component_set_vtype (comp, type);

	for (prop = icalcomponent_get_first_property (ical, ICAL_ANY_PROPERTY);
	     prop;
	     prop = icalcomponent_get_next_property (ical, ICAL_ANY_PROPERTY))
		load_property (comp, prop);

	/* FIXME: parse ALARM subcomponents */

	return comp;
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

	g_return_val_if_fail (comp != NULL, CAL_COMPONENT_NO_TYPE);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), CAL_COMPONENT_NO_TYPE);

	priv = comp->priv;
	return priv->type;
}

/**
 * cal_component_set_vtype:
 * @comp: A calendar component object.
 * @type: Type of the component, as defined by RFC 2445.
 * 
 * Sets the type of a calendar component object.  This function should be used
 * as soon as possible after creating a new calendar component so that its type
 * can be known to the rest of the program.
 **/
void
cal_component_set_vtype (CalComponent *comp, CalComponentVType type)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));
	g_return_if_fail (type != CAL_COMPONENT_NO_TYPE);

	priv = comp->priv;
	priv->type = type;
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
	return priv->uid;
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
	g_assert (priv->uid != NULL);

	g_free (priv->uid);
	priv->uid = g_strdup (uid);
}

/**
 * cal_component_get_summary:
 * @comp: A calendar component object.
 * 
 * Queries the summary of a calendar component object.
 * 
 * Return value: Summary string.
 **/
const char *
cal_component_get_summary (CalComponent *comp)
{
	CalComponentPrivate *priv;

	g_return_val_if_fail (comp != NULL, NULL);
	g_return_val_if_fail (IS_CAL_COMPONENT (comp), NULL);

	priv = comp->priv;
	return priv->summary;
}

/**
 * cal_component_set_summary:
 * @comp: A calendar component object.
 * @summary: Summary string.
 * 
 * Sets the summary of a calendar component object.
 **/
void
cal_component_set_summary (CalComponent *comp, const char *summary)
{
	CalComponentPrivate *priv;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	priv = comp->priv;

	if (priv->summary)
		g_free (priv->summary);

	priv->summary = g_strdup (summary);
}
