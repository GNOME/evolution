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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-tag-editor.h"

#include <time.h>
#include <glib/gi18n-lib.h>

#include "e-util/e-util.h"
#include "e-util/e-binding.h"
#include "widgets/misc/e-dateedit.h"

#define E_MAIL_TAG_EDITOR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_TAG_EDITOR, EMailTagEditorPrivate))

#define DEFAULT_FLAG 2  /* "Follow-Up" */

struct _EMailTagEditorPrivate {
	GtkTreeView *message_list;
	GtkComboBox *combo_entry;
	EDateEdit *target_date;

	gboolean completed;
	time_t completed_date;

	/* EDateEdit settings */
	gint week_start_day;
	gboolean use_24_hour_format;
};

enum {
	PROP_0,
	PROP_COMPLETED,
	PROP_USE_24_HOUR_FORMAT,
	PROP_WEEK_START_DAY
};

enum {
	COLUMN_FROM,
	COLUMN_SUBJECT
};

static gpointer parent_class;

static void
mail_tag_editor_clear_clicked_cb (EMailTagEditor *editor)
{
	gtk_combo_box_set_active (editor->priv->combo_entry, DEFAULT_FLAG);
	e_date_edit_set_time (editor->priv->target_date, (time_t) -1);
	e_mail_tag_editor_set_completed (editor, FALSE);
}

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

		case PROP_USE_24_HOUR_FORMAT:
			e_mail_tag_editor_set_use_24_hour_format (
				E_MAIL_TAG_EDITOR (object),
				g_value_get_boolean (value));
			return;

		case PROP_WEEK_START_DAY:
			e_mail_tag_editor_set_week_start_day (
				E_MAIL_TAG_EDITOR (object),
				g_value_get_int (value));
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

		case PROP_USE_24_HOUR_FORMAT:
			g_value_set_boolean (
				value,
				e_mail_tag_editor_get_use_24_hour_format (
				E_MAIL_TAG_EDITOR (object)));
			return;

		case PROP_WEEK_START_DAY:
			g_value_set_int (
				value,
				e_mail_tag_editor_get_week_start_day (
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
	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	/* XXX Override GTK's brain-dead border width defaults. */

	action_area = gtk_dialog_get_action_area (GTK_DIALOG (widget));
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (widget));

	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
}

static CamelTag *
mail_tag_editor_get_tag_list (EMailTagEditor *editor)
{
	CamelTag *tag_list = NULL;
	time_t date;
	gchar *text;

	text = gtk_combo_box_get_active_text (editor->priv->combo_entry);
	camel_tag_set (&tag_list, "follow-up", text);
	g_free (text);

	date = e_date_edit_get_time (editor->priv->target_date);
	if (date != (time_t) -1) {
		text = camel_header_format_date (date, 0);
		camel_tag_set (&tag_list, "due-by", text);
		g_free (text);
	} else
		camel_tag_set (&tag_list, "due-by", "");

	if (e_mail_tag_editor_get_completed (editor)) {
		text = camel_header_format_date (
			editor->priv->completed_date, 0);
		camel_tag_set (&tag_list, "completed-on", text);
		g_free (text);
	} else
		camel_tag_set (&tag_list, "completed-on", "");

	return tag_list;
}

static void
mail_tag_editor_set_tag_list (EMailTagEditor *editor,
                              CamelTag *tag_list)
{
	GtkWidget *child;
	const gchar *text;
	time_t date;

	/* Extract the GtkEntry from the GtkComboBoxEntry. */
	child = gtk_bin_get_child (GTK_BIN (editor->priv->combo_entry));

	/* XXX This is kind of cheating.  Since we only store the
	 *     translated tag there's no sure-fire way to determine
	 *     the corresponding combo box index (e.g. the tag may
	 *     have been set while running in a different locale). */
	text = camel_tag_get (&tag_list, "follow-up");
	if (text != NULL)
		gtk_entry_set_text (GTK_ENTRY (child), text);

	text = camel_tag_get (&tag_list, "due-by");
	if (text != NULL && *text != '\0') {
		date = camel_header_decode_date (text, NULL);
		e_date_edit_set_time (editor->priv->target_date, date);
	} else
		e_date_edit_set_time (editor->priv->target_date, (time_t) -1);

	text = camel_tag_get (&tag_list, "completed-on");
	if (text != NULL && *text != '\0') {
		date = camel_header_decode_date (text, NULL);
		if (date != (time_t) 0) {
			e_mail_tag_editor_set_completed (editor, TRUE);
			editor->priv->completed_date = date;
		}
	}
}

static void
mail_tag_editor_class_init (EMailTagEditorClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailTagEditorPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_tag_editor_set_property;
	object_class->get_property = mail_tag_editor_get_property;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = mail_tag_editor_realize;

	class->get_tag_list = mail_tag_editor_get_tag_list;
	class->set_tag_list = mail_tag_editor_set_tag_list;

	g_object_class_install_property (
		object_class,
		PROP_COMPLETED,
		g_param_spec_boolean (
			"completed",
			"Completed",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_USE_24_HOUR_FORMAT,
		g_param_spec_boolean (
			"use-24-hour-format",
			"Use 24-Hour Format",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WEEK_START_DAY,
		g_param_spec_int (
			"week-start-day",
			"Week Start Day",
			NULL,
			0,  /* Monday */
			6,  /* Sunday */
			0,
			G_PARAM_READWRITE));
}

static void
mail_tag_editor_init (EMailTagEditor *editor)
{
	GtkBuilder *builder;
	GtkDialog *dialog;
	GtkWidget *widget;
	GtkWidget *content_area;
	GtkWindow *window;
	GtkCellRenderer *renderer;
	GtkListStore *store;

	editor->priv = E_MAIL_TAG_EDITOR_GET_PRIVATE (editor);

	dialog = GTK_DIALOG (editor);
	window = GTK_WINDOW (editor);

	gtk_window_set_default_size (window, 400, 500);
	gtk_window_set_title (window, _("Flag to Follow Up"));
	gtk_window_set_icon_name (window, "stock_mail-flag-for-followup");

	gtk_dialog_add_buttons (
		dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

	content_area = gtk_dialog_get_content_area (dialog);

	/* Load the rest of the UI from the builder file. */

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-dialogs.ui");

	widget = e_builder_get_widget (builder, "toplevel");
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 6);

	widget = e_builder_get_widget (builder, "pixmap");
	e_binding_new (window, "icon-name", widget, "icon-name");

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
	gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (widget), 0);
	editor->priv->combo_entry = GTK_COMBO_BOX (widget);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), DEFAULT_FLAG);

	widget = e_builder_get_widget (builder, "target_date");
	editor->priv->target_date = E_DATE_EDIT (widget);
	e_binding_new (
		editor, "use-24-hour-format",
		widget, "use-24-hour-format");
	e_binding_new (
		editor, "week-start-day",
		widget, "week-start-day");

	widget = e_builder_get_widget (builder, "completed");
	e_mutual_binding_new (editor, "completed", widget, "active");

	widget = e_builder_get_widget (builder, "clear");
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (mail_tag_editor_clear_clicked_cb), editor);

	g_object_unref (builder);
}

GType
e_mail_tag_editor_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailTagEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_tag_editor_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailTagEditor),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_tag_editor_init,
			NULL   /* valute_table */
		};

		type = g_type_register_static (
			GTK_TYPE_DIALOG, "EMailTagEditor", &type_info, 0);
	}

	return type;
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

	if (completed == editor->priv->completed)
		return;

	editor->priv->completed = completed;
	editor->priv->completed_date = completed ? time (NULL) : 0;

	g_object_notify (G_OBJECT (editor), "completed");
}

gint
e_mail_tag_editor_get_week_start_day (EMailTagEditor *editor)
{
	g_return_val_if_fail (E_IS_MAIL_TAG_EDITOR (editor), 1);

	return editor->priv->week_start_day;
}

void
e_mail_tag_editor_set_week_start_day (EMailTagEditor *editor,
                                      gint week_start_day)
{
	g_return_if_fail (E_IS_MAIL_TAG_EDITOR (editor));
	g_return_if_fail (week_start_day >= 0 && week_start_day < 7);

	editor->priv->week_start_day = week_start_day;

	g_object_notify (G_OBJECT (editor), "week-start-day");
}

gboolean
e_mail_tag_editor_get_use_24_hour_format (EMailTagEditor *editor)
{
	g_return_val_if_fail (E_IS_MAIL_TAG_EDITOR (editor), TRUE);

	return editor->priv->use_24_hour_format;
}

void
e_mail_tag_editor_set_use_24_hour_format (EMailTagEditor *editor,
                                          gboolean use_24_hour_format)
{
	g_return_if_fail (E_IS_MAIL_TAG_EDITOR (editor));

	if (use_24_hour_format == editor->priv->use_24_hour_format)
		return;

	editor->priv->use_24_hour_format = use_24_hour_format;

	g_object_notify (G_OBJECT (editor), "use-24-hour-format");
}

CamelTag *
e_mail_tag_editor_get_tag_list (EMailTagEditor *editor)
{
	EMailTagEditorClass *class;

	g_return_val_if_fail (E_IS_MAIL_TAG_EDITOR (editor), NULL);

	class = E_MAIL_TAG_EDITOR_GET_CLASS (editor);
	g_return_val_if_fail (class->get_tag_list != NULL, NULL);

	return class->get_tag_list (editor);
}

void
e_mail_tag_editor_set_tag_list (EMailTagEditor *editor,
                                CamelTag *tag_list)
{
	EMailTagEditorClass *class;

	g_return_if_fail (E_IS_MAIL_TAG_EDITOR (editor));
	g_return_if_fail (tag_list != NULL);

	class = E_MAIL_TAG_EDITOR_GET_CLASS (editor);
	g_return_if_fail (class->set_tag_list != NULL);

	class->set_tag_list (editor, tag_list);
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
