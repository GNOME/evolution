/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#include <gtk/gtkframe.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include "filter-editor.h"

#define d(x)

static FilterRule * create_rule(RuleEditor *re);

static void filter_editor_class_init (FilterEditorClass *klass);
static void filter_editor_init (FilterEditor *fe);
static void filter_editor_finalise (GObject *obj);


static RuleEditorClass *parent_class = NULL;


GtkType
filter_editor_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		static const GtkTypeInfo info = {
			"FilterEditor",
			sizeof (FilterEditor),
			sizeof (FilterEditorClass),
			(GtkClassInitFunc) filter_editor_class_init,
			(GtkObjectInitFunc) filter_editor_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (RULE_TYPE_EDITOR, &info);
	}
	
	return type;
}

static void
filter_editor_class_init (FilterEditorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	RuleEditorClass *re_class = (RuleEditorClass *) klass;
	
	parent_class = gtk_type_class (rule_editor_get_type ());
	
	gobject_class->finalize = filter_editor_finalise;
	
	/* override methods */
	re_class->create_rule = create_rule;
}

static void
filter_editor_init (FilterEditor *fe)
{
	;
}

static void
filter_editor_finalise (GObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_editor_new:
 *
 * Create a new FilterEditor object.
 * 
 * Return value: A new #FilterEditor object.
 **/
FilterEditor *
filter_editor_new (FilterContext *fc, const char **source_names)
{
	FilterEditor *fe = (FilterEditor *) gtk_type_new (filter_editor_get_type ());
	GladeXML *gui;
	GtkWidget *w;
	
	gui = glade_xml_new (FILTER_GLADEDIR "/filter.glade", "rule_editor");
	filter_editor_construct (fe, fc, gui, source_names);
	
        w = glade_xml_get_widget (gui, "rule_frame");
	gtk_frame_set_label (GTK_FRAME (w), _("Filter Rules"));
	
	g_object_unref (gui);
	
	return fe;
}

static void
select_source (GtkMenuItem *mi, FilterEditor *fe)
{
	char *source;
	
	source = gtk_object_get_data (GTK_OBJECT (mi), "source");
	g_assert (source);
	
	rule_editor_set_source ((RuleEditor *)fe, source);
}

void
filter_editor_construct (FilterEditor *fe, FilterContext *fc, GladeXML *gui, const char **source_names)
{
	GtkWidget *menu, *item, *omenu;
	int i;
	
        omenu = glade_xml_get_widget (gui, "filter_source");
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (omenu));
	menu = gtk_menu_new ();
	
	for (i = 0; source_names[i]; i++) {
		item = gtk_menu_item_new_with_label (_(source_names[i]));
		g_object_set_data_full (item, "source", g_strdup (source_names[i]), g_free);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
		g_signal_connect (item, "activate", select_source, fe);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_widget_show (omenu);
	
	rule_editor_construct ((RuleEditor *)fe, (RuleContext *)fc, gui, source_names[0]);
}

static FilterRule *
create_rule (RuleEditor *re)
{
	FilterRule *rule = filter_rule_new ();
	FilterPart *part;
	
	/* create a rule with 1 part & 1 action in it */
	rule = (FilterRule *)filter_filter_new ();
	part = rule_context_next_part (re->context, NULL);
	filter_rule_add_part (rule, filter_part_clone (part));
	part = filter_context_next_action ((FilterContext *)re->context, NULL);
	filter_filter_add_action ((FilterFilter *)rule, filter_part_clone (part));
	
	return rule;
}
