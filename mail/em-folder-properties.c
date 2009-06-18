/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-vee-folder.h>
#include <glib/gi18n.h>

#include "em-folder-properties.h"
#include "em-config.h"

#include "mail-component.h"
#include "mail-ops.h"
#include "mail-mt.h"
#include "mail-vfolder.h"
#include "mail-config.h"

struct _prop_data {
	gpointer object;
	CamelArgV *argv;
	GtkWidget **widgets;

	GSList *properties;
	gchar *name;
	gint total;
	gint unread;
	EMConfig *config;
	CamelFolderQuotaInfo *quota;
};

static void
emfp_dialog_response (GtkWidget *dialog, gint response, struct _prop_data *prop_data)
{
	if (response == GTK_RESPONSE_OK)
		e_config_commit((EConfig *)prop_data->config);
	else
		e_config_abort((EConfig *)prop_data->config);

	gtk_widget_destroy (dialog);
}

static void
emfp_commit(EConfig *ec, GSList *items, gpointer data)
{
	struct _prop_data *prop_data = data;
	CamelArgV *argv = prop_data->argv;
	gint i;

	for (i = 0; i < argv->argc; i++) {
		CamelArg *arg = &argv->argv[i];

		switch (arg->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_BOO:
			arg->ca_int = gtk_toggle_button_get_active ((GtkToggleButton *) prop_data->widgets[i]);
			break;
		case CAMEL_ARG_STR:
			g_free (arg->ca_str);
			arg->ca_str = (gchar *) gtk_entry_get_text ((GtkEntry *) prop_data->widgets[i]);
			break;
		case CAMEL_ARG_INT:
			arg->ca_int = gtk_spin_button_get_value_as_int ((GtkSpinButton *) prop_data->widgets[i]);
			break;
		case CAMEL_ARG_DBL:
			arg->ca_double = gtk_spin_button_get_value ((GtkSpinButton *) prop_data->widgets[i]);
			break;
		default:
			g_warning ("This shouldn't be reached\n");
			break;
		}
	}

	camel_object_setv (prop_data->object, NULL, argv);
}

static void
emfp_free(EConfig *ec, GSList *items, gpointer data)
{
	struct _prop_data *prop_data = data;
	gint i;

	g_slist_free(items);

	for (i = 0; i < prop_data->argv->argc; i++) {
		if ((prop_data->argv->argv[i].tag & CAMEL_ARG_TYPE) == CAMEL_ARG_STR)
			g_free (prop_data->argv->argv[i].ca_str);
	}

	camel_object_free (prop_data->object, CAMEL_FOLDER_PROPERTIES, prop_data->properties);
	camel_object_free (prop_data->object, CAMEL_FOLDER_NAME, prop_data->name);

	camel_object_unref (prop_data->object);
	g_free (prop_data->argv);

	camel_folder_quota_info_free (prop_data->quota);

	g_free (prop_data);
}

static gint
add_numbered_row (GtkTable *table, gint row, const gchar *description, const gchar *format, gint num)
{
	gchar *str;
	GtkWidget *label;

	g_return_val_if_fail (table != NULL, row);
	g_return_val_if_fail (description != NULL, row);
	g_return_val_if_fail (format != NULL, row);

	label = gtk_label_new (description);
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);

	str = g_strdup_printf (format, num);

	label = gtk_label_new (str);
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
	gtk_table_attach (table, label, 1, 2, row, row+1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	g_free (str);

	return row + 1;
}

static GtkWidget *
emfp_get_folder_item(EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, gpointer data)
{
	GtkWidget *w, *table, *label;
	struct _prop_data *prop_data = data;
	gint row = 0, i;
	GSList *l;

	if (old)
		return old;

	table = gtk_table_new (g_slist_length (prop_data->properties) + 2, 2, FALSE);
	gtk_table_set_row_spacings ((GtkTable *) table, 6);
	gtk_table_set_col_spacings ((GtkTable *) table, 12);
	gtk_widget_show (table);
	gtk_box_pack_start ((GtkBox *) parent, table, TRUE, TRUE, 0);

	/* to be on the safe side, ngettext is used here, see e.g. comment #3 at bug 272567 */
	row = add_numbered_row (GTK_TABLE (table), row, ngettext ("Unread messages:", "Unread messages:", prop_data->unread), "%d", prop_data->unread);

	/* TODO: can this be done in a loop? */
	/* to be on the safe side, ngettext is used here, see e.g. comment #3 at bug 272567 */
	row = add_numbered_row (GTK_TABLE (table), row, ngettext ("Total messages:", "Total messages:", prop_data->total), "%d", prop_data->total);

	if (prop_data->quota) {
		CamelFolderQuotaInfo *info;
		CamelFolderQuotaInfo *quota = prop_data->quota;

		for (info = quota; info; info = info->next) {
			gchar *descr;
			gint procs;

			/* should not happen, but anyway... */
			if (!info->total)
				continue;

			/* Show quota name only when available and we
			 * have more than one quota info. */
			if (info->name && quota->next)
				descr = g_strdup_printf (
					_("Quota usage (%s):"), _(info->name));
			else
				descr = g_strdup_printf (_("Quota usage"));

			procs = (gint) ((((double) info->used) / ((double) info->total)) * 100.0 + 0.5);

			row = add_numbered_row (GTK_TABLE (table), row, descr, "%d%%", procs);

			g_free (descr);
		}
	}

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
		case CAMEL_ARG_INT:
			label = gtk_label_new (prop->description);
			gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
			gtk_widget_show (label);
			gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row + 1, GTK_FILL, 0, 0, 0);

			w = gtk_spin_button_new_with_range (G_MININT, G_MAXINT, 1.0);
			gtk_spin_button_set_value ((GtkSpinButton *) w, (double) prop_data->argv->argv[i].ca_int);
			gtk_spin_button_set_numeric ((GtkSpinButton *) w, TRUE);
			gtk_spin_button_set_digits ((GtkSpinButton *) w, 0);
			gtk_widget_show (w);
			gtk_table_attach ((GtkTable *) table, w, 1, 2, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
			prop_data->widgets[i] = w;
			break;
		case CAMEL_ARG_DBL:
			label = gtk_label_new (prop->description);
			gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
			gtk_widget_show (label);
			gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row + 1, GTK_FILL, 0, 0, 0);

			w = gtk_spin_button_new_with_range (G_MININT, G_MAXINT, 1.0);
			gtk_spin_button_set_value ((GtkSpinButton *) w, prop_data->argv->argv[i].ca_double);
			gtk_spin_button_set_numeric ((GtkSpinButton *) w, TRUE);
			gtk_spin_button_set_digits ((GtkSpinButton *) w, 2);
			gtk_widget_show (w);
			gtk_table_attach ((GtkTable *) table, w, 1, 2, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
			prop_data->widgets[i] = w;
			break;
		default:
			g_warning ("This shouldn't be reached\n");
			break;
		}

		row++;
		l = l->next;
		i++;
	}

	return table;
}

#define EMFP_FOLDER_SECTION (2)

static EMConfigItem emfp_items[] = {
	{ E_CONFIG_BOOK, (gchar *) "", NULL },
	{ E_CONFIG_PAGE, (gchar *) "00.general", (gchar *) N_("General") },
	{ E_CONFIG_SECTION, (gchar *) "00.general/00.folder", NULL /* set by code */ },
	{ E_CONFIG_ITEM, (gchar *) "00.general/00.folder/00.info", NULL, emfp_get_folder_item },
};
static gboolean emfp_items_translated = FALSE;

static void
emfp_dialog_got_folder_quota (CamelFolder *folder, CamelFolderQuotaInfo *quota, gpointer data)
{
	GtkWidget *dialog, *w;
	struct _prop_data *prop_data;
	GSList *l;
	gint32 count, i,deleted;
	EMConfig *ec;
	EMConfigTargetFolder *target;
	CamelArgGetV *arggetv;
	CamelArgV *argv;
	gboolean hide_deleted;
	GConfClient *gconf;
	CamelStore *store;
	gchar *uri = (gchar *)data;

	if (folder == NULL) {
		g_free (uri);
		return;
	}

	store = folder->parent_store;

	prop_data = g_malloc0 (sizeof (*prop_data));
	prop_data->object = folder;
	camel_object_ref (folder);
	prop_data->quota = camel_folder_quota_info_clone (quota);

	/*
	  Get number of VISIBLE and DELETED messages, instead of TOTAL messages.  VISIBLE+DELETED
	   gives the correct count that matches the label below the Send & Receive button
	*/
	camel_object_get (folder, NULL, CAMEL_FOLDER_PROPERTIES, &prop_data->properties, CAMEL_FOLDER_NAME, &prop_data->name,
			  CAMEL_FOLDER_VISIBLE, &prop_data->total, CAMEL_FOLDER_UNREAD, &prop_data->unread, CAMEL_FOLDER_DELETED, &deleted, NULL);

	gconf = mail_config_get_gconf_client ();
	hide_deleted = !gconf_client_get_bool(gconf, "/apps/evolution/mail/display/show_deleted", NULL);

	/*
	   Do the calculation only for those accounts that support VTRASHes
	 */
	if (store->flags & CAMEL_STORE_VTRASH) {
		if (CAMEL_IS_VTRASH_FOLDER(folder))
			prop_data->total += deleted;
		else if (!hide_deleted && deleted > 0)
			prop_data->total += deleted;
	}

	/*
	 * If the ffolder is junk folder, get total number of mails.
	 */
	if (store->flags & CAMEL_STORE_VJUNK) {
		camel_object_get (folder, NULL, CAMEL_FOLDER_TOTAL, &prop_data->total, NULL);
	}

	if (store == mail_component_peek_local_store(NULL)
	    && (!strcmp(prop_data->name, "Drafts")
		|| !strcmp(prop_data->name, "Templates")
		|| !strcmp(prop_data->name, "Inbox")
		|| !strcmp(prop_data->name, "Outbox")
		|| !strcmp(prop_data->name, "Sent"))) {
		emfp_items[EMFP_FOLDER_SECTION].label = _(prop_data->name);
		if (!emfp_items_translated) {
			for (i=0;i<sizeof(emfp_items)/sizeof(emfp_items[0]);i++) {
				if (emfp_items[i].label)
					emfp_items[i].label = _(emfp_items[i].label);
			}
			emfp_items_translated = TRUE;
		}
	} else if (!strcmp(prop_data->name, "INBOX"))
		emfp_items[EMFP_FOLDER_SECTION].label = _("Inbox");
	else
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

	/** @HookPoint-EMConfig: Folder Properties Window
	 * @Id: org.gnome.evolution.mail.folderConfig
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.mail.config:1.0
	 * @Target: EMConfigTargetFolder
	 *
	 * The folder properties window.
	 */
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

	g_free (uri);
}

static void
emfp_dialog_got_folder (gchar *uri, CamelFolder *folder, gpointer data)
{
	/* this should be called in a thread too */
	mail_get_folder_quota (folder, emfp_dialog_got_folder_quota, g_strdup (uri), mail_msg_unordered_push);
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
em_folder_properties_show(GtkWindow *parent, CamelFolder *folder, const gchar *uri)
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
		mail_get_folder(uri, 0, emfp_dialog_got_folder, NULL, mail_msg_unordered_push);
	else
		emfp_dialog_got_folder((gchar *)uri, folder, NULL);
}
