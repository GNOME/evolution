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
#include <glib/gi18n.h>

#include <gconf/gconf-client.h>

#include <e-util/e-binding.h>

#include "em-folder-properties.h"
#include "em-config.h"

#include "e-mail-local.h"
#include "mail-ops.h"
#include "mail-mt.h"
#include "mail-vfolder.h"
#include "mail-config.h"

struct _prop_data {
	gpointer object;
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
emfp_free(EConfig *ec, GSList *items, gpointer data)
{
	struct _prop_data *prop_data = data;

	g_slist_free(items);

	camel_object_state_write (prop_data->object);
	g_object_unref (prop_data->object);

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
	GObjectClass *class;
	GParamSpec **properties;
	GtkWidget *widget, *table;
	struct _prop_data *prop_data = data;
	guint ii, n_properties;
	gint row = 0;

	if (old)
		return old;

	table = gtk_table_new (2, 2, FALSE);
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

	class = G_OBJECT_GET_CLASS (prop_data->object);
	properties = g_object_class_list_properties (class, &n_properties);

	for (ii = 0; ii < n_properties; ii++) {
		const gchar *blurb;

		if ((properties[ii]->flags & CAMEL_PARAM_PERSISTENT) == 0)
			continue;

		blurb = g_param_spec_get_blurb (properties[ii]);

		switch (properties[ii]->value_type) {
			case G_TYPE_BOOLEAN:
				widget = gtk_check_button_new_with_label (blurb);
				e_mutual_binding_new (
					prop_data->object,
					properties[ii]->name,
					widget, "active");
				gtk_widget_show (widget);
				gtk_table_attach (
					GTK_TABLE (table), widget,
					0, 2, row, row + 1,
					GTK_FILL | GTK_EXPAND, 0, 0, 0);
				row++;
				break;
			default:
				g_warn_if_reached ();
				break;
		}
	}

	g_free (properties);

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
emfp_dialog_got_folder_quota (CamelFolder *folder,
                              const gchar *folder_uri,
                              CamelFolderQuotaInfo *quota,
                              gpointer data)
{
	GtkWidget *dialog, *w;
	GtkWidget *content_area;
	struct _prop_data *prop_data;
	GSList *l;
	gint32 i,deleted;
	EMConfig *ec;
	EMConfigTargetFolder *target;
	EShellWindow *shell_window;
	EShellView *shell_view;
	CamelStore *local_store;
	CamelStore *parent_store;
	gboolean hide_deleted;
	GConfClient *gconf;
	const gchar *name;

	if (folder == NULL)
		return;

	shell_view = E_SHELL_VIEW (data);
	shell_window = e_shell_view_get_shell_window (shell_view);

	local_store = e_mail_local_get_store ();
	parent_store = camel_folder_get_parent_store (folder);

	prop_data = g_malloc0 (sizeof (*prop_data));
	prop_data->object = g_object_ref (folder);
	prop_data->quota = camel_folder_quota_info_clone (quota);

	/*
	  Get number of VISIBLE and DELETED messages, instead of TOTAL messages.  VISIBLE+DELETED
	   gives the correct count that matches the label below the Send & Receive button
	*/
	name = camel_folder_get_name (folder);
	prop_data->total = folder->summary->visible_count;
	prop_data->unread = folder->summary->unread_count;
	deleted = folder->summary->deleted_count;

	gconf = mail_config_get_gconf_client ();
	hide_deleted = !gconf_client_get_bool(gconf, "/apps/evolution/mail/display/show_deleted", NULL);

	/*
	   Do the calculation only for those accounts that support VTRASHes
	 */
	if (parent_store->flags & CAMEL_STORE_VTRASH) {
		if (CAMEL_IS_VTRASH_FOLDER(folder))
			prop_data->total += deleted;
		else if (!hide_deleted && deleted > 0)
			prop_data->total += deleted;
	}

	/*
	 * If the ffolder is junk folder, get total number of mails.
	 */
	if (parent_store->flags & CAMEL_STORE_VJUNK)
		prop_data->total = camel_folder_summary_count (folder->summary);

	if (parent_store == local_store
	    && (!strcmp (name, "Drafts")
		|| !strcmp (name, "Templates")
		|| !strcmp (name, "Inbox")
		|| !strcmp (name, "Outbox")
		|| !strcmp (name, "Sent"))) {
		emfp_items[EMFP_FOLDER_SECTION].label = gettext (name);
		if (!emfp_items_translated) {
			for (i = 0; i < G_N_ELEMENTS (emfp_items); i++) {
				if (emfp_items[i].label)
					emfp_items[i].label = _(emfp_items[i].label);
			}
			emfp_items_translated = TRUE;
		}
	} else if (!strcmp (name, "INBOX"))
		emfp_items[EMFP_FOLDER_SECTION].label = _("Inbox");
	else
		emfp_items[EMFP_FOLDER_SECTION].label = (gchar *) name;

	dialog = gtk_dialog_new_with_buttons (
		_("Folder Properties"), GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_CLOSE, GTK_RESPONSE_OK, NULL);
	gtk_window_set_default_size ((GtkWindow *) dialog, 192, 160);

	gtk_widget_ensure_style (dialog);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);

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
	for (i = 0; i < G_N_ELEMENTS (emfp_items); i++)
		l = g_slist_prepend(l, &emfp_items[i]);
	e_config_add_items((EConfig *)ec, l, NULL, NULL, emfp_free, prop_data);

	target = em_config_target_new_folder(ec, folder, folder_uri);
	e_config_set_target((EConfig *)ec, (EConfigTarget *)target);
	w = e_config_create_widget((EConfig *)ec);

	gtk_box_pack_start (GTK_BOX (content_area), w, TRUE, TRUE, 0);

	/* we do 'apply on ok' ... since instant apply may apply some very long running tasks */

	g_signal_connect (dialog, "response", G_CALLBACK (emfp_dialog_response), prop_data);
	gtk_widget_show (dialog);
}

static void
emfp_dialog_got_folder (gchar *uri, CamelFolder *folder, gpointer data)
{
	EShellView *shell_view = data;

	/* this should be called in a thread too */
	mail_get_folder_quota (
		folder, uri, emfp_dialog_got_folder_quota,
		shell_view, mail_msg_unordered_push);
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
em_folder_properties_show (EShellView *shell_view,
                           CamelFolder *folder,
                           const gchar *uri)
{
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (uri != NULL);

	/* HACK: its the old behaviour, not very 'neat' but it works */
	if (!strncmp (uri, "vfolder:", 8)) {
		CamelURL *url = camel_url_new (uri, NULL);

		/* MORE HACK: UNMATCHED is a special folder which you can't modify, so check for it here */
		if (url == NULL
		    || url->fragment == NULL
		    || strcmp(url->fragment, CAMEL_UNMATCHED_NAME) != 0) {
			if (url)
				camel_url_free (url);
			vfolder_edit_rule (uri);
			return;
		}
		if (url != NULL)
			camel_url_free (url);
	}

	if (folder == NULL)
		mail_get_folder(uri, 0, emfp_dialog_got_folder, shell_view, mail_msg_unordered_push);
	else
		emfp_dialog_got_folder((gchar *)uri, folder, shell_view);
}
