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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include "e-util/e-util-private.h"

#include "e-mail-backend.h"
#include "em-folder-tree-model.h"
#include "em-utils.h"
#include "em-vfolder-editor-context.h"
#include "em-vfolder-editor.h"
#include "em-vfolder-editor-rule.h"
#include "mail-autofilter.h"
#include "e-mail-ui-session.h"

#include "mail-vfolder-ui.h"

#define d(x)  /* (printf("%s:%s: ",  G_STRLOC, G_STRFUNC), (x))*/

/* NOTE: Once mail is moved to EDS, this context needs to be created ofr Mail UI. */
extern EMVFolderContext *context;	/* context remains open all time */

void
vfolder_edit (EMailBackend *backend,
              GtkWindow *parent_window)
{
	EShellBackend *shell_backend;
	GtkWidget *dialog;
	const gchar *config_dir;
	gchar *filename;
	EMailSession *session;

	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (GTK_IS_WINDOW (parent_window));

	shell_backend = E_SHELL_BACKEND (backend);
	config_dir = e_shell_backend_get_config_dir (shell_backend);
	filename = g_build_filename (config_dir, "vfolders.xml", NULL);
	session = e_mail_backend_get_session (backend);

	vfolder_load_storage (session);

	dialog = em_vfolder_editor_new (context);
	gtk_window_set_title (
		GTK_WINDOW (dialog), _("Search Folders"));
	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), parent_window);

	switch (gtk_dialog_run (GTK_DIALOG (dialog))) {
		case GTK_RESPONSE_OK:
			e_rule_context_save ((ERuleContext *) context, filename);
			break;
		default:
			e_rule_context_revert ((ERuleContext *) context, filename);
			break;
	}

	gtk_widget_destroy (dialog);
}

static void
vfolder_edit_response_cb (GtkWidget *dialog,
                          gint response_id,
                          gpointer user_data)
{
	if (response_id == GTK_RESPONSE_OK) {
		GObject *object;
		EFilterRule *rule;
		EFilterRule *newrule;
		const gchar *config_dir;
		gchar *user;

		object = G_OBJECT (dialog);
		rule = g_object_get_data (object, "vfolder-rule");
		newrule = g_object_get_data (object, "vfolder-newrule");

		e_filter_rule_persist_customizations (newrule);

		e_filter_rule_copy (rule, newrule);
		config_dir = mail_session_get_config_dir ();
		user = g_build_filename (config_dir, "vfolders.xml", NULL);
		e_rule_context_save ((ERuleContext *) context, user);
		g_free (user);
	}

	gtk_widget_destroy (dialog);
}

void
vfolder_edit_rule (EMailSession *session,
                   const gchar *folder_uri,
                   EAlertSink *alert_sink)
{
	GtkWidget *dialog;
	GtkWidget *widget;
	GtkWidget *container;
	EFilterRule *rule = NULL;
	EFilterRule *newrule;
	gchar *folder_name = NULL;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (E_IS_ALERT_SINK (alert_sink));

	e_mail_folder_uri_parse (
		CAMEL_SESSION (session), folder_uri,
		NULL, &folder_name, NULL);

	if (folder_name != NULL) {
		rule = e_rule_context_find_rule (
			(ERuleContext *) context, folder_name, NULL);
		g_free (folder_name);
	}

	if (rule == NULL) {
		/* TODO: we should probably just create it ... */
		e_alert_submit (
			alert_sink, "mail:vfolder-notexist",
			folder_uri, NULL);
		return;
	}

	g_object_ref (rule);
	newrule = e_filter_rule_clone (rule);

	dialog = gtk_dialog_new_with_buttons (
		_("Edit Search Folder"), NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK, NULL);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 500);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_set_spacing (GTK_BOX (container), 6);

	widget = e_filter_rule_get_widget (newrule, (ERuleContext *) context);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	g_object_set_data_full (
		G_OBJECT (dialog), "vfolder-rule",
		rule, (GDestroyNotify) g_object_unref);
	g_object_set_data_full (
		G_OBJECT (dialog), "vfolder-newrule",
		newrule, (GDestroyNotify) g_object_unref);

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (vfolder_edit_response_cb), NULL);

	gtk_widget_show (dialog);
}

static void
new_rule_clicked (GtkWidget *w,
                  gint button,
                  gpointer data)
{
	if (button == GTK_RESPONSE_OK) {
		const gchar *config_dir;
		gchar *user;
		EFilterRule *rule = g_object_get_data ((GObject *) w, "rule");
		EAlert *alert = NULL;

		if (!e_filter_rule_validate (rule, &alert)) {
			e_alert_run_dialog (GTK_WINDOW (w), alert);
			g_object_unref (alert);
			return;
		}

		if (e_rule_context_find_rule (
			(ERuleContext *) context, rule->name, rule->source)) {
			e_alert_run_dialog_for_args (
				GTK_WINDOW (w), "mail:vfolder-notunique",
				rule->name, NULL);
			return;
		}

		g_object_ref (rule);
		e_rule_context_add_rule ((ERuleContext *) context, rule);
		config_dir = mail_session_get_config_dir ();
		user = g_build_filename (config_dir, "vfolders.xml", NULL);
		e_rule_context_save ((ERuleContext *) context, user);
		g_free (user);
	}

	gtk_widget_destroy (w);
}

static void
new_rule_changed_cb (EFilterRule *rule,
                     GtkDialog *dialog)
{
	g_return_if_fail (rule != NULL);
	g_return_if_fail (dialog != NULL);

	gtk_dialog_set_response_sensitive (
		dialog, GTK_RESPONSE_OK, rule->parts != NULL);
}

/* clones a filter/search rule into a matching vfolder rule
 * (assuming the same system definitions) */
EFilterRule *
vfolder_clone_rule (EMailSession *session,
                    EFilterRule *in)
{
	EFilterRule *rule;
	xmlNodePtr xml;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	rule = em_vfolder_editor_rule_new (session);

	xml = e_filter_rule_xml_encode (in);
	e_filter_rule_xml_decode (rule, xml, (ERuleContext *) context);
	xmlFreeNodeList (xml);

	return rule;
}

static void
release_rule_notify_cb (gpointer rule)
{
	/* disconnect the "changed" signal */
	g_signal_handlers_disconnect_by_data (
		rule, g_object_get_data (rule, "editor-dlg"));
	g_object_set_data (rule, "editor-dlg", NULL);
	g_object_unref (rule);
}

/* adds a rule with a gui */
void
vfolder_gui_add_rule (EMVFolderRule *rule)
{
	GtkWidget *w;
	GtkDialog *gd;
	GtkWidget *container;

	w = e_filter_rule_get_widget ((EFilterRule *) rule, (ERuleContext *) context);

	gd = (GtkDialog *) gtk_dialog_new_with_buttons (
		_("New Search Folder"),
		NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK, NULL);

	gtk_dialog_set_default_response (gd, GTK_RESPONSE_OK);
	gtk_container_set_border_width (GTK_CONTAINER (gd), 6);

	container = gtk_dialog_get_content_area (gd);
	gtk_box_set_spacing (GTK_BOX (container), 6);

	g_object_set (gd, "resizable", TRUE, NULL);
	gtk_window_set_default_size (GTK_WINDOW (gd), 500, 500);
	gtk_box_pack_start (GTK_BOX (container), w, TRUE, TRUE, 0);
	gtk_widget_show ((GtkWidget *) gd);
	g_object_set_data (G_OBJECT (rule), "editor-dlg", gd);
	g_object_set_data_full (
		G_OBJECT (gd), "rule", rule,
		release_rule_notify_cb);
	g_signal_connect (
		rule, "changed",
		G_CALLBACK (new_rule_changed_cb), gd);
	new_rule_changed_cb ((EFilterRule *) rule, gd);
	g_signal_connect (
		gd, "response",
		G_CALLBACK (new_rule_clicked), NULL);
	gtk_widget_show ((GtkWidget *) gd);
}

void
vfolder_gui_add_from_message (EMailSession *session,
                              CamelMimeMessage *message,
                              gint flags,
                              CamelFolder *folder)
{
	EMVFolderRule *rule;

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	rule = (EMVFolderRule *) em_vfolder_rule_from_message (
		context, message, flags, folder);
	vfolder_gui_add_rule (rule);
}

void
vfolder_gui_add_from_address (EMailSession *session,
                              CamelInternetAddress *addr,
                              gint flags,
                              CamelFolder *folder)
{
	EMVFolderRule *rule;

	g_return_if_fail (addr != NULL);

	rule = (EMVFolderRule *) em_vfolder_rule_from_address (
		context, addr, flags, folder);
	vfolder_gui_add_rule (rule);
}
