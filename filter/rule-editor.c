/*
 *  Copyright (C) 2000 Helix Code Inc.
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

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <glade/glade.h>

#include <gal/widgets/e-unicode.h>
#include "rule-editor.h"
#include "rule-context.h"
#include "filter-rule.h"

#define d(x)

static void set_source(RuleEditor *re, const char *source);
static void set_sensitive(RuleEditor *re);
static FilterRule *create_rule(RuleEditor *re);

static void rule_editor_class_init(RuleEditorClass *class);
static void rule_editor_init(RuleEditor *gspaper);
static void rule_editor_finalise(GtkObject *obj);

#define _PRIVATE(x)(((RuleEditor *)(x))->priv)

enum {
	BUTTON_ADD,
	BUTTON_EDIT,
	BUTTON_DELETE,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LAST
};

struct _RuleEditorPrivate {
	GtkButton *buttons[BUTTON_LAST];
};

static GnomeDialogClass *parent_class;

enum {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
rule_editor_get_type(void)
{
	static guint type = 0;
	
	if(!type) {
		GtkTypeInfo type_info = {
			"RuleEditor",
			sizeof(RuleEditor),
			sizeof(RuleEditorClass),
			(GtkClassInitFunc)rule_editor_class_init,
			(GtkObjectInitFunc)rule_editor_init,
			(GtkArgSetFunc)NULL,
			(GtkArgGetFunc)NULL
		};
		
		type = gtk_type_unique(gnome_dialog_get_type(), &type_info);
	}
	
	return type;
}

static void
rule_editor_class_init(RuleEditorClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *)class;
	parent_class = gtk_type_class(gnome_dialog_get_type());
	
	object_class->finalize = rule_editor_finalise;

	/* override methods */
	class->set_source = set_source;
	class->set_sensitive = set_sensitive;
	class->create_rule = create_rule;

	/* signals */
	
	gtk_object_class_add_signals(object_class, signals, LAST_SIGNAL);
}

static void
rule_editor_init(RuleEditor *o)
{
	o->priv = g_malloc0(sizeof(*o->priv));
}

static void
rule_editor_finalise(GtkObject *obj)
{
	RuleEditor *o = (RuleEditor *)obj;

	gtk_object_unref((GtkObject *)o->context);

	g_free(o->priv);

       ((GtkObjectClass *)(parent_class))->finalize(obj);
}

/**
 * rule_editor_new:
 *
 * Create a new RuleEditor object.
 * 
 * Return value: A new #RuleEditor object.
 **/
RuleEditor *
rule_editor_new(RuleContext *f, const char *source)
{
	GladeXML *gui;
	RuleEditor *o = (RuleEditor *)gtk_type_new(rule_editor_get_type());
	GtkWidget *w;

	gui = glade_xml_new(FILTER_GLADEDIR "/filter.glade", "rule_editor");
	rule_editor_construct(o, f, gui, source);

        w = glade_xml_get_widget(gui, "rule_frame");
	gtk_frame_set_label((GtkFrame *)w, _("Rules"));

	gtk_object_unref((GtkObject *)gui);

	return o;
}

/* used internally by implementations if required */
void
rule_editor_set_sensitive(RuleEditor *re)
{
	return ((RuleEditorClass *)((GtkObject *)re)->klass)->set_sensitive(re);
}

/* used internally by implementations */
void
rule_editor_set_source(RuleEditor *re, const char *source)
{
	return ((RuleEditorClass *)((GtkObject *)re)->klass)->set_source(re, source);
}

/* factory method for "add" button */
FilterRule *
rule_editor_create_rule(RuleEditor *re)
{
	return ((RuleEditorClass *)((GtkObject *)re)->klass)->create_rule(re);
}

static FilterRule *
create_rule(RuleEditor *re)
{
	FilterRule *rule = filter_rule_new();
	FilterPart *part;

	/* create a rule with 1 part in it */
	part = rule_context_next_part(re->context, NULL);
	filter_rule_add_part(rule, filter_part_clone(part));

	return rule;
}

static void
add_editor_clicked (GtkWidget *dialog, int button, RuleEditor *re)
{
	GtkWidget *item;
	GList *l = NULL;
	char *string;
	
	switch (button) {
	case 0:
		if (!filter_rule_validate (re->edit)) {
			/* no need to popup a dialog because the validate code does that. */
			return;
		}
		
		gtk_object_ref (GTK_OBJECT (re->edit));
		string = e_utf8_to_gtk_string (GTK_WIDGET (re->list), re->edit->name);
		item = gtk_list_item_new_with_label (string);
		g_free (string);
		
		gtk_object_set_data (GTK_OBJECT (item), "rule", re->edit);
		gtk_widget_show (item);
		
		l = g_list_append (l, GTK_LIST_ITEM (item));
		
		gtk_list_append_items (re->list, l);
		gtk_list_select_child (re->list, item);
		
		re->current = re->edit;
		rule_context_add_rule (re->context, re->current);
	case 1:
	default:
		gnome_dialog_close (GNOME_DIALOG (dialog));
	case -1:
		gtk_object_unref (GTK_OBJECT (re->edit));
		re->edit = NULL;
		
		rule_editor_set_sensitive (re);
	}
}

static void
rule_add (GtkWidget *widget, RuleEditor *re)
{
	GtkWidget *dialog;
	GtkWidget *rules;
	
	d(printf ("add rule\n"));
	
	re->edit = rule_editor_create_rule (re);
	filter_rule_set_source (re->edit, re->source);
	rules = filter_rule_get_widget (re->edit, re->context);
	
	dialog = gnome_dialog_new (_("Add Rule"),
				   GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);
	
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), rules, TRUE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (dialog), "clicked", add_editor_clicked, re);
	
	gtk_widget_show (dialog);
}

static void
edit_editor_clicked (GtkWidget *dialog, int button, RuleEditor *re)
{
	GtkWidget *item;
	char *string;
	int pos;
	
	switch (button) {
	case 0:
		if (!filter_rule_validate (re->edit)) {
			/* no need to popup a dialog because the validate code does that. */
			return;
		}
		
		pos = rule_context_get_rank_rule (re->context, re->current, re->source);
		if (pos != -1) {
			item = g_list_nth_data (((GtkList *)re->list)->children, pos);
			string = e_utf8_to_gtk_string (GTK_WIDGET (item), re->current->name);
			gtk_label_set_text (GTK_LABEL (GTK_BIN (item)->child), string);
			g_free (string);
		}
	case 1:
	default:
		gnome_dialog_close (GNOME_DIALOG (dialog));
	case -1:
		gtk_object_unref (GTK_OBJECT (re->edit));
		re->edit = NULL;
		
		rule_editor_set_sensitive (re);
	}
}

static void
rule_edit (GtkWidget *widget, RuleEditor *re)
{
	GtkWidget *dialog;
	GtkWidget *rules;
	
	if (re->current == NULL)
		return;
	
	re->edit = re->current;
	gtk_object_ref (GTK_OBJECT (re->edit));
	
	rules = filter_rule_get_widget (re->current, re->context);
	dialog = gnome_dialog_new (_("Edit Rule"),
				   GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);
	
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), rules, TRUE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (dialog), "clicked", edit_editor_clicked, re);
	
	gtk_widget_show (dialog);
}

static void
rule_delete (GtkWidget *widget, RuleEditor *re)
{
	int pos;
	GList *l;
	GtkListItem *item;
	
	d(printf ("delete rule\n"));
	pos = rule_context_get_rank_rule (re->context, re->current, re->source);
	if (pos != -1) {
		rule_context_remove_rule (re->context, re->current);
		
		item = g_list_nth_data (((GtkList *)re->list)->children, pos);
		l = g_list_append (NULL, item);
		gtk_list_remove_items (re->list, l);
		g_list_free (l);
		
		gtk_object_unref (GTK_OBJECT (re->current));
		re->current = NULL;
	}
	
	rule_editor_set_sensitive (re);
}

static void
rule_move (RuleEditor *re, int from, int to)
{
	GList *l;
	GtkListItem *item;
	
	d(printf ("moving %d to %d\n", from, to));
	rule_context_rank_rule (re->context, re->current, to);
	
	item = g_list_nth_data (re->list->children, from);
	l = g_list_append (NULL, item);
	gtk_list_remove_items_no_unref (re->list, l);
	gtk_list_insert_items (re->list, l, to);			      
	gtk_list_select_child (re->list, GTK_WIDGET (item));
	
	rule_editor_set_sensitive (re);
}

static void
rule_up (GtkWidget *widget, RuleEditor *re)
{
	int pos;
	
	d(printf ("up rule\n"));
	pos = rule_context_get_rank_rule (re->context, re->current, re->source);
	if (pos > 0)
		rule_move (re, pos, pos - 1);
}

static void
rule_down (GtkWidget *widget, RuleEditor *re)
{
	int pos;
	
	d(printf ("down rule\n"));
	pos = rule_context_get_rank_rule (re->context, re->current, re->source);
	rule_move (re, pos, pos + 1);
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
set_sensitive (RuleEditor *re)
{
	FilterRule *rule = NULL;
	int index = -1, count = 0;
	
	while ((rule = rule_context_next_rule(re->context, rule, re->source))) {
		if (rule == re->current)
			index = count;
		count++;
	}
	
	d(printf("index = %d count=%d\n", index, count));
	
	count--;
	
	gtk_widget_set_sensitive(GTK_WIDGET(re->priv->buttons[BUTTON_EDIT]), index != -1);
	gtk_widget_set_sensitive(GTK_WIDGET(re->priv->buttons[BUTTON_DELETE]), index != -1);
	gtk_widget_set_sensitive(GTK_WIDGET(re->priv->buttons[BUTTON_UP]), index > 0);
	gtk_widget_set_sensitive(GTK_WIDGET(re->priv->buttons[BUTTON_DOWN]), index >= 0 && index < count);
}


static void
select_rule (GtkWidget *w, GtkWidget *child, RuleEditor *re)
{
	re->current = gtk_object_get_data (GTK_OBJECT (child), "rule");
	
	g_assert (re->current);
	
	rule_editor_set_sensitive (re);
}

static void
double_click (GtkWidget *widget, GdkEventButton *event, RuleEditor *re)
{
	if (re->current && event->type == GDK_2BUTTON_PRESS)
		rule_edit (widget, re);
}

static void
set_source(RuleEditor *re, const char *source)
{
	FilterRule *rule = NULL;
	GList *newitems = NULL;
	
	gtk_list_clear_items(GTK_LIST(re->list), 0, -1);
	
	d(printf("Checking for rules that are of type %s\n", source));
	while ((rule = rule_context_next_rule(re->context, rule, source)) != NULL) {
		GtkWidget *item;
		char *s;
		
		d(printf("   hit %s(%d)\n", rule->name, source));
		s = e_utf8_to_gtk_string(GTK_WIDGET(re->list), rule->name);
		item = gtk_list_item_new_with_label(s);
		g_free(s);
		gtk_object_set_data(GTK_OBJECT(item), "rule", rule);
		gtk_widget_show(GTK_WIDGET(item));
		newitems = g_list_append(newitems, item);
	}
	
	gtk_list_append_items(re->list, newitems);
	g_free(re->source);
	re->source = g_strdup(source);
	re->current = NULL;
	rule_editor_set_sensitive(re);
}

void
rule_editor_construct(RuleEditor *re, RuleContext *context, GladeXML *gui, const char *source)
{
	GtkWidget *w;
	int i;

	re->context = context;
	gtk_object_ref((GtkObject *)context);

        w = glade_xml_get_widget(gui, "rule_editor");
	gtk_box_pack_start((GtkBox *)GNOME_DIALOG(re)->vbox, w, TRUE, TRUE, 0);
	
	for (i=0; i<BUTTON_LAST; i++) {
		re->priv->buttons[i] = (GtkButton *)w = glade_xml_get_widget(gui, edit_buttons[i].name);
		gtk_signal_connect(GTK_OBJECT(w), "clicked", edit_buttons[i].func, re);
	}

        re->list = (GtkList *) w = glade_xml_get_widget(gui, "rule_list");
	gtk_signal_connect(GTK_OBJECT(w), "select_child", select_rule, re);
	gtk_signal_connect(GTK_OBJECT(w), "button_press_event", double_click, re);

	rule_editor_set_source(re, source);

	gnome_dialog_append_buttons((GnomeDialog *)re, GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, 0);
}
