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
#include <libgnome/gnome-i18n.h>

#include "em-folder-properties.h"
#include "em-config.h"

#include "mail-ops.h"
#include "mail-mt.h"
#include "mail-vfolder.h"

struct _prop_data {
	void *object;
	CamelArgV *argv;
	GtkWidget **widgets;

	GSList *properties;
	char *name;
	int total;
	int unread;
	EMConfig *config;
};

static void
emfp_dialog_response (GtkWidget *dialog, int response, struct _prop_data *prop_data)
{
	if (response == GTK_RESPONSE_OK)
		e_config_commit((EConfig *)prop_data->config);
	else
		e_config_abort((EConfig *)prop_data->config);

	gtk_widget_destroy (dialog);
}

static void
emfp_commit(EConfig *ec, GSList *items, void *data)
{
	struct _prop_data *prop_data = data;
	CamelArgV *argv = prop_data->argv;
	int i;
	
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
}

static void
emfp_free(EConfig *ec, GSList *items, void *data)
{
	struct _prop_data *prop_data = data;
	int i;
	
	g_slist_free(items);

	for (i = 0; i < prop_data->argv->argc; i++) {
		if ((prop_data->argv->argv[i].tag & CAMEL_ARG_TYPE) == CAMEL_ARG_STR)
			g_free (prop_data->argv->argv[i].ca_str);
	}

	camel_object_free (prop_data->object, CAMEL_FOLDER_PROPERTIES, prop_data->properties);
	camel_object_free (prop_data->object, CAMEL_FOLDER_NAME, prop_data->name);
	
	camel_object_unref (prop_data->object);
	g_free (prop_data->argv);

	g_free (prop_data);
}

static GtkWidget *
emfp_get_folder_item(EConfig *ec, EConfigItem *item, struct _GtkWidget *parent, struct _GtkWidget *old, void *data)
{
	char countstr[16];
	GtkWidget *w, *table, *label;
	struct _prop_data *prop_data = data;
	int row = 0, i;
	GSList *l;

	if (old)
		return old;

	table = gtk_table_new (g_slist_length (prop_data->properties) + 2, 2, FALSE);
	gtk_table_set_row_spacings ((GtkTable *) table, 6);
	gtk_table_set_col_spacings ((GtkTable *) table, 12);
	gtk_widget_show (table);
	gtk_box_pack_start ((GtkBox *) parent, table, TRUE, TRUE, 0);

	/* TODO: can this be done in a loop? */
	label = gtk_label_new (ngettext ("Total message:", "Total messages:", prop_data->total));
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
	
	sprintf(countstr, "%d", prop_data->total);
	label = gtk_label_new (countstr);
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 1, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	row++;

	label = gtk_label_new (ngettext ("Unread message:", "Unread messages:", prop_data->unread));
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
	
	sprintf(countstr, "%d", prop_data->unread);
	label = gtk_label_new (countstr);
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 1, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	row++;

	/* setup the ui with the values retrieved */
	l = prop_data->properties;
	i = 0;
	while (l) {
		CamelProperty *prop = l->data;
		
		switch (prop->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_BOO:
			w = gtk_check_button_new_with_label (prop->description);
			gtk_toggle_button_set_active ((GtkToggleButton *) w, prop_data->argv->argv[i].ca_int != 0);
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
			if (prop_data->argv->argv[i].ca_str) {
				gtk_entry_set_text ((GtkEntry *) w, prop_data->argv->argv[i].ca_str);
				camel_object_free (prop_data->object, prop_data->argv->argv[i].tag, prop_data->argv->argv[i].ca_str);
				prop_data->argv->argv[i].ca_str = NULL;
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

	return table;
}

#define EMFP_FOLDER_SECTION (2)

static EMConfigItem emfp_items[] = {
	{ E_CONFIG_BOOK,  "", NULL },
	{ E_CONFIG_PAGE, "00.general", N_("General") },
	{ E_CONFIG_SECTION, "00.general/00.folder", NULL /* set by code */ },
	{ E_CONFIG_ITEM, "00.general/00.folder/00.info", NULL, emfp_get_folder_item },
};

static void
emfp_dialog_got_folder (char *uri, CamelFolder *folder, void *data)
{
	GtkWidget *dialog, *w;
	struct _prop_data *prop_data;
	GSList *l;
	gint32 count, i;
	EMConfig *ec;
	EMConfigTargetFolder *target;
	CamelArgGetV *arggetv;
	CamelArgV *argv;

	if (folder == NULL)
		return;

	prop_data = g_malloc0 (sizeof (*prop_data));
	prop_data->object = folder;
	camel_object_ref (folder);

	camel_object_get (folder, NULL, CAMEL_FOLDER_PROPERTIES, &prop_data->properties, CAMEL_FOLDER_NAME, &prop_data->name,
			  CAMEL_FOLDER_TOTAL, &prop_data->total, CAMEL_FOLDER_UNREAD, &prop_data->unread, NULL);

	emfp_items[EMFP_FOLDER_SECTION].label = prop_data->name;

	count = g_slist_length (prop_data->properties);

	prop_data->widgets = g_malloc0 (sizeof (prop_data->widgets[0]) * count);

	/* build an arggetv/argv to retrieve/store the results */
	argv = g_malloc0 (sizeof (*argv) + (count - CAMEL_ARGV_MAX) * sizeof (argv->argv[0]));
	argv->argc = count;
	arggetv = g_malloc0 (sizeof (*arggetv) + (count - CAMEL_ARGV_MAX) * sizeof (arggetv->argv[0]));
	arggetv->argc = count;
	
	i = 0;
	l = prop_data->properties;
	while (l) {
		CamelProperty *prop = l->data;
		
		argv->argv[i].tag = prop->tag;
		arggetv->argv[i].tag = prop->tag;
		arggetv->argv[i].ca_ptr = &argv->argv[i].ca_ptr;
		
		l = l->next;
		i++;
	}
	
	camel_object_getv (prop_data->object, NULL, arggetv);
	g_free (arggetv);
	prop_data->argv = argv;

	dialog = gtk_dialog_new_with_buttons (_("Folder Properties"), NULL,
					      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);
	gtk_window_set_default_size ((GtkWindow *) dialog, 192, 160);
	gtk_widget_ensure_style (dialog);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *) dialog)->vbox, 12);

	ec = em_config_new(E_CONFIG_BOOK, "org.gnome.evolution.mail.folderConfig");
	prop_data->config = ec;
	l = NULL;
	for (i=0;i<sizeof(emfp_items)/sizeof(emfp_items[0]);i++)
		l = g_slist_prepend(l, &emfp_items[i]);
	e_config_add_items((EConfig *)ec, l, emfp_commit, NULL, emfp_free, prop_data);

	target = em_config_target_new_folder(ec, folder, uri);
	e_config_set_target((EConfig *)ec, (EConfigTarget *)target);
	w = e_config_create_widget((EConfig *)ec);

	gtk_box_pack_start ((GtkBox *) ((GtkDialog *) dialog)->vbox, w, TRUE, TRUE, 0);

	/* we do 'apply on ok' ... since instant apply may apply some very long running tasks */
	
	g_signal_connect (dialog, "response", G_CALLBACK (emfp_dialog_response), prop_data);
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
	if (!strncmp(uri, "vfolder:", 8))
		vfolder_edit_rule(uri);
	else if (folder == NULL)
		mail_get_folder(uri, 0, emfp_dialog_got_folder, NULL, mail_thread_new);
	else
		emfp_dialog_got_folder((char *)uri, folder, NULL);
}
