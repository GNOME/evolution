/* pst-importer.c
 *
 * Author: Chris Halls <chris.halls@credativ.co.uk>
 *	  Bharath Acharya <abharath@novell.com>
 *
 * Copyright (C) 2006  Chris Halls
 *
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
 */

#include "evolution-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <libebook/libebook.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-sidebar.h>

#include <mail/e-mail-backend.h>
#include <mail/em-folder-selection-button.h>
#include <mail/em-utils.h>

#include <libpst/libpst.h>
#include <libpst/timeconv.h>

#ifdef WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#endif

typedef struct _PstImporter PstImporter;

gint pst_init (pst_file *pst, gchar *filename);
gchar *get_pst_rootname (pst_file *pst, gchar *filename);
static void pst_error_msg (const gchar *fmt, ...);
static void pst_import_folders (PstImporter *m, pst_desc_tree *topitem);
static void pst_process_item (PstImporter *m, pst_desc_tree *d_ptr, gchar **previouss_folder);
static void pst_process_folder (PstImporter *m, pst_item *item);
static void pst_process_email (PstImporter *m, pst_item *item);
static void pst_process_contact (PstImporter *m, pst_item *item);
static void pst_process_appointment (PstImporter *m, pst_item *item);
static void pst_process_task (PstImporter *m, pst_item *item);
static void pst_process_journal (PstImporter *m, pst_item *item);

static void pst_import_file (PstImporter *m);
gchar *foldername_to_utf8 (const gchar *pstname);
gchar *string_to_utf8 (const gchar *string);
void contact_set_date (EContact *contact, EContactField id, FILETIME *date);
static void fill_calcomponent (PstImporter *m, pst_item *item, ECalComponent *ec, const gchar *type);
ICalTime *get_ical_date (FILETIME *date, gboolean is_date);
gchar *rfc2445_datetime_format (FILETIME *ft);

gboolean org_credativ_evolution_readpst_supported (EPlugin *epl, EImportTarget *target);
GtkWidget *org_credativ_evolution_readpst_getwidget (EImport *ei, EImportTarget *target, EImportImporter *im);
void org_credativ_evolution_readpst_import (EImport *ei, EImportTarget *target, EImportImporter *im);
void org_credativ_evolution_readpst_cancel (EImport *ei, EImportTarget *target, EImportImporter *im);
gint e_plugin_lib_enable (EPlugin *ep, gint enable);

/* em-folder-selection-button.h is private, even though other internal evo plugins use it!
 * so declare the functions here
 * TODO: sort out whether this should really be private
*/

static guchar pst_signature[] = { '!', 'B', 'D', 'N' };

struct _PstImporter {
	MailMsg base;

	EImport *import;
	EImportTarget *target;

	gint waiting_open;
	GMutex status_lock;
	gchar *status_what;
	gint status_pc;
	gint status_timeout_id;
	GCancellable *cancellable;

	pst_file pst;

	CamelFolder *folder;
	gchar *folder_name;
	gchar *folder_uri;
	gint folder_count;
	gint current_item;

	EBookClient *addressbook;
	ECalClient *calendar;
	ECalClient *tasks;
	ECalClient *journal;

	/* progress indicator */
	gint position;
	gint total;
};

gboolean
org_credativ_evolution_readpst_supported (EPlugin *epl,
                                          EImportTarget *target)
{
	gchar signature[sizeof (pst_signature)];
	gboolean ret = FALSE;
	gint fd, n;
	EImportTargetURI *s;
	gchar *filename;

	if (target->type != E_IMPORT_TARGET_URI) {
		return FALSE;
	}

	s = (EImportTargetURI *) target;

	if (s->uri_src == NULL) {
		return TRUE;
	}

	if (strncmp (s->uri_src, "file:///", strlen ("file:///")) != 0) {
		return FALSE;
	}

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	fd = g_open (filename, O_RDONLY, 0);
	g_free (filename);

	if (fd != -1) {
		n = read (fd, signature, sizeof (pst_signature));
		ret = n == sizeof (pst_signature) && memcmp (signature, pst_signature, sizeof (pst_signature)) == 0;
		close (fd);
	}

	return ret;
}

static void
checkbox_mail_toggle_cb (GtkToggleButton *tb,
                         EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-mail", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
checkbox_addr_toggle_cb (GtkToggleButton *tb,
                         EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-addr", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
checkbox_appt_toggle_cb (GtkToggleButton *tb,
                         EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-appt", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
checkbox_task_toggle_cb (GtkToggleButton *tb,
                         EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-task", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
checkbox_journal_toggle_cb (GtkToggleButton *tb,
                            EImportTarget *target)
{
	g_datalist_set_data (&target->data, "pst-do-journal", GINT_TO_POINTER (gtk_toggle_button_get_active (tb)));
}

static void
folder_selected (EMFolderSelectionButton *button,
                 EImportTargetURI *target)
{
	g_free (target->uri_dest);
	target->uri_dest = g_strdup (em_folder_selection_button_get_folder_uri (button));
}

/**
 * Suggest a folder to import data into
 */
static gchar *
get_suggested_foldername (EImportTargetURI *target)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	GtkWindow *window;
	const gchar *inbox;
	gchar *delim, *filename;
	gchar *rootname = NULL;
	GString *foldername;
	pst_file pst;

	/* XXX Dig up the EMailSession from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	foldername = NULL;

	/* preselect the folder selected in a mail view */
	window = e_shell_get_active_window (shell);
	if (E_IS_SHELL_WINDOW (window)) {
		EShellWindow *shell_window;
		const gchar *view;

		shell_window = E_SHELL_WINDOW (window);
		view = e_shell_window_get_active_view (shell_window);

		if (view && g_str_equal (view, "mail")) {
			EShellView *shell_view;
			EShellSidebar *shell_sidebar;
			EMFolderTree *folder_tree = NULL;
			gchar *selected_uri;

			shell_view = e_shell_window_get_shell_view (
				shell_window, view);

			shell_sidebar =
				e_shell_view_get_shell_sidebar (shell_view);

			g_object_get (
				shell_sidebar, "folder-tree",
				&folder_tree, NULL);

			selected_uri = em_folder_tree_get_selected_uri (folder_tree);

			g_object_unref (folder_tree);

			if (selected_uri && *selected_uri)
				foldername = g_string_new (selected_uri);

			g_free (selected_uri);
		}
	}

	if (!foldername) {
		/* Suggest a folder that is in the same mail storage as the users' inbox,
		 * with a name derived from the .PST file */
		inbox =
			e_mail_session_get_local_folder_uri (
			session, E_MAIL_LOCAL_FOLDER_INBOX);

		delim = g_strrstr (inbox, "#");
		if (delim != NULL) {
			foldername = g_string_new_len (inbox, delim - inbox);
		} else {
			foldername = g_string_new (inbox);
		}
	}

	g_string_append_c (foldername, '/');

	filename = g_filename_from_uri (target->uri_src, NULL, NULL);

	if (pst_init (&pst, filename) == 0) {
		rootname = get_pst_rootname (&pst, filename);
	}

	g_free (filename);

	if (rootname != NULL) {
		gchar *utf8name;
		utf8name = foldername_to_utf8 (rootname);
		g_string_append (foldername, utf8name);
		g_free (utf8name);
		g_free (rootname);
	} else {
		g_string_append (foldername, "outlook_data");
	}

	/* FIXME Leaking a CamelFolder reference here. */
	/* FIXME Not passing a GCancellable or GError here. */
	if (e_mail_session_uri_to_folder_sync (
		session, foldername->str, 0, NULL, NULL) != NULL) {
		CamelFolder *folder;

		/* Folder exists - add a number */
		gint i, len;
		len = foldername->len;

		for (i = 1; i < 10000; i++) {
			g_string_truncate (foldername, len);
			g_string_append_printf (foldername, "_%d", i);
			/* FIXME Not passing a GCancellable or GError here. */
			if ((folder = e_mail_session_uri_to_folder_sync (
				session, foldername->str, 0, NULL, NULL)) == NULL) {
				/* Folder does not exist */
				break;
			}
		}

		if (folder != NULL) {
			pst_error_msg ("Error searching for an unused folder name. uri=%s", foldername->str);
		}
	}

	return g_string_free (foldername, FALSE);

}

static void
widget_sanitizer_cb (GtkToggleButton *button,
                     GtkWidget *source_combo)
{
	g_return_if_fail (button != NULL);
	g_return_if_fail (source_combo != NULL);

	gtk_widget_set_sensitive (source_combo, gtk_toggle_button_get_active (button));
}

static const gchar *
get_source_combo_key (const gchar *extension_name)
{
	if (g_strcmp0 (extension_name, E_SOURCE_EXTENSION_ADDRESS_BOOK) == 0)
		return "pst-contacts-source-combo";

	if (g_strcmp0 (extension_name, E_SOURCE_EXTENSION_CALENDAR) == 0)
		return "pst-events-source-combo";

	if (g_strcmp0 (extension_name, E_SOURCE_EXTENSION_TASK_LIST) == 0)
		return "pst-tasks-source-combo";

	if (g_strcmp0 (extension_name, E_SOURCE_EXTENSION_MEMO_LIST) == 0)
		return "pst-memos-source-combo";

	return NULL;
}

static void
add_source_list_with_check (GtkWidget *frame,
                            const gchar *caption,
                            EClientCache *client_cache,
                            const gchar *extension_name,
                            GCallback toggle_callback,
                            EImportTarget *target,
                            gboolean active)
{
	ESourceRegistry *registry;
	ESource *source = NULL;
	GtkWidget *check, *hbox;
	GtkWidget *combo = NULL;

	g_return_if_fail (frame != NULL);
	g_return_if_fail (caption != NULL);
	g_return_if_fail (toggle_callback != NULL);

	registry = e_client_cache_ref_registry (client_cache);
	source = e_source_registry_ref_default_for_extension_name (
		registry, extension_name);
	g_object_unref (registry);
	g_return_if_fail (source != NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

	check = gtk_check_button_new_with_mnemonic (caption);
	gtk_toggle_button_set_active ((GtkToggleButton *) check, active);
	g_signal_connect (check, "toggled", toggle_callback, target);
	gtk_box_pack_start ((GtkBox *) hbox, check, FALSE, FALSE, 0);

	combo = e_client_combo_box_new (client_cache, extension_name);
	e_source_combo_box_set_active (E_SOURCE_COMBO_BOX (combo), source);

	gtk_box_pack_end ((GtkBox *) hbox, combo, FALSE, FALSE, 0);

	g_signal_connect (
		check, "toggled",
		G_CALLBACK (widget_sanitizer_cb), combo);
	widget_sanitizer_cb (GTK_TOGGLE_BUTTON (check), combo);

	gtk_box_pack_start ((GtkBox *) frame, hbox, FALSE, FALSE, 0);

	if (combo) {
		const gchar *key = get_source_combo_key (extension_name);

		g_return_if_fail (key != NULL);

		g_datalist_set_data (&target->data, key, combo);
	}

	g_object_unref (source);
}

static void
pst_import_check_items (EImportTarget *target)
{
	gboolean has_mail = FALSE, has_addr = FALSE, has_appt = FALSE, has_task = FALSE, has_journal = FALSE;
	gchar *filename;
	pst_file pst;
	pst_item *item = NULL, *subitem;
	pst_desc_tree *d_ptr, *topitem;

	filename = g_filename_from_uri (((EImportTargetURI *) target)->uri_src, NULL, NULL);

	if (pst_init (&pst, filename) < 0) {
		goto end;
	}

	if ((item = pst_parse_item (&pst, pst.d_head, NULL)) == NULL) {
		goto end;
	}

	if ((topitem = pst_getTopOfFolders (&pst, item)) == NULL) {
		goto end;
	}

	d_ptr = topitem->child;

	/* Walk through folder tree */
	while (d_ptr != NULL && (!has_mail || !has_addr || !has_appt || !has_task || !has_journal)) {
		subitem = pst_parse_item (&pst, d_ptr, NULL);

		if (subitem != NULL &&
		    subitem->message_store == NULL &&
		    subitem->folder == NULL) {
			switch (subitem->type) {
			case PST_TYPE_CONTACT:
				if (subitem->contact)
					has_addr = TRUE;
				break;
			case PST_TYPE_APPOINTMENT:
				if (subitem->appointment)
					has_appt = TRUE;
				break;
			case PST_TYPE_TASK:
				if (subitem->appointment)
					has_task = TRUE;
				break;
			case PST_TYPE_JOURNAL:
				if (subitem->appointment)
					has_journal = TRUE;
				break;
			case PST_TYPE_NOTE:
			case PST_TYPE_SCHEDULE:
			case PST_TYPE_REPORT:
				if (subitem->email)
					has_mail = TRUE;
				break;
			}
		}

		pst_freeItem (subitem);

		if (d_ptr->child != NULL) {
			d_ptr = d_ptr->child;
		} else if (d_ptr->next != NULL) {
			d_ptr = d_ptr->next;
		} else {
			while (d_ptr != topitem && d_ptr->next == NULL) {
				d_ptr = d_ptr->parent;
			}

			if (d_ptr == topitem)
				break;

			d_ptr = d_ptr->next;
		}
	}

	pst_freeItem (item);

 end:
	g_free (filename);
	g_datalist_set_data (&target->data, "pst-do-mail", GINT_TO_POINTER (has_mail));
	g_datalist_set_data (&target->data, "pst-do-addr", GINT_TO_POINTER (has_addr));
	g_datalist_set_data (&target->data, "pst-do-appt", GINT_TO_POINTER (has_appt));
	g_datalist_set_data (&target->data, "pst-do-task", GINT_TO_POINTER (has_task));
	g_datalist_set_data (&target->data, "pst-do-journal", GINT_TO_POINTER (has_journal));
}

GtkWidget *
org_credativ_evolution_readpst_getwidget (EImport *ei,
                                          EImportTarget *target,
                                          EImportImporter *im)
{
	EShell *shell;
	EClientCache *client_cache;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	GtkWidget *hbox, *framebox, *w, *check;
	gchar *foldername;

	pst_import_check_items (target);

	framebox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

	/* Mail */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	check = gtk_check_button_new_with_mnemonic (_("_Mail"));
	gtk_toggle_button_set_active ((GtkToggleButton *) check, GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-mail")));
	g_signal_connect (
		check, "toggled",
		G_CALLBACK (checkbox_mail_toggle_cb), target);
	gtk_box_pack_start ((GtkBox *) hbox, check, FALSE, FALSE, 0);

	shell = e_shell_get_default ();
	client_cache = e_shell_get_client_cache (shell);
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	w = em_folder_selection_button_new (
		session, _("Select folder"),
		_("Select folder to import into"));
	foldername = get_suggested_foldername ((EImportTargetURI *) target);
	((EImportTargetURI *) target)->uri_dest = g_strdup (foldername);
	em_folder_selection_button_set_folder_uri ((EMFolderSelectionButton *) w, foldername);
	g_signal_connect (
		w, "selected",
		G_CALLBACK (folder_selected), target);
	gtk_box_pack_end ((GtkBox *) hbox, w, FALSE, FALSE, 0);
	g_signal_connect (
		check, "toggled",
		G_CALLBACK (widget_sanitizer_cb), w);
	widget_sanitizer_cb (GTK_TOGGLE_BUTTON (check), w);

	w = gtk_label_new (_("Destination folder:"));
	gtk_box_pack_end ((GtkBox *) hbox, w, FALSE, TRUE, 6);
	g_signal_connect (
		check, "toggled",
		G_CALLBACK (widget_sanitizer_cb), w);
	widget_sanitizer_cb (GTK_TOGGLE_BUTTON (check), w);

	gtk_box_pack_start ((GtkBox *) framebox, hbox, FALSE, FALSE, 0);

	add_source_list_with_check (
		framebox, _("_Address Book"),
		client_cache, E_SOURCE_EXTENSION_ADDRESS_BOOK,
		G_CALLBACK (checkbox_addr_toggle_cb), target,
		GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-addr")));
	add_source_list_with_check (
		framebox, _("A_ppointments"),
		client_cache, E_SOURCE_EXTENSION_CALENDAR,
		G_CALLBACK (checkbox_appt_toggle_cb), target,
		GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-appt")));
	add_source_list_with_check (
		framebox, _("_Tasks"),
		client_cache, E_SOURCE_EXTENSION_TASK_LIST,
		G_CALLBACK (checkbox_task_toggle_cb), target,
		GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-task")));
	add_source_list_with_check (
		framebox, _("_Journal entries"),
		client_cache, E_SOURCE_EXTENSION_MEMO_LIST,
		G_CALLBACK (checkbox_journal_toggle_cb), target,
		GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-journal")));

	gtk_widget_show_all (framebox);

	g_free (foldername);

	return framebox;
}

static void
pst_get_client_cb (GObject *source_object,
                   GAsyncResult *result,
                   gpointer user_data)
{
	PstImporter *m = user_data;
	EClient *client;
	GError *error = NULL;

	g_return_if_fail (result != NULL);
	g_return_if_fail (m != NULL);
	g_return_if_fail (m->waiting_open > 0);

	client = e_client_combo_box_get_client_finish (
		E_CLIENT_COMBO_BOX (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	if (E_IS_BOOK_CLIENT (client))
		m->addressbook = E_BOOK_CLIENT (client);

	if (E_IS_CAL_CLIENT (client)) {
		ECalClient *cal_client = E_CAL_CLIENT (client);

		switch (e_cal_client_get_source_type (cal_client)) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				m->calendar = cal_client;
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				m->tasks = cal_client;
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				m->journal = cal_client;
				break;
			default:
				g_warn_if_reached ();
				break;
		}
	}

	m->waiting_open--;
	if (!m->waiting_open)
		mail_msg_unordered_push (m);
}

static void
open_client (PstImporter *m,
             const gchar *extension_name)
{
	ESourceComboBox *combo_box;
	ESource *source;
	const gchar *key;

	key = get_source_combo_key (extension_name);
	combo_box = g_datalist_get_data (&m->target->data, key);
	g_return_if_fail (combo_box != NULL);

	source = e_source_combo_box_ref_active (combo_box);
	g_return_if_fail (source != NULL);

	m->waiting_open++;

	e_client_combo_box_get_client (
		E_CLIENT_COMBO_BOX (combo_box),
		source, m->cancellable,
		pst_get_client_cb, m);

	g_object_unref (source);
}

static void
pst_prepare_run (PstImporter *m)
{
	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-addr"))) {
		open_client (m, E_SOURCE_EXTENSION_ADDRESS_BOOK);
	}

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-appt"))) {
		open_client (m, E_SOURCE_EXTENSION_CALENDAR);
	}

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-task"))) {
		open_client (m, E_SOURCE_EXTENSION_TASK_LIST);
	}

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-journal"))) {
		open_client (m, E_SOURCE_EXTENSION_MEMO_LIST);
	}

	if (!m->waiting_open)
		mail_msg_unordered_push (m);
}

static gchar *
pst_import_describe (PstImporter *m,
                     gint complete)
{
	return g_strdup (_("Importing Outlook data"));
}

static void
pst_import_import (PstImporter *m,
                   GCancellable *cancellable,
                   GError **error)
{
	pst_import_file (m);
}

static void
count_items (PstImporter *m,
             pst_desc_tree *topitem)
{
	pst_desc_tree *d_ptr;

	m->position = 3;
	m->total = 5;
	d_ptr = topitem->child;

	/* Walk through folder tree */
	while (d_ptr != NULL) {
		m->total++;

		if (d_ptr->child != NULL) {
			d_ptr = d_ptr->child;
		} else if (d_ptr->next != NULL) {
			d_ptr = d_ptr->next;
		} else {
			while (d_ptr != topitem && d_ptr->next == NULL) {
				d_ptr = d_ptr->parent;
			}

			if (d_ptr == topitem)
				break;

			d_ptr = d_ptr->next;
		}
	}
}

static void
pst_import_file (PstImporter *m)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *session;
	gint ret;
	gchar *filename;
	pst_item *item = NULL;
	pst_desc_tree *d_ptr;

	/* XXX Dig up the EMailSession from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	filename = g_filename_from_uri (((EImportTargetURI *) m->target)->uri_src, NULL, NULL);
	m->folder_uri = g_strdup (((EImportTargetURI *) m->target)->uri_dest); /* Destination folder, was set in our widget */

	camel_operation_push_message (m->cancellable, _("Importing “%s”"), filename);

	if (GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-mail"))) {
		e_mail_session_uri_to_folder_sync (
			session, m->folder_uri, CAMEL_STORE_FOLDER_CREATE,
			m->cancellable, &m->base.error);
	}

	ret = pst_init (&m->pst, filename);

	if (ret < 0) {
		g_free (filename);
		camel_operation_pop_message (m->cancellable);
		return;
	}

	g_free (filename);

	camel_operation_progress (m->cancellable, 1);

	if ((item = pst_parse_item (&m->pst, m->pst.d_head, NULL)) == NULL) {
		pst_error_msg ("Could not get root record");
		return;
	}

	camel_operation_progress (m->cancellable, 2);

	if ((d_ptr = pst_getTopOfFolders (&m->pst, item)) == NULL) {
		pst_error_msg ("Top of folders record not found. Cannot continue");
		return;
	}

	camel_operation_progress (m->cancellable, 3);
	count_items (m, d_ptr);
	pst_import_folders (m, d_ptr);

	camel_operation_progress (m->cancellable, 100);

	camel_operation_pop_message (m->cancellable);

	pst_freeItem (item);

}

static void
pst_import_folders (PstImporter *m,
                    pst_desc_tree *topitem)
{
	GHashTable *node_to_folderuri; /* pointers of hierarchy nodes, to them associated folder uris */
	pst_desc_tree *d_ptr = NULL;

	node_to_folderuri = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

	if (topitem) {
		d_ptr = topitem->child;
		g_hash_table_insert (node_to_folderuri, topitem, g_strdup (m->folder_uri));
	}

	/* Walk through folder tree */
	while (d_ptr != NULL && (g_cancellable_is_cancelled (m->cancellable) == FALSE)) {
		gchar *previous_folder = NULL;

		m->position++;
		camel_operation_progress (m->cancellable, 100 * m->position / m->total);

		pst_process_item (m, d_ptr, &previous_folder);

		if (d_ptr->child != NULL) {
			g_clear_object (&m->folder);

			g_return_if_fail (m->folder_uri != NULL);
			g_hash_table_insert (node_to_folderuri, d_ptr, g_strdup (m->folder_uri));

			d_ptr = d_ptr->child;
		} else if (d_ptr->next != NULL) {
			/* for cases where there is an empty folder node, with no subnodes */
			if (previous_folder) {
				g_free (m->folder_uri);
				m->folder_uri = previous_folder;
				previous_folder = NULL;
			}

			d_ptr = d_ptr->next;
		} else {
			while (d_ptr && d_ptr != topitem && d_ptr->next == NULL) {
				g_clear_object (&m->folder);

				g_free (m->folder_uri);
				m->folder_uri = NULL;

				d_ptr = d_ptr->parent;

				if (d_ptr && d_ptr != topitem) {
					m->folder_uri = g_strdup (g_hash_table_lookup (node_to_folderuri, d_ptr->parent));
					g_return_if_fail (m->folder_uri != NULL);
				}
			}

			if (d_ptr == topitem) {
				g_free (previous_folder);
				break;
			}

			d_ptr = d_ptr ? d_ptr->next : NULL;
		}

		g_free (previous_folder);
	}

	g_hash_table_destroy (node_to_folderuri);
}

static void
pst_process_item (PstImporter *m,
                  pst_desc_tree *d_ptr,
                  gchar **previous_folder)
{
	pst_item *item = NULL;

	if (d_ptr->desc == NULL)
		return;

	item = pst_parse_item (&m->pst, d_ptr, NULL);

	if (item == NULL)
		return;

	if (item->message_store != NULL) {
		pst_error_msg ("A second message_store has been found - ignored");
		pst_freeItem (item);
		return;
	}

	if (item->folder != NULL) {
		if (previous_folder)
			*previous_folder = g_strdup (m->folder_uri);
		pst_process_folder (m, item);
	} else {
		switch (item->type) {
		case PST_TYPE_CONTACT:
			if (item->contact && m->addressbook && GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-addr")))
				pst_process_contact (m, item);
			break;
		case PST_TYPE_APPOINTMENT:
			if (item->appointment && m->calendar && GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-appt")))
				pst_process_appointment (m, item);
			break;
		case PST_TYPE_TASK:
			if (item->appointment && m->tasks && GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-task")))
				pst_process_task (m, item);
			break;
		case PST_TYPE_JOURNAL:
			if (item->appointment && m->journal && GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-journal")))
				pst_process_journal (m, item);
			break;
		case PST_TYPE_NOTE:
		case PST_TYPE_SCHEDULE:
		case PST_TYPE_REPORT:
			if (item->email && GPOINTER_TO_INT (g_datalist_get_data (&m->target->data, "pst-do-mail")))
				pst_process_email (m, item);
			break;
		}

		m->current_item++;
	}

	pst_freeItem (item);
}

/**
 * string_to_utf8:
 * @string: String from PST file
 *
 * Convert string to utf8. Currently we just use the locale, but maybe
 * there is encoding information hidden somewhere in the PST file?
 *
 * Returns: utf8 representation (caller should free), or NULL for error.
 */
gchar *
string_to_utf8 (const gchar *string)
{
	gchar *utf8;

	if (g_utf8_validate (string, -1, NULL))
		return g_strdup (string);

	utf8 = g_locale_to_utf8 (string, -1, NULL, NULL, NULL);

	return utf8;
}

/**
 * foldername_to_utf8:
 * @foldername: from PST file
 *
 * Convert foldername to utf8 and escape characters if needed
 *
 * Returns: converted folder name, or NULL for error. Caller should free
 */
gchar *
foldername_to_utf8 (const gchar *pstname)
{
	gchar *utf8name, *folder_name;

	utf8name = string_to_utf8 (pstname);

	if (utf8name == NULL) {
		folder_name = camel_url_encode (pstname, NULL);
		g_warning ("foldername_to_utf8: Cannot convert to utf8! foldername=%s", folder_name);
	} else {
		/* Encode using the current locale */
		folder_name = camel_url_encode (utf8name, NULL);
		g_free (utf8name);
	}

	g_strdelimit (folder_name, "/", '_');
	g_strescape (folder_name, NULL);

	return folder_name;
}

static void
pst_process_folder (PstImporter *m,
                    pst_item *item)
{
	gchar *uri;
	g_free (m->folder_name);

	if (item->file_as.str != NULL) {
		m->folder_name = foldername_to_utf8 (item->file_as.str);
	} else {
		g_critical ("Folder: No name! item->file_as=%s", item->file_as.str);
		m->folder_name = g_strdup ("unknown_name");
	}

	uri = g_strjoin ("/", m->folder_uri, m->folder_name, NULL);
	g_free (m->folder_uri);
	m->folder_uri = uri;

	g_clear_object (&m->folder);

	m->folder_count = item->folder->item_count;
	m->current_item = 0;
}

/**
 * pst_create_folder:
 * @m: PstImporter set to current folder
 *
 * Create current folder in mail hierarchy. Parent folders will also be
 * created.
 */
static void
pst_create_folder (PstImporter *m)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *session;
	const gchar *parent;
	gchar *dest, *dest_end, *pos;
	gint dest_len;

	/* XXX Dig up the EMailSession from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	parent = ((EImportTargetURI *) m->target)->uri_dest;
	dest = g_strdup (m->folder_uri);

	g_return_if_fail (g_str_has_prefix (dest, parent));

	g_clear_object (&m->folder);

	dest_len = strlen (dest);
	dest_end = dest + dest_len;

	pos = dest + strlen (parent);

	while (pos != NULL && pos < dest_end) {
		pos = g_strstr_len (pos + 1, dest_end - pos, "/");
		if (pos != NULL) {
			CamelFolder *folder;

			*pos = '\0';

			folder = e_mail_session_uri_to_folder_sync (
				session, dest, CAMEL_STORE_FOLDER_CREATE,
				m->cancellable, &m->base.error);
			if (folder)
				g_object_unref (folder);
			else
				break;
			*pos = '/';
		}
	}

	g_free (dest);

	if (!m->base.error)
		m->folder = e_mail_session_uri_to_folder_sync (
			session, m->folder_uri, CAMEL_STORE_FOLDER_CREATE,
			m->cancellable, &m->base.error);
}

/**
 * attachment_to_part:
 * @m: a #PstImporter
 * @attach: attachment to convert
 *
 * Create a #CamelMimePart from given PST attachment
 *
 * Returns: #CamelMimePart containing data and mime type
 */
static CamelMimePart *
attachment_to_part (PstImporter *m,
                    pst_item_attach *attach)
{
	CamelMimePart *part;
	const gchar *mimetype;

	part = camel_mime_part_new ();

	if (attach->filename2.str || attach->filename1.str) {
		camel_mime_part_set_filename (part, (attach->filename2.str ? attach->filename2.str : attach->filename1.str));
		camel_mime_part_set_disposition (part, "attachment");
		camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_BASE64);
	} else {
		camel_mime_part_set_disposition (part, "inline");
	}

	if (attach->mimetype.str != NULL) {
		mimetype = attach->mimetype.str;
	} else {
		mimetype = "application/octet-stream";
	}

	if (attach->data.data != NULL) {
		camel_mime_part_set_content (part, attach->data.data, attach->data.size, mimetype);
	} else {
		pst_binary attach_rc;
		attach_rc = pst_attach_to_mem (&m->pst, attach);

		camel_mime_part_set_content (part, (gchar *) attach_rc.data, attach_rc.size, mimetype);
		free (attach_rc.data);
	}

	return part;
}

static void
dequote_string (gchar *str)
{
	if (str[0] == '\'' || str[0] == '\"') {
		gint len = strlen (str);

		if (len > 1 && (str[len - 1] == '\'' || str[len - 1] == '\"')) {
			str[0] = ' ';
			str[len - 1] = ' ';
			g_strstrip (str);
		}
	}
}

static gboolean
lookup_address (pst_item *item,
                const gchar *str,
                gboolean is_unique,
                CamelAddress *addr)
{
	gboolean res = FALSE;
	gchar *address;

	if (!item || !str || !*str || !addr)
		return FALSE;

	address = g_strdup (str);
	dequote_string (address);

	if (item->contact && item->file_as.str &&
	    (is_unique || g_str_equal (item->file_as.str, str)) &&
	    item->contact->address1.str &&
	    item->contact->address1_transport.str &&
	    g_ascii_strcasecmp (item->contact->address1_transport.str, "SMTP") == 0 &&
	    !g_str_equal (address, item->contact->address1.str)) {
		gchar *tmp = address;

		address = g_strconcat ("\"", address, "\" <", item->contact->address1.str, ">", NULL);

		g_free (tmp);
	}

	res = camel_address_decode (addr, address) > 0;

	g_free (address);

	return res;
}

static const gchar *
strip_smtp (const gchar *str)
{
	if (str && g_ascii_strncasecmp (str, "SMTP:", 5) == 0)
		return str + 5;

	return str;
}

static void
pst_process_email (PstImporter *m,
                   pst_item *item)
{
	CamelMimeMessage *msg;
	CamelInternetAddress *addr;
	CamelMultipart *mp;
	CamelMimePart *part;
	CamelMessageInfo *info;
	pst_item_attach *attach;
	gboolean has_attachments;
	gchar *comp_str = NULL;
	gboolean success;

	if (m->folder == NULL) {
		pst_create_folder (m);
		if (!m->folder)
			return;
	}

	/* stops on the first valid attachment */
	for (attach = item->attach; attach; attach = attach->next) {
		if (attach->data.data || attach->i_id)
			break;
	}

	has_attachments = attach != NULL;

	if (item->type == PST_TYPE_SCHEDULE && item->appointment) {
		ECalComponent *comp;
		ICalComponent *vcal;
		ICalPropertyMethod method;

		comp = e_cal_component_new ();
		e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);
		fill_calcomponent (m, item, comp, "meeting-request");

		vcal = e_cal_util_new_top_level ();

		method = I_CAL_METHOD_PUBLISH;
		if (item->ascii_type) {
			if (g_str_has_prefix (item->ascii_type, "IPM.Schedule.Meeting.Request"))
				method = I_CAL_METHOD_REQUEST;
			else if (g_str_has_prefix (item->ascii_type, "IPM.Schedule.Meeting.Canceled"))
				method = I_CAL_METHOD_CANCEL;
			else if (g_str_has_prefix (item->ascii_type, "IPM.Schedule.Meeting.Resp."))
				method = I_CAL_METHOD_REPLY;
		}

		i_cal_component_set_method (vcal, method);

		i_cal_component_take_component (vcal, i_cal_component_clone (e_cal_component_get_icalcomponent (comp)));

		comp_str = i_cal_component_as_ical_string (vcal);

		g_object_unref (vcal);
		g_object_unref (comp);

		if (comp_str && !*comp_str) {
			g_free (comp_str);
			comp_str = NULL;
		}
	}

	camel_folder_freeze (m->folder);

	msg = camel_mime_message_new ();

	if (item->subject.str != NULL) {
		gchar *subj;

		subj = string_to_utf8 (item->subject.str);
		if (subj == NULL) {
			g_warning ("Could not convert email subject to utf8: %s", item->subject.str);
			camel_mime_message_set_subject (msg, "(lost subject)");
		} else {
			camel_mime_message_set_subject (msg, subj);
			g_free (subj);
		}
	}

	addr = camel_internet_address_new ();

	if (item->email->outlook_sender_name.str != NULL && item->email->outlook_sender.str != NULL) {
		camel_internet_address_add (addr, item->email->outlook_sender_name.str, strip_smtp (item->email->outlook_sender.str));
	} else if (item->email->outlook_sender_name.str != NULL) {
		camel_address_decode (CAMEL_ADDRESS (addr), strip_smtp (item->email->outlook_sender_name.str));
	} else if (item->email->outlook_sender.str != NULL) {
		camel_address_decode (CAMEL_ADDRESS (addr), strip_smtp (item->email->outlook_sender.str));
	} else {
		/* Evo prints a warning if no from is set, so supply an empty address */
		camel_internet_address_add (addr, "", "");
	}

	camel_mime_message_set_from (msg, addr);
	g_object_unref (addr);

	if (item->email->sent_date != NULL) {
		camel_mime_message_set_date (msg, pst_fileTimeToUnixTime (item->email->sent_date), 0);
	}

	if (item->email->messageid.str != NULL) {
		camel_mime_message_set_message_id (msg, item->email->messageid.str);
	}

	if (item->email->header.str != NULL) {
		/* Use mime parser to read headers */
		CamelStream *stream;
		/*g_debug ("  Email headers length=%zd", strlen (item->email->header));*/
		/*g_message ("  Email headers... %s...", item->email->header);*/

		stream = camel_stream_mem_new_with_buffer (item->email->header.str, strlen (item->email->header.str));
		if (!camel_data_wrapper_construct_from_stream_sync ((CamelDataWrapper *) msg, stream, NULL, NULL))
			g_warning ("Error reading headers, skipped");

	} else {

		if (item->email->sentto_address.str != NULL) {
			addr = camel_internet_address_new ();

			if (lookup_address (item, item->email->sentto_address.str, item->email->cc_address.str == NULL, CAMEL_ADDRESS (addr)))
				camel_mime_message_set_recipients (msg, "To", addr);

			g_object_unref (addr);
		}

		if (item->email->cc_address.str != NULL) {
			addr = camel_internet_address_new ();

			if (lookup_address (item, item->email->cc_address.str, item->email->sentto_address.str == NULL, CAMEL_ADDRESS (addr)))
				camel_mime_message_set_recipients (msg, "CC", addr);

			g_object_unref (addr);
		}
	}

	mp = camel_multipart_new ();

	if (has_attachments) {

		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (mp), "multipart/mixed");

	} else if (item->email->htmlbody.str && item->body.str) {

		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (mp), "multipart/alternative");

	} else if (item->email->htmlbody.str) {

		camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (mp), "text/html");

	}

	camel_multipart_set_boundary (mp, NULL);

	if (item->body.str != NULL) {
		/* Read internet headers */

		/*g_debug ("  Email body length=%zd", strlen (item->email->body));
		g_message ("  Email body %100s...", item->email->body);*/

		part = camel_mime_part_new ();
		camel_mime_part_set_content (part, item->body.str, strlen (item->body.str), "text/plain");
		camel_multipart_add_part (mp, part);
		g_object_unref (part);
	}

	if (item->email->htmlbody.str != NULL) {
		/*g_debug ("  HTML body length=%zd", strlen (item->email->htmlbody));*/
		part = camel_mime_part_new ();
		camel_mime_part_set_content (part, item->email->htmlbody.str, strlen (item->email->htmlbody.str), "text/html");
		camel_multipart_add_part (mp, part);
		g_object_unref (part);
	}

	if (comp_str) {
		part = camel_mime_part_new ();
		camel_mime_part_set_content (part, comp_str, strlen (comp_str), "text/calendar");
		camel_multipart_add_part (mp, part);
		g_object_unref (part);
	}

	for (attach = item->attach; attach; attach = attach->next) {
		if (attach->data.data || attach->i_id) {
			part = attachment_to_part (m, attach);
			camel_multipart_add_part (mp, part);
			g_object_unref (part);
		}
	}

	/*camel_mime_message_dump (msg, TRUE);*/

	if (item->email->htmlbody.str || item->attach) {
		camel_medium_set_content (CAMEL_MEDIUM (msg), CAMEL_DATA_WRAPPER (mp));
	} else if (item->body.str) {
		camel_mime_part_set_content (CAMEL_MIME_PART (msg), item->body.str, strlen (item->body.str), "text/plain");
	} else {
		g_warning (
			"Email without body. Subject:%s",
			(item->subject.str ? item->subject.str : "(empty)"));
		camel_mime_part_set_content (CAMEL_MIME_PART (msg), "\n", 1, "text/plain");
	}

	info = camel_message_info_new (NULL);

	/* Read message flags (see comments in libpst.c */
	if (item->flags & 0x01)
		camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, ~0);

	if (item->email->importance == 2)
		camel_message_info_set_flags (info, CAMEL_MESSAGE_FLAGGED, ~0);

	if (item->flags & 0x08)
		camel_message_info_set_flags (info, CAMEL_MESSAGE_DRAFT, ~0);

	/* FIXME Not passing a GCancellable or GError here. */
	success = camel_folder_append_message_sync (
		m->folder, msg, info, NULL, NULL, NULL);
	g_clear_object (&info);
	g_object_unref (msg);

	/* FIXME Not passing a GCancellable or GError here. */
	camel_folder_synchronize_sync (m->folder, FALSE, NULL, NULL);
	camel_folder_thaw (m->folder);

	g_free (comp_str);

	if (!success) {
		g_debug ("%s: Exception!", G_STRFUNC);
		return;
	}

}

static void
contact_set_string (EContact *contact,
                    EContactField id,
                    gchar *string)
{
	if (string != NULL) {
		e_contact_set (contact, id, string);
	}
}

static void
unknown_field (EContact *contact,
               GString *notes,
               const gchar *name,
               gchar *string)
{
	/* Field could not be mapped directly so add to notes field */
	if (string != NULL) {
		g_string_append_printf (notes, "%s: %s\n", name, string);
	}
}

static void
contact_set_address (EContact *contact,
                     EContactField id,
                     gchar *address,
                     gchar *city,
                     gchar *country,
                     gchar *po_box,
                     gchar *postal_code,
                     gchar *state,
                     gchar *street)
{
	EContactAddress *eaddress;

	if (address || city || country || po_box || postal_code || state || street) {
		eaddress = e_contact_address_new ();
		if (po_box) {
			eaddress->po = g_strdup (po_box);
		}
		/* eaddress->ext = */

		if (street) {
			eaddress->street = g_strdup (street);
		}

		if (city) {
			eaddress->locality = g_strdup (city);
		}

		if (state) {
			eaddress->region = g_strdup (state);
		}

		if (postal_code) {
			eaddress->code = g_strdup (postal_code);
		}

		if (country) {
			eaddress->country = g_strdup (country);
		}

		e_contact_set (contact, id, eaddress);
		e_contact_address_free (eaddress);
	}
}

void
contact_set_date (EContact *contact,
                  EContactField id,
                  FILETIME *date)
{
	if (date && (date->dwLowDateTime || date->dwHighDateTime)) {
		time_t t1;
		struct tm tm;
		EContactDate *bday;
		bday = e_contact_date_new ();

		t1 = pst_fileTimeToUnixTime (date);
		gmtime_r (&t1, &tm);

		bday->year = tm.tm_year + 1900;
		bday->month = tm.tm_mon + 1;
		bday->day = tm.tm_mday;

		e_contact_set (contact, id, bday);
	}
}

static void
pst_process_contact (PstImporter *m,
                     pst_item *item)
{
	pst_item_contact *c;
	EContact *ec;
	GString *notes;
	GError *error = NULL;

	c = item->contact;
	notes = g_string_sized_new (2048);

	ec = e_contact_new ();
	/* pst's fullname field only contains first, middle, surname */
	if (c->display_name_prefix.str || c->suffix.str) {
		GString *name = g_string_sized_new (128);

		if (c->display_name_prefix.str) {
			g_string_assign (name, c->display_name_prefix.str);
		}

		if (c->first_name.str) {
			g_string_append_printf (name, "%s%s", (name->len ? " " : ""), c->first_name.str);
		}

		if (c->middle_name.str) {
			g_string_append_printf (name, "%s%s", (name->len ? " " : ""), c->middle_name.str);
		}

		if (c->surname.str) {
			g_string_append_printf (name, "%s%s", (name->len ? " " : ""), c->surname.str);
		}

		if (c->surname.str) {
			g_string_append_printf (name, "%s%s", (name->len ? " " : ""), c->surname.str);
		}

		contact_set_string (ec, E_CONTACT_FULL_NAME, name->str);
		g_string_free (name, TRUE);

	} else {
		contact_set_string (ec, E_CONTACT_FULL_NAME, c->fullname.str);
	}

	/* unknown_field (ec, notes, "initials", c->initials); */

	contact_set_string (ec, E_CONTACT_NICKNAME, c->nickname.str);

	contact_set_string (ec, E_CONTACT_ORG, c->company_name.str);
	contact_set_string (ec, E_CONTACT_ORG_UNIT, c->department.str);
	contact_set_string (ec, E_CONTACT_TITLE, c->job_title.str);

	contact_set_address (
		ec,E_CONTACT_ADDRESS_WORK,
		c->business_address.str, c->business_city.str, c->business_country.str,
		c->business_po_box.str, c->business_postal_code.str, c->business_state.str, c->business_street.str);

	contact_set_address (
		ec,E_CONTACT_ADDRESS_HOME,
		c->home_address.str, c->home_city.str, c->home_country.str,
		c->home_po_box.str, c->home_postal_code.str, c->home_state.str, c->home_street.str);

	contact_set_address (
		ec,E_CONTACT_ADDRESS_OTHER,
		c->other_address.str, c->other_city.str, c->other_country.str,
		c->other_po_box.str, c->other_postal_code.str, c->other_state.str, c->other_street.str);

	contact_set_string (ec, E_CONTACT_PHONE_ASSISTANT, c->assistant_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_BUSINESS_FAX, c->business_fax.str);
	contact_set_string (ec, E_CONTACT_PHONE_BUSINESS, c->business_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_BUSINESS_2, c->business_phone2.str);
	contact_set_string (ec, E_CONTACT_PHONE_CALLBACK, c->callback_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_CAR, c->car_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_COMPANY, c->company_main_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_HOME_FAX, c->home_fax.str);
	contact_set_string (ec, E_CONTACT_PHONE_HOME, c->home_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_HOME_2, c->home_phone2.str);
	contact_set_string (ec, E_CONTACT_PHONE_ISDN, c->isdn_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_MOBILE, c->mobile_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_OTHER_FAX, c->primary_fax.str);  /* ? */
	contact_set_string (ec, E_CONTACT_PHONE_PAGER, c->pager_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_PRIMARY, c->primary_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_RADIO, c->radio_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_TTYTDD, c->ttytdd_phone.str);
	contact_set_string (ec, E_CONTACT_PHONE_TELEX, c->telex.str);
	unknown_field (ec, notes, "account_name", c->account_name.str);
	contact_set_date (ec, E_CONTACT_ANNIVERSARY, c->wedding_anniversary);
	contact_set_string (ec, E_CONTACT_ASSISTANT, c->assistant_name.str);
	unknown_field (ec, notes, "billing_information", c->billing_information.str);
	contact_set_date (ec, E_CONTACT_BIRTH_DATE, c->birthday);
	/* contact_set_string (ec, E_CONTACT_CATEGORIES, c->??); */

	contact_set_string (ec, E_CONTACT_EMAIL_1 , c->address1.str);
	contact_set_string (ec, E_CONTACT_EMAIL_2 , c->address2.str);
	contact_set_string (ec, E_CONTACT_EMAIL_3 , c->address3.str);

	/*unknown_field (ec, notes, "address1_desc" , c->address1_desc);
	unknown_field (ec, notes, "address1_transport" , c->address1_transport);
	unknown_field (ec, notes, "address2_desc" , c->address2_desc);
	unknown_field (ec, notes, "address2_transport" , c->address2_transport);
	unknown_field (ec, notes, "address3_desc" , c->address3_desc);
	unknown_field (ec, notes, "address3_transport" , c->address3_transport);*/

	/*unknown_field (ec, notes, "def_postal_address", c->def_postal_address);*/

	/* unknown_field (ec, ??, c->gender); */
	unknown_field (ec, notes, "gov_id", c->gov_id.str);
	unknown_field (ec, notes, "customer_id", c->customer_id.str);
	unknown_field (ec, notes, "hobbies", c->hobbies.str);
	unknown_field (ec, notes, "followup", c->followup.str);

	contact_set_string (ec, E_CONTACT_FREEBUSY_URL , c->free_busy_address.str);

	unknown_field (ec, notes, "keyword", c->keyword.str);
	unknown_field (ec, notes, "language", c->language.str);
	unknown_field (ec, notes, "location", c->location.str);
	contact_set_string (ec, E_CONTACT_OFFICE, c->office_loc.str);
	unknown_field (ec, notes, "computer_name", c->computer_name.str);
	unknown_field (ec, notes, "ftp_site", c->ftp_site.str);

	contact_set_string (ec, E_CONTACT_MANAGER , c->manager_name.str);
	unknown_field (ec, notes, "mileage", c->mileage.str);
	unknown_field (ec, notes, "org_id", c->org_id.str);
	contact_set_string (ec, E_CONTACT_ROLE, c->profession.str);

	contact_set_string (ec, E_CONTACT_SPOUSE , c->spouse_name.str);

	if (c->personal_homepage.str) {
		contact_set_string (ec, E_CONTACT_HOMEPAGE_URL , c->personal_homepage.str);
		if (c->business_homepage.str) {
			unknown_field (ec, notes, "business_homepage", c->business_homepage.str);
		}
	} else if (c->business_homepage.str) {
		contact_set_string (ec, E_CONTACT_HOMEPAGE_URL , c->business_homepage.str);
	}

	if (item->comment.str) {
		g_string_append_printf (notes, "%s\n", item->comment.str);
	}

	if (item->email && item->body.str) {
		g_string_append_printf (notes, "%s\n", item->body.str);
	}

	contact_set_string (ec, E_CONTACT_NOTE, notes->str);
	g_string_free (notes, TRUE);

	e_book_client_add_contact_sync (
		m->addressbook, ec, E_BOOK_OPERATION_FLAG_NONE, NULL, NULL, &error);

	g_object_unref (ec);

	if (error != NULL) {
		g_warning (
			"%s: Failed to add contact: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}
}

/**
 * get_ical_date:
 * @date: time value from libpst
 * @is_date: treat as date only (all day event)?
 *
 * Convert pst time to ICalTime
 *
 * Returns: converted date
 */
ICalTime *
get_ical_date (FILETIME *date,
               gboolean is_date)
{
	if (date && (date->dwLowDateTime || date->dwHighDateTime)) {
		time_t t;

		t = pst_fileTimeToUnixTime (date);
		return i_cal_time_new_from_timet_with_zone (t, is_date, NULL);
	} else {
		return NULL;
	}
}

static void
set_cal_attachments (ECalClient *cal,
                     ECalComponent *ec,
                     PstImporter *m,
                     pst_item_attach *attach)
{
	GSList *list = NULL;
	const gchar *uid;
	gchar *store_dir;

	if (attach == NULL) {
		return;
	}

	uid = e_cal_component_get_uid (ec);
	store_dir = g_filename_from_uri (e_cal_client_get_local_attachment_store (cal), NULL, NULL);

	while (attach != NULL) {
		const gchar * orig_filename;
		gchar *filename, *tmp, *path, *dirname, *uri;
		CamelMimePart *part;
		CamelDataWrapper *content;
		CamelStream *stream;
		struct stat st;

		part = attachment_to_part (m, attach);

		orig_filename = camel_mime_part_get_filename (part);

		if (orig_filename == NULL) {
			g_warning ("Ignoring unnamed attachment");
			attach = attach->next;
			continue;  /* Ignore unnamed attachments */
		}

		tmp = camel_file_util_safe_filename (orig_filename);
		filename = g_strdup_printf ("%s-%s", uid, tmp);
		path = g_build_filename (store_dir, filename, NULL);

		g_free (tmp);
		g_free (filename);

		dirname = g_path_get_dirname (path);
		if (g_mkdir_with_parents (dirname, 0777) == -1) {
			g_warning ("Could not create directory %s: %s", dirname, g_strerror (errno));
			g_free (dirname);
			attach = attach->next;
			continue;
		}
		g_free (dirname);

		if (g_access (path, F_OK) == 0) {
			if (g_access (path, W_OK) != 0) {
				g_warning ("Could not write file %s - file exists", path);
				attach = attach->next;
				continue;
			}
		}

		if (g_stat (path, &st) != -1 && !S_ISREG (st.st_mode)) {
			g_warning ("Could not write file %s - not a file", path);
			attach = attach->next;
			continue;
		}

		if (!(stream = camel_stream_fs_new_with_name (path, O_WRONLY | O_CREAT | O_TRUNC, 0666, NULL))) {
			g_warning ("Could not create stream for file %s - %s", path, g_strerror (errno));
			attach = attach->next;
			continue;
		}

		content = camel_medium_get_content (CAMEL_MEDIUM (part));

		if (camel_data_wrapper_decode_to_stream_sync (content, stream, NULL, NULL) == -1
			|| camel_stream_flush (stream, NULL, NULL) == -1)
		{
			g_warning ("Could not write attachment to %s: %s", path, g_strerror (errno));
			g_object_unref (stream);
			attach = attach->next;
			continue;
		}

		g_object_unref (stream);

		uri = g_filename_to_uri (path, NULL, NULL);
		list = g_slist_append (list, i_cal_attach_new_from_url (uri));
		g_free (uri);

		g_object_unref (part);
		g_free (path);

		attach = attach->next;

	}

	g_free (store_dir);

	e_cal_component_set_attachments (ec, list);
	g_slist_free_full (list, g_object_unref);
}

static void
fill_calcomponent (PstImporter *m,
                   pst_item *item,
                   ECalComponent *ec,
                   const gchar *type)
{
	pst_item_appointment *a;
	pst_item_email *e;
	ECalComponentText *text;

	a = item->appointment;
	e = item->email;

	g_return_if_fail (item->appointment != NULL);

	if (item->create_date) {
		ICalTime *tt;
		tt = get_ical_date (item->create_date, FALSE);
		e_cal_component_set_created (ec, tt);
		g_clear_object (&tt);
	}
	if (item->modify_date) {
		ICalTime *tt;
		tt = get_ical_date (item->modify_date, FALSE);
		e_cal_component_set_last_modified (ec, tt);
		g_clear_object (&tt);
	}

	if (e) {
		if (item->subject.str || e->processed_subject.str) {
			text = NULL;
			if (item->subject.str) {
				text = e_cal_component_text_new (item->subject.str, NULL);
			} else if (e->processed_subject.str) {
				text = e_cal_component_text_new (e->processed_subject.str, NULL);
			}

			e_cal_component_set_summary (ec, text);
			e_cal_component_text_free (text);
		}
		if (item->body.str) {
			GSList l;
			text = e_cal_component_text_new (item->body.str, NULL);
			l.data = text;
			l.next = NULL;
			e_cal_component_set_descriptions (ec, &l);
			e_cal_component_text_free (text);
		}
	} else {
		g_warning ("%s without subject / body!", type);
	}

	if (a->location.str) {
		e_cal_component_set_location (ec, a->location.str);
	}

	if (a->start) {
		ECalComponentDateTime *dtstart;

		dtstart = e_cal_component_datetime_new_take (
			get_ical_date (a->start, a->all_day),
			g_strdup (a->timezonestring.str));
		e_cal_component_set_dtstart (ec, dtstart);
		e_cal_component_datetime_free (dtstart);
	}

	if (a->end) {
		ECalComponentDateTime *dtend;

		dtend = e_cal_component_datetime_new_take (
			get_ical_date (a->end, a->all_day),
			g_strdup (a->timezonestring.str));
		e_cal_component_set_dtend (ec, dtend);
		e_cal_component_datetime_free (dtend);
	}

	switch (a->showas) {
		case PST_FREEBUSY_TENTATIVE:
			e_cal_component_set_status (ec, I_CAL_STATUS_TENTATIVE);
			break;
		case PST_FREEBUSY_FREE:
			/* mark as transparent and as confirmed */
			e_cal_component_set_transparency (ec, E_CAL_COMPONENT_TRANSP_TRANSPARENT);
			e_cal_component_set_status (ec, I_CAL_STATUS_CONFIRMED);
			break;
		case PST_FREEBUSY_BUSY:
		case PST_FREEBUSY_OUT_OF_OFFICE:
			e_cal_component_set_status (ec, I_CAL_STATUS_CONFIRMED);
			break;
	}
	switch (a->label) {
		case PST_APP_LABEL_NONE:
			break;
		case PST_APP_LABEL_IMPORTANT:
			e_cal_component_set_categories (ec, "Important"); break;
		case PST_APP_LABEL_BUSINESS:
			e_cal_component_set_categories (ec, "Business"); break;
		case PST_APP_LABEL_PERSONAL:
			e_cal_component_set_categories (ec, "Personal"); break;
		case PST_APP_LABEL_VACATION:
			e_cal_component_set_categories (ec, "Vacation"); break;
		case PST_APP_LABEL_MUST_ATTEND:
			e_cal_component_set_categories (ec, "Must-attend"); break;
		case PST_APP_LABEL_TRAVEL_REQ:
			e_cal_component_set_categories (ec, "Travel-required"); break;
		case PST_APP_LABEL_NEEDS_PREP:
			e_cal_component_set_categories (ec, "Needs-preparation"); break;
		case PST_APP_LABEL_BIRTHDAY:
			e_cal_component_set_categories (ec, "Birthday"); break;
		case PST_APP_LABEL_ANNIVERSARY:
			e_cal_component_set_categories (ec, "Anniversary"); break;
		case PST_APP_LABEL_PHONE_CALL:
			e_cal_component_set_categories (ec, "Phone-call"); break;
	}

	if (a->alarm || a->alarm_minutes) {
		ECalComponentAlarm *alarm;

		alarm = e_cal_component_alarm_new ();

		if (a->alarm_minutes) {
			ECalComponentAlarmTrigger *trigger = NULL;
			ICalDuration *duration;

			duration = i_cal_duration_new_from_int (- (a->alarm_minutes) * 60);
			trigger = e_cal_component_alarm_trigger_new_relative (E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, duration);
			e_cal_component_alarm_take_trigger (alarm, trigger);
			g_object_unref (duration);
		}

		if (a->alarm) {
			if (a->alarm_filename.str) {
				e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_AUDIO);
			} else {
				e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);
			}
		}

		e_cal_component_add_alarm (ec, alarm);
		e_cal_component_alarm_free (alarm);

	}

	if (a->recurrence_description.str != PST_APP_RECUR_NONE) {
		ICalRecurrence *recr;
		GSList recur_list;

		recr = i_cal_recurrence_new ();

		i_cal_recurrence_set_interval (recr, 1); /* Interval not implemented in libpst */
		if (a->recurrence_end) {
			ICalTime *tt;

			tt = get_ical_date (a->recurrence_end, FALSE);
			if (tt) {
				i_cal_recurrence_set_until (recr, tt);
				g_object_unref (tt);
			}
		}

		switch (a->recurrence_type) {
			case PST_APP_RECUR_DAILY:
				i_cal_recurrence_set_freq (recr, I_CAL_DAILY_RECURRENCE);
				break;
			case PST_APP_RECUR_WEEKLY:
				i_cal_recurrence_set_freq (recr, I_CAL_WEEKLY_RECURRENCE);
				break;
			case PST_APP_RECUR_MONTHLY:
				i_cal_recurrence_set_freq (recr, I_CAL_MONTHLY_RECURRENCE);
				break;
			case PST_APP_RECUR_YEARLY:
				i_cal_recurrence_set_freq (recr, I_CAL_YEARLY_RECURRENCE);
				break;
			default:
				i_cal_recurrence_set_freq (recr, I_CAL_NO_RECURRENCE);
				break;
		}

		recur_list.data = recr;
		recur_list.next = NULL;
		e_cal_component_set_rrules (ec, &recur_list);
		g_object_unref (recr);
	}

	if (item->type == PST_TYPE_SCHEDULE && item->email && item->ascii_type) {
		const gchar *organizer, *organizer_addr, *attendee, *attendee_addr;

		if (g_str_has_prefix (item->ascii_type, "IPM.Schedule.Meeting.Resp.")) {
			organizer = item->email->outlook_recipient_name.str;
			organizer_addr = item->email->outlook_recipient.str;
			attendee = item->email->outlook_sender_name.str;
			attendee_addr = item->email->outlook_sender.str;
		} else {
			organizer = item->email->outlook_sender_name.str;
			organizer_addr = item->email->outlook_sender.str;
			attendee = item->email->outlook_recipient_name.str;
			attendee_addr = item->email->outlook_recipient.str;
		}

		if (organizer || organizer_addr) {
			ECalComponentOrganizer *org;

			org = e_cal_component_organizer_new ();
			e_cal_component_organizer_set_value (org, organizer_addr);
			e_cal_component_organizer_set_cn (org, organizer);

			e_cal_component_set_organizer (ec, org);
			e_cal_component_organizer_free (org);
		}

		if (attendee || attendee_addr) {
			ECalComponentAttendee *att;
			GSList *attendees;

			att = e_cal_component_attendee_new ();
			e_cal_component_attendee_set_value (att, attendee_addr);
			e_cal_component_attendee_set_cn (att, attendee);
			e_cal_component_attendee_set_cutype (att, I_CAL_CUTYPE_INDIVIDUAL);
			e_cal_component_attendee_set_partstat (att, I_CAL_PARTSTAT_NEEDSACTION);
			e_cal_component_attendee_set_role (att, I_CAL_ROLE_REQPARTICIPANT);
			e_cal_component_attendee_set_rsvp (att, TRUE);

			attendees = g_slist_append (NULL, att);
			e_cal_component_set_attendees (ec, attendees);
			g_slist_free_full (attendees, e_cal_component_attendee_free);
		}
	}

	e_cal_component_commit_sequence	 (ec);
}

static void
pst_process_component (PstImporter *m,
                       pst_item *item,
                       const gchar *comp_type,
                       ECalComponentVType vtype,
                       ECalClient *cal)
{
	ECalComponent *ec;
	GError *error = NULL;

	g_return_if_fail (item->appointment != NULL);

	ec = e_cal_component_new ();
	e_cal_component_set_new_vtype (ec, vtype);

	fill_calcomponent (m, item, ec, comp_type);
	set_cal_attachments (cal, ec, m, item->attach);

	e_cal_client_create_object_sync (
		cal, e_cal_component_get_icalcomponent (ec),
		E_CAL_OPERATION_FLAG_NONE, NULL, NULL, &error);

	if (error != NULL) {
		g_warning (
			"Creation of %s failed: %s",
			comp_type, error->message);
		g_error_free (error);
	}

	g_object_unref (ec);
}

static void
pst_process_appointment (PstImporter *m,
                         pst_item *item)
{
	pst_process_component (m, item, "appointment", E_CAL_COMPONENT_EVENT, m->calendar);
}

static void
pst_process_task (PstImporter *m,
                  pst_item *item)
{
	pst_process_component (m, item, "task", E_CAL_COMPONENT_TODO, m->tasks);
}

static void
pst_process_journal (PstImporter *m,
                     pst_item *item)
{
	pst_process_component (m, item, "journal", E_CAL_COMPONENT_JOURNAL, m->journal);
}

/* Print an error message - maybe later bring up an error dialog? */
static void
pst_error_msg (const gchar *fmt,
               ...)
{
	va_list ap;

	va_start (ap, fmt);
	g_critical (fmt, ap);
	va_end (ap);
}

static void
pst_import_imported (PstImporter *m)
{
	e_import_complete (m->target->import, (EImportTarget *) m->target, m->base.error);
}

static void
pst_import_free (PstImporter *m)
{
	/* pst_close (&m->pst); */
	if (m->addressbook)
		g_object_unref (m->addressbook);
	if (m->calendar)
		g_object_unref (m->calendar);
	if (m->tasks)
		g_object_unref (m->tasks);
	if (m->journal)
		g_object_unref (m->journal);

	g_object_unref (m->cancellable);

	g_free (m->status_what);
	g_mutex_clear (&m->status_lock);

	g_source_remove (m->status_timeout_id);
	m->status_timeout_id = 0;

	g_free (m->folder_name);
	g_free (m->folder_uri);

	g_object_unref (m->import);
}

static MailMsgInfo pst_import_info = {
	sizeof (PstImporter),
	(MailMsgDescFunc) pst_import_describe,
	(MailMsgExecFunc) pst_import_import,
	(MailMsgDoneFunc) pst_import_imported,
	(MailMsgFreeFunc) pst_import_free,
};

static gboolean
pst_status_timeout (gpointer data)
{
	PstImporter *importer = data;
	gint pc;
	gchar *what;

	if (importer->status_what) {
		g_mutex_lock (&importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock (&importer->status_lock);

		e_import_status (importer->target->import, (EImportTarget *) importer->target, what, pc);
	}

	return TRUE;
}

static void
pst_status (CamelOperation *op,
            const gchar *what,
            gint pc,
            gpointer data)
{
	PstImporter *importer = data;

	g_mutex_lock (&importer->status_lock);
	g_free (importer->status_what);
	importer->status_what = g_strdup (what);
	importer->status_pc = pc;
	g_mutex_unlock (&importer->status_lock);
}

static void
pst_import (EImport *ei,
            EImportTarget *target)
{
	PstImporter *m;

	m = mail_msg_new (&pst_import_info);
	g_datalist_set_data (&target->data, "pst-msg", m);
	m->import = ei;
	g_object_ref (m->import);
	m->target = target;

	m->folder_name = NULL;
	m->folder_uri = NULL;

	m->addressbook = NULL;
	m->calendar = NULL;
	m->tasks = NULL;
	m->journal = NULL;
	m->waiting_open = 0;

	m->status_timeout_id =
		e_named_timeout_add (100, pst_status_timeout, m);
	g_mutex_init (&m->status_lock);
	m->cancellable = camel_operation_new ();

	g_signal_connect (
		m->cancellable, "status",
		G_CALLBACK (pst_status), m);

	pst_prepare_run (m);
}

/* Start the main import operation */
void
org_credativ_evolution_readpst_import (EImport *ei,
                                       EImportTarget *target,
                                       EImportImporter *im)
{
	if (GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-mail"))
	    || GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-addr"))
	    || GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-appt"))
	    || GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-task"))
	    || GPOINTER_TO_INT (g_datalist_get_data (&target->data, "pst-do-journal"))) {
				pst_import (ei, target);
	} else {
		e_import_complete (target->import, target, NULL);
	}
}

void
org_credativ_evolution_readpst_cancel (EImport *ei,
                                       EImportTarget *target,
                                       EImportImporter *im)
{
	PstImporter *m = g_datalist_get_data (&target->data, "pst-msg");

	if (m) {
		g_cancellable_cancel (m->cancellable);
	}
}

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}

/**
 * pst_init:
 * @pst: pst_file structure to be used by libpst
 * @filename: path to file
 *
 * Open PST file and determine root folder name
 *
 * Returns: 0 for success, -1 for failure
 */
gint
pst_init (pst_file *pst,
          gchar *filename)
{

#if 0
	gchar *d_log = "readpst.log";
	/* initialize log file */
	DEBUG_INIT (d_log);
	DEBUG_REGISTER_CLOSE ();
#endif

	if (pst_open (pst, filename, NULL) < 0) {
		pst_error_msg ("Error opening PST file %s", filename);
		return -1;
	}

	if (pst_load_index (pst) < 0) {
		pst_error_msg ("Error loading indexes");
		return -1;
	}

	if (pst_load_extended_attributes (pst) < 0) {
		pst_error_msg ("Error loading file items");
		return -1;
	}

	return 0;
}

/**
 * get_pst_rootname:
 * @pst: pst_file structure to be used by libpst
 * @filename: if non %NULL, fallback to this name if folder name is not
 * available
 *
 * Open determine root folder name of PST file
 *
 * Returns: pointer to name of root folder (should be freed by caller),
 * or %NULL if error
 */
gchar *
get_pst_rootname (pst_file *pst,
                  gchar *filename)
{
	pst_item *item = NULL;
	gchar *rootname = NULL;

	if ((item = pst_parse_item (pst, pst->d_head, NULL)) == NULL) {
		pst_error_msg ("Could not get root record");
		return NULL;
	}

	if (item->message_store == NULL) {
		pst_error_msg ("Could not get root message store");
		pst_freeItem (item);
		return NULL;
	}

	/* default the file_as to the same as the main filename if it doesn't exist */
	if (item->file_as.str == NULL) {
		if (filename == NULL) {
			pst_freeItem (item);
			return NULL;
		}
		rootname = g_path_get_basename (filename);
	} else {
		rootname = g_strdup (item->file_as.str);
	}

	pst_freeItem (item);

	return rootname;
}
