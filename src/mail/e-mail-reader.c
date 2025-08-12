/*
 * e-mail-reader.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-mail-reader.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <shell/e-shell-utils.h>

#include <libemail-engine/libemail-engine.h>

#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-part-utils.h>

#include "e-mail-backend.h"
#include "e-mail-browser.h"
#include "e-mail-enumtypes.h"
#include "e-mail-label-dialog.h"
#include "e-mail-label-list-store.h"
#include "e-mail-notes.h"
#include "e-mail-reader-utils.h"
#include "e-mail-remote-content-popover.h"
#include "e-mail-ui-session.h"
#include "e-mail-view.h"
#include "em-composer-utils.h"
#include "em-event.h"
#include "em-folder-selector.h"
#include "em-folder-tree.h"
#include "em-utils.h"
#include "mail-autofilter.h"
#include "mail-vfolder-ui.h"
#include "message-list.h"

#define E_MAIL_READER_GET_PRIVATE(obj) \
	((EMailReaderPrivate *) g_object_get_qdata \
	(G_OBJECT (obj), quark_private))

#define d(x)

typedef struct _EMailReaderClosure EMailReaderClosure;
typedef struct _EMailReaderPrivate EMailReaderPrivate;

struct _EMailReaderClosure {
	EMailReader *reader;
	EActivity *activity;
	CamelMimeMessage *message;
	CamelFolder *folder;
	gchar *message_uid;
	gboolean selection_is_html;
};

struct _EMailReaderPrivate {

	EMailForwardStyle forward_style;
	EMailReplyStyle reply_style;

	/* This timer runs when the user selects a single message. */
	guint message_selected_timeout_id;

	/* This allows message retrieval to be cancelled if another
	 * message is selected before the retrieval has completed. */
	GCancellable *retrieving_message;

	/* These flags work to prevent a folder switch from
	 * automatically marking the message as read. We only want
	 * that to happen when the -user- selects a message. */
	guint folder_was_just_selected : 1;
	guint avoid_next_mark_as_seen : 1;
	guint did_try_to_open_message : 1;

	guint group_by_threads : 1;
	guint mark_seen_always : 1;
	guint delete_selects_previous : 1;

	/* to be able to start the mark_seen timeout only after
	 * the message is loaded into the EMailDisplay */
	gboolean schedule_mark_seen;
	guint schedule_mark_seen_interval;

	gpointer followup_alert; /* weak pointer to an EAlert */

	GSList *ongoing_operations; /* GCancellable * */

	GMenuModel *reply_group_menu;
	GMenuModel *forward_as_menu;
	GMenu *labels_menu;
};

enum {
	CHANGED,
	COMPOSER_CREATED,
	FOLDER_LOADED,
	MESSAGE_LOADED,
	MESSAGE_SEEN,
	SHOW_SEARCH_BAR,
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

static GQuark quark_private;
static guint signals[LAST_SIGNAL];

G_DEFINE_INTERFACE (EMailReader, e_mail_reader, G_TYPE_INITIALLY_UNOWNED)

static void
mail_reader_set_display_formatter_for_message (EMailReader *reader,
                                               EMailDisplay *display,
                                               const gchar *message_uid,
                                               CamelMimeMessage *message,
                                               CamelFolder *folder);

static void
mail_reader_closure_free (EMailReaderClosure *closure)
{
	g_clear_object (&closure->reader);
	g_clear_object (&closure->activity);
	g_clear_object (&closure->folder);
	g_clear_object (&closure->message);
	g_free (closure->message_uid);

	g_slice_free (EMailReaderClosure, closure);
}

static void
mail_reader_private_free (EMailReaderPrivate *priv)
{
	if (priv->message_selected_timeout_id > 0)
		g_source_remove (priv->message_selected_timeout_id);

	if (priv->retrieving_message != NULL) {
		g_cancellable_cancel (priv->retrieving_message);
		g_object_unref (priv->retrieving_message);
		priv->retrieving_message = NULL;
	}

	g_clear_object (&priv->reply_group_menu);
	g_clear_object (&priv->forward_as_menu);
	g_clear_object (&priv->labels_menu);

	g_free (priv);
}

static void
action_mail_add_sender_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EMailReader *reader = user_data;
	EShell *shell;
	EMailBackend *backend;
	EMailSession *session;
	EShellBackend *shell_backend;
	CamelInternetAddress *cia;
	CamelMessageInfo *info = NULL;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *address;
	const gchar *message_uid;

	folder = e_mail_reader_ref_folder (reader);
	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len == 1);
	message_uid = g_ptr_array_index (uids, 0);

	info = camel_folder_get_message_info (folder, message_uid);
	if (info == NULL)
		goto exit;

	address = camel_message_info_get_from (info);
	if (address == NULL || *address == '\0')
		goto exit;

	/* XXX EBookShellBackend should be listening for this
	 *     event.  Kind of kludgey, but works for now. */
	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	e_shell_event (shell, "contact-quick-add-email", (gpointer) address);

	/* Remove this address from the photo cache. */
	cia = camel_internet_address_new ();
	if (camel_address_decode (CAMEL_ADDRESS (cia), address) > 0) {
		EPhotoCache *photo_cache;
		const gchar *address_only = NULL;

		photo_cache = e_mail_ui_session_get_photo_cache (
			E_MAIL_UI_SESSION (session));
		if (camel_internet_address_get (cia, 0, NULL, &address_only))
			e_photo_cache_remove_photo (photo_cache, address_only);
	}
	g_object_unref (cia);

exit:
	g_clear_object (&info);
	g_ptr_array_unref (uids);

	g_clear_object (&folder);
}

static void
action_add_to_address_book_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	EShell *shell;
	EMailBackend *backend;
	EMailDisplay *display;
	EMailSession *session;
	EShellBackend *shell_backend;
	CamelInternetAddress *cia;
	EPhotoCache *photo_cache;
	EWebView *web_view;
	CamelURL *curl;
	const gchar *uri;
	const gchar *address_only = NULL;
	gchar *email;

	/* This action is defined in EMailDisplay. */

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	display = e_mail_reader_get_mail_display (reader);
	if (display == NULL)
		return;

	web_view = E_WEB_VIEW (display);
	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	if (curl->path == NULL || *curl->path == '\0')
		goto exit;

	cia = camel_internet_address_new ();
	if (camel_address_decode (CAMEL_ADDRESS (cia), curl->path) < 0) {
		g_object_unref (cia);
		goto exit;
	}

	/* XXX EBookShellBackend should be listening for this
	 *     event.  Kind of kludgey, but works for now. */
	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	email = camel_address_format (CAMEL_ADDRESS (cia));
	e_shell_event (shell, "contact-quick-add-email", email);
	g_free (email);

	/* Remove this address from the photo cache. */
	photo_cache = e_mail_ui_session_get_photo_cache (
		E_MAIL_UI_SESSION (session));
	if (camel_internet_address_get (cia, 0, NULL, &address_only))
		e_photo_cache_remove_photo (photo_cache, address_only);

	g_object_unref (cia);

exit:
	camel_url_free (curl);
}

static void
action_mail_check_for_junk_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailBackend *backend;
	EMailSession *session;
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_ref_folder (reader);
	backend = e_mail_reader_get_backend (reader);
	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);

	session = e_mail_backend_get_session (backend);

	mail_filter_folder (
		session, folder, uids,
		E_FILTER_SOURCE_JUNKTEST, FALSE);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
mail_reader_copy_or_move_selected_messages (EMailReader *reader,
					    gboolean is_move)
{
	CamelFolder *folder;
	EMailBackend *backend;
	EMailSession *session;
	GtkWindow *window;
	GPtrArray *uids;
	gchar *folder_uri;

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	folder = e_mail_reader_ref_folder (reader);
	window = e_mail_reader_get_window (reader);
	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);
	folder_uri = em_utils_select_folder_for_copy_move_message (window, is_move, folder);

	if (folder_uri)
		mail_transfer_messages (session, folder, uids, is_move, folder_uri, 0, NULL, NULL);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
	g_free (folder_uri);
}

static void
mail_reader_manage_color_flag_on_selection (EMailReader *reader,
					    const gchar *color)
{
	CamelFolder *folder;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	folder = e_mail_reader_ref_folder (reader);

	if (folder != NULL) {
		GPtrArray *uids;
		guint ii;

		camel_folder_freeze (folder);

		uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);

		for (ii = 0; ii < uids->len; ii++) {
			CamelMessageInfo *info;

			info = camel_folder_get_message_info (folder, uids->pdata[ii]);
			if (info) {
				camel_message_info_set_user_tag (info, "color", color);
				g_object_unref (info);
			}
		}

		g_ptr_array_unref (uids);

		camel_folder_thaw (folder);

		g_object_unref (folder);
	}
}

static void
action_mail_color_assign_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWidget *dialog;

	dialog = gtk_color_chooser_dialog_new (NULL, e_mail_reader_get_window (reader));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		GdkRGBA rgba;
		gchar *color;

		gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &rgba);

		color = g_strdup_printf ("#%02X%02X%02X",
			0xFF & ((gint) (255 * rgba.red)),
			0xFF & ((gint) (255 * rgba.green)),
			0xFF & ((gint) (255 * rgba.blue)));

		if (color) {
			mail_reader_manage_color_flag_on_selection (reader, color);

			g_free (color);
		}
	}

	gtk_widget_destroy (dialog);
}

static void
action_mail_color_unset_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailReader *reader = user_data;
	mail_reader_manage_color_flag_on_selection (reader, NULL);
}

static void
action_mail_copy_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EMailReader *reader = user_data;
	mail_reader_copy_or_move_selected_messages (reader, FALSE);
}

static gboolean
mail_reader_replace_vee_folder_with_real (CamelFolder **inout_folder,
					  const gchar *uid,
					  gchar **out_real_uid)
{
	g_return_val_if_fail (inout_folder != NULL, FALSE);
	g_return_val_if_fail (CAMEL_IS_FOLDER (*inout_folder), FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);
	g_return_val_if_fail (out_real_uid != NULL, FALSE);

	*out_real_uid = NULL;

	if (CAMEL_IS_VEE_FOLDER (*inout_folder)) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (*inout_folder, uid);
		if (info) {
			CamelFolder *real_folder;

			real_folder = camel_vee_folder_get_location (CAMEL_VEE_FOLDER (*inout_folder), CAMEL_VEE_MESSAGE_INFO (info), out_real_uid);

			if (real_folder && *out_real_uid) {
				g_object_unref (*inout_folder);

				*inout_folder = g_object_ref (real_folder);
			}

			g_object_unref (info);
		} else {
			g_warn_if_reached ();
		}
	}

	return *out_real_uid != NULL;
}

static void
action_mail_edit_note_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	if (uids && uids->len == 1) {
		gchar *real_uid = NULL;
		const gchar *uid = uids->pdata[0];

		if (mail_reader_replace_vee_folder_with_real (&folder, uid, &real_uid))
			uid = real_uid;

		e_mail_notes_edit (e_mail_reader_get_window (reader), folder, uid);

		g_free (real_uid);
	} else {
		g_warn_if_reached ();
	}

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

typedef struct {
	CamelFolder *folder;
	gchar *uid;
} DeleteNoteData;

static void
delete_note_data_free (gpointer ptr)
{
	DeleteNoteData *dnd = ptr;

	if (dnd) {
		g_clear_object (&dnd->folder);
		g_free (dnd->uid);
		g_slice_free (DeleteNoteData, dnd);
	}
}

static void
mail_delete_note_thread (EAlertSinkThreadJobData *job_data,
			 gpointer user_data,
			 GCancellable *cancellable,
			 GError **error)
{
	DeleteNoteData *dnd = user_data;

	g_return_if_fail (dnd != NULL);
	g_return_if_fail (CAMEL_IS_FOLDER (dnd->folder));
	g_return_if_fail (dnd->uid != NULL);

	e_mail_notes_remove_sync (dnd->folder, dnd->uid, cancellable, error);
}

static void
action_mail_delete_note_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	if (uids && uids->len == 1) {
		DeleteNoteData *dnd;
		EAlertSink *alert_sink;
		EActivity *activity;
		gchar *full_display_name;
		gchar *real_uid = NULL;
		const gchar *uid = uids->pdata[0];

		if (mail_reader_replace_vee_folder_with_real (&folder, uid, &real_uid))
			uid = real_uid;

		dnd = g_slice_new0 (DeleteNoteData);
		dnd->folder = g_object_ref (folder);
		dnd->uid = g_strdup (uid);

		full_display_name = e_mail_folder_to_full_display_name (folder, NULL);
		alert_sink = e_mail_reader_get_alert_sink (reader);

		activity = e_alert_sink_submit_thread_job (alert_sink,
			_("Deleting message note…"),
			"mail:failed-delete-note",
			full_display_name ? full_display_name : camel_folder_get_full_name (folder),
			mail_delete_note_thread, dnd, delete_note_data_free);

		if (activity)
			e_shell_backend_add_activity (E_SHELL_BACKEND (e_mail_reader_get_backend (reader)), activity);

		g_clear_object (&activity);
		g_free (full_display_name);
		g_free (real_uid);
	} else {
		g_warn_if_reached ();
	}

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_delete_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EMailReader *reader = user_data;
	guint32 mask = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED;
	guint32 set = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED;

	if (!e_mail_reader_confirm_delete (reader))
		return;

	/* FIXME Verify all selected messages are deletable.
	 *       But handle it by disabling this action. */

	if (e_mail_reader_mark_selected (reader, mask, set) != 0 &&
	    !e_mail_reader_close_on_delete_or_junk (reader)) {
		if (e_mail_reader_get_delete_selects_previous (reader))
			e_mail_reader_select_previous_message (reader, FALSE);
		else
			e_mail_reader_select_next_message (reader, FALSE);
	}
}

static void
action_mail_filter_on_mailing_list_cb (EUIAction *action,
				       GVariant *parameter,
				       gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_create_filter_from_selected (reader, AUTO_MLIST);
}

static void
action_mail_filter_on_recipients_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_create_filter_from_selected (reader, AUTO_TO);
}

static void
action_mail_filter_on_sender_cb (EUIAction *action,
				 GVariant *parameter,
				 gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_create_filter_from_selected (reader, AUTO_FROM);
}

static void
action_mail_filter_on_subject_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_create_filter_from_selected (reader, AUTO_SUBJECT);
}

static void
action_mail_filters_apply_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailBackend *backend;
	EMailSession *session;
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_ref_folder (reader);
	backend = e_mail_reader_get_backend (reader);
	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);

	session = e_mail_backend_get_session (backend);

	mail_filter_folder (
		session, folder, uids,
		E_FILTER_SOURCE_DEMAND, FALSE);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_remove_attachments_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_remove_attachments (reader);
}

static void
action_mail_remove_duplicates_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_remove_duplicates (reader);
}

static void
action_mail_find_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_show_search_bar (reader);
}

static void
action_mail_flag_clear_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);
	window = e_mail_reader_get_window (reader);

	em_utils_flag_for_followup_clear (window, folder, uids);

	e_mail_reader_reload (reader);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_flag_completed_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);
	window = e_mail_reader_get_window (reader);

	em_utils_flag_for_followup_completed (window, folder, uids);

	e_mail_reader_reload (reader);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_flag_for_followup_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);

	em_utils_flag_for_followup (reader, folder, uids);

	e_mail_reader_reload (reader);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_forward_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWindow *window;
	GPtrArray *uids;

	window = e_mail_reader_get_window (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL);

	if (em_utils_ask_open_many (window, uids->len)) {
		CamelFolder *folder;

		folder = e_mail_reader_ref_folder (reader);

		e_mail_reader_forward_messages (
			reader, folder, uids,
			e_mail_reader_get_forward_style (reader));

		g_clear_object (&folder);
	}

	g_ptr_array_unref (uids);
}

static void
action_mail_forward_attached_cb (EUIAction *action,
				 GVariant *parameter,
				 gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWindow *window;
	GPtrArray *uids;

	window = e_mail_reader_get_window (reader);
	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);
	g_return_if_fail (uids != NULL);

	if (em_utils_ask_open_many (window, uids->len)) {
		CamelFolder *folder;

		folder = e_mail_reader_ref_folder (reader);

		e_mail_reader_forward_messages (
			reader, folder, uids,
			E_MAIL_FORWARD_STYLE_ATTACHED);

		g_clear_object (&folder);
	}

	g_ptr_array_unref (uids);
}

static void
action_mail_forward_inline_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWindow *window;
	GPtrArray *uids;

	window = e_mail_reader_get_window (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL);

	if (em_utils_ask_open_many (window, uids->len)) {
		CamelFolder *folder;

		folder = e_mail_reader_ref_folder (reader);

		e_mail_reader_forward_messages (
			reader, folder, uids,
			E_MAIL_FORWARD_STYLE_INLINE);

		g_clear_object (&folder);
	}

	g_ptr_array_unref (uids);
}

static void
action_mail_forward_quoted_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWindow *window;
	GPtrArray *uids;

	window = e_mail_reader_get_window (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL);

	if (em_utils_ask_open_many (window, uids->len)) {
		CamelFolder *folder;

		folder = e_mail_reader_ref_folder (reader);

		e_mail_reader_forward_messages (
			reader, folder, uids,
			E_MAIL_FORWARD_STYLE_QUOTED);

		g_clear_object (&folder);
	}

	g_ptr_array_unref (uids);
}

static void
action_mail_goto_containing_folder_cb (EUIAction *action,
				       GVariant *parameter,
				       gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *message_uid;

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len == 1);
	message_uid = g_ptr_array_index (uids, 0);
	folder = e_mail_reader_ref_folder (reader);

	if (CAMEL_IS_VEE_FOLDER (folder)) {
		CamelMessageInfo *mi;

		mi = camel_folder_get_message_info (folder, message_uid);
		if (mi) {
			CamelFolder *real_folder;
			gchar *real_uid = NULL;

			real_folder = camel_vee_folder_get_location (CAMEL_VEE_FOLDER (folder), (const CamelVeeMessageInfo *) mi, &real_uid);

			if (real_folder) {
				GtkWindow *window;
				MessageList *ml;

				ml = MESSAGE_LIST (e_mail_reader_get_message_list (reader));

				message_list_freeze (ml);
				e_mail_reader_set_folder (reader, real_folder);
				e_mail_reader_set_message (reader, real_uid);
				message_list_thaw (ml);

				window = e_mail_reader_get_window (reader);
				if (E_IS_SHELL_WINDOW (window)) {
					EShellWindow *shell_window = E_SHELL_WINDOW (window);
					EShellView *shell_view;

					shell_view = e_shell_window_get_shell_view (shell_window, e_shell_window_get_active_view (shell_window));
					if (shell_view) {
						EShellSidebar *shell_sidebar;

						shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
						if (shell_sidebar) {
							EMFolderTree *folder_tree = NULL;

							g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);

							if (folder_tree) {
								gchar *furi = e_mail_folder_uri_from_folder (real_folder);

								if (furi) {
									em_folder_tree_set_selected (folder_tree, furi, FALSE);
									g_free (furi);
								}

								g_clear_object (&folder_tree);
							}
						}
					}
				}
			}

			g_free (real_uid);
			g_clear_object (&mi);
		}
	} else {
		g_warn_if_reached ();
	}

	g_ptr_array_unref (uids);
	g_clear_object (&folder);
}

static void
action_mail_label_change_more_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data);

static void
action_mail_label_new_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailLabelDialog *label_dialog;
	EMailLabelListStore *label_store;
	EMailBackend *backend;
	EMailSession *session;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *dialog;
	GPtrArray *uids;
	GdkRGBA label_color;
	const gchar *label_name;
	gchar *label_tag;
	gint n_children;
	guint ii;
	gboolean repeat = TRUE;

	dialog = e_mail_label_dialog_new (e_mail_reader_get_window (reader));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Add Label"));

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (E_MAIL_UI_SESSION (session));
	label_dialog = E_MAIL_LABEL_DIALOG (dialog);

	while (repeat) {
		repeat = FALSE;

		if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
			break;

		label_name = e_mail_label_dialog_get_label_name (label_dialog);
		e_mail_label_dialog_get_label_color (label_dialog, &label_color);

		if (e_mail_label_list_store_lookup_by_name (label_store, label_name, NULL)) {
			repeat = TRUE;
			e_alert_run_dialog_for_args (
				GTK_WINDOW (dialog),
				"mail:error-label-exists", label_name, NULL);
			continue;
		}

		e_mail_label_list_store_set (
			label_store, NULL, label_name, &label_color);

		/* XXX This is awkward.  We've added a new label to the list store
		 *     but we don't have the new label's tag nor an iterator to use
		 *     to fetch it.  We know the label was appended to the store,
		 *     so we have to dig it out manually.  EMailLabelListStore API
		 *     probably needs some rethinking. */
		model = GTK_TREE_MODEL (label_store);
		n_children = gtk_tree_model_iter_n_children (model, NULL);
		g_warn_if_fail (gtk_tree_model_iter_nth_child (model, &iter, NULL, n_children - 1));
		label_tag = e_mail_label_list_store_get_tag (label_store, &iter);

		uids = e_mail_reader_get_selected_uids (reader);
		if (uids) {
			CamelFolder *folder;

			folder = e_mail_reader_ref_folder (reader);

			for (ii = 0; ii < uids->len; ii++) {
				CamelMessageInfo *nfo;

				nfo = camel_folder_get_message_info (folder, uids->pdata[ii]);
				if (!nfo)
					continue;

				camel_message_info_set_user_flag (nfo, label_tag, TRUE);

				g_clear_object (&nfo);
			}

			g_clear_object (&folder);
			g_ptr_array_unref (uids);
		}

		g_free (label_tag);
	}

	gtk_widget_destroy (dialog);
}

static void
action_mail_label_none_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailBackend *backend;
	EMailSession *session;
	EMailLabelListStore *label_store;
	CamelFolder *folder;
	GtkTreeIter iter;
	GPtrArray *uids;
	gboolean valid;
	guint ii;

	uids = e_mail_reader_get_selected_uids (reader);
	if (!uids)
		return;

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	folder = e_mail_reader_ref_folder (reader);

	valid = gtk_tree_model_get_iter_first (
		GTK_TREE_MODEL (label_store), &iter);

	while (valid) {
		gchar *tag;

		tag = e_mail_label_list_store_get_tag (label_store, &iter);

		for (ii = 0; ii < uids->len; ii++) {
			CamelMessageInfo *nfo;

			nfo = camel_folder_get_message_info (folder, uids->pdata[ii]);
			if (nfo) {
				camel_message_info_set_user_flag (nfo, tag, FALSE);
				camel_message_info_set_user_tag (nfo, "label", NULL);
				g_clear_object (&nfo);
			}
		}

		g_free (tag);

		valid = gtk_tree_model_iter_next (
			GTK_TREE_MODEL (label_store), &iter);
	}

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_load_images_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailDisplay *display;

	display = e_mail_reader_get_mail_display (reader);

	e_mail_display_load_images (display);
}

static void
action_mail_mark_important_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	guint32 mask = CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_DELETED;
	guint32 set = CAMEL_MESSAGE_FLAGGED;

	e_mail_reader_mark_selected (reader, mask, set);
}

static void
action_mail_mark_junk_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EMailReader *reader = user_data;
	guint32 mask =
		CAMEL_MESSAGE_SEEN |
		CAMEL_MESSAGE_JUNK |
		CAMEL_MESSAGE_NOTJUNK |
		CAMEL_MESSAGE_JUNK_LEARN;
	guint32 set =
		CAMEL_MESSAGE_SEEN |
		CAMEL_MESSAGE_JUNK |
		CAMEL_MESSAGE_JUNK_LEARN;
	GPtrArray *uids;
	gchar *primary;
	gboolean can_do_it;

	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);

	if (!uids || !uids->len) {
		if (uids)
			g_ptr_array_unref (uids);
		return;
	}

	primary = g_strdup_printf (ngettext ("Are you sure you want to mark %d message as Junk?",
					     "Are you sure you want to mark %d messages as Junk?",
					     uids->len), uids->len);

	can_do_it = e_util_prompt_user (e_mail_reader_get_window (reader),
		"org.gnome.evolution.mail", "prompt-on-mark-as-junk",
		"mail:ask-mark-as-junk",
		primary,
		ngettext ("The message will be shown in the Junk folder.",
			  "The messages will be shown in the Junk folder.",
			  uids->len),
		NULL);

	g_ptr_array_unref (uids);
	g_free (primary);

	if (!can_do_it)
		return;

	if (e_mail_reader_mark_selected (reader, mask, set) != 0 &&
	    !e_mail_reader_close_on_delete_or_junk (reader)) {
		if (e_mail_reader_get_delete_selects_previous (reader))
			e_mail_reader_select_previous_message (reader, TRUE);
		else
			e_mail_reader_select_next_message (reader, TRUE);
	}
}

static void
action_mail_mark_notjunk_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailReader *reader = user_data;
	guint32 mask =
		CAMEL_MESSAGE_JUNK |
		CAMEL_MESSAGE_NOTJUNK |
		CAMEL_MESSAGE_JUNK_LEARN;
	guint32 set  =
		CAMEL_MESSAGE_NOTJUNK |
		CAMEL_MESSAGE_JUNK_LEARN;

	if (e_mail_reader_mark_selected (reader, mask, set) != 0) {
		if (e_mail_reader_get_delete_selects_previous (reader))
			e_mail_reader_select_previous_message (reader, TRUE);
		else
			e_mail_reader_select_next_message (reader, TRUE);
	}
}

static void
action_mail_mark_read_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EMailReader *reader = user_data;
	guint32 mask = CAMEL_MESSAGE_SEEN;
	guint32 set = CAMEL_MESSAGE_SEEN;

	e_mail_reader_mark_selected (reader, mask, set);
}

static void
action_mail_mark_unimportant_cb (EUIAction *action,
				 GVariant *parameter,
				 gpointer user_data)
{
	EMailReader *reader = user_data;
	guint32 mask = CAMEL_MESSAGE_FLAGGED;
	guint32 set = 0;

	e_mail_reader_mark_selected (reader, mask, set);
}

static void
action_mail_mark_ignore_thread_sub_cb (EUIAction *action,
				       GVariant *parameter,
				       gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_mark_selected_ignore_thread (reader, E_IGNORE_THREAD_SUBSET_SET);
}

static void
action_mail_mark_unignore_thread_sub_cb (EUIAction *action,
					 GVariant *parameter,
					 gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_mark_selected_ignore_thread (reader, E_IGNORE_THREAD_SUBSET_UNSET);
}

static void
action_mail_mark_ignore_thread_whole_cb (EUIAction *action,
					 GVariant *parameter,
					 gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_mark_selected_ignore_thread (reader, E_IGNORE_THREAD_WHOLE_SET);
}

static void
action_mail_mark_unignore_thread_whole_cb (EUIAction *action,
					   GVariant *parameter,
					   gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_mark_selected_ignore_thread (reader, E_IGNORE_THREAD_WHOLE_UNSET);
}

static void
action_mail_mark_unread_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWidget *message_list;
	EMFolderTreeModel *model;
	CamelFolder *folder;
	guint32 mask = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED;
	guint32 set = 0;
	guint n_marked;

	message_list = e_mail_reader_get_message_list (reader);

	n_marked = e_mail_reader_mark_selected (reader, mask, set);

	if (MESSAGE_LIST (message_list)->seen_id != 0) {
		g_source_remove (MESSAGE_LIST (message_list)->seen_id);
		MESSAGE_LIST (message_list)->seen_id = 0;
	}

	folder = e_mail_reader_ref_folder (reader);

	/* Notify the tree model that the user has marked messages as
	 * unread so it doesn't mistake the event as new mail arriving. */
	model = em_folder_tree_model_get_default ();
	em_folder_tree_model_user_marked_unread (model, folder, n_marked);

	g_clear_object (&folder);
}

static void
action_mail_message_edit_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailReader *reader = user_data;
	EShell *shell;
	EMailBackend *backend;
	ESourceRegistry *registry;
	CamelFolder *folder;
	GPtrArray *uids;
	gboolean replace;
	gboolean keep_signature;

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL);

	backend = e_mail_reader_get_backend (reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));
	registry = e_shell_get_registry (shell);

	folder = e_mail_reader_ref_folder (reader);
	replace = em_utils_folder_is_drafts (registry, folder);
	keep_signature = replace ||
		em_utils_folder_is_sent (registry, folder) ||
		em_utils_folder_is_outbox (registry, folder);
	e_mail_reader_edit_messages (reader, folder, uids, replace, keep_signature);
	g_clear_object (&folder);

	g_ptr_array_unref (uids);
}

typedef struct _CreateComposerData {
	EMailReader *reader;
	CamelMimeMessage *message;
	CamelFolder *folder;
	const gchar *message_uid; /* In the Camel string pool */
	gboolean is_redirect;
} CreateComposerData;

static void
mail_reader_new_composer_created_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (ccd != NULL);

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		if (ccd->is_redirect)
			em_utils_redirect_message (composer, ccd->message);
		else
			em_utils_compose_new_message_with_selection (composer, ccd->folder, ccd->message_uid);

		e_mail_reader_composer_created (ccd->reader, composer, ccd->message);
	}

	g_clear_object (&ccd->reader);
	g_clear_object (&ccd->message);
	g_clear_object (&ccd->folder);
	camel_pstring_free (ccd->message_uid);
	g_slice_free (CreateComposerData, ccd);
}

static void
action_mail_message_new_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailReader *reader = user_data;
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	CamelFolder *folder;
	CreateComposerData *ccd;
	GPtrArray *selected_uids = NULL;
	const gchar *selected_uid = NULL;

	folder = e_mail_reader_ref_folder (reader);
	backend = e_mail_reader_get_backend (reader);

	selected_uids = e_mail_reader_get_selected_uids (reader);
	if (selected_uids && selected_uids->len > 0)
		selected_uid = g_ptr_array_index (selected_uids, 0);

	if (!selected_uid) {
		GtkWidget *message_list;

		message_list = e_mail_reader_get_message_list (reader);
		if (message_list)
			selected_uid = MESSAGE_LIST (message_list)->cursor_uid;
	}

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	ccd = g_slice_new0 (CreateComposerData);
	ccd->reader = g_object_ref (reader);
	ccd->folder = folder;
	ccd->message_uid = camel_pstring_strdup (selected_uid);
	ccd->is_redirect = FALSE;

	e_msg_composer_new (shell, mail_reader_new_composer_created_cb, ccd);

	if (selected_uids)
		g_ptr_array_unref (selected_uids);
}

static void
action_mail_message_open_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailReader *reader = user_data;

	if (E_IS_MAIL_BROWSER (reader))
		return;

	e_mail_reader_open_selected_mail (reader);
}

static void
action_mail_archive_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelFolder *folder;
	EMailBackend *backend;
	EMailSession *session;
	GPtrArray *uids;
	gchar *archive_folder;

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);
	g_return_if_fail (uids != NULL);

	folder = e_mail_reader_ref_folder (reader);

	if (CAMEL_IS_VEE_FOLDER (folder) && uids->len > 1) {
		GHashTable *split_by_folder; /* CamelFolder * ~> GPtrArray { gchar * uids } */
		GHashTableIter iter;
		gpointer key, value;
		guint ii, n_successful = 0, n_failed = 0;

		split_by_folder = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, (GDestroyNotify) g_ptr_array_unref);

		for (ii = 0; ii < uids->len; ii++) {
			const gchar *uid = g_ptr_array_index (uids, ii);
			CamelFolder *real_folder = NULL;
			gchar *real_message_uid = NULL;

			em_utils_get_real_folder_and_message_uid (folder, uid, &real_folder, NULL, &real_message_uid);

			if (real_folder && real_message_uid) {
				GPtrArray *array;

				array = g_hash_table_lookup (split_by_folder, real_folder);

				if (array) {
					g_object_unref (real_folder);
				} else {
					array = g_ptr_array_new_with_free_func (g_free);
					g_hash_table_insert (split_by_folder, real_folder, array);
				}

				g_ptr_array_add (array, real_message_uid);
			} else {
				g_clear_object (&real_folder);
				g_free (real_message_uid);
			}
		}

		g_hash_table_iter_init (&iter, split_by_folder);

		while (g_hash_table_iter_next (&iter, &key, &value)) {
			CamelFolder *real_folder = key;
			GPtrArray *real_uids = value;

			archive_folder = em_utils_get_archive_folder_uri_from_folder (real_folder, backend, real_uids, TRUE);

			if (archive_folder && *archive_folder) {
				n_successful++;
				mail_transfer_messages (
					session, folder, uids,
					TRUE, archive_folder, 0, NULL, NULL);
			} else {
				n_failed++;
			}

			g_free (archive_folder);
		}

		g_hash_table_destroy (split_by_folder);

		if (n_failed && !n_successful) {
			EAlertSink *alert_sink;

			alert_sink = e_mail_reader_get_alert_sink (reader);

			e_alert_submit (
				alert_sink, "mail:no-archive-folder",
				NULL);
		}
	} else {
		archive_folder = em_utils_get_archive_folder_uri_from_folder (folder, backend, uids, TRUE);

		if (archive_folder && *archive_folder) {
			mail_transfer_messages (
				session, folder, uids,
				TRUE, archive_folder, 0, NULL, NULL);
		} else {
			EAlertSink *alert_sink;

			alert_sink = e_mail_reader_get_alert_sink (reader);

			e_alert_submit (
				alert_sink, "mail:no-archive-folder",
				NULL);
		}

		g_free (archive_folder);
	}

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_move_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EMailReader *reader = user_data;

	mail_reader_copy_or_move_selected_messages (reader, TRUE);
}

static void
action_mail_next_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_NEXT;
	flags = 0;
	mask = 0;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
}

static void
action_mail_next_important_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_NEXT | MESSAGE_LIST_SELECT_WRAP;
	flags = CAMEL_MESSAGE_FLAGGED;
	mask = CAMEL_MESSAGE_FLAGGED;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
}

static void
action_mail_next_thread_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_next_thread (MESSAGE_LIST (message_list));
}

static void
mail_reader_select_unread (EMailReader *reader,
			   gboolean move_forward)
{
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	direction = (move_forward ? MESSAGE_LIST_SELECT_NEXT : MESSAGE_LIST_SELECT_PREVIOUS) |
		    MESSAGE_LIST_SELECT_WRAP | MESSAGE_LIST_SELECT_INCLUDE_COLLAPSED;
	flags = 0;
	mask = CAMEL_MESSAGE_SEEN;

	message_list = e_mail_reader_get_message_list (reader);

	if (!message_list_select (MESSAGE_LIST (message_list), direction, flags, mask)) {
		GtkWindow *window;

		window = e_mail_reader_get_window (reader);
		if (E_IS_SHELL_WINDOW (window)) {
			EShellWindow *shell_window;
			EMFolderTree *folder_tree = NULL;
			const gchar *active_view;

			shell_window = E_SHELL_WINDOW (window);
			active_view = e_shell_window_get_active_view (shell_window);

			if (g_strcmp0 (active_view, "mail") == 0) {
				EShellView *shell_view;
				EShellSidebar *shell_sidebar;

				shell_view = e_shell_window_get_shell_view (shell_window, "mail");
				shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
				g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);

				if (folder_tree) {
					gboolean selected;

					if (move_forward)
						selected = em_folder_tree_select_next_path (folder_tree, TRUE);
					else
						selected = em_folder_tree_select_prev_path (folder_tree, TRUE);

					if (selected)
						message_list_set_regen_selects_unread (MESSAGE_LIST (message_list), TRUE);
				}

				g_clear_object (&folder_tree);
			}
		}
	}
}

static void
action_mail_next_unread_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailReader *reader = user_data;

	mail_reader_select_unread (reader, TRUE);
}

static void
action_mail_previous_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_PREVIOUS;
	flags = 0;
	mask = 0;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
}

static void
action_mail_previous_important_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_PREVIOUS | MESSAGE_LIST_SELECT_WRAP;
	flags = CAMEL_MESSAGE_FLAGGED;
	mask = CAMEL_MESSAGE_FLAGGED;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
}

static void
action_mail_previous_thread_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_prev_thread (MESSAGE_LIST (message_list));
}

static void
action_mail_previous_unread_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EMailReader *reader = user_data;
	mail_reader_select_unread (reader, FALSE);
}

static void
action_mail_print_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkPrintOperationAction print_action;

	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_mail_reader_print (reader, print_action);
}

static void
action_mail_print_preview_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkPrintOperationAction print_action;

	print_action = GTK_PRINT_OPERATION_ACTION_PREVIEW;
	e_mail_reader_print (reader, print_action);
}

static void
mail_reader_redirect_cb (CamelFolder *folder,
                         GAsyncResult *result,
                         EMailReaderClosure *closure)
{
	EShell *shell;
	EMailBackend *backend;
	EAlertSink *alert_sink;
	CamelMimeMessage *message;
	CreateComposerData *ccd;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (closure->activity);

	message = camel_folder_get_message_finish (folder, result, &error);

	if (e_activity_handle_cancellation (closure->activity, error)) {
		g_warn_if_fail (message == NULL);
		mail_reader_closure_free (closure);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (message == NULL);
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		mail_reader_closure_free (closure);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	backend = e_mail_reader_get_backend (closure->reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	ccd = g_slice_new0 (CreateComposerData);
	ccd->reader = g_object_ref (closure->reader);
	ccd->message = message;
	ccd->message_uid = camel_pstring_strdup (closure->message_uid);
	ccd->is_redirect = TRUE;

	e_msg_composer_new (shell, mail_reader_new_composer_created_cb, ccd);

	mail_reader_closure_free (closure);
}

static void
action_mail_redirect_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailReader *reader = user_data;
	EActivity *activity;
	GCancellable *cancellable;
	EMailReaderClosure *closure;
	GtkWidget *message_list;
	CamelFolder *folder;
	const gchar *message_uid;

	message_list = e_mail_reader_get_message_list (reader);
	message_uid = MESSAGE_LIST (message_list)->cursor_uid;
	g_return_if_fail (message_uid != NULL);

	/* Open the message asynchronously. */

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	closure = g_slice_new0 (EMailReaderClosure);
	closure->activity = activity;
	closure->reader = g_object_ref (reader);
	closure->message_uid = g_strdup (message_uid);

	folder = e_mail_reader_ref_folder (reader);

	camel_folder_get_message (
		folder, message_uid, G_PRIORITY_DEFAULT,
		cancellable, (GAsyncReadyCallback)
		mail_reader_redirect_cb, closure);

	g_clear_object (&folder);
}

static void
action_mail_reply_all_check (CamelFolder *folder,
                             GAsyncResult *result,
                             EMailReaderClosure *closure)
{
	EAlertSink *alert_sink;
	CamelMimeMessage *message;
	CamelInternetAddress *to, *cc;
	gint recip_count = 0;
	EMailReplyType type = E_MAIL_REPLY_TO_ALL;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (closure->activity);

	message = camel_folder_get_message_finish (folder, result, &error);

	if (e_activity_handle_cancellation (closure->activity, error)) {
		g_warn_if_fail (message == NULL);
		mail_reader_closure_free (closure);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (message == NULL);
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		mail_reader_closure_free (closure);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	to = camel_mime_message_get_recipients (
		message, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (
		message, CAMEL_RECIPIENT_TYPE_CC);

	recip_count = camel_address_length (CAMEL_ADDRESS (to));
	recip_count += camel_address_length (CAMEL_ADDRESS (cc));

	if (recip_count >= 15) {
		GtkWidget *dialog;
		GtkWidget *check;
		GtkWidget *container;
		gint response;

		dialog = e_alert_dialog_new_for_args (
			e_mail_reader_get_window (closure->reader),
			"mail:ask-reply-many-recips", NULL);

		container = e_alert_dialog_get_content_area (
			E_ALERT_DIALOG (dialog));

		/* Check buttons */
		check = gtk_check_button_new_with_mnemonic (
			_("_Do not ask me again."));
		gtk_box_pack_start (
			GTK_BOX (container), check, FALSE, FALSE, 0);
		gtk_widget_show (check);

		response = gtk_dialog_run (GTK_DIALOG (dialog));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check))) {
			GSettings *settings;
			const gchar *key;

			settings = e_util_ref_settings ("org.gnome.evolution.mail");

			key = "prompt-on-reply-many-recips";
			g_settings_set_boolean (settings, key, FALSE);

			g_object_unref (settings);
		}

		gtk_widget_destroy (dialog);

		switch (response) {
			case GTK_RESPONSE_NO:
				type = E_MAIL_REPLY_TO_SENDER;
				break;
			case GTK_RESPONSE_CANCEL:
			case GTK_RESPONSE_DELETE_EVENT:
				goto exit;
			default:
				break;
		}
	}

	e_mail_reader_reply_to_message (closure->reader, message, type);

exit:
	g_object_unref (message);

	mail_reader_closure_free (closure);
}

static void
action_mail_reply_all_cb (EUIAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	EMailReader *reader = user_data;
	GSettings *settings;
	const gchar *key;
	guint32 state;
	gboolean ask;

	state = e_mail_reader_check_state (reader);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	key = "prompt-on-reply-many-recips";
	ask = g_settings_get_boolean (settings, key);

	g_object_unref (settings);

	if (ask && !(state & E_MAIL_READER_SELECTION_IS_MAILING_LIST)) {
		EActivity *activity;
		GCancellable *cancellable;
		EMailReaderClosure *closure;
		CamelFolder *folder;
		GtkWidget *message_list;
		const gchar *message_uid;

		message_list = e_mail_reader_get_message_list (reader);
		message_uid = MESSAGE_LIST (message_list)->cursor_uid;
		g_return_if_fail (message_uid != NULL);

		activity = e_mail_reader_new_activity (reader);
		cancellable = e_activity_get_cancellable (activity);

		closure = g_slice_new0 (EMailReaderClosure);
		closure->activity = activity;
		closure->reader = g_object_ref (reader);

		folder = e_mail_reader_ref_folder (reader);

		camel_folder_get_message (
			folder, message_uid, G_PRIORITY_DEFAULT,
			cancellable, (GAsyncReadyCallback)
			action_mail_reply_all_check, closure);

		g_clear_object (&folder);

		return;
	}

	e_mail_reader_reply_to_message (reader, NULL, E_MAIL_REPLY_TO_ALL);
}

static gboolean
emr_get_skip_insecure_parts (EMailReader *reader)
{
	if (!reader)
		return TRUE;

	return e_mail_display_get_skip_insecure_parts (e_mail_reader_get_mail_display (reader));
}

static void
action_mail_reply_alternative_got_message (GObject *source_object,
					   GAsyncResult *result,
					   gpointer user_data)
{
	EMailReaderClosure *closure = user_data;
	EAlertSink *alert_sink;
	CamelMimeMessage *message;
	gboolean is_selection;
	CamelFolder *folder = NULL;
	const gchar *message_uid = NULL;
	EMailPartList *part_list = NULL;
	EMailPartValidityFlags validity_pgp_sum = 0;
	EMailPartValidityFlags validity_smime_sum = 0;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (closure->activity);

	message = e_mail_reader_utils_get_selection_or_message_finish (E_MAIL_READER (source_object), result,
			&is_selection, &folder, &message_uid, &part_list, &validity_pgp_sum, &validity_smime_sum, &error);

	if (e_activity_handle_cancellation (closure->activity, error)) {
		g_warn_if_fail (message == NULL);
		mail_reader_closure_free (closure);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (message == NULL);
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			error->message, NULL);
		mail_reader_closure_free (closure);
		g_error_free (error);
		return;
	}

	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	g_clear_object (&closure->activity);

	em_utils_reply_alternative (e_mail_reader_get_window (closure->reader),
		e_shell_backend_get_shell (E_SHELL_BACKEND (e_mail_reader_get_backend (closure->reader))),
		alert_sink, message, folder, message_uid,
		e_mail_reader_get_reply_style (closure->reader),
		is_selection ? NULL : part_list, validity_pgp_sum, validity_smime_sum,
		!is_selection && emr_get_skip_insecure_parts (closure->reader));

	mail_reader_closure_free (closure);
	camel_pstring_free (message_uid);
	g_clear_object (&message);
	g_clear_object (&folder);
	g_clear_object (&part_list);
	g_clear_error (&error);
}

static void
action_mail_reply_alternative_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EMailReader *reader = user_data;
	EActivity *activity;
	GCancellable *cancellable;
	EMailReaderClosure *closure;
	GtkWidget *message_list;
	const gchar *message_uid;

	message_list = e_mail_reader_get_message_list (reader);
	message_uid = MESSAGE_LIST (message_list)->cursor_uid;
	g_return_if_fail (message_uid != NULL);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	closure = g_slice_new0 (EMailReaderClosure);
	closure->activity = activity;
	closure->reader = g_object_ref (reader);

	e_mail_reader_utils_get_selection_or_message (reader, NULL, cancellable,
		action_mail_reply_alternative_got_message, closure);
}

static void
action_mail_reply_group_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailReader *reader = user_data;
	GSettings *settings;
	gboolean reply_list;
	guint32 state;
	const gchar *key;

	state = e_mail_reader_check_state (reader);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	key = "composer-group-reply-to-list";
	reply_list = g_settings_get_boolean (settings, key);

	g_object_unref (settings);

	if (reply_list && (state & E_MAIL_READER_SELECTION_IS_MAILING_LIST)) {
		e_mail_reader_reply_to_message (
			reader, NULL, E_MAIL_REPLY_TO_LIST);
	} else
		action_mail_reply_all_cb (action, NULL, reader);
}

static void
action_mail_reply_list_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_reply_to_message (reader, NULL, E_MAIL_REPLY_TO_LIST);
}

static gboolean
message_is_list_administrative (CamelMimeMessage *message)
{
	const gchar *header;

	header = camel_medium_get_header (
		CAMEL_MEDIUM (message), "X-List-Administrivia");
	if (header == NULL)
		return FALSE;

	while (*header == ' ' || *header == '\t')
		header++;

	return g_ascii_strncasecmp (header, "yes", 3) == 0;
}

static gboolean
mail_reader_check_recipients_differ_inet (CamelInternetAddress *addr1,
					  CamelInternetAddress *addr2)
{
	gint ii, len;

	if (!addr1 || !addr2)
		return FALSE;

	len = camel_address_length (CAMEL_ADDRESS (addr1));

	if (len != camel_address_length (CAMEL_ADDRESS (addr2)))
		return TRUE;

	for (ii = 0; ii < len; ii++) {
		const gchar *email1 = NULL, *email2 = NULL;

		if (camel_internet_address_get (addr1, ii, NULL, &email1) &&
		    camel_internet_address_get (addr2, ii, NULL, &email2) &&
		    email1 && email2 && g_ascii_strcasecmp (email1, email2) != 0)
			return TRUE;
	}

	return FALSE;
}

static gboolean
mail_reader_check_recipients_differ_nntp (CamelNNTPAddress *addr1,
					  CamelNNTPAddress *addr2)
{
	gint ii, len;

	if (!addr1 || !addr2)
		return FALSE;

	len = camel_address_length (CAMEL_ADDRESS (addr1));

	if (len != camel_address_length (CAMEL_ADDRESS (addr2)))
		return TRUE;

	for (ii = 0; ii < len; ii++) {
		const gchar *name1 = NULL, *name2 = NULL;

		if (camel_nntp_address_get (addr1, ii, &name1) &&
		    camel_nntp_address_get (addr2, ii, &name2) &&
		    name1 && name2 && g_ascii_strcasecmp (name1, name2) != 0)
			return TRUE;
	}

	return FALSE;
}

static gboolean
mail_reader_get_recipients_differ (ESourceRegistry *registry,
				   CamelMimeMessage *message,
				   gboolean with_postto,
				   EMailReplyType reply_type1,
				   EMailReplyType reply_type2,
				   EMailReplyType reply_type3)
{
	CamelInternetAddress *to1, *to2;
	CamelInternetAddress *cc1, *cc2;
	CamelNNTPAddress *postto1 = NULL, *postto2 = NULL;
	gboolean differ;

	to1 = camel_internet_address_new ();
	cc1 = camel_internet_address_new ();
	to2 = camel_internet_address_new ();
	cc2 = camel_internet_address_new ();

	if (with_postto) {
		postto1 = camel_nntp_address_new ();
		postto2 = camel_nntp_address_new ();
	}

	em_utils_get_reply_recipients (registry, message, reply_type1, NULL, to1, cc1, postto1);
	em_utils_get_reply_recipients (registry, message, reply_type2, NULL, to2, cc2, postto2);

	differ = mail_reader_check_recipients_differ_inet (to1, to2) ||
		 mail_reader_check_recipients_differ_inet (cc1, cc2) ||
		 mail_reader_check_recipients_differ_nntp (postto1, postto2);

	if (!differ) {
		CamelAddress *addr;

		addr = CAMEL_ADDRESS (to2);
		while (camel_address_length (addr))
			camel_address_remove (addr, 0);

		addr = CAMEL_ADDRESS (cc2);
		while (camel_address_length (addr))
			camel_address_remove (addr, 0);

		if (postto2) {
			addr = CAMEL_ADDRESS (postto2);
			while (camel_address_length (addr))
				camel_address_remove (addr, 0);
		}

		em_utils_get_reply_recipients (registry, message, reply_type3, NULL, to2, cc2, postto2);

		differ = mail_reader_check_recipients_differ_inet (to1, to2) ||
			 mail_reader_check_recipients_differ_inet (cc1, cc2) ||
			 mail_reader_check_recipients_differ_nntp (postto1, postto2);
	}

	g_clear_object (&to1);
	g_clear_object (&to2);
	g_clear_object (&cc1);
	g_clear_object (&cc2);
	g_clear_object (&postto1);
	g_clear_object (&postto2);

	return differ;
}

static void
action_mail_reply_sender_check (CamelFolder *folder,
                                GAsyncResult *result,
                                EMailReaderClosure *closure)
{
	EAlertSink *alert_sink;
	CamelMimeMessage *message;
	EMailReplyType type = E_MAIL_REPLY_TO_SENDER;
	ESourceRegistry *registry;
	GSettings *settings;
	gboolean ask_ignore_list_reply_to;
	gboolean ask_list_reply_to;
	gboolean munged_list_message;
	gboolean active;
	const gchar *key;
	GError *local_error = NULL;

	alert_sink = e_activity_get_alert_sink (closure->activity);

	message = camel_folder_get_message_finish (
		folder, result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((message != NULL) && (local_error == NULL)) ||
		((message == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (closure->activity, local_error)) {
		mail_reader_closure_free (closure);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			local_error->message, NULL);
		mail_reader_closure_free (closure);
		g_error_free (local_error);
		return;
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	key = "composer-ignore-list-reply-to";
	ask_ignore_list_reply_to = g_settings_get_boolean (settings, key);

	key = "prompt-on-list-reply-to";
	ask_list_reply_to = g_settings_get_boolean (settings, key);

	munged_list_message = em_utils_is_munged_list_message (message);
	registry = e_shell_get_registry (e_shell_backend_get_shell (E_SHELL_BACKEND (e_mail_reader_get_backend (closure->reader))));

	if (message_is_list_administrative (message)) {
		/* Do not ask for messages which are list administrative,
		 * like list confirmation messages. */
	} else if (ask_ignore_list_reply_to || !munged_list_message) {
		/* Don't do the "Are you sure you want to reply in private?"
		 * pop-up if it's a Reply-To: munged list message... unless
		 * we're ignoring munging. */
		if (mail_reader_get_recipients_differ (registry, message, closure->folder != NULL,
		    E_MAIL_REPLY_TO_SENDER, E_MAIL_REPLY_TO_LIST, E_MAIL_REPLY_TO_ALL)) {
			GtkWidget *dialog;
			GtkWidget *check;
			GtkWidget *container;
			gint response;

			dialog = e_alert_dialog_new_for_args (
				e_mail_reader_get_window (closure->reader),
				"mail:ask-list-private-reply", NULL);

			container = e_alert_dialog_get_content_area (
				E_ALERT_DIALOG (dialog));

			/* Check buttons */
			check = gtk_check_button_new_with_mnemonic (
				_("_Do not ask me again."));
			gtk_box_pack_start (
				GTK_BOX (container), check, FALSE, FALSE, 0);
			gtk_widget_show (check);

			response = gtk_dialog_run (GTK_DIALOG (dialog));

			active = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (check));
			if (active) {
				key = "prompt-on-private-list-reply";
				g_settings_set_boolean (settings, key, FALSE);
			}

			gtk_widget_destroy (dialog);

			if (response == GTK_RESPONSE_YES)
				type = E_MAIL_REPLY_TO_ALL;
			else if (response == GTK_RESPONSE_OK)
				type = E_MAIL_REPLY_TO_LIST;
			else if (response == GTK_RESPONSE_CANCEL ||
				response == GTK_RESPONSE_DELETE_EVENT) {
				goto exit;
			}
		}

	} else if (ask_list_reply_to && mail_reader_get_recipients_differ (registry, message, closure->folder != NULL,
		   E_MAIL_REPLY_TO_SENDER, E_MAIL_REPLY_TO_FROM, E_MAIL_REPLY_TO_LIST)) {
		GtkWidget *dialog;
		GtkWidget *container;
		GtkWidget *check_again;
		GtkWidget *check_always_ignore;
		gint response;

		dialog = e_alert_dialog_new_for_args (
			e_mail_reader_get_window (closure->reader),
			"mail:ask-list-honour-reply-to", NULL);

		container = e_alert_dialog_get_content_area (
			E_ALERT_DIALOG (dialog));

		check_again = gtk_check_button_new_with_mnemonic (
			_("_Do not ask me again."));
		gtk_box_pack_start (
			GTK_BOX (container), check_again, FALSE, FALSE, 0);
		gtk_widget_show (check_again);

		check_always_ignore = gtk_check_button_new_with_mnemonic (
			_("_Always ignore Reply-To: for mailing lists."));
		gtk_box_pack_start (
			GTK_BOX (container), check_always_ignore,
			FALSE, FALSE, 0);
		gtk_widget_show (check_always_ignore);

		response = gtk_dialog_run (GTK_DIALOG (dialog));

		active = gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (check_again));
		if (active) {
			key = "prompt-on-list-reply-to";
			g_settings_set_boolean (settings, key, FALSE);
		}

		key = "composer-ignore-list-reply-to";
		active = gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (check_always_ignore));
		g_settings_set_boolean (settings, key, active);

		gtk_widget_destroy (dialog);

		switch (response) {
			case GTK_RESPONSE_NO:
				type = E_MAIL_REPLY_TO_FROM;
				break;
			case GTK_RESPONSE_OK:
				type = E_MAIL_REPLY_TO_LIST;
				break;
			case GTK_RESPONSE_CANCEL:
			case GTK_RESPONSE_DELETE_EVENT:
				goto exit;
			default:
				break;
		}
	}

	e_mail_reader_reply_to_message (closure->reader, message, type);

exit:
	g_object_unref (settings);
	g_object_unref (message);

	mail_reader_closure_free (closure);
}

static void
action_mail_reply_sender_cb (EUIAction *action,
			     GVariant *parameter,
			     gpointer user_data)
{
	EMailReader *reader = user_data;
	GSettings *settings;
	gboolean ask_list_reply_to;
	gboolean ask_private_list_reply;
	gboolean ask;
	guint32 state;
	const gchar *key;

	state = e_mail_reader_check_state (reader);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	key = "prompt-on-list-reply-to";
	ask_list_reply_to = g_settings_get_boolean (settings, key);

	key = "prompt-on-private-list-reply";
	ask_private_list_reply = g_settings_get_boolean (settings, key);

	g_object_unref (settings);

	ask = (ask_private_list_reply || ask_list_reply_to);

	if (ask && (state & E_MAIL_READER_SELECTION_IS_MAILING_LIST)) {
		EActivity *activity;
		GCancellable *cancellable;
		EMailReaderClosure *closure;
		CamelFolder *folder;
		GtkWidget *message_list;
		const gchar *message_uid;

		message_list = e_mail_reader_get_message_list (reader);
		message_uid = MESSAGE_LIST (message_list)->cursor_uid;
		g_return_if_fail (message_uid != NULL);

		activity = e_mail_reader_new_activity (reader);
		cancellable = e_activity_get_cancellable (activity);

		closure = g_slice_new0 (EMailReaderClosure);
		closure->activity = activity;
		closure->reader = g_object_ref (reader);

		folder = e_mail_reader_ref_folder (reader);

		camel_folder_get_message (
			folder, message_uid, G_PRIORITY_DEFAULT,
			cancellable, (GAsyncReadyCallback)
			action_mail_reply_sender_check, closure);

		g_clear_object (&folder);

		return;
	}

	e_mail_reader_reply_to_message (reader, NULL, E_MAIL_REPLY_TO_SENDER);
}

static void
action_mail_reply_recipient_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_reply_to_message (reader, NULL, E_MAIL_REPLY_TO_RECIPIENT);
}

static void
action_mail_save_as_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_save_messages (reader);
}

static void
action_mail_search_folder_from_mailing_list_cb (EUIAction *action,
						GVariant *parameter,
						gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_create_vfolder_from_selected (reader, AUTO_MLIST);
}

static void
action_mail_search_folder_from_recipients_cb (EUIAction *action,
					      GVariant *parameter,
					      gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_create_vfolder_from_selected (reader, AUTO_TO);
}

static void
action_mail_search_folder_from_sender_cb (EUIAction *action,
					  GVariant *parameter,
					  gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_create_vfolder_from_selected (reader, AUTO_FROM);
}

static void
action_mail_search_folder_from_subject_cb (EUIAction *action,
					   GVariant *parameter,
					   gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_create_vfolder_from_selected (reader, AUTO_SUBJECT);
}

static void
e_mail_reader_update_display_mode (EMailReader *self)
{
	EMailDisplay *display;
	EMailFormatterMode mode;
	EUIAction *action;

	display = e_mail_reader_get_mail_display (self);

	/* Ignore action when viewing message source. */
	mode = e_mail_display_get_mode (display);
	if (mode == E_MAIL_FORMATTER_MODE_SOURCE)
		return;
	if (mode == E_MAIL_FORMATTER_MODE_RAW)
		return;

	action = e_mail_reader_get_action (self, "mail-show-all-headers");

	if (e_ui_action_get_active (action))
		mode = E_MAIL_FORMATTER_MODE_ALL_HEADERS;
	else
		mode = E_MAIL_FORMATTER_MODE_NORMAL;

	e_mail_display_set_mode (display, mode);
}

static void
mail_source_retrieved (GObject *source_object,
                       GAsyncResult *result,
                       gpointer user_data)
{
	EMailReaderClosure *closure;
	CamelMimeMessage *message;
	EMailDisplay *display;
	GError *error = NULL;

	closure = (EMailReaderClosure *) user_data;
	display = e_mail_reader_get_mail_display (closure->reader);

	message = camel_folder_get_message_finish (
		CAMEL_FOLDER (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((message != NULL) && (error == NULL)) ||
		((message == NULL) && (error != NULL)));

	if (message != NULL) {
		mail_reader_set_display_formatter_for_message (
			closure->reader, display,
			closure->message_uid, message,
			CAMEL_FOLDER (source_object));
		g_object_unref (message);
	} else if (error) {
		if (display) {
			gchar *status;

			status = g_strdup_printf (
				"%s<br>%s",
				_("Failed to retrieve message:"),
				error->message);
			e_mail_display_set_status (display, status);
			g_free (status);
		}

		g_error_free (error);
	}

	e_activity_set_state (closure->activity, E_ACTIVITY_COMPLETED);

	mail_reader_closure_free (closure);
}

static void
action_mail_show_source_cb (EUIAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailDisplay *display;
	EMailBackend *backend;
	GtkWidget *browser;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *message_uid;
	gchar *string;
	EActivity *activity;
	GCancellable *cancellable;
	EMailReaderClosure *closure;
	MessageList *ml;

	backend = e_mail_reader_get_backend (reader);
	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len == 1);
	message_uid = g_ptr_array_index (uids, 0);

	if (!E_IS_MAIL_BROWSER (e_mail_reader_get_window (reader))) {
		EMailBrowser *mail_browser;

		mail_browser = em_utils_find_message_window (E_MAIL_FORMATTER_MODE_SOURCE, folder, message_uid);

		if (mail_browser) {
			gtk_window_present (GTK_WINDOW (mail_browser));
			g_ptr_array_unref (uids);
			g_clear_object (&folder);
			return;
		}
	}

	browser = e_mail_browser_new (backend, E_MAIL_FORMATTER_MODE_SOURCE);
	ml = MESSAGE_LIST (e_mail_reader_get_message_list (E_MAIL_READER (browser)));

	message_list_freeze (ml);
	e_mail_reader_set_folder (E_MAIL_READER (browser), folder);
	e_mail_reader_set_message (E_MAIL_READER (browser), message_uid);
	message_list_thaw (ml);

	display = e_mail_reader_get_mail_display (E_MAIL_READER (browser));

	string = g_strdup_printf (_("Retrieving message “%s”"), message_uid);
	e_mail_display_set_part_list (display, NULL);
	e_mail_display_set_status (display, string);
	gtk_widget_show (browser);

	activity = e_mail_reader_new_activity (E_MAIL_READER (browser));
	e_activity_set_text (activity, string);
	cancellable = e_activity_get_cancellable (activity);
	g_free (string);

	closure = g_slice_new0 (EMailReaderClosure);
	closure->reader = E_MAIL_READER (g_object_ref (browser));
	closure->activity = g_object_ref (activity);
	closure->message_uid = g_strdup (message_uid);

	camel_folder_get_message (
		folder, message_uid, G_PRIORITY_DEFAULT,
		cancellable, mail_source_retrieved, closure);

	g_object_unref (activity);

	g_ptr_array_unref (uids);

	g_clear_object (&folder);
}

static void
action_mail_toggle_important_cb (EUIAction *action,
				 GVariant *parameter,
				 gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelFolder *folder;
	GPtrArray *uids;
	guint ii;

	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);
	if (!uids)
		return;

	folder = e_mail_reader_ref_folder (reader);

	camel_folder_freeze (folder);

	for (ii = 0; ii < uids->len; ii++) {
		guint32 flags;

		flags = camel_folder_get_message_flags (
			folder, uids->pdata[ii]);
		flags ^= CAMEL_MESSAGE_FLAGGED;
		if (flags & CAMEL_MESSAGE_FLAGGED)
			flags &= ~CAMEL_MESSAGE_DELETED;

		camel_folder_set_message_flags (
			folder, uids->pdata[ii], CAMEL_MESSAGE_FLAGGED |
			CAMEL_MESSAGE_DELETED, flags);
	}

	camel_folder_thaw (folder);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

static void
action_mail_undelete_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailReader *reader = user_data;
	guint32 mask = CAMEL_MESSAGE_DELETED;
	guint32 set = 0;

	e_mail_reader_mark_selected (reader, mask, set);
}

static void
action_mail_zoom_100_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailDisplay *display;

	display = e_mail_reader_get_mail_display (reader);

	e_web_view_zoom_100 (E_WEB_VIEW (display));
}

static void
action_mail_zoom_in_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailDisplay *display;

	display = e_mail_reader_get_mail_display (reader);

	e_web_view_zoom_in (E_WEB_VIEW (display));
}

static void
action_mail_zoom_out_cb (EUIAction *action,
			 GVariant *parameter,
			 gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailDisplay *display;

	display = e_mail_reader_get_mail_display (reader);

	e_web_view_zoom_out (E_WEB_VIEW (display));
}

static void
action_mail_search_web_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailDisplay *display;
	EUIAction *wv_action;

	display = e_mail_reader_get_mail_display (reader);
	wv_action = e_web_view_get_action (E_WEB_VIEW (display), "search-web");

	g_action_activate (G_ACTION (wv_action), NULL);
}

static void
action_search_folder_recipient_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailBackend *backend;
	EMailSession *session;
	EWebView *web_view;
	CamelURL *curl;
	const gchar *uri;

	/* This action is defined in EMailDisplay. */

	web_view = E_WEB_VIEW (e_mail_reader_get_mail_display (reader));

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	if (curl->path != NULL && *curl->path != '\0') {
		CamelFolder *folder;
		CamelInternetAddress *inet_addr;

		folder = e_mail_reader_ref_folder (reader);

		inet_addr = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
		vfolder_gui_add_from_address (
			session, inet_addr, AUTO_TO, folder);
		g_object_unref (inet_addr);

		g_clear_object (&folder);
	}

	camel_url_free (curl);
}

static void
action_search_folder_sender_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EMailReader *reader = user_data;
	EMailBackend *backend;
	EMailSession *session;
	EWebView *web_view;
	CamelURL *curl;
	const gchar *uri;

	/* This action is defined in EMailDisplay. */

	web_view = E_WEB_VIEW (e_mail_reader_get_mail_display (reader));

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	if (curl->path != NULL && *curl->path != '\0') {
		CamelFolder *folder;
		CamelInternetAddress *inet_addr;

		folder = e_mail_reader_ref_folder (reader);

		inet_addr = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
		vfolder_gui_add_from_address (
			session, inet_addr, AUTO_FROM, folder);
		g_object_unref (inet_addr);

		g_clear_object (&folder);
	}

	camel_url_free (curl);
}

static void
charset_menu_change_state_cb (EUIAction *action,
			      GVariant *parameter,
			      gpointer user_data)
{
	EMailReader *self = user_data;
	EMailFormatter *formatter;
	EMailDisplay *mail_display;

	g_return_if_fail (E_IS_MAIL_READER (self));

	e_ui_action_set_state (action, parameter);

	mail_display = e_mail_reader_get_mail_display (self);
	formatter = mail_display ? e_mail_display_get_formatter (mail_display) : NULL;

	if (formatter) {
		const gchar *charset;

		charset = g_variant_get_string (parameter, NULL);

		/* Default value is an empty string in the GMenu, but a NULL for the formatter */
		if (charset && !*charset)
			charset = NULL;

		e_mail_formatter_set_charset (formatter, charset);
	}
}

static gboolean
e_mail_reader_ui_manager_create_item_cb (EUIManager *ui_manager,
					 EUIElement *elem,
					 EUIAction *action,
					 EUIElementKind for_kind,
					 GObject **out_item,
					 gpointer user_data)
{
	EMailReader *self = user_data;
	EMailReaderPrivate *priv;
	const gchar *name;

	g_return_val_if_fail (E_IS_MAIL_READER (self), FALSE);

	name = g_action_get_name (G_ACTION (action));

	if (!g_str_has_prefix (name, "EMailReader::"))
		return FALSE;

	priv = E_MAIL_READER_GET_PRIVATE (self);

	#define is_action(_nm) (g_strcmp0 (name, (_nm)) == 0)

	if (is_action ("EMailReader::mail-reply-group") ||
	    is_action ("EMailReader::mail-forward-as-group")) {
		EUIAction *tool_action;
		GMenuModel *menu_model;

		if (is_action ("EMailReader::mail-reply-group")) {
			tool_action = e_ui_manager_get_action (ui_manager, "mail-reply-group");
			menu_model = priv->reply_group_menu;
		} else {
			tool_action = e_ui_manager_get_action (ui_manager, "mail-forward");
			menu_model = priv->forward_as_menu;
		}

		*out_item = e_ui_manager_create_item_from_menu_model (ui_manager, elem, tool_action, for_kind, menu_model);
	} else if (for_kind == E_UI_ELEMENT_KIND_MENU) {
		if (is_action ("EMailReader::charset-menu")) {
			GMenu *charset_menu;
			GMenuItem *menu_item;
			EMailDisplay *mail_display;

			charset_menu = g_menu_new ();

			menu_item = g_menu_item_new (_("_Default"), NULL);
			g_menu_item_set_action_and_target (menu_item, "mail.EMailReader::charset-menu", "s", "");
			g_menu_append_item (charset_menu, menu_item);
			g_clear_object (&menu_item);

			e_charset_add_to_g_menu (charset_menu, "mail.EMailReader::charset-menu");

			*out_item = G_OBJECT (g_menu_item_new_submenu (e_ui_action_get_label (action), G_MENU_MODEL (charset_menu)));

			g_clear_object (&charset_menu);

			mail_display = e_mail_reader_get_mail_display (self);

			if (mail_display) {
				EMailFormatter *formatter;

				formatter = e_mail_display_get_formatter (mail_display);
				if (formatter) {
					const gchar *charset;

					charset = e_mail_formatter_get_charset (formatter);
					e_ui_action_set_state (action, g_variant_new_string (charset ? charset : ""));
				} else {
					e_ui_action_set_state (action, g_variant_new_string (""));
				}
			} else {
				e_ui_action_set_state (action, g_variant_new_string (""));
			}
		} else if (is_action ("EMailReader::mail-label-actions")) {
			*out_item = G_OBJECT (g_menu_item_new_section (NULL, G_MENU_MODEL (priv->labels_menu)));
		} else {
			g_warning ("%s: Unhandled menu action '%s'", G_STRFUNC, name);
		}
	} else if (for_kind == E_UI_ELEMENT_KIND_TOOLBAR) {
		g_warning ("%s: Unhandled toolbar action '%s'", G_STRFUNC, name);
	} else if (for_kind == E_UI_ELEMENT_KIND_HEADERBAR) {
		g_warning ("%s: Unhandled headerbar action '%s'", G_STRFUNC, name);
	} else {
		g_warning ("%s: Unhandled element kind '%d' for action '%s'", G_STRFUNC, (gint) for_kind, name);
	}

	#undef is_action

	return TRUE;
}

void
e_mail_reader_init_ui_data_default (EMailReader *self)
{
	static const EUIActionEntry mail_entries[] = {

		{ "mail-add-sender",
		  NULL,
		  N_("A_dd Sender to Address Book"),
		  NULL,
		  N_("Add sender to address book"),
		  action_mail_add_sender_cb, NULL, NULL, NULL },

		{ "mail-archive",
		  "mail-archive",
		  N_("_Archive"),
		  "<Alt><Control>a",
		  N_("Move selected messages to the Archive folder for the account"),
		  action_mail_archive_cb, NULL, NULL, NULL },

		{ "mail-check-for-junk",
		  "mail-mark-junk",
		  N_("Check for _Junk"),
		  "<Control><Alt>j",
		  N_("Filter the selected messages for junk status"),
		  action_mail_check_for_junk_cb, NULL, NULL, NULL },

		{ "mail-color-assign",
		  NULL,
		  N_("Assign C_olor…"),
		  NULL,
		  N_("Assign color for the selected messages"),
		  action_mail_color_assign_cb, NULL, NULL, NULL },

		{ "mail-color-unset",
		  NULL,
		  N_("Unse_t Color"),
		  NULL,
		  N_("Unset color for the selected messages"),
		  action_mail_color_unset_cb, NULL, NULL, NULL },

		{ "mail-copy",
		  "mail-copy",
		  N_("_Copy to Folder…"),
		  "<Shift><Control>y",
		  N_("Copy selected messages to another folder"),
		  action_mail_copy_cb, NULL, NULL, NULL },

		{ "mail-delete",
		  "user-trash",
		  N_("_Delete Message"),
		  "<Control>d",
		  N_("Mark the selected messages for deletion"),
		  action_mail_delete_cb, NULL, NULL, NULL },

		{ "mail-add-note",
		  "evolution-memos",
		  N_("_Add note…"),
		  NULL,
		  N_("Add a note for the selected message"),
		  action_mail_edit_note_cb, NULL, NULL, NULL },

		{ "mail-delete-note",
		  NULL,
		  N_("Delete no_te"),
		  NULL,
		  N_("Delete the note for the selected message"),
		  action_mail_delete_note_cb, NULL, NULL, NULL },

		{ "mail-edit-note",
		  "evolution-memos",
		  N_("_Edit note…"),
		  NULL,
		  N_("Edit a note for the selected message"),
		  action_mail_edit_note_cb, NULL, NULL, NULL },

		{ "mail-filter-rule-for-mailing-list",
		  NULL,
		  N_("Create a Filter Rule for Mailing _List…"),
		  NULL,
		  N_("Create a rule to filter messages to this mailing list"),
		  action_mail_filter_on_mailing_list_cb, NULL, NULL, NULL },

		{ "mail-filter-rule-for-recipients",
		  NULL,
		  N_("Create a Filter Rule for _Recipients…"),
		  NULL,
		  N_("Create a rule to filter messages to these recipients"),
		  action_mail_filter_on_recipients_cb, NULL, NULL, NULL },

		{ "mail-filter-rule-for-sender",
		  NULL,
		  N_("Create a Filter Rule for Se_nder…"),
		  NULL,
		  N_("Create a rule to filter messages from this sender"),
		  action_mail_filter_on_sender_cb, NULL, NULL, NULL },

		{ "mail-filter-rule-for-subject",
		  NULL,
		  N_("Create a Filter Rule for _Subject…"),
		  NULL,
		  N_("Create a rule to filter messages with this subject"),
		  action_mail_filter_on_subject_cb, NULL, NULL, NULL },

		{ "mail-filters-apply",
		  "stock_mail-filters-apply",
		  N_("A_pply Filters"),
		  "<Control>y",
		  N_("Apply filter rules to the selected messages"),
		  action_mail_filters_apply_cb, NULL, NULL, NULL },

		{ "mail-find",
		  "edit-find",
		  N_("_Find in Message…"),
		  "<Shift><Control>f",
		  N_("Search for text in the body of the displayed message"),
		  action_mail_find_cb, NULL, NULL, NULL },

		{ "mail-flag-clear",
		  NULL,
		  N_("_Clear Flag"),
		  NULL,
		  N_("Remove the follow-up flag from the selected messages"),
		  action_mail_flag_clear_cb, NULL, NULL, NULL },

		{ "mail-flag-completed",
		  NULL,
		  N_("_Flag Completed"),
		  NULL,
		  N_("Set the follow-up flag to completed on the selected messages"),
		  action_mail_flag_completed_cb, NULL, NULL, NULL },

		{ "mail-flag-for-followup",
		  "stock_mail-flag-for-followup",
		  N_("Follow _Up…"),
		  "<Shift><Control>g",
		  N_("Flag the selected messages for follow-up"),
		  action_mail_flag_for_followup_cb, NULL, NULL, NULL },

		{ "mail-flag-for-followup-full",
		  "stock_mail-flag-for-followup",
		  N_("Mark for Follo_w Up…"),
		  NULL,
		  N_("Flag the selected messages for follow-up"),
		  action_mail_flag_for_followup_cb, NULL, NULL, NULL },

		{ "mail-forward",
		  "mail-forward",
		  N_("_Forward"),
		  "<Control><Alt>f",
		  N_("Forward the selected message to someone"),
		  action_mail_forward_cb, NULL, NULL, NULL },

		{ "mail-forward-attached",
		  NULL,
		  N_("_Attached"),
		  NULL,
		  N_("Forward the selected message to someone as an attachment"),
		  action_mail_forward_attached_cb, NULL, NULL, NULL },

		{ "mail-forward-attached-full",
		  NULL,
		  N_("Forward As _Attached"),
		  NULL,
		  N_("Forward the selected message to someone as an attachment"),
		  action_mail_forward_attached_cb, NULL, NULL, NULL },

		{ "mail-forward-inline",
		  NULL,
		  N_("_Inline"),
		  NULL,
		  N_("Forward the selected message in the body of a new message"),
		  action_mail_forward_inline_cb, NULL, NULL, NULL },

		{ "mail-forward-inline-full",
		  NULL,
		  N_("Forward As _Inline"),
		  NULL,
		  N_("Forward the selected message in the body of a new message"),
		  action_mail_forward_inline_cb, NULL, NULL, NULL },

		{ "mail-forward-quoted",
		  NULL,
		  N_("_Quoted"),
		  NULL,
		  N_("Forward the selected message quoted like a reply"),
		  action_mail_forward_quoted_cb, NULL, NULL, NULL },

		{ "mail-forward-quoted-full",
		  NULL,
		  N_("Forward As _Quoted"),
		  NULL,
		  N_("Forward the selected message quoted like a reply"),
		  action_mail_forward_quoted_cb, NULL, NULL, NULL },

		{ "mail-goto-containing-folder",
		  NULL,
		  N_("_Containing Folder"),
		  "<Control><Alt>o",
		  N_("Go to the folder the message originates from"),
		  action_mail_goto_containing_folder_cb, NULL, NULL, NULL },

		{ "mail-label-change-more",
		  NULL,
		  N_("Change _More Labels…"),
		  "l",
		  NULL,
		  action_mail_label_change_more_cb, NULL, NULL, NULL },

		{ "mail-label-new",
		  NULL,
		  N_("_New Label"),
		  NULL,
		  NULL,
		  action_mail_label_new_cb, NULL, NULL, NULL },

		{ "mail-label-none",
		  NULL,
		  /* Translators: "None" is used in the message label context menu.
		   *              It removes all labels from the selected messages. */
		  N_("N_one"),
		  "0",
		  NULL,
		  action_mail_label_none_cb, NULL, NULL, NULL },

		{ "mail-load-images",
		  "insert-image",
		  N_("_Load Images"),
		  "<Control>i",
		  N_("Force images in HTML mail to be loaded"),
		  action_mail_load_images_cb, NULL, NULL, NULL },

		{ "mail-mark-ignore-thread-sub",
		  NULL,
		  N_("_Ignore Subthread"),
		  NULL,
		  N_("Mark new mails in a subthread as read automatically"),
		  action_mail_mark_ignore_thread_sub_cb, NULL, NULL, NULL },

		{ "mail-mark-ignore-thread-whole",
		  NULL,
		  N_("_Ignore Thread"),
		  NULL,
		  N_("Mark new mails in this thread as read automatically"),
		  action_mail_mark_ignore_thread_whole_cb, NULL, NULL, NULL },

		{ "mail-mark-important",
		  "mail-mark-important",
		  N_("_Important"),
		  NULL,
		  N_("Mark the selected messages as important"),
		  action_mail_mark_important_cb, NULL, NULL, NULL },

		{ "mail-mark-important-full",
		  "mail-mark-important",
		  N_("Mark as _Important"),
		  NULL,
		  N_("Mark the selected messages as important"),
		  action_mail_mark_important_cb, NULL, NULL, NULL },

		{ "mail-mark-junk",
		  "mail-mark-junk",
		  N_("_Junk"),
		  "<Control>j",
		  N_("Mark the selected messages as junk"),
		  action_mail_mark_junk_cb, NULL, NULL, NULL },

		{ "mail-mark-junk-full",
		  "mail-mark-junk",
		  N_("Mark as _Junk"),
		  NULL,
		  N_("Mark the selected messages as junk"),
		  action_mail_mark_junk_cb, NULL, NULL, NULL },

		{ "mail-mark-notjunk",
		  "mail-mark-notjunk",
		  N_("_Not Junk"),
		  "<Shift><Control>j",
		  N_("Mark the selected messages as not being junk"),
		  action_mail_mark_notjunk_cb, NULL, NULL, NULL },

		{ "mail-mark-notjunk-full",
		  "mail-mark-notjunk",
		  N_("Mark as _Not Junk"),
		  NULL,
		  N_("Mark the selected messages as not being junk"),
		  action_mail_mark_notjunk_cb, NULL, NULL, NULL },

		{ "mail-mark-read",
		  "mail-mark-read",
		  N_("_Read"),
		  "<Control>k",
		  N_("Mark the selected messages as having been read"),
		  action_mail_mark_read_cb, NULL, NULL, NULL },

		{ "mail-mark-read-full",
		  "mail-mark-read",
		  N_("Mar_k as Read"),
		  "<Control>k",
		  N_("Mark the selected messages as having been read"),
		  action_mail_mark_read_cb, NULL, NULL, NULL },

		{ "mail-mark-unignore-thread-sub",
		  NULL,
		  N_("Do not _Ignore Subthread"),
		  NULL,
		  N_("Do not mark new mails in a subthread as read automatically"),
		  action_mail_mark_unignore_thread_sub_cb, NULL, NULL, NULL },

		{ "mail-mark-unignore-thread-whole",
		  NULL,
		  N_("Do not _Ignore Thread"),
		  NULL,
		  N_("Do not mark new mails in this thread as read automatically"),
		  action_mail_mark_unignore_thread_whole_cb, NULL, NULL, NULL },

		{ "mail-mark-unimportant",
		  NULL,
		  N_("Uni_mportant"),
		  NULL,
		  N_("Mark the selected messages as unimportant"),
		  action_mail_mark_unimportant_cb, NULL, NULL, NULL },

		{ "mail-mark-unimportant-full",
		  NULL,
		  N_("Mark as Uni_mportant"),
		  NULL,
		  N_("Mark the selected messages as unimportant"),
		  action_mail_mark_unimportant_cb, NULL, NULL, NULL },

		{ "mail-mark-unread",
		  "mail-mark-unread",
		  N_("_Unread"),
		  "<Shift><Control>k",
		  N_("Mark the selected messages as not having been read"),
		  action_mail_mark_unread_cb, NULL, NULL, NULL },

		{ "mail-mark-unread-full",
		  "mail-mark-unread",
		  N_("Mark as _Unread"),
		  NULL,
		  N_("Mark the selected messages as not having been read"),
		  action_mail_mark_unread_cb, NULL, NULL, NULL },

		{ "mail-message-edit",
		  NULL,
		  N_("_Edit as New Message…"),
		  NULL,
		  N_("Open the selected messages in the composer for editing"),
		  action_mail_message_edit_cb, NULL, NULL, NULL },

		{ "mail-message-new",
		  "mail-message-new",
		  N_("Compose _New Message"),
		  "<Shift><Control>m",
		  N_("Open a window for composing a mail message"),
		  action_mail_message_new_cb, NULL, NULL, NULL },

		{ "mail-message-open",
		  NULL,
		  N_("_Open in New Window"),
		  "<Control>o",
		  N_("Open the selected messages in a new window"),
		  action_mail_message_open_cb, NULL, NULL, NULL },

		{ "mail-move",
		  "mail-move",
		  N_("_Move to Folder…"),
		  "<Shift><Control>v",
		  N_("Move selected messages to another folder"),
		  action_mail_move_cb, NULL, NULL, NULL },

		{ "mail-next",
		  "go-next",
		  N_("_Next Message"),
		  "<Control>Page_Down",
		  N_("Display the next message"),
		  action_mail_next_cb, NULL, NULL, NULL },

		{ "mail-next-important",
		  NULL,
		  N_("Next _Important Message"),
		  NULL,
		  N_("Display the next important message"),
		  action_mail_next_important_cb, NULL, NULL, NULL },

		{ "mail-next-thread",
		  NULL,
		  N_("Next _Thread"),
		  NULL,
		  N_("Display the next thread"),
		  action_mail_next_thread_cb, NULL, NULL, NULL },

		{ "mail-next-unread",
		  "go-jump",
		  N_("Next _Unread Message"),
		  "<Control>bracketright",
		  N_("Display the next unread message"),
		  action_mail_next_unread_cb, NULL, NULL, NULL },

		{ "mail-previous",
		  "go-previous",
		  N_("_Previous Message"),
		  "<Control>Page_Up",
		  N_("Display the previous message"),
		  action_mail_previous_cb, NULL, NULL, NULL },

		{ "mail-previous-important",
		  NULL,
		  N_("Pr_evious Important Message"),
		  NULL,
		  N_("Display the previous important message"),
		  action_mail_previous_important_cb, NULL, NULL, NULL },

		{ "mail-previous-thread",
		  NULL,
		  N_("Previous T_hread"),
		  NULL,
		  N_("Display the previous thread"),
		  action_mail_previous_thread_cb, NULL, NULL, NULL },

		{ "mail-previous-unread",
		  NULL,
		  N_("P_revious Unread Message"),
		  "<Control>bracketleft",
		  N_("Display the previous unread message"),
		  action_mail_previous_unread_cb, NULL, NULL, NULL },

		{ "mail-print",
		  "document-print",
		  N_("_Print…"),
		  "<Control>p",
		  N_("Print this message"),
		  action_mail_print_cb, NULL, NULL, NULL },

		{ "mail-print-preview",
		  "document-print-preview",
		  N_("Pre_view…"),
		  NULL,
		  N_("Preview the message to be printed"),
		  action_mail_print_preview_cb, NULL, NULL, NULL },

		{ "mail-redirect",
		  NULL,
		  N_("Re_direct"),
		  NULL,
		  N_("Redirect (bounce) the selected message to someone"),
		  action_mail_redirect_cb, NULL, NULL, NULL },

		{ "mail-remove-attachments",
		  "edit-delete",
		  N_("Remo_ve Attachments"),
		  NULL,
		  N_("Remove attachments"),
		  action_mail_remove_attachments_cb, NULL, NULL, NULL },

		{ "mail-remove-duplicates",
		   NULL,
		   N_("Remove Du_plicate Messages"),
		   NULL,
		   N_("Checks selected messages for duplicates"),
		   action_mail_remove_duplicates_cb, NULL, NULL, NULL },

		{ "mail-reply-all",
		  NULL,
		  N_("Reply to _All"),
		  "<Shift><Control>r",
		  N_("Compose a reply to all the recipients of the selected message"),
		  action_mail_reply_all_cb, NULL, NULL, NULL },

		{ "mail-reply-alternative",
		  NULL,
		  N_("Al_ternative Reply…"),
		  "<Alt><Control>r",
		  N_("Choose reply options for the selected message"),
		  action_mail_reply_alternative_cb, NULL, NULL, NULL },

		{ "mail-reply-group",
		  "mail-reply-all",
		  N_("_Group Reply"),
		  NULL,
		  N_("Reply to the mailing list, or to all recipients"),
		  action_mail_reply_group_cb, NULL, NULL, NULL },

		{ "mail-reply-list",
		  NULL,
		  N_("Reply to _List"),
		  "<Control>l",
		  N_("Compose a reply to the mailing list of the selected message"),
		  action_mail_reply_list_cb, NULL, NULL, NULL },

		{ "mail-reply-sender",
		  "mail-reply-sender",
		  N_("_Reply to Sender"),
		  "<Control>r",
		  N_("Compose a reply to the sender of the selected message"),
		  action_mail_reply_sender_cb, NULL, NULL, NULL },

		{ "mail-save-as",
		  "document-save-as",
		  N_("_Save to File…"),
		  "<Control>s",
		  N_("Save selected messages as an mbox file"),
		  action_mail_save_as_cb, NULL, NULL, NULL },

		{ "mail-search-web",
		  NULL,
		  N_("Search _Web…"),
		  NULL,
		  N_("Search the Web with the selected text"),
		  action_mail_search_web_cb, NULL, NULL, NULL },

		{ "mail-show-source",
		  NULL,
		  N_("_Message Source"),
		  "<Control>u",
		  N_("Show the raw email source of the message"),
		  action_mail_show_source_cb, NULL, NULL, NULL },

		{ "mail-toggle-important",
		  NULL,
		  "Toggle important",  /* No menu item; key press only */
		  NULL,
		  NULL,
		  action_mail_toggle_important_cb, NULL, NULL, NULL },

		{ "mail-undelete",
		  NULL,
		  N_("_Undelete Message"),
		  "<Shift><Control>d",
		  N_("Undelete the selected messages"),
		  action_mail_undelete_cb, NULL, NULL, NULL },

		{ "mail-zoom-100",
		  "zoom-original",
		  N_("_Normal Size"),
		  "<Control>0",
		  N_("Reset the text to its original size"),
		  action_mail_zoom_100_cb, NULL, NULL, NULL },

		{ "mail-zoom-in",
		  "zoom-in",
		  N_("_Zoom In"),
		  "<Control>plus",
		  N_("Increase the text size"),
		  action_mail_zoom_in_cb, NULL, NULL, NULL },

		{ "mail-zoom-out",
		  "zoom-out",
		  N_("Zoom _Out"),
		  "<Control>minus",
		  N_("Decrease the text size"),
		  action_mail_zoom_out_cb, NULL, NULL, NULL },

		{ "mail-caret-mode",
		  NULL,
		  N_("_Caret Mode"),
		  "F7",
		  N_("Show a blinking cursor in the body of displayed messages"),
		  NULL, NULL, "false", NULL },

		{ "mail-show-all-headers",
		  NULL,
		  N_("All Message _Headers"),
		  NULL,
		  N_("Show messages with all email headers"),
		  NULL, NULL, "false", NULL },

		/*** Menus ***/

		{ "mail-create-menu", NULL, N_("Cre_ate"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "mail-forward-as-menu", NULL, N_("F_orward As"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "mail-label-menu", NULL, N_("_Label"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "mail-reply-group-menu", NULL, N_("_Group Reply"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "mail-goto-menu", NULL, N_("_Go To"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "mail-mark-as-menu", NULL, N_("Mar_k As"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "mail-message-menu", NULL, N_("_Message"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "mail-zoom-menu", NULL, N_("_Zoom"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EMailReader::charset-menu", NULL, N_("Ch_aracter Encoding"), NULL, NULL, NULL, "s", "''", charset_menu_change_state_cb },
		{ "EMailReader::mail-reply-group", NULL, N_("_Group Reply"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EMailReader::mail-forward-as-group", NULL, N_("_Forward"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EMailReader::mail-label-actions", NULL, N_("List of Labels"), NULL, NULL, NULL, NULL, NULL, NULL }
	};

	static const EUIActionEntry search_folder_entries[] = {

		{ "mail-search-folder-from-mailing-list",
		  NULL,
		  N_("Create a Search Folder from Mailing _List…"),
		  NULL,
		  N_("Create a search folder for this mailing list"),
		  action_mail_search_folder_from_mailing_list_cb, NULL, NULL, NULL },

		{ "mail-search-folder-from-recipients",
		  NULL,
		  N_("Create a Search Folder from Recipien_ts…"),
		  NULL,
		  N_("Create a search folder for these recipients"),
		  action_mail_search_folder_from_recipients_cb, NULL, NULL, NULL },

		{ "mail-search-folder-from-sender",
		  NULL,
		  N_("Create a Search Folder from Sen_der…"),
		  NULL,
		  N_("Create a search folder for this sender"),
		  action_mail_search_folder_from_sender_cb, NULL, NULL, NULL },

		{ "mail-search-folder-from-subject",
		  NULL,
		  N_("Create a Search Folder from S_ubject…"),
		  NULL,
		  N_("Create a search folder for this subject"),
		  action_mail_search_folder_from_subject_cb, NULL, NULL, NULL },
	};

	const struct _name_pair {
		const gchar *src_name;
		const gchar *dst_name;
	} name_pairs[] = {
		{ "mail-flag-for-followup", "mail-flag-for-followup-full" },
		{ "mail-forward-attached", "mail-forward-attached-full" },
		{ "mail-forward-inline", "mail-forward-inline-full" },
		{ "mail-forward-quoted", "mail-forward-quoted-full" },
		{ "mail-mark-important", "mail-mark-important-full" },
		{ "mail-mark-junk", "mail-mark-junk-full" },
		{ "mail-mark-notjunk", "mail-mark-notjunk-full" },
		{ "mail-mark-unimportant", "mail-mark-unimportant-full" },
		{ "mail-mark-unread", "mail-mark-unread-full" }
	};
	EUIManager *ui_manager;
	EMailReaderPrivate *priv;
	EMailDisplay *display;
	EUIAction *action;
	GSettings *settings;
	gint ii;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_MAIL_READER (self));

	ui_manager = e_mail_reader_get_ui_manager (self);
	if (!ui_manager)
		return;

	display = e_mail_reader_get_mail_display (self);

	g_signal_connect_object (ui_manager, "create-item",
		G_CALLBACK (e_mail_reader_ui_manager_create_item_cb), self, 0);

	e_ui_manager_add_actions (ui_manager, "mail", NULL,
		mail_entries, G_N_ELEMENTS (mail_entries), self);
	e_ui_manager_add_actions (ui_manager, "search-folders", NULL,
		search_folder_entries, G_N_ELEMENTS (search_folder_entries), self);

	e_ui_manager_set_actions_usable_for_kinds (ui_manager, E_UI_ELEMENT_KIND_MENU,
		"mail-create-menu",
		"mail-forward-as-menu",
		"mail-label-menu",
		"mail-reply-group-menu",
		"mail-goto-menu",
		"mail-mark-as-menu",
		"mail-message-menu",
		"mail-zoom-menu",
		"EMailReader::charset-menu",
		"EMailReader::mail-label-actions",
		NULL);

	for (ii = 0; ii < G_N_ELEMENTS (name_pairs); ii++) {
		e_binding_bind_property (
			e_ui_manager_get_action (ui_manager, name_pairs[ii].src_name), "sensitive",
			e_ui_manager_get_action (ui_manager, name_pairs[ii].dst_name), "sensitive",
			G_BINDING_SYNC_CREATE);
	}

	if (!e_ui_parser_merge_file (e_ui_manager_get_parser (ui_manager), "evolution-mail-reader.eui", &local_error))
		g_warning ("%s: Failed to read %s file: %s", G_STRFUNC, "evolution-mail-reader.eui", local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);

	priv = E_MAIL_READER_GET_PRIVATE (self);
	priv->reply_group_menu = G_MENU_MODEL (e_ui_manager_create_item (ui_manager, "mail-reply-group-menu"));
	priv->forward_as_menu = G_MENU_MODEL (e_ui_manager_create_item (ui_manager, "mail-forward-as-menu"));

	action = e_mail_reader_get_action (self, "mail-delete");
	e_ui_action_add_secondary_accel (action, "Delete");
	e_ui_action_add_secondary_accel (action, "KP_Delete");

	action = e_mail_reader_get_action (self, "mail-message-open");
	e_ui_action_add_secondary_accel (action, "Return");
	e_ui_action_add_secondary_accel (action, "KP_Enter");
	e_ui_action_add_secondary_accel (action, "ISO_Enter");

	action = e_mail_reader_get_action (self, "mail-next-unread");
	e_ui_action_add_secondary_accel (action, "period");
	e_ui_action_add_secondary_accel (action, "bracketright");

	action = e_mail_reader_get_action (self, "mail-previous-unread");
	e_ui_action_add_secondary_accel (action, "comma");
	e_ui_action_add_secondary_accel (action, "bracketleft");

	action = e_mail_reader_get_action (self, "mail-reply-all");
	e_ui_action_add_secondary_accel (action, "Reply");

	action = e_mail_reader_get_action (self, "mail-forward");
	e_ui_action_add_secondary_accel (action, "MailForward");

	action = e_mail_reader_get_action (self, "mail-toggle-important");
	e_ui_action_add_secondary_accel (action, "exclam");

	action = e_mail_reader_get_action (self, "mail-zoom-in");
	e_ui_action_add_secondary_accel (action, "ZoomIn");

	action = e_mail_reader_get_action (self, "mail-zoom-out");
	e_ui_action_add_secondary_accel (action, "ZoomOut");

	action = e_mail_reader_get_action (self, "mail-next-unread");
	e_ui_action_add_secondary_accel (action, "<Primary>period");

	action = e_mail_reader_get_action (self, "mail-previous-unread");
	e_ui_action_add_secondary_accel (action, "<Primary>comma");

	action = e_mail_reader_get_action (self, "mail-zoom-in");
	e_ui_action_add_secondary_accel (action, "<Primary>equal");
	e_ui_action_add_secondary_accel (action, "<Primary>KP_Add");

	action = e_mail_reader_get_action (self, "mail-zoom-out");
	e_ui_action_add_secondary_accel (action, "<Primary>KP_Subtract");

	/* Bind GObject properties to GSettings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	action = e_mail_reader_get_action (self, "mail-caret-mode");
	g_settings_bind (
		settings, "caret-mode",
		action, "active", G_SETTINGS_BIND_DEFAULT);

	action = e_mail_reader_get_action (self, "mail-show-all-headers");
	g_settings_bind (
		settings, "show-all-headers",
		action, "active", G_SETTINGS_BIND_DEFAULT);

	/* Mode change when viewing message source is ignored. */
	if (e_mail_display_get_mode (display) == E_MAIL_FORMATTER_MODE_SOURCE ||
	    e_mail_display_get_mode (display) == E_MAIL_FORMATTER_MODE_RAW) {
		e_ui_action_set_sensitive (action, FALSE);
		e_ui_action_set_visible (action, FALSE);
	} else {
		e_mail_reader_update_display_mode (self);
	}

	g_signal_connect_object (action, "notify::active",
		G_CALLBACK (e_mail_reader_update_display_mode), self, G_CONNECT_SWAPPED);

	g_object_unref (settings);

#ifndef G_OS_WIN32
	/* Lockdown integration. */

	settings = e_util_ref_settings ("org.gnome.desktop.lockdown");

	action = e_mail_reader_get_action (self, "mail-print");
	g_settings_bind (
		settings, "disable-printing",
		action, "visible",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_NO_SENSITIVITY |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	action = e_mail_reader_get_action (self, "mail-print-preview");
	g_settings_bind (
		settings, "disable-printing",
		action, "visible",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_NO_SENSITIVITY |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	action = e_mail_reader_get_action (self, "mail-save-as");
	g_settings_bind (
		settings, "disable-save-to-disk",
		action, "visible",
		G_SETTINGS_BIND_GET |
		G_SETTINGS_BIND_NO_SENSITIVITY |
		G_SETTINGS_BIND_INVERT_BOOLEAN);

	g_object_unref (settings);
#endif

	/* Bind properties. */

	action = e_mail_reader_get_action (self, "mail-caret-mode");

	e_binding_bind_property (
		action, "active",
		display, "caret-mode",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}

static void
mail_reader_double_click_cb (EMailReader *reader,
                             gint row,
                             ETreePath path,
                             gint col,
                             GdkEvent *event)
{
	EUIAction *action;

	/* Ignore double clicks on columns that handle their own state. */
	if (MESSAGE_LIST_COLUMN_IS_ACTIVE (col))
		return;

	action = e_mail_reader_get_action (reader, "mail-message-open");
	g_action_activate (G_ACTION (action), NULL);
}

static gboolean
mail_reader_message_seen_cb (gpointer user_data)
{
	EMailReaderClosure *closure = user_data;
	EMailReader *reader;
	GtkWidget *message_list;
	EMailPartList *parts;
	EMailDisplay *display;
	CamelMimeMessage *message;
	const gchar *current_uid;
	const gchar *message_uid;
	gboolean uid_is_current = TRUE;

	reader = closure->reader;
	message_uid = closure->message_uid;

	display = e_mail_reader_get_mail_display (reader);
	parts = e_mail_display_get_part_list (display);
	message_list = e_mail_reader_get_message_list (reader);

	g_return_val_if_fail (IS_MESSAGE_LIST (message_list), FALSE);

	/* zero the timeout id now, if it was not rescheduled */
	if (g_source_get_id (g_main_current_source ()) == MESSAGE_LIST (message_list)->seen_id)
		MESSAGE_LIST (message_list)->seen_id = 0;

	if (e_tree_is_dragging (E_TREE (message_list)))
		return FALSE;

	current_uid = MESSAGE_LIST (message_list)->cursor_uid;
	uid_is_current &= (g_strcmp0 (current_uid, message_uid) == 0);

	if (parts != NULL)
		message = e_mail_part_list_get_message (parts);
	else
		message = NULL;

	if (uid_is_current && message != NULL)
		g_signal_emit (
			reader, signals[MESSAGE_SEEN], 0,
			message_uid, message);

	return FALSE;
}

static void
schedule_timeout_mark_seen (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	MessageList *message_list;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));
	g_return_if_fail (message_list != NULL);

	if (message_list->cursor_uid) {
		EMailReaderClosure *timeout_closure;

		if (message_list->seen_id > 0) {
			g_source_remove (message_list->seen_id);
			message_list->seen_id = 0;
		}

		timeout_closure = g_slice_new0 (EMailReaderClosure);
		timeout_closure->reader = g_object_ref (reader);
		timeout_closure->message_uid = g_strdup (message_list->cursor_uid);

		MESSAGE_LIST (message_list)->seen_id =
			e_named_timeout_add_full (
				G_PRIORITY_DEFAULT, priv->schedule_mark_seen_interval,
				mail_reader_message_seen_cb,
				timeout_closure, (GDestroyNotify)
				mail_reader_closure_free);
	}
}

static void
mail_reader_load_changed_cb (EMailReader *reader,
                             WebKitLoadEvent event,
                             EMailDisplay *display)
{
	EMailReaderPrivate *priv;

	if (event != WEBKIT_LOAD_FINISHED)
		return;

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	if (priv->schedule_mark_seen &&
	    E_IS_MAIL_VIEW (reader) &&
	    e_mail_display_get_part_list (display) &&
	    e_mail_view_get_preview_visible (E_MAIL_VIEW (reader))) {
		if (priv->folder_was_just_selected)
			priv->folder_was_just_selected = FALSE;
		else
			schedule_timeout_mark_seen (reader);
	}
}

static void
mail_reader_remote_content_clicked_cb (EMailReader *reader,
				       const GdkRectangle *position,
				       gpointer user_data)
{
	GtkWidget *mail_display = user_data;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (E_IS_MAIL_DISPLAY (mail_display));

	e_mail_remote_content_popover_run (reader, mail_display, position);
}

static void
mail_reader_autocrypt_import_clicked_cb (EMailReader *reader,
					 const GdkRectangle *position,
					 gpointer user_data)
{
	EMailDisplay *mail_display = user_data;
	EMailPartList *part_list;
	GPtrArray *autocrypt_keys;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (E_IS_MAIL_DISPLAY (mail_display));

	part_list = e_mail_display_get_part_list (mail_display);
	autocrypt_keys = e_mail_part_list_get_autocrypt_keys (part_list);

	if (autocrypt_keys) {
		GtkWindow *parent;
		guint ii;

		parent = e_mail_reader_get_window (reader);

		for (ii = 0; ii < autocrypt_keys->len; ii++) {
			EMailAutocryptKey *key = g_ptr_array_index (autocrypt_keys, ii);
			GError *error = NULL;

			if (key && !em_utils_import_pgp_key (parent, NULL, key->keydata, key->keydata_size, &error) &&
			    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
				EAlertSink *alert_sink;

				alert_sink = e_mail_reader_get_alert_sink (reader);
				e_alert_submit (alert_sink, "mail:error-import-pgp-key", error ? error->message : _("Unknown error"), NULL);

				g_clear_error (&error);
				break;
			}

			g_clear_error (&error);
		}
	}
}

static void
maybe_schedule_timeout_mark_seen (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	MessageList *message_list;
	gboolean schedule_timeout;
	gint timeout_interval = -1;
	const gchar *message_uid;

	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));

	message_uid = message_list->cursor_uid;
	if (message_uid == NULL ||
	    e_tree_is_dragging (E_TREE (message_list)))
		return;

	schedule_timeout =
		(message_uid != NULL) &&
		e_mail_reader_utils_get_mark_seen_setting (reader, &timeout_interval);

	if (message_list->seen_id > 0) {
		g_source_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	priv->schedule_mark_seen = schedule_timeout;
	priv->schedule_mark_seen_interval = timeout_interval;
}

static gboolean
discard_timeout_mark_seen_cb (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	MessageList *message_list;

	g_return_val_if_fail (reader != NULL, FALSE);

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	priv->schedule_mark_seen = FALSE;

	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));
	g_return_val_if_fail (message_list != NULL, FALSE);

	if (message_list->seen_id > 0) {
		g_source_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}

	return FALSE;
}


static void
mail_reader_preview_pane_visible_changed_cb (EMailReader *reader,
					     GParamSpec *param,
					     GtkWidget *widget)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (!gtk_widget_is_visible (widget))
		discard_timeout_mark_seen_cb (reader);
}

static void
mail_reader_remove_followup_alert (EMailReader *reader)
{
	EMailReaderPrivate *priv;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	if (!priv)
		return;

	if (priv->followup_alert)
		e_alert_response (priv->followup_alert, GTK_RESPONSE_OK);
}

static void
mail_reader_manage_followup_flag (EMailReader *reader,
				  CamelFolder *folder,
				  const gchar *message_uid)
{
	EMailReaderPrivate *priv;
	CamelMessageInfo *info;
	const gchar *followup, *completed_on, *due_by;
	time_t date;
	gchar *date_str = NULL;
	gboolean alert_added = FALSE;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uid != NULL);

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	if (!priv)
		return;

	info = camel_folder_get_message_info (folder, message_uid);
	if (!info)
		return;

	followup = camel_message_info_get_user_tag (info, "follow-up");
	if (followup && *followup) {
		EPreviewPane *preview_pane;
		const gchar *alert_tag;
		EAlert *alert;

		completed_on = camel_message_info_get_user_tag (info, "completed-on");
		due_by = camel_message_info_get_user_tag (info, "due-by");

		if (completed_on && *completed_on) {
			alert_tag = "mail:follow-up-completed-info";
			date = camel_header_decode_date (completed_on, NULL);
			date_str = e_datetime_format_format ("mail", "header", DTFormatKindDateTime, date);
		} else if (due_by && *due_by) {
			time_t now;

			alert_tag = "mail:follow-up-dueby-info";
			date = camel_header_decode_date (due_by, NULL);
			date_str = e_datetime_format_format ("mail", "header", DTFormatKindDateTime, date);

			now = time (NULL);
			if (now > date)
				alert_tag = "mail:follow-up-overdue-error";
		} else {
			alert_tag = "mail:follow-up-flag-info";
		}

		alert = e_alert_new (alert_tag, followup, date_str ? date_str : "???", NULL);

		g_free (date_str);

		preview_pane = e_mail_reader_get_preview_pane (reader);
		e_alert_sink_submit_alert (E_ALERT_SINK (preview_pane), alert);

		alert_added = TRUE;

		mail_reader_remove_followup_alert (reader);
		priv->followup_alert = alert;
		g_object_add_weak_pointer (G_OBJECT (priv->followup_alert), &priv->followup_alert);

		g_object_unref (alert);
	}

	g_clear_object (&info);

	if (!alert_added)
		mail_reader_remove_followup_alert (reader);
}

static void
mail_reader_reload (EMailReader *reader)
{
	CamelFolder *folder;
	GPtrArray *uids;
	EMailDisplay *mail_display;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);

	if (uids && uids->len == 1)
		mail_reader_manage_followup_flag (reader, folder, uids->pdata[0]);

	g_clear_object (&folder);
	if (uids)
		g_ptr_array_unref (uids);

	mail_display = e_mail_reader_get_mail_display (reader);
	e_mail_display_reload (mail_display);
}

static void
mail_reader_message_loaded_cb (CamelFolder *folder,
                               GAsyncResult *result,
                               EMailReaderClosure *closure)
{
	EMailReader *reader;
	EMailReaderPrivate *priv;
	CamelMimeMessage *message = NULL;
	GtkWidget *message_list;
	const gchar *message_uid;
	GError *error = NULL;

	reader = closure->reader;
	message_uid = closure->message_uid;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	/* If the private struct is NULL, the EMailReader was destroyed
	 * while we were loading the message and we're likely holding the
	 * last reference.  Nothing to do but drop the reference.
	 * FIXME Use a GWeakRef instead of this hack. */
	if (priv == NULL) {
		mail_reader_closure_free (closure);
		return;
	}

	message = camel_folder_get_message_finish (folder, result, &error);

	/* If the user picked a different message in the time it took
	 * to fetch this message, then don't bother rendering it. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
		goto exit;
	}

	message_list = e_mail_reader_get_message_list (reader);

	if (message_list == NULL) {
		/* For cases where message fetching took so long that
		 * user closed the message window before this was called. */
		goto exit;
	}

	if (message != NULL) {
		CamelMessageInfo *mi;

		mail_reader_manage_followup_flag (reader, folder, message_uid);

		mi = camel_folder_get_message_info (folder, message_uid);
		if (mi) {
			if (camel_util_fill_message_info_user_headers (mi, camel_medium_get_headers (CAMEL_MEDIUM (message))))
				gtk_widget_queue_draw (message_list);

			g_object_unref (mi);
		}

		g_signal_emit (
			reader, signals[MESSAGE_LOADED], 0,
			message_uid, message);
	}

exit:
	if (error != NULL) {
		EPreviewPane *preview_pane;
		EWebView *web_view;

		preview_pane = e_mail_reader_get_preview_pane (reader);
		web_view = e_preview_pane_get_web_view (preview_pane);

		if (g_error_matches (error, CAMEL_SERVICE_ERROR, CAMEL_SERVICE_ERROR_UNAVAILABLE) &&
		    CAMEL_IS_OFFLINE_FOLDER (folder) &&
		    camel_service_get_connection_status (CAMEL_SERVICE (camel_folder_get_parent_store (folder))) != CAMEL_SERVICE_CONNECTED)
			e_alert_submit (
				E_ALERT_SINK (web_view),
				"mail:no-retrieve-message-offline",
				NULL);
		else
			e_alert_submit (
				E_ALERT_SINK (web_view),
				"mail:no-retrieve-message",
				error->message, NULL);
	}

	g_clear_error (&error);

	mail_reader_closure_free (closure);

	g_clear_object (&message);
}

static gboolean
mail_reader_message_selected_timeout_cb (gpointer user_data)
{
	EMailReader *reader;
	EMailReaderPrivate *priv;
	EMailDisplay *display;
	GtkWidget *message_list;
	const gchar *cursor_uid;
	const gchar *format_uid;
	EMailPartList *parts;

	reader = E_MAIL_READER (user_data);
	priv = E_MAIL_READER_GET_PRIVATE (reader);

	message_list = e_mail_reader_get_message_list (reader);
	display = e_mail_reader_get_mail_display (reader);
	parts = e_mail_display_get_part_list (display);

	cursor_uid = MESSAGE_LIST (message_list)->cursor_uid;
	if (parts != NULL)
		format_uid = e_mail_part_list_get_message_uid (parts);
	else
		format_uid = NULL;

	if (MESSAGE_LIST (message_list)->last_sel_single) {
		GtkWidget *widget;
		gboolean display_visible;
		gboolean selected_uid_changed;

		/* Decide whether to download the full message now. */
		widget = GTK_WIDGET (display);
		display_visible = gtk_widget_get_mapped (widget);

		selected_uid_changed = (g_strcmp0 (cursor_uid, format_uid) != 0);

		if (display_visible && selected_uid_changed) {
			EMailReaderClosure *closure;
			GCancellable *cancellable;
			CamelFolder *folder;
			EActivity *activity;
			gchar *string;

			string = g_strdup_printf (
				_("Retrieving message “%s”"), cursor_uid);
			e_mail_display_set_part_list (display, NULL);
			e_mail_display_set_status (display, string);
			g_free (string);

			activity = e_mail_reader_new_activity (reader);
			e_activity_set_text (activity, _("Retrieving message"));
			cancellable = e_activity_get_cancellable (activity);

			closure = g_slice_new0 (EMailReaderClosure);
			closure->activity = activity;
			closure->reader = g_object_ref (reader);
			closure->message_uid = g_strdup (cursor_uid);

			folder = e_mail_reader_ref_folder (reader);

			camel_folder_get_message (
				folder, cursor_uid, G_PRIORITY_DEFAULT,
				cancellable, (GAsyncReadyCallback)
				mail_reader_message_loaded_cb, closure);

			g_clear_object (&folder);

			if (priv->retrieving_message != NULL)
				g_object_unref (priv->retrieving_message);
			priv->retrieving_message = g_object_ref (cancellable);
		}
	} else {
		e_mail_display_set_part_list (display, NULL);
	}

	priv->message_selected_timeout_id = 0;

	return FALSE;
}

static void
mail_reader_message_selected_cb (EMailReader *reader,
                                 const gchar *message_uid)
{
	EMailReaderPrivate *priv;
	MessageList *message_list;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	/* Cancel the previous message retrieval activity. */
	g_cancellable_cancel (priv->retrieving_message);

	/* Cancel the message selected timer. */
	if (priv->message_selected_timeout_id > 0) {
		g_source_remove (priv->message_selected_timeout_id);
		priv->message_selected_timeout_id = 0;
	}

	if (priv->folder_was_just_selected && message_uid) {
		if (priv->did_try_to_open_message)
			priv->folder_was_just_selected = FALSE;
		else
			priv->did_try_to_open_message = TRUE;
	}

	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));
	if (message_list) {
		EMailPartList *parts;
		const gchar *cursor_uid, *format_uid;

		parts = e_mail_display_get_part_list (e_mail_reader_get_mail_display (reader));

		cursor_uid = MESSAGE_LIST (message_list)->cursor_uid;
		if (parts != NULL)
			format_uid = e_mail_part_list_get_message_uid (parts);
		else
			format_uid = NULL;

		/* It can happen when the message was loaded that quickly that
		   it was delivered before this callback. */
		if (g_strcmp0 (cursor_uid, format_uid) == 0) {
			e_mail_reader_changed (reader);
			return;
		}
	}

	/* Cancel the seen timer. */
	if (message_list != NULL && message_list->seen_id) {
		g_source_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}

	if (message_list_selected_count (message_list) != 1) {
		EMailDisplay *display;

		display = e_mail_reader_get_mail_display (reader);
		e_mail_display_set_part_list (display, NULL);
		e_web_view_clear (E_WEB_VIEW (display));

	} else if (priv->folder_was_just_selected) {
		/* Skip the timeout if we're restoring the previous message
		 * selection.  The timeout is there for when we're scrolling
		 * rapidly through the message list. */
		mail_reader_message_selected_timeout_cb (reader);

	} else {
		priv->message_selected_timeout_id = e_named_timeout_add (
			100, mail_reader_message_selected_timeout_cb, reader);
	}

	e_mail_reader_changed (reader);
}

static void
mail_reader_message_cursor_change_cb (EMailReader *reader)
{
	MessageList *message_list;
	EMailReaderPrivate *priv;

	g_return_if_fail (reader != NULL);

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	g_return_if_fail (priv != NULL);

	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));
	g_return_if_fail (message_list != NULL);

	if (message_list->seen_id == 0 &&
	    E_IS_MAIL_VIEW (reader) &&
	    e_mail_view_get_preview_visible (E_MAIL_VIEW (reader)) &&
	    !priv->avoid_next_mark_as_seen)
		maybe_schedule_timeout_mark_seen (reader);
}

static void
mail_reader_emit_folder_loaded (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	MessageList *message_list;

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));

	if (priv && (message_list_count (message_list) <= 0 ||
	    message_list_selected_count (message_list) <= 0))
		priv->avoid_next_mark_as_seen = FALSE;

	g_signal_emit (reader, signals[FOLDER_LOADED], 0);
}

static void
mail_reader_message_list_built_cb (MessageList *message_list,
				   EMailReader *reader)
{
	EMailReaderPrivate *priv;

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	mail_reader_emit_folder_loaded (reader);

	/* No cursor_uid means that there will not be emitted any
	   "cursor-changed" and "message-selected" signal, thus
	   unset the "just selected folder" flag */
	if (!message_list->cursor_uid)
		priv->folder_was_just_selected = FALSE;
}

static EAlertSink *
mail_reader_get_alert_sink (EMailReader *reader)
{
	EPreviewPane *preview_pane;

	preview_pane = e_mail_reader_get_preview_pane (reader);

	if (!gtk_widget_is_visible (GTK_WIDGET (preview_pane))) {
		GtkWindow *window;

		window = e_mail_reader_get_window (reader);

		if (E_IS_SHELL_WINDOW (window))
			return E_ALERT_SINK (window);
	}

	return E_ALERT_SINK (preview_pane);
}

static GPtrArray *
mail_reader_get_selected_uids (EMailReader *reader)
{
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	return message_list_get_selected (MESSAGE_LIST (message_list));
}

static GPtrArray *
mail_reader_get_selected_uids_with_collapsed_threads (EMailReader *reader)
{
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	return message_list_get_selected_with_collapsed_threads (MESSAGE_LIST (message_list));
}

static CamelFolder *
mail_reader_ref_folder (EMailReader *reader)
{
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	return message_list_ref_folder (MESSAGE_LIST (message_list));
}

static void
mail_reader_set_folder (EMailReader *reader,
                        CamelFolder *folder)
{
	EMailReaderPrivate *priv;
	EMailDisplay *display;
	CamelFolder *previous_folder;
	GtkWidget *message_list;
	EMailBackend *backend;
	EShell *shell;
	gboolean sync_folder;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	display = e_mail_reader_get_mail_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	previous_folder = e_mail_reader_ref_folder (reader);

	backend = e_mail_reader_get_backend (reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	/* Only synchronize the real folder if we're online. */
	sync_folder =
		(previous_folder != NULL) &&
		(CAMEL_IS_VEE_FOLDER (previous_folder) ||
		e_shell_get_online (shell));
	if (sync_folder)
		mail_sync_folder (previous_folder, TRUE, NULL, NULL);

	/* Skip the rest if we're already viewing the folder. */
	if (folder != previous_folder) {
		e_web_view_clear (E_WEB_VIEW (display));

		priv->folder_was_just_selected = (folder != NULL) && !priv->mark_seen_always;
		priv->did_try_to_open_message = FALSE;

		/* This is to make sure any post-poned changes in Search
		 * Folders will be propagated on folder selection. */
		if (CAMEL_IS_VEE_FOLDER (folder))
			mail_sync_folder (folder, FALSE, NULL, NULL);

		message_list_set_folder (MESSAGE_LIST (message_list), folder);

		mail_reader_emit_folder_loaded (reader);
	}

	g_clear_object (&previous_folder);
}

static void
mail_reader_set_message (EMailReader *reader,
                         const gchar *message_uid)
{
	GtkWidget *message_list;
	EMailReaderPrivate *priv;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	/* For a case when the preview panel had been disabled */
	priv->folder_was_just_selected = FALSE;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_uid (
		MESSAGE_LIST (message_list), message_uid, FALSE);
}

static void
mail_reader_folder_loaded (EMailReader *reader)
{
	guint32 state;

	state = e_mail_reader_check_state (reader);
	e_mail_reader_update_actions (reader, state);
}

static void
mail_reader_message_list_suggest_update_actions_cb (EMailReader *reader)
{
	guint32 state;

	state = e_mail_reader_check_state (reader);
	e_mail_reader_update_actions (reader, state);
}

static void
set_mail_display_part_list (GObject *object,
                            GAsyncResult *result,
                            gpointer user_data)
{
	EMailPartList *part_list;
	EMailReader *reader;
	EMailDisplay *display;
	GError *local_error = NULL;

	reader = E_MAIL_READER (object);

	part_list = e_mail_reader_parse_message_finish (reader, result, &local_error);

	if (local_error) {
		g_warn_if_fail (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED));

		g_clear_error (&local_error);
		return;
	}

	display = e_mail_reader_get_mail_display (reader);

	e_mail_display_set_part_list (display, part_list);
	e_mail_display_load (display, NULL);

	/* Remove the reference added when parts list was
	 * created, so that only owners are EMailDisplays. */
	g_object_unref (part_list);
}

static void
mail_reader_set_display_formatter_for_message (EMailReader *reader,
                                               EMailDisplay *display,
                                               const gchar *message_uid,
                                               CamelMimeMessage *message,
                                               CamelFolder *folder)
{
	CamelObjectBag *registry;
	EMailPartList *parts;
	EMailReaderPrivate *priv;
	gchar *mail_uri;

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	mail_uri = e_mail_part_build_uri (folder, message_uid, NULL, NULL);
	registry = e_mail_part_list_get_registry ();
	parts = camel_object_bag_peek (registry, mail_uri);
	g_free (mail_uri);

	if (parts == NULL) {
		if (!priv->retrieving_message)
			priv->retrieving_message = camel_operation_new ();

		e_mail_reader_parse_message (
			reader, folder, message_uid, message,
			priv->retrieving_message,
			set_mail_display_part_list, NULL);
	} else {
		e_mail_display_set_part_list (display, parts);
		e_mail_display_load (display, NULL);
		g_object_unref (parts);
	}
}

static void
mail_reader_message_loaded (EMailReader *reader,
                            const gchar *message_uid,
                            CamelMimeMessage *message)
{
	EMailReaderPrivate *priv;
	GtkWidget *message_list;
	EMailBackend *backend;
	CamelFolder *folder;
	EMailDisplay *display;
	EShellBackend *shell_backend;
	EShell *shell;
	EMEvent *event;
	EMEventTargetMessage *target;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	folder = e_mail_reader_ref_folder (reader);
	backend = e_mail_reader_get_backend (reader);
	display = e_mail_reader_get_mail_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	/** @Event: message.reading
	 * @Title: Viewing a message
	 * @Target: EMEventTargetMessage
	 *
	 * message.reading is emitted whenever a user views a message.
	 */
	event = em_event_peek ();
	target = em_event_target_new_message (
		event, folder, message, message_uid, 0, NULL);
	e_event_emit (
		(EEvent *) event, "message.reading",
		(EEventTarget *) target);

	mail_reader_set_display_formatter_for_message (
		reader, display, message_uid, message, folder);

	/* Reset the shell view icon. */
	e_shell_event (shell, "mail-icon", (gpointer) "evolution-mail");

	if (MESSAGE_LIST (message_list)->seen_id > 0) {
		g_source_remove (MESSAGE_LIST (message_list)->seen_id);
		MESSAGE_LIST (message_list)->seen_id = 0;
	}

	/* Determine whether to mark the message as read. */
	if (message != NULL &&
	    !priv->avoid_next_mark_as_seen)
		maybe_schedule_timeout_mark_seen (reader);

	priv->avoid_next_mark_as_seen = FALSE;

	g_clear_object (&folder);
}

static void
mail_reader_message_seen (EMailReader *reader,
                          const gchar *message_uid,
                          CamelMimeMessage *message)
{
	CamelFolder *folder;
	guint32 mask, set;

	mask = CAMEL_MESSAGE_SEEN;
	set = CAMEL_MESSAGE_SEEN;

	folder = e_mail_reader_ref_folder (reader);
	camel_folder_set_message_flags (folder, message_uid, mask, set);
	g_clear_object (&folder);
}

static void
mail_reader_show_search_bar (EMailReader *reader)
{
	EPreviewPane *preview_pane;

	preview_pane = e_mail_reader_get_preview_pane (reader);
	e_preview_pane_show_search_bar (preview_pane);
}

static void
action_mail_label_cb (EUIAction *action,
		      GParamSpec *param,
		      gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *tag;
	gint ii;

	tag = g_object_get_data (G_OBJECT (action), "tag");
	g_return_if_fail (tag != NULL);

	uids = e_mail_reader_get_selected_uids (reader);
	if (!uids)
		return;

	folder = e_mail_reader_ref_folder (reader);

	camel_folder_freeze (folder);
	for (ii = 0; ii < uids->len; ii++) {
		CamelMessageInfo *nfo;

		nfo = camel_folder_get_message_info (folder, uids->pdata[ii]);
		if (!nfo)
			continue;

		if (e_ui_action_get_active (action)) {
			camel_message_info_set_user_flag (nfo, tag, TRUE);
		} else {
			camel_message_info_set_user_flag (nfo, tag, FALSE);
			camel_message_info_set_user_tag (nfo, "label", NULL);
		}

		g_clear_object (&nfo);
	}
	camel_folder_thaw (folder);

	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

#define LABEL_UNKNOWN	0
#define LABEL_EXISTS	(1 << 0)
#define LABEL_NOTEXIST	(1 << 1)

static GHashTable *
mail_reader_gather_labels_info (EMailReader *reader,
				EMailLabelListStore *label_store,
				GPtrArray *uids)
{
	GHashTable *labels_info;
	CamelFolder *folder;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean valid;
	guint ii;

	labels_info = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	model = GTK_TREE_MODEL (label_store);
	folder = e_mail_reader_ref_folder (reader);

	if (!folder)
		return labels_info;

	for (ii = 0; ii < uids->len; ii++) {
		CamelMessageInfo *info;

		info = camel_folder_get_message_info (folder, uids->pdata[ii]);
		if (!info)
			continue;

		for (valid = gtk_tree_model_get_iter_first (model, &iter);
		     valid;
		     valid = gtk_tree_model_iter_next (model, &iter)) {
			gchar *tag;
			guint value;

			tag = e_mail_label_list_store_get_tag (label_store, &iter);
			value = GPOINTER_TO_UINT (g_hash_table_lookup (labels_info, tag));
			if ((!(value & LABEL_EXISTS)) || (!(value & LABEL_NOTEXIST))) {
				gboolean exists = FALSE, notexist = FALSE;

				/* Check for new-style labels. */
				if (camel_message_info_get_user_flag (info, tag)) {
					exists = TRUE;
				} else {
					/* Check for old-style labels. */
					const gchar *old_label = camel_message_info_get_user_tag (info, "label");
					if (old_label) {
						gchar *new_label;

						/* Convert old-style labels ("<name>") to "$Label<name>". */
						new_label = g_alloca (strlen (old_label) + 10);
						g_stpcpy (g_stpcpy (new_label, "$Label"), old_label);

						if (g_strcmp0 (new_label, tag) == 0)
							exists = TRUE;
						else
							notexist = TRUE;
					} else {
						notexist = TRUE;
					}
				}

				value = value |
					(exists ? LABEL_EXISTS : LABEL_UNKNOWN) |
					(notexist ? LABEL_NOTEXIST : LABEL_UNKNOWN);

				g_hash_table_insert (labels_info, tag, GUINT_TO_POINTER (value));

				/* the hash table took the 'tag' */
				tag = NULL;
			}

			g_free (tag);
		}

		g_clear_object (&info);
	}

	g_clear_object (&folder);

	return labels_info;
}

static void
mail_reader_update_label_action (EUIAction *action,
				 GHashTable *labels_info, /* gchar * ~> guint */
				 const gchar *label_tag)
{
	gboolean exists = FALSE;
	gboolean not_exists = FALSE;
	gboolean sensitive;
	guint value;

	/* Figure out the proper label action state for the selected
	 * messages.  If all the selected messages have the given label,
	 * make the toggle action active.  If all the selected message
	 * DO NOT have the given label, make the toggle action inactive.
	 * If some do and some don't, make the action insensitive. */

	value = GPOINTER_TO_UINT (g_hash_table_lookup (labels_info, label_tag));
	exists = (value & LABEL_EXISTS) != 0;
	not_exists = (value & LABEL_NOTEXIST) != 0;

	sensitive = !(exists && not_exists);
	e_ui_action_set_active (action, exists);
	e_ui_action_set_sensitive (action, sensitive);
}

static void
mail_reader_update_labels_menu (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	EMailLabelListStore *label_store;
	EMailBackend *backend;
	EMailSession *session;
	EUIManager *ui_manager = NULL;
	EUIActionGroup *action_group;
	GtkTreeIter iter;
	GHashTable *labels_info; /* gchar * ~> guint { LABEL_EXISTS | LABEL_NOTEXIST | LABEL_UNKNOWN } */
	GPtrArray *uids;
	gboolean valid;
	gint ii = 0;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	if (!priv->labels_menu)
		return;

	ui_manager = e_mail_reader_get_ui_manager (reader);
	if (!ui_manager)
		return;

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (E_MAIL_UI_SESSION (session));

	action_group = e_ui_manager_get_action_group (ui_manager, "mail-labels");

	e_ui_manager_freeze (ui_manager);

	g_menu_remove_all (priv->labels_menu);
	e_ui_action_group_remove_all (action_group);

	uids = e_mail_reader_get_selected_uids (reader);
	labels_info = mail_reader_gather_labels_info (reader, label_store, uids);

	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (label_store), &iter);

	while (valid) {
		EUIAction *action;
		GMenuItem *menu_item;
		gchar action_name[128];
		gchar *icon_name;
		gchar *label;
		gchar *tag;

		label = e_mail_label_list_store_get_name (label_store, &iter);
		icon_name = e_mail_label_list_store_dup_icon_name (label_store, &iter);
		tag = e_mail_label_list_store_get_tag (label_store, &iter);

		g_warn_if_fail (g_snprintf (action_name, sizeof (action_name), "mail-label-%d", ii) < sizeof (action_name));

		action = e_ui_action_new_stateful ("mail-labels", action_name, NULL, g_variant_new_boolean (FALSE));
		e_ui_action_set_label (action, label);
		if (icon_name && *icon_name)
			e_ui_action_set_icon_name (action, icon_name);

		g_object_set_data_full (G_OBJECT (action), "tag", tag, g_free);

		/* Configure the action before we connect to signals. */
		mail_reader_update_label_action (action, labels_info, tag);

		g_signal_connect (
			action, "notify::active",
			G_CALLBACK (action_mail_label_cb), reader);

		if (ii + 1 < 10) {
			gchar accel[5];

			accel[0] = '1' + ii;
			accel[1] = '\0';

			e_ui_action_set_accel (action, accel);
		}

		e_ui_action_group_add (action_group, action);

		menu_item = g_menu_item_new (NULL, NULL);
		e_ui_manager_update_item_from_action (ui_manager, menu_item, action);
		g_menu_append_item (priv->labels_menu, menu_item);
		g_clear_object (&menu_item);

		g_object_unref (action);
		g_free (label);
		g_free (icon_name);

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (label_store), &iter);
		ii++;
	}

	g_hash_table_destroy (labels_info);
	g_ptr_array_unref (uids);

	e_ui_manager_thaw (ui_manager);
}

static void
mail_label_change_more_checkbox_toggled_cb (GtkToggleButton *button)
{
	g_signal_handlers_block_by_func (button, mail_label_change_more_checkbox_toggled_cb, NULL);

	if (gtk_toggle_button_get_inconsistent (button) &&
	    gtk_toggle_button_get_active (button)) {
		gtk_toggle_button_set_active (button, FALSE);
		gtk_toggle_button_set_inconsistent (button, FALSE);
	} else if (!gtk_toggle_button_get_active (button)) {
		gtk_toggle_button_set_inconsistent (button, TRUE);
		gtk_toggle_button_set_active (button, FALSE);
	}

	g_signal_handlers_unblock_by_func (button, mail_label_change_more_checkbox_toggled_cb, NULL);
}

typedef struct _MoreLabelsData {
	GPtrArray *uids;
	GPtrArray *checkboxes;
	CamelFolder *folder;
} MoreLabelsData;

static void
more_labels_data_free (gpointer ptr)
{
	MoreLabelsData *mld = ptr;

	if (mld) {
		g_ptr_array_unref (mld->checkboxes);
		g_ptr_array_unref (mld->uids);
		g_clear_object (&mld->folder);
		g_free (mld);
	}
}

static void
mail_label_change_more_store_changes (MoreLabelsData *mld,
				      gboolean unset_all)
{
	guint ii, jj;

	camel_folder_freeze (mld->folder);

	for (ii = 0; ii < mld->checkboxes->len; ii++) {
		GtkToggleButton *checkbox = g_ptr_array_index (mld->checkboxes, ii);
		const gchar *tag;

		if (!unset_all && gtk_toggle_button_get_inconsistent (checkbox))
			continue;

		tag = g_object_get_data (G_OBJECT (checkbox), "tag");
		if (!tag || !*tag)
			continue;

		for (jj = 0; jj < mld->uids->len; jj++) {
			const gchar *uid = g_ptr_array_index (mld->uids, jj);
			CamelMessageInfo *nfo;

			nfo = camel_folder_get_message_info (mld->folder, uid);
			if (!nfo)
				continue;

			if (unset_all || !gtk_toggle_button_get_active (checkbox)) {
				camel_message_info_set_user_flag (nfo, tag, FALSE);
				camel_message_info_set_user_tag (nfo, "label", NULL);
			} else {
				camel_message_info_set_user_flag (nfo, tag, TRUE);
			}

			g_clear_object (&nfo);
		}
	}

	camel_folder_thaw (mld->folder);
}

static void
mail_label_change_more_set_selected_clicked_cb (GtkWidget *button,
						gpointer user_data)
{
	GtkWidget *popover = user_data;
	MoreLabelsData *mld = g_object_get_data (G_OBJECT (popover), "more-labels-data");

	g_return_if_fail (mld != NULL);

	mail_label_change_more_store_changes (mld, FALSE);

	/* do it as the last thing, it frees the 'mld' structure */
	gtk_widget_hide (popover);
}

static void
mail_label_change_more_set_none_clicked_cb (GtkWidget *button,
					    gpointer user_data)
{
	GtkWidget *popover = user_data;
	MoreLabelsData *mld = g_object_get_data (G_OBJECT (popover), "more-labels-data");

	g_return_if_fail (mld != NULL);

	mail_label_change_more_store_changes (mld, TRUE);

	/* do it as the last thing, it frees the 'mld' structure */
	gtk_widget_hide (popover);
}

static void
action_mail_label_change_more_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EMailReader *reader = user_data;
	GtkBox *box;
	GtkGrid *grid;
	GtkWidget *widget, *popover;
	EMailLabelListStore *label_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GHashTable *labels_info; /* gchar * ~> guint { LABEL_EXISTS | LABEL_NOTEXIST | LABEL_UNKNOWN } */
	GPtrArray *uids;
	GPtrArray *checkboxes;
	gboolean valid;
	guint ii, col, row, max_rows, split_to_rows;
	MoreLabelsData *mld;

	uids = e_mail_reader_get_selected_uids (reader);
	if (!uids)
		return;

	label_store = e_mail_ui_session_get_label_store (E_MAIL_UI_SESSION (e_mail_backend_get_session (e_mail_reader_get_backend (reader))));
	labels_info = mail_reader_gather_labels_info (reader, label_store, uids);

	model = GTK_TREE_MODEL (label_store);
	checkboxes = g_ptr_array_sized_new (gtk_tree_model_iter_n_children (model, NULL));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		GtkWidget *checkbox;
		gchar *label;
		gchar *icon_name;
		gchar *tag;
		guint value;
		gboolean exists, not_exists;

		label = e_mail_label_list_store_get_name (label_store, &iter);
		icon_name = e_mail_label_list_store_dup_icon_name (label_store, &iter);
		tag = e_mail_label_list_store_get_tag (label_store, &iter);

		checkbox = gtk_check_button_new_with_mnemonic (label);
		if (icon_name && *icon_name) {
			gtk_button_set_image (GTK_BUTTON (checkbox), gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU));
			gtk_button_set_always_show_image (GTK_BUTTON (checkbox), TRUE);
		}
		g_object_set_data_full (G_OBJECT (checkbox), "tag", tag, g_free);
		gtk_widget_show (checkbox);

		value = GPOINTER_TO_UINT (g_hash_table_lookup (labels_info, tag));
		exists = (value & LABEL_EXISTS) != 0;
		not_exists = (value & LABEL_NOTEXIST) != 0;

		if (exists && not_exists)
			gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (checkbox), TRUE);
		else if (exists)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);

		g_signal_connect (checkbox, "toggled",
			G_CALLBACK (mail_label_change_more_checkbox_toggled_cb), NULL);

		g_ptr_array_add (checkboxes, checkbox);

		g_free (label);
		g_free (icon_name);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (G_OBJECT (grid),
		"margin-start", 12,
		"margin-end", 12,
		"margin-top", 12,
		"margin-bottom", 12,
		"visible", TRUE,
		"column-homogeneous", TRUE,
		"column-spacing", 4,
		"row-spacing", 4,
		NULL);

	/* The '7' is just a magic value, to have up to 7 rows in a column */
	split_to_rows = 7;
	if (checkboxes->len > split_to_rows) {
		#define need_cols(_n_rows) ((checkboxes->len / (_n_rows)) + ((checkboxes->len % (_n_rows)) == 0 ? 0 : 1))
		/* to make it look more naturally, when the last column is less than half filled */
		while (split_to_rows > 5 && (checkboxes->len % split_to_rows) <= split_to_rows / 2 + 1) {
			/* using one-less row does not increase the column count, then use less rows */
			if (need_cols (split_to_rows) == need_cols (split_to_rows - 1))
				split_to_rows--;
			else
				break;
		}
		#undef need_cols
	}

	col = 0;
	row = 1;
	max_rows = 1;

	for (ii = 0; ii < checkboxes->len; ii++) {
		widget = g_ptr_array_index (checkboxes, ii);

		gtk_grid_attach (grid, widget, col, row, 1, 1);
		row++;

		if (row - 1 == split_to_rows && ii + 1 < checkboxes->len) {
			row = 1;
			col++;
		}

		if (row > max_rows)
			max_rows = row;
	}

	mld = g_new0 (MoreLabelsData, 1);
	mld->uids = uids;
	mld->checkboxes = checkboxes;
	mld->folder = e_mail_reader_ref_folder (reader);

	popover = gtk_popover_new (GTK_WIDGET (e_mail_reader_get_message_list (reader)));
	gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);
	gtk_container_add (GTK_CONTAINER (popover), GTK_WIDGET (grid));

	g_object_set_data_full (G_OBJECT (popover), "more-labels-data", mld, more_labels_data_free);

	widget = gtk_label_new (g_dngettext (GETTEXT_PACKAGE, "Modify labels for selected message", "Modify labels for selected messages", mld->uids->len));
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 0, col + 1, 1);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
	box = GTK_BOX (widget);

	widget = gtk_button_new_with_mnemonic (_("Set _Selected"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (mail_label_change_more_set_selected_clicked_cb), popover);

	widget = gtk_button_new_with_mnemonic (_("Set _None"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	g_signal_connect (widget, "clicked",
		G_CALLBACK (mail_label_change_more_set_none_clicked_cb), popover);

	widget = gtk_button_new_with_mnemonic (_("_Cancel"));
	gtk_box_pack_start (box, widget, FALSE, FALSE, 0);

	g_signal_connect_swapped (widget, "clicked",
		G_CALLBACK (gtk_widget_hide), popover);

	gtk_widget_show_all (GTK_WIDGET (box));
	gtk_grid_attach (grid, GTK_WIDGET (box), 0, max_rows + 1, col + 1, 1);

	g_signal_connect (popover, "closed",
		G_CALLBACK (gtk_widget_destroy), NULL);

	widget = gtk_popover_get_relative_to (GTK_POPOVER (popover));
	if (widget) {
		GtkAllocation allocation;
		GdkRectangle rect;

		gtk_widget_get_allocation (widget, &allocation);

		/* Point into the middle of the message list, for a lack of easy check where the list's cursor/selection is */
		rect.x = allocation.width / 2;
		rect.y = allocation.height / 2;
		rect.width = 1;
		rect.height = 1;

		gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
	}

	gtk_widget_show (popover);

	g_hash_table_destroy (labels_info);
}

static void
mail_reader_update_actions (EMailReader *reader,
                            guint32 state)
{
	EUIAction *action;
	gboolean sensitive;
	gboolean visible;
	EMailDisplay *mail_display;
	CamelFolder *folder;

	/* Be descriptive. */
	gboolean any_messages_selected;
	gboolean enable_flag_clear;
	gboolean enable_flag_completed;
	gboolean have_enabled_account;
	gboolean multiple_messages_selected;
	gboolean selection_has_attachment_messages;
	gboolean selection_has_deleted_messages;
	gboolean selection_has_ignore_thread_messages;
	gboolean selection_has_notignore_thread_messages;
	gboolean selection_has_important_messages;
	gboolean selection_has_junk_messages;
	gboolean selection_has_not_junk_messages;
	gboolean selection_has_read_messages;
	gboolean selection_has_undeleted_messages;
	gboolean selection_has_unimportant_messages;
	gboolean selection_has_unread_messages;
	gboolean selection_has_mail_note;
	gboolean selection_has_color;
	gboolean selection_is_mailing_list;
	gboolean single_message_selected;
	gboolean first_message_selected = FALSE;
	gboolean last_message_selected = FALSE;

	have_enabled_account =
		(state & E_MAIL_READER_HAVE_ENABLED_ACCOUNT);
	single_message_selected =
		(state & E_MAIL_READER_SELECTION_SINGLE);
	multiple_messages_selected =
		(state & E_MAIL_READER_SELECTION_MULTIPLE);
	/* FIXME Missing CAN_ADD_SENDER */
	enable_flag_clear =
		(state & E_MAIL_READER_SELECTION_FLAG_CLEAR);
	enable_flag_completed =
		(state & E_MAIL_READER_SELECTION_FLAG_COMPLETED);
	selection_has_attachment_messages =
		(state & E_MAIL_READER_SELECTION_HAS_ATTACHMENTS);
	selection_has_deleted_messages =
		(state & E_MAIL_READER_SELECTION_HAS_DELETED);
	selection_has_ignore_thread_messages =
		(state & E_MAIL_READER_SELECTION_HAS_IGNORE_THREAD);
	selection_has_notignore_thread_messages =
		(state & E_MAIL_READER_SELECTION_HAS_NOTIGNORE_THREAD);
	selection_has_important_messages =
		(state & E_MAIL_READER_SELECTION_HAS_IMPORTANT);
	selection_has_junk_messages =
		(state & E_MAIL_READER_SELECTION_HAS_JUNK);
	selection_has_not_junk_messages =
		(state & E_MAIL_READER_SELECTION_HAS_NOT_JUNK);
	selection_has_read_messages =
		(state & E_MAIL_READER_SELECTION_HAS_READ);
	selection_has_undeleted_messages =
		(state & E_MAIL_READER_SELECTION_HAS_UNDELETED);
	selection_has_unimportant_messages =
		(state & E_MAIL_READER_SELECTION_HAS_UNIMPORTANT);
	selection_has_unread_messages =
		(state & E_MAIL_READER_SELECTION_HAS_UNREAD);
	selection_has_mail_note =
		(state & E_MAIL_READER_SELECTION_HAS_MAIL_NOTE);
	selection_has_color =
		(state & E_MAIL_READER_SELECTION_HAS_COLOR);
	selection_is_mailing_list =
		(state & E_MAIL_READER_SELECTION_IS_MAILING_LIST);

	any_messages_selected =
		(single_message_selected || multiple_messages_selected);

	mail_display = e_mail_reader_get_mail_display (reader);

	if (any_messages_selected) {
		MessageList *message_list;
		gint row = -1, count = -1;
		ETreeTableAdapter *etta;
		ETreePath node = NULL;

		message_list = MESSAGE_LIST (
			e_mail_reader_get_message_list (reader));
		etta = e_tree_get_table_adapter (E_TREE (message_list));

		if (message_list->cursor_uid != NULL)
			node = g_hash_table_lookup (
				message_list->uid_nodemap,
				message_list->cursor_uid);

		if (node != NULL) {
			row = e_tree_table_adapter_row_of_node (etta, node);
			count = e_table_model_row_count (E_TABLE_MODEL (etta));
		}

		first_message_selected = row <= 0;
		last_message_selected = row < 0 || row + 1 >= count;
	}

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-add-sender");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-archive");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-check-for-junk");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-color-assign");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected && selection_has_color;
	action = e_mail_reader_get_action (reader, "mail-color-unset");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-copy");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-create-menu");
	e_ui_action_set_sensitive (action, sensitive);

	/* If a single message is selected, let the user hit delete to
	 * advance the cursor even if the message is already deleted. */
	sensitive =
		(single_message_selected ||
		selection_has_undeleted_messages) &&
		(state & E_MAIL_READER_FOLDER_IS_VTRASH) == 0;
	action = e_mail_reader_get_action (reader, "mail-delete");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected && !selection_has_mail_note;
	action = e_mail_reader_get_action (reader, "mail-add-note");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_visible (action, sensitive);

	sensitive = single_message_selected && selection_has_mail_note;
	action = e_mail_reader_get_action (reader, "mail-edit-note");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_visible (action, sensitive);

	sensitive = single_message_selected && selection_has_mail_note;
	action = e_mail_reader_get_action (reader, "mail-delete-note");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_visible (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-filters-apply");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected && selection_is_mailing_list;
	action = e_mail_reader_get_action (reader, "mail-filter-rule-for-mailing-list");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected && mail_display && gtk_widget_is_visible (GTK_WIDGET (mail_display));
	action = e_mail_reader_get_action (reader, "mail-find");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = enable_flag_clear;
	action = e_mail_reader_get_action (reader, "mail-flag-clear");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = enable_flag_completed;
	action = e_mail_reader_get_action (reader, "mail-flag-completed");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-flag-for-followup");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-forward");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-forward-attached");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-forward-as-menu");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-forward-inline");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-forward-quoted");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = TRUE;
	action = e_mail_reader_get_action (reader, "mail-goto-menu");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-load-images");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-mark-as-menu");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = selection_has_notignore_thread_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-ignore-thread-sub");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_visible (action, sensitive);

	sensitive = selection_has_notignore_thread_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-ignore-thread-whole");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_visible (action, sensitive);

	sensitive = selection_has_unimportant_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-important");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = selection_has_not_junk_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-junk");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = selection_has_junk_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-notjunk");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = selection_has_unread_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-read");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = selection_has_ignore_thread_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-unignore-thread-sub");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_visible (action, sensitive);

	sensitive = selection_has_ignore_thread_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-unignore-thread-whole");
	e_ui_action_set_sensitive (action, sensitive);
	e_ui_action_set_visible (action, sensitive);

	sensitive = selection_has_important_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-unimportant");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = selection_has_read_messages;
	action = e_mail_reader_get_action (reader, "mail-mark-unread");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-message-edit");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account;
	action = e_mail_reader_get_action (reader, "mail-message-new");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-message-open");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-move");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected && !last_message_selected;
	action = e_mail_reader_get_action (reader, "mail-next");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-next-important");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected && !last_message_selected;
	action = e_mail_reader_get_action (reader, "mail-next-thread");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = TRUE;
	action = e_mail_reader_get_action (reader, "mail-next-unread");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected && !first_message_selected;
	action = e_mail_reader_get_action (reader, "mail-previous");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-previous-important");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = TRUE;
	action = e_mail_reader_get_action (reader, "mail-previous-unread");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected && !first_message_selected;
	action = e_mail_reader_get_action (reader, "mail-previous-thread");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-print");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-print-preview");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-redirect");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected && selection_has_attachment_messages;
	action = e_mail_reader_get_action (reader, "mail-remove-attachments");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = multiple_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-remove-duplicates");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-reply-all");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-reply-alternative");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-reply-group");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-reply-group-menu");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && single_message_selected &&
		selection_is_mailing_list;
	action = e_mail_reader_get_action (reader, "mail-reply-list");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-reply-sender");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, "mail-save-as");
	e_ui_action_set_sensitive (action, sensitive);

	folder = e_mail_reader_ref_folder (reader);
	visible = CAMEL_IS_VEE_FOLDER (folder);
	if (visible) {
		GtkWindow *window = e_mail_reader_get_window (reader);
		/* to be able to switch also the selected folder in the folder tree */
		visible = E_IS_SHELL_WINDOW (window);
	}
	sensitive = single_message_selected && visible;
	action = e_mail_reader_get_action (reader, "mail-goto-containing-folder");
	e_ui_action_set_sensitive (action, sensitive);
	/* hide when cannot be used at all, like in the real folders */
	e_ui_action_set_visible (action, visible);
	g_clear_object (&folder);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-show-source");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = selection_has_deleted_messages;
	action = e_mail_reader_get_action (reader, "mail-undelete");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-zoom-100");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-zoom-in");
	e_ui_action_set_sensitive (action, sensitive);

	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, "mail-zoom-out");
	e_ui_action_set_sensitive (action, sensitive);

	action = e_mail_reader_get_action (reader, "mail-search-web");
	e_ui_action_set_sensitive (action, single_message_selected &&
		mail_display && e_web_view_has_selection (E_WEB_VIEW (mail_display)));

	mail_reader_update_labels_menu (reader);
}

static gboolean
mail_reader_close_on_delete_or_junk (EMailReader *reader)
{
	return FALSE;
}

static void
e_mail_reader_default_init (EMailReaderInterface *iface)
{
	quark_private = g_quark_from_static_string ("e-mail-reader-private");

	iface->init_ui_data = e_mail_reader_init_ui_data_default;
	iface->get_alert_sink = mail_reader_get_alert_sink;
	iface->get_selected_uids = mail_reader_get_selected_uids;
	iface->get_selected_uids_with_collapsed_threads = mail_reader_get_selected_uids_with_collapsed_threads;
	iface->ref_folder = mail_reader_ref_folder;
	iface->set_folder = mail_reader_set_folder;
	iface->set_message = mail_reader_set_message;
	iface->open_selected_mail = e_mail_reader_open_selected;
	iface->folder_loaded = mail_reader_folder_loaded;
	iface->message_loaded = mail_reader_message_loaded;
	iface->message_seen = mail_reader_message_seen;
	iface->show_search_bar = mail_reader_show_search_bar;
	iface->update_actions = mail_reader_update_actions;
	iface->close_on_delete_or_junk = mail_reader_close_on_delete_or_junk;
	iface->reload = mail_reader_reload;

	g_object_interface_install_property (
		iface,
		g_param_spec_enum (
			"forward-style",
			"Forward Style",
			"How to forward messages",
			E_TYPE_MAIL_FORWARD_STYLE,
			E_MAIL_FORWARD_STYLE_ATTACHED,
			G_PARAM_READWRITE));

	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"group-by-threads",
			"Group by Threads",
			"Whether to group messages by threads",
			FALSE,
			G_PARAM_READWRITE));

	g_object_interface_install_property (
		iface,
		g_param_spec_enum (
			"reply-style",
			"Reply Style",
			"How to reply to messages",
			E_TYPE_MAIL_REPLY_STYLE,
			E_MAIL_REPLY_STYLE_QUOTED,
			G_PARAM_READWRITE));

	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"mark-seen-always",
			"Mark Seen Always",
			"Whether to mark unread message seen even after folder change",
			FALSE,
			G_PARAM_READWRITE));

	g_object_interface_install_property (
		iface,
		g_param_spec_boolean (
			"delete-selects-previous",
			"Delete Selects Previous",
			"Whether go to the previous message after message deletion",
			FALSE,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[COMPOSER_CREATED] = g_signal_new (
		"composer-created",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailReaderInterface, composer_created),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		E_TYPE_MSG_COMPOSER,
		CAMEL_TYPE_MIME_MESSAGE);

	signals[FOLDER_LOADED] = g_signal_new (
		"folder-loaded",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMailReaderInterface, folder_loaded),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[MESSAGE_LOADED] = g_signal_new (
		"message-loaded",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailReaderInterface, message_loaded),
		NULL, NULL,
		e_marshal_VOID__STRING_OBJECT,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		CAMEL_TYPE_MIME_MESSAGE);

	signals[MESSAGE_SEEN] = g_signal_new (
		"message-seen",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailReaderInterface, message_seen),
		NULL, NULL,
		e_marshal_VOID__STRING_OBJECT,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		CAMEL_TYPE_MIME_MESSAGE);

	signals[SHOW_SEARCH_BAR] = g_signal_new (
		"show-search-bar",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailReaderInterface, show_search_bar),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailReaderInterface, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__UINT,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);
}

void
e_mail_reader_init (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	GtkWidget *message_list;
	EMailDisplay *display;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	message_list = e_mail_reader_get_message_list (reader);
	display = e_mail_reader_get_mail_display (reader);

	/* Initialize a private struct. */
	priv = g_new0 (EMailReaderPrivate, 1);
	g_object_set_qdata_full (G_OBJECT (reader), quark_private, priv, (GDestroyNotify) mail_reader_private_free);

	e_binding_bind_property (
		reader, "group-by-threads",
		message_list, "group-by-threads",
		G_BINDING_SYNC_CREATE);

	priv->labels_menu = g_menu_new ();

	/* Fine tuning. */

	g_signal_connect (
		e_mail_display_get_action (display, "add-to-address-book"), "activate",
		G_CALLBACK (action_add_to_address_book_cb), reader);

	g_signal_connect (
		e_mail_display_get_action (display, "send-reply"), "activate",
		G_CALLBACK (action_mail_reply_recipient_cb), reader);

	g_signal_connect (
		e_mail_display_get_action (display, "search-folder-recipient"), "activate",
		G_CALLBACK (action_search_folder_recipient_cb), reader);

	g_signal_connect (
		e_mail_display_get_action (display, "search-folder-sender"), "activate",
		G_CALLBACK (action_search_folder_sender_cb), reader);

	/* Connect signals. */
	g_signal_connect_swapped (
		display, "load-changed",
		G_CALLBACK (mail_reader_load_changed_cb), reader);

	g_signal_connect_swapped (
		display, "remote-content-clicked",
		G_CALLBACK (mail_reader_remote_content_clicked_cb), reader);

	g_signal_connect_swapped (
		display, "autocrypt-import-clicked",
		G_CALLBACK (mail_reader_autocrypt_import_clicked_cb), reader);

	g_signal_connect_swapped (
		message_list, "message-selected",
		G_CALLBACK (mail_reader_message_selected_cb), reader);

	g_signal_connect_swapped (
		message_list, "update-actions",
		G_CALLBACK (mail_reader_message_list_suggest_update_actions_cb), reader);

	/* re-schedule mark-as-seen,... */
	g_signal_connect_swapped (
		message_list, "cursor-change",
		G_CALLBACK (mail_reader_message_cursor_change_cb), reader);

	/* but do not mark-as-seen if... */
	g_signal_connect_swapped (
		message_list, "tree-drag-begin",
		G_CALLBACK (discard_timeout_mark_seen_cb), reader);

	g_signal_connect_swapped (
		message_list, "tree-drag-end",
		G_CALLBACK (discard_timeout_mark_seen_cb), reader);

	g_signal_connect_swapped (
		message_list, "right-click",
		G_CALLBACK (discard_timeout_mark_seen_cb), reader);

	g_signal_connect_swapped (
		e_mail_reader_get_preview_pane (reader), "notify::visible",
		G_CALLBACK (mail_reader_preview_pane_visible_changed_cb), reader);

	g_signal_connect_after (
		message_list, "message-list-built",
		G_CALLBACK (mail_reader_message_list_built_cb), reader);

	g_signal_connect_swapped (
		message_list, "double-click",
		G_CALLBACK (mail_reader_double_click_cb), reader);

	g_signal_connect_swapped (
		message_list, "selection-change",
		G_CALLBACK (e_mail_reader_changed), reader);
}

static void
mail_reader_ongoing_operation_destroyed (gpointer user_data,
					 GObject *cancellable)
{
	EMailReader *reader = user_data;
	EMailReaderPrivate *priv;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	priv->ongoing_operations = g_slist_remove (priv->ongoing_operations, cancellable);
}

void
e_mail_reader_dispose (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	EMailDisplay *mail_display;
	GtkWidget *message_list;
	GSList *ongoing_operations, *link;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	if (priv->message_selected_timeout_id > 0) {
		g_source_remove (priv->message_selected_timeout_id);
		priv->message_selected_timeout_id = 0;
	}

	if (priv->retrieving_message)
		g_cancellable_cancel (priv->retrieving_message);

	ongoing_operations = g_slist_copy_deep (priv->ongoing_operations, (GCopyFunc) g_object_ref, NULL);
	g_slist_free (priv->ongoing_operations);
	priv->ongoing_operations = NULL;

	for (link = ongoing_operations; link; link = g_slist_next (link)) {
		GCancellable *cancellable = link->data;

		g_object_weak_unref (G_OBJECT (cancellable), mail_reader_ongoing_operation_destroyed, reader);

		g_cancellable_cancel (cancellable);
	}

	g_slist_free_full (ongoing_operations, g_object_unref);

	mail_display = e_mail_reader_get_mail_display (reader);
	if (mail_display)
		g_signal_handlers_disconnect_by_data (mail_display, reader);

	message_list = e_mail_reader_get_message_list (reader);
	if (message_list)
		g_signal_handlers_disconnect_by_data (message_list, reader);
}

void
e_mail_reader_changed (EMailReader *reader)
{
	MessageList *message_list;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	g_signal_emit (reader, signals[CHANGED], 0);

	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));

	if (!message_list || message_list_selected_count (message_list) != 1)
		mail_reader_remove_followup_alert (reader);
}

guint32
e_mail_reader_check_state (EMailReader *reader)
{
	EShell *shell;
	GPtrArray *uids;
	CamelFolder *folder;
	CamelStore *store = NULL;
	EMailBackend *backend;
	ESourceRegistry *registry;
	EMailSession *mail_session;
	EMailAccountStore *account_store;
	const gchar *tag;
	gboolean can_clear_flags = FALSE;
	gboolean can_flag_completed = FALSE;
	gboolean can_flag_for_followup = FALSE;
	gboolean has_attachments = FALSE;
	gboolean has_deleted = FALSE;
	gboolean has_ignore_thread = FALSE;
	gboolean has_notignore_thread = FALSE;
	gboolean has_important = FALSE;
	gboolean has_junk = FALSE;
	gboolean has_not_junk = FALSE;
	gboolean has_read = FALSE;
	gboolean has_undeleted = FALSE;
	gboolean has_unimportant = FALSE;
	gboolean has_unread = FALSE;
	gboolean has_mail_note = FALSE;
	gboolean has_color = FALSE;
	gboolean have_enabled_account = FALSE;
	gboolean drafts_or_outbox = FALSE;
	gboolean is_mailing_list;
	gboolean is_junk_folder = FALSE;
	gboolean is_vtrash_folder = FALSE;
	guint32 state = 0;
	guint ii;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	backend = e_mail_reader_get_backend (reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));
	registry = e_shell_get_registry (shell);
	mail_session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (
		E_MAIL_UI_SESSION (mail_session));

	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);

	if (folder != NULL) {
		guint32 folder_flags;

		store = camel_folder_get_parent_store (folder);
		folder_flags = camel_folder_get_flags (folder);
		is_junk_folder = (folder_flags & CAMEL_FOLDER_IS_JUNK) != 0;
		is_vtrash_folder = (camel_store_get_flags (store) & CAMEL_STORE_VTRASH) != 0 && (folder_flags & CAMEL_FOLDER_IS_TRASH) != 0;
		drafts_or_outbox =
			em_utils_folder_is_drafts (registry, folder) ||
			em_utils_folder_is_outbox (registry, folder);
	}

	/* Initialize this flag based on whether there are any
	 * messages selected.  We will update it in the loop. */
	is_mailing_list = (uids->len > 0);

	for (ii = 0; ii < uids->len; ii++) {
		CamelMessageInfo *info;
		const gchar *string;
		guint32 flags;

		info = camel_folder_get_message_info (
			folder, uids->pdata[ii]);
		if (info == NULL)
			continue;

		flags = camel_message_info_get_flags (info);

		if (flags & CAMEL_MESSAGE_SEEN)
			has_read = TRUE;
		else
			has_unread = TRUE;

		if (flags & CAMEL_MESSAGE_ATTACHMENTS)
			has_attachments = TRUE;

		if (drafts_or_outbox) {
			has_junk = FALSE;
			has_not_junk = FALSE;
		} else {
			guint32 bitmask;

			/* XXX Strictly speaking, this logic is correct.
			 *     Problem is there's nothing in the message
			 *     list that indicates whether a message is
			 *     already marked "Not Junk".  So the user may
			 *     think the "Not Junk" button is enabling and
			 *     disabling itself randomly as he reads mail. */

			if (flags & CAMEL_MESSAGE_JUNK)
				has_junk = TRUE;
			if (flags & CAMEL_MESSAGE_NOTJUNK)
				has_not_junk = TRUE;

			bitmask = CAMEL_MESSAGE_JUNK | CAMEL_MESSAGE_NOTJUNK;

			/* If neither junk flag is set, the
			 * message can be marked either way. */
			if ((flags & bitmask) == 0) {
				has_junk = TRUE;
				has_not_junk = TRUE;
			}
		}

		if (flags & CAMEL_MESSAGE_DELETED)
			has_deleted = TRUE;
		else
			has_undeleted = TRUE;

		if (flags & CAMEL_MESSAGE_FLAGGED)
			has_important = TRUE;
		else
			has_unimportant = TRUE;

		tag = camel_message_info_get_user_tag (info, "follow-up");
		if (tag != NULL && *tag != '\0') {
			can_clear_flags = TRUE;
			tag = camel_message_info_get_user_tag (
				info, "completed-on");
			if (tag == NULL || *tag == '\0')
				can_flag_completed = TRUE;
		} else
			can_flag_for_followup = TRUE;

		string = camel_message_info_get_mlist (info);
		is_mailing_list &= (string != NULL && *string != '\0');

		has_ignore_thread = has_ignore_thread || camel_message_info_get_user_flag (info, "ignore-thread");
		has_notignore_thread = has_notignore_thread || !camel_message_info_get_user_flag (info, "ignore-thread");
		has_mail_note = has_mail_note || camel_message_info_get_user_flag (info, E_MAIL_NOTES_USER_FLAG);
		has_color = has_color || camel_message_info_get_user_tag (info, "color") != NULL;

		g_clear_object (&info);
	}

	have_enabled_account =
		e_mail_account_store_have_enabled_service (
		account_store, CAMEL_TYPE_STORE);

	if (have_enabled_account)
		state |= E_MAIL_READER_HAVE_ENABLED_ACCOUNT;
	if (uids->len == 1)
		state |= E_MAIL_READER_SELECTION_SINGLE;
	if (uids->len > 1)
		state |= E_MAIL_READER_SELECTION_MULTIPLE;
	if (!drafts_or_outbox && uids->len == 1)
		state |= E_MAIL_READER_SELECTION_CAN_ADD_SENDER;
	if (can_clear_flags)
		state |= E_MAIL_READER_SELECTION_FLAG_CLEAR;
	if (can_flag_completed)
		state |= E_MAIL_READER_SELECTION_FLAG_COMPLETED;
	if (can_flag_for_followup)
		state |= E_MAIL_READER_SELECTION_FLAG_FOLLOWUP;
	if (has_attachments)
		state |= E_MAIL_READER_SELECTION_HAS_ATTACHMENTS;
	if (has_deleted)
		state |= E_MAIL_READER_SELECTION_HAS_DELETED;
	if (has_ignore_thread)
		state |= E_MAIL_READER_SELECTION_HAS_IGNORE_THREAD;
	if (has_notignore_thread)
		state |= E_MAIL_READER_SELECTION_HAS_NOTIGNORE_THREAD;
	if (has_important)
		state |= E_MAIL_READER_SELECTION_HAS_IMPORTANT;
	if (has_junk)
		state |= E_MAIL_READER_SELECTION_HAS_JUNK;
	if (has_not_junk)
		state |= E_MAIL_READER_SELECTION_HAS_NOT_JUNK;
	if (has_read)
		state |= E_MAIL_READER_SELECTION_HAS_READ;
	if (has_undeleted)
		state |= E_MAIL_READER_SELECTION_HAS_UNDELETED;
	if (has_unimportant)
		state |= E_MAIL_READER_SELECTION_HAS_UNIMPORTANT;
	if (has_unread)
		state |= E_MAIL_READER_SELECTION_HAS_UNREAD;
	if (is_mailing_list)
		state |= E_MAIL_READER_SELECTION_IS_MAILING_LIST;
	if (is_junk_folder)
		state |= E_MAIL_READER_FOLDER_IS_JUNK;
	if (is_vtrash_folder)
		state |= E_MAIL_READER_FOLDER_IS_VTRASH;
	if (has_mail_note)
		state |= E_MAIL_READER_SELECTION_HAS_MAIL_NOTE;
	if (has_color)
		state |= E_MAIL_READER_SELECTION_HAS_COLOR;

	if (!(state & E_MAIL_READER_SELECTION_SINGLE)) {
		GPtrArray *real_selected_uids;

		real_selected_uids = e_mail_reader_get_selected_uids (reader);

		if (real_selected_uids && real_selected_uids->len == 1) {
			state |= E_MAIL_READER_SELECTION_SINGLE;
		}

		if (real_selected_uids)
			g_ptr_array_unref (real_selected_uids);
	}

	g_clear_object (&folder);
	g_ptr_array_unref (uids);

	return state;
}

EActivity *
e_mail_reader_new_activity (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	EActivity *activity;
	EMailBackend *backend;
	EAlertSink *alert_sink;
	GCancellable *cancellable;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	activity = e_activity_new ();

	alert_sink = e_mail_reader_get_alert_sink (reader);
	e_activity_set_alert_sink (activity, alert_sink);

	cancellable = camel_operation_new ();

	priv->ongoing_operations = g_slist_prepend (priv->ongoing_operations, cancellable);
	g_object_weak_ref (G_OBJECT (cancellable), mail_reader_ongoing_operation_destroyed, reader);

	e_activity_set_cancellable (activity, cancellable);
	g_object_unref (cancellable);

	backend = e_mail_reader_get_backend (reader);
	e_shell_backend_add_activity (E_SHELL_BACKEND (backend), activity);

	return activity;
}

void
e_mail_reader_update_actions (EMailReader *reader,
                              guint32 state)
{
	EUIManager *ui_manager;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	ui_manager = e_mail_reader_get_ui_manager (reader);

	if (ui_manager)
		e_ui_manager_freeze (ui_manager);

	g_signal_emit (reader, signals[UPDATE_ACTIONS], 0, state);

	if (ui_manager)
		e_ui_manager_thaw (ui_manager);
}

void
e_mail_reader_init_ui_data (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	if (iface->init_ui_data != NULL)
		iface->init_ui_data (reader);
}

EUIManager *
e_mail_reader_get_ui_manager (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_ui_manager != NULL, NULL);

	return iface->get_ui_manager (reader);
}

EUIAction *
e_mail_reader_get_action (EMailReader *reader,
                          const gchar *action_name)
{
	EUIAction *action;
	EUIManager *ui_manager;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	ui_manager = e_mail_reader_get_ui_manager (reader);
	if (!ui_manager)
		return NULL;

	action = e_ui_manager_get_action (ui_manager, action_name);
	if (action == NULL)
		g_critical ("%s: action '%s' not found", G_STRFUNC, action_name);

	return action;
}

EAlertSink *
e_mail_reader_get_alert_sink (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_alert_sink != NULL, NULL);

	return iface->get_alert_sink (reader);
}

EMailBackend *
e_mail_reader_get_backend (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_backend != NULL, NULL);

	return iface->get_backend (reader);
}

EMailDisplay *
e_mail_reader_get_mail_display (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_mail_display != NULL, NULL);

	return iface->get_mail_display (reader);
}

gboolean
e_mail_reader_get_hide_deleted (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_hide_deleted != NULL, FALSE);

	return iface->get_hide_deleted (reader);
}

GtkWidget *
e_mail_reader_get_message_list (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_message_list != NULL, NULL);

	return iface->get_message_list (reader);
}

GtkMenu *
e_mail_reader_get_popup_menu (EMailReader *reader)
{
	EUIManager *ui_manager;
	GObject *ui_object;
	GtkMenu *menu;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	ui_manager = e_mail_reader_get_ui_manager (reader);
	if (!ui_manager)
		return NULL;

	ui_object = e_ui_manager_create_item (ui_manager, "mail-preview-popup");
	g_return_val_if_fail (G_IS_MENU_MODEL (ui_object), NULL);

	menu = GTK_MENU (gtk_menu_new_from_model (G_MENU_MODEL (ui_object)));

	g_clear_object (&ui_object);

	gtk_menu_attach_to_widget (menu, GTK_WIDGET (reader), NULL);
	e_util_connect_menu_detach_after_deactivate (menu);

	return menu;
}

EPreviewPane *
e_mail_reader_get_preview_pane (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_preview_pane != NULL, NULL);

	return iface->get_preview_pane (reader);
}

GPtrArray *
e_mail_reader_get_selected_uids (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_selected_uids != NULL, NULL);

	return iface->get_selected_uids (reader);
}

GPtrArray *
e_mail_reader_get_selected_uids_with_collapsed_threads (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_selected_uids_with_collapsed_threads != NULL, NULL);

	return iface->get_selected_uids_with_collapsed_threads (reader);
}

GtkWindow *
e_mail_reader_get_window (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->get_window != NULL, NULL);

	return iface->get_window (reader);
}

gboolean
e_mail_reader_close_on_delete_or_junk (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	iface = E_MAIL_READER_GET_INTERFACE (reader);

	return iface->close_on_delete_or_junk != NULL &&
	       iface->close_on_delete_or_junk (reader);
}

CamelFolder *
e_mail_reader_ref_folder (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->ref_folder != NULL, NULL);

	return iface->ref_folder (reader);
}

void
e_mail_reader_set_folder (EMailReader *reader,
                          CamelFolder *folder)
{
	EMailReaderInterface *iface;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_if_fail (iface->set_folder != NULL);

	iface->set_folder (reader, folder);
}

void
e_mail_reader_set_message (EMailReader *reader,
                           const gchar *message_uid)
{
	EMailReaderInterface *iface;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_if_fail (iface->set_message != NULL);

	iface->set_message (reader, message_uid);
}

guint
e_mail_reader_open_selected_mail (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (iface->open_selected_mail != NULL, 0);

	return iface->open_selected_mail (reader);
}

EMailForwardStyle
e_mail_reader_get_forward_style (EMailReader *reader)
{
	EMailReaderPrivate *priv;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	return priv->forward_style;
}

void
e_mail_reader_set_forward_style (EMailReader *reader,
                                 EMailForwardStyle style)
{
	EMailReaderPrivate *priv;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	if (priv->forward_style == style)
		return;

	priv->forward_style = style;

	g_object_notify (G_OBJECT (reader), "forward-style");
}

gboolean
e_mail_reader_get_group_by_threads (EMailReader *reader)
{
	EMailReaderPrivate *priv;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	return priv->group_by_threads;
}

void
e_mail_reader_set_group_by_threads (EMailReader *reader,
                                    gboolean group_by_threads)
{
	EMailReaderPrivate *priv;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	if (priv->group_by_threads == group_by_threads)
		return;

	priv->group_by_threads = group_by_threads;

	g_object_notify (G_OBJECT (reader), "group-by-threads");
}

EMailReplyStyle
e_mail_reader_get_reply_style (EMailReader *reader)
{
	EMailReaderPrivate *priv;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	return priv->reply_style;
}

void
e_mail_reader_set_reply_style (EMailReader *reader,
                               EMailReplyStyle style)
{
	EMailReaderPrivate *priv;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	if (priv->reply_style == style)
		return;

	priv->reply_style = style;

	g_object_notify (G_OBJECT (reader), "reply-style");
}

gboolean
e_mail_reader_get_mark_seen_always (EMailReader *reader)
{
	EMailReaderPrivate *priv;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	return priv->mark_seen_always;
}

void
e_mail_reader_set_mark_seen_always (EMailReader *reader,
                                    gboolean mark_seen_always)
{
	EMailReaderPrivate *priv;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	if (priv->mark_seen_always == mark_seen_always)
		return;

	priv->mark_seen_always = mark_seen_always;

	g_object_notify (G_OBJECT (reader), "mark-seen-always");
}

gboolean
e_mail_reader_get_delete_selects_previous (EMailReader *reader)
{
	EMailReaderPrivate *priv;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	return priv->delete_selects_previous;
}

void
e_mail_reader_set_delete_selects_previous (EMailReader *reader,
					   gboolean delete_selects_previous)
{
	EMailReaderPrivate *priv;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	if (priv->delete_selects_previous == delete_selects_previous)
		return;

	priv->delete_selects_previous = delete_selects_previous;

	g_object_notify (G_OBJECT (reader), "delete-selects-previous");
}

void
e_mail_reader_show_search_bar (EMailReader *reader)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));

	g_signal_emit (reader, signals[SHOW_SEARCH_BAR], 0);
}

void
e_mail_reader_avoid_next_mark_as_seen (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	MessageList *message_list;

	g_return_if_fail (reader != NULL);

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	g_return_if_fail (priv != NULL);

	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));
	g_return_if_fail (message_list != NULL);

	priv->avoid_next_mark_as_seen = TRUE;
}

void
e_mail_reader_unset_folder_just_selected (EMailReader *reader)
{
	EMailReaderPrivate *priv;

	g_return_if_fail (reader != NULL);

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	g_return_if_fail (priv != NULL);

	priv->folder_was_just_selected = FALSE;
}

/**
 * e_mail_reader_composer_created:
 * @reader: an #EMailReader
 * @composer: an #EMsgComposer
 * @message: the source #CamelMimeMessage, or %NULL
 *
 * Emits an #EMailReader::composer-created signal to indicate the @composer
 * window was created in response to a user action on @reader.  Examples of
 * such actions include replying, forwarding, and composing a new message.
 * If applicable, the source @message (i.e. the message being replied to or
 * forwarded) should be included.
 **/
void
e_mail_reader_composer_created (EMailReader *reader,
                                EMsgComposer *composer,
                                CamelMimeMessage *message)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (message != NULL)
		g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	g_signal_emit (
		reader, signals[COMPOSER_CREATED], 0, composer, message);
}

void
e_mail_reader_reload (EMailReader *reader)
{
	EMailReaderInterface *iface;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	iface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_if_fail (iface->reload != NULL);

	iface->reload (reader);
}

gboolean
e_mail_reader_ignore_accel (EMailReader *reader)
{
	EMailDisplay *mail_display;
	GtkWidget *toplevel;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	mail_display = e_mail_reader_get_mail_display (E_MAIL_READER (reader));

	if (!mail_display)
		return FALSE;

	if (gtk_widget_has_focus (GTK_WIDGET (mail_display)) &&
	    e_web_view_get_need_input (E_WEB_VIEW (mail_display)))
		return TRUE;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (mail_display));

	if (GTK_IS_WINDOW (toplevel))
		return e_util_ignore_accel_for_focused (gtk_window_get_focus (GTK_WINDOW (toplevel)));

	return FALSE;
}
