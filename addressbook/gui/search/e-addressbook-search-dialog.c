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


static void eab_search_dialog_init             (EABSearchDialog          *widget);
static void eab_search_dialog_class_init       (EABSearchDialogClass     *klass);
static void eab_search_dialog_dispose          (GObject *object);

static GtkDialog *parent_class = NULL;

#define PARENT_TYPE GTK_TYPE_DIALOG

E_MAKE_TYPE (eab_search_dialog,
	     "EABSearchDialog",
	     EABSearchDialog,
	     eab_search_dialog_class_init,
	     eab_search_dialog_init,
	     PARENT_TYPE)

enum
{
	PROP_VIEW = 1
};

static GtkWidget *
get_widget (EABSearchDialog *view)
{
	RuleContext *context;
	FilterRule  *rule;

	context = eab_view_peek_search_context (view->view);
	rule    = eab_view_peek_search_rule    (view->view);

	if (!context || !rule) {
		g_warning ("Could not get search context.");
		return gtk_entry_new ();
	}

	return filter_rule_get_widget (rule, context);
}

static void
eab_search_dialog_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	EABSearchDialog *search_dialog;

	search_dialog = EAB_SEARCH_DIALOG (object);
	
	switch (property_id) {
	case PROP_VIEW:
		search_dialog->view = g_value_get_object (value);
		search_dialog->search = get_widget (search_dialog);
		gtk_container_set_border_width (GTK_CONTAINER (search_dialog->search), 12);
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (search_dialog)->vbox),
				    search_dialog->search, TRUE, TRUE, 0);
		gtk_widget_show (search_dialog->search);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
eab_search_dialog_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	EABSearchDialog *search_dialog;

	search_dialog = EAB_SEARCH_DIALOG (object);

	switch (property_id) {
	case PROP_VIEW:
		g_value_set_object (value, search_dialog->view);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
eab_search_dialog_class_init (EABSearchDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->set_property = eab_search_dialog_set_property;
	object_class->get_property = eab_search_dialog_get_property;
	object_class->dispose = eab_search_dialog_dispose;

	g_object_class_install_property (object_class, PROP_VIEW,
					 g_param_spec_object ("view", NULL, NULL, E_TYPE_AB_VIEW,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static char *
get_query (EABSearchDialog *view)
{
	FilterRule *rule;
	GString *out = g_string_new("");
	char *ret;

	rule = eab_view_peek_search_rule (view->view);

	filter_rule_build_code(rule, out);
	ret = out->str;
	printf("Searching using %s\n", ret);
	g_string_free(out, FALSE);
	return ret;
}

static void
dialog_response (GtkWidget *widget, int response_id, EABSearchDialog *dialog)
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
eab_search_dialog_init (EABSearchDialog *view)
{
	GtkDialog *dialog = GTK_DIALOG (view);

	gtk_widget_realize (GTK_WIDGET (dialog));
	gtk_dialog_set_has_separator (dialog, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 12);

	gtk_window_set_default_size (GTK_WINDOW (view), 550, 400);
	gtk_window_set_title(GTK_WINDOW(view), _("Advanced Search"));

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
eab_search_dialog_new (EABView *addr_view)
{
	EABSearchDialog *view = g_object_new (EAB_SEARCH_DIALOG_TYPE, "view", addr_view, NULL);

	return GTK_WIDGET(view);
}

static void
eab_search_dialog_dispose (GObject *object)
{
	EABSearchDialog *view;

	view = EAB_SEARCH_DIALOG (object);

	G_OBJECT_CLASS(parent_class)->dispose (object);
}
