/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
#include <gtk/gtk.h>
#include <gnome.h>

#include "mail-search-dialogue.h"

static void mail_search_dialogue_class_init	(MailSearchDialogueClass *class);
static void mail_search_dialogue_init	(MailSearchDialogue *gspaper);
static void mail_search_dialogue_finalise	(GtkObject *obj);

static GnomeDialogClass *parent_class;

guint
mail_search_dialogue_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailSearchDialogue",
			sizeof(MailSearchDialogue),
			sizeof(MailSearchDialogueClass),
			(GtkClassInitFunc)mail_search_dialogue_class_init,
			(GtkObjectInitFunc)mail_search_dialogue_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique (gnome_dialog_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_search_dialogue_class_init (MailSearchDialogueClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class (gnome_dialog_get_type ());

	object_class->finalize = mail_search_dialogue_finalise;
	/* override methods */

}

static void
mail_search_dialogue_construct (MailSearchDialogue *o, FilterRule *rule)
{
	FilterPart *part;
	GnomeDialog *dialogue = GNOME_DIALOG (o);
	
	gtk_window_set_policy (GTK_WINDOW (dialogue), FALSE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (dialogue), 500, 400);
	
	o->context = rule_context_new ();
	rule_context_add_part_set (o->context, "partset", filter_part_get_type (),
				   rule_context_add_part, rule_context_next_part);
	rule_context_load (o->context, EVOLUTION_DATADIR "/evolution/vfoldertypes.xml", "");
	if (rule) {
		o->rule = rule;
		o->guts = filter_rule_get_widget (o->rule, o->context);
	} else {
		o->rule = filter_rule_new ();
		part = rule_context_next_part (o->context, NULL);
		if (part == NULL) {
			g_warning ("Problem loading search: no parts to load");
			o->guts = gtk_entry_new ();
		} else {
			filter_rule_add_part (o->rule, filter_part_clone (part));
			o->guts = filter_rule_get_widget (o->rule, o->context);
		}
	}
	
	gtk_widget_show (o->guts);
	gtk_box_pack_start (GTK_BOX (dialogue->vbox), o->guts, TRUE, TRUE, 0);
}

static void
mail_search_dialogue_init (MailSearchDialogue *o)
{
	GnomeDialog *dialogue = GNOME_DIALOG (o);
	
	gnome_dialog_append_buttons (dialogue,
				     GNOME_STOCK_BUTTON_OK,
				     _("_Search"),
				     GNOME_STOCK_BUTTON_CANCEL,
				     NULL);
	gnome_dialog_set_default (dialogue, 0);
}


static void
mail_search_dialogue_finalise (GtkObject *obj)
{
	MailSearchDialogue *o = (MailSearchDialogue *)obj;
	
	if (o->context)
		gtk_object_unref (GTK_OBJECT (o->context));
	if (o->rule)
		gtk_object_unref (GTK_OBJECT (o->rule));
	
        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * mail_search_dialogue_new:
 *
 * Create a new MailSearchDialogue object.
 * 
 * Return value: A new #MailSearchDialogue object.
 **/
MailSearchDialogue *
mail_search_dialogue_new ()
{
	MailSearchDialogue *o = (MailSearchDialogue *)gtk_type_new (mail_search_dialogue_get_type ());
	mail_search_dialogue_construct (o, NULL);
	return o;
}

MailSearchDialogue *
mail_search_dialogue_new_with_rule (FilterRule *rule)
{
	MailSearchDialogue *o = (MailSearchDialogue *)gtk_type_new (mail_search_dialogue_get_type ());
	if (rule)
		gtk_object_ref (GTK_OBJECT (rule));
	mail_search_dialogue_construct (o, rule);
	return o;
}

/**
 * mail_search_dialogue_get_query:
 * @msd: 
 * 
 * Get the query string represting the current search criterea.
 * 
 * Return value: 
 **/
char *
mail_search_dialogue_get_query (MailSearchDialogue *msd)
{
	GString *out = g_string_new ("");
	char *ret;
	
	filter_rule_build_code (msd->rule, out);
	ret = out->str;
	g_string_free (out, FALSE);
	return ret;
}
