/*
 *  Copyright (C) 2000, 2001 Ximian, Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkbox.h>
#include <libgnome/gnome-i18n.h>

#include "mail-search-dialogue.h"

static void mail_search_dialogue_class_init	(MailSearchDialogueClass *class);
static void mail_search_dialogue_init	(MailSearchDialogue *gspaper);
static void mail_search_dialogue_finalise	(GObject *obj);

static GtkDialogClass *parent_class;

GType
mail_search_dialogue_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (MailSearchDialogueClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) mail_search_dialogue_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (MailSearchDialogue),
			0,
			(GInstanceInitFunc) mail_search_dialogue_init,
		};
		
		type = g_type_register_static (gtk_dialog_get_type (), "MailSearchDialogue", &info, 0);
	}
	
	return type;
}

static void
mail_search_dialogue_class_init (MailSearchDialogueClass *class)
{
	GObjectClass *object_class;
	
	object_class = (GObjectClass *)class;
	parent_class = g_type_class_ref(gtk_dialog_get_type ());

	object_class->finalize = mail_search_dialogue_finalise;
	/* override methods */

}

static void
mail_search_dialogue_construct (MailSearchDialogue *o, FilterRule *rule)
{
	FilterPart *part;
	GtkDialog *dialogue = GTK_DIALOG (o);
	
	g_object_set(dialogue,
		     "allow_shrink", FALSE,
		     "allow_grow", TRUE,
		     "default_width", 500,
		     "default_height", 400, NULL);
	
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
	GtkDialog *dialogue = GTK_DIALOG (o);
	
	gtk_dialog_add_buttons (dialogue,
				GTK_STOCK_OK,
				GTK_RESPONSE_OK,
				_("_Search"),
				GTK_RESPONSE_APPLY,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL,
				NULL);
	gtk_dialog_set_default_response (dialogue, GTK_RESPONSE_OK);
}


static void
mail_search_dialogue_finalise (GObject *obj)
{
	MailSearchDialogue *o = (MailSearchDialogue *)obj;
	
	if (o->context)
		g_object_unref(o->context);
	if (o->rule)
		g_object_unref(o->rule);
	
        ((GObjectClass *)(parent_class))->finalize(obj);
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
	MailSearchDialogue *o = (MailSearchDialogue *)g_object_new(mail_search_dialogue_get_type(), NULL);
	mail_search_dialogue_construct (o, NULL);
	return o;
}

MailSearchDialogue *
mail_search_dialogue_new_with_rule (FilterRule *rule)
{
	MailSearchDialogue *o = (MailSearchDialogue *)g_object_new (mail_search_dialogue_get_type (), NULL);
	if (rule)
		g_object_ref((rule));
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
