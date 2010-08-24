/*
 * e-signature-manager.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-signature-manager.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>
#include "e-util/e-binding.h"
#include "e-signature-tree-view.h"
#include "e-signature-script-dialog.h"

#define E_SIGNATURE_MANAGER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SIGNATURE_MANAGER, ESignatureManagerPrivate))

struct _ESignatureManagerPrivate {
	ESignatureList *signature_list;

	GtkWidget *tree_view;
	GtkWidget *add_button;
	GtkWidget *add_script_button;
	GtkWidget *edit_button;
	GtkWidget *remove_button;

	guint allow_scripts : 1;
	guint prefer_html   : 1;
};

enum {
	PROP_0,
	PROP_ALLOW_SCRIPTS,
	PROP_PREFER_HTML,
	PROP_SIGNATURE_LIST
};

enum {
	ADD_SIGNATURE,
	ADD_SIGNATURE_SCRIPT,
	EDITOR_CREATED,
	EDIT_SIGNATURE,
	REMOVE_SIGNATURE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	ESignatureManager,
	e_signature_manager,
	GTK_TYPE_TABLE)

static void
signature_manager_emit_editor_created (ESignatureManager *manager,
                                       GtkWidget *editor)
{
	g_return_if_fail (E_IS_SIGNATURE_EDITOR (editor));

	g_signal_emit (manager, signals[EDITOR_CREATED], 0, editor);
}

static gboolean
signature_manager_key_press_event_cb (ESignatureManager *manager,
                                      GdkEventKey *event)
{
	if (event->keyval == GDK_Delete) {
		e_signature_manager_remove_signature (manager);
		return TRUE;
	}

	return FALSE;
}

static gboolean
signature_manager_run_script_dialog (ESignatureManager *manager,
                                     ESignature *signature,
                                     const gchar *title)
{
	GtkWidget *dialog;
	GFile *script_file;
	const gchar *name;
	const gchar *filename;
	const gchar *script_name;
	gboolean success = FALSE;
	gpointer parent;
	gchar *path;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	dialog = e_signature_script_dialog_new (parent);
	gtk_window_set_title (GTK_WINDOW (dialog), title);

	name = e_signature_get_name (signature);
	filename = e_signature_get_filename (signature);

	if (filename != NULL && name != NULL) {

		script_file = g_file_new_for_path (filename);
		script_name = name;

		e_signature_script_dialog_set_script_file (
			E_SIGNATURE_SCRIPT_DIALOG (dialog), script_file);
		e_signature_script_dialog_set_script_name (
			E_SIGNATURE_SCRIPT_DIALOG (dialog), script_name);

		g_object_unref (script_file);
	}

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	script_file = e_signature_script_dialog_get_script_file (
		E_SIGNATURE_SCRIPT_DIALOG (dialog));
	script_name = e_signature_script_dialog_get_script_name (
		E_SIGNATURE_SCRIPT_DIALOG (dialog));

	path = g_file_get_path (script_file);
	e_signature_set_name (signature, script_name);
	e_signature_set_filename (signature, path);
	g_free (path);

	g_object_unref (script_file);

	success = TRUE;

exit:
	gtk_widget_destroy (dialog);

	return success;
}

static void
signature_manager_selection_changed_cb (ESignatureManager *manager,
                                        GtkTreeSelection *selection)
{
	ESignatureTreeView *tree_view;
	ESignature *signature;
	GtkWidget *edit_button;
	GtkWidget *remove_button;
	gboolean sensitive;

	edit_button = manager->priv->edit_button;
	remove_button = manager->priv->remove_button;

	tree_view = e_signature_manager_get_tree_view (manager);
	signature = e_signature_tree_view_get_selected (tree_view);
	sensitive = (signature != NULL);

	gtk_widget_set_sensitive (edit_button, sensitive);
	gtk_widget_set_sensitive (remove_button, sensitive);
}

static void
signature_manager_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALLOW_SCRIPTS:
			e_signature_manager_set_allow_scripts (
				E_SIGNATURE_MANAGER (object),
				g_value_get_boolean (value));
			return;

		case PROP_PREFER_HTML:
			e_signature_manager_set_prefer_html (
				E_SIGNATURE_MANAGER (object),
				g_value_get_boolean (value));
			return;

		case PROP_SIGNATURE_LIST:
			e_signature_manager_set_signature_list (
				E_SIGNATURE_MANAGER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_manager_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALLOW_SCRIPTS:
			g_value_set_boolean (
				value,
				e_signature_manager_get_allow_scripts (
				E_SIGNATURE_MANAGER (object)));
			return;

		case PROP_PREFER_HTML:
			g_value_set_boolean (
				value,
				e_signature_manager_get_prefer_html (
				E_SIGNATURE_MANAGER (object)));
			return;

		case PROP_SIGNATURE_LIST:
			g_value_set_object (
				value,
				e_signature_manager_get_signature_list (
				E_SIGNATURE_MANAGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_manager_dispose (GObject *object)
{
	ESignatureManagerPrivate *priv;

	priv = E_SIGNATURE_MANAGER_GET_PRIVATE (object);

	if (priv->signature_list != NULL) {
		g_object_unref (priv->signature_list);
		priv->signature_list = NULL;
	}

	if (priv->tree_view != NULL) {
		g_object_unref (priv->tree_view);
		priv->tree_view = NULL;
	}

	if (priv->add_button != NULL) {
		g_object_unref (priv->add_button);
		priv->add_button = NULL;
	}

	if (priv->add_script_button != NULL) {
		g_object_unref (priv->add_script_button);
		priv->add_script_button = NULL;
	}

	if (priv->edit_button != NULL) {
		g_object_unref (priv->edit_button);
		priv->edit_button = NULL;
	}

	if (priv->remove_button != NULL) {
		g_object_unref (priv->remove_button);
		priv->remove_button = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_signature_manager_parent_class)->dispose (object);
}

static void
signature_manager_add_signature (ESignatureManager *manager)
{
	ESignatureTreeView *tree_view;
	GtkWidget *editor;

	tree_view = e_signature_manager_get_tree_view (manager);

	editor = e_signature_editor_new ();
	gtkhtml_editor_set_html_mode (
		GTKHTML_EDITOR (editor), manager->priv->prefer_html);
	signature_manager_emit_editor_created (manager, editor);

	gtk_widget_grab_focus (GTK_WIDGET (tree_view));
}

static void
signature_manager_add_signature_script (ESignatureManager *manager)
{
	ESignatureTreeView *tree_view;
	ESignatureList *signature_list;
	ESignature *signature;
	const gchar *title;

	title = _("Add Signature Script");
	tree_view = e_signature_manager_get_tree_view (manager);
	signature_list = e_signature_manager_get_signature_list (manager);

	signature = e_signature_new ();
	e_signature_set_is_script (signature, TRUE);
	e_signature_set_is_html (signature, TRUE);

	if (signature_manager_run_script_dialog (manager, signature, title))
		e_signature_list_add (signature_list, signature);

	e_signature_list_save (signature_list);
	g_object_unref (signature);

	gtk_widget_grab_focus (GTK_WIDGET (tree_view));
}

static void
signature_manager_editor_created (ESignatureManager *manager,
                                  ESignatureEditor *editor)
{
	GtkWindowPosition position;
	gpointer parent;

	position = GTK_WIN_POS_CENTER_ON_PARENT;
	parent = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	gtk_window_set_transient_for (GTK_WINDOW (editor), parent);
	gtk_window_set_position (GTK_WINDOW (editor), position);
	gtk_widget_show (GTK_WIDGET (editor));
}

static void
signature_manager_edit_signature (ESignatureManager *manager)
{
	ESignatureTreeView *tree_view;
	ESignatureList *signature_list;
	ESignature *signature;
	GtkWidget *editor;
	const gchar *title;
	const gchar *filename;

	tree_view = e_signature_manager_get_tree_view (manager);
	signature = e_signature_tree_view_get_selected (tree_view);
	signature_list = e_signature_manager_get_signature_list (manager);

	if (signature == NULL)
		return;

	if (e_signature_get_is_script (signature))
		goto script;

	filename = e_signature_get_filename (signature);
	if (filename == NULL || *filename == '\0')
		e_signature_set_filename (signature, _("Unnamed"));

	editor = e_signature_editor_new ();
	e_signature_editor_set_signature (
		E_SIGNATURE_EDITOR (editor), signature);
	signature_manager_emit_editor_created (manager, editor);

	goto exit;

script:
	title = _("Edit Signature Script");

	if (signature_manager_run_script_dialog (manager, signature, title))
		e_signature_list_change (signature_list, signature);

	e_signature_list_save (signature_list);

exit:
	gtk_widget_grab_focus (GTK_WIDGET (tree_view));

	g_object_unref (signature);
}

static void
signature_manager_remove_signature (ESignatureManager *manager)
{
	ESignatureTreeView *tree_view;
	ESignatureList *signature_list;
	ESignature *signature;
	const gchar *filename;
	gboolean is_script;

	tree_view = e_signature_manager_get_tree_view (manager);
	signature = e_signature_tree_view_get_selected (tree_view);
	signature_list = e_signature_tree_view_get_signature_list (tree_view);

	if (signature == NULL)
		return;

	filename = e_signature_get_filename (signature);
	is_script = e_signature_get_is_script (signature);

	if (filename != NULL && !is_script)
		g_unlink (filename);

	e_signature_list_remove (signature_list, signature);
	e_signature_list_save (signature_list);

	gtk_widget_grab_focus (GTK_WIDGET (tree_view));
}

static void
e_signature_manager_class_init (ESignatureManagerClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ESignatureManagerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = signature_manager_set_property;
	object_class->get_property = signature_manager_get_property;
	object_class->dispose = signature_manager_dispose;

	class->add_signature = signature_manager_add_signature;
	class->add_signature_script = signature_manager_add_signature_script;
	class->editor_created = signature_manager_editor_created;
	class->edit_signature = signature_manager_edit_signature;
	class->remove_signature = signature_manager_remove_signature;

	g_object_class_install_property (
		object_class,
		PROP_ALLOW_SCRIPTS,
		g_param_spec_boolean (
			"allow-scripts",
			"Allow Scripts",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_PREFER_HTML,
		g_param_spec_boolean (
			"prefer-html",
			"Prefer HTML",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE_LIST,
		g_param_spec_object (
			"signature-list",
			"Signature List",
			NULL,
			E_TYPE_SIGNATURE_LIST,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[ADD_SIGNATURE] = g_signal_new (
		"add-signature",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ESignatureManagerClass, add_signature),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[ADD_SIGNATURE_SCRIPT] = g_signal_new (
		"add-signature-script",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ESignatureManagerClass, add_signature_script),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[EDITOR_CREATED] = g_signal_new (
		"editor-created",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESignatureManagerClass, editor_created),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SIGNATURE_EDITOR);

	signals[EDIT_SIGNATURE] = g_signal_new (
		"edit-signature",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ESignatureManagerClass, edit_signature),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[REMOVE_SIGNATURE] = g_signal_new (
		"remove-signature",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ESignatureManagerClass, remove_signature),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_signature_manager_init (ESignatureManager *manager)
{
	GtkTreeSelection *selection;
	GtkWidget *container;
	GtkWidget *widget;

	manager->priv = E_SIGNATURE_MANAGER_GET_PRIVATE (manager);

	gtk_table_resize (GTK_TABLE (manager), 1, 2);
	gtk_table_set_col_spacings (GTK_TABLE (manager), 6);
	gtk_table_set_row_spacings (GTK_TABLE (manager), 12);

	container = GTK_WIDGET (manager);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_table_attach (
		GTK_TABLE (container), widget, 0, 1, 0, 1,
		GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = e_signature_tree_view_new ();
	gtk_container_add (GTK_CONTAINER (container), widget);
	manager->priv->tree_view = g_object_ref (widget);
	gtk_widget_show (widget);

	e_mutual_binding_new (
		manager, "signature-list",
		widget, "signature-list");

	g_signal_connect_swapped (
		widget, "key-press-event",
		G_CALLBACK (signature_manager_key_press_event_cb),
		manager);

	g_signal_connect_swapped (
		widget, "row-activated",
		G_CALLBACK (e_signature_manager_edit_signature),
		manager);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (signature_manager_selection_changed_cb),
		manager);

	container = GTK_WIDGET (manager);

	widget = gtk_vbutton_box_new ();
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_table_attach (
		GTK_TABLE (container), widget,
		1, 2, 0, 2, 0, GTK_FILL, 0, 0);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->add_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_signature_manager_add_signature),
		manager);

	widget = gtk_button_new_with_mnemonic (_("Add _Script"));
	gtk_button_set_image (
		GTK_BUTTON (widget), gtk_image_new_from_stock (
		GTK_STOCK_EXECUTE, GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->add_script_button = g_object_ref (widget);
	gtk_widget_show (widget);

	e_binding_new (
		manager, "allow-scripts",
		widget, "sensitive");

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_signature_manager_add_signature_script),
		manager);

	widget = gtk_button_new_from_stock (GTK_STOCK_EDIT);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->edit_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_signature_manager_edit_signature),
		manager);

	widget = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	manager->priv->remove_button = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (e_signature_manager_remove_signature),
		manager);
}

GtkWidget *
e_signature_manager_new (ESignatureList *signature_list)
{
	g_return_val_if_fail (E_IS_SIGNATURE_LIST (signature_list), NULL);

	return g_object_new (
		E_TYPE_SIGNATURE_MANAGER,
		"signature-list", signature_list, NULL);
}

void
e_signature_manager_add_signature (ESignatureManager *manager)
{
	g_return_if_fail (E_IS_SIGNATURE_MANAGER (manager));

	g_signal_emit (manager, signals[ADD_SIGNATURE], 0);
}

void
e_signature_manager_add_signature_script (ESignatureManager *manager)
{
	g_return_if_fail (E_IS_SIGNATURE_MANAGER (manager));

	g_signal_emit (manager, signals[ADD_SIGNATURE_SCRIPT], 0);
}

void
e_signature_manager_edit_signature (ESignatureManager *manager)
{
	g_return_if_fail (E_IS_SIGNATURE_MANAGER (manager));

	g_signal_emit (manager, signals[EDIT_SIGNATURE], 0);
}

void
e_signature_manager_remove_signature (ESignatureManager *manager)
{
	g_return_if_fail (E_IS_SIGNATURE_MANAGER (manager));

	g_signal_emit (manager, signals[REMOVE_SIGNATURE], 0);
}

gboolean
e_signature_manager_get_allow_scripts (ESignatureManager *manager)
{
	g_return_val_if_fail (E_IS_SIGNATURE_MANAGER (manager), FALSE);

	return manager->priv->allow_scripts;
}

void
e_signature_manager_set_allow_scripts (ESignatureManager *manager,
                                       gboolean allow_scripts)
{
	g_return_if_fail (E_IS_SIGNATURE_MANAGER (manager));

	manager->priv->allow_scripts = allow_scripts;

	g_object_notify (G_OBJECT (manager), "allow-scripts");
}

gboolean
e_signature_manager_get_prefer_html (ESignatureManager *manager)
{
	g_return_val_if_fail (E_IS_SIGNATURE_MANAGER (manager), FALSE);

	return manager->priv->prefer_html;
}

void
e_signature_manager_set_prefer_html (ESignatureManager *manager,
                                     gboolean prefer_html)
{
	g_return_if_fail (E_IS_SIGNATURE_MANAGER (manager));

	manager->priv->prefer_html = prefer_html;

	g_object_notify (G_OBJECT (manager), "prefer-html");
}

ESignatureList *
e_signature_manager_get_signature_list (ESignatureManager *manager)
{
	g_return_val_if_fail (E_IS_SIGNATURE_MANAGER (manager), NULL);

	return manager->priv->signature_list;
}

void
e_signature_manager_set_signature_list (ESignatureManager *manager,
                                        ESignatureList *signature_list)
{
	g_return_if_fail (E_IS_SIGNATURE_MANAGER (manager));

	if (signature_list != NULL) {
		g_return_if_fail (E_IS_SIGNATURE_LIST (signature_list));
		g_object_ref (signature_list);
	}

	if (manager->priv->signature_list != NULL)
		g_object_unref (manager->priv->signature_list);

	manager->priv->signature_list = signature_list;

	g_object_notify (G_OBJECT (manager), "signature-list");
}

ESignatureTreeView *
e_signature_manager_get_tree_view (ESignatureManager *manager)
{
	g_return_val_if_fail (E_IS_SIGNATURE_MANAGER (manager), NULL);

	return E_SIGNATURE_TREE_VIEW (manager->priv->tree_view);
}
