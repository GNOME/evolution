/*
 *  Copyright (C) 2000-2001 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
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

#include <string.h>
#include <glib.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktable.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <gal/widgets/e-unicode.h>

#include "filter-filter.h"
#include "filter-context.h"

#define d(x)

static int validate(FilterRule *);
static xmlNodePtr xml_encode (FilterRule *);
static int xml_decode (FilterRule *, xmlNodePtr, struct _RuleContext *f);
/*static void build_code(FilterRule *, GString *out);*/
static GtkWidget *get_widget (FilterRule *fr, struct _RuleContext *f);

static void filter_filter_class_init (FilterFilterClass *class);
static void filter_filter_init (FilterFilter *gspaper);
static void filter_filter_finalise (GtkObject *obj);

#define _PRIVATE(x) (((FilterFilter *)(x))->priv)

struct _FilterFilterPrivate {
};

static FilterRuleClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_filter_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterFilter",
			sizeof(FilterFilter),
			sizeof(FilterFilterClass),
			(GtkClassInitFunc)filter_filter_class_init,
			(GtkObjectInitFunc)filter_filter_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(filter_rule_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_filter_class_init (FilterFilterClass *class)
{
	GtkObjectClass *object_class;
	FilterRuleClass *filter_rule = (FilterRuleClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class (filter_rule_get_type ());
	
	object_class->finalize = filter_filter_finalise;
	
	/* override methods */
	filter_rule->validate = validate;
	filter_rule->xml_encode = xml_encode;
	filter_rule->xml_decode = xml_decode;
	/*filter_rule->build_code = build_code;*/
	filter_rule->get_widget = get_widget;
	
	/* signals */
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_filter_init (FilterFilter *o)
{
	o->priv = g_malloc0 (sizeof (*o->priv));
}

static void
unref_list (GList *l)
{
	while (l) {
		gtk_object_unref (GTK_OBJECT (l->data));
		l = g_list_next (l);
	}
}

static void
filter_filter_finalise (GtkObject *obj)
{
	FilterFilter *o = (FilterFilter *)obj;
	
	unref_list (o->actions);
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

/**
 * filter_filter_new:
 *
 * Create a new FilterFilter object.
 * 
 * Return value: A new #FilterFilter object.
 **/
FilterFilter *
filter_filter_new (void)
{
	FilterFilter *o = (FilterFilter *)gtk_type_new(filter_filter_get_type ());
	return o;
}

void
filter_filter_add_action (FilterFilter *fr, FilterPart *fp)
{
	fr->actions = g_list_append (fr->actions, fp);
}

void
filter_filter_remove_action (FilterFilter *fr, FilterPart *fp)
{
	fr->actions = g_list_remove (fr->actions, fp);
}

void
filter_filter_replace_action (FilterFilter *fr, FilterPart *fp, FilterPart *new)
{
	GList *l;
	
	l = g_list_find (fr->actions, fp);
	if (l) {
		l->data = new;
	} else {
		fr->actions = g_list_append (fr->actions, new);
	}
}

void
filter_filter_build_action (FilterFilter *fr, GString *out)
{
	g_string_append (out, "(begin\n");
	filter_part_build_code_list (fr->actions, out);
	g_string_append (out, ")\n");
}

static int
validate(FilterRule *fr)
{
	int valid;
	GList *parts;
	FilterFilter *ff = (FilterFilter *)fr;

        valid = ((FilterRuleClass *)(parent_class))->validate(fr);

	/* validate rule actions */
	parts = ff->actions;
	while (parts && valid) {
		valid = filter_part_validate((FilterPart *)parts->data);
		parts = parts->next;
	}

	return valid;
}

static xmlNodePtr
xml_encode (FilterRule *fr)
{
	xmlNodePtr node, set, work;
	GList *l;
	FilterFilter *ff = (FilterFilter *)fr;
	
        node = ((FilterRuleClass *)(parent_class))->xml_encode (fr);
	g_assert (node != NULL);
	set = xmlNewNode (NULL, "actionset");
	xmlAddChild (node, set);
	l = ff->actions;
	while (l) {
		work = filter_part_xml_encode ((FilterPart *)l->data);
		xmlAddChild (set, work);
		l = g_list_next (l);
	}
	
	return node;

}

static void
load_set (xmlNodePtr node, FilterFilter *ff, RuleContext *f)
{
	xmlNodePtr work;
	char *rulename;
	FilterPart *part;
	
	work = node->childs;
	while (work) {
		if (!strcmp (work->name, "part")) {
			rulename = xmlGetProp (work, "name");
			part = filter_context_find_action ((FilterContext *)f, rulename);
			if (part) {
				part = filter_part_clone (part);
				filter_part_xml_decode (part, work);
				filter_filter_add_action (ff, part);
			} else {
				g_warning ("cannot find rule part '%s'\n", rulename);
			}
			xmlFree (rulename);
		} else {
			g_warning ("Unknwon xml node in part: %s", work->name);
		}
		work = work->next;
	}
}

static int
xml_decode (FilterRule *fr, xmlNodePtr node, struct _RuleContext *f)
{
	xmlNodePtr work;
	FilterFilter *ff = (FilterFilter *)fr;
	int result;
	
        result = ((FilterRuleClass *)(parent_class))->xml_decode (fr, node, f);
	if (result != 0)
		return result;
	
	work = node->childs;
	while (work) {
		if (!strcmp (work->name, "actionset")) {
			load_set (work, ff, f);
		}
		work = work->next;
	}
	
	return 0;
}

/*static void build_code(FilterRule *fr, GString *out)
{
        return ((FilterRuleClass *)(parent_class))->build_code(fr, out);
}*/

struct _part_data {
	FilterRule *fr;
	FilterContext *f;
	FilterPart *part;
	GtkWidget *partwidget, *container;
};

static void
option_activate (GtkMenuItem *item, struct _part_data *data)
{
	FilterPart *part = gtk_object_get_data (GTK_OBJECT (item), "part");
	FilterPart *newpart;
	
	/* dont update if we haven't changed */
	if (!strcmp (part->title, data->part->title))
		return;
	
	/* here we do a widget shuffle, throw away the old widget/rulepart,
	   and create another */
	if (data->partwidget)
		gtk_container_remove (GTK_CONTAINER (data->container), data->partwidget);
	
	newpart = filter_part_clone (part);
	filter_filter_replace_action ((FilterFilter *)data->fr, data->part, newpart);
	gtk_object_unref (GTK_OBJECT (data->part));
	data->part = newpart;
	data->partwidget = filter_part_get_widget (newpart);
	if (data->partwidget)
		gtk_box_pack_start (GTK_BOX (data->container), data->partwidget, FALSE, FALSE, 0);
	
	gtk_object_set_data (GTK_OBJECT (data->container), "part", newpart);
}

static GtkWidget *
get_rule_part_widget (FilterContext *f, FilterPart *newpart, FilterRule *fr)
{
	FilterPart *part = NULL;
	GtkWidget *menu;
	GtkWidget *item;
	GtkWidget *omenu;
	GtkWidget *hbox;
	GtkWidget *p;
	int index = 0, current = 0;
	struct _part_data *data;
	
	data = g_malloc0 (sizeof (*data));
	data->fr = fr;
	data->f = f;
	data->part = newpart;
	
	hbox = gtk_hbox_new (FALSE, 0);
	p = filter_part_get_widget (newpart);
	
	data->partwidget = p;
	data->container = hbox;
	
	menu = gtk_menu_new ();
	while ((part = filter_context_next_action (f, part))) {
		item = gtk_menu_item_new_with_label (_(part->title));
		
		gtk_object_set_data (GTK_OBJECT (item), "part", part);
		gtk_signal_connect (GTK_OBJECT (item), "activate", option_activate, data);
		gtk_menu_append (GTK_MENU (menu), item);
		gtk_widget_show (item);
		
		if (!strcmp (newpart->title, part->title)) {
			current = index;
		}
		index++;
	}
	
	omenu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), current);
	gtk_widget_show (omenu);
	
	gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 0);
	if (p) {
		gtk_box_pack_start (GTK_BOX (hbox), p, FALSE, FALSE, 0);
	}
	gtk_widget_show_all (hbox);
	
	return hbox;
}

struct _rule_data {
	FilterRule *fr;
	FilterContext *f;
	GtkWidget *parts;
};

static void
less_parts (GtkWidget *button, struct _rule_data *data)
{
	FilterPart *part;
	GtkWidget *rule;
	GList *l;
	
	l = ((FilterFilter *)data->fr)->actions;
	if (g_list_length (l) < 2)
		return;
	
	rule = gtk_object_get_data (GTK_OBJECT (button), "rule");
	part = gtk_object_get_data (GTK_OBJECT (rule), "part");
	
	/* remove the part from the list */
	filter_filter_remove_action ((FilterFilter *)data->fr, part);
	gtk_object_unref (GTK_OBJECT (part));
	
	/* and from the display */
	gtk_container_remove (GTK_CONTAINER (data->parts), rule);
	gtk_container_remove (GTK_CONTAINER (data->parts), button);
}

static void
attach_rule (GtkWidget *rule, struct _rule_data *data, FilterPart *part, int row)
{
	GtkWidget *remove;
	GtkWidget *pixmap;
	
	gtk_table_attach (GTK_TABLE (data->parts), rule, 0, 1, row, row + 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	
	pixmap = gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_REMOVE);
	remove = gnome_pixmap_button (pixmap, _("Remove"));
	gtk_object_set_data (GTK_OBJECT (remove), "rule", rule);
	gtk_object_set_data (GTK_OBJECT (rule), "part", part);
	/*gtk_button_set_relief (GTK_BUTTON (remove), GTK_RELIEF_NONE);*/
	gtk_signal_connect (GTK_OBJECT (remove), "clicked", less_parts, data);
	gtk_table_attach (GTK_TABLE (data->parts), remove, 1, 2, row, row + 1,
			  0, 0, 0, 0);
	gtk_widget_show (remove);
}

static void
more_parts (GtkWidget *button, struct _rule_data *data)
{
	FilterPart *new;
	
	/* create a new rule entry, use the first type of rule */
	new = filter_context_next_action ((FilterContext *)data->f, NULL);
	if (new) {
		GtkWidget *w;
		guint16 rows;
		
		new = filter_part_clone (new);
		filter_filter_add_action ((FilterFilter *)data->fr, new);
		w = get_rule_part_widget (data->f, new, data->fr);
		
		rows = GTK_TABLE (data->parts)->nrows;
		gtk_table_resize (GTK_TABLE (data->parts), rows + 1, 2);
		attach_rule (w, data, new, rows);
	}
}

static GtkWidget *
get_widget (FilterRule *fr, struct _RuleContext *f)
{
	GtkWidget *widget;
	GtkWidget *parts, *inframe;
	GtkWidget *hbox;
	GtkWidget *add, *pixmap;
	GtkWidget *w;
	GtkWidget *frame;
	GtkWidget *scrolledwindow;
	GtkObject *hadj, *vadj;
	GList *l;
	FilterPart *part;
	struct _rule_data *data;
	FilterFilter *ff = (FilterFilter *)fr;
	gint rows, i = 0;
	
        widget = ((FilterRuleClass *)(parent_class))->get_widget (fr, f);
	
	/* and now for the action area */
	frame = gtk_frame_new (_("Then"));
	inframe = gtk_vbox_new (FALSE, 3);
	gtk_container_add (GTK_CONTAINER (frame), inframe);
	
	rows = g_list_length (ff->actions);
	parts = gtk_table_new (rows, 2, FALSE);
	data = g_malloc0 (sizeof (*data));
	data->f = (FilterContext *)f;
	data->fr = fr;
	data->parts = parts;
	
	hbox = gtk_hbox_new (FALSE, 3);
	
	pixmap = gnome_stock_new_with_icon (GNOME_STOCK_PIXMAP_ADD);
	add = gnome_pixmap_button (pixmap, _("Add action"));
	gtk_button_set_relief (GTK_BUTTON (add), GTK_RELIEF_NONE);
	gtk_signal_connect (GTK_OBJECT (add), "clicked", more_parts, data);
	gtk_box_pack_start (GTK_BOX (hbox), add, FALSE, FALSE, 3);
	
	gtk_box_pack_start (GTK_BOX (inframe), hbox, FALSE, FALSE, 3);
	
	l = ff->actions;
	while (l) {
		part = l->data;
		d(printf ("adding action %s\n", part->title));
		w = get_rule_part_widget ((FilterContext *)f, part, fr);
		attach_rule (w, data, part, i++);
		l = g_list_next (l);
	}
	
	hadj = gtk_adjustment_new (0.0, 0.0, 1.0, 1.0 ,1.0, 1.0);
	vadj = gtk_adjustment_new (0.0, 0.0, 1.0, 1.0 ,1.0, 1.0);
	scrolledwindow = gtk_scrolled_window_new (GTK_ADJUSTMENT (hadj), GTK_ADJUSTMENT (vadj));
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), parts);
	
	gtk_box_pack_start (GTK_BOX (inframe), scrolledwindow, TRUE, TRUE, 3);
	
	/*gtk_box_pack_start (GTK_BOX (inframe), parts, FALSE, FALSE, 3);*/
	
	gtk_widget_show_all (frame);
	
	gtk_box_pack_start (GTK_BOX (widget), frame, TRUE, TRUE, 3);
	
	return widget;
}
