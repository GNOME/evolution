/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-addressbook-search-dialog.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtkbox.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkstock.h>
#include <libgnome/gnome-i18n.h>
#include "gal/util/e-util.h"

#include "e-addressbook-search-dialog.h"


static void e_addressbook_search_dialog_init		 (EAddressbookSearchDialog		 *widget);
static void e_addressbook_search_dialog_class_init	 (EAddressbookSearchDialogClass	 *klass);
static void e_addressbook_search_dialog_dispose          (GObject *object);

static GtkDialog *parent_class = NULL;

#define PARENT_TYPE GTK_TYPE_DIALOG

E_MAKE_TYPE (e_addressbook_search_dialog,
	     "EAddressbookSearchDialog",
	     EAddressbookSearchDialog,
	     e_addressbook_search_dialog_class_init,
	     e_addressbook_search_dialog_init,
	     PARENT_TYPE)

static void
e_addressbook_search_dialog_class_init (EAddressbookSearchDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = e_addressbook_search_dialog_dispose;
}

static GtkWidget *
get_widget (EAddressbookSearchDialog *view)
{
	FilterPart *part;

	view->context = rule_context_new();
	/* FIXME: hide this in a class */
	rule_context_add_part_set(view->context, "partset", filter_part_get_type(),
				  rule_context_add_part, rule_context_next_part);
	rule_context_load(view->context, SEARCH_RULE_DIR "/addresstypes.xml", "");
	view->rule = filter_rule_new();
	part = rule_context_next_part(view->context, NULL);
	if (part == NULL) {
		g_warning("Problem loading search for addressbook no parts to load");
		return gtk_entry_new();
	} else {
		filter_rule_add_part(view->rule, filter_part_clone(part));
		return filter_rule_get_widget(view->rule, view->context);
	}
}

static char *
get_query (EAddressbookSearchDialog *view)
{
	GString *out = g_string_new("");
	char *ret;

	filter_rule_build_code(view->rule, out);
	ret = out->str;
	printf("Searching using %s\n", ret);
	g_string_free(out, FALSE);
	return ret;
}

static void
dialog_response (GtkWidget *widget, int response_id, EAddressbookSearchDialog *dialog)
{
	char *query;

	if (response_id == GTK_RESPONSE_OK) {
		query = get_query(dialog);
		g_object_set(dialog->view,
			     "query", query,
			     NULL);
		g_free(query);
	}

	gtk_widget_destroy(GTK_WIDGET (dialog));
}

static void
e_addressbook_search_dialog_init (EAddressbookSearchDialog *view)
{
	GtkDialog *dialog = GTK_DIALOG (view);

	gtk_window_set_default_size (GTK_WINDOW (view), 550, 400);
	gtk_window_set_title(GTK_WINDOW(view), _("Advanced Search"));
	view->search = get_widget(view);
	gtk_box_pack_start(GTK_BOX(dialog->vbox), view->search, TRUE, TRUE, 0);
	gtk_widget_show(view->search);

	gtk_dialog_add_buttons (dialog,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				/*GTK_STOCK_SAVE, GTK_RESPONSE_APPLY,*/
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

	g_signal_connect(dialog, "response",
			 G_CALLBACK(dialog_response), view);
}

GtkWidget *
e_addressbook_search_dialog_new (EAddressbookView *addr_view)
{
	EAddressbookSearchDialog *view = g_object_new (E_ADDRESSBOOK_SEARCH_DIALOG_TYPE, NULL);
	view->view = addr_view;
	return GTK_WIDGET(view);
}

static void
e_addressbook_search_dialog_dispose (GObject *object)
{
	EAddressbookSearchDialog *view;

	view = E_ADDRESSBOOK_SEARCH_DIALOG (object);

	if (view->context) {
		g_object_unref(view->context);
		view->context = NULL;
	}
	if (view->rule) {
		g_object_unref(view->rule);
		view->rule = NULL;
	}

	G_OBJECT_CLASS(parent_class)->dispose (object);
}
