/* e-source-selector-dialog.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * published by the Free Software Foundation; either the version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser  General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Rodrigo Moya <rodrigo@novell.com>
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-misc-utils.h"
#include "e-tree-view-frame.h"
#include "e-source-selector.h"
#include "e-source-selector-dialog.h"

struct _ESourceSelectorDialogPrivate {
	GtkWidget *selector;
	ESourceRegistry *registry;
	ESource *selected_source;
	ESource *except_source;
	gchar *extension_name;
};

enum {
	PROP_0,
	PROP_EXTENSION_NAME,
	PROP_REGISTRY,
	PROP_SELECTOR,
	PROP_EXCEPT_SOURCE
};

G_DEFINE_TYPE_WITH_PRIVATE (ESourceSelectorDialog, e_source_selector_dialog, GTK_TYPE_DIALOG)

static void
source_selector_dialog_row_activated_cb (GtkTreeView *tree_view,
                                         GtkTreePath *path,
                                         GtkTreeViewColumn *column,
                                         GtkWidget *dialog)
{
	GtkTreeSelection *selection;

	if (!path)
		return;

	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_path_is_selected (selection, path))
		return;

	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
primary_selection_changed_cb (ESourceSelector *selector,
                              ESourceSelectorDialog *dialog)
{
	ESourceSelectorDialogPrivate *priv = dialog->priv;

	if (priv->selected_source != NULL)
		g_object_unref (priv->selected_source);
	priv->selected_source =
		e_source_selector_ref_primary_selection (selector);

	if (priv->selected_source != NULL) {
		ESource *except_source;

		except_source = e_source_selector_dialog_get_except_source (dialog);

		if (except_source != NULL)
			if (e_source_equal (except_source, priv->selected_source)) {
				g_object_unref (priv->selected_source);
				priv->selected_source = NULL;
			}
	}

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK,
		(priv->selected_source != NULL));
}

static void
source_selector_dialog_set_extension_name (ESourceSelectorDialog *dialog,
                                           const gchar *extension_name)
{
	g_return_if_fail (extension_name != NULL);
	g_return_if_fail (dialog->priv->extension_name == NULL);

	dialog->priv->extension_name = g_strdup (extension_name);
}

static void
source_selector_dialog_set_registry (ESourceSelectorDialog *dialog,
                                     ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (dialog->priv->registry == NULL);

	dialog->priv->registry = g_object_ref (registry);
}

static void
source_selector_dialog_set_property (GObject *object,
                                     guint property_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXTENSION_NAME:
			source_selector_dialog_set_extension_name (
				E_SOURCE_SELECTOR_DIALOG (object),
				g_value_get_string (value));
			return;

		case PROP_REGISTRY:
			source_selector_dialog_set_registry (
				E_SOURCE_SELECTOR_DIALOG (object),
				g_value_get_object (value));
			return;

		case PROP_EXCEPT_SOURCE:
			e_source_selector_dialog_set_except_source (
				E_SOURCE_SELECTOR_DIALOG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_selector_dialog_get_property (GObject *object,
                                     guint property_id,
                                     GValue *value,
                                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXTENSION_NAME:
			g_value_set_string (
				value,
				e_source_selector_dialog_get_extension_name (
				E_SOURCE_SELECTOR_DIALOG (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_source_selector_dialog_get_registry (
				E_SOURCE_SELECTOR_DIALOG (object)));
			return;

		case PROP_SELECTOR:
			g_value_set_object (
				value,
				e_source_selector_dialog_get_selector (
				E_SOURCE_SELECTOR_DIALOG (object)));
			return;

		case PROP_EXCEPT_SOURCE:
			g_value_set_object (
				value,
				e_source_selector_dialog_get_except_source (
				E_SOURCE_SELECTOR_DIALOG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_selector_dialog_dispose (GObject *object)
{
	ESourceSelectorDialog *self = E_SOURCE_SELECTOR_DIALOG (object);

	g_clear_object (&self->priv->registry);
	g_clear_object (&self->priv->selected_source);
	g_clear_object (&self->priv->except_source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_source_selector_dialog_parent_class)->dispose (object);
}

static void
source_selector_dialog_finalize (GObject *object)
{
	ESourceSelectorDialog *self = E_SOURCE_SELECTOR_DIALOG (object);

	g_free (self->priv->extension_name);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_source_selector_dialog_parent_class)->finalize (object);
}

static void
source_selector_dialog_constructed (GObject *object)
{
	ESourceSelectorDialog *dialog;
	ESource *primary_selection;
	GtkWidget *container;
	GtkWidget *widget;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_source_selector_dialog_parent_class)->constructed (object);

	dialog = E_SOURCE_SELECTOR_DIALOG (object);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	widget = e_tree_view_frame_new ();
	e_tree_view_frame_set_toolbar_visible (E_TREE_VIEW_FRAME (widget), FALSE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_source_selector_new (
		dialog->priv->registry,
		dialog->priv->extension_name);
	e_source_selector_set_show_toggles (E_SOURCE_SELECTOR (widget), FALSE);
	e_tree_view_frame_set_tree_view (
		E_TREE_VIEW_FRAME (container),
		GTK_TREE_VIEW (widget));
	dialog->priv->selector = widget;

	g_signal_connect (
		widget, "row_activated",
		G_CALLBACK (source_selector_dialog_row_activated_cb), dialog);
	g_signal_connect (
		widget, "primary_selection_changed",
		G_CALLBACK (primary_selection_changed_cb), dialog);

	primary_selection = e_source_selector_ref_primary_selection (E_SOURCE_SELECTOR (widget));
	if (primary_selection)
		primary_selection_changed_cb (E_SOURCE_SELECTOR (widget), dialog);
	g_clear_object (&primary_selection);
}

static void
e_source_selector_dialog_class_init (ESourceSelectorDialogClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = source_selector_dialog_set_property;
	object_class->get_property = source_selector_dialog_get_property;
	object_class->dispose = source_selector_dialog_dispose;
	object_class->finalize = source_selector_dialog_finalize;
	object_class->constructed = source_selector_dialog_constructed;

	g_object_class_install_property (
		object_class,
		PROP_EXTENSION_NAME,
		g_param_spec_string (
			"extension-name",
			NULL,
			NULL,
			NULL,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			NULL,
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			NULL,
			NULL,
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_EXCEPT_SOURCE,
		g_param_spec_object (
			"except-source",
			NULL,
			NULL,
			E_TYPE_SOURCE,
			G_PARAM_WRITABLE));
}

static void
e_source_selector_dialog_init (ESourceSelectorDialog *dialog)
{
	GtkWidget *action_area;

	dialog->priv = e_source_selector_dialog_get_instance_private (dialog);

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Select destination"));
	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 500);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_widget_set_margin_top (action_area, 5);

	gtk_dialog_add_buttons (
		GTK_DIALOG (dialog),
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
}

/**
 * e_source_selector_dialog_new:
 * @parent: a parent window
 * @registry: an #ESourceRegistry
 * @extension_name: the name of an #ESource extension
 *
 * Displays a list of sources from @registry having an extension named
 * @extension_name in a dialog window.  The sources are grouped by backend
 * or groupware account, which are described by the parent source.
 *
 * Returns: a new #ESourceSelectorDialog
 **/
GtkWidget *
e_source_selector_dialog_new (GtkWindow *parent,
                              ESourceRegistry *registry,
                              const gchar *extension_name)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	return g_object_new (
		E_TYPE_SOURCE_SELECTOR_DIALOG,
		"use-header-bar", e_util_get_use_header_bar (),
		"transient-for", parent,
		"registry", registry,
		"extension-name", extension_name,
		NULL);
}

/**
 * e_source_selector_dialog_get_registry:
 * @dialog: an #ESourceSelectorDialog
 *
 * Returns the #ESourceRegistry passed to e_source_selector_dialog_new().
 *
 * Returns: the #ESourceRegistry for @dialog
 *
 * Since: 3.6
 **/
ESourceRegistry *
e_source_selector_dialog_get_registry (ESourceSelectorDialog *dialog)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR_DIALOG (dialog), NULL);

	return dialog->priv->registry;
}

/**
 * e_source_selector_dialog_get_extension_name:
 * @dialog: an #ESourceSelectorDialog
 *
 * Returns the extension name passed to e_source_selector_dialog_new().
 *
 * Returns: the extension name for @dialog
 *
 * Since: 3.6
 **/
const gchar *
e_source_selector_dialog_get_extension_name (ESourceSelectorDialog *dialog)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR_DIALOG (dialog), NULL);

	return dialog->priv->extension_name;
}

/**
 * e_source_selector_dialog_get_selector:
 * @dialog: an #ESourceSelectorDialog
 *
 * Returns the #ESourceSelector widget embedded in @dialog.
 *
 * Returns: the #ESourceSelector widget
 *
 * Since: 3.6
 **/
ESourceSelector *
e_source_selector_dialog_get_selector (ESourceSelectorDialog *dialog)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR_DIALOG (dialog), NULL);

	return E_SOURCE_SELECTOR (dialog->priv->selector);
}

/**
 * e_source_selector_dialog_peek_primary_selection:
 * @dialog: an #ESourceSelectorDialog
 *
 * Peek the currently selected source in the given @dialog.
 *
 * Returns: the selected #ESource
 */
ESource *
e_source_selector_dialog_peek_primary_selection (ESourceSelectorDialog *dialog)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR_DIALOG (dialog), NULL);

	return dialog->priv->selected_source;
}

/**
 * e_source_selector_dialog_get_except_source:
 * @dialog: an #ESourceSelectorDialog
 *
 * Get the currently #ESource, which cannot be selected in the given @dialog.
 * Use e_source_selector_dialog_set_except_source() to set such.
 *
 * Returns: the #ESource, which cannot be selected
 *
 * Since: 3.18
 **/
ESource *
e_source_selector_dialog_get_except_source (ESourceSelectorDialog *dialog)
{
	g_return_val_if_fail (E_IS_SOURCE_SELECTOR_DIALOG (dialog), NULL);

	return dialog->priv->except_source;
}

/**
 * e_source_selector_dialog_set_except_source:
 * @dialog: an #ESourceSelectorDialog
 * @except_source: (allow-none): an #ESource, which cannot be selected, or %NULL
 *
 * Set the @except_source, the one which cannot be selected in the given @dialog.
 * Use %NULL to allow to select all sources.
 *
 * Since: 3.18
 **/
void
e_source_selector_dialog_set_except_source (ESourceSelectorDialog *dialog,
					    ESource *except_source)
{
	g_return_if_fail (E_IS_SOURCE_SELECTOR_DIALOG (dialog));
	if (except_source)
		g_return_if_fail (E_IS_SOURCE (except_source));

	if ((dialog->priv->except_source && except_source && e_source_equal (dialog->priv->except_source, except_source)) ||
	    dialog->priv->except_source == except_source)
		return;

	g_clear_object (&dialog->priv->except_source);
	dialog->priv->except_source = except_source ? g_object_ref (except_source) : NULL;

	primary_selection_changed_cb (E_SOURCE_SELECTOR (dialog->priv->selector), dialog);

	g_object_notify (G_OBJECT (dialog), "except-source");
}
