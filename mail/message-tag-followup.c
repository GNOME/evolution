/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximain, Inc. (www.ximian.com)
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

#include "message-tag-followup.h"

static void message_tag_followup_class_init (MessageTagFollowUpClass *class);
static void message_tag_followup_init (MessageTagFollowUp *followup);
static void message_tag_followup_finalise (GtkObject *obj);

static const char *tag_get_name  (MessageTagEditor *editor);
static const char *tag_get_value (MessageTagEditor *editor);
static void tag_set_value (MessageTagEditor *editor, const char *value);


static struct {
	const char *i18n_name;
	const char *name;
	int value;
} available_flags[] = {
	{ N_("Call"),                  "call",           FOLLOWUP_FLAG_CALL },
	{ N_("Do Not Forward"),        "do-not-forward", FOLLOWUP_FLAG_DO_NOT_FORWARD },
	{ N_("Follow-Up"),             "follow-up",      FOLLOWUP_FLAG_FOLLOWUP },
	{ N_("For Your Information"),  "fyi",            FOLLOWUP_FLAG_FYI },
	{ N_("Forward"),               "forward",        FOLLOWUP_FLAG_FORWARD },
	{ N_("No Response Necessary"), "no-response",    FOLLOWUP_FLAG_NO_RESPONSE_NECESSARY },
	{ N_("Read"),                  "read",           FOLLOWUP_FLAG_READ },
	{ N_("Reply"),                 "reply",          FOLLOWUP_FLAG_REPLY },
	{ N_("Reply to All"),          "reply-all",      FOLLOWUP_FLAG_REPLY_ALL },
	{ N_("Review"),                "review",         FOLLOWUP_FLAG_REVIEW },
	{ N_("None"),                  NULL,             FOLLOWUP_FLAG_NONE }
};

static MessageTagEditorClass *parent_class;

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
	
	editor_class->get_name = tag_get_name;
	editor_class->get_value = tag_get_value;
	editor_class->set_value = tag_set_value;
}

static void
message_tag_followup_init (MessageTagFollowUp *editor)
{
	editor->tag = g_new (struct _FollowUpTag, 1);
	editor->tag->type = FOLLOWUP_FLAG_NONE;
	editor->tag->target_date = time (NULL);
	editor->tag->completed = 0;
	
	editor->value = NULL;
	
	editor->type = NULL;
	editor->none = NULL;
	editor->target_date = NULL;
	editor->completed = NULL;
	editor->clear = NULL;
}


static void
message_tag_followup_finalise (GtkObject *obj)
{
	MessageTagFollowUp *editor = (MessageTagFollowUp *) obj;
	
	g_free (editor->tag);
	g_free (editor->value);
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}


static const char *
tag_get_name (MessageTagEditor *editor)
{
	return "follow-up";
}

static const char *
tag_get_value (MessageTagEditor *editor)
{
	MessageTagFollowUp *followup = (MessageTagFollowUp *) editor;
	
	g_free (followup->value);
	followup->value = message_tag_followup_encode (followup->tag);
	
	return followup->value;
}

static void
set_widget_values (MessageTagFollowUp *followup)
{
	time_t completed;
	GtkWidget *item;
	GList *items;
	
	items = GTK_MENU_SHELL (followup->type)->children;
	item = g_list_nth_data (items, followup->tag->type);
	
	gtk_option_menu_set_history (followup->type, followup->tag->type);
	gtk_signal_emit_by_name (GTK_OBJECT (followup->type), "activate", followup);
	
	e_date_edit_set_time (followup->target_date, followup->tag->target_date);
	
	completed = followup->tag->completed;
	gtk_toggle_button_set_active (followup->completed, completed ? TRUE : FALSE);
	if (completed)
		followup->tag->completed = completed;
}

static void
tag_set_value (MessageTagEditor *editor, const char *value)
{
	MessageTagFollowUp *followup = (MessageTagFollowUp *) editor;
	
	g_free (followup->tag);
	followup->tag = message_tag_followup_decode (value);
	
	set_widget_values (followup);
}


struct _FollowUpTag *
message_tag_followup_decode (const char *value)
{
	struct _FollowUpTag *tag;
	const char *inptr;
	int i;
	
	tag = g_new (struct _FollowUpTag, 1);
	
	for (i = 0; i < FOLLOWUP_FLAG_NONE; i++) {
		if (!strncmp (value, available_flags[i].name, strlen (available_flags[i].name)))
			break;
	}
	
	tag->type = i;
	
	inptr = value + strlen (available_flags[i].name);
	
	if (*inptr == ':') {
		inptr++;
		tag->target_date = strtoul (inptr, (char **) &inptr, 16);
		if (*inptr == ':') {
			inptr++;
			tag->completed = strtoul (inptr, (char **) &inptr, 16);
		} else
			tag->completed = 0;
	} else {
		tag->target_date = time (NULL);
		tag->completed = 0;
	}
	
	return tag;
}


char *
message_tag_followup_encode (struct _FollowUpTag *tag)
{
	g_return_val_if_fail (tag != NULL, NULL);
	
	if (tag->type == FOLLOWUP_FLAG_NONE)
		return NULL;
	
	return g_strdup_printf ("%s:%lx:%lx", available_flags[tag->type].name,
				(unsigned long) tag->target_date,
				(unsigned long) tag->completed);
}

const char *
message_tag_followup_i18n_name (int type)
{
	g_return_val_if_fail (type >= 0 && type <= FOLLOWUP_FLAG_NONE, NULL);
	
	if (type != FOLLOWUP_FLAG_NONE)
		return _(available_flags[type].i18n_name);
	else
		return NULL;
}

static void
clear_clicked (GtkButton *button, gpointer user_data)
{
	MessageTagFollowUp *followup = user_data;
	
	gtk_widget_show (followup->none);
	gtk_option_menu_set_history (followup->type, FOLLOWUP_FLAG_NONE);
	gtk_signal_emit_by_name (GTK_OBJECT (followup->none), "activate", followup);
	
	e_date_edit_set_time (followup->target_date, time (NULL));
	gtk_toggle_button_set_active (followup->completed, FALSE);
}

static void
completed_toggled (GtkToggleButton *button, gpointer user_data)
{
	MessageTagFollowUp *followup = user_data;
	
	if (gtk_toggle_button_get_active (followup->completed))
		followup->tag->completed = time (NULL);
	else
		followup->tag->completed = 0;
}

static void
type_changed (GtkWidget *item, gpointer user_data)
{
	MessageTagFollowUp *followup = user_data;
	
	followup->tag->type = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "value"));
	
	if (item != followup->none)
		gtk_widget_hide (followup->none);
}

static void
target_date_changed (EDateEdit *widget, gpointer user_data)
{
	MessageTagFollowUp *followup = user_data;
	
	followup->tag->target_date = e_date_edit_get_time (widget);
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
	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (widget), FALSE);
	e_date_edit_set_time_popup_range (E_DATE_EDIT (widget), 0, 24);
	
	return widget;
}

static void
construct (MessageTagEditor *editor)
{
	MessageTagFollowUp *followup = (MessageTagFollowUp *) editor;
	GtkWidget *widget, *menu, *item;
	GladeXML *gui;
	int i;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/message-tags.glade", "followup_editor");
	
	widget = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_reparent (widget, GNOME_DIALOG (editor)->vbox);
	
	followup->type = GTK_OPTION_MENU (glade_xml_get_widget (gui, "followup_type"));
	gtk_option_menu_remove_menu (followup->type);
	menu = gtk_menu_new ();
	for (i = 0; i <= FOLLOWUP_FLAG_NONE; i++) {
		item = gtk_menu_item_new_with_label (_(available_flags[i].i18n_name));
		gtk_object_set_data (GTK_OBJECT (item), "value",
				     GINT_TO_POINTER (available_flags[i].value));
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    type_changed, followup);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
	}
	followup->none = item;
	gtk_option_menu_set_menu (followup->type, menu);
	gtk_signal_emit_by_name (GTK_OBJECT (item), "activate", followup);
	gtk_option_menu_set_history (followup->type, FOLLOWUP_FLAG_NONE);
	
	followup->target_date = E_DATE_EDIT (glade_xml_get_widget (gui, "target_date"));
	e_date_edit_set_time (followup->target_date, time (NULL));
	gtk_signal_connect (GTK_OBJECT (followup->target_date), "changed",
			    target_date_changed, followup);
	
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
