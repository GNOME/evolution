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

#include <gtk/gtk.h>
#include <gnome.h>
#include <gnome-xml/xmlmemory.h>

#include <e-util/e-unicode.h>

#include "filter-rule.h"
#include "filter-context.h"

#define d(x)

static xmlNodePtr xml_encode(FilterRule *);
static int xml_decode(FilterRule *, xmlNodePtr, RuleContext *);
static void build_code(FilterRule *, GString *out);
static GtkWidget *get_widget(FilterRule *fr, struct _RuleContext *f);

static void filter_rule_class_init	(FilterRuleClass *class);
static void filter_rule_init	(FilterRule *gspaper);
static void filter_rule_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((FilterRule *)(x))->priv)

struct _FilterRulePrivate {
	GtkWidget *parts;	/* where the parts are stored */
};

static GtkObjectClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_rule_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterRule",
			sizeof(FilterRule),
			sizeof(FilterRuleClass),
			(GtkClassInitFunc)filter_rule_class_init,
			(GtkObjectInitFunc)filter_rule_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(gtk_object_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_rule_class_init (FilterRuleClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(gtk_object_get_type ());

	object_class->finalize = filter_rule_finalise;

	/* override methods */
	class->xml_encode = xml_encode;
	class->xml_decode = xml_decode;
	class->build_code = build_code;
	class->get_widget = get_widget;

	/* signals */

	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
filter_rule_init (FilterRule *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
unref_list(GList *l)
{
	while (l) {
		gtk_object_unref((GtkObject *)l->data);
		l = g_list_next(l);
	}
}

static void
filter_rule_finalise(GtkObject *obj)
{
	FilterRule *o = (FilterRule *)obj;

	g_free(o->name);
	unref_list(o->parts);

        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * filter_rule_new:
 *
 * Create a new FilterRule object.
 * 
 * Return value: A new #FilterRule object.
 **/
FilterRule *
filter_rule_new(void)
{
	FilterRule *o = (FilterRule *)gtk_type_new(filter_rule_get_type ());
	return o;
}

void		filter_rule_set_name	(FilterRule *fr, const char *name)
{
	g_free(fr->name);
	fr->name = g_strdup(name);
}

xmlNodePtr	filter_rule_xml_encode	(FilterRule *fr)
{
	return ((FilterRuleClass *)((GtkObject *)fr)->klass)->xml_encode(fr);
}

static xmlNodePtr xml_encode(FilterRule *fr)
{
	xmlNodePtr node, set, work;
	GList *l;

	node = xmlNewNode(NULL, "rule");
	switch (fr->grouping) {
	case FILTER_GROUP_ALL:
		xmlSetProp(node, "grouping", "all");
		break;
	case FILTER_GROUP_ANY:
		xmlSetProp(node, "grouping", "any");
		break;
	}

	switch (fr->source) {
	case FILTER_SOURCE_INCOMING:
		xmlSetProp(node, "source", "incoming");
		break;
	case FILTER_SOURCE_DEMAND:
		xmlSetProp(node, "source", "ondemand");
		break;
	case FILTER_SOURCE_OUTGOING:
		xmlSetProp(node, "source", "outgoing");
		break;
	}

	if (fr->name) {
		work = xmlNewNode(NULL, "title");
		xmlNodeSetContent(work, fr->name);
		xmlAddChild(node, work);
	}
	set = xmlNewNode(NULL, "partset");
	xmlAddChild(node, set);
	l = fr->parts;
	while (l) {
		work = filter_part_xml_encode((FilterPart *)l->data);
		xmlAddChild(set, work);
		l = g_list_next(l);
	}
	return node;
}

static void load_set(xmlNodePtr node, FilterRule *fr, RuleContext *f)
{
	xmlNodePtr work;
	char *rulename;
	FilterPart *part;

	work = node->childs;
	while (work) {
		if (!strcmp(work->name, "part")) {
			rulename = xmlGetProp(work, "name");
			part = rule_context_find_part(f, rulename);
			if (part) {
				part = filter_part_clone(part);
				filter_part_xml_decode(part, work);
				filter_rule_add_part(fr, part);
			} else {
				g_warning("cannot find rule part '%s'\n", rulename);
			}
			xmlFree(rulename);
		} else {
			g_warning("Unknwon xml node in part: %s", work->name);
		}
		work = work->next;
	}
}

int		filter_rule_xml_decode	(FilterRule *fr, xmlNodePtr node, RuleContext *f)
{
	return ((FilterRuleClass *)((GtkObject *)fr)->klass)->xml_decode(fr, node, f);
}

static int xml_decode(FilterRule *fr, xmlNodePtr node, RuleContext *f)
{
	xmlNodePtr work;
	char *grouping;
	char *source;

	if (fr->name) {
		g_free(fr->name);
		fr->name = NULL;
	}

	grouping = xmlGetProp(node, "grouping");
	if (!strcmp(grouping, "any"))
		fr->grouping = FILTER_GROUP_ANY;
	else
		fr->grouping = FILTER_GROUP_ALL;

	/* FIXME: free source and grouping? */
	source = xmlGetProp (node, "source");
	if (!source) /*default to incoming*/
		fr->source = FILTER_SOURCE_INCOMING;
	else if (!strcmp (source, "outgoing"))
		fr->source = FILTER_SOURCE_OUTGOING;
	else if (!strcmp (source, "ondemand"))
		fr->source = FILTER_SOURCE_DEMAND;
	else if (!strcmp (source, "incoming"))
		fr->source = FILTER_SOURCE_INCOMING;
	else {
		g_warning ("Unknown filter source type \"%s\"", source);
		fr->source = FILTER_SOURCE_INCOMING;
	}

	work = node->childs;
	while (work) {
		if (!strcmp(work->name, "partset")) {
			load_set(work, fr, f);
		} else if (!strcmp(work->name, "title")) {
			if (!fr->name)
				fr->name = xmlNodeGetContent(work);
		}
		work = work->next;
	}
	return 0;
}

void		filter_rule_add_part	(FilterRule *fr, FilterPart *fp)
{
	fr->parts = g_list_append(fr->parts, fp);
}

void		filter_rule_remove_part	(FilterRule *fr, FilterPart *fp)
{
	fr->parts = g_list_remove(fr->parts, fp);
}

void		filter_rule_replace_part(FilterRule *fr, FilterPart *fp, FilterPart *new)
{
	GList *l;

	l = g_list_find(fr->parts, fp);
	if (l) {
		l->data = new;
	} else {
		fr->parts = g_list_append(fr->parts, new);
	}
}

void		filter_rule_build_code	(FilterRule *fr, GString *out)
{
	return ((FilterRuleClass *)((GtkObject *)fr)->klass)->build_code(fr, out);
}

static void build_code(FilterRule *fr, GString *out)
{
	switch (fr->grouping) {
	case FILTER_GROUP_ALL:
		g_string_append(out, " (and\n  ");
		break;
	case FILTER_GROUP_ANY:
		g_string_append(out, " (or\n  ");
		break;
	default:
		g_warning("Invalid grouping");
	}

	filter_part_build_code_list(fr->parts, out);
	g_string_append(out, ")\n");
}


static void
match_all(GtkRadioButton *w, FilterRule *fr)
{
	if (gtk_toggle_button_get_active((GtkToggleButton *)w))
		fr->grouping = FILTER_GROUP_ALL;
}

static void
match_any(GtkRadioButton *w, FilterRule *fr)
{
	if (gtk_toggle_button_get_active((GtkToggleButton *)w))
		fr->grouping = FILTER_GROUP_ANY;
}

struct _part_data {
	FilterRule *fr;
	RuleContext *f;
	FilterPart *part;
	GtkWidget *partwidget, *container;
};

static void option_activate(GtkMenuItem *item, struct _part_data *data)
{
	FilterPart *part = gtk_object_get_data((GtkObject *)item, "part");
	FilterPart *newpart;

	/* dont update if we haven't changed */
	if (!strcmp(part->title, data->part->title))
		return;

	/* here we do a widget shuffle, throw away the old widget/rulepart,
	   and create another */
	if (data->partwidget)
		gtk_container_remove((GtkContainer *)data->container, data->partwidget);
	newpart = filter_part_clone(part);
	filter_rule_replace_part(data->fr, data->part, newpart);
	gtk_object_unref((GtkObject *)data->part);
	data->part = newpart;
	data->partwidget = filter_part_get_widget(newpart);
	if (data->partwidget)
		gtk_box_pack_start((GtkBox *)data->container, data->partwidget, FALSE, FALSE, 0);
}

static GtkWidget *
get_rule_part_widget(RuleContext *f, FilterPart *newpart, FilterRule *fr)
{
	FilterPart *part = NULL;
	GtkMenu *menu;
	GtkMenuItem *item;
	GtkOptionMenu *omenu;
	GtkHBox *hbox;
	GtkWidget *p;
	int index=0, current=0;
	struct _part_data *data;
	gchar *s;

	data = g_malloc0(sizeof(*data));
	data->fr = fr;
	data->f = f;
	data->part = newpart;

	hbox = (GtkHBox *)gtk_hbox_new(FALSE, 0);
	/* only set to automatically clean up the memory */
	gtk_object_set_data_full((GtkObject *)hbox, "data", data, g_free);

	p = filter_part_get_widget(newpart);

	data->partwidget = p;
	data->container = (GtkWidget *)hbox;

	menu = (GtkMenu *)gtk_menu_new();
	/* sigh, this is a little ugly */
	while ((part=rule_context_next_part(f, part))) {
		s = e_utf8_to_gtk_string ((GtkWidget *) menu, part->title);
		item = (GtkMenuItem *)gtk_menu_item_new_with_label (s);
		g_free (s);
		gtk_object_set_data((GtkObject *)item, "part", part);
		gtk_signal_connect((GtkObject *)item, "activate", option_activate, data);
		gtk_menu_append(menu, (GtkWidget *)item);
		gtk_widget_show((GtkWidget *)item);
		if (!strcmp(newpart->title, part->title)) {
			current = index;
		}
		index++;
	}

	omenu = (GtkOptionMenu *)gtk_option_menu_new();
	gtk_option_menu_set_menu(omenu, (GtkWidget *)menu);
	gtk_option_menu_set_history(omenu, current);
	gtk_widget_show((GtkWidget *)omenu);

	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)omenu, FALSE, FALSE, 0);
	if (p) {
		gtk_box_pack_start((GtkBox *)hbox, p, FALSE, FALSE, 0);
	}
	gtk_widget_show_all((GtkWidget *)hbox);

	return (GtkWidget *)hbox;
}

struct _rule_data {
	FilterRule *fr;
	RuleContext *f;
	GtkWidget *parts;
};

static void
less_parts(GtkWidget *button, struct _rule_data *data)
{
	GList *l;
	FilterPart *part;
	GtkWidget *w;

	l = data->fr->parts;
	if (g_list_length(l) < 2)
		return;

	/* remove the last one from the list */
	l = g_list_last(l);
	part = l->data;
	filter_rule_remove_part(data->fr, part);
	gtk_object_unref((GtkObject *)part);

	/* and from the display */
	l = g_list_last(GTK_BOX(data->parts)->children);
	w = ((GtkBoxChild *)l->data)->widget;
	gtk_container_remove((GtkContainer *)data->parts, w);
}

static void
more_parts(GtkWidget *button, struct _rule_data *data)
{
	FilterPart *new;
	GtkWidget *w;

	/* create a new rule entry, use the first type of rule */
	new = rule_context_next_part(data->f, NULL);
	if (new) {
		new = filter_part_clone(new);
		filter_rule_add_part(data->fr, new);
		w = get_rule_part_widget(data->f, new, data->fr);
		gtk_box_pack_start((GtkBox *)data->parts, w, FALSE, FALSE, 0);
	}
}

static void
name_changed(GtkEntry *entry, FilterRule *fr)
{
	g_free(fr->name);
	fr->name = e_utf8_gtk_entry_get_text(entry);
}

GtkWidget	*filter_rule_get_widget	(FilterRule *fr, struct _RuleContext *f)
{
	return ((FilterRuleClass *)((GtkObject *)fr)->klass)->get_widget(fr, f);
}

static GtkWidget *get_widget(FilterRule *fr, struct _RuleContext *f)
{
	GtkVBox *vbox, *parts, *inframe;
	GtkHBox *hbox;
	GtkWidget *w;
	GtkRadioButton *g0, *g1;
	GtkFrame *frame;
	GtkEntry *name;
	GtkLabel *label;
	GList *l;
	FilterPart *part;
	struct _rule_data *data;

	/* this stuff should probably be a table, but the
	   rule parts need to be a vbox */
	vbox = (GtkVBox *)gtk_vbox_new(FALSE, 3);

	label = (GtkLabel *)gtk_label_new("Name");
	name = (GtkEntry *)gtk_entry_new();
	if (fr->name)
		e_utf8_gtk_entry_set_text(name, fr->name);
	hbox = (GtkHBox *)gtk_hbox_new(FALSE, 3);
	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)label, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)name, TRUE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)hbox, FALSE, FALSE, 3);
	gtk_signal_connect((GtkObject *)name, "changed", name_changed, fr);

	frame = (GtkFrame *)gtk_frame_new("Messages matching");
	inframe = (GtkVBox *)gtk_vbox_new(FALSE, 3);
	gtk_container_add((GtkContainer *)frame, (GtkWidget *)inframe);

	hbox = (GtkHBox *)gtk_hbox_new(FALSE, 3);
	g0 = (GtkRadioButton *)gtk_radio_button_new_with_label(NULL, "Match all parts");
	g1 = (GtkRadioButton *)gtk_radio_button_new_with_label(g0->group, "Match any part");
	if (fr->grouping == FILTER_GROUP_ALL) {
		gtk_toggle_button_set_active((GtkToggleButton *)g0, TRUE);
	} else {
		gtk_toggle_button_set_active((GtkToggleButton *)g1, TRUE);
	}
	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)g0, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)g1, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *)inframe, (GtkWidget *)hbox, FALSE, FALSE, 3);

	gtk_signal_connect((GtkObject *)g0, "toggled", match_all, fr);
	gtk_signal_connect((GtkObject *)g1, "toggled", match_any, fr);

	/* this is the parts list, it should probably be inside a scrolling list */
	parts = (GtkVBox *)gtk_vbox_new(FALSE, 3);

	/* data for the parts part of the display */
	data = g_malloc0(sizeof(*data));
	data->f = f;
	data->fr = fr;
	data->parts = (GtkWidget *)parts;

	/* only set to automatically clean up the memory */
	gtk_object_set_data_full((GtkObject *)vbox, "data", data, g_free);

	l = fr->parts;
	while (l) {
		part = l->data;
		w = get_rule_part_widget(f, part, fr);
		gtk_box_pack_start((GtkBox *)parts, (GtkWidget *)w, FALSE, FALSE, 3);
		l = g_list_next(l);
	}

	gtk_box_pack_start((GtkBox *)inframe, (GtkWidget *)parts, FALSE, FALSE, 3);

	hbox = (GtkHBox *)gtk_hbox_new(FALSE, 3);
	w = gtk_button_new_with_label(_("Less"));
	gtk_signal_connect((GtkObject *)w, "clicked", less_parts, data);
	gtk_box_pack_end((GtkBox *)hbox, (GtkWidget *)w, FALSE, FALSE, 3);
	w = gtk_button_new_with_label(_("More"));
	gtk_signal_connect((GtkObject *)w, "clicked", more_parts, data);
	gtk_box_pack_end((GtkBox *)hbox, (GtkWidget *)w, FALSE, FALSE, 3);
	gtk_box_pack_start((GtkBox *)inframe, (GtkWidget *)hbox, FALSE, FALSE, 3);

	gtk_box_pack_start((GtkBox *)vbox, (GtkWidget *)frame, FALSE, FALSE, 3);

	gtk_widget_show_all((GtkWidget *)vbox);
	return (GtkWidget *)vbox;
}

FilterRule	*filter_rule_next_list		(GList *l, FilterRule *last)
{
	GList *node = l;

	if (last != NULL) {
		node = g_list_find(node, last);
		if (node == NULL)
			node = l;
		else
			node = g_list_next(node);
	}
	if (node)
		return node->data;
	return NULL;
}

FilterRule	*filter_rule_find_list		(GList *l, const char *name)
{
	while (l) {
		FilterRule *rule = l->data;
		if (!strcmp(rule->name, name))
			return rule;
		l = g_list_next(l);
	}
	return NULL;
}

