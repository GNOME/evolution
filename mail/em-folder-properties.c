/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtkbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>

#include <camel/camel-folder.h>
#include <camel/camel-vee-folder.h>

#include "em-folder-properties.h"

#include "mail-component.h"
#include "mail-ops.h"
#include "mail-mt.h"
#include "mail-vfolder.h"

struct _prop_data {
	void *object;
	CamelArgV *argv;
	GtkWidget **widgets;
};

static void
emfp_dialog_response (GtkWidget *dialog, int response, struct _prop_data *prop_data)
{
	CamelArgV *argv = prop_data->argv;
	int i;
	
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}
	
	for (i = 0; i < argv->argc; i++) {
		CamelArg *arg = &argv->argv[i];
		
		switch (arg->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_BOO:
			arg->ca_int = gtk_toggle_button_get_active ((GtkToggleButton *) prop_data->widgets[i]);
			break;
		case CAMEL_ARG_STR:
			g_free (arg->ca_str);
			arg->ca_str = (char *) gtk_entry_get_text ((GtkEntry *) prop_data->widgets[i]);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
	
	camel_object_setv (prop_data->object, NULL, argv);
	gtk_widget_destroy (dialog);
}

static void
emfp_dialog_free (void *data)
{
	struct _prop_data *prop_data = data;
	int i;
	
	for (i = 0; i < prop_data->argv->argc; i++) {
		if ((prop_data->argv->argv[i].tag & CAMEL_ARG_TYPE) == CAMEL_ARG_STR)
			g_free (prop_data->argv->argv[i].ca_str);
	}
	
	camel_object_unref (prop_data->object);
	g_free (prop_data->argv);
	g_free (prop_data);
}

static void
emfp_dialog_got_folder (char *uri, CamelFolder *folder, void *data)
{
	GtkWidget *dialog, *w, *table, *label, *vbox, *hbox;
	struct _prop_data *prop_data;
	CamelArgGetV *arggetv;
	CamelArgV *argv;
	GSList *list, *l;
	gint32 count, i;
	char *name, *title;
	char countstr[16];
	int row = 0, total=0, unread=0;
	
	if (folder == NULL)
		return;
	
	camel_object_get (folder, NULL, CAMEL_FOLDER_PROPERTIES, &list, CAMEL_FOLDER_NAME, &name,
			  CAMEL_FOLDER_TOTAL, &total, CAMEL_FOLDER_UNREAD, &unread, NULL);
	
	dialog = gtk_dialog_new_with_buttons (_("Folder Properties"), NULL,
					      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);
	gtk_window_set_default_size ((GtkWindow *) dialog, 192, 160);
	gtk_widget_ensure_style (dialog);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *) dialog)->vbox, 0);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *) dialog)->vbox, 12);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width ((GtkContainer *) vbox, 12);
	gtk_box_pack_start ((GtkBox *) ((GtkDialog *) dialog)->vbox, vbox, TRUE, TRUE, 0);
	gtk_widget_show (vbox);

	if (folder->parent_store == mail_component_peek_local_store(NULL)
	    && (!strcmp(name, "Drafts")
		|| !strcmp(name, "Inbox")
		|| !strcmp(name, "Outbox")
		|| !strcmp(name, "Sent")))
		title = g_strdup_printf("<b>%s</b>", _(name));
	else
		title = g_strdup_printf ("<b>%s</b>", name);

	label = gtk_label_new (title);
	gtk_label_set_use_markup ((GtkLabel *) label, TRUE);
	gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
	gtk_box_pack_start ((GtkBox *) vbox, label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	g_free (title);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start ((GtkBox *) vbox, hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	label = gtk_label_new ("");
	gtk_box_pack_start ((GtkBox *) hbox, label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	/* TODO: maybe we want some basic properties here, like message counts/approximate size/etc */
	table = gtk_table_new (g_slist_length (list) + 2, 2, FALSE);
	gtk_table_set_row_spacings ((GtkTable *) table, 6);
	gtk_table_set_col_spacings ((GtkTable *) table, 12);
	gtk_widget_show (table);
	gtk_box_pack_start ((GtkBox *) hbox, table, TRUE, TRUE, 0);

	/* TODO: can this be done in a loop? */
	label = gtk_label_new (ngettext ("Total message:", "Total messages:", total));
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
	
	sprintf(countstr, "%d", total);
	label = gtk_label_new (countstr);
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 1, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	row++;

	label = gtk_label_new (ngettext ("Unread message:", "Unread messages:", unread));
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
	
	sprintf(countstr, "%d", unread);
	label = gtk_label_new (countstr);
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 1, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	row++;

	/* build an arggetv/argv to retrieve/store the results */
	count = g_slist_length (list);
	arggetv = g_malloc0 (sizeof (*arggetv) + (count - CAMEL_ARGV_MAX) * sizeof (arggetv->argv[0]));
	arggetv->argc = count;
	argv = g_malloc0 (sizeof (*argv) + (count - CAMEL_ARGV_MAX) * sizeof (argv->argv[0]));
	argv->argc = count;
	
	i = 0;
	l = list;
	while (l) {
		CamelProperty *prop = l->data;
		
		argv->argv[i].tag = prop->tag;
		arggetv->argv[i].tag = prop->tag;
		arggetv->argv[i].ca_ptr = &argv->argv[i].ca_ptr;
		
		l = l->next;
		i++;
	}
	
	camel_object_getv (folder, NULL, arggetv);
	g_free (arggetv);
	
	prop_data = g_malloc0 (sizeof (*prop_data));
	prop_data->widgets = g_malloc0 (sizeof (prop_data->widgets[0]) * count);
	prop_data->argv = argv;
	
	/* setup the ui with the values retrieved */
	l = list;
	i = 0;
	while (l) {
		CamelProperty *prop = l->data;
		
		switch (prop->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_BOO:
			w = gtk_check_button_new_with_label (prop->description);
			gtk_toggle_button_set_active ((GtkToggleButton *) w, argv->argv[i].ca_int != 0);
			gtk_widget_show (w);
			gtk_table_attach ((GtkTable *) table, w, 0, 2, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
			prop_data->widgets[i] = w;
			break;
		case CAMEL_ARG_STR:
			label = gtk_label_new (prop->description);
			gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
			gtk_widget_show (label);
			gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
			
			w = gtk_entry_new ();
			gtk_widget_show (w);
			if (argv->argv[i].ca_str) {
				gtk_entry_set_text ((GtkEntry *) w, argv->argv[i].ca_str);
				camel_object_free (folder, argv->argv[i].tag, argv->argv[i].ca_str);
				argv->argv[i].ca_str = NULL;
			}
			gtk_table_attach ((GtkTable *) table, w, 1, 2, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
			prop_data->widgets[i] = w;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		
		row++;
		l = l->next;
	}
	
	prop_data->object = folder;
	camel_object_ref (folder);
	
	camel_object_free (folder, CAMEL_FOLDER_PROPERTIES, list);
	camel_object_free (folder, CAMEL_FOLDER_NAME, name);
	
	/* we do 'apply on ok' ... since instant apply may apply some very long running tasks */
	
	g_signal_connect (dialog, "response", G_CALLBACK (emfp_dialog_response), prop_data);
	g_object_set_data_full ((GObject *) dialog, "e-prop-data", prop_data, emfp_dialog_free);
	gtk_widget_show (dialog);
}

/**
 * em_folder_properties_show:
 * @parent: parent window for dialogue (currently unused)
 * @folder: 
 * @uri: 
 * 
 * Show folder properties for @folder and @uri.  If @folder is passed
 * as NULL, then the folder @uri will be loaded first.
 **/
void
em_folder_properties_show(GtkWindow *parent, CamelFolder *folder, const char *uri)
{
	/* HACK: its the old behaviour, not very 'neat' but it works */
	if (!strncmp(uri, "vfolder:", 8)) {
		CamelURL *url = camel_url_new(uri, NULL);

		/* MORE HACK: UNMATCHED is a special folder which you can't modify, so check for it here */
		if (url == NULL
		    || url->fragment == NULL
		    || strcmp(url->fragment, CAMEL_UNMATCHED_NAME) != 0) {
			if (url)
				camel_url_free(url);
			vfolder_edit_rule(uri);
			return;
		}
		if (url)
			camel_url_free(url);
	}

	if (folder == NULL)
		mail_get_folder(uri, 0, emfp_dialog_got_folder, NULL, mail_thread_new);
	else
		emfp_dialog_got_folder((char *)uri, folder, NULL);
}
