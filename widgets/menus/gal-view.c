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

#include <config.h>

#include "e-util/e-util.h"

#include "gal-view.h"

G_DEFINE_TYPE (GalView, gal_view, G_TYPE_OBJECT)

#define d(x)

d(static gint depth = 0;)

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint gal_view_signals [LAST_SIGNAL] = { 0, };

/**
 * gal_view_edit
 * @view: The view to edit
 * @parent: the parent window.
 */
void
gal_view_edit            (GalView *view,
			  GtkWindow *parent)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	if (GAL_VIEW_GET_CLASS (view)->edit)
		GAL_VIEW_GET_CLASS (view)->edit (view, parent);
}

/**
 * gal_view_load
 * @view: The view to load to
 * @filename: The file to load from
 */
void
gal_view_load  (GalView *view,
		const gchar *filename)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));

	if (GAL_VIEW_GET_CLASS (view)->load)
		GAL_VIEW_GET_CLASS (view)->load (view, filename);
}

/**
 * gal_view_save
 * @view: The view to save
 * @filename: The file to save to
 */
void
gal_view_save    (GalView *view,
		  const gchar *filename)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));

	if (GAL_VIEW_GET_CLASS (view)->save)
		GAL_VIEW_GET_CLASS (view)->save (view, filename);
}

/**
 * gal_view_get_title
 * @view: The view to query.
 *
 * Returns: The title of the view.
 */
const gchar *
gal_view_get_title       (GalView *view)
{
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	if (GAL_VIEW_GET_CLASS (view)->get_title)
		return GAL_VIEW_GET_CLASS (view)->get_title (view);
	else
		return NULL;
}

/**
 * gal_view_set_title
 * @view: The view to set.
 * @title: The new title value.
 */
void
gal_view_set_title       (GalView *view,
			  const gchar *title)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));

	if (GAL_VIEW_GET_CLASS (view)->set_title)
		GAL_VIEW_GET_CLASS (view)->set_title (view, title);
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
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	if (GAL_VIEW_GET_CLASS (view)->get_type_code)
		return GAL_VIEW_GET_CLASS (view)->get_type_code (view);
	else
		return NULL;
}

/**
 * gal_view_clone
 * @view: The view to clone.
 *
 * Returns: The clone.
 */
GalView *
gal_view_clone       (GalView *view)
{
	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GAL_IS_VIEW (view), NULL);

	if (GAL_VIEW_GET_CLASS (view)->clone)
		return GAL_VIEW_GET_CLASS (view)->clone (view);
	else
		return NULL;
}

/**
 * gal_view_changed
 * @view: The view that changed.
 */
void
gal_view_changed       (GalView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GAL_IS_VIEW (view));

	g_signal_emit(view,
		      gal_view_signals [CHANGED], 0);
}

static void
gal_view_class_init      (GalViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	klass->edit           = NULL;
	klass->load           = NULL;
	klass->save           = NULL;
	klass->get_title      = NULL;
	klass->clone          = NULL;

	klass->changed        = NULL;

	gal_view_signals [CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GalViewClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
gal_view_init      (GalView *view)
{
}

