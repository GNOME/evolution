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

	struct categories {
		icalproperty *prop;
	};
	GSList *categories_list;

	struct text {
		icalproperty *prop;
		icalparameter *altrep_param;
	};

	GSList *comment_list;

	GSList *description_list;

	struct datetime {
		icalproperty *prop;
		icalparameter *tzid_param;
	};

	struct datetime dtstart;
	struct datetime dtend;
	struct datetime due;

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

	priv->uid_prop = NULL;

	priv->categories_list = free_slist (priv->categories_list);

	priv->description_list = free_slist (priv->description_list);

	priv->dtstart.prop = NULL;
	priv->dtstart.tzid_param = NULL;

	priv->dtend.prop = NULL;
	priv->dtend.tzid_param = NULL;

	priv->due.prop = NULL;
	priv->due.tzid_param = NULL;

	priv->summary.prop = NULL;
	priv->summary.altrep_param = NULL;
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

/* Scans a text (i.e. text + altrep) property */
static void
scan_text (CalComponent *comp, GSList **text_list, icalproperty *prop)
{
	CalComponentPrivate *priv;
	struct text *text;
	icalparameter *param;

	priv = comp->priv;

	text = g_new (struct text, 1);
	text->prop = prop;

	for (param = icalproperty_get_first_parameter (prop, ICAL_ANY_PARAMETER);
	     param;
	     param = icalproperty_get_next_parameter (prop, ICAL_ANY_PARAMETER)) {
		icalparameter_kind kind;

		kind = icalparameter_isa (param);

		switch (kind) {
		case ICAL_ALTREP_PARAMETER:
			text->altrep_param = param;
			break;

		default:
			break;
		}
	}

	*text_list = g_slist_append (*text_list, text);
}

/* Scans a date/time and timezone pair property */
static void
scan_datetime (CalComponent *comp, struct datetime *datetime, icalproperty *prop)
{
	CalComponentPrivate *priv;
	icalparameter *param;

	priv = comp->priv;

	datetime->prop = prop;

	for (param = icalproperty_get_first_parameter (prop, ICAL_ANY_PARAMETER);
	     param;
	     param = icalproperty_get_next_parameter (prop, ICAL_ANY_PARAMETER)) {
		icalparameter_kind kind;

		kind = icalparameter_isa (param);

		switch (kind) {
		case ICAL_TZID_PARAMETER:
			datetime->tzid_param = param;
			break;

		default:
			break;
		}
	}
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

	kind = icalproperty_isa (prop);

	switch (kind) {
	case ICAL_CATEGORIES_PROPERTY:
		scan_categories (comp, prop);
		break;

	case ICAL_COMMENT_PROPERTY:
		scan_text (comp, &priv->comment_list, prop);
		break;

	case ICAL_DESCRIPTION_PROPERTY:
		scan_text (comp, &priv->description_list, prop);
		break;

	case ICAL_DTEND_PROPERTY:
		scan_datetime (comp, &priv->dtend, prop);
		break;

	case ICAL_DTSTART_PROPERTY:
		scan_datetime (comp, &priv->dtstart, prop);
		break;

	case ICAL_DUE_PROPERTY:
		scan_datetime (comp, &priv->due, prop);
		break;
		
	case ICAL_SUMMARY_PROPERTY:
		scan_summary (comp, prop);
		break;

	case ICAL_UID_PROPERTY:
		priv->uid_prop = prop;
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
	g_return_val_if_fail (priv->icalcomp != NULL, NULL);

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
	g_return_if_fail (priv->icalcomp != NULL);

	/* This MUST exist, since we ensured that it did */
	g_assert (priv->uid_prop != NULL);

	icalproperty_set_uid (priv->uid_prop, (char *) uid);
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

		text = l->data;
		g_return_if_fail (text != NULL);
		g_free (text);
	}

	g_slist_free (text_list);
}

/* Gets a text list value */
static void
get_text_list (GSList *text_list,
	       char *(* get_prop_func) (icalproperty *prop),
	       GSList **tl)
{
	GSList *l;

	if (!text_list) {
		*tl = NULL;
		return;
	}

	*tl = NULL;

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
}

/**
 * cal_component_get_summary:
 * @comp: A calendar component object.
 * @summary: Return value for the summary property and its parameters.
 * 
 * Queries the summary of a calendar component object.
 **/
void
cal_component_get_summary (CalComponent *comp, CalComponentPropSummary *summary)
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
cal_component_set_summary (CalComponent *comp, const CalComponentPropSummary *summary)
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
