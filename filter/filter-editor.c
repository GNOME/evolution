/*
 *  Copyright (C) 2000, 2001 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@helixcode.com>
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
#include <gnome.h>
#include <glade/glade.h>

#include <gal/widgets/e-unicode.h>
#include "filter-editor.h"
#include "filter-context.h"
#include "filter-filter.h"

#define d(x)

static FilterRule * create_rule(RuleEditor *re);

static void filter_editor_class_init (FilterEditorClass *class);
static void filter_editor_init (FilterEditor *gspaper);
static void filter_editor_finalise (GtkObject *obj);

#define _PRIVATE(x) (((FilterEditor *)(x))->priv)

struct _FilterEditorPrivate {
};

static GnomeDialogClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_editor_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterEditor",
			sizeof(FilterEditor),
			sizeof(FilterEditorClass),
			(GtkClassInitFunc)filter_editor_class_init,
			(GtkObjectInitFunc)filter_editor_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique (rule_editor_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_editor_class_init (FilterEditorClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass *)class;
	RuleEditorClass *re_class = (RuleEditorClass *)class;

	parent_class = gtk_type_class(gnome_dialog_get_type ());
	
	object_class->finalize = filter_editor_finalise;

	/* override methods */
	re_class->create_rule = create_rule;

	/* signals */
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_editor_init (FilterEditor *o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
filter_editor_finalise (GtkObject *obj)
{
	FilterEditor *o = (FilterEditor *)obj;

	g_free(o->priv);

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_editor_new:
 *
 * Create a new FilterEditor object.
 * 
 * Return value: A new #FilterEditor object.
 **/
FilterEditor *
filter_editor_new(FilterContext *f, const char **source_names)
{
	FilterEditor *o = (FilterEditor *)gtk_type_new (filter_editor_get_type ());
	GladeXML *gui;
	GtkWidget *w;

	gui = glade_xml_new(FILTER_GLADEDIR "/filter.glade", "rule_editor");
	filter_editor_construct(o, f, gui, source_names);

        w = glade_xml_get_widget(gui, "rule_frame");
	gtk_frame_set_label((GtkFrame *)w, _("Filter Rules"));

	gtk_object_unref((GtkObject *)gui);

	return o;
}

static void
select_source (GtkMenuItem *mi, FilterEditor *fe)
{
	char *source;
	
	source = gtk_object_get_data(GTK_OBJECT(mi), "source");
	g_assert(source);

	rule_editor_set_source((RuleEditor *)fe, source);
}

void
filter_editor_construct(FilterEditor *fe, FilterContext *fc, GladeXML *gui, const char **source_names)
{
	GtkWidget *menu, *item, *omenu;
	int i;

        omenu = glade_xml_get_widget (gui, "filter_source");
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (omenu));
	menu = gtk_menu_new ();
	
	for (i=0;source_names[i];i++) {
		item = gtk_menu_item_new_with_label(_(source_names[i]));
		gtk_object_set_data_full(GTK_OBJECT(item), "source", g_strdup(source_names[i]), g_free);
		gtk_menu_append(GTK_MENU(menu), item);
		gtk_widget_show((GtkWidget *)item);
		gtk_signal_connect(GTK_OBJECT(item), "activate", select_source, fe);

	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(omenu), menu);
	gtk_widget_show((GtkWidget *)omenu);

	rule_editor_construct((RuleEditor *)fe, (RuleContext *)fc, gui, source_names[0]);
}

static FilterRule *
create_rule(RuleEditor *re)
{
	FilterRule *rule = filter_rule_new();
	FilterPart *part;

	/* create a rule with 1 part & 1 action in it */
	rule = (FilterRule *)filter_filter_new();
	part = rule_context_next_part(re->context, NULL);
	filter_rule_add_part(rule, filter_part_clone(part));
	part = filter_context_next_action ((FilterContext *)re->context, NULL);
	filter_filter_add_action((FilterFilter *)rule, filter_part_clone (part));

	return rule;
}
