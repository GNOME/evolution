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
#include "filter-format.h"


#include "filter-druid.h"

static void filter_druid_class_init (FilterDruidClass *klass);
static void filter_druid_init       (FilterDruid *obj);

#define _PRIVATE(x) (((FilterDruid *)(x))->priv)

struct _FilterDruidPrivate {
	GtkWidget *notebook;
	int page;

	char *default_html;

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
static GtkNotebookClass *filter_druid_parent;

enum SIGNALS {
	OPTION_SELECTED,
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
		
		type = gtk_type_unique (gtk_notebook_get_type (), &type_info);
	}
	
	return type;
}

static void
object_destroy(FilterDruid *obj)
{
	struct _FilterDruidPrivate *p = _PRIVATE(obj);

	g_free(p->default_html);

	gtk_signal_disconnect_by_data((GtkObject *)p->list0, obj);

	/* FIXME: free lists? */

	GTK_OBJECT_CLASS(filter_druid_parent)->destroy(obj);
}

static void
filter_druid_class_init (FilterDruidClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	
	filter_druid_parent = gtk_type_class (gtk_notebook_get_type ());

	object_class->destroy = object_destroy;

	signals[OPTION_SELECTED] =
		gtk_signal_new ("option_selected",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FilterDruidClass, option_selected),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);

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
html_write_options(GtkHTML *html, struct filter_option *option, char *def)
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
		if (def == NULL)
			def = "Select options.";
		gtk_html_write(html, stream, def, strlen(def));
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
		printf("freeing current option\n");
		/* free option_current copy */
		optionsl = f->option_current->options;
		while (optionsl) {
			GList *op = optionsl;
			optionsl = g_list_next(optionsl);
			filter_clone_optionrule_free(op->data);
		}
		g_list_free(f->option_current->options);
		g_free(f->option_current);
		f->option_current = NULL;
	}

	if (child) {
		op = gtk_object_get_data(GTK_OBJECT(child), "option");

		printf("option = %p\n", op);
		
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

		gtk_signal_emit(GTK_OBJECT(f), signals[OPTION_SELECTED], op);
	}

	update_display(f, 0);
}

static void
unselect_option_child(GtkList *list, GtkWidget *child, FilterDruid *f)
{
	printf("unselect option child\n");
	select_option_child(list, NULL, f);
}

static void
arg_changed(FilterArg *arg, FilterDruid *f)
{
	FilterArg *orig;

	printf("value changed!!!\n");

	orig = gtk_object_get_data(arg, "origin");
	if (orig) {
		filter_arg_copy(orig, arg);
		update_display(f, 0);
	} else {
		/* FIXME: uh, what the fuck to do here? */
		update_display(f, 0);
	}
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
			FilterArg *orig;

			printf("arg = %p\n", arg);

			gtk_signal_connect((GtkObject *)arg, "changed", arg_changed, f);
			filter_arg_edit_values(arg);
		}
	}
}

static void
option_name_changed(GtkEntry *entry, FilterDruid *f)
{
	struct filter_desc *desc;

	printf("name chaned: %s\n", gtk_entry_get_text(entry));

	if (f->option_current) {
		/* FIXME: lots of memory leaks */
		desc = g_malloc0(sizeof(*desc));
		desc->data = g_strdup(gtk_entry_get_text(entry));
		desc->type = FILTER_XML_TEXT;
		desc->vartype = -1;
		desc->varname = NULL;
		f->option_current->description = g_list_append(NULL, desc);
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

		if (initial) {
			printf("adding options\n");
			gtk_signal_handler_block_by_data((GtkObject *)p->list0, f);
			gtk_list_remove_items((GtkList *)p->list0, p->items0);
			p->items0 = fill_options(f->options);
			gtk_list_append_items((GtkList *)p->list0, p->items0);
			gtk_signal_handler_unblock_by_data((GtkObject *)p->list0, f);
			gtk_frame_set_label(p->listframe0, "Select rule type");
		}

		html_write_options((GtkHTML *)p->html0, f->option_current, p->default_html);
		break;
	case 1:
	case 2:
	case 3:
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

		html_write_options((GtkHTML *)p->html0, f->option_current, p->default_html);
		break;
	case 4:
		if (initial) {
			char *text;
			text = filter_description_text(f->option_current->description, NULL);
			if (text == NULL) {
				/* maybe this could fudge something out of the first
				   bits of the rule */
				if (f->option_current->type == FILTER_XML_SEND) {
					text = "Filter messages sent";
				} else {
					text = "Filter messages received";
				}
				gtk_entry_set_text(GTK_ENTRY(p->name1), text);
			} else {
				gtk_entry_set_text(GTK_ENTRY(p->name1), text);
				g_free(text);
			}
			gtk_notebook_set_page(GTK_NOTEBOOK(p->notebook), 1);
		}

		html_write_options((GtkHTML *)p->html1, f->option_current, p->default_html);
		break;

	}
}

void
filter_druid_set_rules(FilterDruid *f, GList *options, GList *rules, struct filter_option *current)
{
	struct filter_option *new;
	GList *optionsl;

	f->options = options;
	f->rules = rules;
	f->user = NULL;

	if (current) {
		/* FIXME: free this list if it isn't empty ... */
		/* clone the 'current' option */
		new = g_malloc(sizeof(*new));
		new->type = current->type;
		new->description = current->description;
		new->options = NULL;
		optionsl = current->options;
		while (optionsl) {
			struct filter_optionrule *ornew,
				*or = optionsl->data;
			ornew = filter_clone_optionrule(or);
			new->options = g_list_append(new->options, ornew);
			optionsl = g_list_next(optionsl);
		}
		f->option_current = new;
	} else {
		f->option_current = NULL;
	}

	update_display(f, 1);
}

static void
build_druid(FilterDruid *d)
{
	GtkWidget *vbox, *frame, *scrolled_window, *list, *html, *hbox, *label, *vbox1;
	struct _FilterDruidPrivate *p = _PRIVATE(d);

#if 0
	gnome_dialog_append_buttons((GnomeDialog *)d, "Prev", "Next", "Finish", "Cancel", 0);
	gnome_dialog_set_close((GnomeDialog *)d, FALSE);
	gnome_dialog_set_sensitive((GnomeDialog *)d, 0, FALSE);
	gnome_dialog_set_sensitive((GnomeDialog *)d, 1, FALSE);
	gnome_dialog_set_sensitive((GnomeDialog *)d, 2, FALSE);
	gnome_dialog_set_default((GnomeDialog *)d, 1);
#endif

	p->notebook = d;
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
/*	gtk_signal_connect(GTK_OBJECT(d), "clicked", dialogue_clicked, d);*/

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

	gtk_signal_connect(GTK_OBJECT(p->name1), "changed", option_name_changed, d);

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
}

void
filter_druid_set_page(FilterDruid *f, enum FilterDruidPage page)
{
	struct _FilterDruidPrivate *p = _PRIVATE(f);
	int initial = p->page != page;

	p->page = page;
	update_display(f, initial);
}


void
filter_druid_set_default_html(FilterDruid *f, const char *html)
{
	struct _FilterDruidPrivate *p = _PRIVATE(f);

	g_free(p->default_html);
	p->default_html = g_strdup(html);
}

enum FilterDruidPage
filter_druid_get_page(FilterDruid *f)
{
	struct _FilterDruidPrivate *p = _PRIVATE(f);

	return p->page;
}

