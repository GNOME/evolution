/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-source.c
 *
 * Copyright (C) 2003  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>

#include "e-source.h"

#include "e-util-marshal.h"
#include "e-uid.h"

#include <string.h>
#include <gal/util/e-util.h>


#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

#define ES_CLASS(obj)  E_SOURCE_CLASS (G_OBJECT_GET_CLASS (obj))


/* String used to put the color in the XML.  */
#define COLOR_FORMAT_STRING "%06x"


/* Private members.  */

struct _ESourcePrivate {
	ESourceGroup *group;

	char *uid;
	char *name;
	char *relative_uri;

	gboolean has_color;
	guint32 color;
};


/* Signals.  */

enum {
	CHANGED,
	LAST_SIGNAL
};
static unsigned int signals[LAST_SIGNAL] = { 0 };


/* Callbacks.  */

static void
group_weak_notify (ESource *source,
		   GObject **where_the_object_was)
{
	source->priv->group = NULL;

	g_signal_emit (source, signals[CHANGED], 0);
}


/* GObject methods.  */

static void
impl_finalize (GObject *object)
{
	ESourcePrivate *priv = E_SOURCE (object)->priv;

	g_free (priv->uid);
	g_free (priv->name);
	g_free (priv->relative_uri);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
impl_dispose (GObject *object)
{
	ESourcePrivate *priv = E_SOURCE (object)->priv;

	if (priv->group != NULL) {
		g_object_weak_unref (G_OBJECT (priv->group), (GWeakNotify) group_weak_notify, object);
		priv->group = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}


/* Initialization.  */

static void
class_init (ESourceClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);

	signals[CHANGED] = 
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ESourceClass, changed),
			      NULL, NULL,
			      e_util_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
init (ESource *source)
{
	ESourcePrivate *priv;

	priv = g_new0 (ESourcePrivate, 1);
	source->priv = priv;
}


/* Public methods.  */

ESource *
e_source_new  (const char *name,
	       const char *relative_uri)
{
	ESource *source;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (relative_uri != NULL, NULL);

	source = g_object_new (e_source_get_type (), NULL);
	source->priv->uid = e_uid_new ();

	e_source_set_name (source, name);
	e_source_set_relative_uri (source, relative_uri);
	return source;
}

ESource *
e_source_new_from_xml_node (xmlNodePtr node)
{
	ESource *source;
	xmlChar *uid;

	uid = xmlGetProp (node, "uid");
	if (uid == NULL)
		return NULL;

	source = g_object_new (e_source_get_type (), NULL);

	source->priv->uid = g_strdup (uid);
	xmlFree (uid);

	if (e_source_update_from_xml_node (source, node, NULL))
		return source;

	g_object_unref (source);
	return NULL;
}

/**
 * e_source_update_from_xml_node:
 * @source: An ESource.
 * @node: A pointer to the node to parse.
 * 
 * Update the ESource properties from @node.
 * 
 * Return value: %TRUE if the data in @node was recognized and parsed into
 * acceptable values for @source, %FALSE otherwise.
 **/
gboolean
e_source_update_from_xml_node (ESource *source,
			       xmlNodePtr node,
			       gboolean *changed_return)
{
	xmlChar *name;
	xmlChar *relative_uri;
	xmlChar *color_string;
	gboolean retval;
	gboolean changed = FALSE;

	name = xmlGetProp (node, "name");
	relative_uri = xmlGetProp (node, "relative_uri");
	color_string = xmlGetProp (node, "color");

	if (name == NULL || relative_uri == NULL) {
		retval = FALSE;
		goto done;
	}

	if (source->priv->name == NULL
	    || strcmp (name, source->priv->name) != 0
	    || source->priv->relative_uri == NULL
	    || strcmp (relative_uri, source->priv->relative_uri) != 0) {
		g_free (source->priv->name);
		source->priv->name = g_strdup (name);

		g_free (source->priv->relative_uri);
		source->priv->relative_uri = g_strdup (relative_uri);

		changed = TRUE;
	}

	if (color_string == NULL) {
		if (source->priv->has_color) {
			source->priv->has_color = FALSE;
			changed = TRUE;
		}
	} else {
		guint32 color = 0;

		sscanf (color_string, COLOR_FORMAT_STRING, &color);
		if (! source->priv->has_color || source->priv->color != color) {
			source->priv->has_color = TRUE;
			source->priv->color = color;
			changed = TRUE;
		}
	}

	retval = TRUE;

 done:
	if (changed)
		g_signal_emit (source, signals[CHANGED], 0);

	if (changed_return != NULL)
		*changed_return = changed;

	if (name != NULL)
		xmlFree (name);
	if (relative_uri != NULL)
		xmlFree (relative_uri);
	if (color_string != NULL)
		xmlFree (color_string);

	return retval;
}

/**
 * e_source_name_from_xml_node:
 * @node: A pointer to an XML node.
 * 
 * Assuming that @node is a valid ESource specification, retrieve the name of
 * the source from it.
 * 
 * Return value: Name of the source in the specified @node.  The caller must
 * free the string.
 **/
char *
e_source_uid_from_xml_node (xmlNodePtr node)
{
	xmlChar *uid = xmlGetProp (node, "uid");
	char *retval;

	if (uid == NULL)
		return NULL;

	retval = g_strdup (uid);
	xmlFree (uid);
	return retval;
}

void
e_source_set_group (ESource *source,
		    ESourceGroup *group)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (group == NULL || E_IS_SOURCE_GROUP (group));

	if (source->priv->group == group)
		return;

	if (source->priv->group != NULL)
		g_object_weak_unref (G_OBJECT (source->priv->group), (GWeakNotify) group_weak_notify, source);

	source->priv->group = group;
	if (group != NULL)
		g_object_weak_ref (G_OBJECT (group), (GWeakNotify) group_weak_notify, source);

	g_signal_emit (source, signals[CHANGED], 0);
}

void
e_source_set_name (ESource *source,
		   const char *name)
{
	g_return_if_fail (E_IS_SOURCE (source));

	if (source->priv->name == name)
		return;

	g_free (source->priv->name);
	source->priv->name = g_strdup (name);

	g_signal_emit (source, signals[CHANGED], 0);
}

void
e_source_set_relative_uri (ESource *source,
			   const char *relative_uri)
{
	g_return_if_fail (E_IS_SOURCE (source));

	if (source->priv->relative_uri == relative_uri)
		return;

	g_free (source->priv->relative_uri);
	source->priv->relative_uri = g_strdup (relative_uri);

	g_signal_emit (source, signals[CHANGED], 0);
}

void
e_source_set_color (ESource *source,
		    guint32 color)
{
	g_return_if_fail (E_IS_SOURCE (source));

	if (source->priv->has_color && source->priv->color == color)
		return;

	source->priv->has_color = TRUE;
	source->priv->color = color;

	g_signal_emit (source, signals[CHANGED], 0);
}

void
e_source_unset_color (ESource *source)
{
	g_return_if_fail (E_IS_SOURCE (source));

	if (! source->priv->has_color)
		return;

	source->priv->has_color = FALSE;
	g_signal_emit (source, signals[CHANGED], 0);
}


ESourceGroup *
e_source_peek_group (ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return source->priv->group;
}

const char *
e_source_peek_uid (ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return source->priv->uid;
}

const char *
e_source_peek_name (ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return source->priv->name;
}

const char *
e_source_peek_relative_uri (ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return source->priv->relative_uri;
}

/**
 * e_source_get_color:
 * @source: An ESource
 * @color_return: Pointer to a variable where the returned color will be
 * stored.
 * 
 * If @source has an associated color, return it in *@color_return.
 * 
 * Return value: %TRUE if the @source has a defined color (and hence
 * *@color_return was set), %FALSE otherwise.
 **/
gboolean
e_source_get_color (ESource *source,
		    guint32 *color_return)
{
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	if (! source->priv->has_color)
		return FALSE;

	if (color_return != NULL)
		*color_return = source->priv->color;

	return TRUE;
}


char *
e_source_get_uri (ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	if (source->priv->group == NULL)
		return NULL;

	return g_build_filename (e_source_group_peek_base_uri (source->priv->group),
				 source->priv->relative_uri,
				 NULL);
}


void
e_source_dump_to_xml_node (ESource *source,
			   xmlNodePtr parent_node)
{
	gboolean has_color;
	guint32 color;
	xmlNodePtr node = xmlNewChild (parent_node, NULL, "source", NULL);

	g_return_if_fail (E_IS_SOURCE (source));


	xmlSetProp (node, "uid", e_source_peek_uid (source));
	xmlSetProp (node, "name", e_source_peek_name (source));
	xmlSetProp (node, "relative_uri", e_source_peek_relative_uri (source));

	has_color = e_source_get_color (source, &color);
	if (has_color) {
		char *color_string = g_strdup_printf (COLOR_FORMAT_STRING, color);
		xmlSetProp (node, "color", color_string);
		g_free (color_string);
	}
}


E_MAKE_TYPE (e_source, "ESource", ESource, class_init, init, PARENT_TYPE)
