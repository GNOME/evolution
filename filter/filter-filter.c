/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@ximian.com>
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

#include <string.h>

#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>

#include "filter-filter.h"
#include "filter-context.h"

#define d(x)

static int validate(FilterRule *fr);
static int filter_eq(FilterRule *fr, FilterRule *cm);
static xmlNodePtr xml_encode (FilterRule *fr);
static int xml_decode (FilterRule *fr, xmlNodePtr, RuleContext *rc);
static void rule_copy (FilterRule *dest, FilterRule *src);
/*static void build_code(FilterRule *, GString *out);*/
static GtkWidget *get_widget (FilterRule *fr, RuleContext *rc);

static void filter_filter_class_init (FilterFilterClass *klass);
static void filter_filter_init (FilterFilter *ff);
static void filter_filter_finalise (GObject *obj);


static FilterRuleClass *parent_class = NULL;


GType
filter_filter_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterFilterClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_filter_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterFilter),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_filter_init,
		};
		
		type = g_type_register_static (FILTER_TYPE_RULE, "FilterFilter", &info, 0);
	}
	
	return type;
}

static void
filter_filter_class_init (FilterFilterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterRuleClass *fr_class = (FilterRuleClass *) klass;
	
	parent_class = g_type_class_ref (FILTER_TYPE_RULE);
	
	object_class->finalize = filter_filter_finalise;
	
	/* override methods */
	fr_class->validate = validate;
	fr_class->eq = filter_eq;
	fr_class->xml_encode = xml_encode;
	fr_class->xml_decode = xml_decode;
	/*fr_class->build_code = build_code;*/
	fr_class->copy = rule_copy;
	fr_class->get_widget = get_widget;
}

static void
filter_filter_init (FilterFilter *ff)
{
	;
}

static void
unref_list (GList *l)
{
	while (l) {
		g_object_unref (l->data);
		l = l->next;
	}
}

static void
filter_filter_finalise (GObject *obj)
{
	FilterFilter *ff = (FilterFilter *) obj;
	
	unref_list (ff->actions);
	g_list_free (ff->actions);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
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
	return (FilterFilter *) g_object_new (FILTER_TYPE_FILTER, NULL, NULL);
}

void
filter_filter_add_action (FilterFilter *fr, FilterPart *fp)
{
	fr->actions = g_list_append (fr->actions, fp);
	
	filter_rule_emit_changed ((FilterRule *) fr);
}

void
filter_filter_remove_action (FilterFilter *fr, FilterPart *fp)
{
	fr->actions = g_list_remove (fr->actions, fp);
	
	filter_rule_emit_changed ((FilterRule *) fr);
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
	
	filter_rule_emit_changed ((FilterRule *) fr);
}

void
filter_filter_build_action (FilterFilter *fr, GString *out)
{
	g_string_append (out, "(begin\n");
	filter_part_build_code_list (fr->actions, out);
	g_string_append (out, ")\n");
}

static int
validate (FilterRule *fr)
{
	FilterFilter *ff = (FilterFilter *) fr;
	GList *parts;
	int valid;
	
        valid = FILTER_RULE_CLASS (parent_class)->validate (fr);
	
	/* validate rule actions */
	parts = ff->actions;
	while (parts && valid) {
		valid = filter_part_validate ((FilterPart *) parts->data);
		parts = parts->next;
	}
	
	return valid;
}

static int
list_eq (GList *al, GList *bl)
{
	int truth = TRUE;
	
	while (truth && al && bl) {
		FilterPart *a = al->data, *b = bl->data;
		
		truth = filter_part_eq (a, b);
		al = al->next;
		bl = bl->next;
	}
	
	return truth && al == NULL && bl == NULL;
}

static int
filter_eq (FilterRule *fr, FilterRule *cm)
{
        return FILTER_RULE_CLASS (parent_class)->eq (fr, cm)
		&& list_eq (((FilterFilter *) fr)->actions, ((FilterFilter *) cm)->actions);
}

static xmlNodePtr
xml_encode (FilterRule *fr)
{
	FilterFilter *ff = (FilterFilter *) fr;
	xmlNodePtr node, set, work;
	GList *l;
	
        node = FILTER_RULE_CLASS (parent_class)->xml_encode (fr);
	g_assert (node != NULL);
	set = xmlNewNode (NULL, "actionset");
	xmlAddChild (node, set);
	l = ff->actions;
	while (l) {
		work = filter_part_xml_encode ((FilterPart *) l->data);
		xmlAddChild (set, work);
		l = l->next;
	}
	
	return node;

}

static void
load_set (xmlNodePtr node, FilterFilter *ff, RuleContext *rc)
{
	xmlNodePtr work;
	char *rulename;
	FilterPart *part;
	
	work = node->children;
	while (work) {
		if (!strcmp (work->name, "part")) {
			rulename = xmlGetProp (work, "name");
			part = filter_context_find_action ((FilterContext *) rc, rulename);
			if (part) {
				part = filter_part_clone (part);
				filter_part_xml_decode (part, work);
				filter_filter_add_action (ff, part);
			} else {
				g_warning ("cannot find rule part '%s'\n", rulename);
			}
			xmlFree (rulename);
		} else if (work->type == XML_ELEMENT_NODE) {
			g_warning ("Unknown xml node in part: %s", work->name);
		}
		work = work->next;
	}
}

static int
xml_decode (FilterRule *fr, xmlNodePtr node, RuleContext *rc)
{
	FilterFilter *ff = (FilterFilter *) fr;
	xmlNodePtr work;
	int result;
	
        result = FILTER_RULE_CLASS (parent_class)->xml_decode (fr, node, rc);
	if (result != 0)
		return result;
	
	work = node->children;
	while (work) {
		if (!strcmp (work->name, "actionset")) {
			load_set (work, ff, rc);
		}
		work = work->next;
	}
	
	return 0;
}

static void
rule_copy (FilterRule *dest, FilterRule *src)
{
	FilterFilter *fdest, *fsrc;
	GList *node;
	
	fdest = (FilterFilter *) dest;
	fsrc = (FilterFilter *) src;
	
	if (fdest->actions) {
		g_list_foreach (fdest->actions, (GFunc) g_object_unref, NULL);
		g_list_free (fdest->actions);
		fdest->actions = NULL;
	}
	
	node = fsrc->actions;
	while (node) {
		FilterPart *part = node->data;
		
		g_object_ref (part);
		fdest->actions = g_list_append (fdest->actions, part);
		node = node->next;
	}
	
	FILTER_RULE_CLASS (parent_class)->copy (dest, src);
}

/*static void build_code(FilterRule *fr, GString *out)
{
        return FILTER_RULE_CLASS (parent_class)->build_code (fr, out);
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
	FilterPart *part = g_object_get_data ((GObject *) item, "part");
	FilterPart *newpart;
	
	/* dont update if we haven't changed */
	if (!strcmp (part->title, data->part->title))
		return;
	
	/* here we do a widget shuffle, throw away the old widget/rulepart,
	   and create another */
	if (data->partwidget)
		gtk_container_remove (GTK_CONTAINER (data->container), data->partwidget);
	
	newpart = filter_part_clone (part);
	filter_part_copy_values (newpart, data->part);
	filter_filter_replace_action ((FilterFilter *) data->fr, data->part, newpart);
	g_object_unref (data->part);
	data->part = newpart;
	data->partwidget = filter_part_get_widget (newpart);
	if (data->partwidget)
		gtk_box_pack_start (GTK_BOX (data->container), data->partwidget, FALSE, FALSE, 0);
	
	g_object_set_data ((GObject *) data->container, "part", newpart);
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
		
		g_object_set_data ((GObject *) item, "part", part);
		g_signal_connect (item, "activate", G_CALLBACK (option_activate), data);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show (item);
		
		if (!strcmp (newpart->title, part->title))
			current = index;
		
		index++;
	}
	
	omenu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), current);
	gtk_widget_show (omenu);
	
	gtk_box_pack_start (GTK_BOX (hbox), omenu, FALSE, FALSE, 0);
	if (p)
		gtk_box_pack_start (GTK_BOX (hbox), p, FALSE, FALSE, 0);
	
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
	
	l = ((FilterFilter *) data->fr)->actions;
	if (g_list_length (l) < 2)
		return;
	
	rule = g_object_get_data ((GObject *) button, "rule");
	part = g_object_get_data ((GObject *) rule, "part");
	
	/* remove the part from the list */
	filter_filter_remove_action ((FilterFilter *) data->fr, part);
	g_object_unref (part);
	
	/* and from the display */
	gtk_container_remove (GTK_CONTAINER (data->parts), rule);
	gtk_container_remove (GTK_CONTAINER (data->parts), button);
}

static void
attach_rule (GtkWidget *rule, struct _rule_data *data, FilterPart *part, int row)
{
	GtkWidget *remove;
	
	gtk_table_attach (GTK_TABLE (data->parts), rule, 0, 1, row, row + 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	
	remove = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	g_object_set_data ((GObject *) remove, "rule", rule);
	g_object_set_data ((GObject *) rule, "part", part);
	/*gtk_button_set_relief (GTK_BUTTON (remove), GTK_RELIEF_NONE);*/
	g_signal_connect (remove, "clicked", G_CALLBACK (less_parts), data);
	gtk_table_attach (GTK_TABLE (data->parts), remove, 1, 2, row, row + 1,
			  0, 0, 0, 0);
	gtk_widget_show (remove);
}

static void
more_parts (GtkWidget *button, struct _rule_data *data)
{
	FilterPart *new;
	
	/* create a new rule entry, use the first type of rule */
	new = filter_context_next_action ((FilterContext *) data->f, NULL);
	if (new) {
		GtkWidget *w;
		guint16 rows;
		
		new = filter_part_clone (new);
		filter_filter_add_action ((FilterFilter *) data->fr, new);
		w = get_rule_part_widget (data->f, new, data->fr);
		
		rows = GTK_TABLE (data->parts)->nrows;
		gtk_table_resize (GTK_TABLE (data->parts), rows + 1, 2);
		attach_rule (w, data, new, rows);
	}
}

static GtkWidget *
get_widget (FilterRule *fr, RuleContext *rc)
{
	GtkWidget *widget, *hbox, *add, *frame;
	GtkWidget *parts, *inframe, *w;
	GtkWidget *scrolledwindow;
	GtkObject *hadj, *vadj;
	GList *l;
	FilterPart *part;
	struct _rule_data *data;
	FilterFilter *ff = (FilterFilter *)fr;
	int rows, i = 0;
	
        widget = FILTER_RULE_CLASS (parent_class)->get_widget (fr, rc);
	
	/* and now for the action area */
	frame = gtk_frame_new (_("Then"));
	inframe = gtk_vbox_new (FALSE, 3);
	gtk_container_add (GTK_CONTAINER (frame), inframe);
	gtk_container_set_border_width (GTK_CONTAINER (inframe), 6);
	
	rows = g_list_length (ff->actions);
	parts = gtk_table_new (rows, 2, FALSE);
	data = g_malloc0 (sizeof (*data));
	data->f = (FilterContext *) rc;
	data->fr = fr;
	data->parts = parts;
	
	hbox = gtk_hbox_new (FALSE, 3);
	
	add = gtk_button_new_from_stock (GTK_STOCK_ADD);
	/* gtk_button_set_relief (GTK_BUTTON (add), GTK_RELIEF_NONE); */
	g_signal_connect (add, "clicked", G_CALLBACK (more_parts), data);
	gtk_box_pack_start (GTK_BOX (hbox), add, FALSE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (inframe), hbox, FALSE, FALSE, 3);
	
	l = ff->actions;
	while (l) {
		part = l->data;
		d(printf ("adding action %s\n", part->title));
		w = get_rule_part_widget ((FilterContext *) rc, part, fr);
		attach_rule (w, data, part, i++);
		l = l->next;
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
