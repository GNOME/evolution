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

#include <gal/widgets/e-unicode.h>
#include "filter-editor.h"
#include "filter-context.h"
#include "filter-filter.h"

#define d(x)

#if 0
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
		
		type = gtk_type_unique(gnome_dialog_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_editor_class_init (FilterEditorClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(gnome_dialog_get_type ());

	object_class->finalize = filter_editor_finalise;
	/* override methods */

	/* signals */

	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
filter_editor_init (FilterEditor *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
filter_editor_finalise (GtkObject *obj)
{
	FilterEditor *o = (FilterEditor *)obj;

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
filter_editor_new (void)
{
	FilterEditor *o = (FilterEditor *)gtk_type_new(filter_editor_get_type ());
	return o;
}

#endif


enum {
	BUTTON_ADD,
	BUTTON_EDIT,
	BUTTON_DELETE,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LAST
};

struct _editor_data {
	RuleContext *f;
	FilterRule *current;
	GtkList *list;
	GtkButton *buttons[BUTTON_LAST];
	char *current_source;
};

static void set_sensitive (struct _editor_data *data);

static void
rule_add (GtkWidget *widget, struct _editor_data *data)
{
	FilterFilter *rule;
	int result;
	GtkWidget *gd;
	GtkWidget *w;
	FilterPart *part;
	
	d(printf ("add rule\n"));
	/* create a new rule with 1 match and 1 action */
	rule = filter_filter_new ();
	filter_rule_set_source((FilterRule *)rule, data->current_source);
	
	part = rule_context_next_part (data->f, NULL);
	filter_rule_add_part ((FilterRule *)rule, filter_part_clone (part));
	part = filter_context_next_action ((FilterContext *)data->f, NULL);
	filter_filter_add_action (rule, filter_part_clone (part));
	
	w = filter_rule_get_widget ((FilterRule *)rule, data->f);
	gd = gnome_dialog_new (_("Add Rule"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	gtk_window_set_policy(GTK_WINDOW(gd), FALSE, TRUE, FALSE);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (gd)->vbox), w, TRUE, TRUE, 0);
	gtk_widget_show (gd);
	
	result = gnome_dialog_run_and_close (GNOME_DIALOG (gd));
	
	if (result == 0) {
		GtkListItem *item;
		GList *l = NULL;
		gchar *s = e_utf8_to_gtk_string (GTK_WIDGET (data->list), ((FilterRule *)rule)->name);
		
		item = (GtkListItem *) gtk_list_item_new_with_label (s);
		g_free (s);
		
		gtk_object_set_data (GTK_OBJECT (item), "rule", rule);
		gtk_widget_show (GTK_WIDGET (item));
		
		l = g_list_append (l, item);
		gtk_list_append_items (data->list, l);
		gtk_list_select_child (data->list, GTK_WIDGET (item));
		
		data->current = (FilterRule *)rule;
		rule_context_add_rule (data->f, (FilterRule *)rule);
		set_sensitive (data);
	} else {
		gtk_object_unref (GTK_OBJECT (rule));
	}
}

static void
rule_edit (GtkWidget *widget, struct _editor_data *data)
{
	GtkWidget *w;
	int result;
	GtkWidget *gd;
	FilterRule *rule;
	int pos;
	
	d(printf ("edit rule\n"));
	rule = data->current;
	w = filter_rule_get_widget (rule, data->f);
	gd = gnome_dialog_new(_("Edit Rule"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	gtk_window_set_policy(GTK_WINDOW(gd), FALSE, TRUE, FALSE);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (gd)->vbox), w, TRUE, TRUE, 0);
	gtk_widget_show (gd);
	result = gnome_dialog_run_and_close (GNOME_DIALOG (gd));
	
	if (result == 0) {
		pos = rule_context_get_rank_rule(data->f, data->current, data->current_source);
		if (pos != -1) {
			GtkListItem *item = g_list_nth_data (data->list->children, pos);
			gchar *s = e_utf8_to_gtk_string ((GtkWidget *) item, data->current->name);
			gtk_label_set_text((GtkLabel *)(((GtkBin *)item)->child), s);
			g_free (s);
		}
	 }
}

static void
rule_delete (GtkWidget *widget, struct _editor_data *data)
{
	int pos;
	GList *l;
	GtkListItem *item;

	d(printf("ddelete rule\n"));
	pos = rule_context_get_rank_rule(data->f, data->current, data->current_source);
	if (pos != -1) {
		rule_context_remove_rule (data->f, data->current);
		
		item = g_list_nth_data (data->list->children, pos);
		l = g_list_append (NULL, item);
		gtk_list_remove_items (data->list, l);
		g_list_free (l);
		
		gtk_object_unref (GTK_OBJECT (data->current));
		data->current = NULL;
	}
	set_sensitive (data);
}

static void
rule_move (struct _editor_data *data, int from, int to)
{
	GList *l;
	GtkListItem *item;
	
	d(printf("moving %d to %d\n", from, to));
	rule_context_rank_rule (data->f, data->current, to);
	
	item = g_list_nth_data (data->list->children, from);
	l = g_list_append (NULL, item);
	gtk_list_remove_items_no_unref (data->list, l);
	gtk_list_insert_items (data->list, l, to);			      
	gtk_list_select_child (data->list, GTK_WIDGET (item));
	set_sensitive (data);
}

static void
rule_up (GtkWidget *widget, struct _editor_data *data)
{
	int pos;
	
	d(printf("up rule\n"));
	pos = rule_context_get_rank_rule(data->f, data->current, data->current_source);
	if (pos > 0) {
		rule_move (data, pos, pos-1);
	}
}

static void
rule_down (GtkWidget *widget, struct _editor_data *data)
{
	int pos;
	
	d(printf ("down rule\n"));
	pos = rule_context_get_rank_rule(data->f, data->current, data->current_source);
	rule_move (data, pos, pos+1);
}

static struct {
	char *name;
	GtkSignalFunc func;
} edit_buttons[] = {
	{ "rule_add", rule_add },
	{ "rule_edit", rule_edit },
	{ "rule_delete", rule_delete },
	{ "rule_up", rule_up },
	{ "rule_down", rule_down },
};

static void
set_sensitive (struct _editor_data *data)
{
	FilterRule *rule = NULL;
	int index = -1, count = 0;
	
	while ((rule = rule_context_next_rule (data->f, rule, data->current_source))) {
		if (rule == data->current)
			index=count;
		count++;
	}
	d(printf ("index = %d count=%d\n", index, count));
	count--;
	gtk_widget_set_sensitive (GTK_WIDGET (data->buttons[BUTTON_EDIT]), index != -1);
	gtk_widget_set_sensitive (GTK_WIDGET (data->buttons[BUTTON_DELETE]), index != -1);
	gtk_widget_set_sensitive (GTK_WIDGET (data->buttons[BUTTON_UP]), index > 0);
	gtk_widget_set_sensitive (GTK_WIDGET (data->buttons[BUTTON_DOWN]), index >= 0 && index < count);
}

static void
select_rule (GtkWidget *w, GtkWidget *child, struct _editor_data *data)
{
	data->current = gtk_object_get_data (GTK_OBJECT (child), "rule");
	
	if (data->current)
		d(printf ("seledct rule: %s\n", data->current->name));
	else
		d(printf ("bad data?\n"));
	
	set_sensitive (data);
}

static void
double_click (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->type == GDK_2BUTTON_PRESS)
		rule_edit (widget, user_data);
}

/* FIXME: we need a way to change a rule from one source type
 * to a different type. Maybe keep the selected ones? 
 */

static void
select_source (GtkMenuItem *mi, struct _editor_data *data)
{
	FilterRule *rule = NULL;
	GList *newitems = NULL;
	char *source;
	
	source = gtk_object_get_data (GTK_OBJECT (mi), "source");
	
	gtk_list_clear_items (GTK_LIST (data->list), 0, -1);
	
	d(printf ("Checking for rules that are of type %d\n", source));
	while ((rule = rule_context_next_rule (data->f, rule, source)) != NULL) {
		GtkWidget *item;
		char *s;
		
		d(printf ("   hit %s (%d)\n", rule->name, source));
		s = e_utf8_to_gtk_string (GTK_WIDGET (data->list), rule->name);
		item = gtk_list_item_new_with_label (s);
		g_free (s);
		gtk_object_set_data (GTK_OBJECT (item), "rule", rule);
		gtk_widget_show (GTK_WIDGET (item));
		newitems = g_list_append (newitems, item);
	}
	
	gtk_list_append_items (data->list, newitems);
	data->current_source = source;
	data->current = NULL;
	set_sensitive (data);
}

static char *source_names[] = {
	"incoming",
	"demand",
	"outgoing"
};

GtkWidget *
filter_editor_construct (struct _FilterContext *f)
{
	GladeXML *gui;
	GtkWidget *d, *w, *b, *firstitem = NULL;
	GList *l;
	struct _editor_data *data;
	int i;
	
	g_assert (IS_FILTER_CONTEXT (f));
	
	data = g_malloc0 (sizeof (*data));
	data->f = (RuleContext *)f;
	
	gui = glade_xml_new (FILTER_GLADEDIR "/filter.glade", "edit_filter");
        d = glade_xml_get_widget (gui, "edit_filter");
	gtk_object_set_data_full (GTK_OBJECT (d), "data", data, g_free);
	
	gtk_window_set_title (GTK_WINDOW (d), "Edit Filters");
	for (i = 0; i < BUTTON_LAST; i++) {
		data->buttons[i] = (GtkButton *)w = glade_xml_get_widget (gui, edit_buttons[i].name);
		gtk_signal_connect (GTK_OBJECT (w), "clicked", edit_buttons[i].func, data);
	}
	
        w = glade_xml_get_widget (gui, "filter_source");
	l = GTK_MENU_SHELL (GTK_OPTION_MENU (w)->menu)->children;
	i = 0;
	while (l) {
		b = GTK_WIDGET (l->data);
		
		if (i == 0)
			firstitem = b;
		
		/* make sure that the glade is in sync with the source list! */
		if (i < sizeof(source_names)/sizeof(source_names[0])) {
			gtk_object_set_data (GTK_OBJECT (b), "source", source_names[i]);
		} else {
			g_warning("Glade file " FILTER_GLADEDIR "/filter.glade out of sync with editor code");
		}
		gtk_signal_connect (GTK_OBJECT (b), "activate", select_source, data);
		
		i++;
		l = l->next;
	}
	
        w = glade_xml_get_widget (gui, "rule_list");
	data->list = GTK_LIST (w);
	gtk_signal_connect (GTK_OBJECT (w), "select_child", select_rule, data);
	gtk_signal_connect (GTK_OBJECT (w), "button_press_event", double_click, data);
	select_source (GTK_MENU_ITEM (firstitem), data);
	
	gtk_object_unref (GTK_OBJECT (gui));
	
	return d;
}
