/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
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

#include <glib.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <gtkhtml/gtkhtml.h>
#include <string.h>

#include <gnome-xml/tree.h>
#include <gnome-xml/parser.h>

#include "filter-arg-types.h"
#include "filter-xml.h"
#include "filter-sexp.h"
#include "filter-format.h"


#include "filter-druid.h"

static void filter_druid_class_init (FilterDruidClass *klass);
static void filter_druid_init       (FilterDruid *obj);

#define _PRIVATE(x) (((FilterDruid *)(x))->priv)

struct _FilterDruidPrivate {
	GtkWidget *notebook;
	int page;

	/* page 0 */
	GtkWidget *list0;
	GtkWidget *html0;
	GtkWidget *add0, *remove0, *up0, *down0;
	GList *items0;
	GtkFrame *listframe0;

	/* page 1 */
	GtkWidget *name1;
	GtkWidget *activate1;
	GtkHTML *html1;
};

/* forward ref's */
static void build_druid(FilterDruid *d);
static void update_display(FilterDruid *f, int initial);

/* globals */
static GnomeDialogClass *filter_druid_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_druid_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterDruid",
			sizeof (FilterDruid),
			sizeof (FilterDruidClass),
			(GtkClassInitFunc) filter_druid_class_init,
			(GtkObjectInitFunc) filter_druid_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gnome_dialog_get_type (), &type_info);
	}
	
	return type;
}

static void
filter_druid_class_init (FilterDruidClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	filter_druid_parent = gtk_type_class (gnome_dialog_get_type ());

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_druid_init (FilterDruid *obj)
{
	struct _FilterDruidPrivate *priv;

	obj->priv = g_malloc0(sizeof(*obj->priv));
	priv = _PRIVATE(obj);
}

/**
 * filter_druid_new:
 *
 * Create a new FilterDruid object.
 * 
 * Return value: A new FilterDruid widget.
 **/
FilterDruid *
filter_druid_new (void)
{
	FilterDruid *new = FILTER_DRUID ( gtk_type_new (filter_druid_get_type ()));

	build_druid(new);

	return new;
}


extern int filter_find_arg(FilterArg *a, char *name);

#include "check.xpm"
#include "blank.xpm"


struct filter_optionrule *
find_optionrule(struct filter_option *option, char *name)
{
	GList *optionrulel;
	struct filter_optionrule *or;
	
	optionrulel = option->options;
	while (optionrulel) {
		or = optionrulel->data;
		if (!strcmp(or->rule->name, name)) {
			return or;
		}
		optionrulel = g_list_next(optionrulel);
	}
	return NULL;
}

static char nooption[] = "<h2>Select option</h2><p>Select an option type from the list above.</p>"
"<p>This will set the basic rule options for your new filtering rule.</p>";
static int display_order[] = {
	FILTER_XML_MATCH,
	FILTER_XML_EXCEPT,
	FILTER_XML_ACTION,
};
static char *display_pretext[] = {
	"<b>For messages matching:</b><br><ul>",
	"<b>Unless:</b><br><ul>",
	"<b>Perform these actions:</b><br><ul>",
};
static char *display_posttext[] = {
	"</ul>",
	"</ul>",
	"</ul>",
};

void
html_write_options(GtkHTML *html, struct filter_option *option)
{
	GtkHTMLStreamHandle *stream;
	GList *optionrulel;
	int i;
	
	stream = gtk_html_begin(html, "");
	gtk_html_write(html, stream, "<body bgcolor=white alink=blue>", strlen("<body bgcolor=white alink=blue>"));
	if (option) {
		char *t;

		if (option->type == FILTER_XML_SEND) {
			t = "<p>When a message is <i>sent</i>.</p>";
		} else {
			t = "<p>When a message is <i>received</i>.</p>";
		}
		gtk_html_write(html, stream, t, strlen(t));

		for (i=0;i<sizeof(display_order)/sizeof(display_order[0]);i++) {
			int doneheader = FALSE;
			optionrulel = option->options;
			while (optionrulel) {
				struct filter_optionrule *or = optionrulel->data;
				
				if (or->rule->type == display_order[i]) {
					if (!doneheader) {
						gtk_html_write(html, stream, display_pretext[i], strlen(display_pretext[i]));
						doneheader = TRUE;
					}

					filter_description_html_write(or->rule->description, or->args, html, stream);
					gtk_html_write(html, stream, "<br>", strlen("<br>"));
				}
				optionrulel = g_list_next(optionrulel);
			}
			if (doneheader) {
				gtk_html_write(html, stream, display_posttext[i], strlen(display_posttext[i]));
			}
		}
	} else {
		gtk_html_write(html, stream, nooption, strlen(nooption));
	}
	gtk_html_end(html, stream, GTK_HTML_STREAM_OK);
}

GList *
fill_rules(GList *rules, struct filter_option *option, int type)
{
	GList *optionl, *rulel;
	GtkWidget *listitem, *hbox, *checkbox, *label;
	GList *items = NULL;

	rulel = rules;
	while (rulel) {
		struct filter_rule *fr = rulel->data;
		char *labeltext;

		if (fr->type == type) {
			int state;

			state = find_optionrule(option, fr->name) != NULL;

			labeltext = filter_description_text(fr->description, NULL);

			printf("adding rule %s\n", labeltext);
			
			hbox = gtk_hbox_new(FALSE, 3);
			checkbox = gnome_pixmap_new_from_xpm_d(state?check_xpm:blank_xpm);
			gtk_box_pack_start(GTK_BOX(hbox), checkbox, FALSE, FALSE, 0);
			label = gtk_label_new(labeltext);
			gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);
			listitem = gtk_list_item_new();
			gtk_container_add(GTK_CONTAINER(listitem), hbox);
			gtk_widget_show_all(listitem);
			
			gtk_object_set_data(GTK_OBJECT(listitem), "checkbox", checkbox);
			gtk_object_set_data(GTK_OBJECT(listitem), "checkstate", (void *)state);
			gtk_object_set_data(GTK_OBJECT(listitem), "rule", fr);
			
			items = g_list_append(items, listitem);
		}
		rulel = g_list_next(rulel);
	}
	return items;
}

GList *
fill_options(GList *options)
{
	GList *optionl, *rulel, *optionrulel;
	GtkWidget *listitem, *hbox, *checkbox, *label;
	GList *items = NULL;

	optionl = options;
	while (optionl) {
		struct filter_option *op = optionl->data;
		char *labeltext;

		labeltext = filter_description_text(op->description, NULL);
		listitem = gtk_list_item_new_with_label(labeltext);
		g_free(labeltext);
		gtk_widget_show_all(listitem);

		gtk_object_set_data(GTK_OBJECT(listitem), "option", op);
		
		items = g_list_append(items, listitem);
		optionl = g_list_next(optionl);
	}
	return items;
}

GtkWidget *list_global, *html_global;
struct filter_option *option_current;

static void
select_rule_child(GtkList *list, GtkWidget *child, FilterDruid *f)
{
	GtkWidget *w;
	struct filter_rule *fr = gtk_object_get_data(GTK_OBJECT(child), "rule");
	int state;
	struct filter_optionrule *rule;
	struct _FilterDruidPrivate *p = _PRIVATE(f);

	w = gtk_object_get_data(GTK_OBJECT(child), "checkbox");
	state = !(int) gtk_object_get_data(GTK_OBJECT(child), "checkstate");

	gnome_pixmap_load_xpm_d(GNOME_PIXMAP(w), state?check_xpm:blank_xpm);
	gtk_object_set_data(GTK_OBJECT(child), "checkstate", (void *)state);

	if (state) {
		printf("adding rule %p\n", fr);
		rule = filter_optionrule_new_from_rule(fr);
		f->option_current->options = g_list_append(f->option_current->options, rule);
	} else {
		rule = find_optionrule(f->option_current, fr->name);
		if (rule) {
			f->option_current->options = g_list_remove(f->option_current->options, rule);
			filter_clone_optionrule_free(rule);
		}
	}

	update_display(f, 0);
}



static void
select_option_child(GtkList *list, GtkWidget *child, FilterDruid *f)
{
	struct filter_option *op;
	struct filter_option *new;
	GList *optionsl;
	struct _FilterDruidPrivate *p = _PRIVATE(f);

	switch (p->page) {
	case 1:
	case 2:
	case 3:
		select_rule_child(list, child, f);
	default:
		return;
	case 0:
		break;
	}

	if (f->option_current) {
		/* free option_current copy */
		optionsl = f->option_current->options;
		while (optionsl) {
			GList *op = optionsl;
			optionsl = g_list_next(optionsl);
			g_free(op->data);
		}
		g_list_free(f->option_current->options);
		g_free(f->option_current);
		f->option_current = NULL;
	}

	if (child) {
		op = gtk_object_get_data(GTK_OBJECT(child), "option");
		
		/* clone the option */
		new = g_malloc(sizeof(*new));
		new->type = op->type;
		new->description = op->description;
		new->options = NULL;
		optionsl = op->options;
		while (optionsl) {
			struct filter_optionrule *ornew,
				*or = optionsl->data;
			ornew = filter_clone_optionrule(or);
			new->options = g_list_append(new->options, ornew);
			optionsl = g_list_next(optionsl);
		}
		f->option_current = new;
	}

	update_display(f, 0);
}

static void
unselect_option_child(GtkList *list, GtkWidget *child, FilterDruid *f)
{
	select_option_child(list, NULL, f);
}

static void
arg_link_clicked(GtkHTML *html, const char *url, FilterDruid *f)
{
	printf("url clicked: %s\n", url);
	if (!strncmp(url, "arg:", 4)) {
		FilterArg *arg;
		void *dummy;

		if (sscanf(url+4, "%p %p", &dummy, &arg)==2
			&& arg) {
			printf("arg = %p\n", arg);
			filter_arg_edit_values(arg);
			/* should have a changed signal which propagates the rewrite */
			update_display(f, 0);
		}
	}
}

static void
dialogue_clicked(FilterDruid *d, int button, void *data)
{
	GString *s = g_string_new("");
	struct _FilterDruidPrivate *p = _PRIVATE(d);
	int initial=0;

	printf("button %d clicked ...\n", button);

	g_string_free(s, TRUE);	

	switch(button) {
	case 1:
		if (p->page<4) {
			p->page++;
			initial =1;
		}
		break;
	case 0:
		if (p->page>0) {
			p->page--;
			initial = 1;
		}
		break;
	}
	update_display(d, initial);
}

static int filter_types[] = { FILTER_XML_MATCH, FILTER_XML_EXCEPT, FILTER_XML_ACTION };
static char *filter_titles[] = {
	"Select rule(s), where messages match",
	"Select rule(s), where messages do not match",
	"Select action(s) to apply to messages"

};
static void
update_display(FilterDruid *f, int initial)
{
	struct _FilterDruidPrivate *p = _PRIVATE(f);

	printf("rending page %d options\n", p->page);

	switch (p->page) {
	case 0:
		printf("option_current = %p  <###################\n", f->option_current);
		gnome_dialog_set_sensitive((GnomeDialog *)f, 0, FALSE);
		gnome_dialog_set_sensitive((GnomeDialog *)f, 1, f->option_current != NULL);

		if (initial) {
			printf("adding options\n");
			gtk_signal_handler_block_by_data((GtkObject *)p->list0, f);
			gtk_list_remove_items((GtkList *)p->list0, p->items0);
			p->items0 = fill_options(f->options);
			gtk_list_append_items((GtkList *)p->list0, p->items0);
			gtk_signal_handler_unblock_by_data((GtkObject *)p->list0, f);
			gtk_frame_set_label(p->listframe0, "Select rule type");
		}

		html_write_options((GtkHTML *)p->html0, f->option_current);
		break;
	case 1:
	case 2:
	case 3:
		gnome_dialog_set_sensitive((GnomeDialog *)f, 1, TRUE);
		gnome_dialog_set_sensitive((GnomeDialog *)f, 0, TRUE);
		gnome_dialog_set_sensitive((GnomeDialog *)f, 2, FALSE);

		if (initial) {
			printf("adding rules\n");
			gtk_signal_handler_block_by_data((GtkObject *)p->list0, f);
			gtk_list_remove_items((GtkList *)p->list0, p->items0);
			p->items0 = fill_rules(f->rules, f->option_current, filter_types[p->page-1]);
			gtk_list_append_items((GtkList *)p->list0, p->items0);
			gtk_signal_handler_unblock_by_data((GtkObject *)p->list0, f);
			gtk_frame_set_label(p->listframe0, filter_titles[p->page-1]);
			gtk_notebook_set_page(GTK_NOTEBOOK(p->notebook), 0);
		}

		html_write_options((GtkHTML *)p->html0, f->option_current);
		break;
	case 4:
		gnome_dialog_set_sensitive((GnomeDialog *)f, 1, FALSE);
		gnome_dialog_set_sensitive((GnomeDialog *)f, 0, TRUE);
		gnome_dialog_set_sensitive((GnomeDialog *)f, 2, TRUE);

		if (initial) {
			char *text;
			text = filter_description_text(f->option_current->description, NULL);
			if (text == NULL) {
				/* maybe this could fudge something out of the first
				   bits of the rule */
				if (f->option_current->type == FILTER_XML_SEND) {
					text = "Filter messages sent";
				} else {
					text = "	Filter messages received";
				}
				gtk_entry_set_text(GTK_ENTRY(p->name1), text);
			} else {
				gtk_entry_set_text(GTK_ENTRY(p->name1), text);
				g_free(text);
			}
			gtk_notebook_set_page(GTK_NOTEBOOK(p->notebook), 1);
		}

		html_write_options((GtkHTML *)p->html1, f->option_current);
		break;

	}
}

void
filter_druid_set_rules(FilterDruid *f, GList *options, GList *rules, struct filter_option *current)
{
	f->options = options;
	f->rules = rules;
	f->user = NULL;

	/* FIXME: free this list if it isn't empty ... */
	f->option_current = current;

	update_display(f, 1);
}

static void
build_druid(FilterDruid *d)
{
	GtkWidget *vbox, *frame, *scrolled_window, *list, *html, *hbox, *label, *vbox1;
	struct _FilterDruidPrivate *p = _PRIVATE(d);

	gnome_dialog_append_buttons((GnomeDialog *)d, "Prev", "Next", "Finish", "Cancel", 0);
	gnome_dialog_set_close((GnomeDialog *)d, FALSE);
	gnome_dialog_set_sensitive((GnomeDialog *)d, 0, FALSE);
	gnome_dialog_set_sensitive((GnomeDialog *)d, 1, FALSE);
	gnome_dialog_set_sensitive((GnomeDialog *)d, 2, FALSE);
	gnome_dialog_set_default((GnomeDialog *)d, 1);

	p->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(p->notebook), FALSE);
	
	/* page0, initial setup page */
	vbox = gtk_vbox_new(FALSE, 3);
	frame = gtk_frame_new("Filters");
	p->listframe0 = (GtkFrame *)frame;

	list = gtk_list_new();
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_usize(scrolled_window, 400, 150);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_set_focus_vadjustment
		(GTK_CONTAINER (list),
		 gtk_scrolled_window_get_vadjustment
		 (GTK_SCROLLED_WINDOW (scrolled_window)));
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_box_pack_start((GtkBox *)vbox, frame, TRUE, TRUE, 0);

	frame = gtk_frame_new("Filter Description (click on values to edit)");
	html = gtk_html_new();
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_usize(scrolled_window, 400, 150);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_window), html);
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_box_pack_start((GtkBox *)vbox, frame, TRUE, TRUE, 0);

	p->html0 = html;
	p->list0 = list;

	gtk_signal_connect(GTK_OBJECT(list), "select_child", select_option_child, d);
	gtk_signal_connect(GTK_OBJECT(list), "unselect_child", select_option_child, d);
/*	gtk_signal_connect(GTK_OBJECT(list), "unselect_child", unselect_option_child, d); */
	gtk_signal_connect(GTK_OBJECT(d), "clicked", dialogue_clicked, d);

	gtk_signal_connect(GTK_OBJECT(html), "link_clicked", arg_link_clicked, d);

	gtk_notebook_append_page(GTK_NOTEBOOK(p->notebook), vbox, NULL);


	/* page1, used for the final page display */
	vbox = gtk_vbox_new(FALSE, 3);

	frame = gtk_frame_new("Rule options");
	vbox1 = gtk_vbox_new(FALSE, 3);

	hbox = gtk_hbox_new(FALSE, 3);
	label = gtk_label_new("Name of rule");
	p->name1 = gtk_entry_new();
	gtk_box_pack_start((GtkBox *)hbox, label, FALSE, FALSE, 0);
	gtk_box_pack_start((GtkBox *)hbox, p->name1, TRUE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)vbox1, hbox, TRUE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(frame), vbox1);

	p->activate1 = gtk_check_button_new_with_label("Activate rule?");
	gtk_box_pack_start((GtkBox *)vbox1, p->activate1, TRUE, FALSE, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->activate1), TRUE);

	gtk_box_pack_start((GtkBox *)vbox, frame, TRUE, TRUE, 0);

	/* another copy of the filter thingy */
	frame = gtk_frame_new("Filter Description (click on values to edit)");
	html = gtk_html_new();
	p->html1 = (GtkHTML *)html;
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_usize(scrolled_window, 400, 150);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_window), html);
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_box_pack_start((GtkBox *)vbox, frame, TRUE, TRUE, 0);

	/* finish off */
	gtk_notebook_append_page(GTK_NOTEBOOK(p->notebook), vbox, NULL);

	gtk_signal_connect(GTK_OBJECT(html), "link_clicked", arg_link_clicked, d);

	gtk_widget_show_all(p->notebook);

	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox), p->notebook, TRUE, TRUE, 0);
}

#if 0
/* crappo */
static void
build_first(FilterDruid *d)
{
	GtkWidget *vbox, *frame, *scrolled_window, *list, *html, *hbox;
	struct _FilterDruidPrivate *p = _PRIVATE(d);

	gnome_dialog_append_buttons((GnomeDialog *)d, "Prev", "Next", "Finish", "Cancel", 0);
	gnome_dialog_set_close((GnomeDialog *)d, FALSE);

	p->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(p->notebook), FALSE);
	
	/* page0, initial setup page */
	hbox = gtk_hbox_new(FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 0);
	frame = gtk_frame_new("Filters");
	list = gtk_list_new();
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_set_focus_vadjustment
		(GTK_CONTAINER (list),
		 gtk_scrolled_window_get_vadjustment
		 (GTK_SCROLLED_WINDOW (scrolled_window)));
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_box_pack_start((GtkBox *)vbox, frame, TRUE, TRUE, 0);

	frame = gtk_frame_new("Filter Description");
	html = gtk_html_new();
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_window), html);
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_box_pack_start((GtkBox *)vbox, frame, TRUE, TRUE, 0);

	p->html0 = html;
	p->list0 = list;

	gtk_widget_set_usize(html, 300, 200);
	gtk_widget_set_usize(list, 300, 200);

	gtk_box_pack_start((GtkBox *)hbox, vbox, TRUE, TRUE, 0);

	/* buttons */
	vbox = gtk_vbox_new(FALSE, 0);
	
	p->add0 = gtk_button_new_with_label ("Add");
	p->remove0 = gtk_button_new_with_label ("Remove");
	p->up0 = gtk_button_new_with_label ("Up");
	p->down0 = gtk_button_new_with_label ("Down");

	gtk_box_pack_start((GtkBox *)vbox, p->add0, FALSE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)vbox, p->remove0, FALSE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)vbox, p->up0, FALSE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)vbox, p->down0, FALSE, TRUE, 0);

	gtk_box_pack_start((GtkBox *)hbox, vbox, FALSE, FALSE, 0);

	gtk_notebook_append_page(GTK_NOTEBOOK(p->notebook), hbox, NULL);

	gtk_widget_show_all(p->notebook);

	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(d)->vbox), p->notebook, TRUE, TRUE, 0);
}
#endif

void create_dialogue(void)
{
	GtkWidget *dialogue,
		*scrolled_window,
		*list,
		*html,
		*frame;

	dialogue = gnome_dialog_new("Filter Rules",
				    GNOME_STOCK_BUTTON_PREV , GNOME_STOCK_BUTTON_NEXT, 
				    "Finish", GNOME_STOCK_BUTTON_CANCEL, 0);

	list = gtk_list_new();
	frame = gtk_frame_new("Filter Type");
	scrolled_window = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_set_focus_vadjustment
		(GTK_CONTAINER (list),
		 gtk_scrolled_window_get_vadjustment
		 (GTK_SCROLLED_WINDOW (scrolled_window)));
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialogue)->vbox), frame, TRUE, TRUE, GNOME_PAD);

#if 0
	gtk_signal_connect(GTK_OBJECT(list), "select_child", select_rule_child, NULL);
	gtk_signal_connect(GTK_OBJECT(list), "unselect_child", select_rule_child, NULL);
#else
	gtk_signal_connect(GTK_OBJECT(list), "select_child", select_option_child, NULL);
	gtk_signal_connect(GTK_OBJECT(list), "unselect_child", select_option_child, NULL);
#endif

	frame = gtk_frame_new("Filter Description");
	html = gtk_html_new();
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrolled_window), html);
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialogue)->vbox), frame, TRUE, TRUE, GNOME_PAD);

	gtk_signal_connect(GTK_OBJECT(html), "link_clicked", arg_link_clicked, NULL);
	gtk_signal_connect(GTK_OBJECT(dialogue), "clicked", dialogue_clicked, NULL);

	list_global = list;
	html_global = html;

	gtk_widget_show_all(dialogue);
}

int main(int argc, char **argv)
{
	FilterSEXP *f;
	FilterSEXPResult *r;
	GList *rules, *options, *options2;
	xmlDocPtr doc, out, optionset, filteroptions;
	GString *s;

	gnome_init("Test", "0.0", argc, argv);
	gdk_rgb_init ();
	gtk_widget_set_default_colormap (gdk_rgb_get_cmap ());
	gtk_widget_set_default_visual (gdk_rgb_get_visual ());

	{
		GtkWidget *d = (GtkWidget *)filter_druid_new();

		doc = xmlParseFile("filterdescription.xml");
		rules = filter_load_ruleset(doc);
		options = filter_load_optionset(doc, rules);
		options2 = options;
		out = xmlParseFile("saveoptions.xml");
		options = filter_load_optionset(out, rules);

		filter_druid_set_rules((FilterDruid *)d, options2, rules, options->data);
/*		filter_druid_set_rules((FilterDruid *)d, options2, rules, NULL);*/

		gtk_widget_show(d);
		gtk_main();
	}
#if 0

	create_dialogue();

	doc = xmlParseFile("filterdescription.xml");
	rules = filter_load_ruleset(doc);
	options = filter_load_optionset(doc, rules);
	options2 = options;
	out = xmlParseFile("saveoptions.xml");
	options = filter_load_optionset(out, rules);

#if 0
	option_current = options->data;
	fill_rules(list_global, rules, options->data, FILTER_XML_MATCH);
#else
	option_current = NULL;
	fill_options(list_global, options2);
#endif
	gtk_main();

	while (options) {
		struct filter_option *fo = options->data;
		GList *optionrulel;

		optionrulel = fo->options;
		while (optionrulel) {
			struct filter_optionrule *or = optionrulel->data;

			printf("formatting rule: %s\n", or->rule->name);

			/*filter_description_text(or->rule->description, or->args);*/
			filter_description_html_write(or->rule->description, or->args, NULL, NULL);

			optionrulel = g_list_next(optionrulel);
		}
		options = g_list_next(options);
	}

	return 0;

	s = g_string_new("");
	g_string_append(s, "");

	printf("total rule = '%s'\n", s->str);

	f = filter_sexp_new();
	filter_sexp_add_variable(f, 0, "sender", NULL);
	filter_sexp_add_variable(f, 0, "receipient", NULL);
	filter_sexp_add_variable(f, 0, "folder", NULL);

	/* simple functions */
	filter_sexp_add_function(f, 0, "header-get", NULL, NULL);
	filter_sexp_add_function(f, 0, "header-contains", NULL, NULL);
	filter_sexp_add_function(f, 0, "copy-to", NULL, NULL);

	filter_sexp_add_ifunction(f, 0, "set", NULL, NULL);

	/* control functions */
	filter_sexp_add_ifunction(f, 0, "match-all", NULL, NULL);
	filter_sexp_add_ifunction(f, 0, "match", NULL, NULL);
	filter_sexp_add_ifunction(f, 0, "action", NULL, NULL);
	filter_sexp_add_ifunction(f, 0, "except", NULL, NULL);

	filter_sexp_input_text(f, s->str, strlen(s->str));
	filter_sexp_parse(f);
#endif
	
}
