/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Author: Not Zed <notzed@lostzed.mmc.com.au>
 *          Jeffrey Stedfast <fejj@ximian.com>
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
#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>

#include "camel/camel-url.h"
#include "vfolder-context.h"
#include "vfolder-rule.h"
#include "shell/evolution-shell-client.h"

#define d(x) 

static int validate(FilterRule *);
static int vfolder_eq(FilterRule *fr, FilterRule *cm);
static xmlNodePtr xml_encode(FilterRule *);
static int xml_decode(FilterRule *, xmlNodePtr, RuleContext *f);
static void rule_copy (FilterRule *dest, FilterRule *src);
/*static void build_code(FilterRule *, GString *out);*/
static GtkWidget *get_widget(FilterRule *fr, RuleContext *f);

static void vfolder_rule_class_init (VfolderRuleClass *klass);
static void vfolder_rule_init (VfolderRule *vr);
static void vfolder_rule_finalise (GObject *obj);


static FilterRuleClass *parent_class = NULL;


GType
vfolder_rule_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (VfolderRuleClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) vfolder_rule_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (VfolderRule),
			0,    /* n_preallocs */
			(GInstanceInitFunc) vfolder_rule_init,
		};
		
		type = g_type_register_static (FILTER_TYPE_RULE, "VfolderRule", &info, 0);
	}
	
	return type;
}

static void
vfolder_rule_class_init (VfolderRuleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterRuleClass *fr_class = (FilterRuleClass *) klass;
	
	parent_class = g_type_class_ref (FILTER_TYPE_RULE);
	
	object_class->finalize = vfolder_rule_finalise;
	
	/* override methods */
	fr_class->validate   = validate;
	fr_class->eq = vfolder_eq;
	fr_class->xml_encode = xml_encode;
	fr_class->xml_decode = xml_decode;
	fr_class->copy = rule_copy;
	/*fr_class->build_code = build_code;*/
	fr_class->get_widget = get_widget;
}

static void
vfolder_rule_init (VfolderRule *vr)
{
	;
}

static void
vfolder_rule_finalise (GObject *obj)
{
	VfolderRule *vr = (VfolderRule *) obj;
	
	g_list_foreach (vr->sources, (GFunc) g_free, NULL);
	g_list_free (vr->sources);
	
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * vfolder_rule_new:
 *
 * Create a new VfolderRule object.
 * 
 * Return value: A new #VfolderRule object.
 **/
VfolderRule *
vfolder_rule_new (void)
{
	return (VfolderRule *) g_object_new (VFOLDER_TYPE_RULE, NULL, NULL);
}

void
vfolder_rule_add_source (VfolderRule *vr, const char *uri)
{
	g_assert (IS_VFOLDER_RULE (vr));
	
	vr->sources = g_list_append (vr->sources, g_strdup (uri));
	
	filter_rule_emit_changed ((FilterRule *) vr);
}

const char *
vfolder_rule_find_source (VfolderRule *vr, const char *uri)
{
	GList *l;
	
	g_assert (IS_VFOLDER_RULE (vr));
	
	/* only does a simple string or address comparison, should
	   probably do a decoded url comparison */
	l = vr->sources;
	while (l) {
		if (l->data == uri || !strcmp (l->data, uri))
			return l->data;
		l = l->next;
	}
	
	return NULL;
}

void
vfolder_rule_remove_source (VfolderRule *vr, const char *uri)
{
	char *found;
	
	g_assert (IS_VFOLDER_RULE (vr));
	
	found = (char *) vfolder_rule_find_source (vr, uri);
	if (found) {
		vr->sources = g_list_remove (vr->sources, found);
		g_free (found);
		filter_rule_emit_changed ((FilterRule *) vr);
	}
}

const char *
vfolder_rule_next_source (VfolderRule *vr, const char *last)
{
	GList *node;
	
	if (last == NULL) {
		node = vr->sources;
	} else {
		node = g_list_find (vr->sources, (char *) last);
		if (node == NULL)
			node = vr->sources;
		else
			node = g_list_next (node);
	}
	
	if (node)
		return (const char *) node->data;
	
	return NULL;
}

static int
validate (FilterRule *fr)
{
	GtkWidget *dialog;
	
	g_return_val_if_fail (fr != NULL, FALSE);
	
	if (!fr->name || !*fr->name) {
		/* FIXME: set a aprent window? */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("You must name this vfolder."));
		
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		
		return 0;
	}
	
	/* We have to have at least one source set in the "specific" case.
	   Do not translate this string! */
	if (fr->source && !strcmp (fr->source, "specific") && VFOLDER_RULE (fr)->sources == NULL) {
		/* FIXME: set a parent window? */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("You need to to specify at least one folder as a source."));
		
		gtk_dialog_run ((GtkDialog *) dialog);
		gtk_widget_destroy (dialog);
		
		return 0;
	}
	
	return FILTER_RULE_CLASS (parent_class)->validate (fr);
}

static int
list_eq (GList *al, GList *bl)
{
	int truth = TRUE;
	
	while (truth && al && bl) {
		char *a = al->data, *b = bl->data;
		
		truth = strcmp (a, b) == 0;
		al = al->next;
		bl = bl->next;
	}
	
	return truth && al == NULL && bl == NULL;
}

static int
vfolder_eq (FilterRule *fr, FilterRule *cm)
{
        return FILTER_RULE_CLASS (parent_class)->eq (fr, cm)
		&& list_eq (((VfolderRule *) fr)->sources, ((VfolderRule *) cm)->sources);
}

static xmlNodePtr
xml_encode (FilterRule *fr)
{
	VfolderRule *vr = (VfolderRule *) fr;
	xmlNodePtr node, set, work;
	GList *l;
	
        node = FILTER_RULE_CLASS (parent_class)->xml_encode (fr);
	g_assert(node != NULL);
	set = xmlNewNode (NULL, "sources");
	xmlAddChild (node, set);
	l = vr->sources;
	while (l) {
		work = xmlNewNode (NULL, "folder");
		xmlSetProp (work, "uri", l->data);
		xmlAddChild (set, work);
		l = l->next;
	}
	
	return node;
}

static int
xml_decode (FilterRule *fr, xmlNodePtr node, struct _RuleContext *f)
{
	xmlNodePtr set, work;
	int result;
	VfolderRule *vr = (VfolderRule *)fr;
	char *uri;
	
        result = FILTER_RULE_CLASS (parent_class)->xml_decode (fr, node, f);
	if (result != 0)
		return result;
	
	set = node->children;
	while (set) {
		if (!strcmp (set->name, "sources")) {
			work = set->children;
			while (work) {
				if (!strcmp (work->name, "folder")) {
					uri = xmlGetProp (work, "uri");
					if (uri) {
						vr->sources = g_list_append (vr->sources, g_strdup (uri));
						xmlFree (uri);
					}
				}
				work = work->next;
			}
		}
		set = set->next;
	}
	return 0;
}

static void
rule_copy (FilterRule *dest, FilterRule *src)
{
	VfolderRule *vdest, *vsrc;
	GList *node;
	
	vdest = (VfolderRule *) dest;
	vsrc = (VfolderRule *) src;
	
	if (vdest->sources) {
		g_list_foreach (vdest->sources, (GFunc) g_free, NULL);
		g_list_free (vdest->sources);
		vdest->sources = NULL;
	}
	
	node = vsrc->sources;
	while (node) {
		char *uri = node->data;
		
		vdest->sources = g_list_append (vdest->sources, g_strdup (uri));
		node = node->next;
	}
	
	FILTER_RULE_CLASS (parent_class)->copy (dest, src);
}


enum {
	BUTTON_ADD,
	BUTTON_REMOVE,
	BUTTON_LAST,
};

struct _source_data {
	RuleContext *rc;
	VfolderRule *vr;
	const char *current;
	GtkListStore *model;
	GtkTreeView *list;
	GtkButton *buttons[BUTTON_LAST];
};

static void source_add(GtkWidget *widget, struct _source_data *data);
static void source_remove(GtkWidget *widget, struct _source_data *data);

static struct {
	char *name;
	GtkSignalFunc func;
} edit_buttons[] = {
	{ "source_add",    G_CALLBACK (source_add)    },
	{ "source_remove", G_CALLBACK (source_remove) },
};

static void
set_sensitive (struct _source_data *data)
{
	gtk_widget_set_sensitive ((GtkWidget *) data->buttons[BUTTON_ADD], TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) data->buttons[BUTTON_REMOVE], data->current != NULL);
}

static void
select_source (GtkWidget *list, struct _source_data *data)
{
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	gtk_tree_view_get_cursor (data->list, &path, &column);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (data->model), &iter, path);
	gtk_tree_path_free (path);
	
	gtk_tree_model_get (GTK_TREE_MODEL (data->model), &iter, 0, &data->current, -1);
	
	set_sensitive (data);
}

static void
select_source_with (GtkWidget *widget, struct _source_data *data)
{
	char *source = g_object_get_data ((GObject *) widget, "source");
	
	filter_rule_set_source ((FilterRule *) data->vr, source);
}

/* attempt to make a 'nice' folder name out of the raw uri */
static char *format_source(const char *uri)
{
	CamelURL *url = camel_url_new(uri, NULL);
	GString *out;
	char *res;

	/* bad uri */
	if (url == NULL)
		return g_strdup(uri);

	out = g_string_new(url->protocol);
	g_string_append_c(out, ':');
	if (url->user && url->host) {
		g_string_append_printf(out, "%s@%s", url->user, url->host);
		if (url->port)
			g_string_append_printf(out, ":%d", url->port);
	}
	if (url->fragment)
		g_string_append(out, url->fragment);
	else if (url->path)
		g_string_append(out, url->path);

	res = out->str;
	g_string_free(out, FALSE);

	return res;
}

static void
source_add (GtkWidget *widget, struct _source_data *data)
{
	static const char *allowed_types[] = { "mail/*", NULL };
	GNOME_Evolution_Folder *folder;
	GtkTreeSelection *selection;
	GtkWidget *window;
	GtkTreeIter iter;
	char *uri, *urinice;
	
	window = gtk_widget_get_toplevel (widget);
	gtk_widget_set_sensitive (window, FALSE);

#if 0				/* EPFIXME */
	evolution_shell_client_user_select_folder (global_shell_client, GTK_WINDOW (window),
						   _("Select Folder"), "", allowed_types, &folder);
#else
	folder = NULL;
#endif
	
	gtk_widget_set_sensitive (window, TRUE);
	
	if (folder) {
		uri = g_strdup (folder->physicalUri);
		data->vr->sources = g_list_append (data->vr->sources, uri);
		
		gtk_list_store_append (data->model, &iter);
		urinice = format_source(uri);
		gtk_list_store_set (data->model, &iter, 0, urinice, 1, uri, -1);
		g_free(urinice);
		selection = gtk_tree_view_get_selection (data->list);
		gtk_tree_selection_select_iter (selection, &iter);
		data->current = uri;
	}
	
	CORBA_free (folder);
	set_sensitive (data);
}

static void
source_remove (GtkWidget *widget, struct _source_data *data)
{
	GtkTreeSelection *selection;
	const char *source;
	GtkTreePath *path;
	GtkTreeIter iter;
	int index = 0;
	int n;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->list));
	
	source = NULL;
	while ((source = vfolder_rule_next_source (data->vr, source))) {
		path = gtk_tree_path_new ();
		gtk_tree_path_append_index (path, index);
		
		if (gtk_tree_selection_path_is_selected (selection, path)) {
			gtk_tree_model_get_iter (GTK_TREE_MODEL (data->model), &iter, path);
			
			vfolder_rule_remove_source (data->vr, source);
			gtk_list_store_remove (data->model, &iter);
			gtk_tree_path_free (path);
			
			/* now select the next rule */
			n = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (data->model), NULL);
			index = index >= n ? n - 1 : index;
			
			if (index >= 0) {
				path = gtk_tree_path_new ();
				gtk_tree_path_append_index (path, index);
				gtk_tree_model_get_iter (GTK_TREE_MODEL (data->model), &iter, path);
				gtk_tree_path_free (path);
				
				gtk_tree_selection_select_iter (selection, &iter);
				gtk_tree_model_get (GTK_TREE_MODEL (data->model), &iter, 0, &data->current, -1);
			} else {
				data->current = NULL;
			}
			
			break;
		}
		
		index++;
		gtk_tree_path_free (path);
	}
	
	set_sensitive (data);
}


GtkWidget *vfolder_editor_sourcelist_new (char *widget_name, char *string1, char *string2,
					  int int1, int int2);

GtkWidget *
vfolder_editor_sourcelist_new (char *widget_name, char *string1, char *string2, int int1, int int2)
{
	GtkWidget *table, *scrolled;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *model;
	
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	table = gtk_tree_view_new_with_model ((GtkTreeModel *) model);
	gtk_tree_view_set_headers_visible ((GtkTreeView *) table, FALSE);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes ((GtkTreeView *) table, -1,
						     _("VFolder source"), renderer,
						     "text", 0, NULL);
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) table);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	gtk_container_add (GTK_CONTAINER (scrolled), table);
	
	g_object_set_data ((GObject *) scrolled, "table", table);
	g_object_set_data ((GObject *) scrolled, "model", model);
	
	gtk_widget_show (scrolled);
	gtk_widget_show (table);
	
	return scrolled;
}


/* DO NOT internationalise these strings */
const char *source_names[] = {
	"specific",
	"local",
	"remote_active",
	"local_remote_active"
};

static GtkWidget *
get_widget (FilterRule *fr, RuleContext *rc)
{
	VfolderRule *vr = (VfolderRule *) fr;
	GtkWidget *widget, *frame, *list;
	struct _source_data *data;
	GtkOptionMenu *omenu;
	const char *source;
	GtkTreeIter iter;
	GladeXML *gui;
	int i, row;
	GList *l;
	
        widget = FILTER_RULE_CLASS (parent_class)->get_widget (fr, rc);
	
	data = g_malloc0 (sizeof (*data));
	data->rc = rc;
	data->vr = vr;
	
	gui = glade_xml_new (FILTER_GLADEDIR "/filter.glade", "vfolder_source_frame", NULL);
        frame = glade_xml_get_widget (gui, "vfolder_source_frame");
	
	g_object_set_data_full ((GObject *) frame, "data", data, g_free);
	
	for (i = 0; i < BUTTON_LAST; i++) {
		data->buttons[i] = (GtkButton *) glade_xml_get_widget (gui, edit_buttons[i].name);
		g_signal_connect (data->buttons[i], "clicked", edit_buttons[i].func, data);
	}
	
	list = glade_xml_get_widget (gui, "source_list");
	data->list = (GtkTreeView *) g_object_get_data ((GObject *) list, "table");
	data->model = (GtkListStore *) g_object_get_data ((GObject *) list, "model");
	
	source = NULL;
	while ((source = vfolder_rule_next_source (vr, source))) {
		char *nice = format_source(source);

		gtk_list_store_append (data->model, &iter);
		gtk_list_store_set (data->model, &iter, 0, nice, 1, source, -1);
		g_free(nice);
	}
	
	g_signal_connect (data->list, "cursor-changed", G_CALLBACK (select_source), data);
	
	omenu = (GtkOptionMenu *) glade_xml_get_widget (gui, "source_option");
	l = GTK_MENU_SHELL (omenu->menu)->children;
	i = 0;
	row = 0;
	while (l) {
		GtkWidget *item = GTK_WIDGET (l->data);
		
		/* make sure that the glade is in sync with the source list! */
		if (i < sizeof (source_names) / sizeof (source_names[0])) {
			g_object_set_data ((GObject *) item, "source", (char *) source_names[i]);
			if (fr->source && strcmp (source_names[i], fr->source) == 0) {
				row = i;
			}
		} else {
			g_warning ("Glade file " FILTER_GLADEDIR "/filter.glade out of sync with editor code");
		}
		
		g_signal_connect (item, "activate", G_CALLBACK (select_source_with), data);
		
		i++;
		l = l->next;
	}
	
	gtk_option_menu_set_history (omenu, row);
	if (fr->source == NULL)
		filter_rule_set_source (fr, (char *) source_names[row]);
	
	set_sensitive (data);
		
	g_object_unref (gui);

	gtk_box_pack_start (GTK_BOX (widget), frame, TRUE, TRUE, 3);
	
	return widget;
}
