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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
/* Work around namespace clobbage in <windows.h> */
#define DATADIR windows_DATADIR
#include <windows.h>
#undef DATADIR
#endif

#include <libebook/libebook.h>

#include <shell/e-shell.h>

#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-formatter-quote.h>
#include "em-format/e-mail-formatter-utils.h"

#include "e-mail-printer.h"
#include "e-mail-tag-editor.h"
#include "em-composer-utils.h"
#include "em-filter-editor.h"
#include "em-folder-properties.h"
#include "em-folder-selector.h"

#include "em-utils.h"

/* How many is too many? */
/* Used in em_util_ask_open_many() */
#define TOO_MANY 10

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
	proceed = e_util_prompt_user (
		parent, "org.gnome.evolution.mail", "prompt-on-open-many",
		"mail:ask-open-many", string, NULL);
	g_free (string);

	return proceed;
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
	GtkWidget *editor;
	GtkWindow *window;
	CamelNameValueArray *tags;
	guint ii, tags_len;
	gint response;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	window = e_mail_reader_get_window (reader);

	editor = e_mail_tag_editor_new ();
	gtk_window_set_transient_for (GTK_WINDOW (editor), window);

	for (ii = 0; ii < uids->len; ii++) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (folder, uids->pdata[ii]);

		if (info == NULL)
			continue;

		e_mail_tag_editor_add_message (
			E_MAIL_TAG_EDITOR (editor),
			camel_message_info_get_from (info),
			camel_message_info_get_subject (info));

		g_clear_object (&info);
	}

	/* special-case... */
	if (uids->len == 1) {
		CamelMessageInfo *info;
		const gchar *message_uid;

		message_uid = g_ptr_array_index (uids, 0);
		info = camel_folder_get_message_info (folder, message_uid);
		if (info) {
			tags = camel_message_info_dup_user_tags (info);

			if (tags)
				e_mail_tag_editor_set_tag_list (E_MAIL_TAG_EDITOR (editor), tags);

			camel_name_value_array_free (tags);
			g_clear_object (&info);
		}
	}

	response = gtk_dialog_run (GTK_DIALOG (editor));
	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_REJECT)
		goto exit;

	if (response == GTK_RESPONSE_OK) {
		tags = e_mail_tag_editor_get_tag_list (E_MAIL_TAG_EDITOR (editor));
		if (!tags)
			goto exit;
	} else {
		tags = NULL;
	}

	tags_len = tags ? camel_name_value_array_get_length (tags) : 0;

	camel_folder_freeze (folder);
	for (ii = 0; ii < uids->len; ii++) {
		CamelMessageInfo *info;
		guint jj;

		info = camel_folder_get_message_info (folder, uids->pdata[ii]);

		if (info == NULL)
			continue;

		camel_message_info_freeze_notifications (info);

		if (response == GTK_RESPONSE_REJECT) {
			camel_message_info_set_user_tag (info, "follow-up", NULL);
			camel_message_info_set_user_tag (info, "due-by", NULL);
			camel_message_info_set_user_tag (info, "completed-on", NULL);
		} else {
			for (jj = 0; jj < tags_len; jj++) {
				const gchar *name = NULL, *value = NULL;

				if (!camel_name_value_array_get (tags, jj, &name, &value))
					continue;

				camel_message_info_set_user_tag (info, name, value);
			}
		}

		camel_message_info_thaw_notifications (info);
		g_clear_object (&info);
	}

	camel_folder_thaw (folder);
	camel_name_value_array_free (tags);

exit:
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
			camel_message_info_freeze_notifications (mi);
			camel_message_info_set_user_tag (mi, "follow-up", NULL);
			camel_message_info_set_user_tag (mi, "due-by", NULL);
			camel_message_info_set_user_tag (mi, "completed-on", NULL);
			camel_message_info_thaw_notifications (mi);
			g_clear_object (&mi);
		}
	}

	camel_folder_thaw (folder);
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
			tag = camel_message_info_get_user_tag (mi, "follow-up");
			if (tag && tag[0])
				camel_message_info_set_user_tag (mi, "completed-on", now);
			g_clear_object (&mi);
		}
	}

	camel_folder_thaw (folder);

	g_free (now);
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

static gboolean
em_utils_print_messages_to_file (CamelFolder *folder,
                                 const gchar *uid,
                                 const gchar *filename)
{
	EMailParser *parser;
	EMailPartList *parts_list;
	CamelMimeMessage *message;
	CamelStore *parent_store;
	CamelSession *session;
	gboolean success = FALSE;

	message = camel_folder_get_message_sync (folder, uid, NULL, NULL);
	if (message == NULL)
		return FALSE;

	parent_store = camel_folder_get_parent_store (folder);
	session = camel_service_ref_session (CAMEL_SERVICE (parent_store));

	parser = e_mail_parser_new (session);

	/* XXX em_utils_selection_set_urilist() is synchronous,
	 *     so this function has to be synchronous as well.
	 *     That means potentially blocking for awhile. */
	parts_list = e_mail_parser_parse_sync (
		parser, folder, uid, message, NULL);
	if (parts_list != NULL) {
		EMailBackend *mail_backend;
		EAsyncClosure *closure;
		GAsyncResult *result;
		EMailPrinter *printer;
		GtkPrintOperationResult print_result;

		mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (e_shell_get_default (), "mail"));
		g_return_val_if_fail (mail_backend != NULL, FALSE);

		printer = e_mail_printer_new (parts_list, e_mail_backend_get_remote_content (mail_backend));
		e_mail_printer_set_export_filename (printer, filename);

		closure = e_async_closure_new ();

		e_mail_printer_print (
			printer, GTK_PRINT_OPERATION_ACTION_EXPORT,
			NULL, NULL, e_async_closure_callback, closure);

		result = e_async_closure_wait (closure);

		print_result = e_mail_printer_print_finish (
			printer, result, NULL);

		e_async_closure_free (closure);

		g_object_unref (printer);
		g_object_unref (parts_list);

		success = (print_result != GTK_PRINT_OPERATION_RESULT_ERROR);
	}

	g_object_unref (parser);
	g_object_unref (session);

	return success;
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
	gboolean any_read = FALSE;

	camel_mime_parser_scan_from (mp, TRUE);
	camel_mime_parser_init_with_stream (mp, stream, NULL);

	while (camel_mime_parser_step (mp, NULL, NULL) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMimeMessage *msg;

		any_read = TRUE;

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

	/* No message had bean read, maybe it's not MBOX, but a plain message */
	if (!any_read) {
		CamelMimeMessage *msg;

		if (G_IS_SEEKABLE (stream))
			g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, NULL);

		msg = camel_mime_message_new ();
		if (camel_data_wrapper_construct_from_stream_sync (
			(CamelDataWrapper *) msg, stream, NULL, NULL))
			/* FIXME camel_folder_append_message_sync() may block. */
			camel_folder_append_message_sync (
				folder, msg, NULL, NULL, NULL, NULL);
		g_object_unref (msg);
	}

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
	const guchar *data;
	gint length;

	data = gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	if (data == NULL || length == -1)
		return;

	stream = camel_stream_mem_new_with_buffer ((const gchar *) data, length);

	em_utils_read_messages_from_stream (folder, stream);

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

			g_clear_object (&info);
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
 * em_utils_selection_uidlist_foreach_sync:
 * @selection_data: a #GtkSelectionData with x-uid-list content
 * @session: an #EMailSession
 * @func: a function to call for each UID and its folder
 * @user_data: user data for @func
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Calls @func for each folder and UID provided in @selection_data.
 *
 * Warning: Could take some time to run.
 *
 * Since: 3.28
 **/
void
em_utils_selection_uidlist_foreach_sync (GtkSelectionData *selection_data,
					 EMailSession *session,
					 EMUtilsUIDListFunc func,
					 gpointer user_data,
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
	gboolean can_continue = TRUE;
	GError *local_error = NULL;

	g_return_if_fail (selection_data != NULL);
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (func != NULL);

	data = gtk_selection_data_get_data (selection_data);
	length = gtk_selection_data_get_length (selection_data);

	if (data == NULL || length == -1)
		return;

	items = g_ptr_array_new ();
	g_ptr_array_set_free_func (items, (GDestroyNotify) g_free);

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
		g_ptr_array_unref (items);
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

		if (!local_error && can_continue) {
			/* FIXME e_mail_session_uri_to_folder_sync() may block. */
			folder = e_mail_session_uri_to_folder_sync (
				session, uri, 0, cancellable, &local_error);
			if (folder) {
				can_continue = func (folder, uids, user_data, cancellable, &local_error);
				g_object_unref (folder);
			}
		}

		g_ptr_array_free (uids, TRUE);
	}

	g_hash_table_destroy (uids_by_uri);
	g_ptr_array_unref (items);

	if (local_error)
		g_propagate_error (error, local_error);
}

struct UIDListData {
	CamelFolder *dest;
	gboolean move;
};

static gboolean
uidlist_move_uids_cb (CamelFolder *folder,
		      const GPtrArray *uids,
		      gpointer user_data,
		      GCancellable *cancellable,
		      GError **error)
{
	struct UIDListData *uld = user_data;

	g_return_val_if_fail (uld != NULL, FALSE);

	/* FIXME camel_folder_transfer_messages_to_sync() may block. */
	return camel_folder_transfer_messages_to_sync (
		folder, (GPtrArray *) uids, uld->dest, uld->move, NULL, cancellable, error);
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
	struct UIDListData uld;

	g_return_if_fail (CAMEL_IS_FOLDER (dest));

	uld.dest = dest;
	uld.move = move;

	em_utils_selection_uidlist_foreach_sync	(selection_data, session, uidlist_move_uids_cb, &uld, cancellable, error);
}

static gchar *
em_utils_build_export_basename_internal (const gchar *subject,
					 time_t reftime,
					 const gchar *extension)
{
	gchar *basename;
	struct tm *ts;
	gchar datetmp[15];

	if (reftime <= 0)
		reftime = time (NULL);

	ts = localtime (&reftime);
	strftime (datetmp, sizeof (datetmp), "%Y%m%d%H%M%S", ts);

	if (subject == NULL || *subject == '\0')
		subject = "Untitled Message";

	if (extension == NULL)
		extension = "";

	basename = g_strdup_printf ("%s_%s%s", datetmp, subject, extension);

	return basename;
}

/**
 * em_utils_build_export_basename:
 * @folder: a #CamelFolder where the message belongs
 * @uid: a message UID
 * @extension: (nullable): a filename extension
 *
 * Builds a name that consists of data and time when the message was received,
 * message subject and extension.
 *
 * Returns: (transfer full): a newly allocated string with generated basename
 *
 * Since: 3.22
 **/
gchar *
em_utils_build_export_basename (CamelFolder *folder,
                                const gchar *uid,
                                const gchar *extension)
{
	CamelMessageInfo *info;
	const gchar *subject = NULL;
	gchar *basename;
	time_t reftime = 0;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	/* Try to get the drop filename from the message or folder. */
	info = camel_folder_get_message_info (folder, uid);
	if (info != NULL) {
		subject = camel_message_info_get_subject (info);
		reftime = camel_message_info_get_date_sent (info);
	}

	basename = em_utils_build_export_basename_internal (subject, reftime, extension);

	g_clear_object (&info);

	return basename;
}

/**
 * em_utils_selection_set_urilist:
 * @context:
 * @data:
 * @folder:
 * @uids:
 *
 * Set the selection data @data to a uri which points to a file, which is
 * a berkely mailbox format mailbox.  The file is automatically cleaned
 * up when the application quits.
 **/
void
em_utils_selection_set_urilist (GdkDragContext *context,
				GtkSelectionData *data,
                                CamelFolder *folder,
                                GPtrArray *uids)
{
	gchar *tmpdir;
	gchar *uri;
	gint fd;
	/* This is waiting for https://bugs.webkit.org/show_bug.cgi?id=212814 */
	#if 0
	GSettings *settings;
	gchar *save_file_format;
	#endif
	gboolean save_as_mbox;

	g_return_if_fail (uids != NULL);

	/* can be 0 with empty folders */
	if (!uids->len)
		return;

	/* Use cached value from the last call, if exists */
	tmpdir = g_object_get_data (G_OBJECT (context), "evo-urilist");
	if (tmpdir) {
		GdkAtom type;

		type = gtk_selection_data_get_target (data);
		gtk_selection_data_set (
			data, type, 8,
			(guchar *) tmpdir,
			strlen (tmpdir));

		return;
	}

	tmpdir = e_mkdtemp ("drag-n-drop-XXXXXX");
	if (tmpdir == NULL)
		return;

	/* This is waiting for https://bugs.webkit.org/show_bug.cgi?id=212814 */
	#if 0
	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	/* Save format is mbox unless pdf is explicitly requested. */
	save_file_format = g_settings_get_string (
		settings, "drag-and-drop-save-file-format");
	save_as_mbox = (g_strcmp0 (save_file_format, "pdf") != 0);
	g_free (save_file_format);

	g_object_unref (settings);
	#else
	save_as_mbox = TRUE;
	#endif

	if (save_as_mbox) {
		CamelStream *fstream;
		gchar *basename;
		gchar *filename;

		if (uids->len > 1) {
			basename = g_strdup_printf (
				_("Messages from %s"),
				camel_folder_get_display_name (folder));
		} else {
			basename = em_utils_build_export_basename (
				folder, uids->pdata[0], NULL);
		}
		e_util_make_safe_filename (basename);

		/* Add .mbox extension to the filename */
		if (!g_str_has_suffix (basename, ".mbox")) {
			gchar *tmp = basename;
			basename = g_strconcat (basename, ".mbox", NULL);
			g_free (tmp);
		}

		filename = g_build_filename (tmpdir, basename, NULL);
		g_free (basename);

		fd = g_open (
			filename,
			O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0666);
		if (fd == -1) {
			g_free (filename);
			goto exit;
		}

		uri = g_filename_to_uri (filename, NULL, NULL);
		fstream = camel_stream_fs_new_with_fd (fd);
		if (fstream != NULL) {
			if (em_utils_write_messages_to_stream (folder, uids, fstream) == 0) {
				GdkAtom type;
				gchar *uri_crlf;

				/* terminate with \r\n to be compliant with the spec */
				uri_crlf = g_strconcat (uri, "\r\n", NULL);

				type = gtk_selection_data_get_target (data);
				gtk_selection_data_set (
					data, type, 8,
					(guchar *) uri_crlf,
					strlen (uri_crlf));

				/* Remember it, to not regenerate it, when the target widget asks for the data again */
				g_object_set_data_full (G_OBJECT (context), "evo-urilist", uri_crlf, g_free);
			}
			g_object_unref (fstream);
		} else
			close (fd);

		g_free (filename);
		g_free (uri);

	} else {  /* save as pdf */
		gchar **uris;
		guint n_uris = 0;
		guint ii;

		uris = g_new0 (gchar *, uids->len + 1);
		for (ii = 0; ii < uids->len; ii++) {
			gchar *basename;
			gchar *filename;
			gboolean success;

			basename = em_utils_build_export_basename (
				folder, uids->pdata[ii], ".pdf");
			e_util_make_safe_filename (basename);
			filename = g_build_filename (tmpdir, basename, NULL);
			g_free (basename);

			/* validity test */
			fd = g_open (
				filename,
				O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0666);
			if (fd == -1) {
				g_strfreev (uris);
				g_free (filename);
				goto exit;
			}
			close (fd);

			/* export */
			success = em_utils_print_messages_to_file (
				folder, uids->pdata[ii], filename);
			if (success) {
				/* terminate with \r\n to be compliant with the spec */
				uri = g_filename_to_uri (filename, NULL, NULL);
				uris[n_uris++] = g_strconcat (uri, "\r\n", NULL);
				g_free (uri);
			}

			g_free (filename);
		}

		if (gtk_selection_data_set_uris (data, uris)) {
			/* Remember it, to not regenerate it, when the target widget asks for the data again */
			g_object_set_data_full (G_OBJECT (context), "evo-urilist",
				g_strndup ((const gchar *) gtk_selection_data_get_data (data), gtk_selection_data_get_length (data)),
				g_free);
		}

		g_strfreev (uris);
	}

exit:
	g_free (tmpdir);
	/* the 'fd' from the 'save_as_mbox' part is freed within the 'fstream' */
	/* coverity[leaked_handle] */
}

/**
 * em_utils_selection_get_urilist:
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

		/* 'fd', if set, is freed within the 'stream' */
		/* coverity[overwrite_var] */
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

	/* 'fd', if set, is freed within the 'stream' */
	/* coverity[leaked_handle] */
}

/* ********************************************************************** */

static gboolean
is_only_text_part_in_this_level (GList *parts,
                                 EMailPart *text_html_part)
{
	const gchar *text_html_part_id;
	const gchar *dot;
	gint level_len;
	GList *iter;

	g_return_val_if_fail (parts != NULL, FALSE);
	g_return_val_if_fail (text_html_part != NULL, FALSE);

	text_html_part_id = e_mail_part_get_id (text_html_part);

	dot = strrchr (text_html_part_id, '.');
	if (!dot)
		return FALSE;

	level_len = dot - text_html_part_id;
	for (iter = parts; iter; iter = iter->next) {
		EMailPart *part = E_MAIL_PART (iter->data);
		const gchar *mime_type;
		const gchar *part_id;

		if (part == NULL)
			continue;

		if (part == text_html_part)
			continue;

		if (part->is_hidden)
			continue;

		if (e_mail_part_get_is_attachment (part))
			continue;

		mime_type = e_mail_part_get_mime_type (part);
		if (mime_type == NULL)
			continue;

		part_id = e_mail_part_get_id (part);
		dot = strrchr (part_id, '.');
		if (dot - part_id != level_len ||
		    strncmp (text_html_part_id, part_id, level_len) != 0)
			continue;

		if (g_ascii_strncasecmp (mime_type, "text/", 5) == 0)
			return FALSE;
	}

	return TRUE;
}

/**
 * em_utils_message_to_html_ex:
 * @session: a #CamelSession
 * @message: a #CamelMimeMessage
 * @credits: (nullable): credits attribution string when quoting, or %NULL
 * @flags: the %EMFormatQuote flags
 * @part_list: (nullable): an #EMailPartList
 * @prepend: (nulalble): text to prepend, or %NULL
 * @append: (nullable): text to append, or %NULL
 * @validity_found: (nullable): if not %NULL, then here will be set what validities
 *         had been found during message conversion. Value is a bit OR
 *         of EM_FORMAT_VALIDITY_FOUND_* constants.
 * @out_part_list: (nullable): if not %NULL, sets it to the part list being
 *         used to generate the body. Unref it with g_object_unref(),
 *         when no longer needed.
 *
 * Convert a message to html, quoting if the @credits attribution
 * string is given.
 *
 * Return value: The html version as a NULL terminated string.
 *
 * Since: 3.42
 **/
gchar *
em_utils_message_to_html_ex (CamelSession *session,
                             CamelMimeMessage *message,
                             const gchar *credits,
                             guint32 flags,
                             EMailPartList *part_list,
                             const gchar *prepend,
                             const gchar *append,
                             EMailPartValidityFlags *validity_found,
			     EMailPartList **out_part_list)
{
	EMailFormatter *formatter;
	EMailParser *parser = NULL;
	GOutputStream *stream;
	EShell *shell;
	GtkWindow *window;
	EMailPart *hidden_text_html_part = NULL;
	EMailPartValidityFlags is_validity_found = 0;
	gsize n_bytes_written = 0;
	GQueue queue = G_QUEUE_INIT;
	GList *head, *link;
	gboolean found_text_part = FALSE;
	gchar *data;

	shell = e_shell_get_default ();
	window = e_shell_get_active_window (shell);

	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);

	stream = g_memory_output_stream_new_resizable ();

	formatter = e_mail_formatter_quote_new (credits, flags);
	e_mail_formatter_update_style (formatter,
		gtk_widget_get_state_flags (GTK_WIDGET (window)));

	if (part_list == NULL) {
		GSettings *settings;
		gchar *charset;

		/* FIXME We should be getting this from the
		 *       current view, not the global setting. */
		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		charset = g_settings_get_string (settings, "charset");
		if (charset && *charset)
			e_mail_formatter_set_default_charset (formatter, charset);
		g_object_unref (settings);
		g_free (charset);

		parser = e_mail_parser_new (session);
		part_list = e_mail_parser_parse_sync (parser, NULL, NULL, message, NULL);
	} else {
		g_object_ref (part_list);
	}

	/* Return all found validities and possibly show hidden prefer-plain part */
	e_mail_part_list_queue_parts (part_list, NULL, &queue);
	head = g_queue_peek_head_link (&queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *part = link->data;
		const gchar *mime_type;

		mime_type = e_mail_part_get_mime_type (part);

		/* prefer-plain can hide HTML parts, even when it's the only
		 * text part in the email, thus show it (and hide again later) */
		if (!found_text_part && !hidden_text_html_part &&
		    mime_type != NULL &&
		    !e_mail_part_get_is_attachment (part)) {
			if (!part->is_hidden &&
			    g_ascii_strcasecmp (mime_type, "text/plain") == 0) {
				found_text_part = TRUE;
			} else if (g_ascii_strcasecmp (mime_type, "text/html") == 0) {
				if (!part->is_hidden) {
					found_text_part = TRUE;
				} else if (is_only_text_part_in_this_level (head, part)) {
					part->is_hidden = FALSE;
					hidden_text_html_part = part;
				}
			}
		}

		is_validity_found |= e_mail_part_get_validity_flags (part);
	}

	while (!g_queue_is_empty (&queue))
		g_object_unref (g_queue_pop_head (&queue));

	if (validity_found != NULL)
		*validity_found = is_validity_found;

	if (prepend != NULL && *prepend != '\0')
		g_output_stream_write_all (
			stream, prepend, strlen (prepend), NULL, NULL, NULL);

	e_mail_formatter_format_sync (
		formatter, part_list, stream, 0,
		E_MAIL_FORMATTER_MODE_PRINTING, NULL);
	g_object_unref (formatter);

	if (hidden_text_html_part != NULL)
		hidden_text_html_part->is_hidden = TRUE;

	if (out_part_list)
		*out_part_list = part_list;
	else
		g_object_unref (part_list);

	g_clear_object (&parser);

	if (append != NULL && *append != '\0')
		g_output_stream_write_all (
			stream, append, strlen (append), NULL, NULL, NULL);

	g_output_stream_write_all (stream, "", 1, &n_bytes_written, NULL, NULL);

	g_output_stream_close (stream, NULL, NULL);

	data = g_memory_output_stream_steal_data (
		G_MEMORY_OUTPUT_STREAM (stream));

	g_object_unref (stream);

	return data;
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

	if (!e_util_prompt_user ((GtkWindow *) parent,
		"org.gnome.evolution.mail",
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
	      const gchar * const *separators,
              gint *skip_len)
{
	gboolean res = FALSE;
	gint plen;

	g_return_val_if_fail (subject != NULL, FALSE);
	g_return_val_if_fail (prefix != NULL, FALSE);
	g_return_val_if_fail (*prefix, FALSE);
	g_return_val_if_fail (skip_len != NULL, FALSE);

	plen = strlen (prefix);
	if (g_ascii_strncasecmp (subject, prefix, plen) != 0)
		return FALSE;

	if (g_ascii_isspace (subject[plen]))
		plen++;

	res = e_util_utf8_strstrcase (subject + plen, ":") == subject + plen;
	if (res)
		plen += strlen (":");

	if (!res) {
		res = e_util_utf8_strstrcase (subject + plen, "︰") == subject + plen;
		if (res)
			plen += strlen ("︰");
	}

	if (!res && separators) {
		gint ii;

		for (ii = 0; separators[ii]; ii++) {
			const gchar *separator = separators[ii];

			res = *separator && e_util_utf8_strstrcase (subject + plen, separator) == subject + plen;
			if (res) {
				plen += strlen (separator);
				break;
			}
		}
	}

	if (res) {
		if (g_ascii_isspace (subject[plen]))
			plen++;

		*skip_len = plen;
	}

	return res;
}

gboolean
em_utils_is_re_in_subject (const gchar *subject,
                           gint *skip_len,
			   const gchar * const *use_prefixes_strv,
			   const gchar * const *use_separators_strv)
{
	gchar **prefixes_strv;
	gchar **separators_strv;
	const gchar *localized_re, *localized_separator;
	gboolean res;
	gint ii;

	g_return_val_if_fail (subject != NULL, FALSE);
	g_return_val_if_fail (skip_len != NULL, FALSE);

	*skip_len = -1;

	if (strlen (subject) < 3)
		return FALSE;

	if (use_separators_strv) {
		separators_strv = (gchar **) use_separators_strv;
	} else {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		separators_strv = g_settings_get_strv (settings, "composer-localized-re-separators");
		g_object_unref (settings);

		if (separators_strv && !*separators_strv) {
			g_strfreev (separators_strv);
			separators_strv = NULL;
		}
	}

	if (check_prefix (subject, "Re", (const gchar * const *) separators_strv, skip_len)) {
		if (!use_separators_strv)
			g_strfreev (separators_strv);

		return TRUE;
	}

	/* Translators: This is a reply attribution in the message reply subject. Both 'Re'-s in the 'reply-attribution' translation context should translate into the same string. */
	localized_re = C_("reply-attribution", "Re");

	/* Translators: This is a reply attribution separator in the message reply subject. This should match the ':' in 'Re: %s' in the 'reply-attribution' translation context. */
	localized_separator = C_("reply-attribution", ":");

	if (check_prefix (subject, localized_re, (const gchar * const *) separators_strv, skip_len)) {
		if (!use_separators_strv)
			g_strfreev (separators_strv);

		return TRUE;
	}

	if (localized_separator && g_strcmp0 (localized_separator, ":") != 0) {
		const gchar *localized_separator_strv[2];

		localized_separator_strv[0] = localized_separator;
		localized_separator_strv[1] = NULL;

		if (check_prefix (subject, localized_re, (const gchar * const *) localized_separator_strv, skip_len)) {
			if (!use_separators_strv)
				g_strfreev (separators_strv);

			return TRUE;
		}
	}

	if (use_prefixes_strv) {
		prefixes_strv = (gchar **) use_prefixes_strv;
	} else {
		GSettings *settings;
		gchar *prefixes;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		prefixes = g_settings_get_string (settings, "composer-localized-re");
		g_object_unref (settings);

		if (!prefixes || !*prefixes) {
			g_free (prefixes);

			if (!use_separators_strv)
				g_strfreev (separators_strv);

			return FALSE;
		}

		prefixes_strv = g_strsplit (prefixes, ",", -1);
		g_free (prefixes);
	}

	if (!prefixes_strv) {
		if (!use_separators_strv)
			g_strfreev (separators_strv);

		return FALSE;
	}

	res = FALSE;

	for (ii = 0; !res && prefixes_strv[ii]; ii++) {
		const gchar *prefix = prefixes_strv[ii];

		if (*prefix)
			res = check_prefix (subject, prefix, (const gchar * const *) separators_strv, skip_len);
	}

	if (!use_prefixes_strv)
		g_strfreev (prefixes_strv);
	if (!use_separators_strv)
		g_strfreev (separators_strv);

	return res;
}

gchar *
em_utils_get_archive_folder_uri_from_folder (CamelFolder *folder,
					     EMailBackend *mail_backend,
					     GPtrArray *uids,
					     gboolean deep_uids_check)
{
	CamelStore *store;
	ESource *source = NULL;
	gchar *archive_folder = NULL;
	gchar *folder_uri;
	gboolean aa_enabled;
	EAutoArchiveConfig aa_config;
	gint aa_n_units;
	EAutoArchiveUnit aa_unit;
	gchar *aa_custom_target_folder_uri;

	if (!folder)
		return NULL;

	folder_uri = e_mail_folder_uri_build (
		camel_folder_get_parent_store (folder),
		camel_folder_get_full_name (folder));

	if (em_folder_properties_autoarchive_get (mail_backend, folder_uri,
		&aa_enabled, &aa_config, &aa_n_units, &aa_unit, &aa_custom_target_folder_uri)) {
		if (aa_config == E_AUTO_ARCHIVE_CONFIG_MOVE_TO_CUSTOM &&
		    aa_custom_target_folder_uri && *aa_custom_target_folder_uri) {
			g_free (folder_uri);
			return aa_custom_target_folder_uri;
		}

		g_free (aa_custom_target_folder_uri);

		if (aa_config == E_AUTO_ARCHIVE_CONFIG_DELETE) {
			g_free (folder_uri);
			return NULL;
		}
	}
	g_free (folder_uri);

	store = camel_folder_get_parent_store (folder);
	if (g_strcmp0 (E_MAIL_SESSION_LOCAL_UID, camel_service_get_uid (CAMEL_SERVICE (store))) == 0) {
		return mail_config_dup_local_archive_folder ();
	}

	if (CAMEL_IS_VEE_FOLDER (folder) && uids && uids->len > 0) {
		CamelVeeFolder *vee_folder = CAMEL_VEE_FOLDER (folder);
		CamelFolder *orig_folder = NULL;

		store = NULL;

		if (deep_uids_check) {
			gint ii;

			for (ii = 0; ii < uids->len; ii++) {
				orig_folder = camel_vee_folder_dup_vee_uid_folder (vee_folder, uids->pdata[ii]);
				if (orig_folder) {
					if (store && camel_folder_get_parent_store (orig_folder) != store) {
						g_clear_object (&orig_folder);
						/* Do not know which archive folder to use when there are
						   selected messages from multiple accounts/stores. */
						store = NULL;
						break;
					}

					store = camel_folder_get_parent_store (orig_folder);
					g_clear_object (&orig_folder);
				}
			}
		} else {
			orig_folder = camel_vee_folder_dup_vee_uid_folder (CAMEL_VEE_FOLDER (folder), uids->pdata[0]);
			if (orig_folder) {
				store = camel_folder_get_parent_store (orig_folder);
				g_clear_object (&orig_folder);
			}
		}

		if (store && orig_folder) {
			folder_uri = e_mail_folder_uri_build (
				camel_folder_get_parent_store (orig_folder),
				camel_folder_get_full_name (orig_folder));

			if (em_folder_properties_autoarchive_get (mail_backend, folder_uri,
				&aa_enabled, &aa_config, &aa_n_units, &aa_unit, &aa_custom_target_folder_uri)) {
				if (aa_config == E_AUTO_ARCHIVE_CONFIG_MOVE_TO_CUSTOM &&
				    aa_custom_target_folder_uri && *aa_custom_target_folder_uri) {
					g_free (folder_uri);
					return aa_custom_target_folder_uri;
				}

				g_free (aa_custom_target_folder_uri);

				if (aa_config == E_AUTO_ARCHIVE_CONFIG_DELETE) {
					g_free (folder_uri);
					return NULL;
				}
			}

			g_free (folder_uri);
		}
	}

	if (store) {
		ESourceRegistry *registry;

		registry = e_mail_session_get_registry (e_mail_backend_get_session (mail_backend));
		source = e_source_registry_ref_source (registry, camel_service_get_uid (CAMEL_SERVICE (store)));
	}

	if (source) {
		if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
			ESourceMailAccount *account_ext;

			account_ext = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);

			archive_folder = e_source_mail_account_dup_archive_folder (account_ext);
			if (!archive_folder || !*archive_folder) {
				g_free (archive_folder);
				archive_folder = NULL;
			}
		}

		g_object_unref (source);
	}

	return archive_folder;
}

gboolean
em_utils_process_autoarchive_sync (EMailBackend *mail_backend,
				   CamelFolder *folder,
				   const gchar *folder_uri,
				   GCancellable *cancellable,
				   GError **error)
{
	gboolean aa_enabled;
	EAutoArchiveConfig aa_config;
	gint aa_n_units;
	EAutoArchiveUnit aa_unit;
	gchar *aa_custom_target_folder_uri = NULL;
	GDateTime *now_time, *use_time;
	gchar *search_sexp;
	GPtrArray *uids = NULL;
	gboolean success;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (mail_backend), FALSE);
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (folder_uri != NULL, FALSE);

	if (!em_folder_properties_autoarchive_get (mail_backend, folder_uri,
		&aa_enabled, &aa_config, &aa_n_units, &aa_unit, &aa_custom_target_folder_uri))
		return TRUE;

	if (!aa_enabled) {
		g_free (aa_custom_target_folder_uri);
		return TRUE;
	}

	if (aa_config == E_AUTO_ARCHIVE_CONFIG_MOVE_TO_CUSTOM && (!aa_custom_target_folder_uri || !*aa_custom_target_folder_uri)) {
		g_free (aa_custom_target_folder_uri);
		return TRUE;
	}

	now_time = g_date_time_new_now_utc ();
	switch (aa_unit) {
		case E_AUTO_ARCHIVE_UNIT_DAYS:
			use_time = g_date_time_add_days (now_time, -aa_n_units);
			break;
		case E_AUTO_ARCHIVE_UNIT_WEEKS:
			use_time = g_date_time_add_weeks (now_time, -aa_n_units);
			break;
		case E_AUTO_ARCHIVE_UNIT_MONTHS:
			use_time = g_date_time_add_months (now_time, -aa_n_units);
			break;
		default:
			g_date_time_unref (now_time);
			g_free (aa_custom_target_folder_uri);
			return TRUE;
	}

	g_date_time_unref (now_time);

	search_sexp = g_strdup_printf ("(match-all (and "
		"(not (system-flag \"junk\")) "
		"(not (system-flag \"deleted\")) "
		"(< (get-sent-date) %" G_GINT64_FORMAT ")"
		"))", g_date_time_to_unix (use_time));
	success = camel_folder_search_sync (folder, search_sexp, &uids, cancellable, error);

	if (success && uids && uids->len > 0) {
		gint ii;

		if (aa_config == E_AUTO_ARCHIVE_CONFIG_MOVE_TO_ARCHIVE ||
		    aa_config == E_AUTO_ARCHIVE_CONFIG_MOVE_TO_CUSTOM) {
			CamelFolder *dest;

			if (aa_config == E_AUTO_ARCHIVE_CONFIG_MOVE_TO_ARCHIVE) {
				g_free (aa_custom_target_folder_uri);
				aa_custom_target_folder_uri = em_utils_get_archive_folder_uri_from_folder (folder, mail_backend, uids, TRUE);
			}

			dest = aa_custom_target_folder_uri ? e_mail_session_uri_to_folder_sync (
				e_mail_backend_get_session (mail_backend), aa_custom_target_folder_uri, 0,
				cancellable, error) : NULL;
			if (dest != NULL && dest != folder) {
				camel_folder_freeze (folder);
				camel_folder_freeze (dest);

				if (camel_folder_transfer_messages_to_sync (
					folder, uids, dest, TRUE, NULL,
					cancellable, error)) {
					/* make sure all deleted messages are marked as seen */
					for (ii = 0; ii < uids->len; ii++) {
						camel_folder_set_message_flags (
							folder, uids->pdata[ii],
							CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
					}
				} else {
					success = FALSE;
				}

				camel_folder_thaw (folder);
				camel_folder_thaw (dest);

				if (success)
					success = camel_folder_synchronize_sync (dest, FALSE, cancellable, error);
			}

			g_clear_object (&dest);
		} else if (aa_config == E_AUTO_ARCHIVE_CONFIG_DELETE) {
			camel_folder_freeze (folder);

			camel_operation_push_message (cancellable, "%s", _("Deleting old messages"));

			for (ii = 0; ii < uids->len; ii++) {
				camel_folder_set_message_flags (
					folder, uids->pdata[ii],
					CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);
			}

			camel_operation_pop_message (cancellable);

			camel_folder_thaw (folder);
		}
	}

	if (uids)
		g_ptr_array_unref (uids);

	g_free (search_sexp);
	g_free (aa_custom_target_folder_uri);
	g_date_time_unref (use_time);

	return success;
}

/**
 * em_utils_account_path_to_folder_uri:
 * @session: (nullable): a #CamelSession, or %NULL
 * @account_path: the account path to transform to folder URI
 *
 * Transform the @account_path to a folder URI. It can be in a form
 * of 'account-name/folder/path', aka 'On This Computer/Inbox/Private'.
 * The account name is compared case insensitively.
 *
 * When the @session is %NULL, it' is taken from the default shell, if
 * such exists.
 *
 * Returns: (nullable): a folder URI corresponding to the @account_path,
 *    or %NULL, when the account could not be found. Free the returned
 *    string with g_free(), when no longer needed.
 *
 * Since: 3.40
 **/
gchar *
em_utils_account_path_to_folder_uri (CamelSession *session,
				     const gchar *account_path)
{
	GList *services, *link;
	const gchar *dash;
	gchar *folder_uri = NULL, *account_name;

	g_return_val_if_fail (account_path != NULL, NULL);

	dash = strchr (account_path, '/');
	if (!dash)
		return NULL;

	if (!session) {
		EShell *shell;
		EShellBackend *shell_backend;
		EMailSession *mail_session;

		shell = e_shell_get_default ();
		if (!shell)
			return NULL;

		shell_backend = e_shell_get_backend_by_name (shell, "mail");
		if (!shell_backend)
			return NULL;

		mail_session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));
		if (!mail_session)
			return NULL;

		session = CAMEL_SESSION (mail_session);
	}

	account_name = e_util_utf8_data_make_valid (account_path, dash - account_path);

	services = camel_session_list_services (session);
	for (link = services; link; link = g_list_next (link)) {
		CamelService *service = link->data;

		/* Case sensitive account name compare, because the folder names are also compared case sensitively */
		if (CAMEL_IS_STORE (service) && g_strcmp0 (camel_service_get_display_name (service), account_name) == 0) {
			folder_uri = e_mail_folder_uri_build (CAMEL_STORE (service), dash + 1);
			break;
		}
	}

	g_list_free_full (services, g_object_unref);
	g_free (account_name);

	return folder_uri;
}

EMailBrowser *
em_utils_find_message_window (EMailFormatterMode display_mode,
			      CamelFolder *folder,
			      const gchar *message_uid)
{
	EShell *shell;
	GList *windows, *link;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (message_uid != NULL, NULL);

	shell = e_shell_get_default ();
	windows = gtk_application_get_windows (GTK_APPLICATION (shell));

	for (link = windows; link; link = g_list_next (link)) {
		GtkWindow *window = link->data;

		if (E_IS_MAIL_BROWSER (window)) {
			EMailBrowser *browser = E_MAIL_BROWSER (window);
			gboolean matched = FALSE;

			if (e_mail_browser_get_display_mode (browser) == display_mode) {
				CamelFolder *tmp_folder;
				GPtrArray *uids;

				tmp_folder = e_mail_reader_ref_folder (E_MAIL_READER (browser));
				uids = e_mail_reader_get_selected_uids (E_MAIL_READER (browser));

				if (uids->len == 1) {
					const gchar *uid = g_ptr_array_index (uids, 0);

					matched = g_strcmp0 (message_uid, uid) == 0 &&
						  folder == tmp_folder;

					if (!matched) {
						CamelFolder *real_folder = NULL, *tmp_real_folder = NULL;
						gchar *real_uid = NULL, *tmp_real_uid = NULL;

						if (CAMEL_IS_VEE_FOLDER (folder))
							em_utils_get_real_folder_and_message_uid (folder, message_uid, &real_folder, NULL, &real_uid);

						if (CAMEL_IS_VEE_FOLDER (tmp_folder))
							em_utils_get_real_folder_and_message_uid (tmp_folder, uid, &tmp_real_folder, NULL, &tmp_real_uid);

						matched = (real_folder || tmp_real_folder) &&
							(real_folder ? real_folder : folder) == (tmp_real_folder ? tmp_real_folder : tmp_folder) &&
							g_strcmp0 (real_uid ? real_uid : message_uid, tmp_real_uid ? tmp_real_uid : uid) == 0;

						g_clear_object (&tmp_real_folder);
						g_clear_object (&real_folder);
						g_free (tmp_real_uid);
						g_free (real_uid);
					}
				}

				g_ptr_array_unref (uids);
				g_clear_object (&tmp_folder);
			}

			if (matched)
				return browser;
		}
	}

	return NULL;
}

/**
 * em_utils_import_pgp_key:
 * @parent: a #GtkWindow parent for a dialog
 * @session: (nullable): a #CamelSession
 * @keydata: key data to import
 * @keydata_size: size of the @keydata, in bytes
 * @error: return location for a #GError, or %NULL
 *
 * Asks the user whether he/she wants to import the provided OpenPGP key
 * and tries to import it.
 *
 * Returns: whether the import succeeded
 *
 * Since: 3.50
 **/
gboolean
em_utils_import_pgp_key (GtkWindow *parent,
			 CamelSession *session,
			 const guint8 *keydata,
			 gsize keydata_size,
			 GError **error)
{
	struct _trust_options {
		const gchar *label;
		CamelGpgTrust trust;
		GtkToggleButton *button;
	} trust_options[] = {
		{ NC_("trust", "_Unknown"),		CAMEL_GPG_TRUST_UNKNOWN,	NULL },
		{ NC_("trust", "_Never trust"),		CAMEL_GPG_TRUST_NEVER,		NULL },
		{ NC_("trust", "Trust _marginally"),	CAMEL_GPG_TRUST_MARGINAL,	NULL },
		{ NC_("trust", "Trust _fully"),		CAMEL_GPG_TRUST_FULL,		NULL },
		{ NC_("trust", "Trust _ultimately"),	CAMEL_GPG_TRUST_ULTIMATE,	NULL }
	};
	EAlert *alert;
	CamelGpgContext *gpgctx;
	GSList *key_infos = NULL, *link;
	GtkWidget *dialog;
	GtkWidget *widget;
	GtkWidget *container;
	guint ii;
	gboolean success = FALSE;

	if (session)
		g_return_val_if_fail (CAMEL_IS_SESSION (session), FALSE);
	g_return_val_if_fail (keydata != NULL, FALSE);
	g_return_val_if_fail (keydata_size > 0, FALSE);

	gpgctx = CAMEL_GPG_CONTEXT (camel_gpg_context_new (session));

	if (!camel_gpg_context_get_key_data_info_sync (gpgctx, keydata, keydata_size, 0, &key_infos, NULL, error)) {
		g_clear_object (&gpgctx);
		return FALSE;
	}

	alert = e_alert_new ("mail:ask-import-pgp-key", NULL);
	dialog = e_alert_dialog_new (parent, alert);
	g_object_unref (alert);

	container = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	container = widget;

	for (link = key_infos; link; link = g_slist_next (link)) {
		CamelGpgKeyInfo *nfo = link->data;

		if (nfo && camel_gpg_key_info_get_id (nfo)) {
			GSList *user_ids;
			gchar *tmp;

			tmp = g_strdup_printf (_("Key ID: %s"), camel_gpg_key_info_get_id (nfo));
			widget = gtk_label_new (tmp);
			g_object_set (widget,
				"halign", GTK_ALIGN_START,
				"margin-top", link == key_infos ? 0 : 12,
				"selectable", TRUE,
				"xalign", 0.0,
				NULL);
			gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
			g_free (tmp);

			tmp = g_strdup_printf (_("Fingerprint: %s"), camel_gpg_key_info_get_fingerprint (nfo));
			widget = gtk_label_new (tmp);
			g_object_set (widget,
				"halign", GTK_ALIGN_START,
				"margin-start", 12,
				"selectable", TRUE,
				"xalign", 0.0,
				NULL);
			gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
			g_free (tmp);

			if (camel_gpg_key_info_get_creation_date (nfo) > 0) {
				gchar *fmt;

				fmt = e_datetime_format_format ("mail", "table", DTFormatKindDateTime, (time_t) camel_gpg_key_info_get_creation_date (nfo));
				if (fmt) {
					tmp = g_strdup_printf (_("Created: %s"), fmt);
					widget = gtk_label_new (tmp);
					g_object_set (widget,
						"halign", GTK_ALIGN_START,
						"margin-start", 12,
						"selectable", TRUE,
						"xalign", 0.0,
						NULL);
					gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
					g_free (tmp);
					g_free (fmt);
				}
			}

			user_ids = camel_gpg_key_info_get_user_ids (nfo);
			if (user_ids) {
				GSList *id_link;
				GString *ids;

				ids = g_string_new ("");

				for (id_link = user_ids; id_link; id_link = g_slist_next (id_link)) {
					const gchar *id = id_link->data;

					if (id && *id) {
						if (ids->len > 0)
							g_string_append (ids, ", ");
						g_string_append (ids, id);
					}
				}

				if (ids->len > 0) {
					tmp = g_strdup_printf (_("User ID: %s"), ids->str);
					widget = gtk_label_new (tmp);
					g_object_set (widget,
						"halign", GTK_ALIGN_START,
						"margin-start", 12,
						"selectable", TRUE,
						"max-width-chars", 80,
						"width-chars", 80,
						"wrap", TRUE,
						"wrap-mode", PANGO_WRAP_WORD_CHAR,
						"xalign", 0.0,
						NULL);
					gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
					g_free (tmp);
				}

				g_string_free (ids, TRUE);
			}
		}
	}

	widget = gtk_label_new (_("Set trust level for the key:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_START);
	gtk_widget_set_margin_top (widget, 12);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	for (ii = 0; ii < G_N_ELEMENTS (trust_options); ii++) {
		widget = gtk_radio_button_new_with_mnemonic_from_widget (GTK_RADIO_BUTTON (trust_options[0].button),
			g_dpgettext2 (GETTEXT_PACKAGE, "trust", trust_options[ii].label));
		gtk_widget_set_margin_start (widget, 12);
		gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

		trust_options[ii].button = GTK_TOGGLE_BUTTON (widget);
	}

	/* Preselect the 'full' trust level, thus the key can be used to encrypt messages */
	g_warn_if_fail (ii > 3);
	g_warn_if_fail (trust_options[3].trust == CAMEL_GPG_TRUST_FULL);
	gtk_toggle_button_set_active (trust_options[3].button, TRUE);

	gtk_widget_show_all (container);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES) {
		CamelGpgTrust trust = CAMEL_GPG_TRUST_NONE;

		for (ii = 0; ii < G_N_ELEMENTS (trust_options); ii++) {
			if (gtk_toggle_button_get_active (trust_options[ii].button)) {
				trust = trust_options[ii].trust;
				break;
			}
		}

		success = camel_gpg_context_import_key_sync (gpgctx, keydata, keydata_size, 0, NULL, error);
		if (success) {
			for (link = key_infos; link && success; link = g_slist_next (link)) {
				CamelGpgKeyInfo *nfo = link->data;

				if (nfo && camel_gpg_key_info_get_id (nfo)) {
					success = camel_gpg_context_set_key_trust_sync (gpgctx,
						camel_gpg_key_info_get_id (nfo), trust, NULL, error);
				}
			}
		}
	} else {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Operation was cancelled"));
	}

	g_slist_free_full (key_infos, (GDestroyNotify) camel_gpg_key_info_free);
	gtk_widget_destroy (dialog);
	g_clear_object (&gpgctx);

	return success;
}

typedef struct _PrintData {
	GSList *hidden_parts; /* EMailPart * */
	GAsyncReadyCallback callback;
	gpointer user_data;
} PrintData;

static void
print_data_free (gpointer ptr)
{
	PrintData *pd = ptr;

	if (pd) {
		GSList *link;

		for (link = pd->hidden_parts; link; link = g_slist_next (link)) {
			EMailPart *part = link->data;

			part->is_hidden = FALSE;
		}

		g_slist_free_full (pd->hidden_parts, g_object_unref);
		g_free (pd);
	}
}

static void
em_utils_print_part_list_done_cb (GObject *source_object,
				  GAsyncResult *result,
				  gpointer user_data)
{
	PrintData *pd = user_data;

	g_return_if_fail (pd != NULL);

	if (pd->callback)
		pd->callback (source_object, result, pd->user_data);

	print_data_free (pd);
}

void
em_utils_print_part_list (EMailPartList *part_list,
			  EMailDisplay *mail_display,
			  GtkPrintOperationAction print_action,
			  GCancellable *cancellable,
			  GAsyncReadyCallback callback,
			  gpointer user_data)
{
	EMailFormatter *formatter;
	EMailPrinter *printer;
	EMailRemoteContent *remote_content;
	PrintData *pd;
	gchar *export_basename;

	g_return_if_fail (E_IS_MAIL_PART_LIST (part_list));
	g_return_if_fail (E_IS_MAIL_DISPLAY (mail_display));

	pd = g_new0 (PrintData, 1);
	pd->callback = callback;
	pd->user_data = user_data;

	formatter = e_mail_display_get_formatter (mail_display);
	remote_content = e_mail_display_ref_remote_content (mail_display);

	if (e_mail_display_get_skip_insecure_parts (mail_display)) {
		GList *head, *link;
		GHashTable *secured_message_ids;
		GQueue queue = G_QUEUE_INIT;

		e_mail_part_list_queue_parts (part_list, NULL, &queue);

		head = g_queue_peek_head_link (&queue);
		secured_message_ids = e_mail_formatter_utils_extract_secured_message_ids (head);

		if (secured_message_ids) {
			gboolean has_encrypted_part = FALSE;

			for (link = head; link != NULL; link = g_list_next (link)) {
				EMailPart *part = E_MAIL_PART (link->data);

				if (!e_mail_formatter_utils_consider_as_secured_part (part, secured_message_ids))
					continue;

				if (!e_mail_part_has_validity (part)) {
					if (!part->is_hidden) {
						part->is_hidden = TRUE;
						pd->hidden_parts = g_slist_prepend (pd->hidden_parts, g_object_ref (part));
					}
					continue;
				}

				if (e_mail_part_get_validity (part, E_MAIL_PART_VALIDITY_ENCRYPTED)) {
					/* consider the second and following encrypted parts as evil */
					if (has_encrypted_part) {
						if (!part->is_hidden) {
							part->is_hidden = TRUE;
							pd->hidden_parts = g_slist_prepend (pd->hidden_parts, g_object_ref (part));
						}
					} else {
						has_encrypted_part = TRUE;
					}
				}
			}
		}

		while (!g_queue_is_empty (&queue))
			g_object_unref (g_queue_pop_head (&queue));

		g_clear_pointer (&secured_message_ids, g_hash_table_destroy);
	}

	printer = e_mail_printer_new (part_list, remote_content);
	if (e_mail_part_list_get_folder (part_list)) {
		export_basename = em_utils_build_export_basename (
			e_mail_part_list_get_folder (part_list),
			e_mail_part_list_get_message_uid (part_list),
			NULL);
	} else {
		CamelMimeMessage *msg;

		msg = e_mail_part_list_get_message (part_list);
		if (msg) {
			export_basename = em_utils_build_export_basename_internal (
				camel_mime_message_get_subject (msg),
				camel_mime_message_get_date (msg, NULL),
				NULL);
		} else {
			export_basename = NULL;
		}
	}

	e_util_make_safe_filename (export_basename);
	e_mail_printer_set_export_filename (printer, export_basename);
	g_free (export_basename);

	if (e_mail_display_get_mode (mail_display) == E_MAIL_FORMATTER_MODE_SOURCE)
		e_mail_printer_set_mode (printer, E_MAIL_FORMATTER_MODE_SOURCE);

	g_clear_object (&remote_content);

	e_mail_printer_print (
		printer,
		print_action,
		formatter,
		cancellable,
		em_utils_print_part_list_done_cb,
		pd);

	g_object_unref (printer);
}

gboolean
em_utils_print_part_list_finish (GObject *source_object,
				 GAsyncResult *result,
				 GError **error)
{
	g_return_val_if_fail (E_IS_MAIL_PRINTER (source_object), FALSE);

	return e_mail_printer_print_finish (E_MAIL_PRINTER (source_object), result, error);
}

gchar *
em_utils_select_folder_for_copy_move_message (GtkWindow *parent,
					      gboolean is_move,
					      CamelFolder *folder)
{
	/* Remembers the previously selected folder when transferring messages. */
	static gchar default_xfer_messages_uri[512] = { 0, };

	EMFolderSelector *selector;
	EMFolderTree *folder_tree;
	EMFolderTreeModel *model;
	GSettings *settings;
	GtkWidget *dialog;
	gchar *res_uri = NULL;

	model = em_folder_tree_model_get_default ();

	dialog = em_folder_selector_new (parent, model);

	gtk_window_set_title (GTK_WINDOW (dialog), is_move ? _("Move to Folder") : _("Copy to Folder"));

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_can_create (selector, TRUE);
	em_folder_selector_set_default_button_label (selector, is_move ? _("_Move") : _("C_opy"));

	folder_tree = em_folder_selector_get_folder_tree (selector);

	em_folder_tree_set_excluded (
		folder_tree,
		EMFT_EXCLUDE_NOSELECT |
		EMFT_EXCLUDE_VIRTUAL |
		EMFT_EXCLUDE_VTRASH);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (!g_settings_get_boolean (settings, "copy-move-to-folder-preserve-expand"))
		gtk_tree_view_expand_all (GTK_TREE_VIEW (folder_tree));

	g_clear_object (&settings);

	em_folder_selector_maybe_collapse_archive_folders (selector);

	if (*default_xfer_messages_uri) {
		em_folder_tree_set_selected (folder_tree, default_xfer_messages_uri, FALSE);
	} else if (folder) {
		gchar *furi = e_mail_folder_uri_from_folder (folder);

		if (furi) {
			em_folder_tree_set_selected (folder_tree, furi, FALSE);
			g_free (furi);
		}
	}

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		const gchar *uri;

		uri = em_folder_selector_get_selected_uri (EM_FOLDER_SELECTOR (dialog));

		if (uri) {
			if (g_snprintf (default_xfer_messages_uri, sizeof (default_xfer_messages_uri), "%s", uri) >= sizeof (default_xfer_messages_uri))
				default_xfer_messages_uri[0] = '\0';

			res_uri = g_strdup (uri);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));

	return res_uri;
}
