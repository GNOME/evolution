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
#include <glade/glade.h>

#include <e-util/e-unicode.h>
#include "vfolder-context.h"
#include "vfolder-rule.h"
#include "shell/evolution-shell-client.h"

#define d(x) x

static xmlNodePtr xml_encode(FilterRule *);
static int xml_decode(FilterRule *, xmlNodePtr, struct _RuleContext *f);
/*static void build_code(FilterRule *, GString *out);*/
static GtkWidget *get_widget(FilterRule *fr, struct _RuleContext *f);

extern EvolutionShellClient *global_shell_client;

static void vfolder_rule_class_init	(VfolderRuleClass *class);
static void vfolder_rule_init	(VfolderRule *gspaper);
static void vfolder_rule_finalise	(GtkObject *obj);

#define _PRIVATE(x) (((VfolderRule *)(x))->priv)

struct _VfolderRulePrivate {
};

static FilterRuleClass *parent_class;

guint
vfolder_rule_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"VfolderRule",
			sizeof(VfolderRule),
			sizeof(VfolderRuleClass),
			(GtkClassInitFunc)vfolder_rule_class_init,
			(GtkObjectInitFunc)vfolder_rule_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(filter_rule_get_type (), &type_info);
	}
	
	return type;
}

static void
vfolder_rule_class_init (VfolderRuleClass *class)
{
	GtkObjectClass *object_class;
	FilterRuleClass *filter_rule = (FilterRuleClass *)class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(filter_rule_get_type ());

	object_class->finalize = vfolder_rule_finalise;

	/* override methods */
	filter_rule->xml_encode = xml_encode;
	filter_rule->xml_decode = xml_decode;
	/*filter_rule->build_code = build_code;*/
	filter_rule->get_widget = get_widget;
}

static void
vfolder_rule_init (VfolderRule *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
vfolder_rule_finalise(GtkObject *obj)
{
	VfolderRule *o = (VfolderRule *)obj;
	o = o;
        ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * vfolder_rule_new:
 *
 * Create a new VfolderRule object.
 * 
 * Return value: A new #VfolderRule object.
 **/
VfolderRule *
vfolder_rule_new(void)
{
	VfolderRule *o = (VfolderRule *)gtk_type_new(vfolder_rule_get_type ());
	return o;
}

void		vfolder_rule_add_source		(VfolderRule *vr, const char *uri)
{
	g_assert(IS_VFOLDER_RULE(vr));

	vr->sources = g_list_append(vr->sources, g_strdup(uri));
}

const char	*vfolder_rule_find_source	(VfolderRule *vr, const char *uri)
{
	GList *l;

	g_assert(IS_VFOLDER_RULE(vr));

	/* only does a simple string or address comparison, should
	   probably do a decoded url comparison */
	l = vr->sources;
	while (l) {
		if (l->data == uri || !strcmp(l->data, uri))
			return l->data;
		l = g_list_next(l);
	}
	return NULL;
}

void		vfolder_rule_remove_source	(VfolderRule *vr, const char *uri)
{
	char *found;

	g_assert(IS_VFOLDER_RULE(vr));

	found = (char *)vfolder_rule_find_source(vr, uri);
	if (found) {
		vr->sources = g_list_remove(vr->sources, found);
		g_free(found);
	}
}

const char	*vfolder_rule_next_source	(VfolderRule *vr, const char *last)
{
	GList *node;

	if (last == NULL) {
		node = vr->sources;
	} else {
		node = g_list_find(vr->sources, (char *)last);
		if (node == NULL)
			node = vr->sources;
		else
			node = g_list_next(node);
	}
	if (node)
		return (const char *)node->data;
	return NULL;
}

static xmlNodePtr xml_encode(FilterRule *fr)
{
	xmlNodePtr node, set, work;
	GList *l;
	VfolderRule *vr = (VfolderRule *)fr;

        node = ((FilterRuleClass *)(parent_class))->xml_encode(fr);
	g_assert(node != NULL);
	set = xmlNewNode(NULL, "sources");
	xmlAddChild(node, set);
	l = vr->sources;
	while (l) {
		work = xmlNewNode(NULL, "folder");
		xmlSetProp(work, "uri", l->data);
		xmlAddChild(set, work);
		l = g_list_next(l);
	}
	return node;
}

static int xml_decode(FilterRule *fr, xmlNodePtr node, struct _RuleContext *f)
{
	xmlNodePtr set, work;
	int result;
	VfolderRule *vr = (VfolderRule *)fr;
	char *uri;
	
        result = ((FilterRuleClass *)(parent_class))->xml_decode(fr, node, f);
	if (result != 0)
		return result;

	set = node->childs;
	while (set) {
		if (!strcmp(set->name, "sources")) {
			work = set->childs;
			while (work) {
				if (!strcmp(work->name, "folder")) {
					uri = xmlGetProp(work, "uri");
					if (uri)
						vr->sources = g_list_append(vr->sources, uri);
				}
				work = work->next;
			}
		}
		set = set->next;
	}
	return 0;
}

enum {
	BUTTON_ADD,
	BUTTON_REMOVE,
	BUTTON_LAST,
};

struct _source_data {
	RuleContext *f;
	VfolderRule *vr;
	const char *current;
	GtkList *list;
	GtkButton *buttons[BUTTON_LAST];
};

static void source_add(GtkWidget *widget, struct _source_data *data);
static void source_remove(GtkWidget *widget, struct _source_data *data);

static struct {
	char *name;
	GtkSignalFunc func;
} edit_buttons[] = {
	{ "source_add", source_add },
	{ "source_remove", source_remove },
};

static void
set_sensitive(struct _source_data *data)
{
	gtk_widget_set_sensitive((GtkWidget *)data->buttons[BUTTON_ADD], TRUE);
	gtk_widget_set_sensitive((GtkWidget *)data->buttons[BUTTON_REMOVE], data->current != NULL);
}

static void
select_source(GtkWidget *w, GtkWidget *child, struct _source_data *data)
{
	data->current = gtk_object_get_data((GtkObject *)child, "source");
	set_sensitive(data);
}

static void source_add(GtkWidget *widget, struct _source_data *data)
{
	const char *allowed_types[] = { "mail", NULL };
	char *def, *uri;
	GtkListItem *item;
	GList *l;
	gchar *s;

	def = "";
	evolution_shell_client_user_select_folder (global_shell_client,
						   _("Select Folder"),
						   def, allowed_types, NULL, &uri);

	if (uri != NULL && uri[0] != '\0') {
		data->vr->sources = g_list_append(data->vr->sources, uri);

		l = NULL;
		s = e_utf8_to_gtk_string ((GtkWidget *) data->list, uri);
		item = (GtkListItem *)gtk_list_item_new_with_label (s);
		g_free (s);
		gtk_object_set_data((GtkObject *)item, "source", uri);
		gtk_widget_show((GtkWidget *)item);
		l = g_list_append(NULL, item);
		gtk_list_append_items(data->list, l);
		gtk_list_select_child(data->list, (GtkWidget *)item);
		data->current = uri;
	} else {
		g_free(uri);
	}
	set_sensitive(data);
}

static void source_remove(GtkWidget *widget, struct _source_data *data)
{
	const char *source;
	int index = 0;
	GList *l;
	GtkListItem *item;

	source = NULL;
	while ((source = vfolder_rule_next_source(data->vr, source))) {
		if (data->current == source) {
			vfolder_rule_remove_source(data->vr, source);
			item = g_list_nth_data(data->list->children, index);
			l = g_list_append(NULL, item);
			gtk_list_remove_items(data->list, l);
			g_list_free(l);
			data->current = NULL;
			break;
		}
		index++;
	}
	set_sensitive(data);
}

static GtkWidget *get_widget(FilterRule *fr, struct _RuleContext *f)
{
	GtkWidget *widget, *frame, *w;
	GladeXML *gui;
	const char *source;
	VfolderRule *vr = (VfolderRule *)fr;
	struct _source_data *data;
	int i;
	GList *l;

        widget = ((FilterRuleClass *)(parent_class))->get_widget(fr, f);

	data = g_malloc0(sizeof(*data));
	data->f = f;
	data->vr = vr;

	gui = glade_xml_new(FILTER_GLADEDIR "/filter.glade", "vfolder_source_frame");
        frame = glade_xml_get_widget (gui, "vfolder_source_frame");

	gtk_object_set_data_full((GtkObject *)frame, "data", data, g_free);

	for (i=0;i<BUTTON_LAST;i++) {
		data->buttons[i] = (GtkButton *)w = glade_xml_get_widget (gui, edit_buttons[i].name);
		gtk_signal_connect((GtkObject *)w, "clicked", edit_buttons[i].func, data);
	}

        w = glade_xml_get_widget (gui, "source_list");
	data->list = (GtkList *)w;
	l = NULL;
	source = NULL;
	while ((source = vfolder_rule_next_source(vr, source))) {
		GtkListItem *item;
		gchar *s = e_utf8_to_gtk_string ((GtkWidget *) data->list, source);
		item = (GtkListItem *)gtk_list_item_new_with_label (s);
		g_free (s);
		gtk_object_set_data((GtkObject *)item, "source", (void *)source);
		gtk_widget_show((GtkWidget *)item);
		l = g_list_append(l, item);
	}
	gtk_list_append_items(data->list, l);
	gtk_signal_connect((GtkObject *)w, "select_child", select_source, data);
	set_sensitive(data);

	gtk_box_pack_start(GTK_BOX(widget), frame, TRUE, TRUE, 3);
	return widget;
}
