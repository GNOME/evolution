/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "url-editor-dialog.h"

#include <string.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>
#include <e-util/e-util-private.h>

#include <shell/e-shell.h>

G_DEFINE_TYPE (
	UrlEditorDialog,
	url_editor_dialog,
	GTK_TYPE_DIALOG)

static void
create_uri (UrlEditorDialog *dialog)
{
	EPublishUri *uri;

	uri = dialog->uri;

	if (uri->service_type == TYPE_URI) {
		g_free (uri->location);
		uri->location = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->server_entry)));
	} else {
		const gchar *method = "file";
		gchar *server, *file, *port, *username, *password;

		server = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->server_entry)));
		file = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->file_entry)));
		port = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->port_entry)));
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
			method = "ftp";
			break;

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

		g_free (uri->location);
		uri->location = g_strdup_printf (
			"%s://%s%s%s%s%s%s%s",
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
	uri->fb_duration_type = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->fb_duration_combo));
}

static void
check_input (UrlEditorDialog *dialog)
{
	gint n = 0;
	GList *sources;
	EPublishUri *uri;

	uri = dialog->uri;

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->type_selector)) == URI_PUBLISH_AS_ICAL) {
		gtk_widget_hide (dialog->fb_duration_label);
		gtk_widget_hide (dialog->fb_duration_spin);
		gtk_widget_hide (dialog->fb_duration_combo);
	} else {
		gtk_widget_show (dialog->fb_duration_label);
		gtk_widget_show (dialog->fb_duration_spin);
		gtk_widget_show (dialog->fb_duration_combo);
	}

	if (gtk_widget_get_sensitive (dialog->events_selector)) {
		sources = e_source_selector_get_selection (
			E_SOURCE_SELECTOR (dialog->events_selector));
		n += g_list_length (sources);
		g_list_free_full (sources, (GDestroyNotify) g_object_unref);
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
source_selection_changed (ESourceSelector *selector,
                          UrlEditorDialog *dialog)
{
	check_input (dialog);
}

static void
publish_service_changed (GtkComboBox *combo,
                         UrlEditorDialog *dialog)
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
		if (uri->service_type != TYPE_URI)
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
type_selector_changed (GtkComboBox *combo,
                       UrlEditorDialog *dialog)
{
	gint selected = gtk_combo_box_get_active (combo);
	EPublishUri *uri;

	uri = dialog->uri;
	uri->publish_format = selected;

	check_input (dialog);
}

static void
frequency_changed_cb (GtkComboBox *combo,
                      UrlEditorDialog *dialog)
{
	gint selected = gtk_combo_box_get_active (combo);

	EPublishUri *uri;

	uri = dialog->uri;
	uri->publish_frequency = selected;
}

static void
server_entry_changed (GtkEntry *entry,
                      UrlEditorDialog *dialog)
{
	check_input (dialog);
}

static void
file_entry_changed (GtkEntry *entry,
                    UrlEditorDialog *dialog)
{
	check_input (dialog);
}

static void
port_entry_changed (GtkEntry *entry,
                    UrlEditorDialog *dialog)
{
}

static void
username_entry_changed (GtkEntry *entry,
                        UrlEditorDialog *dialog)
{
}

static void
password_entry_changed (GtkEntry *entry,
                        UrlEditorDialog *dialog)
{
}

static void
remember_pw_toggled (GtkToggleButton *toggle,
                     UrlEditorDialog *dialog)
{
}

static void
set_from_uri (UrlEditorDialog *dialog)
{
	EPublishUri *uri;
	GUri *guri;
	const gchar *scheme;
	const gchar *user;
	const gchar *host;
	const gchar *path;
	gint port;

	uri = dialog->uri;

	guri = g_uri_parse (uri->location, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	g_return_if_fail (guri != NULL);

	/* determine our service type */
	scheme = g_uri_get_scheme (guri);
	g_return_if_fail (scheme != NULL);

	if (strcmp (scheme, "smb") == 0)
		uri->service_type = TYPE_SMB;
	else if (strcmp (scheme, "sftp") == 0)
		uri->service_type = TYPE_SFTP;
	else if (strcmp (scheme, "ftp") == 0)
		/* we set TYPE_FTP here for now. if we don't find a
		 * username later, we'll change it to TYPE_ANON_FTP */
		uri->service_type = TYPE_FTP;
	else if (strcmp (scheme, "dav") == 0)
		uri->service_type = TYPE_DAV;
	else if (strcmp (scheme, "davs") == 0)
		uri->service_type = TYPE_DAVS;
	else
		uri->service_type = TYPE_URI;

	user = g_uri_get_user (guri);
	host = g_uri_get_host (guri);
	port = g_uri_get_port (guri);
	path = g_uri_get_path (guri);

	if (user != NULL)
		gtk_entry_set_text (GTK_ENTRY (dialog->username_entry), user);

	if (host != NULL)
		gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), host);

	if (port > 0) {
		gchar *port_str;
		port_str = g_strdup_printf ("%d", port);
		gtk_entry_set_text (GTK_ENTRY (dialog->port_entry), port_str);
		g_free (port_str);
	}

	if (path != NULL)
		gtk_entry_set_text (GTK_ENTRY (dialog->file_entry), path);

	if (uri->service_type == TYPE_URI)
		gtk_entry_set_text (GTK_ENTRY (dialog->server_entry), uri->location);

	gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->publish_service), uri->service_type);

	g_uri_unref (guri);
}

static gboolean
url_editor_dialog_construct (UrlEditorDialog *dialog)
{
	EShell *shell;
	GtkWidget *toplevel;
	GtkWidget *content_area;
	GtkSizeGroup *group;
	EPublishUri *uri;
	ESourceRegistry *registry;

	dialog->builder = gtk_builder_new ();
	e_load_ui_builder_definition (dialog->builder, "publish-calendar.ui");

#define GW(name) ((dialog->name) = e_builder_get_widget (dialog->builder, #name))
	GW (type_selector);
	GW (fb_duration_label);
	GW (fb_duration_spin);
	GW (fb_duration_combo);
	GW (publish_frequency);

	GW (events_swin);

	GW (publish_service);
	GW (server_entry);
	GW (file_entry);

	GW (port_entry);
	GW (username_entry);
	GW (password_entry);
	GW (remember_pw);

	GW (optional_label);

	GW (port_hbox);
	GW (username_hbox);
	GW (password_hbox);
	GW (server_hbox);
	GW (file_hbox);

	GW (port_label);
	GW (username_label);
	GW (password_label);
	GW (server_label);
	GW (file_label);
#undef GW

	uri = dialog->uri;

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	toplevel = e_builder_get_widget (dialog->builder, "publishing toplevel");
	gtk_container_add (GTK_CONTAINER (content_area), toplevel);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);

	dialog->cancel = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
	dialog->ok = gtk_dialog_add_button (GTK_DIALOG (dialog), _("_OK"), GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, FALSE);

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);
	dialog->events_selector = e_source_selector_new (
		registry, E_SOURCE_EXTENSION_CALENDAR);
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
			const gchar *uid = p->data;

			source = e_source_registry_ref_source (
				registry, uid);
			e_source_selector_select_source (
				E_SOURCE_SELECTOR (dialog->events_selector),
				source);
			g_object_unref (source);
		}

		if (uri->location && strlen (uri->location)) {
			set_from_uri (dialog);
		}

		gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->publish_frequency), uri->publish_frequency);
		gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->type_selector), uri->publish_format);

		uri->password = e_passwords_get_password (uri->location);
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

	type_selector_changed (GTK_COMBO_BOX (dialog->type_selector), dialog);
	frequency_changed_cb (GTK_COMBO_BOX (dialog->publish_frequency), dialog);
	publish_service_changed (GTK_COMBO_BOX (dialog->publish_service), dialog);

	g_signal_connect (
		dialog->publish_service, "changed",
		G_CALLBACK (publish_service_changed), dialog);
	g_signal_connect (
		dialog->type_selector, "changed",
		G_CALLBACK (type_selector_changed), dialog);
	g_signal_connect (
		dialog->publish_frequency, "changed",
		G_CALLBACK (frequency_changed_cb), dialog);
	g_signal_connect (
		dialog->events_selector, "selection_changed",
		G_CALLBACK (source_selection_changed), dialog);

	g_signal_connect (
		dialog->server_entry, "changed",
		G_CALLBACK (server_entry_changed), dialog);
	g_signal_connect (
		dialog->file_entry, "changed",
		G_CALLBACK (file_entry_changed), dialog);
	g_signal_connect (
		dialog->port_entry, "changed",
		G_CALLBACK (port_entry_changed), dialog);
	g_signal_connect (
		dialog->username_entry, "changed",
		G_CALLBACK (username_entry_changed), dialog);
	g_signal_connect (
		dialog->password_entry,"changed",
		G_CALLBACK (password_entry_changed), dialog);
	g_signal_connect (
		dialog->remember_pw, "toggled",
		G_CALLBACK (remember_pw_toggled), dialog);

	check_input (dialog);

	return TRUE;
}

GtkWidget *
url_editor_dialog_new (GtkTreeModel *url_list_model,
                       EPublishUri *uri)
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

	g_clear_object (&dialog->url_list_model);
	g_clear_object (&dialog->builder);

	G_OBJECT_CLASS (url_editor_dialog_parent_class)->dispose (obj);
}

static void
url_editor_dialog_class_init (UrlEditorDialogClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = url_editor_dialog_dispose;
}

static void
url_editor_dialog_init (UrlEditorDialog *dialog)
{
}

gboolean
url_editor_dialog_run (UrlEditorDialog *dialog)
{
	gint response;

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response == GTK_RESPONSE_OK) {
		GList *list, *link;

		g_free (dialog->uri->password);
		if (dialog->uri->events) {
			g_slist_foreach (dialog->uri->events, (GFunc) g_free, NULL);
			dialog->uri->events = NULL;
		}

		create_uri (dialog);

		dialog->uri->password = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->password_entry)));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->remember_pw))) {
			e_passwords_add_password (dialog->uri->location, dialog->uri->password);
			e_passwords_remember_password (dialog->uri->location);
		} else {
			e_passwords_forget_password (dialog->uri->location);
		}

		list = e_source_selector_get_selection (
			E_SOURCE_SELECTOR (dialog->events_selector));

		for (link = list; link != NULL; link = g_list_next (link)) {
			ESource *source;
			const gchar *uid;

			source = E_SOURCE (link->data);
			uid = e_source_get_uid (source);
			dialog->uri->events = g_slist_append (
				dialog->uri->events, g_strdup (uid));
		}

		g_list_free_full (list, (GDestroyNotify) g_object_unref);
	}
	gtk_widget_hide (GTK_WIDGET (dialog));

	return response == GTK_RESPONSE_OK;
}
