/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  Abstract filter argument class.
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

#include "filter-arg.h"


static void filter_arg_class_init (FilterArgClass *class);
static void filter_arg_init       (FilterArg      *gspaper);

#define _PRIVATE(x) (((FilterArg *)(x))->priv)

struct _FilterArgPrivate {
	GtkWidget *dialogue;	/* editor widget */
	xmlNodePtr oldargs;
};

static GtkObjectClass *parent_class;

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
filter_arg_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"FilterArg",
			sizeof (FilterArg),
			sizeof (FilterArgClass),
			(GtkClassInitFunc) filter_arg_class_init,
			(GtkObjectInitFunc) filter_arg_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_object_get_type (), &type_info);
	}
	
	return type;
}

static FilterArg *
clone_default(FilterArg *a)
{
	xmlNodePtr values;
	FilterArg *new = FILTER_ARG ( gtk_type_new (((GtkObject *)a)->klass->type) );

	/* clone values */
	new->name = g_strdup(a->name);
	values = filter_arg_values_get_xml(a);
	filter_arg_values_add_xml(new, values);
	xmlFreeNodeList(values);

	return new;
}

static void
write_html_nothing(FilterArg *arg, GtkHTML *html, GtkHTMLStream *stream)
{
	/* empty */
}

static void
write_text_nothing(FilterArg *arg, GString *string)
{
	/* empty */
}

static void
edit_values_nothing(FilterArg *arg)
{
	/* empty */
}

static int
edit_value_nothing(FilterArg *arg, int index)
{
	return index;
}

static void
free_value_nothing(FilterArg *arg, void *v)
{
	/* empty */
}

static void
filter_arg_class_init (FilterArgClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class (gtk_object_get_type ());

	class->write_html = write_html_nothing;
	class->write_text = write_text_nothing;
	class->edit_values = edit_values_nothing;
	class->edit_value = edit_value_nothing;
	class->free_value = free_value_nothing;
	class->clone = clone_default;
	
	signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (FilterArgClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
filter_arg_init (FilterArg *arg)
{
	arg->values = NULL;
	arg->priv = g_malloc0(sizeof(*arg->priv));
}

/**
 * filter_arg_new:
 *
 * Create a new FilterArg widget.
 * 
 * Return value: A new FilterArg widget.
 **/
FilterArg *
filter_arg_new (char *name)
{
	FilterArg *a = FILTER_ARG ( gtk_type_new (filter_arg_get_type ()));
	if (name)
		a->name = g_strdup(name);
	return a;
}

FilterArg *
filter_arg_clone (FilterArg *arg)
{
	return ((FilterArgClass *)(arg->object.klass))->clone(arg);
}

void
filter_arg_copy(FilterArg *dst, FilterArg *src)
{
	xmlNodePtr values;

	g_return_if_fail( ((GtkObject *)src)->klass->type == ((GtkObject *)dst)->klass->type );

	/* remove old values */
	while (dst->values) {
		filter_arg_remove(dst, dst->values->data);
	}

	/* clone values */
	values = filter_arg_values_get_xml(src);
	filter_arg_values_add_xml(dst, values);
	xmlFreeNodeList(values);
}

void
filter_arg_add(FilterArg *arg, void *v)
{
	g_return_if_fail(v != NULL);

	arg->values = g_list_append(arg->values, v);
	gtk_signal_emit(GTK_OBJECT(arg), signals[CHANGED]);
}

void
filter_arg_remove(FilterArg *arg, void *v)
{
	arg->values = g_list_remove(arg->values, v);
	((FilterArgClass *)(arg->object.klass))->free_value(arg, v);
	gtk_signal_emit(GTK_OBJECT(arg), signals[CHANGED]);
}


void
filter_arg_write_html(FilterArg *arg, GtkHTML *html, GtkHTMLStream *stream)
{
	((FilterArgClass *)(arg->object.klass))->write_html(arg, html, stream);
}

void
filter_arg_write_text(FilterArg *arg, GString *string)
{
	int count, i;

	count = filter_arg_get_count(arg);
	for (i=0;i<count;i++) {
		g_string_append(string, filter_arg_get_value_as_string(arg, i));
		if (i<count-1) {
			g_string_append(string, ", ");
		}
		if (i==count-2 && count>1) {
			g_string_append(string, "or ");
		}
	}

#if 0
	((FilterArgClass *)(arg->object.klass))->write_text(arg, string);
#endif
}
void
filter_arg_edit_values(FilterArg *arg)
{
	void filter_arg_edit_values_1(FilterArg *arg);

	g_return_if_fail(arg != NULL);


#if 1
	filter_arg_edit_values_1(arg);
#else	

	if (((FilterArgClass *)(arg->object.klass))->edit_values)
		((FilterArgClass *)(arg->object.klass))->edit_values(arg);
	else
		g_warning("No implementation of virtual method edit_values");
#endif
}

int
filter_arg_edit_value(FilterArg *arg, int index)
{
	return ((FilterArgClass *)(arg->object.klass))->edit_value(arg, index);
}

xmlNodePtr
filter_arg_values_get_xml(FilterArg *arg)
{
	return ((FilterArgClass *)(arg->object.klass))->values_get_xml(arg);
}
void
filter_arg_values_add_xml(FilterArg *arg, xmlNodePtr node)
{
	((FilterArgClass *)(arg->object.klass))->values_add_xml(arg, node);
}

/* returns the number of args in the arg list */
int
filter_arg_get_count(FilterArg *arg)
{
	int count=0;
	GList *l;

	for (l = arg->values;l;l=g_list_next(l))
		count++;
	return count;
}

void *
filter_arg_get_value(FilterArg *arg, int index)
{
	int count = 0;
	GList *l;

	for (l = arg->values; l && count < index; l = g_list_next(l))
		count++;
	if (l)
		return l->data;
	return NULL;
}

char *
filter_arg_get_value_as_string(FilterArg *arg, int index)
{
	void *data;

	data = filter_arg_get_value(arg, index);
	if (data) {
		return ((FilterArgClass *)(arg->object.klass))->get_value_as_string(arg, data);
	} else {
		return "";
	}
}


struct filter_arg_edit {
	FilterArg *arg;
	GtkList *list;
	GList *items;
	GnomeDialog *dialogue;
	GtkWidget *add, *remove, *edit;
	GtkWidget *item_current;
};

static void
filter_arg_edit_add(GtkWidget *w, struct filter_arg_edit *edata)
{
	GtkWidget *listitem;
	int i;

	i = filter_arg_edit_value(edata->arg, -1);
	if (i>=0) {
		gtk_list_remove_items_no_unref(edata->list, edata->items);
		listitem = gtk_list_item_new_with_label(filter_arg_get_value_as_string(edata->arg, i));
		gtk_object_set_data(GTK_OBJECT (listitem), "arg_i", filter_arg_get_value(edata->arg, i));
		edata->items = g_list_append(edata->items, listitem);
		gtk_widget_show(listitem);
		
		/* this	api is nonsense */
		gtk_list_append_items(edata->list, g_list_copy(edata->items));
	}
}

void dump_list(GList *list)
{
	printf("dumping list:\n");
	for (;list;list = g_list_next(list)) {
		printf(" %p %p\n", list, list->data);
	}
}

static void
fill_list(struct filter_arg_edit *edata)
{
	GList *items = NULL;
	int i, count;
	GtkListItem *listitem;

	gtk_list_remove_items(edata->list, edata->items);
	g_list_free(edata->items);

	count = filter_arg_get_count(edata->arg);
	for (i=0;i<count;i++) {
		char *labeltext;
		labeltext = filter_arg_get_value_as_string(edata->arg, i);
		listitem = (GtkListItem *)gtk_list_item_new_with_label(labeltext);
		gtk_object_set_data((GtkObject *)listitem, "arg_i", filter_arg_get_value(edata->arg, i));
		items = g_list_append(items, listitem);
		gtk_widget_show(GTK_WIDGET(listitem));
	}
	
	edata->item_current = NULL;
	edata->items = items;
	
	gtk_list_append_items(edata->list, g_list_copy(edata->items));
}

static void
filter_arg_edit_edit(GtkWidget *w, struct filter_arg_edit *edata)
{
	char *name;
	int i;

	/* yurck */
	if (edata->item_current
	    && (name = gtk_object_get_data(GTK_OBJECT (edata->item_current), "arg_i"))
	    && (i = g_list_index(edata->arg->values, name)) >= 0
	    && (i = filter_arg_edit_value(edata->arg, i)) >= 0) {

		fill_list(edata);
	}
}

static void
filter_arg_edit_delete(GtkWidget *w, struct filter_arg_edit *edata)
{
	char *name;

	/* yurck */
	name = gtk_object_get_data(GTK_OBJECT (edata->item_current), "arg_i");
	if (edata->item_current && name) {
		filter_arg_remove(edata->arg, name);
		fill_list(edata);
	}
}

static void
edit_sensitise(struct filter_arg_edit *edata)
{
	int state = edata->item_current != NULL;
	gtk_widget_set_sensitive(edata->remove, state);
	gtk_widget_set_sensitive(edata->edit, state);
}

static void
filter_arg_edit_select(GtkWidget *w, GtkListItem *list, struct filter_arg_edit *edata)
{
	edata->item_current = GTK_WIDGET (list);
	edit_sensitise(edata);
}

static void
filter_arg_edit_unselect(GtkWidget *w, GtkListItem *list, struct filter_arg_edit *edata)
{
	edata->item_current = NULL;
	edit_sensitise(edata);
}

static void
filter_arg_edit_clicked(GnomeDialog *d, int button, struct filter_arg_edit *edata)
{
	struct _FilterArgPrivate *p = _PRIVATE(edata->arg);

	if (button == 0) {
		gtk_signal_emit(GTK_OBJECT(edata->arg), signals[CHANGED]);
	} else {
		/* cancel button, restore old values ... */
		while (edata->arg->values) {
			filter_arg_remove(edata->arg, edata->arg->values->data);
		}
		filter_arg_values_add_xml(edata->arg, p->oldargs);
	}
	xmlFreeNodeList(p->oldargs);
	p->oldargs = NULL;
	p->dialogue = NULL;
	gnome_dialog_close(d);
}

static void
filter_arg_edit_destroy(GnomeDialog *d, struct filter_arg_edit *edata)
{
	struct _FilterArgPrivate *p = _PRIVATE(edata->arg);

	if (p->oldargs) {
		while (edata->arg->values) {
			filter_arg_remove(edata->arg, edata->arg->values->data);
		}
		filter_arg_values_add_xml(edata->arg, p->oldargs);
		xmlFreeNodeList(p->oldargs);
		p->oldargs = NULL;
	}

	if (p->dialogue) {
		p->dialogue = NULL;
		gnome_dialog_close(d);
	}
	g_free(edata);
}

void
filter_arg_edit_values_1(FilterArg *arg)
{
	GtkWidget *list;
	GnomeDialog *dialogue;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *button;
	GtkWidget *scrolled_window, *frame;
	struct filter_arg_edit * edata;
	struct _FilterArgPrivate *p = _PRIVATE(arg);

	/* dont show more than 1 editor for each type */
	if (p->dialogue) {
		gdk_window_raise(GTK_WIDGET(p->dialogue)->window);
		return;
	}

	/* copy the current state */
	p->oldargs = filter_arg_values_get_xml(arg);

	edata = g_malloc0(sizeof(*edata));
	edata->item_current = NULL;
	edata->arg = arg;

	dialogue = (GnomeDialog *)gnome_dialog_new("Edit values", "Ok", "Cancel", 0);
	edata->dialogue = dialogue;

	p->dialogue = GTK_WIDGET (dialogue);

	hbox = gtk_hbox_new(FALSE, 0);

	list = gtk_list_new();
	edata->list = GTK_LIST (list);
	edata->items = NULL;
	fill_list(edata);

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	frame = gtk_frame_new("Option values");

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), list);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_set_focus_vadjustment(GTK_CONTAINER (list),
					    gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window)));
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
	gtk_widget_set_usize(frame, 200, 300);
	gtk_box_pack_start(GTK_BOX (hbox), frame, TRUE, TRUE, 0);

	/* buttons */
	vbox = gtk_vbox_new(FALSE, 0);
	
	button = gtk_button_new_with_label ("Add");
	gtk_box_pack_start(GTK_BOX (vbox), button, FALSE, TRUE, 0);
	edata->add = button;
	button = gtk_button_new_with_label ("Remove");
	gtk_box_pack_start(GTK_BOX (vbox), button, FALSE, TRUE, 0);
	edata->remove = button;
	button = gtk_button_new_with_label ("Edit");
	gtk_box_pack_start(GTK_BOX (vbox), button, FALSE, TRUE, 0);
	edata->edit = button;

	gtk_box_pack_start(GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	gtk_signal_connect(GTK_OBJECT (edata->add), "clicked", filter_arg_edit_add, edata);
	gtk_signal_connect(GTK_OBJECT (edata->edit), "clicked", filter_arg_edit_edit, edata);
	gtk_signal_connect(GTK_OBJECT (edata->remove), "clicked", filter_arg_edit_delete, edata);
	gtk_signal_connect(GTK_OBJECT (edata->list), "select_child", filter_arg_edit_select, edata);
	gtk_signal_connect(GTK_OBJECT (edata->list), "unselect_child", filter_arg_edit_unselect, edata);

	gtk_widget_show(list);
	gtk_widget_show_all(hbox);
	gtk_box_pack_start(GTK_BOX (dialogue->vbox), hbox, TRUE, TRUE, 0);

	gtk_signal_connect(GTK_OBJECT (dialogue), "clicked", filter_arg_edit_clicked, edata);
	gtk_signal_connect(GTK_OBJECT (dialogue), "destroy", filter_arg_edit_destroy, edata);

	edit_sensitise(edata);

	gtk_widget_show(GTK_WIDGET (dialogue));
}


