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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
/* Work around namespace clobbage in <windows.h> */
#define DATADIR windows_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include <camel/camel-stream-fs.h>
#include <camel/camel-url-scanner.h>
#include <camel/camel-file-utils.h>

#include "em-filter-editor.h"

#include <bonobo/bonobo-listener.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-event-source.h>

#include <glib/gi18n.h>

#include <gio/gio.h>

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "message-tag-followup.h"

#include <libedataserver/e-data-server-util.h>
#include <libedataserver/e-flag.h>
#include <libedataserver/e-proxy.h>
#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "e-util/e-mktemp.h"
#include "e-util/e-account-utils.h"
#include "e-util/e-dialog-utils.h"
#include "e-util/e-error.h"
#include "shell/e-shell.h"
#include "widgets/misc/e-alert-activity.h"
#include "widgets/misc/e-attachment.h"

#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-format-quote.h"
#include "e-mail-local.h"

static void emu_save_part_done (CamelMimePart *part, gchar *name, gint done, gpointer data);

#define d(x)

const gchar *
em_utils_get_data_dir (void)
{
	EShell *shell;
	EShellBackend *shell_backend;

	/* XXX This is a temporary solution until I can figure out a
	 *     better way.  Ideally, nothing below the module layer
	 *     should need to know about the user data directory. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	return e_shell_backend_get_data_dir (shell_backend);
}

const gchar *
em_utils_get_config_dir (void)
{
	EShell *shell;
	EShellBackend *shell_backend;

	/* XXX This is a temporary solution until I can figure out a
	 *     better way.  Ideally, nothing below the module layer
	 *     should need to know about the user config directory. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	return e_shell_backend_get_config_dir (shell_backend);
}

/**
 * em_utils_prompt_user:
 * @parent: parent window
 * @promptkey: gconf key to check if we should prompt the user or not.
 * @tag: e_error tag.
 * @arg0: The first of a NULL terminated list of arguments for the error.
 *
 * Convenience function to query the user with a Yes/No dialog and a
 * "Do not show this dialog again" checkbox. If the user checks that
 * checkbox, then @promptkey is set to %FALSE, otherwise it is set to
 * %TRUE.
 *
 * Returns %TRUE if the user clicks Yes or %FALSE otherwise.
 **/
gboolean
em_utils_prompt_user(GtkWindow *parent, const gchar *promptkey, const gchar *tag, const gchar *arg0, ...)
{
	GtkWidget *mbox, *check = NULL;
	va_list ap;
	gint button;
	GConfClient *gconf = mail_config_get_gconf_client();

	if (promptkey
	    && !gconf_client_get_bool(gconf, promptkey, NULL))
		return TRUE;

	va_start(ap, arg0);
	mbox = e_error_newv(parent, tag, arg0, ap);
	va_end(ap);

	if (promptkey) {
		check = gtk_check_button_new_with_mnemonic (_("_Do not show this message again."));
		gtk_container_set_border_width((GtkContainer *)check, 12);
		gtk_box_pack_start ((GtkBox *)((GtkDialog *) mbox)->vbox, check, TRUE, TRUE, 0);
		gtk_widget_show (check);
	}

	button = gtk_dialog_run ((GtkDialog *) mbox);
	if (promptkey)
		gconf_client_set_bool(gconf, promptkey, !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check)), NULL);

	gtk_widget_destroy(mbox);

	return button == GTK_RESPONSE_YES;
}

/**
 * em_utils_uids_copy:
 * @uids: array of uids
 *
 * Duplicates the array of uids held by @uids into a new
 * GPtrArray. Use em_utils_uids_free() to free the resultant uid
 * array.
 *
 * Returns a duplicate copy of @uids.
 **/
GPtrArray *
em_utils_uids_copy (GPtrArray *uids)
{
	GPtrArray *copy;
	gint i;

	copy = g_ptr_array_new ();
	g_ptr_array_set_size (copy, uids->len);

	for (i = 0; i < uids->len; i++)
		copy->pdata[i] = g_strdup (uids->pdata[i]);

	return copy;
}

/**
 * em_utils_uids_free:
 * @uids: array of uids
 *
 * Frees the array of uids pointed to by @uids back to the system.
 **/
void
em_utils_uids_free (GPtrArray *uids)
{
	gint i;

	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);

	g_ptr_array_free (uids, TRUE);
}

/**
 * em_utils_check_user_can_send_mail:
 *
 * Returns %TRUE if the user has an account configured (to send mail)
 * or %FALSE otherwise.
 **/
gboolean
em_utils_check_user_can_send_mail (void)
{
	EAccountList *account_list;
	EAccount *account;

	account_list = e_get_account_list ();

	if (e_list_length ((EList *) account_list) == 0)
		return FALSE;

	if (!(account = e_get_default_account ()))
		return FALSE;

	/* Check for a transport */
	if (!account->transport->url)
		return FALSE;

	return TRUE;
}

/* Editing Filters/Search Folders... */

static GtkWidget *filter_editor = NULL;

static void
em_filter_editor_response (GtkWidget *dialog, gint button, gpointer user_data)
{
	EMFilterContext *fc;

	if (button == GTK_RESPONSE_OK) {
		const gchar *data_dir;
		gchar *user;

		data_dir = em_utils_get_data_dir ();
		fc = g_object_get_data ((GObject *) dialog, "context");
		user = g_strdup_printf ("%s/filters.xml", data_dir);
		rule_context_save ((RuleContext *) fc, user);
		g_free (user);
	}

	gtk_widget_destroy (dialog);

	filter_editor = NULL;
}

static EMFilterSource em_filter_source_element_names[] = {
	{ "incoming", },
	{ "outgoing", },
	{ NULL }
};

/**
 * em_utils_edit_filters:
 * @parent: parent window
 *
 * Opens or raises the filters editor dialog so that the user may edit
 * his/her filters. If @parent is non-NULL, then the dialog will be
 * created as a child window of @parent's toplevel window.
 **/
void
em_utils_edit_filters (GtkWidget *parent)
{
	const gchar *data_dir;
	gchar *user, *system;
	EMFilterContext *fc;

	if (filter_editor) {
		gdk_window_raise (GTK_WIDGET (filter_editor)->window);
		return;
	}

	data_dir = em_utils_get_data_dir ();

	fc = em_filter_context_new ();
	user = g_build_filename (data_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	rule_context_load ((RuleContext *) fc, system, user);
	g_free (user);
	g_free (system);

	if (((RuleContext *) fc)->error) {
		GtkWidget *w = e_error_new((GtkWindow *)parent, "mail:filter-load-error", ((RuleContext *)fc)->error, NULL);
		em_utils_show_error_silent (w);
		return;
	}

	if (em_filter_source_element_names[0].name == NULL) {
		em_filter_source_element_names[0].name = _("Incoming");
		em_filter_source_element_names[1].name = _("Outgoing");
	}

	filter_editor = (GtkWidget *) em_filter_editor_new (fc, em_filter_source_element_names);
	if (parent != NULL)
		e_dialog_set_transient_for ((GtkWindow *) filter_editor, parent);

	gtk_window_set_title (GTK_WINDOW (filter_editor), _("Message Filters"));
	g_object_set_data_full ((GObject *) filter_editor, "context", fc, (GDestroyNotify) g_object_unref);
	g_signal_connect (filter_editor, "response", G_CALLBACK (em_filter_editor_response), NULL);
	gtk_widget_show (GTK_WIDGET (filter_editor));
}

/*
 * Picked this from e-d-s/libedataserver/e-data.
 * But it allows more characters to occur in filenames, especially when saving attachment.
 */
void
em_filename_make_safe (gchar *string)
{
	gchar *p, *ts;
	gunichar c;
#ifdef G_OS_WIN32
	const gchar *unsafe_chars = "/\":*?<>|\\#";
#else
	const gchar *unsafe_chars = "/#";
#endif

	g_return_if_fail (string != NULL);
	p = string;

	while (p && *p) {
		c = g_utf8_get_char (p);
		ts = p;
		p = g_utf8_next_char (p);
		/* I wonder what this code is supposed to actually
		 * achieve, and whether it does that as currently
		 * written?
		 */
		if (!g_unichar_isprint(c) || ( c < 0xff && strchr (unsafe_chars, c&0xff ))) {
			while (ts<p)
				*ts++ = '_';
		}
	}
}

/* Saving messages... */

static const gchar *
emu_save_get_filename_for_part (CamelMimePart *part)
{
	const gchar *filename;

	filename = camel_mime_part_get_filename (part);
	if (filename == NULL) {
		if (CAMEL_IS_MIME_MESSAGE (part)) {
			filename = camel_mime_message_get_subject (
				CAMEL_MIME_MESSAGE (part));
			if (filename == NULL)
				filename = _("message");
		} else
			filename = _("attachment");
	}

	return filename;
}

/**
 * em_utils_save_part:
 * @parent: parent window
 * @prompt: prompt string
 * @part: part to save
 *
 * Saves a mime part to disk (prompting the user for filename).
 **/
void
em_utils_save_part (GtkWindow *parent, const gchar *prompt, CamelMimePart *part)
{
	GtkWidget *file_chooser;
	const gchar *utf8_filename;
	gchar *uri = NULL, *filename;

	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

	utf8_filename = emu_save_get_filename_for_part (part);
	filename = g_filename_from_utf8 (utf8_filename, -1, NULL, NULL, NULL);
	em_filename_make_safe (filename);

	file_chooser = e_file_get_save_filesel (
		parent, prompt, filename, GTK_FILE_CHOOSER_ACTION_SAVE);

	if (gtk_dialog_run (GTK_DIALOG (file_chooser)) != GTK_RESPONSE_OK)
		goto exit;

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (file_chooser));

	/* XXX Would be nice to mention _why_ we can't save. */
	if (!e_file_can_save (GTK_WINDOW (file_chooser), uri)) {
		g_warning ("Unable to save %s", uri);
		goto exit;
	}

	e_file_update_save_path (
		gtk_file_chooser_get_current_folder_uri (
		GTK_FILE_CHOOSER (file_chooser)), TRUE);

	mail_save_part (part, uri, NULL, NULL, FALSE);

exit:
	gtk_widget_destroy (file_chooser);
	g_free (uri);
	g_free (filename);
}

/* It "assigns" to each part its unique file name, based on the appearance.
   parts contains CamelMimePart, returned value contains gchar *, in same
   order as parts; resulting list should free data and GSList itself as well.
*/
static GSList *
get_unique_file_names (GSList *parts)
{
	GSList *iter;
	GSList *file_names = NULL;

	if (!parts)
		return NULL;

	for (iter = parts; iter != NULL; iter = iter->next) {
		CamelMimePart *part = iter->data;
		const gchar *utf8_filename;
		gchar *filename;

		utf8_filename = emu_save_get_filename_for_part (part);
		filename = g_filename_from_utf8 (utf8_filename, -1, NULL, NULL, NULL);
		em_filename_make_safe (filename);

		file_names = g_slist_prepend (file_names, filename);
	}

	if (file_names) {
		GSList *sorted_file_names;
		gint counter = 1;
		const gchar *last;
		GCompareFunc cmp_func = (GCompareFunc) strcmp;

		#ifdef G_OS_WIN32
		cmp_func = (GCompareFunc) g_ascii_strcasecmp;
		#endif

		/* we prepended, so reverse to make right order */
		file_names = g_slist_reverse (file_names);

		sorted_file_names = g_slist_sort (g_slist_copy (file_names), cmp_func);
		last = sorted_file_names->data;
		for (iter = sorted_file_names->next; iter; iter = iter->next) {
			gchar *name = iter->data;

			if (name && last && cmp_func (name, last) == 0) {
				gchar *new_name;
				gchar *p = strrchr (name, '.');
				GSList *i2;

				/* if we have an extension, then place number before it (at p is ".ext"),
				   otherwise just append number in brackets */
				if (p)
					new_name = g_strdup_printf ("%.*s(%d)%s", (gint) (p - name), name, counter, p);
				else
					new_name = g_strdup_printf ("%s(%d)", name, counter);

				/* we need to find the proper item in unsorted list and replace with new name;
				   we should always find that item, so no check for leaks */
				for (i2 = file_names; i2; i2 = i2->next) {
					if (i2->data == name) {
						g_free (name);
						i2->data = new_name;
						break;
					}
				}

				counter++;
			} else {
				last = name;
				counter = 1;
			}
		}

		g_slist_free (sorted_file_names);
	}

	return file_names;
}

void
em_utils_save_parts (GtkWindow *parent, const gchar *prompt, GSList *parts)
{
	GtkWidget *file_chooser;
	gchar *path_uri;
	GSList *iter, *file_names, *iter_file;

	g_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

	file_chooser = e_file_get_save_filesel (
		parent, prompt, NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

	if (gtk_dialog_run (GTK_DIALOG (file_chooser)) != GTK_RESPONSE_OK)
		goto exit;

	path_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (file_chooser));

	e_file_update_save_path (path_uri, FALSE);

	file_names = get_unique_file_names (parts);

	for (iter = parts, iter_file = file_names; iter && iter_file; iter = iter->next, iter_file = iter_file->next) {
		CamelMimePart *part = iter->data;
		gchar *uri, *filename;

		filename = iter_file->data;
		uri = g_build_path ("/", path_uri, filename, NULL);
		g_free (filename);
		iter_file->data = NULL;

		/* XXX Would be nice to mention _why_ we can't save. */
		if (e_file_can_save (GTK_WINDOW (file_chooser), uri))
			mail_save_part (part, uri, NULL, NULL, FALSE);
		else
			g_warning ("Unable to save %s", uri);

		g_free (uri);
	}

	g_slist_free (file_names);
	g_free (path_uri);

exit:
	gtk_widget_destroy (file_chooser);
}

/**
 * em_utils_save_part_to_file:
 * @parent: parent window
 * @filename: filename to save to
 * @part: part to save
 *
 * Save a part's content to a specific file
 * Creates all needed directories and overwrites without prompting
 *
 * Returns %TRUE if saving succeeded, %FALSE otherwise
 **/
gboolean
em_utils_save_part_to_file(GtkWindow *parent, const gchar *filename, CamelMimePart *part)
{
	gint done;
	gchar *dirname;
	struct stat st;

	if (filename[0] == 0)
		return FALSE;

	dirname = g_path_get_dirname(filename);
	if (g_mkdir_with_parents(dirname, 0777) == -1) {
		GtkWidget *w = e_error_new(parent, "mail:no-create-path", filename, g_strerror(errno), NULL);
		g_free(dirname);
		em_utils_show_error_silent (w);
		return FALSE;
	}
	g_free(dirname);

	if (g_access(filename, F_OK) == 0) {
		if (g_access(filename, W_OK) != 0) {
			e_error_run(parent, E_ERROR_ASK_FILE_EXISTS_OVERWRITE, filename, NULL);
			return FALSE;
		}
	}

	if (g_stat(filename, &st) != -1 && !S_ISREG(st.st_mode)) {
		GtkWidget *w = e_error_new(parent, "mail:no-write-path-notfile", filename, NULL);
		em_utils_show_error_silent (w);
		return FALSE;
	}

	/* FIXME: This doesn't handle default charsets */
	mail_msg_wait(mail_save_part(part, filename, emu_save_part_done, &done, FALSE));

	return done;
}

struct _save_messages_data {
	CamelFolder *folder;
	GPtrArray *uids;
};

static void
emu_save_messages_response(GtkWidget *filesel, gint response, struct _save_messages_data *data)
{
	gchar *uri;

	if (response == GTK_RESPONSE_OK) {
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (filesel));

		if (!e_file_can_save((GtkWindow *)filesel, uri)) {
			g_free(uri);
			return;
		}

		e_file_update_save_path(gtk_file_chooser_get_current_folder_uri(
					GTK_FILE_CHOOSER (filesel)), TRUE);
		mail_save_messages(data->folder, data->uids, uri, NULL, NULL);
		data->uids = NULL;
		g_free(uri);
	}

	camel_object_unref(data->folder);
	if (data->uids)
		em_utils_uids_free(data->uids);
	g_free(data);
	gtk_widget_destroy((GtkWidget *)filesel);
}

/**
 * em_utils_save_messages:
 * @parent: parent window
 * @folder: folder containing messages to save
 * @uids: uids of messages to save
 *
 * Saves a group of messages to disk in mbox format (prompting the
 * user for filename).
 **/
void
em_utils_save_messages (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids)
{
	struct _save_messages_data *data;
	GtkWidget *filesel;
	gchar *filename = NULL;
	CamelMessageInfo *info = NULL;

	g_return_if_fail (GTK_IS_WINDOW (parent));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	info = camel_folder_get_message_info (folder, uids->pdata[0]);
	if (info) {
		filename = g_strdup (camel_message_info_subject (info));
		e_filename_make_safe (filename);
		camel_message_info_free (info);
	}

	filesel = e_file_get_save_filesel (parent, _("Save Message..."), filename, GTK_FILE_CHOOSER_ACTION_SAVE);
	if (filename)
		g_free (filename);

	camel_object_ref(folder);

	data = g_malloc(sizeof(struct _save_messages_data));
	data->folder = folder;
	data->uids = uids;

	g_signal_connect(filesel, "response", G_CALLBACK(emu_save_messages_response), data);
	gtk_widget_show((GtkWidget *)filesel);
}

/* ********************************************************************** */

static void
emu_add_address_cb(BonoboListener *listener, const gchar *name, const CORBA_any *any, CORBA_Environment *ev, gpointer data)
{
	gchar *type = bonobo_event_subtype(name);

	if (!strcmp(type, "Destroy"))
		gtk_widget_destroy((GtkWidget *)data);

	g_free(type);
}

/* one of email or vcard should be always NULL, never both of them */
static void
emu_add_address_or_vcard (GtkWindow *parent, const gchar *email, const gchar *vcard)
{
	GtkWidget *win;
	GtkWidget *control;
	/*GtkWidget *socket;*/
	gchar *email_buf = NULL;

	if (email) {
		CamelInternetAddress *cia;

		cia = camel_internet_address_new ();
		if (camel_address_decode ((CamelAddress *) cia, email) == -1) {
			camel_object_unref (cia);
			return;
		}

		email_buf = camel_address_format ((CamelAddress *) cia);
		camel_object_unref (cia);
	}

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title((GtkWindow *)win, _("Add address"));

	gtk_window_set_transient_for ((GtkWindow *)win, parent);

	gtk_window_set_position((GtkWindow *)win, GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_type_hint((GtkWindow *)win, GDK_WINDOW_TYPE_HINT_DIALOG);

	control = bonobo_widget_new_control("OAFIID:GNOME_Evolution_Addressbook_AddressPopup:" BASE_VERSION, CORBA_OBJECT_NIL);

	if (email_buf)
		bonobo_widget_set_property ((BonoboWidget *) control, "email", TC_CORBA_string, email_buf, NULL);
	else
		bonobo_widget_set_property ((BonoboWidget *) control, "vcard", TC_CORBA_string, vcard, NULL);

	g_free (email_buf);

	bonobo_event_source_client_add_listener(bonobo_widget_get_objref((BonoboWidget *)control), emu_add_address_cb, NULL, NULL, win);

	/*socket = find_socket (GTK_CONTAINER (control));
	  g_object_weak_ref ((GObject *) socket, (GWeakNotify) gtk_widget_destroy, win);*/

	gtk_container_add((GtkContainer *)win, control);
	gtk_widget_show_all(win);
}

/**
 * em_utils_add_address:
 * @parent:
 * @email:
 *
 * Add address @email to the addressbook.
 **/
void
em_utils_add_address (GtkWindow *parent, const gchar *email)
{
	g_return_if_fail (GTK_IS_WINDOW (parent));

	emu_add_address_or_vcard (parent, email, NULL);
}

/**
 * em_utils_add_vcard:
 * Adds whole vCard to the addressbook.
 **/
void
em_utils_add_vcard (GtkWindow *parent, const gchar *vcard)
{
	g_return_if_fail (GTK_IS_WINDOW (parent));

	emu_add_address_or_vcard (parent, NULL, vcard);
}

/* ********************************************************************** */
/* Flag-for-Followup... */

/**
 * em_utils_flag_for_followup:
 * @reader: an #EMailReader
 * @folder: folder containing messages to flag
 * @uids: uids of messages to flag
 *
 * Open the Flag-for-Followup editor for the messages specified by
 * @folder and @uids.
 **/
void
em_utils_flag_for_followup (EMailReader *reader,
                            CamelFolder *folder,
                            GPtrArray *uids)
{
	EMFormatHTMLDisplay *html_display;
	MessageTagEditor *editor;
	GtkWindow *parent;
	CamelTag *tags;
	gint i;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	editor = message_tag_followup_new ();
	parent = e_mail_reader_get_window (reader);
	gtk_window_set_transient_for (GTK_WINDOW (editor), parent);

	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (folder, uids->pdata[i]);

		if (info == NULL)
			continue;

		message_tag_followup_append_message (
			MESSAGE_TAG_FOLLOWUP (editor),
			camel_message_info_from (info),
			camel_message_info_subject (info));

		camel_message_info_free(info);
	}

	/* special-case... */
	if (uids->len == 1) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (folder, uids->pdata[0]);
		if (info) {
			tags = (CamelTag *) camel_message_info_user_tags (info);

			if (tags)
				message_tag_editor_set_tag_list (editor, tags);
			camel_message_info_free (info);
		}
	}

	if (gtk_dialog_run (GTK_DIALOG (editor)) != GTK_RESPONSE_OK)
		goto exit;

	tags = message_tag_editor_get_tag_list (editor);
	if (tags == NULL)
		goto exit;

	camel_folder_freeze (folder);
	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info;
		CamelTag *iter;

		info = camel_folder_get_message_info(folder, uids->pdata[i]);

		if (info == NULL)
			continue;

		for (iter = tags; iter != NULL; iter = iter->next)
			camel_message_info_set_user_tag (
				info, iter->name, iter->value);

		camel_message_info_free (info);
	}

	camel_folder_thaw (folder);
	camel_tag_list_free (&tags);

	html_display = e_mail_reader_get_html_display (reader);
	em_format_redraw (EM_FORMAT (html_display));

exit:
	/* XXX We shouldn't be freeing this. */
	em_utils_uids_free (uids);

	gtk_widget_destroy (GTK_WIDGET (editor));
}

/**
 * em_utils_flag_for_followup_clear:
 * @parent: parent window
 * @folder: folder containing messages to unflag
 * @uids: uids of messages to unflag
 *
 * Clears the Flag-for-Followup flag on the messages referenced by
 * @folder and @uids.
 **/
void
em_utils_flag_for_followup_clear (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids)
{
	gint i;

	g_return_if_fail (GTK_IS_WINDOW (parent));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	camel_folder_freeze (folder);
	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *mi = camel_folder_get_message_info(folder, uids->pdata[i]);

		if (mi) {
			camel_message_info_set_user_tag(mi, "follow-up", NULL);
			camel_message_info_set_user_tag(mi, "due-by", NULL);
			camel_message_info_set_user_tag(mi, "completed-on", NULL);
			camel_message_info_free(mi);
		}
	}
	camel_folder_thaw (folder);

	em_utils_uids_free (uids);
}

/**
 * em_utils_flag_for_followup_completed:
 * @parent: parent window
 * @folder: folder containing messages to 'complete'
 * @uids: uids of messages to 'complete'
 *
 * Sets the completed state (and date/time) for each message
 * referenced by @folder and @uids that is marked for
 * Flag-for-Followup.
 **/
void
em_utils_flag_for_followup_completed (GtkWindow *parent, CamelFolder *folder, GPtrArray *uids)
{
	gchar *now;
	gint i;

	g_return_if_fail (GTK_IS_WINDOW (parent));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	now = camel_header_format_date (time (NULL), 0);

	camel_folder_freeze (folder);
	for (i = 0; i < uids->len; i++) {
		const gchar *tag;
		CamelMessageInfo *mi = camel_folder_get_message_info(folder, uids->pdata[i]);

		if (mi) {
			tag = camel_message_info_user_tag(mi, "follow-up");
			if (tag && tag[0])
				camel_message_info_set_user_tag(mi, "completed-on", now);
			camel_message_info_free(mi);
		}
	}
	camel_folder_thaw (folder);

	g_free (now);

	em_utils_uids_free (uids);
}

#include "camel/camel-stream-mem.h"
#include "camel/camel-stream-filter.h"
#include "camel/camel-mime-filter-from.h"

/* This kind of sucks, because for various reasons most callers need to run synchronously
   in the gui thread, however this could take a long, blocking time, to run */
static gint
em_utils_write_messages_to_stream(CamelFolder *folder, GPtrArray *uids, CamelStream *stream)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilterFrom *from_filter;
	gint i, res = 0;

	from_filter = camel_mime_filter_from_new();
	filtered_stream = camel_stream_filter_new_with_stream(stream);
	camel_stream_filter_add(filtered_stream, (CamelMimeFilter *)from_filter);
	camel_object_unref(from_filter);

	for (i=0; i<uids->len; i++) {
		CamelMimeMessage *message;
		gchar *from;

		message = camel_folder_get_message(folder, uids->pdata[i], NULL);
		if (message == NULL) {
			res = -1;
			break;
		}

		/* we need to flush after each stream write since we are writing to the same stream */
		from = camel_mime_message_build_mbox_from(message);

		if (camel_stream_write_string(stream, from) == -1
		    || camel_stream_flush(stream) == -1
		    || camel_data_wrapper_write_to_stream((CamelDataWrapper *)message, (CamelStream *)filtered_stream) == -1
		    || camel_stream_flush((CamelStream *)filtered_stream) == -1)
			res = -1;

		g_free(from);
		camel_object_unref(message);

		if (res == -1)
			break;
	}

	camel_object_unref(filtered_stream);

	return res;
}

/* This kind of sucks, because for various reasons most callers need to run synchronously
   in the gui thread, however this could take a long, blocking time, to run */
static gint
em_utils_read_messages_from_stream(CamelFolder *folder, CamelStream *stream)
{
	CamelException *ex = camel_exception_new();
	CamelMimeParser *mp = camel_mime_parser_new();
	gint res = -1;

	camel_mime_parser_scan_from(mp, TRUE);
	camel_mime_parser_init_with_stream(mp, stream);

	while (camel_mime_parser_step(mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMimeMessage *msg;

		/* NB: de-from filter, once written */
		msg = camel_mime_message_new();
		if (camel_mime_part_construct_from_parser((CamelMimePart *)msg, mp) == -1) {
			camel_object_unref(msg);
			break;
		}

		camel_folder_append_message(folder, msg, NULL, NULL, ex);
		camel_object_unref(msg);

		if (camel_exception_is_set (ex))
			break;

		camel_mime_parser_step(mp, NULL, NULL);
	}

	camel_object_unref(mp);
	if (!camel_exception_is_set(ex))
		res = 0;
	camel_exception_free(ex);

	return res;
}

/**
 * em_utils_selection_set_mailbox:
 * @data: selection data
 * @folder: folder containign messages to copy into the selection
 * @uids: uids of the messages to copy into the selection
 *
 * Creates a mailbox-format selection.
 * Warning: Could be BIG!
 * Warning: This could block the ui for an extended period.
 **/
void
em_utils_selection_set_mailbox(GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids)
{
	CamelStream *stream;

	stream = camel_stream_mem_new();
	if (em_utils_write_messages_to_stream(folder, uids, stream) == 0)
		gtk_selection_data_set(data, data->target, 8,
				       ((CamelStreamMem *)stream)->buffer->data,
				       ((CamelStreamMem *)stream)->buffer->len);

	camel_object_unref(stream);
}

/**
 * em_utils_selection_get_mailbox:
 * @data: selection data
 * @folder:
 *
 * Receive a mailbox selection/dnd
 * Warning: Could be BIG!
 * Warning: This could block the ui for an extended period.
 * FIXME: Exceptions?
 **/
void
em_utils_selection_get_mailbox(GtkSelectionData *data, CamelFolder *folder)
{
	CamelStream *stream;

	if (data->data == NULL || data->length == -1)
		return;

	/* TODO: a stream mem with read-only access to existing data? */
	/* NB: Although copying would let us run this async ... which it should */
	stream = (CamelStream *)camel_stream_mem_new_with_buffer((gchar *)data->data, data->length);
	em_utils_read_messages_from_stream(folder, stream);
	camel_object_unref(stream);
}

/**
 * em_utils_selection_get_message:
 * @data:
 * @folder:
 *
 * get a message/rfc822 data.
 **/
void
em_utils_selection_get_message(GtkSelectionData *data, CamelFolder *folder)
{
	CamelStream *stream;
	CamelException *ex;
	CamelMimeMessage *msg;

	if (data->data == NULL || data->length == -1)
		return;

	ex = camel_exception_new();
	stream = (CamelStream *)camel_stream_mem_new_with_buffer((gchar *)data->data, data->length);
	msg = camel_mime_message_new();
	if (camel_data_wrapper_construct_from_stream((CamelDataWrapper *)msg, stream) == 0)
		camel_folder_append_message(folder, msg, NULL, NULL, ex);
	camel_object_unref(msg);
	camel_object_unref(stream);
	camel_exception_free(ex);
}

/**
 * em_utils_selection_set_uidlist:
 * @data: selection data
 * @uri:
 * @uids:
 *
 * Sets a "x-uid-list" format selection data.
 *
 * FIXME: be nice if this could take a folder argument rather than uri
 **/
void
em_utils_selection_set_uidlist(GtkSelectionData *data, const gchar *uri, GPtrArray *uids)
{
	GByteArray *array = g_byte_array_new();
	gint i;

	/* format: "uri\0uid1\0uid2\0uid3\0...\0uidn\0" */

	g_byte_array_append(array, (guchar *)uri, strlen(uri)+1);

	for (i=0; i<uids->len; i++)
		g_byte_array_append(array, uids->pdata[i], strlen(uids->pdata[i])+1);

	gtk_selection_data_set(data, data->target, 8, array->data, array->len);
	g_byte_array_free(array, TRUE);
}

/**
 * em_utils_selection_get_uidlist:
 * @data: selection data
 * @move: do we delete the messages.
 *
 * Convert a uid list into a copy/move operation.
 *
 * Warning: Could take some time to run.
 **/
void
em_utils_selection_get_uidlist(GtkSelectionData *data, CamelFolder *dest, gint move, CamelException *ex)
{
	/* format: "uri\0uid1\0uid2\0uid3\0...\0uidn" */
	gchar *inptr, *inend;
	GPtrArray *uids;
	CamelFolder *folder;

	if (data == NULL || data->data == NULL || data->length == -1)
		return;

	uids = g_ptr_array_new();

	inptr = (gchar *)data->data;
	inend = (gchar *)(data->data + data->length);
	while (inptr < inend) {
		gchar *start = inptr;

		while (inptr < inend && *inptr)
			inptr++;

		if (start > (gchar *)data->data)
			g_ptr_array_add(uids, g_strndup(start, inptr-start));

		inptr++;
	}

	if (uids->len == 0) {
		g_ptr_array_free(uids, TRUE);
		return;
	}

	folder = mail_tool_uri_to_folder((gchar *)data->data, 0, ex);
	if (folder) {
		camel_folder_transfer_messages_to(folder, uids, dest, NULL, move, ex);
		camel_object_unref(folder);
	}

	em_utils_uids_free(uids);
}

/**
 * em_utils_selection_set_urilist:
 * @data:
 * @folder:
 * @uids:
 *
 * Set the selection data @data to a uri which points to a file, which is
 * a berkely mailbox format mailbox.  The file is automatically cleaned
 * up when the application quits.
 **/
void
em_utils_selection_set_urilist(GtkSelectionData *data, CamelFolder *folder, GPtrArray *uids)
{
	gchar *tmpdir;
	CamelStream *fstream;
	gchar *uri, *file = NULL, *tmpfile;
	gint fd;
	CamelMessageInfo *info;

	tmpdir = e_mkdtemp("drag-n-drop-XXXXXX");
	if (tmpdir == NULL)
		return;

	/* Try to get the drop filename from the message or folder */
	if (uids->len == 1) {
		info = camel_folder_get_message_info(folder, uids->pdata[0]);
		if (info) {
			file = g_strdup(camel_message_info_subject(info));
			camel_folder_free_message_info(folder, info);
		}
	}

	/* TODO: Handle conflicts? */
	if (file == NULL) {
		/* Drop filename for messages from a mailbox */
		file = g_strdup_printf(_("Messages from %s"), folder->name);
	}

	e_filename_make_safe(file);

	tmpfile = g_build_filename(tmpdir, file, NULL);
	g_free(tmpdir);
	g_free(file);

	fd = g_open(tmpfile, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0666);
	if (fd == -1) {
		g_free(tmpfile);
		return;
	}

	uri = g_filename_to_uri(tmpfile, NULL, NULL);
	g_free(tmpfile);
	fstream = camel_stream_fs_new_with_fd(fd);
	if (fstream) {
		if (em_utils_write_messages_to_stream(folder, uids, fstream) == 0) {
			/* terminate with \r\n to be compliant with the spec */
			gchar *uri_crlf = g_strconcat(uri, "\r\n", NULL);

			gtk_selection_data_set(data, data->target, 8, (guchar *)uri_crlf, strlen(uri_crlf));
			g_free(uri_crlf);
		}

		camel_object_unref(fstream);
	} else
		close(fd);

	g_free(uri);
}

/**
 * em_utils_selection_set_urilist:
 * @data:
 * @folder:
 * @uids:
 *
 * Get the selection data @data from a uri list which points to a
 * file, which is a berkely mailbox format mailbox.  The file is
 * automatically cleaned up when the application quits.
 **/
void
em_utils_selection_get_urilist(GtkSelectionData *data, CamelFolder *folder)
{
	CamelStream *stream;
	CamelURL *url;
	gint fd, i, res = 0;
	gchar *tmp, **uris;

	d(printf(" * drop uri list\n"));

	tmp = g_strndup((gchar *)data->data, data->length);
	uris = g_strsplit(tmp, "\n", 0);
	g_free(tmp);
	for (i=0;res == 0 && uris[i];i++) {
		g_strstrip(uris[i]);
		if (uris[i][0] == '#')
			continue;

		url = camel_url_new(uris[i], NULL);
		if (url == NULL)
			continue;

		if (strcmp(url->protocol, "file") == 0
		    && (fd = g_open(url->path, O_RDONLY | O_BINARY, 0)) != -1) {
			stream = camel_stream_fs_new_with_fd(fd);
			if (stream) {
				res = em_utils_read_messages_from_stream(folder, stream);
				camel_object_unref(stream);
			} else
				close(fd);
		}
		camel_url_free(url);
	}

	g_strfreev(uris);
}

static void
emu_save_part_done(CamelMimePart *part, gchar *name, gint done, gpointer data)
{
	((gint *)data)[0] = done;
}

/**
 * em_utils_temp_save_part:
 * @parent:
 * @part:
 * @mode: readonly or not.
 *
 * Save a part's content to a temporary file, and return the
 * filename.
 *
 * Return value: NULL if anything failed.
 **/
gchar *
em_utils_temp_save_part(GtkWidget *parent, CamelMimePart *part, gboolean mode)
{
	const gchar *filename;
	gchar *tmpdir, *path, *utf8_mfilename = NULL, *mfilename = NULL;
	gint done;
	GtkWidget *w;

	tmpdir = e_mkdtemp("evolution-tmp-XXXXXX");
	if (tmpdir == NULL) {
		w = e_error_new((GtkWindow *)parent, "mail:no-create-tmp-path", g_strerror(errno), NULL);
		em_utils_show_error_silent (w);
		return NULL;
	}

	filename = camel_mime_part_get_filename (part);
	if (filename == NULL) {
		/* This is the default filename used for temporary file creation */
		filename = _("Unknown");
	} else {
		utf8_mfilename = g_strdup (filename);
		e_filename_make_safe (utf8_mfilename);
		mfilename = g_filename_from_utf8 ((const gchar *) utf8_mfilename, -1, NULL, NULL, NULL);
		g_free (utf8_mfilename);
		filename = (const gchar *) mfilename;
	}

	path = g_build_filename(tmpdir, filename, NULL);
	g_free(tmpdir);
	g_free(mfilename);

	/* FIXME: This doesn't handle default charsets */
	if (mode)
		mail_msg_wait(mail_save_part(part, path, emu_save_part_done, &done, TRUE));
	else
		mail_msg_wait(mail_save_part(part, path, emu_save_part_done, &done, FALSE));
	if (!done) {
		/* mail_save_part should popup an error box automagically */
		g_free(path);
		path = NULL;
	}

	return path;
}

/** em_utils_folder_is_templates:
 * @folder: folder
 * @uri: uri for this folder, if known
 *
 * Decides if @folder is a Templates folder.
 *
 * Returns %TRUE if this is a Drafts folder or %FALSE otherwise.
 **/

gboolean
em_utils_folder_is_templates (CamelFolder *folder, const gchar *uri)
{
	CamelFolder *local_templates_folder;
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	gint is = FALSE;
	gchar *templates_uri;

	local_templates_folder =
		e_mail_local_get_folder (E_MAIL_FOLDER_TEMPLATES);

	if (folder == local_templates_folder)
		return TRUE;

	if (folder == NULL || uri == NULL)
		return FALSE;

	accounts = e_get_account_list ();
	iter = e_list_get_iterator ((EList *)accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *)e_iterator_get (iter);

		if (account->templates_folder_uri) {
			templates_uri = em_uri_to_camel (account->templates_folder_uri);
			if (camel_store_folder_uri_equal (folder->parent_store, templates_uri, uri)) {
				g_free (templates_uri);
				is = TRUE;
				break;
			}
			g_free (templates_uri);
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	return is;
}

/**
 * em_utils_folder_is_drafts:
 * @folder: folder
 * @uri: uri for this folder, if known
 *
 * Decides if @folder is a Drafts folder.
 *
 * Returns %TRUE if this is a Drafts folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_drafts(CamelFolder *folder, const gchar *uri)
{
	CamelFolder *local_drafts_folder;
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	gint is = FALSE;
	gchar *drafts_uri;

	local_drafts_folder =
		e_mail_local_get_folder (E_MAIL_FOLDER_DRAFTS);

	if (folder == local_drafts_folder)
		return TRUE;

	if (folder == NULL || uri == NULL)
		return FALSE;

	accounts = e_get_account_list ();
	iter = e_list_get_iterator((EList *)accounts);
	while (e_iterator_is_valid(iter)) {
		account = (EAccount *)e_iterator_get(iter);

		if (account->drafts_folder_uri) {
			drafts_uri = em_uri_to_camel (account->drafts_folder_uri);
			if (camel_store_folder_uri_equal (folder->parent_store, drafts_uri, uri)) {
				g_free (drafts_uri);
				is = TRUE;
				break;
			}
			g_free (drafts_uri);
		}

		e_iterator_next(iter);
	}

	g_object_unref(iter);

	return is;
}

/**
 * em_utils_folder_is_sent:
 * @folder: folder
 * @uri: uri for this folder, if known
 *
 * Decides if @folder is a Sent folder
 *
 * Returns %TRUE if this is a Sent folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_sent(CamelFolder *folder, const gchar *uri)
{
	CamelFolder *local_sent_folder;
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	gint is = FALSE;
	gchar *sent_uri;

	local_sent_folder = e_mail_local_get_folder (E_MAIL_FOLDER_SENT);

	if (folder == local_sent_folder)
		return TRUE;

	if (folder == NULL || uri == NULL)
		return FALSE;

	accounts = e_get_account_list ();
	iter = e_list_get_iterator((EList *)accounts);
	while (e_iterator_is_valid(iter)) {
		account = (EAccount *)e_iterator_get(iter);

		if (account->sent_folder_uri) {
			sent_uri = em_uri_to_camel (account->sent_folder_uri);
			if (camel_store_folder_uri_equal (folder->parent_store, sent_uri, uri)) {
				g_free (sent_uri);
				is = TRUE;
				break;
			}
			g_free (sent_uri);
		}

		e_iterator_next(iter);
	}

	g_object_unref(iter);

	return is;
}

/**
 * em_utils_folder_is_outbox:
 * @folder: folder
 * @uri: uri for this folder, if known
 *
 * Decides if @folder is an Outbox folder
 *
 * Returns %TRUE if this is an Outbox folder or %FALSE otherwise.
 **/
gboolean
em_utils_folder_is_outbox(CamelFolder *folder, const gchar *uri)
{
	CamelFolder *local_outbox_folder;

	local_outbox_folder =
		e_mail_local_get_folder (E_MAIL_FOLDER_OUTBOX);

	/* <Highlander>There can be only one.</Highlander> */
	return folder == local_outbox_folder;
}

/**
 * em_utils_adjustment_page:
 * @adj:
 * @down:
 *
 * Move an adjustment up/down forward/back one page.
 **/
void
em_utils_adjustment_page(GtkAdjustment *adj, gboolean down)
{
	gfloat page_size = adj->page_size - adj->step_increment;

	if (down) {
		if (adj->value < adj->upper - adj->page_size - page_size)
			adj->value += page_size;
		else if (adj->upper >= adj->page_size)
			adj->value = adj->upper - adj->page_size;
		else
			adj->value = adj->lower;
	} else {
		if (adj->value > adj->lower + page_size)
			adj->value -= page_size;
		else
			adj->value = adj->lower;
	}

	gtk_adjustment_value_changed(adj);
}

/* ********************************************************************** */
static EProxy *emu_proxy = NULL;
static GStaticMutex emu_proxy_lock = G_STATIC_MUTEX_INIT;

static gpointer
emu_proxy_setup (gpointer data)
{
	if (!emu_proxy) {
		emu_proxy = e_proxy_new ();
		e_proxy_setup_proxy (emu_proxy);
		/* not necessary to listen for changes here */
	}

	return NULL;
}

/**
 * em_utils_get_proxy_uri:
 *
 * Get the system proxy uri for 'pUri'.
 *
 * Return value: Must be freed when finished with.
 **/
gchar *
em_utils_get_proxy_uri (const gchar *pUri)
{
	gchar *uri = NULL;

	g_static_mutex_lock (&emu_proxy_lock);

	if (!emu_proxy) {
		mail_call_main (MAIL_CALL_p_p, (MailMainFunc)emu_proxy_setup, NULL);
	}

	if (e_proxy_require_proxy_for_uri (emu_proxy, pUri))
		uri = soup_uri_to_string (e_proxy_peek_uri_for (emu_proxy, pUri), FALSE);

	g_static_mutex_unlock (&emu_proxy_lock);

	return uri;
}

/**
 * em_utils_message_to_html:
 * @message:
 * @credits:
 * @flags: EMFormatQuote flags
 * @len:
 * @source:
 * @append: Text to append, can be NULL.
 *
 * Convert a message to html, quoting if the @credits attribution
 * string is given.
 *
 * Return value: The html version.
 **/
gchar *
em_utils_message_to_html(CamelMimeMessage *message, const gchar *credits, guint32 flags, gssize *len, EMFormat *source, const gchar *append)
{
	EMFormatQuote *emfq;
	CamelStreamMem *mem;
	GByteArray *buf;
	gchar *text;

	buf = g_byte_array_new ();
	mem = (CamelStreamMem *) camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (mem, buf);

	emfq = em_format_quote_new(credits, (CamelStream *)mem, flags);
	((EMFormat *) emfq)->composer = TRUE;

	if (!source) {
		GConfClient *gconf;
		gchar *charset;

		/* FIXME: we should be getting this from the current view, not the global setting. */
		gconf = gconf_client_get_default ();
		charset = gconf_client_get_string (gconf, "/apps/evolution/mail/display/charset", NULL);
		em_format_set_default_charset ((EMFormat *) emfq, charset);
		g_object_unref (gconf);
		g_free (charset);
	}

	em_format_format_clone((EMFormat *)emfq, NULL, NULL, message, source);
	g_object_unref (emfq);

	if (append && *append)
		camel_stream_write ((CamelStream*)mem, append, strlen (append));

	camel_stream_write((CamelStream *)mem, "", 1);
	camel_object_unref(mem);

	text = (gchar *)buf->data;
	if (len)
		*len = buf->len-1;
	g_byte_array_free(buf, FALSE);

	return text;
}

/* ********************************************************************** */

/**
 * em_utils_expunge_folder:
 * @parent: parent window
 * @folder: folder to expunge
 *
 * Expunges @folder.
 **/
void
em_utils_expunge_folder (GtkWidget *parent, CamelFolder *folder)
{
	gchar *name;

	camel_object_get(folder, NULL, CAMEL_OBJECT_DESCRIPTION, &name, 0);

	if (!em_utils_prompt_user ((GtkWindow *) parent, "/apps/evolution/mail/prompts/expunge", "mail:ask-expunge", name, NULL))
		return;

	mail_expunge_folder(folder, NULL, NULL);
}

/**
 * em_utils_empty_trash:
 * @parent: parent window
 *
 * Empties all Trash folders.
 **/
void
em_utils_empty_trash (GtkWidget *parent)
{
	CamelProvider *provider;
	EAccountList *accounts;
	EAccount *account;
	EIterator *iter;
	CamelException ex;

	if (!em_utils_prompt_user((GtkWindow *) parent, "/apps/evolution/mail/prompts/empty_trash", "mail:ask-empty-trash", NULL))
		return;

	camel_exception_init (&ex);

	/* expunge all remote stores */
	accounts = e_get_account_list ();
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		/* make sure this is a valid source */
		if (account->enabled && account->source->url) {
			provider = camel_provider_get(account->source->url, &ex);
			if (provider) {
				/* make sure this store is a remote store */
				if (provider->flags & CAMEL_PROVIDER_IS_STORAGE &&
				    provider->flags & CAMEL_PROVIDER_IS_REMOTE) {
					mail_empty_trash (account, NULL, NULL);
				}
			}

			/* clear the exception for the next round */
			camel_exception_clear (&ex);
		}

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	/* Now empty the local trash folder */
	mail_empty_trash (NULL, NULL, NULL);
}

gchar *
em_utils_folder_name_from_uri (const gchar *uri)
{
	CamelURL *url;
	gchar *folder_name = NULL;

	if (uri == NULL || (url = camel_url_new (uri, NULL)) == NULL)
		return NULL;

	if (url->fragment)
		folder_name = url->fragment;
	else if (url->path)
		folder_name = url->path + 1;

	if (folder_name == NULL) {
		camel_url_free (url);
		return NULL;
	}

	folder_name = g_strdup (folder_name);
	camel_url_free (url);

	return folder_name;
}

/* email: uri's are based on the account, with special cases for local
 * stores, vfolder and local mail.
 * e.g.
 *  imap account imap://user@host/ -> email://accountid@accountid.host/
 *  vfolder      vfolder:/storage/path#folder -> email://vfolder@local/folder
 *  local        local:/storage/path#folder   -> email://local@local/folder
 */

gchar *em_uri_from_camel(const gchar *curi)
{
	CamelURL *curl;
	EAccount *account;
	const gchar *uid, *path;
	gchar *euri, *tmp;
	CamelProvider *provider;
	CamelException ex;

	/* Easiest solution to code that shouldnt be calling us */
	if (!strncmp(curi, "email:", 6))
		return g_strdup(curi);

	camel_exception_init(&ex);
	provider = camel_provider_get(curi, &ex);
	if (provider == NULL) {
		camel_exception_clear(&ex);
		d(printf("em uri from camel failed '%s'\n", curi));
		return g_strdup(curi);
	}

	curl = camel_url_new(curi, &ex);
	camel_exception_clear(&ex);
	if (curl == NULL)
		return g_strdup(curi);

	if (strcmp(curl->protocol, "vfolder") == 0)
		uid = "vfolder@local";
	else if ((account = mail_config_get_account_by_source_url(curi)) == NULL)
		uid = "local@local";
	else
		uid = account->uid;
	path = (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)?curl->fragment:curl->path;
	if (path) {
		if (path[0] == '/')
			path++;

		tmp = camel_url_encode(path, ";?");
		euri = g_strdup_printf("email://%s/%s", uid, tmp);
		g_free(tmp);
	} else {
		euri = g_strdup_printf("email://%s/", uid);
	}

	d(printf("em uri from camel '%s' -> '%s'\n", curi, euri));

	camel_url_free(curl);

	return euri;
}

gchar *em_uri_to_camel(const gchar *euri)
{
	EAccountList *accounts;
	const EAccount *account;
	EAccountService *service;
	CamelProvider *provider;
	CamelURL *eurl, *curl;
	gchar *uid, *curi;

	if (strncmp(euri, "email:", 6) != 0) {
		d(printf("em uri to camel not euri '%s'\n", euri));
		return g_strdup(euri);
	}

	eurl = camel_url_new(euri, NULL);
	if (eurl == NULL)
		return g_strdup(euri);

	g_return_val_if_fail (eurl->host != NULL, g_strdup(euri));

	if (eurl->user != NULL) {
		/* Sigh, shoul'dve used mbox@local for mailboxes, not local@local */
		if (strcmp(eurl->host, "local") == 0
		    && (strcmp(eurl->user, "local") == 0 || strcmp(eurl->user, "vfolder") == 0)) {
			gchar *base;

			if (strcmp(eurl->user, "vfolder") == 0)
				curl = camel_url_new("vfolder:", NULL);
			else
				curl = camel_url_new("mbox:", NULL);

			base = g_strdup_printf("%s/.evolution/mail/%s", g_get_home_dir(), eurl->user);
#ifdef G_OS_WIN32
			/* Turn backslashes into slashes to avoid URI encoding */
			{
				gchar *p = base;
				while ((p = strchr (p, '\\')))
					*p++ = '/';
			}
#endif
			camel_url_set_path(curl, base);
			g_free(base);
			camel_url_set_fragment(curl, eurl->path[0]=='/'?eurl->path+1:eurl->path);
			curi = camel_url_to_string(curl, 0);
			camel_url_free(curl);
			camel_url_free(eurl);

			d(printf("em uri to camel local '%s' -> '%s'\n", euri, curi));
			return curi;
		}

		uid = g_strdup_printf("%s@%s", eurl->user, eurl->host);
	} else {
		uid = g_strdup(eurl->host);
	}

	accounts = e_get_account_list ();
	account = e_account_list_find(accounts, E_ACCOUNT_FIND_UID, uid);
	g_free(uid);

	if (account == NULL) {
		camel_url_free(eurl);
		d(printf("em uri to camel no account '%s' -> '%s'\n", euri, euri));
		return g_strdup(euri);
	}

	service = account->source;
	if (!(provider = camel_provider_get (service->url, NULL)))
		return g_strdup (euri);

	curl = camel_url_new(service->url, NULL);
	if (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		camel_url_set_fragment(curl, eurl->path[0]=='/'?eurl->path+1:eurl->path);
	else
		camel_url_set_path(curl, eurl->path);

	curi = camel_url_to_string(curl, 0);

	camel_url_free(eurl);
	camel_url_free(curl);

	d(printf("em uri to camel '%s' -> '%s'\n", euri, curi));

	return curi;
}

/* ********************************************************************** */
#include <libebook/e-book.h>

struct _addr_node {
	gchar *addr;
	time_t stamp;
	gint found;
};

#define EMU_ADDR_CACHE_TIME (60*30) /* in seconds */

static pthread_mutex_t emu_addr_lock = PTHREAD_MUTEX_INITIALIZER;
static ESourceList *emu_addr_list;
static GHashTable *emu_addr_cache;

/* runs sync, in main thread */
static gpointer
emu_addr_setup(gpointer dummy)
{
	GError *err = NULL;

	emu_addr_cache = g_hash_table_new(g_str_hash, g_str_equal);

	if (!e_book_get_addressbooks(&emu_addr_list, &err))
		g_error_free(err);

	return NULL;
}

static void
emu_addr_cancel_book(gpointer data)
{
	EBook *book = data;
	GError *err = NULL;

	/* we dunna care if this fails, its just the best we can try */
	e_book_cancel(book, &err);
	g_clear_error(&err);
}

struct TryOpenEBookStruct {
	GError **error;
	EFlag *flag;
	gboolean result;
};

static void
try_open_e_book_cb (EBook *book, EBookStatus status, gpointer closure)
{
	struct TryOpenEBookStruct *data = (struct TryOpenEBookStruct *)closure;

	if (!data)
		return;

	data->result = status == E_BOOK_ERROR_OK;

	if (!data->result) {
		g_clear_error (data->error);
		g_set_error (data->error, E_BOOK_ERROR, status, "EBookStatus returned %d", status);
	}

	e_flag_set (data->flag);
}

/**
 * try_open_e_book:
 * Tries to open address book asynchronously, but acts as synchronous.
 * The advantage is it checks periodically whether the camel_operation
 * has been canceled or not, and if so, then stops immediately, with
 * result FALSE. Otherwise returns same as e_book_open
 **/
static gboolean
try_open_e_book (EBook *book, gboolean only_if_exists, GError **error)
{
	struct TryOpenEBookStruct data;
	gboolean canceled = FALSE;
	EFlag *flag = e_flag_new ();

	data.error = error;
	data.flag = flag;
	data.result = FALSE;

	if (e_book_async_open (book, only_if_exists, try_open_e_book_cb, &data) != FALSE) {
		e_flag_free (flag);
		g_clear_error (error);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_OTHER_ERROR, "Failed to call e_book_async_open.");
		return FALSE;
	}

	while (canceled = camel_operation_cancel_check (NULL), !canceled && !e_flag_is_set (flag)) {
		GTimeVal wait;

		g_get_current_time (&wait);
		g_time_val_add (&wait, 250000); /* waits 250ms */

		e_flag_timed_wait (flag, &wait);
	}

	if (canceled) {
		g_clear_error (error);
		g_set_error (error, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED, "Operation has been canceled.");
		e_book_cancel_async_op (book, NULL);
		/* it had been canceled, the above callback may not be called, thus setting flag here */
		e_flag_set (flag);
	}

	e_flag_wait (flag);
	e_flag_free (flag);

	return data.result && (!error || !*error);
}

static gboolean
is_local (ESourceGroup *group)
{
	return group &&
		e_source_group_peek_base_uri (group) &&
		g_str_has_prefix (e_source_group_peek_base_uri (group), "file://");
}

gboolean
em_utils_in_addressbook (CamelInternetAddress *iaddr, gboolean local_only)
{
	GError *err = NULL;
	GSList *s, *g, *addr_sources = NULL;
	gint stop = FALSE, found = FALSE;
	EBookQuery *query;
	const gchar *addr;
	struct _addr_node *node;
	time_t now;

	/* TODO: check all addresses? */
	if (iaddr == NULL
	    || !camel_internet_address_get(iaddr, 0, NULL, &addr))
		return FALSE;

	pthread_mutex_lock(&emu_addr_lock);

	if (emu_addr_cache == NULL) {
		mail_call_main(MAIL_CALL_p_p, (MailMainFunc)emu_addr_setup, NULL);
	}

	if (emu_addr_list == NULL) {
		pthread_mutex_unlock(&emu_addr_lock);
		return FALSE;
	}

	now = time(NULL);

	d(printf("Checking '%s' is in addressbook", addr));

	node = g_hash_table_lookup(emu_addr_cache, addr);
	if (node) {
		d(printf(" -> cached, found %s\n", node->found?"yes":"no"));
		if (node->stamp + EMU_ADDR_CACHE_TIME > now) {
			found = node->found;
			pthread_mutex_unlock(&emu_addr_lock);
			return found;
		}
		d(printf("    but expired!\n"));
	} else {
		d(printf(" -> not found in cache\n"));
		node = g_malloc0(sizeof(*node));
		node->addr = g_strdup(addr);
		g_hash_table_insert(emu_addr_cache, node->addr, node);
	}

	query = e_book_query_field_test(E_CONTACT_EMAIL, E_BOOK_QUERY_IS, addr);

	/* FIXME: this aint threadsafe by any measure, but what can you do eh??? */

	for (g = e_source_list_peek_groups(emu_addr_list);g;g=g_slist_next(g)) {
		if (local_only &&  !is_local (g->data))
			continue;

		for (s = e_source_group_peek_sources((ESourceGroup *)g->data);s;s=g_slist_next(s)) {
			ESource *src = s->data;
			const gchar *completion = e_source_get_property (src, "completion");

			if (completion && !g_ascii_strcasecmp (completion, "true")) {
				addr_sources = g_slist_prepend(addr_sources, src);
				g_object_ref(src);
			}
		}
	}

	for (s = addr_sources;!stop && !found && s;s=g_slist_next(s)) {
		ESource *source = s->data;
		GList *contacts;
		EBook *book;
		GHook *hook;

		d(printf(" checking '%s'\n", e_source_get_uri(source)));

		/* could this take a while?  no way to cancel it? */
		book = e_book_new(source, &err);

		if (book == NULL) {
			if (err && !g_error_matches (err, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED))
				g_warning ("%s: Unable to create addressbook: %s", G_STRFUNC, err->message);
			g_clear_error(&err);
			continue;
		}

		g_clear_error(&err);

		hook = mail_cancel_hook_add(emu_addr_cancel_book, book);

		/* ignore errors, but cancellation errors we don't try to go further either */
		if (!try_open_e_book (book, TRUE, &err)
		    || !e_book_get_contacts(book, query, &contacts, &err)) {
			stop = err && g_error_matches (err, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED);
			mail_cancel_hook_remove(hook);
			g_object_unref(book);
			if (err && !stop)
				g_warning ("%s: Can't get contacts: %s", G_STRFUNC, err->message);
			g_clear_error(&err);
			continue;
		}

		mail_cancel_hook_remove(hook);

		if (contacts != NULL) {
			found = TRUE;
			g_list_foreach(contacts, (GFunc)g_object_unref, NULL);
			g_list_free(contacts);
		}

		stop = stop || camel_operation_cancel_check (NULL);

		d(printf(" %s\n", stop?"found":"not found"));

		g_object_unref(book);
	}

	g_slist_free(addr_sources);

	if (!stop) {
		node->found = found;
		node->stamp = now;
	}

	e_book_query_unref(query);

	pthread_mutex_unlock(&emu_addr_lock);

	return found;
}

CamelMimePart *
em_utils_contact_photo (CamelInternetAddress *cia, gboolean local)
{
	const gchar *addr;
	gint stop = FALSE, found = FALSE;
	GSList *s, *g, *addr_sources = NULL;
	GError *err = NULL;
        EBookQuery *query = NULL;
	ESource *source = NULL;
	GList *contacts = NULL;
	EContact *contact = NULL;
	EContactPhoto *photo = NULL;
	EBook *book = NULL;
	CamelMimePart *part;

	if (cia == NULL || !camel_internet_address_get(cia, 0, NULL, &addr)) {
		return NULL;
	}

	if (!emu_addr_list) {
		if (!e_book_get_addressbooks(&emu_addr_list, &err)) {
			g_error_free(err);
			return NULL;
		}
	}

	query = e_book_query_field_test(E_CONTACT_EMAIL, E_BOOK_QUERY_IS, addr);
	for (g = e_source_list_peek_groups(emu_addr_list); g; g = g_slist_next(g)) {
		if (local && !is_local (g->data))
			continue;

		for (s = e_source_group_peek_sources((ESourceGroup *)g->data); s; s=g_slist_next(s)) {
			ESource *src = s->data;
			const gchar *completion = e_source_get_property (src, "completion");

			if (completion && !g_ascii_strcasecmp (completion, "true")) {
				addr_sources = g_slist_prepend(addr_sources, src);
				g_object_ref(src);
			}
		}
	}

	for (s = addr_sources;!stop && !found && s;s=g_slist_next(s)) {
		source = s->data;

		book = e_book_new(source, &err);
		if (!book) {
			if (err && !g_error_matches (err, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED))
				g_warning ("%s: Unable to create addressbook: %s", G_STRFUNC, err->message);
			g_clear_error (&err);
			continue;
		}

		g_clear_error (&err);

		if (!try_open_e_book (book, TRUE, &err)
		    || !e_book_get_contacts(book, query, &contacts, &err)) {
			stop = err && g_error_matches (err, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED);
			g_object_unref(book);
			if (err && !stop)
				g_warning ("%s: Can't get contacts: %s", G_STRFUNC, err->message);
			g_clear_error(&err);
			continue;
		}
		g_clear_error (&err);

		if (contacts != NULL) {
			found = TRUE;

			/* Doesn't matter, we consider the first contact only*/
			contact = contacts->data;
			photo = e_contact_get (contact, E_CONTACT_PHOTO);
			if (!photo)
				photo = e_contact_get (contact, E_CONTACT_LOGO);
			g_list_foreach (contacts, (GFunc)g_object_unref, NULL);
			g_list_free (contacts);
		}

		stop = stop || camel_operation_cancel_check (NULL);

		g_object_unref (source); /* Is it? */
		g_object_unref(book);
	}

	g_slist_free(addr_sources);
	e_book_query_unref(query);

	if (!photo)
		return NULL;

	if (photo->type != E_CONTACT_PHOTO_TYPE_INLINED) {
		e_contact_photo_free (photo);
		return NULL;
	}

	/* Form a mime part out of the photo */
	part = camel_mime_part_new();
	camel_mime_part_set_content(part,
				    (const gchar *) photo->data.inlined.data,
				    photo->data.inlined.length, "image/jpeg");

	e_contact_photo_free (photo);

	return part;
}

void
em_utils_clear_get_password_canceled_accounts_flag (void)
{
	EAccountList *accounts;

	accounts = e_get_account_list ();
	if (accounts) {
		EIterator *iter;

		for (iter = e_list_get_iterator ((EList *) accounts);
		     e_iterator_is_valid (iter);
		     e_iterator_next (iter)) {
			EAccount *account = (EAccount *) e_iterator_get (iter);

			if (account && account->source)
				account->source->get_password_canceled = FALSE;

			if (account && account->transport)
				account->transport->get_password_canceled = FALSE;
		}

		g_object_unref (iter);
	}
}

void
em_utils_show_error_silent (GtkWidget *widget)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EActivity *activity;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	activity = e_alert_activity_new_warning (widget);
	e_shell_backend_add_activity (shell_backend, activity);
	g_object_unref (activity);

	if (g_object_get_data (G_OBJECT (widget), "response-handled") == NULL)
		g_signal_connect (
			widget, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);
}

void
em_utils_show_info_silent (GtkWidget *widget)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EActivity *activity;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	activity = e_alert_activity_new_info (widget);
	e_shell_backend_add_activity (shell_backend, activity);
	g_object_unref (activity);

	if (g_object_get_data (G_OBJECT (widget), "response-handled") == NULL)
		g_signal_connect (
			widget, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);
}

gchar *
em_utils_url_unescape_amp (const gchar *url)
{
	gchar *buff;
	gint i, j, amps;

	if (!url)
		return NULL;

	amps = 0;
	for (i = 0; url [i]; i++) {
		if (url [i] == '&' && strncmp (url + i, "&amp;", 5) == 0)
			amps++;
	}

	buff = g_strdup (url);

	if (!amps)
		return buff;

	for (i = 0, j = 0; url [i]; i++, j++) {
		buff [j] = url [i];

		if (url [i] == '&' && strncmp (url + i, "&amp;", 5) == 0)
			i += 4;
	}
	buff [j] = 0;

	return buff;
}
