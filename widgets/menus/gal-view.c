/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "gal-view.h"

#include <config.h>
#include <e-util/e-util.h>

#define d(x)

enum {
	PROP_0,
	PROP_TITLE,
	PROP_TYPE_CODE
};

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE (GalView, gal_view, G_TYPE_OBJECT)

static void
view_set_property (GObject *object,
                   guint property_id,
                   const GValue *value,
                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TITLE:
			gal_view_set_title (
				GAL_VIEW (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
view_get_property (GObject *object,
                   guint property_id,
                   GValue *value,
                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_TITLE:
			g_value_set_string (
				value, gal_view_get_title (
				GAL_VIEW (object)));
			return;

		case PROP_TYPE_CODE:
			g_value_set_string (
				value, gal_view_get_type_code (
				GAL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gal_view_class_init (GalViewClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = view_set_property;
	object_class->get_property = view_get_property;

	g_object_class_install_property (
		object_class,
		PROP_TITLE,
		g_param_spec_string (
			"title",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TYPE_CODE,
		g_param_spec_string (
			"type-code",
			NULL,
			NULL,
			NULL,
			G_PARAM_READABLE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GalViewClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
gal_view_init (GalView *view)
{
}

/**
 * gal_view_edit
 * @view: The view to edit
 * @parent: the parent window.
 */
void
gal_view_edit (GalView *view,
               GtkWindow *parent)
{
	GalViewClass *class;

	g_return_if_fail (GAL_IS_VIEW (view));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	class = GAL_VIEW_GET_CLASS (view);
	g_return_if_fail (class->edit != NULL);

	class->edit (view, parent);
}

/**
 * gal_view_load
 * @view: The view to load to
 * @filename: The file to load from
 */
void
gal_view_load (GalView *view,
               const gchar *filename)
{
	GalViewClass *class;

	g_return_if_fail (GAL_IS_VIEW (view));
	g_return_if_fail (filename != NULL);

	class = GAL_VIEW_GET_CLASS (view);
	g_return_if_fail (class->load != NULL);

	class->load (view, filename);
}

/**
 * gal_view_save
 * @view: The view to save
 * @filename: The file to save to
 */
void
gal_view_save (GalView *view,
               const gchar *filename)
{
	GalViewClass *class;

	g_return_if_fail (GAL_IS_VIEW (view));
	g_return_if_fail (filename != NULL);

	class = GAL_VIEW_GET_CLASS (view);
	g_return_if_fail (class->save != NULL);

	class->save (view, filename);
}

/**
 * gal_view_get_title
 * @view: The view to query.
 *
 * Returns: The title of the view.
 */
const gchar *
gal_view_get_title (GalView *view)
{
	GalViewClass *class;

	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	class = GAL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class->get_title != NULL, NULL);

	return class->get_title (view);
}

/**
 * gal_view_set_title
 * @view: The view to set.
 * @title: The new title value.
 */
void
gal_view_set_title (GalView *view,
                    const gchar *title)
{
	GalViewClass *class;

	g_return_if_fail (GAL_IS_VIEW (view));

	class = GAL_VIEW_GET_CLASS (view);
	g_return_if_fail (class->set_title != NULL);

	class->set_title (view, title);

	g_object_notify (G_OBJECT (view), "title");
}

/**
 * gal_view_get_type_code
 * @view: The view to get.
 *
 * Returns: The type of the view.
 */
const gchar *
gal_view_get_type_code (GalView *view)
{
	GalViewClass *class;

	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	class = GAL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class->get_type_code != NULL, NULL);

	return class->get_type_code (view);
}

/**
 * gal_view_clone
 * @view: The view to clone.
 *
 * Returns: The clone.
 */
GalView *
gal_view_clone (GalView *view)
{
	GalViewClass *class;

	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	class = GAL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class->clone != NULL, NULL);

	return class->clone (view);
}

/**
 * gal_view_changed
 * @view: The view that changed.
 */
void
gal_view_changed (GalView *view)
{
	g_return_if_fail (GAL_IS_VIEW (view));

	g_signal_emit (view, signals[CHANGED], 0);
}

