/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glade/glade.h>

#include <libgnome/gnome-defs.h>
#include <libgnomeui/gnome-window-icon.h>
#include <libgnomeui/gnome-pixmap.h>

#include <gal/widgets/e-unicode.h>
#include <gal/util/e-unicode-i18n.h>

#include "message-tag-followup.h"

static void message_tag_followup_class_init (MessageTagFollowUpClass *class);
static void message_tag_followup_init (MessageTagFollowUp *followup);
static void message_tag_followup_finalise (GtkObject *obj);

static CamelTag *get_tag_list (MessageTagEditor *editor);
static void set_tag_list (MessageTagEditor *editor, CamelTag *tags);


#define DEFAULT_FLAG  2  /* Follow-Up */
static char *available_flags[] = {
	N_("Call"),
	N_("Do Not Forward"),
	N_("Follow-Up"),
	N_("For Your Information"),
	N_("Forward"),
	N_("No Response Necessary"),
	N_("Read"),
	N_("Reply"),
	N_("Reply to All"),
	N_("Review"),
};

static int num_available_flags = sizeof (available_flags) / sizeof (available_flags[0]);


static MessageTagEditorClass *parent_class = NULL;


GtkType
message_tag_followup_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MessageTagFollowUp",
			sizeof (MessageTagFollowUp),
			sizeof (MessageTagFollowUpClass),
			(GtkClassInitFunc) message_tag_followup_class_init,
			(GtkObjectInitFunc) message_tag_followup_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (message_tag_editor_get_type (), &type_info);
	}
	
	return type;
}

static void
message_tag_followup_class_init (MessageTagFollowUpClass *klass)
{
	GtkObjectClass *object_class;
	MessageTagEditorClass *editor_class;
	
	object_class = (GtkObjectClass *) klass;
	editor_class = (MessageTagEditorClass *) klass;
	parent_class = gtk_type_class (message_tag_editor_get_type ());
	
	object_class->finalize = message_tag_followup_finalise;
	
	editor_class->get_tag_list = get_tag_list;
	editor_class->set_tag_list = set_tag_list;
}

static void
message_tag_followup_init (MessageTagFollowUp *editor)
{
	editor->combo = NULL;
	editor->target_date = NULL;
	editor->completed = NULL;
	editor->clear = NULL;
	editor->completed_date = 0;
}


static void
message_tag_followup_finalise (GtkObject *obj)
{
	MessageTagFollowUp *editor = (MessageTagFollowUp *) obj;
	
	editor->completed_date = 0;
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}


static CamelTag *
get_tag_list (MessageTagEditor *editor)
{
	MessageTagFollowUp *followup = (MessageTagFollowUp *) editor;
	CamelTag *tags = NULL;
	time_t date;
	char *text;
	
	text = e_utf8_gtk_entry_get_text (GTK_ENTRY (followup->combo->entry));
	camel_tag_set (&tags, "follow-up", text);
	g_free (text);
	
	date = e_date_edit_get_time (followup->target_date);
	if (date != (time_t) -1) {
		text = header_format_date (date, 0);
		camel_tag_set (&tags, "due-by", text);
		g_free (text);
	} else {
		camel_tag_set (&tags, "due-by", "");
	}
	
	if (gtk_toggle_button_get_active (followup->completed)) {
		text = header_format_date (followup->completed_date, 0);
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
	const char *text;
	time_t date;
	
	text = camel_tag_get (&tags, "follow-up");
	if (text)
		e_utf8_gtk_entry_set_text (GTK_ENTRY (followup->combo->entry), text);
	
	text = camel_tag_get (&tags, "due-by");
	if (text && *text) {
		date = header_decode_date (text, NULL);
		e_date_edit_set_time (followup->target_date, date);
	} else {
		e_date_edit_set_time (followup->target_date, (time_t) -1);
	}
	
	text = camel_tag_get (&tags, "completed-on");
	if (text && *text) {
		date = header_decode_date (text, NULL);
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
	
	gtk_list_select_item (GTK_LIST (followup->combo->list), DEFAULT_FLAG);
	
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

GtkWidget *target_date_new (const char *s1, const char *s2, int i1, int i2);

GtkWidget *
target_date_new (const char *s1, const char *s2, int i1, int i2)
{
	GtkWidget *widget;
	
	widget = e_date_edit_new ();
	e_date_edit_set_show_date (E_DATE_EDIT (widget), TRUE);
	e_date_edit_set_show_time (E_DATE_EDIT (widget), TRUE);
	e_date_edit_set_week_start_day (E_DATE_EDIT (widget), 6);
	/* FIXME: make this locale dependant?? */
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (widget), FALSE);
	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (widget), TRUE);
	e_date_edit_set_time_popup_range (E_DATE_EDIT (widget), 0, 24);
	
	return widget;
}

static void
construct (MessageTagEditor *editor)
{
	MessageTagFollowUp *followup = (MessageTagFollowUp *) editor;
	GtkWidget *widget;
	GList *strings;
	GladeXML *gui;
	int i;
	
	gtk_window_set_title (GTK_WINDOW (editor), _("Flag to Follow Up"));
	gnome_window_icon_set_from_file (GTK_WINDOW (editor), EVOLUTION_IMAGES "/flag-for-followup-16.png");
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/message-tags.glade", "followup_editor");
	
	widget = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_reparent (widget, GNOME_DIALOG (editor)->vbox);
	
	widget = glade_xml_get_widget (gui, "pixmap");
	gnome_pixmap_load_file (GNOME_PIXMAP (widget), EVOLUTION_GLADEDIR "/flag-for-followup-48.png");
	
	followup->message_list = GTK_CLIST (glade_xml_get_widget (gui, "message_list"));
	
	followup->combo = GTK_COMBO (glade_xml_get_widget (gui, "combo"));
	gtk_combo_set_case_sensitive (followup->combo, FALSE);
	strings = NULL;
	for (i = 0; i < num_available_flags; i++)
		strings = g_list_append (strings, (char *) _(available_flags[i]));
	gtk_combo_set_popdown_strings (followup->combo, strings);
	g_list_free (strings);
	gtk_list_select_item (GTK_LIST (followup->combo->list), DEFAULT_FLAG);
	
	followup->target_date = E_DATE_EDIT (glade_xml_get_widget (gui, "target_date"));
	e_date_edit_set_time (followup->target_date, (time_t) -1);
	
	followup->completed = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "completed"));
	gtk_signal_connect (GTK_OBJECT (followup->completed), "toggled",
			    completed_toggled, followup);
	
	followup->clear = GTK_BUTTON (glade_xml_get_widget (gui, "clear"));
	gtk_signal_connect (GTK_OBJECT (followup->clear), "clicked",
			    clear_clicked, followup);
	
	gtk_object_unref (GTK_OBJECT (gui));
}

MessageTagEditor *
message_tag_followup_new (void)
{
	MessageTagEditor *editor;
	
	editor = (MessageTagEditor *) gtk_type_new (message_tag_followup_get_type ());
	construct (editor);
	
	return editor;
}

void
message_tag_followup_append_message (MessageTagFollowUp *editor,
				     const char *from,
				     const char *subject)
{
	char *text[3];
	
	g_return_if_fail (IS_MESSAGE_TAG_FOLLOWUP (editor));
	
	text[0] = e_utf8_to_gtk_string (GTK_WIDGET (editor->message_list), from);
	text[1] = e_utf8_to_gtk_string (GTK_WIDGET (editor->message_list), subject);
	text[2] = NULL;
	
	gtk_clist_append (editor->message_list, text);
	
	g_free (text[0]);
	g_free (text[1]);
}
