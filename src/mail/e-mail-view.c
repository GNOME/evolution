/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#include "evolution-config.h"

#include "e-mail-view.h"

#include <glib/gi18n-lib.h>

struct _EMailViewPrivate {
	EShellView *shell_view;
	GtkOrientation orientation;
	EMailView *previous_view;

	guint preview_visible : 1;
	guint show_deleted : 1;
	guint show_junk : 1;
};

enum {
	PROP_0,
	PROP_ORIENTATION,
	PROP_PREVIEW_VISIBLE,
	PROP_PREVIOUS_VIEW,
	PROP_SHELL_VIEW,
	PROP_SHOW_DELETED,
	PROP_SHOW_JUNK
};

enum {
	PANE_CLOSE,
	VIEW_CHANGED,
	OPEN_MAIL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMailView, e_mail_view, GTK_TYPE_BOX)

static void
mail_view_set_shell_view (EMailView *view,
                          EShellView *shell_view)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (view->priv->shell_view == NULL);

	view->priv->shell_view = g_object_ref (shell_view);
}

static void
mail_view_set_property (GObject *object,
                        guint property_id,
                        const GValue *value,
                        GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ORIENTATION:
			e_mail_view_set_orientation (
				E_MAIL_VIEW (object),
				g_value_get_enum (value));
			return;

		case PROP_PREVIEW_VISIBLE:
			e_mail_view_set_preview_visible (
				E_MAIL_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_PREVIOUS_VIEW:
			e_mail_view_set_previous_view (
				E_MAIL_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_SHELL_VIEW:
			mail_view_set_shell_view (
				E_MAIL_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_SHOW_DELETED:
			e_mail_view_set_show_deleted (
				E_MAIL_VIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SHOW_JUNK:
			e_mail_view_set_show_junk (
				E_MAIL_VIEW (object),
				g_value_get_boolean (value));
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
		case PROP_ORIENTATION:
			g_value_set_enum (
				value, e_mail_view_get_orientation (
				E_MAIL_VIEW (object)));
			return;

		case PROP_PREVIEW_VISIBLE:
			g_value_set_boolean (
				value, e_mail_view_get_preview_visible (
				E_MAIL_VIEW (object)));
			return;

		case PROP_PREVIOUS_VIEW:
			g_value_set_object (
				value, e_mail_view_get_previous_view (
				E_MAIL_VIEW (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value, e_mail_view_get_shell_view (
				E_MAIL_VIEW (object)));
			return;

		case PROP_SHOW_DELETED:
			g_value_set_boolean (
				value, e_mail_view_get_show_deleted (
				E_MAIL_VIEW (object)));
			return;

		case PROP_SHOW_JUNK:
			g_value_set_boolean (
				value, e_mail_view_get_show_junk (
				E_MAIL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_view_dispose (GObject *object)
{
	EMailView *self = E_MAIL_VIEW (object);

	g_clear_object (&self->priv->shell_view);
	g_clear_object (&self->priv->previous_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_view_parent_class)->dispose (object);
}

static GtkOrientation
mail_view_get_orientation (EMailView *view)
{
	return view->priv->orientation;
}

static void
mail_view_set_orientation (EMailView *view,
                           GtkOrientation orientation)
{
	if (view->priv->orientation == orientation)
		return;

	view->priv->orientation = orientation;

	g_object_notify (G_OBJECT (view), "orientation");

	e_mail_view_update_view_instance (view);
}

static gboolean
mail_view_get_preview_visible (EMailView *view)
{
	return view->priv->preview_visible;
}

static void
mail_view_set_preview_visible (EMailView *view,
                               gboolean preview_visible)
{
	if (view->priv->preview_visible == preview_visible)
		return;

	view->priv->preview_visible = preview_visible;

	g_object_notify (G_OBJECT (view), "preview-visible");
}

static gboolean
mail_view_get_show_deleted (EMailView *view)
{
	return view->priv->show_deleted;
}

static void
mail_view_set_show_deleted (EMailView *view,
                            gboolean show_deleted)
{
	if (view->priv->show_deleted == show_deleted)
		return;

	view->priv->show_deleted = show_deleted;

	g_object_notify (G_OBJECT (view), "show-deleted");
}

static gboolean
mail_view_get_show_junk (EMailView *view)
{
	return view->priv->show_junk;
}

static void
mail_view_set_show_junk (EMailView *view,
			 gboolean show_junk)
{
	if (view->priv->show_junk == show_junk)
		return;

	view->priv->show_junk = show_junk;

	g_object_notify (G_OBJECT (view), "show-junk");
}

static void
e_mail_view_class_init (EMailViewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_view_set_property;
	object_class->get_property = mail_view_get_property;
	object_class->dispose = mail_view_dispose;

	class->get_orientation = mail_view_get_orientation;
	class->set_orientation = mail_view_set_orientation;
	class->get_preview_visible = mail_view_get_preview_visible;
	class->set_preview_visible = mail_view_set_preview_visible;
	class->get_show_deleted = mail_view_get_show_deleted;
	class->set_show_deleted = mail_view_set_show_deleted;
	class->get_show_junk = mail_view_get_show_junk;
	class->set_show_junk = mail_view_set_show_junk;

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

	g_object_class_install_property (
		object_class,
		PROP_ORIENTATION,
		g_param_spec_enum (
			"orientation",
			"Orientation",
			NULL,
			GTK_TYPE_ORIENTATION,
			GTK_ORIENTATION_HORIZONTAL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PREVIEW_VISIBLE,
		g_param_spec_boolean (
			"preview-visible",
			"Preview Visible",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PREVIOUS_VIEW,
		g_param_spec_object (
			"previous-view",
			"Previous View",
			NULL,
			E_TYPE_MAIL_VIEW,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			"Shell View",
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_DELETED,
		g_param_spec_boolean (
			"show-deleted",
			"Show Deleted",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_JUNK,
		g_param_spec_boolean (
			"show-junk",
			"Show Junk",
			NULL,
			FALSE,
			G_PARAM_READWRITE));
}

static void
e_mail_view_init (EMailView *view)
{
	view->priv = e_mail_view_get_instance_private (view);
	view->priv->orientation = GTK_ORIENTATION_VERTICAL;
}

EShellView *
e_mail_view_get_shell_view (EMailView *view)
{
	g_return_val_if_fail (E_IS_MAIL_VIEW (view), NULL);

	return view->priv->shell_view;
}

void
e_mail_view_update_view_instance (EMailView *view)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->update_view_instance != NULL);

	class->update_view_instance (view);
}

GalViewInstance *
e_mail_view_get_view_instance (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), NULL);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class != NULL, NULL);
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
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_search_strings != NULL);

	class->set_search_strings (view, search_strings);
}

GtkOrientation
e_mail_view_get_orientation (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), 0);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class != NULL, 0);
	g_return_val_if_fail (class->get_orientation != NULL, 0);

	return class->get_orientation (view);
}

void
e_mail_view_set_orientation (EMailView *view,
                             GtkOrientation orientation)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_orientation != NULL);

	class->set_orientation (view, orientation);
}

gboolean
e_mail_view_get_preview_visible (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), FALSE);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->get_preview_visible != NULL, FALSE);

	return class->get_preview_visible (view);
}

void
e_mail_view_set_preview_visible (EMailView *view,
                                 gboolean visible)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_preview_visible != NULL);

	class->set_preview_visible (view, visible);
}

EMailView *
e_mail_view_get_previous_view (EMailView *view)
{
	g_return_val_if_fail (E_IS_MAIL_VIEW (view), NULL);

	return view->priv->previous_view;
}

void
e_mail_view_set_previous_view (EMailView *view,
                               EMailView *previous_view)
{
	g_return_if_fail (E_IS_MAIL_VIEW (view));

	if (view->priv->previous_view == previous_view)
		return;

	if (previous_view != NULL) {
		g_return_if_fail (E_IS_MAIL_VIEW (previous_view));
		g_object_ref (previous_view);
	}

	if (view->priv->previous_view != NULL)
		g_object_unref (view->priv->previous_view);

	view->priv->previous_view = previous_view;

	g_object_notify (G_OBJECT (view), "previous-view");
}

gboolean
e_mail_view_get_show_deleted (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), FALSE);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->get_show_deleted != NULL, FALSE);

	return class->get_show_deleted (view);
}

void
e_mail_view_set_show_deleted (EMailView *view,
                              gboolean show_deleted)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_show_deleted != NULL);

	class->set_show_deleted (view, show_deleted);
}

gboolean
e_mail_view_get_show_junk (EMailView *view)
{
	EMailViewClass *class;

	g_return_val_if_fail (E_IS_MAIL_VIEW (view), FALSE);

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_val_if_fail (class != NULL, FALSE);
	g_return_val_if_fail (class->get_show_junk != NULL, FALSE);

	return class->get_show_junk (view);
}

void
e_mail_view_set_show_junk (EMailView *view,
			   gboolean show_junk)
{
	EMailViewClass *class;

	g_return_if_fail (E_IS_MAIL_VIEW (view));

	class = E_MAIL_VIEW_GET_CLASS (view);
	g_return_if_fail (class != NULL);
	g_return_if_fail (class->set_show_junk != NULL);

	class->set_show_junk (view, show_junk);
}
