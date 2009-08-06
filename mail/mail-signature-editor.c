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
 *		Radek Doulik <rodo@ximian.com>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "mail-signature-editor.h"

#include <string.h>
#include <glib/gi18n.h>

#include <e-util/e-error.h>
#include <e-util/e-signature-list.h>
#include <composer/e-msg-composer.h>

#include "mail-config.h"

#define E_SIGNATURE_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SIGNATURE_EDITOR, ESignatureEditorPrivate))

enum {
	PROP_0,
	PROP_SIGNATURE
};

struct _ESignatureEditorPrivate {
	GtkActionGroup *action_group;
	ESignature *signature;
	GtkWidget *entry;
	gchar *original_name;
};

static const gchar *ui =
"<ui>\n"
"  <menubar name='main-menu'>\n"
"    <placeholder name='pre-edit-menu'>\n"
"      <menu action='file-menu'>\n"
"        <menuitem action='save-and-close'/>\n"
"        <separator/>"
"        <menuitem action='close'/>\n"
"      </menu>\n"
"    </placeholder>\n"
"  </menubar>\n"
"  <toolbar name='main-toolbar'>\n"
"    <placeholder name='pre-main-toolbar'>\n"
"      <toolitem action='save-and-close'/>\n"
"    </placeholder>\n"
"  </toolbar>\n"
"</ui>";

static gpointer parent_class = NULL;

static void
handle_error (GError **error)
{
	if (*error != NULL) {
		g_warning ("%s", (*error)->message);
		g_clear_error (error);
	}
}

static void
action_close_cb (GtkAction *action,
                 ESignatureEditor *editor)
{
	gboolean something_changed = FALSE;
	const gchar *original_name;
	const gchar *signature_name;

	original_name = editor->priv->original_name;
	signature_name = gtk_entry_get_text (GTK_ENTRY (editor->priv->entry));

	something_changed |= gtkhtml_editor_has_undo (GTKHTML_EDITOR (editor));
	something_changed |= (strcmp (signature_name, original_name) != 0);

	if (something_changed) {
		gint response;

		response = e_error_run (
			GTK_WINDOW (editor),
			"mail:ask-signature-changed", NULL);
		if (response == GTK_RESPONSE_YES) {
			GtkActionGroup *action_group;

			action_group = editor->priv->action_group;
			action = gtk_action_group_get_action (
				action_group, "save-and-close");
			gtk_action_activate (action);
			return;
		} else if (response == GTK_RESPONSE_CANCEL)
			return;
	}

	gtk_widget_destroy (GTK_WIDGET (editor));
}

static void
action_save_and_close_cb (GtkAction *action,
                          ESignatureEditor *editor)
{
	GtkWidget *entry;
	ESignatureList *signature_list;
	ESignature *signature;
	ESignature *same_name;
	const gchar *filename;
	gchar *signature_name;
	gboolean html;
	GError *error = NULL;

	entry = editor->priv->entry;
	html = gtkhtml_editor_get_html_mode (GTKHTML_EDITOR (editor));

	if (editor->priv->signature == NULL)
		signature = mail_config_signature_new (NULL, FALSE, html);
	else {
		signature = g_object_ref (editor->priv->signature);
		e_signature_set_is_html (signature, html);
	}

	filename = e_signature_get_filename (signature);
	gtkhtml_editor_save (GTKHTML_EDITOR (editor), filename, html, &error);

	if (error != NULL) {
		e_error_run (
			GTK_WINDOW (editor),
			"mail:no-save-signature",
			error->message, NULL);
		g_clear_error (&error);
		return;
	}

	signature_list = mail_config_get_signatures ();

	signature_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	g_strstrip (signature_name);

	/* Make sure the signature name is not blank. */
	if (*signature_name == '\0') {
		e_error_run (
			GTK_WINDOW (editor),
			"mail:blank-signature", NULL);
		gtk_widget_grab_focus (entry);
		g_free (signature_name);
		return;
	}

	/* Don't overwrite an existing signature of the same name.
	 * XXX ESignatureList misuses const. */
	same_name = (ESignature *) e_signature_list_find (
		signature_list, E_SIGNATURE_FIND_NAME, signature_name);
	if (same_name != NULL && !e_signature_is_equal (signature, same_name)) {
		e_error_run (
			GTK_WINDOW (editor),
			"mail:signature-already-exists",
			signature_name, NULL);
		gtk_widget_grab_focus (entry);
		g_free (signature_name);
		return;
	}

	e_signature_set_name (signature, signature_name);
	g_free (signature_name);

	if (editor->priv->signature != NULL)
		e_signature_list_change (signature_list, signature);
	else
		e_signature_list_add (signature_list, signature);
	e_signature_list_save (signature_list);

	gtk_widget_destroy (GTK_WIDGET (editor));
}

static GtkActionEntry entries[] = {

	{ "close",
	  GTK_STOCK_CLOSE,
	  N_("_Close"),
	  "<Control>w",
	  NULL,
	  G_CALLBACK (action_close_cb) },

	{ "save-and-close",
	  GTK_STOCK_SAVE,
	  N_("_Save and Close"),
	  "<Control>Return",
	  NULL,
	  G_CALLBACK (action_save_and_close_cb) },

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL }
};

static gboolean
signature_editor_delete_event_cb (ESignatureEditor *editor,
                                  GdkEvent *event)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = editor->priv->action_group;
	action = gtk_action_group_get_action (action_group, "close");
	gtk_action_activate (action);

	return TRUE;
}

static void
signature_editor_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SIGNATURE:
			e_signature_editor_set_signature (
				E_SIGNATURE_EDITOR (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_editor_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SIGNATURE:
			g_value_set_object (
				value, e_signature_editor_get_signature (
				E_SIGNATURE_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_editor_dispose (GObject *object)
{
	ESignatureEditorPrivate *priv;

	priv = E_SIGNATURE_EDITOR_GET_PRIVATE (object);

	if (priv->action_group != NULL) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	if (priv->signature != NULL) {
		g_object_unref (priv->signature);
		priv->signature = NULL;
	}

	if (priv->entry != NULL) {
		g_object_unref (priv->entry);
		priv->entry = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
signature_editor_finalize (GObject *object)
{
	ESignatureEditorPrivate *priv;

	priv = E_SIGNATURE_EDITOR_GET_PRIVATE (object);

	g_free (priv->original_name);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
signature_editor_class_init (ESignatureEditorClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ESignatureEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = signature_editor_set_property;
	object_class->get_property = signature_editor_get_property;
	object_class->dispose = signature_editor_dispose;
	object_class->finalize = signature_editor_finalize;

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE,
		g_param_spec_object (
			"signature",
			NULL,
			NULL,
			E_TYPE_SIGNATURE,
			G_PARAM_READWRITE));
}

static void
signature_editor_init (ESignatureEditor *editor)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkWidget *container;
	GtkWidget *widget;
	GtkWidget *vbox;
	GError *error = NULL;

	editor->priv = E_SIGNATURE_EDITOR_GET_PRIVATE (editor);
	vbox = GTKHTML_EDITOR (editor)->vbox;

	ui_manager = gtkhtml_editor_get_ui_manager (GTKHTML_EDITOR (editor));

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);
	handle_error (&error);

	action_group = gtk_action_group_new ("signature");
	gtk_action_group_set_translation_domain (
		action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (
		action_group, entries,
		G_N_ELEMENTS (entries), editor);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	editor->priv->action_group = g_object_ref (action_group);

	gtk_ui_manager_ensure_update (ui_manager);

	gtk_window_set_title (GTK_WINDOW (editor), _("Edit Signature"));

	widget = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	/* Position 2 should be between the main and style toolbars. */
	gtk_box_reorder_child (GTK_BOX (vbox), widget, 2);
	gtk_widget_show (widget);
	container = widget;

	widget = gtk_entry_new ();
	gtk_box_pack_end (GTK_BOX (container), widget, TRUE, TRUE, 0);
	editor->priv->entry = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = gtk_label_new_with_mnemonic (_("_Signature Name:"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), editor->priv->entry);
	gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	g_signal_connect (
		editor, "delete-event",
		G_CALLBACK (signature_editor_delete_event_cb), NULL);

	e_signature_editor_set_signature (editor, NULL);
}

GType
e_signature_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ESignatureEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) signature_editor_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ESignatureEditor),
			0,     /* n_preallocs */
			(GInstanceInitFunc) signature_editor_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTKHTML_TYPE_EDITOR, "ESignatureEditor",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_signature_editor_new (void)
{
	return g_object_new (E_TYPE_SIGNATURE_EDITOR, NULL);
}

ESignature *
e_signature_editor_get_signature (ESignatureEditor *editor)
{
	g_return_val_if_fail (E_IS_SIGNATURE_EDITOR (editor), NULL);

	return editor->priv->signature;
}

void
e_signature_editor_set_signature (ESignatureEditor *editor,
                                  ESignature *signature)
{
	const gchar *filename;
	const gchar *signature_name;
	gboolean is_html;
	gchar *contents;
	gsize length;
	GError *error = NULL;

	g_return_if_fail (E_IS_SIGNATURE_EDITOR (editor));

	if (signature != NULL)
		g_return_if_fail (E_SIGNATURE (signature));

	if (editor->priv->signature != NULL) {
		g_object_unref (editor->priv->signature);
		editor->priv->signature = NULL;
	}

	if (signature == NULL)
		goto exit;

	editor->priv->signature = g_object_ref (signature);

	/* Load signature content. */

	filename = e_signature_get_filename (signature);
	is_html = e_signature_get_is_html (signature);

	if (is_html)
		g_file_get_contents (filename, &contents, &length, &error);
	else {
		gchar *data;

		data = e_msg_composer_get_sig_file_content (filename, FALSE);
		contents = g_strdup_printf ("<PRE>\n%s", data);
		length = -1;
		g_free (data);
	}

	if (error == NULL) {
		gtkhtml_editor_set_html_mode (
			GTKHTML_EDITOR (editor), is_html);
		gtkhtml_editor_set_text_html (
			GTKHTML_EDITOR (editor), contents, length);
		g_free (contents);
	} else {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

exit:
	if (signature != NULL)
		signature_name = e_signature_get_name (signature);
	else
		signature_name = _("Unnamed");

	/* Set the entry text before we grab focus. */
	g_free (editor->priv->original_name);
	editor->priv->original_name = g_strdup (signature_name);
	gtk_entry_set_text (GTK_ENTRY (editor->priv->entry), signature_name);

	/* Set the focus appropriately.  If this is a new signature, draw
	 * the user's attention to the signature name entry.  Otherwise go
	 * straight to the editing area. */
	if (signature == NULL)
		gtk_widget_grab_focus (editor->priv->entry);
	else {
		GtkHTML *html;

		html = gtkhtml_editor_get_html (GTKHTML_EDITOR (editor));
		gtk_widget_grab_focus (GTK_WIDGET (html));
	}

	g_object_notify (G_OBJECT (editor), "signature");
}
