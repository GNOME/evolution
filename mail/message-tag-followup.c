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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <glib/gi18n.h>

#include "e-util/e-util-private.h"

#include "misc/e-dateedit.h"

#include "message-tag-followup.h"
#include "mail-config.h"

static void message_tag_followup_class_init (MessageTagFollowUpClass *class);
static void message_tag_followup_init (MessageTagFollowUp *followup);
static void message_tag_followup_finalise (GObject *obj);

static CamelTag *get_tag_list (MessageTagEditor *editor);
static void set_tag_list (MessageTagEditor *editor, CamelTag *tags);

#define DEFAULT_FLAG  2  /* Follow-Up */
static const gchar *available_flags[] = {
	N_("Call"),
	N_("Do Not Forward"),
	N_("Follow-Up"),
	N_("For Your Information"),
	N_("Forward"),
	N_("No Response Necessary"),
	/* Translators: "Read" as in "has been read" (message-tag-followup.c) */
	N_("Read"),
	N_("Reply"),
	N_("Reply to All"),
	N_("Review"),
};

static gint num_available_flags = sizeof (available_flags) / sizeof (available_flags[0]);

static MessageTagEditorClass *parent_class = NULL;

GType
message_tag_followup_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (MessageTagFollowUpClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) message_tag_followup_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (MessageTagFollowUp),
			0,
			(GInstanceInitFunc) message_tag_followup_init,
		};

		type = g_type_register_static (message_tag_editor_get_type (), "MessageTagFollowUp", &info, 0);
	}

	return type;
}

static void
message_tag_followup_class_init (MessageTagFollowUpClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	MessageTagEditorClass *editor_class = (MessageTagEditorClass *) klass;

	parent_class = g_type_class_ref (message_tag_editor_get_type ());

	object_class->finalize = message_tag_followup_finalise;

	editor_class->get_tag_list = get_tag_list;
	editor_class->set_tag_list = set_tag_list;
}

static void
message_tag_followup_init (MessageTagFollowUp *editor)
{
	editor->combo_entry = NULL;
	editor->target_date = NULL;
	editor->completed = NULL;
	editor->clear = NULL;
	editor->completed_date = 0;
}

static void
message_tag_followup_finalise (GObject *obj)
{
	MessageTagFollowUp *editor = (MessageTagFollowUp *) obj;

	editor->completed_date = 0;

        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static CamelTag *
get_tag_list (MessageTagEditor *editor)
{
	MessageTagFollowUp *followup = (MessageTagFollowUp *) editor;
	CamelTag *tags = NULL;
	time_t date;
	gchar *text;

	camel_tag_set (&tags, "follow-up", gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (followup->combo_entry)))));

	date = e_date_edit_get_time (followup->target_date);
	if (date != (time_t) -1) {
		text = camel_header_format_date (date, 0);
		camel_tag_set (&tags, "due-by", text);
		g_free (text);
	} else {
		camel_tag_set (&tags, "due-by", "");
	}

	if (gtk_toggle_button_get_active (followup->completed)) {
		text = camel_header_format_date (followup->completed_date, 0);
		camel_tag_set (&tags, "completed-on", text);
		g_free (text);
	} else {
		camel_tag_set (&tags, "completed-on", "");
	}

	return tags;
}

static void
set_tag_list (MessageTagEditor *editor, CamelTag *tags)
{
	MessageTagFollowUp *followup = (MessageTagFollowUp *) editor;
	const gchar *text;
	time_t date;

	text = camel_tag_get (&tags, "follow-up");
	if (text)
		gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (followup->combo_entry))), text);

	text = camel_tag_get (&tags, "due-by");
	if (text && *text) {
		date = camel_header_decode_date (text, NULL);
		e_date_edit_set_time (followup->target_date, date);
	} else {
		e_date_edit_set_time (followup->target_date, (time_t) -1);
	}

	text = camel_tag_get (&tags, "completed-on");
	if (text && *text) {
		date = camel_header_decode_date (text, NULL);
		if (date != (time_t) 0) {
			gtk_toggle_button_set_active (followup->completed, TRUE);
			followup->completed_date = date;
		}
	}
}

static void
clear_clicked (GtkButton *button, gpointer user_data)
{
	MessageTagFollowUp *followup = user_data;

	gtk_combo_box_set_active (followup->combo_entry, DEFAULT_FLAG);

	e_date_edit_set_time (followup->target_date, (time_t) -1);
	gtk_toggle_button_set_active (followup->completed, FALSE);
}

static void
completed_toggled (GtkToggleButton *button, gpointer user_data)
{
	MessageTagFollowUp *followup = user_data;

	if (gtk_toggle_button_get_active (followup->completed))
		followup->completed_date = time (NULL);
	else
		followup->completed_date = 0;
}

static gint
get_week_start_day (void)
{
	GConfClient *gconf;

	gconf = mail_config_get_gconf_client ();
	return gconf_client_get_int (gconf, "/apps/evolution/calendar/display/week_start_day", NULL);
}

static gboolean
locale_supports_12_hour_format (void)
{
	gchar s[16];
	time_t t = 0;

	strftime(s, sizeof s, "%p", gmtime (&t));
	return s[0] != '\0';
}

GtkWidget *target_date_new (const gchar *s1, const gchar *s2, gint i1, gint i2);

GtkWidget *
target_date_new (const gchar *s1, const gchar *s2, gint i1, gint i2)
{
	gboolean time_24hour = TRUE;
	GConfClient *gconf;
	GtkWidget *widget;
	gint start;

	widget = e_date_edit_new ();
	e_date_edit_set_show_date (E_DATE_EDIT (widget), TRUE);
	e_date_edit_set_show_time (E_DATE_EDIT (widget), TRUE);

	/* Note that this is 0 (Sun) to 6 (Sat), conver to 0 (mon) to 6 (sun) */
	start = (get_week_start_day () + 6) % 7;

	if (locale_supports_12_hour_format ()) {
		gconf = mail_config_get_gconf_client ();
		time_24hour = gconf_client_get_bool (gconf, "/apps/evolution/calendar/display/use_24hour_format", NULL);
	}

	e_date_edit_set_week_start_day (E_DATE_EDIT (widget), start);
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (widget), time_24hour);
	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (widget), TRUE);
	e_date_edit_set_time_popup_range (E_DATE_EDIT (widget), 0, 24);

	return widget;
}

static void
construct (MessageTagEditor *editor)
{
	MessageTagFollowUp *followup = (MessageTagFollowUp *) editor;
	GtkCellRenderer *renderer;
	GtkListStore *model;
	GtkWidget *widget;
	GladeXML *gui;
	gint i;
	gchar *gladefile;

	gtk_window_set_title (GTK_WINDOW (editor), _("Flag to Follow Up"));

	gtk_window_set_icon_name (
		GTK_WINDOW (editor), "stock_mail-flag-for-followup");

	gtk_dialog_set_has_separator (GTK_DIALOG (editor), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (editor)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (editor)->action_area), 12);

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-dialogs.glade",
				      NULL);
	gui = glade_xml_new (gladefile, "followup_editor", NULL);
	g_free (gladefile);

	widget = glade_xml_get_widget (gui, "toplevel");

	/* reparent */
	gtk_widget_reparent (widget, GTK_DIALOG (editor)->vbox);
	gtk_box_set_child_packing (GTK_BOX (GTK_DIALOG (editor)->vbox), widget, TRUE, TRUE, 6, GTK_PACK_START);

	followup->message_list = GTK_TREE_VIEW (glade_xml_get_widget (gui, "message_list"));
	model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model (followup->message_list, (GtkTreeModel *) model);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (followup->message_list, -1, _("From"),
						     renderer, "text", 0, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (followup->message_list, -1, _("Subject"),
						     renderer, "text", 1, NULL);

	followup->combo_entry = GTK_COMBO_BOX (glade_xml_get_widget (gui, "combo"));
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (followup->combo_entry)));
	for (i = 0; i < num_available_flags; i++)
		gtk_combo_box_append_text (followup->combo_entry, (gchar *) _(available_flags[i]));
	gtk_combo_box_set_active (followup->combo_entry, DEFAULT_FLAG);

	followup->target_date = E_DATE_EDIT (glade_xml_get_widget (gui, "target_date"));
	/* glade bug, need to show this ourselves */
	gtk_widget_show ((GtkWidget *) followup->target_date);
	e_date_edit_set_time (followup->target_date, (time_t) -1);

	followup->completed = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "completed"));
	g_signal_connect (followup->completed, "toggled", G_CALLBACK (completed_toggled), followup);

	followup->clear = GTK_BUTTON (glade_xml_get_widget (gui, "clear"));
	g_signal_connect (followup->clear, "clicked", G_CALLBACK (clear_clicked), followup);

	g_object_unref (gui);
}

MessageTagEditor *
message_tag_followup_new (void)
{
	MessageTagEditor *editor;

	editor = (MessageTagEditor *) g_object_new (message_tag_followup_get_type (), NULL);
	construct (editor);

	return editor;
}

void
message_tag_followup_append_message (MessageTagFollowUp *editor, const gchar *from, const gchar *subject)
{
	GtkTreeIter iter;
	GtkListStore *model;

	g_return_if_fail (IS_MESSAGE_TAG_FOLLOWUP (editor));

	model = (GtkListStore *) gtk_tree_view_get_model (editor->message_list);

	gtk_list_store_append (model, &iter);
	gtk_list_store_set (model, &iter, 0, from, 1, subject, -1);
}
