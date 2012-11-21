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

#include <glib/gstdio.h>

#ifdef G_OS_WIN32
/* Work around namespace clobbage in <windows.h> */
#define DATADIR windows_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include "em-filter-editor.h"

#include <glib/gi18n.h>

#include <libebook/libebook.h>

#include <e-util/e-util.h>
#include <e-util/e-util-private.h>
#include <e-util/e-mktemp.h>
#include <e-util/e-dialog-utils.h>
#include <libevolution-utils/e-alert-dialog.h>

#include <shell/e-shell.h>
#include <widgets/misc/e-attachment.h>

#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-formatter-quote.h>

#include <libemail-utils/mail-mt.h>

#include <libemail-engine/e-mail-folder-utils.h>
#include <libemail-engine/e-mail-session.h>
#include <libemail-engine/mail-ops.h>
#include <libemail-engine/mail-tools.h>

#include "e-mail-tag-editor.h"
#include "em-composer-utils.h"
#include "em-utils.h"
#include "e-mail-printer.h"

/* XXX This is a dirty hack on a dirty hack.  We really need
#include <em-format/e-mail-print-formatter.h>
 *     to rework or get rid of the functions that use this. */
extern const gchar *shell_builtin_backend;

/* How many is too many? */
/* Used in em_util_ask_open_many() */
#define TOO_MANY 10

/* drag and drop resulting file naming possibilities */
enum {
	DND_USE_SENT_DATE = 1, /* YYYYMMDDhhmmssms_<title> and use email sent date */
	DND_USE_DND_DATE  = 2,  /*  YYYYMMDDhhmmssms_<title> and drag'drop date */
};

#define d(x)

gboolean
em_utils_ask_open_many (GtkWindow *parent,
                        gint how_many)
{
	gchar *string;
	gboolean proceed;

	if (how_many < TOO_MANY)
		return TRUE;

	string = g_strdup_printf (ngettext (
		/* Translators: This message is shown only for ten or more
		 * messages to be opened.  The %d is replaced with the actual
		 * count of messages. If you need a '%' in your text, then
		 * write it doubled, like '%%'. */
		"Are you sure you want to open %d message at once?",
		"Are you sure you want to open %d messages at once?",
		how_many), how_many);
	proceed = em_utils_prompt_user (
		parent, "prompt-on-open-many",
		"mail:ask-open-many", string, NULL);
	g_free (string);

	return proceed;
}

/**
 * em_utils_prompt_user:
 * @parent: parent window
 * @promptkey: settings key to check if we should prompt the user or not.
 * @tag: e_alert tag.
 *
 * Convenience function to query the user with a Yes/No dialog and a
 * "Do not show this dialog again" checkbox. If the user checks that
 * checkbox, then @promptkey is set to %FALSE, otherwise it is set to
 * %TRUE.
 *
 * Returns %TRUE if the user clicks Yes or %FALSE otherwise.
 **/
gboolean
em_utils_prompt_user (GtkWindow *parent,
                      const gchar *promptkey,
                      const gchar *tag,
                      ...)
{
	GtkWidget *dialog;
	GtkWidget *check = NULL;
	GtkWidget *container;
	va_list ap;
	gint button;
	GSettings *settings;
	EAlert *alert = NULL;

	settings = g_settings_new ("org.gnome.evolution.mail");

	if (promptkey && !g_settings_get_boolean (settings, promptkey)) {
		g_object_unref (settings);
		return TRUE;
	}

	va_start (ap, tag);
	alert = e_alert_new_valist (tag, ap);
	va_end (ap);

	dialog = e_alert_dialog_new (parent, alert);
	g_object_unref (alert);

	container = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));

	if (promptkey) {
		check = gtk_check_button_new_with_mnemonic (
			_("_Do not show this message again"));
		gtk_box_pack_start (
			GTK_BOX (container), check, FALSE, FALSE, 0);
		gtk_widget_show (check);
	}

	button = gtk_dialog_run (GTK_DIALOG (dialog));
	if (promptkey)
		g_settings_set_boolean (
			settings, promptkey,
			!gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (check)));

	gtk_widget_destroy (dialog);

	g_object_unref (settings);

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

/* Editing Filters/Search Folders... */

static GtkWidget *filter_editor = NULL;

static void
em_filter_editor_response (GtkWidget *dialog,
                           gint button,
                           gpointer user_data)
{
	EMFilterContext *fc;

	if (button == GTK_RESPONSE_OK) {
		const gchar *config_dir;
		gchar *user;

		config_dir = mail_session_get_config_dir ();
		fc = g_object_get_data ((GObject *) dialog, "context");
		user = g_build_filename (config_dir, "filters.xml", NULL);
		e_rule_context_save ((ERuleContext *) fc, user);
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
 * @session: an #EMailSession
 * @alert_sink: an #EAlertSink
 * @parent_window: a parent #GtkWindow
 *
 * Opens or raises the filters editor dialog so that the user may edit
 * his/her filters. If @parent is non-NULL, then the dialog will be
 * created as a child window of @parent's toplevel window.
 **/
void
em_utils_edit_filters (EMailSession *session,
                       EAlertSink *alert_sink,
                       GtkWindow *parent_window)
{
	const gchar *config_dir;
	gchar *user, *system;
	EMFilterContext *fc;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (E_IS_ALERT_SINK (alert_sink));

	if (filter_editor) {
		gtk_window_present (GTK_WINDOW (filter_editor));
		return;
	}

	config_dir = mail_session_get_config_dir ();

	fc = em_filter_context_new (session);
	user = g_build_filename (config_dir, "filters.xml", NULL);
	system = g_build_filename (EVOLUTION_PRIVDATADIR, "filtertypes.xml", NULL);
	e_rule_context_load ((ERuleContext *) fc, system, user);
	g_free (user);
	g_free (system);

	if (((ERuleContext *) fc)->error) {
		e_alert_submit (
			alert_sink,
			"mail:filter-load-error",
			((ERuleContext *) fc)->error, NULL);
		return;
	}

	if (em_filter_source_element_names[0].name == NULL) {
		em_filter_source_element_names[0].name = _("Incoming");
		em_filter_source_element_names[1].name = _("Outgoing");
	}

	filter_editor = (GtkWidget *) em_filter_editor_new (
		fc, em_filter_source_element_names);

	if (GTK_IS_WINDOW (parent_window))
		gtk_window_set_transient_for (
			GTK_WINDOW (filter_editor), parent_window);

	gtk_window_set_title (
		GTK_WINDOW (filter_editor), _("Message Filters"));
	g_object_set_data_full (
		G_OBJECT (filter_editor), "context", fc,
		(GDestroyNotify) g_object_unref);
	g_signal_connect (
		filter_editor, "response",
		G_CALLBACK (em_filter_editor_response), NULL);
	gtk_widget_show (GTK_WIDGET (filter_editor));
}

/*
 * Picked this from e-d-s/libedataserver/e-data.
 * But it allows more characters to occur in filenames, especially
 * when saving attachment.
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
		if (!g_unichar_isprint (c) || (c < 0xff && strchr (unsafe_chars, c&0xff))) {
			while (ts < p)
				*ts++ = '_';
		}
	}
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
	EShell *shell;
	EMailBackend *backend;
	EShellSettings *shell_settings;
	EShellBackend *shell_backend;
	EMailDisplay *display;
	GtkWidget *editor;
	GtkWindow *window;
	CamelTag *tags;
	gint i;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	window = e_mail_reader_get_window (reader);
	backend = e_mail_reader_get_backend (reader);

	editor = e_mail_tag_editor_new ();
	gtk_window_set_transient_for (GTK_WINDOW (editor), window);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	/* These settings come from the calendar module. */
	g_object_bind_property (
		shell_settings, "cal-use-24-hour-format",
		editor, "use-24-hour-format",
		G_BINDING_SYNC_CREATE);
	g_object_bind_property (
		shell_settings, "cal-week-start-day",
		editor, "week-start-day",
		G_BINDING_SYNC_CREATE);

	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (folder, uids->pdata[i]);

		if (info == NULL)
			continue;

		e_mail_tag_editor_add_message (
			E_MAIL_TAG_EDITOR (editor),
			camel_message_info_from (info),
			camel_message_info_subject (info));

		camel_folder_free_message_info (folder, info);
	}

	/* special-case... */
	if (uids->len == 1) {
		CamelMessageInfo *info;
		const gchar *message_uid;

		message_uid = g_ptr_array_index (uids, 0);
		info = camel_folder_get_message_info (folder, message_uid);
		if (info) {
			tags = (CamelTag *) camel_message_info_user_tags (info);

			if (tags)
				e_mail_tag_editor_set_tag_list (
					E_MAIL_TAG_EDITOR (editor), tags);
			camel_folder_free_message_info (folder, info);
		}
	}

	if (gtk_dialog_run (GTK_DIALOG (editor)) != GTK_RESPONSE_OK)
		goto exit;

	tags = e_mail_tag_editor_get_tag_list (E_MAIL_TAG_EDITOR (editor));
	if (tags == NULL)
		goto exit;

	camel_folder_freeze (folder);
	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *info;
		CamelTag *iter;

		info = camel_folder_get_message_info (folder, uids->pdata[i]);

		if (info == NULL)
			continue;

		for (iter = tags; iter != NULL; iter = iter->next)
			camel_message_info_set_user_tag (
				info, iter->name, iter->value);

		camel_folder_free_message_info (folder, info);
	}

	camel_folder_thaw (folder);
	camel_tag_list_free (&tags);

	display = e_mail_reader_get_mail_display (reader);
	e_mail_display_reload (display);

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
em_utils_flag_for_followup_clear (GtkWindow *parent,
                                  CamelFolder *folder,
                                  GPtrArray *uids)
{
	gint i;

	g_return_if_fail (GTK_IS_WINDOW (parent));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	camel_folder_freeze (folder);
	for (i = 0; i < uids->len; i++) {
		CamelMessageInfo *mi = camel_folder_get_message_info (folder, uids->pdata[i]);

		if (mi) {
			camel_message_info_set_user_tag (mi, "follow-up", NULL);
			camel_message_info_set_user_tag (mi, "due-by", NULL);
			camel_message_info_set_user_tag (mi, "completed-on", NULL);
			camel_folder_free_message_info (folder, mi);
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
em_utils_flag_for_followup_completed (GtkWindow *parent,
                                      CamelFolder *folder,
                                      GPtrArray *uids)
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
		CamelMessageInfo *mi = camel_folder_get_message_info (folder, uids->pdata[i]);

		if (mi) {
			tag = camel_message_info_user_tag (mi, "follow-up");
			if (tag && tag[0])
				camel_message_info_set_user_tag (mi, "completed-on", now);
			camel_folder_free_message_info (folder, mi);
		}
	}

	camel_folder_thaw (folder);

	g_free (now);

	em_utils_uids_free (uids);
}

/* This kind of sucks, because for various reasons most callers need to run
 * synchronously in the gui thread, however this could take a long, blocking
 * time to run. */
static gint
em_utils_write_messages_to_stream (CamelFolder *folder,
                                   GPtrArray *uids,
                                   CamelStream *stream)
{
	CamelStream *filtered_stream;
	CamelMimeFilter *from_filter;
	gint i, res = 0;

	from_filter = camel_mime_filter_from_new ();
	filtered_stream = camel_stream_filter_new (stream);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream), from_filter);
	g_object_unref (from_filter);

	for (i = 0; i < uids->len; i++) {
		CamelMimeMessage *message;
		gchar *from;

		/* FIXME camel_folder_get_message_sync() may block. */
		message = camel_folder_get_message_sync (
			folder, uids->pdata[i], NULL, NULL);
		if (message == NULL) {
			res = -1;
			break;
		}

		/* We need to flush after each stream write since we are
		 * writing to the same stream. */
		from = camel_mime_message_build_mbox_from (message);

		if (camel_stream_write_string (stream, from, NULL, NULL) == -1
		    || camel_stream_flush (stream, NULL, NULL) == -1
		    || camel_data_wrapper_write_to_stream_sync (
			(CamelDataWrapper *) message, (CamelStream *)
			filtered_stream, NULL, NULL) == -1
		    || camel_stream_flush (
			(CamelStream *) filtered_stream, NULL, NULL) == -1)
			res = -1;

		g_free (from);
		g_object_unref (message);

		if (res == -1)
			break;
	}

	g_object_unref (filtered_stream);

	return res;
}

static void
do_print_msg_to_file (GObject *source,
                      GAsyncResult *result,
                      gpointer user_data)
{
	EMailParser *parser;
	EMailPartList *parts_list;
	gchar *filename = user_data;
	EMailPrinter *printer;

	parser = E_MAIL_PARSER (source);
	parts_list = e_mail_parser_parse_finish (parser, result, NULL);

	printer = e_mail_printer_new (parts_list);
	e_mail_printer_set_export_filename (printer, filename);
	g_signal_connect_swapped (
		printer, "done",
		G_CALLBACK (g_object_unref), printer);

	e_mail_printer_print (printer, GTK_PRINT_OPERATION_ACTION_EXPORT, NULL);

	g_object_unref (parser);
}

static gboolean
em_utils_print_messages_to_file (CamelFolder *folder,
                                 const gchar *uid,
                                 const gchar *filename)
{
	EMailParser *parser;
	CamelMimeMessage *message;
	CamelStore *parent_store;
	CamelSession *session;

	message = camel_folder_get_message_sync (folder, uid, NULL, NULL);
	if (message == NULL)
		return FALSE;

	parent_store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (parent_store));

	parser = e_mail_parser_new (session);

	e_mail_parser_parse (
		parser, folder, uid, message,
		(GAsyncReadyCallback) do_print_msg_to_file,
		NULL, g_strdup (filename));

	return TRUE;
}

/* This kind of sucks, because for various reasons most callers need to run
 * synchronously in the gui thread, however this could take a long, blocking
 * time to run. */
static gint
em_utils_read_messages_from_stream (CamelFolder *folder,
                                    CamelStream *stream)
{
	CamelMimeParser *mp = camel_mime_parser_new ();
	gboolean success = TRUE;

	camel_mime_parser_scan_from (mp, TRUE);
	camel_mime_parser_init_with_stream (mp, stream, NULL);

	while (camel_mime_parser_step (mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMimeMessage *msg;
		gboolean success;

		/* NB: de-from filter, once written */
		msg = camel_mime_message_new ();
		if (!camel_mime_part_construct_from_parser_sync (
			(CamelMimePart *) msg, mp, NULL, NULL)) {
			g_object_unref (msg);
			break;
		}

		/* FIXME camel_folder_append_message_sync() may block. */
		success = camel_folder_append_message_sync (
			folder, msg, NULL, NULL, NULL, NULL);
		g_object_unref (msg);

		if (!success)
			break;

		camel_mime_parser_step (mp, NULL, NULL);
	}

	g_object_unref (mp);

	return success ? 0 : -1;
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
em_utils_selection_set_mailbox (GtkSelectionData *data,
                                CamelFolder *folder,
                                GPtrArray *uids)
{
	GByteArray *byte_array;
	CamelStream *stream;
	GdkAtom target;

	target = gtk_selection_data_get_target (data);

	byte_array = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (byte_array);

	if (em_utils_write_messages_to_stream (folder, uids, stream) == 0)
		gtk_selection_data_set (
			data, target, 8,
			byte_array->data, byte_array->len);

	g_object_unref (stream);
}

/**
 * em_utils_selection_get_mailbox:
 * @selection_data: selection data
 * @folder:
 *
 * Receive a mailbox selection/dnd
 * Warning: Could be BIG!
 * Warning: This could block the ui for an extended period.
 * FIXME: Exceptions?
 **/
void
em_utils_selection_get_mailbox (GtkSelectionData *selection_data,
                                CamelFolder *folder)
{
	CamelStream *stream;
	const guchar *data;
	gint length;

	data = gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	if (data == NULL || length == -1)
		return;

	/* TODO: a stream mem with read-only access to existing data? */
	/* NB: Although copying would let us run this async ... which it should */
	stream = (CamelStream *)
		camel_stream_mem_new_with_buffer ((gchar *) data, length);
	em_utils_read_messages_from_stream (folder, stream);
	g_object_unref (stream);
}

/**
 * em_utils_selection_get_message:
 * @selection_data:
 * @folder:
 *
 * get a message/rfc822 data.
 **/
void
em_utils_selection_get_message (GtkSelectionData *selection_data,
                                CamelFolder *folder)
{
	CamelStream *stream;
	CamelMimeMessage *msg;
	const guchar *data;
	gint length;

	data = gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	if (data == NULL || length == -1)
		return;

	stream = (CamelStream *)
		camel_stream_mem_new_with_buffer ((gchar *) data, length);
	msg = camel_mime_message_new ();
	if (camel_data_wrapper_construct_from_stream_sync (
		(CamelDataWrapper *) msg, stream, NULL, NULL))
		/* FIXME camel_folder_append_message_sync() may block. */
		camel_folder_append_message_sync (
			folder, msg, NULL, NULL, NULL, NULL);
	g_object_unref (msg);
	g_object_unref (stream);
}

/**
 * em_utils_selection_set_uidlist:
 * @selection_data: selection data
 * @folder:
 * @uids:
 *
 * Sets a "x-uid-list" format selection data.
 **/
void
em_utils_selection_set_uidlist (GtkSelectionData *selection_data,
                                CamelFolder *folder,
                                GPtrArray *uids)
{
	GByteArray *array = g_byte_array_new ();
	GdkAtom target;
	gchar *folder_uri;
	gint ii;

	/* format: "uri1\0uid1\0uri2\0uid2\0...\0urin\0uidn\0" */

	if (CAMEL_IS_VEE_FOLDER (folder) &&
	    CAMEL_IS_VEE_STORE (camel_folder_get_parent_store (folder))) {
		CamelVeeFolder *vfolder = CAMEL_VEE_FOLDER (folder);
		CamelFolder *real_folder;
		CamelMessageInfo *info;
		gchar *real_uid;

		for (ii = 0; ii < uids->len; ii++) {
			info = camel_folder_get_message_info (folder, uids->pdata[ii]);
			if (!info) {
				g_warn_if_reached ();
				continue;
			}

			real_folder = camel_vee_folder_get_location (
				vfolder, (CamelVeeMessageInfo *) info, &real_uid);

			if (real_folder) {
				folder_uri = e_mail_folder_uri_from_folder (real_folder);

				g_byte_array_append (array, (guchar *) folder_uri, strlen (folder_uri) + 1);
				g_byte_array_append (array, (guchar *) real_uid, strlen (real_uid) + 1);

				g_free (folder_uri);
			}

			camel_folder_free_message_info (folder, info);
		}
	} else {
		folder_uri = e_mail_folder_uri_from_folder (folder);

		for (ii = 0; ii < uids->len; ii++) {
			g_byte_array_append (array, (guchar *) folder_uri, strlen (folder_uri) + 1);
			g_byte_array_append (array, uids->pdata[ii], strlen (uids->pdata[ii]) + 1);
		}

		g_free (folder_uri);
	}

	target = gtk_selection_data_get_target (selection_data);
	gtk_selection_data_set (
		selection_data, target, 8, array->data, array->len);
	g_byte_array_free (array, TRUE);
}

/**
 * em_utils_selection_get_uidlist:
 * @data: selection data
 * @session: an #EMailSession
 * @move: do we delete the messages.
 *
 * Convert a uid list into a copy/move operation.
 *
 * Warning: Could take some time to run.
 **/
void
em_utils_selection_get_uidlist (GtkSelectionData *selection_data,
                                EMailSession *session,
                                CamelFolder *dest,
                                gint move,
                                GCancellable *cancellable,
                                GError **error)
{
	/* format: "uri1\0uid1\0uri2\0uid2\0...\0urin\0uidn\0" */
	gchar *inptr, *inend;
	GPtrArray *items;
	CamelFolder *folder;
	const guchar *data;
	gint length, ii;
	GHashTable *uids_by_uri;
	GHashTableIter iter;
	gpointer key, value;
	GError *local_error = NULL;

	g_return_if_fail (selection_data != NULL);
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	data = gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	if (data == NULL || length == -1)
		return;

	items = g_ptr_array_new ();

	inptr = (gchar *) data;
	inend = (gchar *) (data + length);
	while (inptr < inend) {
		gchar *start = inptr;

		while (inptr < inend && *inptr)
			inptr++;

		g_ptr_array_add (items, g_strndup (start, inptr - start));

		inptr++;
	}

	if (items->len == 0) {
		g_ptr_array_free (items, TRUE);
		return;
	}

	uids_by_uri = g_hash_table_new (g_str_hash, g_str_equal);
	for (ii = 0; ii < items->len - 1; ii += 2) {
		gchar *uri, *uid;
		GPtrArray *uids;

		uri = items->pdata[ii];
		uid = items->pdata[ii + 1];

		uids = g_hash_table_lookup (uids_by_uri, uri);
		if (!uids) {
			uids = g_ptr_array_new ();
			g_hash_table_insert (uids_by_uri, uri, uids);
		}

		/* reuse uid pointer from uids, do not strdup it */
		g_ptr_array_add (uids, uid);
	}

	g_hash_table_iter_init (&iter, uids_by_uri);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		const gchar *uri = key;
		GPtrArray *uids = value;

		if (!local_error) {
			/* FIXME e_mail_session_uri_to_folder_sync() may block. */
			folder = e_mail_session_uri_to_folder_sync (
				session, uri, 0, cancellable, &local_error);
			if (folder) {
				/* FIXME camel_folder_transfer_messages_to_sync() may block. */
				camel_folder_transfer_messages_to_sync (
					folder, uids, dest, move, NULL, cancellable, &local_error);
				g_object_unref (folder);
			}
		}

		g_ptr_array_free (uids, TRUE);
	}

	g_hash_table_destroy (uids_by_uri);
	em_utils_uids_free (items);

	if (local_error)
		g_propagate_error (error, local_error);
}

static gchar *
em_utils_build_export_filename (CamelFolder *folder,
                                const gchar *uid,
                                const gchar *exporttype,
                                gint exportname,
                                const gchar *tmpdir)
{
	CamelMessageInfo *info;
	gchar *file = NULL;
	gchar *tmpfile = NULL;
	struct tm  *ts;
	gchar datetmp[15];

	/* Try to get the drop filename from the message or folder */
	info = camel_folder_get_message_info (folder, uid);
	if (info) {
		if (camel_message_info_subject (info)) {
			time_t reftime;
			reftime = camel_message_info_date_sent (info);
			if (exportname == DND_USE_DND_DATE) {
				reftime = time (NULL);
			}

			ts = localtime (&reftime);
			strftime (datetmp, 15, "%Y%m%d%H%M%S", ts);

			if (g_ascii_strcasecmp (exporttype, "pdf") == 0)
				file = g_strdup_printf ("%s_%s.pdf", datetmp, camel_message_info_subject (info));
			else
				file = g_strdup_printf ("%s_%s", datetmp, camel_message_info_subject (info));

		}
		camel_folder_free_message_info (folder, info);
	} else {
		time_t reftime;
		reftime = time (NULL);
		ts = localtime (&reftime);
		strftime (datetmp, 15, "%Y%m%d%H%M%S", ts);
		if (g_ascii_strcasecmp (exporttype, "pdf") == 0)
			file = g_strdup_printf ("%s_Untitled Message.pdf", datetmp);
		else
			file = g_strdup_printf ("%s_Untitled Message", datetmp);

	}

	if (file != NULL) {
		e_filename_make_safe (file);
		tmpfile = g_build_filename (tmpdir, file, NULL);
		g_free (file);
	}

	return tmpfile;
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
em_utils_selection_set_urilist (GtkSelectionData *data,
                                CamelFolder *folder,
                                GPtrArray *uids)
{
	gchar *tmpdir;
	gchar *uri;
	gint fd;
	GSettings *settings;
	gchar *exporttype;
	gint exportname;

	tmpdir = e_mkdtemp ("drag-n-drop-XXXXXX");
	if (tmpdir == NULL)
		return;

	settings = g_settings_new ("org.gnome.evolution.mail");
	exporttype = g_settings_get_string (settings, "drag-and-drop-save-file-format");
	if (g_strcmp0 (exporttype, "mbox") != 0 && g_strcmp0 (exporttype, "pdf") != 0) {
		g_free (exporttype);
		exporttype = g_strdup ("mbox");
	}
	exportname = g_settings_get_int (settings, "drag-and-drop-save-name-format");
	g_object_unref (settings);

	if (g_ascii_strcasecmp (exporttype, "mbox") == 0) {
		gchar * file = NULL;
		CamelStream *fstream;

		if (uids->len > 1) {
			gchar * tmp = g_strdup_printf (_("Messages from %s"), camel_folder_get_display_name (folder));
			e_filename_make_safe (tmp);
			file = g_build_filename (tmpdir, tmp, NULL);
			g_free (tmp);
		} else {
			file = em_utils_build_export_filename (folder, uids->pdata[0], exporttype, exportname, tmpdir);
		}

		fd = g_open (file, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0666);
		if (fd == -1) {
			g_free (file);
			g_free (exporttype);
			g_free (tmpdir);
			return;
		}

		uri = g_filename_to_uri (file, NULL, NULL);
		fstream = camel_stream_fs_new_with_fd (fd);
		if (fstream) {
			if (em_utils_write_messages_to_stream (folder, uids, fstream) == 0) {
				GdkAtom type;
				/* terminate with \r\n to be compliant with the spec */
				gchar *uri_crlf = g_strconcat (uri, "\r\n", NULL);

				type = gtk_selection_data_get_target (data);
				gtk_selection_data_set (data, type, 8, (guchar *) uri_crlf, strlen (uri_crlf));
				g_free (uri_crlf);
			}
			g_object_unref (fstream);
		} else
			close (fd);

		g_free (exporttype);
		g_free (tmpdir);
		g_free (file);
		g_free (uri);
	} else if (g_ascii_strcasecmp (exporttype, "pdf") == 0) {
		gchar ** filenames, **uris;
		gint i, uris_count = 0;

		filenames = g_new (gchar *, uids->len);
		uris = g_new (gchar *, uids->len + 1);
		for (i = 0; i < uids->len; i++) {
			filenames[i] = em_utils_build_export_filename (folder, uids->pdata[i], exporttype, exportname, tmpdir);
			/* validity test */
			fd = g_open (filenames[i], O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0666);
			if (fd == -1) {
				gint j;
				for (j = 0; j <= i; j++) {
					g_free (filenames[j]);
				}
				g_free (filenames);
				g_free (uris);
				g_free (tmpdir);
				g_free (exporttype);
				return;
			}
			close (fd);

			/* export */
			if (em_utils_print_messages_to_file (folder, uids->pdata[i], filenames[i])) {
				/* terminate with \r\n to be compliant with the spec */
				uri = g_filename_to_uri (filenames[i], NULL, NULL);
				uris[uris_count++] = g_strconcat (uri, "\r\n", NULL);
				g_free (uri);
			}
		}

		uris[uris_count] = NULL;
		gtk_selection_data_set_uris (data, uris);

		for (i = 0; i < uids->len; i++) {
			g_free (filenames[i]);
		}
		g_free (filenames);
		for (i = 0; i < uris_count; i++) {
			g_free (uris[i]);
		}
		g_free (uris);
		g_free (exporttype);
		g_free (tmpdir);
	} else {
		g_free (exporttype);
		g_free (tmpdir);
	}
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
em_utils_selection_get_urilist (GtkSelectionData *selection_data,
                                CamelFolder *folder)
{
	CamelStream *stream;
	CamelURL *url;
	gint fd, i, res = 0;
	gchar **uris;

	d (printf (" * drop uri list\n"));

	uris = gtk_selection_data_get_uris (selection_data);

	for (i = 0; res == 0 && uris[i]; i++) {
		g_strstrip (uris[i]);
		if (uris[i][0] == '#')
			continue;

		url = camel_url_new (uris[i], NULL);
		if (url == NULL)
			continue;

		if (strcmp (url->protocol, "file") == 0
		    && (fd = g_open (url->path, O_RDONLY | O_BINARY, 0)) != -1) {
			stream = camel_stream_fs_new_with_fd (fd);
			if (stream) {
				res = em_utils_read_messages_from_stream (folder, stream);
				g_object_unref (stream);
			} else
				close (fd);
		}
		camel_url_free (url);
	}

	g_strfreev (uris);
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

EProxy *
em_utils_get_proxy (void)
{
	g_static_mutex_lock (&emu_proxy_lock);

	if (!emu_proxy) {
		mail_call_main (MAIL_CALL_p_p, (MailMainFunc) emu_proxy_setup, NULL);
	}

	g_static_mutex_unlock (&emu_proxy_lock);

	return emu_proxy;
}

static gboolean
is_only_text_part_in_this_level (GSList *parts,
                                 EMailPart *text_html_part)
{
	const gchar *dot;
	gint level_len;
	GSList *iter;

	g_return_val_if_fail (parts != NULL, FALSE);
	g_return_val_if_fail (text_html_part != NULL, FALSE);

	dot = strrchr (text_html_part->id, '.');
	if (!dot)
		return FALSE;

	level_len = dot - text_html_part->id;
	for (iter = parts; iter; iter = iter->next) {
		EMailPart *part = iter->data;

		if (!part || !part->mime_type || part == text_html_part ||
		    part->is_hidden || part->is_attachment)
			continue;

		dot = strrchr (part->id, '.');
		if (dot - part->id != level_len ||
		    strncmp (text_html_part->id, part->id, level_len) != 0)
			continue;

		if (g_ascii_strncasecmp (part->mime_type, "text/", 5) == 0)
			return FALSE;
	}

	return TRUE;
}

/**
 * em_utils_message_to_html:
 * @session: a #CamelSession
 * @message:
 * @credits:
 * @flags: EMFormatQuote flags
 * @source:
 * @append: Text to append, can be NULL.
 * @validity_found: if not NULL, then here will be set what validities
 *         had been found during message conversion. Value is a bit OR
 *         of EM_FORMAT_VALIDITY_FOUND_* constants.
 *
 * Convert a message to html, quoting if the @credits attribution
 * string is given.
 *
 * Return value: The html version as a NULL terminated string.
 **/
gchar *
em_utils_message_to_html (CamelSession *session,
                          CamelMimeMessage *message,
                          const gchar *credits,
                          guint32 flags,
                          EMailPartList *parts_list,
                          const gchar *append,
                          guint32 *validity_found)
{
	EMailFormatter *formatter;
	EMailParser *parser = NULL;
	CamelStream *mem;
	GByteArray *buf;
	EShell *shell;
	GtkWindow *window;
	EMailPart *hidden_text_html_part = NULL;
	guint32 is_validity_found = 0;
	GSList *iter;

	shell = e_shell_get_default ();
	window = e_shell_get_active_window (shell);

	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);

	buf = g_byte_array_new ();
	mem = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (mem), buf);

	formatter = e_mail_formatter_quote_new (credits, flags);
	e_mail_formatter_set_style (formatter,
		gtk_widget_get_style (GTK_WIDGET (window)),
		gtk_widget_get_state (GTK_WIDGET (window)));

	if (!parts_list) {
		GSettings *settings;
		gchar *charset;

		/* FIXME We should be getting this from the
		 *       current view, not the global setting. */
		settings = g_settings_new ("org.gnome.evolution.mail");
		charset = g_settings_get_string (settings, "charset");
		if (charset && *charset)
			e_mail_formatter_set_default_charset (formatter, charset);
		g_object_unref (settings);
		g_free (charset);

		parser = e_mail_parser_new (session);
		parts_list = e_mail_parser_parse_sync (parser, NULL, NULL, message, NULL);
	} else {
		g_object_ref (parts_list);
	}

	/* Return all found validities and possibly show hidden prefer-plain part */
	for (iter = parts_list->list; iter; iter = iter->next) {

		EMailPart *part = iter->data;
		if (!part)
			continue;

		/* prefer-plain can hide HTML parts, even when it's the only
		 * text part in the email, thus show it (and hide again later) */
		if (part->is_hidden && !hidden_text_html_part &&
		    part->mime_type && !part->is_attachment &&
		    g_ascii_strcasecmp (part->mime_type, "text/html") == 0 &&
		    is_only_text_part_in_this_level (parts_list->list, part)) {
			part->is_hidden = FALSE;
			hidden_text_html_part = part;
		}

		if (part->validities) {
			GSList *lst;

			for (lst = part->validities; lst; lst = lst->next) {
				EMailPartValidityPair *pair = lst->data;

				if (pair)
					is_validity_found |= pair->validity_type;
			}
		}
	}

	if (validity_found)
		*validity_found = is_validity_found;

	e_mail_formatter_format_sync (
		formatter, parts_list, mem, 0,
		E_MAIL_FORMATTER_MODE_PRINTING, NULL);
	g_object_unref (formatter);

	if (hidden_text_html_part)
		hidden_text_html_part->is_hidden = TRUE;

	g_object_unref (parts_list);
	if (parser)
		g_object_unref (parser);

	if (append && *append)
		camel_stream_write_string (mem, append, NULL, NULL);

	camel_stream_write (mem, "", 1, NULL, NULL);
	g_object_unref (mem);

	return (gchar *) g_byte_array_free (buf, FALSE);
}

/* ********************************************************************** */

/**
 * em_utils_empty_trash:
 * @parent: parent window
 * @session: an #EMailSession
 *
 * Empties all Trash folders.
 **/
void
em_utils_empty_trash (GtkWidget *parent,
                      EMailSession *session)
{
	ESourceRegistry *registry;
	GList *list, *link;

	g_return_if_fail (E_IS_MAIL_SESSION (session));

	registry = e_mail_session_get_registry (session);

	if (!em_utils_prompt_user ((GtkWindow *) parent,
		"prompt-on-empty-trash",
		"mail:ask-empty-trash", NULL))
		return;

	list = camel_session_list_services (CAMEL_SESSION (session));

	for (link = list; link != NULL; link = g_list_next (link)) {
		CamelProvider *provider;
		CamelService *service;
		ESource *source;
		const gchar *uid;
		gboolean enabled = TRUE;

		service = CAMEL_SERVICE (link->data);
		provider = camel_service_get_provider (service);
		uid = camel_service_get_uid (service);

		if (!CAMEL_IS_STORE (service))
			continue;

		if ((provider->flags & CAMEL_PROVIDER_IS_STORAGE) == 0)
			continue;

		source = e_source_registry_ref_source (registry, uid);

		if (source != NULL) {
			enabled = e_source_registry_check_enabled (
				registry, source);
			g_object_unref (source);
		}

		if (enabled)
			mail_empty_trash (CAMEL_STORE (service));
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

/* ********************************************************************** */

gchar *
em_utils_url_unescape_amp (const gchar *url)
{
	gchar *buff;
	gint i, j, amps;

	if (!url)
		return NULL;

	amps = 0;
	for (i = 0; url[i]; i++) {
		if (url[i] == '&' && strncmp (url + i, "&amp;", 5) == 0)
			amps++;
	}

	buff = g_strdup (url);

	if (!amps)
		return buff;

	for (i = 0, j = 0; url[i]; i++, j++) {
		buff[j] = url[i];

		if (url[i] == '&' && strncmp (url + i, "&amp;", 5) == 0)
			i += 4;
	}
	buff[j] = 0;

	return buff;
}

void
emu_restore_folder_tree_state (EMFolderTree *folder_tree)
{
	EShell *shell;
	EShellBackend *backend;
	GKeyFile *key_file;
	const gchar *config_dir;
	gchar *filename;

	g_return_if_fail (folder_tree != NULL);
	g_return_if_fail (EM_IS_FOLDER_TREE (folder_tree));

	shell = e_shell_get_default ();
	backend = e_shell_get_backend_by_name (shell, "mail");
	g_return_if_fail (backend != NULL);

	config_dir = e_shell_backend_get_config_dir (backend);
	g_return_if_fail (config_dir != NULL);

	filename = g_build_filename (config_dir, "state.ini", NULL);

	key_file = g_key_file_new ();
	g_key_file_load_from_file (key_file, filename, 0, NULL);
	g_free (filename);

	em_folder_tree_restore_state (folder_tree, key_file);

	g_key_file_free (key_file);
}

static gboolean
check_prefix (const gchar *subject,
              const gchar *prefix,
              gint *skip_len)
{
	gint plen;

	g_return_val_if_fail (subject != NULL, FALSE);
	g_return_val_if_fail (prefix != NULL, FALSE);
	g_return_val_if_fail (*prefix, FALSE);
	g_return_val_if_fail (skip_len != NULL, FALSE);

	plen = strlen (prefix);
	if (g_ascii_strncasecmp (subject, prefix, plen) != 0)
		return FALSE;

	if (g_ascii_strncasecmp (subject + plen, ": ", 2) == 0) {
		*skip_len = plen + 2;
		return TRUE;
	}

	if (g_ascii_strncasecmp (subject + plen, " : ", 3) == 0) {
		*skip_len = plen + 3;
		return TRUE;
	}

	return FALSE;
}

gboolean
em_utils_is_re_in_subject (EShell *shell,
                           const gchar *subject,
                           gint *skip_len)
{
	EShellSettings *shell_settings;
	gchar *prefixes, **prefixes_strv;
	gboolean res;
	gint ii;

	g_return_val_if_fail (shell != NULL, FALSE);
	g_return_val_if_fail (subject != NULL, FALSE);
	g_return_val_if_fail (skip_len != NULL, FALSE);

	*skip_len = -1;

	if (strlen (subject) < 3)
		return FALSE;

	if (check_prefix (subject, "Re", skip_len))
		return TRUE;

	shell_settings = e_shell_get_shell_settings (shell);
	prefixes = e_shell_settings_get_string (shell_settings, "composer-localized-re");
	if (!prefixes || !*prefixes) {
		g_free (prefixes);
		return FALSE;
	}

	prefixes_strv = g_strsplit (prefixes, ",", -1);
	g_free (prefixes);

	if (!prefixes_strv)
		return FALSE;

	res = FALSE;

	for (ii = 0; !res && prefixes_strv[ii]; ii++) {
		const gchar *prefix = prefixes_strv[ii];

		if (*prefix)
			res = check_prefix (subject, prefix, skip_len);
	}

	g_strfreev (prefixes_strv);

	return res;
}
