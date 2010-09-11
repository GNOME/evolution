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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "url-editor-dialog.h"
#include <libedataserverui/e-passwords.h>
#include <libedataserver/e-url.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#include <e-util/e-util.h>
#include <e-util/e-util-private.h>

static GtkDialogClass *parent_class = NULL;

static void
create_uri (UrlEditorDialog *dialog)
{
	EPublishUri *uri;

	uri = dialog->uri;

	if (uri->service_type == TYPE_URI) {
		if (uri->location)
			g_free (uri->location);
		uri->location = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->server_entry)));
	} else {
		const gchar *method = "file";
		gchar *server, *file, *port, *username, *password;

		server   = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->server_entry)));
		file     = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->file_entry)));
		port     = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->port_entry)));
		username = g_uri_escape_string (gtk_entry_get_text (GTK_ENTRY (dialog->username_entry)), "", FALSE);
		password = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->password_entry)));

		switch (uri->service_type) {
		case TYPE_SMB:
			method = "smb";
			break;

		case TYPE_SFTP:
			method = "sftp";
			break;

		case TYPE_ANON_FTP:
			g_free (username);
			username = g_strdup ("anonymous");
		case TYPE_FTP:
			method = "ftp";
			break;

		case TYPE_DAV:
			method = "dav";
			break;

		case TYPE_DAVS:
			method = "davs";
			break;
		}

		if (uri->location)
			g_free (uri->location);
		uri->location = g_strdup_printf ("%s://%s%s%s%s%s%s%s",
						 method,
						 username, (username[0] != '\0') ? "@" : "",
						 server,
						 (port[0] != '\0') ? ":" : "", port,
						 (file[0] != '/') ? "/" : "", file);

		g_free (server);
		g_free (file);
		g_free (port);
		g_free (username);
		g_free (password);
	}

	uri->fb_duration_value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (dialog->fb_duration_spin));
	uri->fb_duration_type  = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->fb_duration_combo));
}

static void
check_input (UrlEditorDialog *dialog)
{
	gint n = 0;
	GSList *sources;
	EPublishUri *uri;

	uri = dialog->uri;

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->type_selector)) == 1) {
		gtk_widget_show (dialog->fb_duration_label);
		gtk_widget_show (dialog->fb_duration_spin);
		gtk_widget_show (dialog->fb_duration_combo);
	} else {
		gtk_widget_hide (dialog->fb_duration_label);
		gtk_widget_hide (dialog->fb_duration_spin);
		gtk_widget_hide (dialog->fb_duration_combo);
	}

	if (gtk_widget_get_sensitive (dialog->events_selector)) {
		sources = e_source_selector_get_selection (E_SOURCE_SELECTOR (dialog->events_selector));
		n += g_slist_length (sources);
	}
	if (n == 0)
		goto fail;

	/* This should probably be more complex, since ' ' isn't a valid server name */
	switch (uri->service_type) {
	case TYPE_SMB:
	case TYPE_SFTP:
	case TYPE_FTP:
	case TYPE_DAV:
	case TYPE_DAVS:
	case TYPE_ANON_FTP:
		if (!strlen (gtk_entry_get_text (GTK_ENTRY (dialog->server_entry)))) goto fail;
		if (!strlen (gtk_entry_get_text (GTK_ENTRY (dialog->file_entry))))   goto fail;
		break;
	case TYPE_URI:
		if (!strlen (gtk_entry_get_text (GTK_ENTRY (dialog->server_entry)))) goto fail;
		break;
	}

	create_uri (dialog);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, TRUE);
	return;
fail:
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);
}

static void
source_selection_changed (ESourceSelector *selector, UrlEditorDialog *dialog)
{
	check_input (dialog);
}

static void
publish_service_changed (GtkComboBox *combo, UrlEditorDialog *dialog)
{
	gint selected = gtk_combo_box_get_active (combo);
	EPublishUri *uri;

	uri = dialog->uri;

	/* Big mess that switches around all the fields to match the source type
	 * the user has selected. Tries to keep field contents where possible */
	switch (selected) {
	case TYPE_SMB:
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->server_label), "_Server:");
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->port_label), "_Port:");
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->port_label), "S_hare:");
		gtk_entry_set_text (GTK_ENTRY (dialog->port_entry), "");
		gtk_widget_show (dialog->file_hbox);
		gtk_widget_show (dialog->optional_label);
		gtk_widget_show (dialog->port_hbox);
		gtk_widget_show (dialog->username_hbox);
		gtk_widget_show (dialog->password_hbox);
		gtk_widget_show (dialog->remember_pw);
		break;
	case TYPE_SFTP:
	case TYPE_FTP:
	case TYPE_DAV:
	case TYPE_DAVS:
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->server_label), "_Server:");
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->port_label), "_Port:");
		if (uri->service_type == TYPE_SMB)
			gtk_entry_set_text (GTK_ENTRY (dialog->port_entry), "");
		else if (uri->service_type == TYPE_URI)
			gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), "");
		gtk_widget_show (dialog->file_hbox);
		gtk_widget_show (dialog->optional_label);
		gtk_widget_show (dialog->port_hbox);
		gtk_widget_show (dialog->username_hbox);
		gtk_widget_show (dialog->password_hbox);
		gtk_widget_show (dialog->remember_pw);
		break;
	case TYPE_ANON_FTP:
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->server_label), "_Server:");
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->port_label), "_Port:");
		if (uri->service_type == TYPE_SMB)
			gtk_entry_set_text (GTK_ENTRY (dialog->port_entry), "");
		else if (uri->service_type == TYPE_URI)
			gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), "");
		gtk_widget_show (dialog->file_hbox);
		gtk_widget_show (dialog->optional_label);
		gtk_widget_show (dialog->port_hbox);
		gtk_widget_hide (dialog->username_hbox);
		gtk_widget_hide (dialog->password_hbox);
		gtk_widget_hide (dialog->remember_pw);
		break;
	case TYPE_URI:
		gtk_label_set_text_with_mnemonic (GTK_LABEL (dialog->server_label), "_Location (URI):");
		gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), "");
		gtk_widget_hide (dialog->file_hbox);
		gtk_widget_hide (dialog->optional_label);
		gtk_widget_hide (dialog->port_hbox);
		gtk_widget_hide (dialog->username_hbox);
		gtk_widget_hide (dialog->password_hbox);
		gtk_widget_hide (dialog->remember_pw);
	}
	uri->service_type = selected;
	check_input (dialog);
}

static void
type_selector_changed (GtkComboBox *combo, UrlEditorDialog *dialog)
{
	gint selected = gtk_combo_box_get_active (combo);
	EPublishUri *uri;

	uri = dialog->uri;
	uri->publish_format = selected;

	check_input (dialog);
}

static void
frequency_changed_cb (GtkComboBox *combo, UrlEditorDialog *dialog)
{
	gint selected = gtk_combo_box_get_active (combo);

	EPublishUri *uri;

	uri = dialog->uri;
	uri->publish_frequency = selected;
}

static void
server_entry_changed (GtkEntry *entry, UrlEditorDialog *dialog)
{
	check_input (dialog);
}

static void
file_entry_changed (GtkEntry *entry, UrlEditorDialog *dialog)
{
	check_input (dialog);
}

static void
port_entry_changed (GtkEntry *entry, UrlEditorDialog *dialog)
{
}

static void
username_entry_changed (GtkEntry *entry, UrlEditorDialog *dialog)
{
}

static void
password_entry_changed (GtkEntry *entry, UrlEditorDialog *dialog)
{
}

static void
remember_pw_toggled (GtkToggleButton *toggle, UrlEditorDialog *dialog)
{
}

static void
set_from_uri (UrlEditorDialog *dialog)
{
	EPublishUri *uri;
	gchar *method;
	EUri *euri = NULL;

	uri = dialog->uri;

	euri = e_uri_new (uri->location);
	/* determine our method */
	method = euri->protocol;
	if (strcmp ((const gchar *)method, "smb") == 0)
		uri->service_type = TYPE_SMB;
	else if (strcmp ((const gchar *)method, "sftp") == 0)
		uri->service_type = TYPE_SFTP;
	else if (strcmp ((const gchar *)method, "ftp") == 0)
		/* we set TYPE_FTP here for now. if we don't find a
		 * username later, we'll change it to TYPE_ANON_FTP */
		uri->service_type = TYPE_FTP;
	else if (strcmp ((const gchar *)method, "dav") == 0)
		uri->service_type = TYPE_DAV;
	else if (strcmp ((const gchar *)method, "davs") == 0)
		uri->service_type = TYPE_DAVS;
	else
		uri->service_type = TYPE_URI;

	if (euri->user)
		gtk_entry_set_text (GTK_ENTRY (dialog->username_entry), euri->user);

	if (euri->host)
		gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), euri->host);

	if (euri->port) {
		gchar *port;
		port = g_strdup_printf ("%d", euri->port);
		gtk_entry_set_text (GTK_ENTRY (dialog->port_entry), port);
		g_free (port);
	}

	if (euri->path)
		gtk_entry_set_text (GTK_ENTRY (dialog->file_entry), euri->path);

	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->publish_service), uri->service_type);

	e_uri_free (euri);
}

static gboolean
url_editor_dialog_construct (UrlEditorDialog *dialog)
{
	GtkWidget *toplevel;
	GtkWidget *content_area;
	GConfClient *gconf;
	GtkSizeGroup *group;
	EPublishUri *uri;

	gconf = gconf_client_get_default ();

	dialog->builder = gtk_builder_new ();
	e_load_ui_builder_definition (dialog->builder, "publish-calendar.ui");

#define GW(name) ((dialog->name) = e_builder_get_widget (dialog->builder, #name))
	GW(type_selector);
	GW(fb_duration_label);
	GW(fb_duration_spin);
	GW(fb_duration_combo);
	GW(publish_frequency);

	GW(events_swin);

	GW(publish_service);
	GW(server_entry);
	GW(file_entry);

	GW(port_entry);
	GW(username_entry);
	GW(password_entry);
	GW(remember_pw);

	GW(optional_label);

	GW(port_hbox);
	GW(username_hbox);
	GW(password_hbox);
	GW(server_hbox);
	GW(file_hbox);

	GW(port_label);
	GW(username_label);
	GW(password_label);
	GW(server_label);
	GW(file_label);
#undef GW

	uri = dialog->uri;

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	toplevel = e_builder_get_widget (dialog->builder, "publishing toplevel");
	gtk_container_add (GTK_CONTAINER (content_area), toplevel);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
#if !GTK_CHECK_VERSION(2,90,7)
	g_object_set (dialog, "has-separator", FALSE, NULL);
#endif

	dialog->cancel = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	dialog->ok = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);

	dialog->events_source_list = e_source_list_new_for_gconf (gconf, "/apps/evolution/calendar/sources");
	dialog->events_selector = e_source_selector_new (dialog->events_source_list);
	gtk_widget_show (dialog->events_selector);
	gtk_container_add (GTK_CONTAINER (dialog->events_swin), dialog->events_selector);

	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (group, dialog->type_selector);
	gtk_size_group_add_widget (group, dialog->publish_frequency);
	g_object_unref (group);

	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (group, dialog->publish_service);
	gtk_size_group_add_widget (group, dialog->server_entry);
	gtk_size_group_add_widget (group, dialog->file_entry);
	gtk_size_group_add_widget (group, dialog->port_entry);
	gtk_size_group_add_widget (group, dialog->username_entry);
	gtk_size_group_add_widget (group, dialog->password_entry);
	g_object_unref (group);

	if (uri == NULL) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->publish_frequency), 0);
		gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->type_selector), 0);
		gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->publish_service), 0);

		dialog->uri = g_new0 (EPublishUri, 1);
		uri = dialog->uri;
		uri->enabled = TRUE;
	} else {
		ESource *source;
		GSList *p;

		for (p = uri->events; p; p = g_slist_next (p)) {
			gchar *source_uid = g_strdup (p->data);
			source = e_source_list_peek_source_by_uid (dialog->events_source_list, source_uid);
			e_source_selector_select_source ((ESourceSelector *) dialog->events_selector, source);
			g_free (source_uid);
		}

		if (uri->location && strlen (uri->location)) {
			set_from_uri (dialog);
		}
		gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->publish_frequency), uri->publish_frequency);
		gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->type_selector), uri->publish_format);

		uri->password = e_passwords_get_password ("Calendar", uri->location);
		if (uri->password) {
			if (strlen (uri->password) != 0) {
				gtk_entry_set_text (GTK_ENTRY (dialog->password_entry), uri->password);
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->remember_pw), TRUE);
			} else {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->remember_pw), FALSE);
			}
		}
	}

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->fb_duration_spin), uri->fb_duration_value);
	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->fb_duration_combo), uri->fb_duration_type);

	g_signal_connect (G_OBJECT (dialog->publish_service), "changed",           G_CALLBACK (publish_service_changed),  dialog);
	g_signal_connect (G_OBJECT (dialog->type_selector),   "changed",           G_CALLBACK (type_selector_changed),    dialog);
	g_signal_connect (G_OBJECT (dialog->publish_frequency),   "changed",           G_CALLBACK (frequency_changed_cb),    dialog);
	g_signal_connect (G_OBJECT (dialog->events_selector), "selection_changed", G_CALLBACK (source_selection_changed), dialog);

	g_signal_connect (G_OBJECT (dialog->server_entry),    "changed",           G_CALLBACK (server_entry_changed),     dialog);
	g_signal_connect (G_OBJECT (dialog->file_entry),      "changed",           G_CALLBACK (file_entry_changed),       dialog);
	g_signal_connect (G_OBJECT (dialog->port_entry),      "changed",           G_CALLBACK (port_entry_changed),       dialog);
	g_signal_connect (G_OBJECT (dialog->username_entry),  "changed",           G_CALLBACK (username_entry_changed),   dialog);
	g_signal_connect (G_OBJECT (dialog->password_entry),  "changed",           G_CALLBACK (password_entry_changed),   dialog);
	g_signal_connect (G_OBJECT (dialog->remember_pw),     "toggled",           G_CALLBACK (remember_pw_toggled),      dialog);

	g_object_unref (gconf);

	check_input (dialog);

	return TRUE;
}

GtkWidget *
url_editor_dialog_new (GtkTreeModel *url_list_model, EPublishUri *uri)
{
	UrlEditorDialog *dialog;

	dialog = (UrlEditorDialog *) g_object_new (URL_EDITOR_DIALOG_TYPE, NULL);
	dialog->url_list_model = g_object_ref (url_list_model);
	dialog->uri = uri;

	if (!uri)
		gtk_window_set_title (GTK_WINDOW (dialog), _("New Location"));
	else
		gtk_window_set_title (GTK_WINDOW (dialog), _("Edit Location"));

	if (url_editor_dialog_construct (dialog))
		return GTK_WIDGET (dialog);

	g_object_unref (dialog);
	return NULL;
}

static void
url_editor_dialog_dispose (GObject *obj)
{
	UrlEditorDialog *dialog = (UrlEditorDialog *) obj;

	if (dialog->url_list_model) {
		g_object_unref (dialog->url_list_model);
		dialog->url_list_model = NULL;
	}
	if (dialog->builder) {
		g_object_unref (dialog->builder);
		dialog->builder = NULL;
	}

	((GObjectClass *)(parent_class))->dispose (obj);
}

static void
url_editor_dialog_class_init (UrlEditorDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;
	parent_class = g_type_class_ref (GTK_TYPE_DIALOG);

	object_class->dispose = url_editor_dialog_dispose;
}

static void
url_editor_dialog_init (UrlEditorDialog *dialog)
{
}

GType
url_editor_dialog_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo info = {
			sizeof (UrlEditorDialogClass),
			NULL, NULL,
			(GClassInitFunc) url_editor_dialog_class_init,
			NULL, NULL,
			sizeof (UrlEditorDialog),
			0,
			(GInstanceInitFunc) url_editor_dialog_init,
		};

		type = g_type_register_static (GTK_TYPE_DIALOG, "UrlEditorDialog", &info, 0);
	}

	return type;
}

gboolean
url_editor_dialog_run (UrlEditorDialog *dialog)
{
	gint response;

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response == GTK_RESPONSE_OK) {
		GSList *l, *p;

		if (dialog->uri->password)
			g_free (dialog->uri->password);
		if (dialog->uri->events) {
			g_slist_foreach (dialog->uri->events, (GFunc) g_free, NULL);
			dialog->uri->events = NULL;
		}

		create_uri (dialog);

		dialog->uri->password = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->password_entry)));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->remember_pw))) {
			e_passwords_add_password (dialog->uri->location, dialog->uri->password);
			e_passwords_remember_password ("Calendar", dialog->uri->location);
		} else {
			e_passwords_forget_password ("Calendar", dialog->uri->location);
		}

		l = e_source_selector_get_selection (E_SOURCE_SELECTOR (dialog->events_selector));
		for (p = l; p; p = g_slist_next (p))
			dialog->uri->events = g_slist_append (dialog->uri->events, g_strdup (e_source_peek_uid (p->data)));
	}
	gtk_widget_hide_all (GTK_WIDGET (dialog));

	return response == GTK_RESPONSE_OK;
}
