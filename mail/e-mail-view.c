/*
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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "e-mail-view.h"

#include <glib/gi18n-lib.h>

#define E_MAIL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_VIEW, EMailViewPrivate))

struct _EMailViewPrivate {
	gint placeholder;
};

enum {
	PANE_CLOSE,
	VIEW_CHANGED,
	OPEN_MAIL,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_SHELL_VIEW
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EMailView, e_mail_view, GTK_TYPE_VBOX)

static void
mail_view_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			E_MAIL_VIEW(object)->content =
				g_value_get_object (value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_view_get_property (GObject *object,
                        guint property_id,
                        GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			g_value_set_object (
				value, E_MAIL_VIEW(object)->content);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_view_class_init (EMailViewClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_view_set_property;
	object_class->get_property = mail_view_get_property;

	signals[PANE_CLOSE] = g_signal_new (
		"pane-close",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailViewClass , pane_close),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[VIEW_CHANGED] = g_signal_new (
		"view-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailViewClass , view_changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[OPEN_MAIL] = g_signal_new (
		"open-mail",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailViewClass , open_mail),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1, G_TYPE_STRING);

	/**
	 * EMailView:shell-content
	 *
	 * The #EShellContent to which the widget belongs.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-content",
			NULL,
			NULL,
			E_TYPE_SHELL_CONTENT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_mail_view_init (EMailView *view)
{
	view->priv = E_MAIL_VIEW_GET_PRIVATE (view);
}

void
e_mail_view_update_view_instance (EMailView *view)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class->update_view_instance != NULL);

	class->update_view_instance (view);
}

GalViewInstance *
e_mail_view_get_view_instance (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), NULL);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class->get_view_instance != NULL, NULL);

	return class->get_view_instance (view);
}

void
e_mail_view_set_search_strings (EMailView *view,
                                GSList *search_strings)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class->set_search_strings != NULL);

	class->set_search_strings (view, search_strings);
}

EShellSearchbar *
e_mail_view_get_searchbar (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), NULL);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class->get_searchbar != NULL, NULL);

	return class->get_searchbar (view);
}

void
e_mail_view_set_orientation (EMailView *view,
                             GtkOrientation orientation)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class->set_orientation != NULL);

	class->set_orientation (view, orientation);
}

GtkOrientation
e_mail_view_get_orientation (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), 0);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class->get_orientation != NULL, 0);

	return class->get_orientation (view);
}

void
e_mail_view_set_preview_visible (EMailView *view,
                                 gboolean visible)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class->set_preview_visible != NULL);

	class->set_preview_visible (view, visible);
}

gboolean
e_mail_view_get_preview_visible (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), FALSE);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class->get_preview_visible != NULL, FALSE);

	return class->get_preview_visible (view);
}

void
e_mail_view_set_show_deleted (EMailView *view,
                              gboolean show_deleted)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class->set_show_deleted != NULL);

	class->set_show_deleted (view, show_deleted);
}

gboolean
e_mail_view_get_show_deleted (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), FALSE);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class->get_show_deleted != NULL, FALSE);

	return class->get_show_deleted (view);
}
