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

#include "score-editor.h"
#include "score-context.h"
#include "score-rule.h"

#define d(x)

#if 0
static void score_editor_class_init	(ScoreEditorClass *class);
static void score_editor_init	(ScoreEditor *gspaper);

static GnomeDialogClass *parent_class;

guint
score_editor_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"ScoreEditor",
			sizeof(ScoreEditor),
			sizeof(ScoreEditorClass),
			(GtkClassInitFunc)score_editor_class_init,
			(GtkObjectInitFunc)score_editor_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(gnome_dialog_get_type (), &type_info);
	}
	
	return type;
}

static void
score_editor_class_init (ScoreEditorClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(gnome_dialog_get_type ());

	/* override methods */

}

static void
score_editor_init (ScoreEditor *o)
{
}

/**
 * score_editor_new:
 *
 * Create a new ScoreEditor object.
 * 
 * Return value: A new #ScoreEditor object.
 **/
ScoreEditor *
score_editor_new(void)
{
	ScoreEditor *o = (ScoreEditor *)gtk_type_new(score_editor_get_type ());
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
};

static void set_sensitive(struct _editor_data *data);

static void rule_add(GtkWidget *widget, struct _editor_data *data)
{
	ScoreRule *rule;
	int result;
	GnomeDialog *gd;
	GtkWidget *w;
	FilterPart *part;

	d(printf("add rule\n"));
	/* create a new rule with 1 match and 1 action */
	rule = score_rule_new();

	part = rule_context_next_part(data->f, NULL);
	filter_rule_add_part((FilterRule *)rule, filter_part_clone(part));

	w = filter_rule_get_widget((FilterRule *)rule, data->f);
	gd = (GnomeDialog *)gnome_dialog_new(_("Add Rule"),
					     GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL,
					     NULL);
	gtk_box_pack_start((GtkBox *)gd->vbox, w, FALSE, TRUE, 0);
	gtk_widget_show((GtkWidget *)gd);
	result = gnome_dialog_run_and_close(gd);
	if (result == 0) {
		GtkListItem *item = (GtkListItem *)gtk_list_item_new_with_label(((FilterRule *)rule)->name);
		GList *l = NULL;

		gtk_object_set_data((GtkObject *)item, "rule", rule);
		gtk_widget_show((GtkWidget *)item);
		l = g_list_append(l, item);
		gtk_list_append_items(data->list, l);
		gtk_list_select_child(data->list, (GtkWidget *)item);
		data->current = (FilterRule *)rule;
		rule_context_add_rule(data->f, (FilterRule *)rule);
		set_sensitive(data);
	} else {
		gtk_object_unref((GtkObject *)rule);
	}
}

static void rule_edit(GtkWidget *widget, struct _editor_data *data)
{
	GtkWidget *w;
	int result;
	GnomeDialog *gd;
	FilterRule *rule;
	int pos;

	d(printf("edit rule\n"));
	rule = data->current;
	w = filter_rule_get_widget(rule, data->f);
	gd = (GnomeDialog *)gnome_dialog_new(_("Edit Score Rule"), GNOME_STOCK_BUTTON_OK, NULL);
	gtk_box_pack_start((GtkBox *)gd->vbox, w, FALSE, TRUE, 0);
	gtk_widget_show((GtkWidget *)gd);
	result = gnome_dialog_run_and_close(gd);

	if (result == 0) {
		pos = rule_context_get_rank_rule(data->f, data->current);
		if (pos != -1) {
			GtkListItem *item = g_list_nth_data(data->list->children, pos);
			gtk_label_set_text((GtkLabel *)(((GtkBin *)item)->child), data->current->name);
		}
	 }
}

static void rule_delete(GtkWidget *widget, struct _editor_data *data)
{
	int pos;
	GList *l;
	GtkListItem *item;

	d(printf("ddelete rule\n"));
	pos = rule_context_get_rank_rule(data->f, data->current);
	if (pos != -1) {
		rule_context_remove_rule(data->f, data->current);

		item = g_list_nth_data(data->list->children, pos);
		l = g_list_append(NULL, item);
		gtk_list_remove_items(data->list, l);
		g_list_free(l);

		gtk_object_unref((GtkObject *)data->current);
		data->current = NULL;
	}
	set_sensitive(data);
}

static void rule_move(struct _editor_data *data, int from, int to)
{
	GList *l;
	GtkListItem *item;

	d(printf("moving %d to %d\n", from, to));
	rule_context_rank_rule(data->f, data->current, to);
	
	item = g_list_nth_data(data->list->children, from);
	l = g_list_append(NULL, item);
	gtk_list_remove_items_no_unref(data->list, l);
	gtk_list_insert_items(data->list, l, to);			      
	gtk_list_select_child(data->list, (GtkWidget *)item);
	set_sensitive(data);
}

static void rule_up(GtkWidget *widget, struct _editor_data *data)
{
	int pos;

	d(printf("up rule\n"));
	pos = rule_context_get_rank_rule(data->f, data->current);
	if (pos>0) {
		rule_move(data, pos, pos-1);
	}
}

static void rule_down(GtkWidget *widget, struct _editor_data *data)
{
	int pos;

	d(printf("down rule\n"));
	pos = rule_context_get_rank_rule(data->f, data->current);
	rule_move(data, pos, pos+1);
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
set_sensitive(struct _editor_data *data)
{
	FilterRule *rule = NULL;
	int index=-1, count=0;

	while ((rule = rule_context_next_rule(data->f, rule))) {
		if (rule == data->current)
			index=count;
		count++;
	}
	d(printf("index = %d count=%d\n", index, count));
	count--;
	gtk_widget_set_sensitive((GtkWidget *)data->buttons[BUTTON_EDIT], index != -1);
	gtk_widget_set_sensitive((GtkWidget *)data->buttons[BUTTON_DELETE], index != -1);
	gtk_widget_set_sensitive((GtkWidget *)data->buttons[BUTTON_UP], index > 0);
	gtk_widget_set_sensitive((GtkWidget *)data->buttons[BUTTON_DOWN], index >=0 && index<count);
}

static void
select_rule(GtkWidget *w, GtkWidget *child, struct _editor_data *data)
{
	data->current = gtk_object_get_data((GtkObject *)child, "rule");
	if (data->current)
		d(printf("seledct rule: %s\n", data->current->name));
	else
		d(printf("bad data?\n"));
	set_sensitive(data);
}

GtkWidget	*score_editor_construct	(struct _ScoreContext *f)
{
	GladeXML *gui;
	GtkWidget *d, *w;
	GList *l;
	FilterRule *rule = NULL;
	struct _editor_data *data;
	int i;

	g_assert(IS_SCORE_CONTEXT(f));

	data = g_malloc0(sizeof(*data));
	data->f = (RuleContext *)f;

	gui = glade_xml_new(FILTER_GLADEDIR "/filter.glade", "edit_vfolder");
        d = glade_xml_get_widget (gui, "edit_vfolder");
	gtk_object_set_data_full((GtkObject *)d, "data", data, g_free);

	gtk_window_set_title((GtkWindow *)d, "Edit Score List");
	for (i=0;i<BUTTON_LAST;i++) {
		data->buttons[i] = (GtkButton *)w = glade_xml_get_widget (gui, edit_buttons[i].name);
		gtk_signal_connect((GtkObject *)w, "clicked", edit_buttons[i].func, data);
	}

        w = glade_xml_get_widget (gui, "rule_list");
	data->list = (GtkList *)w;
	l = NULL;
	while ((rule = rule_context_next_rule((RuleContext *)f, rule))) {
		GtkListItem *item = (GtkListItem *)gtk_list_item_new_with_label(rule->name);
		gtk_object_set_data((GtkObject *)item, "rule", rule);
		gtk_widget_show((GtkWidget *)item);
		l = g_list_append(l, item);
	}
	gtk_list_append_items(data->list, l);
	gtk_signal_connect((GtkObject *)w, "select_child", select_rule, data);

	set_sensitive(data);
	gtk_object_unref((GtkObject *)gui);

	return d;
}
