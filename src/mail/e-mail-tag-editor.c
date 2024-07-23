/*
 *
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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-mail-tag-editor.h"

#include <time.h>
#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"

#define DEFAULT_FLAG 2  /* "Follow-Up" */

struct _EMailTagEditorPrivate {
	GtkTreeView *message_list;
	GtkComboBox *combo_entry;
	EDateEdit *target_date;

	gboolean completed;
	time_t completed_date;
};

enum {
	PROP_0,
	PROP_COMPLETED
};

enum {
	COLUMN_FROM,
	COLUMN_SUBJECT
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailTagEditor, e_mail_tag_editor, GTK_TYPE_DIALOG)

static void
mail_tag_editor_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COMPLETED:
			e_mail_tag_editor_set_completed (
				E_MAIL_TAG_EDITOR (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_tag_editor_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COMPLETED:
			g_value_set_boolean (
				value,
				e_mail_tag_editor_get_completed (
				E_MAIL_TAG_EDITOR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_tag_editor_realize (GtkWidget *widget)
{
	GtkWidget *action_area;
	GtkWidget *content_area;

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_mail_tag_editor_parent_class)->realize (widget);

	/* XXX Override GTK's brain-dead border width defaults. */

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (widget));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (widget));

	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
}

static void
e_mail_tag_editor_class_init (EMailTagEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_tag_editor_set_property;
	object_class->get_property = mail_tag_editor_get_property;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = mail_tag_editor_realize;

	g_object_class_install_property (
		object_class,
		PROP_COMPLETED,
		g_param_spec_boolean (
			"completed",
			"Completed",
			NULL,
			FALSE,
			G_PARAM_READWRITE));
}

static void
e_mail_tag_editor_init (EMailTagEditor *editor)
{
	GtkBuilder *builder;
	GtkDialog *dialog;
	GtkWidget *widget;
	GtkWidget *content_area;
	GtkWindow *window;
	GtkCellRenderer *renderer;
	GtkListStore *store;
	const gchar *date_format;

	editor->priv = e_mail_tag_editor_get_instance_private (editor);

	dialog = GTK_DIALOG (editor);
	window = GTK_WINDOW (editor);

	gtk_window_set_default_size (window, 400, 500);
	gtk_window_set_title (window, _("Flag to Follow Up"));
	gtk_window_set_icon_name (window, "stock_mail-flag-for-followup");

	gtk_dialog_add_buttons (
		dialog,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("Cl_ear"), GTK_RESPONSE_REJECT,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

	content_area = gtk_dialog_get_content_area (dialog);

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_DATE_EDIT);

	/* Load the rest of the UI from the builder file. */

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-dialogs.ui");

	widget = e_builder_get_widget (builder, "toplevel");
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 6);

	widget = e_builder_get_widget (builder, "pixmap");
	e_binding_bind_property (
		window, "icon-name",
		widget, "icon-name",
		G_BINDING_SYNC_CREATE);

	widget = e_builder_get_widget (builder, "message_list");
	editor->priv->message_list = GTK_TREE_VIEW (widget);

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (
		GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (widget), -1, _("From"),
		renderer, "text", 0, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (widget), -1, _("Subject"),
		renderer, "text", 1, NULL);

	widget = e_builder_get_widget (builder, "combo");
	editor->priv->combo_entry = GTK_COMBO_BOX (widget);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), DEFAULT_FLAG);

	widget = e_builder_get_widget (builder, "target_date");
	editor->priv->target_date = E_DATE_EDIT (widget);

	date_format = e_datetime_format_get_format ("calendar", "table",  DTFormatKindDate);
	/* the "%ad" is not a strftime format, thus avoid it, if included */
	if (date_format && *date_format && !strstr (date_format, "%ad"))
		e_date_edit_set_date_format (editor->priv->target_date, date_format);

	widget = e_builder_get_widget (builder, "completed");
	e_binding_bind_property (
		editor, "completed",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	g_object_unref (builder);
}

GtkWidget *
e_mail_tag_editor_new (void)
{
	return g_object_new (E_TYPE_MAIL_TAG_EDITOR, NULL);
}

gboolean
e_mail_tag_editor_get_completed (EMailTagEditor *editor)
{
	g_return_val_if_fail (E_IS_MAIL_TAG_EDITOR (editor), FALSE);

	return editor->priv->completed;
}

void
e_mail_tag_editor_set_completed (EMailTagEditor *editor,
                                 gboolean completed)
{
	g_return_if_fail (E_IS_MAIL_TAG_EDITOR (editor));

	if (editor->priv->completed == completed)
		return;

	editor->priv->completed = completed;
	editor->priv->completed_date = completed ? time (NULL) : 0;

	g_object_notify (G_OBJECT (editor), "completed");
}

CamelNameValueArray *
e_mail_tag_editor_get_tag_list (EMailTagEditor *editor)
{
	CamelNameValueArray *tag_list;
	time_t date;
	gchar *text = NULL;
	GtkWidget *entry;

	g_return_val_if_fail (E_IS_MAIL_TAG_EDITOR (editor), NULL);

	tag_list = camel_name_value_array_new ();

	entry = gtk_bin_get_child (GTK_BIN (editor->priv->combo_entry));
	if (entry)
		text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	camel_name_value_array_set_named (tag_list, CAMEL_COMPARE_CASE_SENSITIVE, "follow-up", text);
	g_free (text);

	date = e_date_edit_get_time (editor->priv->target_date);
	if (date != (time_t) -1) {
		text = camel_header_format_date (date, 0);
		camel_name_value_array_set_named (tag_list, CAMEL_COMPARE_CASE_SENSITIVE, "due-by", text);
		g_free (text);
	} else
		camel_name_value_array_set_named (tag_list, CAMEL_COMPARE_CASE_SENSITIVE, "due-by", "");

	if (e_mail_tag_editor_get_completed (editor)) {
		text = camel_header_format_date (editor->priv->completed_date, 0);
		camel_name_value_array_set_named (tag_list, CAMEL_COMPARE_CASE_SENSITIVE, "completed-on", text);
		g_free (text);
	} else
		camel_name_value_array_set_named (tag_list, CAMEL_COMPARE_CASE_SENSITIVE, "completed-on", "");

	return tag_list;
}

void
e_mail_tag_editor_set_tag_list (EMailTagEditor *editor,
				const CamelNameValueArray *tag_list)
{
	GtkWidget *child;
	const gchar *text;
	time_t date;

	g_return_if_fail (E_IS_MAIL_TAG_EDITOR (editor));
	g_return_if_fail (tag_list != NULL);

	/* Extract the GtkEntry from the GtkComboBoxEntry. */
	child = gtk_bin_get_child (GTK_BIN (editor->priv->combo_entry));

	/* XXX This is kind of cheating.  Since we only store the
	 *     translated tag there's no sure-fire way to determine
	 *     the corresponding combo box index (e.g. the tag may
	 *     have been set while running in a different locale). */
	text = camel_name_value_array_get_named (tag_list, CAMEL_COMPARE_CASE_SENSITIVE, "follow-up");
	if (text != NULL)
		gtk_entry_set_text (GTK_ENTRY (child), text);

	text = camel_name_value_array_get_named (tag_list, CAMEL_COMPARE_CASE_SENSITIVE, "due-by");
	if (text != NULL && *text != '\0') {
		date = camel_header_decode_date (text, NULL);
		e_date_edit_set_time (editor->priv->target_date, date);
	} else
		e_date_edit_set_time (editor->priv->target_date, (time_t) -1);

	text = camel_name_value_array_get_named (tag_list, CAMEL_COMPARE_CASE_SENSITIVE, "completed-on");
	if (text != NULL && *text != '\0') {
		date = camel_header_decode_date (text, NULL);
		if (date != (time_t) 0) {
			e_mail_tag_editor_set_completed (editor, TRUE);
			editor->priv->completed_date = date;
		}
	}
}

void
e_mail_tag_editor_add_message (EMailTagEditor *editor,
                               const gchar *from,
                               const gchar *subject)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (E_MAIL_TAG_EDITOR (editor));

	model = gtk_tree_view_get_model (editor->priv->message_list);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (model), &iter,
		COLUMN_FROM, from, COLUMN_SUBJECT, subject, -1);
}
