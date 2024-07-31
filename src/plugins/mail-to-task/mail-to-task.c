/*
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
 * Authors:
 *		Michael Zucchi <notzed@novell.com>
 *		Rodrigo Moya <rodrigo@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* Convert a mail message into a task */

#include "evolution-config.h"

#include <stdio.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include <libecal/libecal.h>

#include "shell/e-shell-view.h"
#include "shell/e-shell-window-actions.h"

#include "mail/e-mail-browser.h"
#include "mail/e-mail-view.h"
#include "mail/em-utils.h"
#include "mail/message-list.h"

#include "calendar/gui/calendar-config.h"
#include "calendar/gui/comp-util.h"
#include "calendar/gui/e-comp-editor.h"
#include "calendar/gui/itip-utils.h"

#include "libemail-engine/libemail-engine.h"

#define E_SHELL_WINDOW_ACTION_CONVERT_TO_APPOINTMENT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-convert-to-appointment")
#define E_SHELL_WINDOW_ACTION_CONVERT_TO_MEETING(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-convert-to-meeting")
#define E_SHELL_WINDOW_ACTION_CONVERT_TO_MEMO(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-convert-to-memo")
#define E_SHELL_WINDOW_ACTION_CONVERT_TO_TASK(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-convert-to-task")

gboolean	mail_to_task_mail_browser_init	(EUIManager *ui_manager,
						 EMailBrowser *browser);
gboolean	mail_to_task_mail_shell_view_init
						(EUIManager *ui_manager,
						 EShellView *shell_view);

static ECompEditor *
get_component_editor (EShell *shell,
                      ECalClient *client,
                      ECalComponent *comp,
                      gboolean is_new,
                      GError **error)
{
	ECompEditorFlags flags = 0;
	ECompEditor *comp_editor = NULL;
	ESourceRegistry *registry;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), NULL);

	registry = e_shell_get_registry (shell);

	if (is_new) {
		flags |= E_COMP_EDITOR_FLAG_IS_NEW;
	} else {
		comp_editor = e_comp_editor_find_existing_for (
			e_client_get_source (E_CLIENT (client)),
			e_cal_component_get_icalcomponent (comp));
	}

	if (!comp_editor) {
		if (itip_organizer_is_user (registry, comp, client))
			flags |= E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER;
		if (e_cal_component_has_attendees (comp))
			flags |= E_COMP_EDITOR_FLAG_WITH_ATTENDEES;

		comp_editor = e_comp_editor_open_for_component (NULL,
			shell, e_client_get_source (E_CLIENT (client)),
			e_cal_component_get_icalcomponent (comp), flags);

		if (comp_editor) {
			/* request save for new events */
			e_comp_editor_set_changed (comp_editor, is_new);
		}
	}

	return comp_editor;
}

static void
set_attendees (ECalComponent *comp,
               CamelMimeMessage *message,
               const gchar *organizer)
{
	GSList *attendees = NULL;
	ECalComponentAttendee *ca;
	CamelInternetAddress *from, *to, *cc, *bcc, *arr[4];
	gint len, i, j;

	from = camel_mime_message_get_reply_to (message);
	if (!from)
		from = camel_mime_message_get_from (message);

	to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	bcc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);

	arr[0] = from; arr[1] = to; arr[2] = cc; arr[3] = bcc;

	for (j = 0; j < 4; j++) {
		if (!arr[j])
			continue;

		len = camel_address_length (CAMEL_ADDRESS (arr[j]));
		for (i = 0; i < len; i++) {
			const gchar *name, *addr;

			if (camel_internet_address_get (arr[j], i, &name, &addr)) {
				gchar *temp;

				temp = g_strconcat ("mailto:", addr, NULL);
				if (organizer && g_ascii_strcasecmp (temp, organizer) == 0) {
					/* do not add organizer twice */
					g_free (temp);
					continue;
				}

				ca = e_cal_component_attendee_new ();

				e_cal_component_attendee_set_value (ca, temp);
				e_cal_component_attendee_set_cn (ca, name);
				e_cal_component_attendee_set_cutype (ca, I_CAL_CUTYPE_INDIVIDUAL);
				e_cal_component_attendee_set_partstat (ca, I_CAL_PARTSTAT_NEEDSACTION);
				if (j == 0) {
					/* From */
					e_cal_component_attendee_set_role (ca, I_CAL_ROLE_CHAIR);
				} else if (j == 2) {
					/* BCC  */
					e_cal_component_attendee_set_role (ca, I_CAL_ROLE_OPTPARTICIPANT);
				} else {
					/* all other */
					e_cal_component_attendee_set_role (ca, I_CAL_ROLE_REQPARTICIPANT);
				}

				attendees = g_slist_prepend (attendees, ca);

				g_free (temp);
			}
		}
	}

	attendees = g_slist_reverse (attendees);

	e_cal_component_set_attendees (comp, attendees);

	g_slist_free_full (attendees, e_cal_component_attendee_free);
}

static const gchar *
prepend_from (CamelMimeMessage *message,
              gchar **text)
{
	gchar *res, *tmp, *addr = NULL;
	const gchar *name = NULL, *eml = NULL;
	CamelInternetAddress *from;

	g_return_val_if_fail (message != NULL, NULL);
	g_return_val_if_fail (text != NULL, NULL);

	from = camel_mime_message_get_reply_to (message);
	if (!from)
		from = camel_mime_message_get_from (message);

	if (from && camel_internet_address_get (from, 0, &name, &eml))
		addr = camel_internet_address_format_address (name, eml);

	if (addr && !g_utf8_validate (addr, -1, NULL)) {
		tmp = e_util_utf8_make_valid (addr);
		g_free (addr);
		addr = tmp;
	}

	/* To Translators: The full sentence looks like: "Created from a mail by John Doe <john.doe@myco.example>" */
	tmp = g_strdup_printf (_("Created from a mail by %s"), addr ? addr : "");

	res = g_strconcat (tmp, "\n", *text, NULL);

	g_free (tmp);
	g_free (addr);
	g_free (*text);

	*text = res;

	return res;
}

static void
set_description (ECalComponent *comp,
		 CamelMimeMessage *message,
		 const gchar *default_charset,
		 const gchar *forced_charset)
{
	CamelDataWrapper *content;
	CamelStream *stream;
	CamelContentType *type;
	CamelMimePart *mime_part = CAMEL_MIME_PART (message);
	const gchar *charset = NULL;
	ECalComponentText *text = NULL;
	GByteArray *byte_array;
	GSList *sl = NULL;
	gchar *str, *convert_str = NULL;
	gint count = 2;

	content = camel_medium_get_content ((CamelMedium *) message);
	if (!content)
		return;

	/*
	 * Get non-multipart content from multipart message.
	 */
	while (CAMEL_IS_MULTIPART (content) && count > 0) {
		mime_part = camel_multipart_get_part (CAMEL_MULTIPART (content), 0);
		content = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
		count--;
	}

	if (!mime_part)
		return;

	type = camel_mime_part_get_content_type (mime_part);
	if (!camel_content_type_is (type, "text", "plain") &&
	    !camel_content_type_is (type, "text", "html"))
		return;

	byte_array = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (byte_array);
	camel_data_wrapper_decode_to_stream_sync (content, stream, NULL, NULL);
	str = g_strndup ((gchar *) byte_array->data, byte_array->len);
	g_object_unref (stream);

	if (forced_charset && *forced_charset) {
		charset = forced_charset;
	} else {
		CamelContentType *mime_type;

		mime_type = camel_data_wrapper_get_mime_type_field (content);

		if (mime_type) {
			charset = camel_content_type_param (mime_type, "charset");
			if (charset && !*charset)
				charset = NULL;
		}
	}

	if (!charset && default_charset && *default_charset)
		charset = default_charset;

	/* convert to UTF-8 string */
	if (str && charset) {
		gsize bytes_read, bytes_written;

		convert_str = g_convert (
			str, strlen (str),
			"UTF-8", charset,
			&bytes_read, &bytes_written, NULL);
	}

	if (!convert_str && str)
		convert_str = e_util_utf8_make_valid (str);

	if (camel_content_type_is (type, "text", "html")) {
		gchar *plain_text;

		plain_text = e_markdown_utils_html_to_text (convert_str ? convert_str : str, -1, E_MARKDOWN_HTML_TO_TEXT_FLAG_NONE);

		if (plain_text && *plain_text) {
			g_free (convert_str);
			convert_str = plain_text;
		} else {
			g_free (plain_text);
		}
	}

	if (convert_str)
		text = e_cal_component_text_new (prepend_from (message, &convert_str), NULL);
	else
		text = e_cal_component_text_new (prepend_from (message, &str), NULL);
	sl = g_slist_append (sl, text);

	e_cal_component_set_descriptions (comp, sl);

	g_free (str);
	g_free (convert_str);
	g_slist_free_full (sl, e_cal_component_text_free);
}

static gchar *
set_organizer (ECalComponent *comp,
	       CamelMimeMessage *message,
	       CamelFolder *folder,
	       const gchar *message_uid)
{
	EShell *shell;
	ESource *source = NULL;
	ESourceRegistry *registry;
	ESourceMailIdentity *extension;
	const gchar *extension_name;
	const gchar *address, *name;
	gchar *mailto = NULL;
	gchar *identity_name = NULL, *identity_address = NULL;

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);

	source = em_utils_guess_mail_identity_with_recipients (registry, message, folder,
		message_uid, &identity_name, &identity_address);

	if (!source && folder) {
		CamelStore *store;

		store = camel_folder_get_parent_store (folder);
		source = em_utils_ref_mail_identity_for_store (registry, store);
	}

	if (source == NULL)
		source = e_source_registry_ref_default_mail_identity (registry);

	g_return_val_if_fail (source != NULL, NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	extension = e_source_get_extension (source, extension_name);

	name = identity_name;
	if (!name || !*name)
		name = e_source_mail_identity_get_name (extension);

	address = identity_address;
	if (!address || !*address) {
		name = e_source_mail_identity_get_name (extension);
		address = e_source_mail_identity_get_address (extension);
	}

	if (address && *address) {
		ECalComponentOrganizer *organizer;

		mailto = g_strconcat ("mailto:", address, NULL);

		organizer = e_cal_component_organizer_new ();
		e_cal_component_organizer_set_value (organizer, mailto);
		e_cal_component_organizer_set_cn (organizer, name);
		e_cal_component_set_organizer (comp, organizer);
		e_cal_component_organizer_free (organizer);
	}

	g_object_unref (source);
	g_free (identity_name);
	g_free (identity_address);

	return mailto;
}

struct _att_async_cb_data {
	gchar **uris;
	EFlag *flag;
};

static void
attachment_load_finished (EAttachmentStore *store,
                          GAsyncResult *result,
                          gpointer user_data)
{
	struct _att_async_cb_data *data = user_data;

	/* XXX Should be no need to check for error here.
	 *     This is just to reset state in the EAttachment. */
	e_attachment_store_load_finish (store, result, NULL);

	e_flag_set (data->flag);
}

static void
attachment_save_finished (EAttachmentStore *store,
                          GAsyncResult *result,
                          gpointer user_data)
{
	struct _att_async_cb_data *data = user_data;
	gchar **uris;
	GError *error = NULL;

	uris = e_attachment_store_save_finish (store, result, &error);
	if (error != NULL)
		data->uris = NULL;
	else
		data->uris = uris;

	g_clear_error (&error);

	e_flag_set (data->flag);
}

static void
set_attachments (ECalClient *client,
                 ECalComponent *comp,
                 CamelMimeMessage *message)
{
	/* XXX Much of this is copied from CompEditor::get_attachment_list().
	 *     Perhaps it should be split off as a separate utility? */

	EAttachmentStore *store;
	CamelDataWrapper *content;
	CamelMultipart *multipart;
	GFile *destination;
	GList *attachment_list = NULL;
	GSList *uri_list = NULL;
	const gchar *comp_uid = NULL;
	const gchar *local_store;
	gchar *filename_prefix, *tmp;
	gint ii, n_parts;
	struct _att_async_cb_data cb_data;

	cb_data.flag = e_flag_new ();
	cb_data.uris = NULL;

	content = camel_medium_get_content ((CamelMedium *) message);
	if (!content || !CAMEL_IS_MULTIPART (content))
		return;

	n_parts = camel_multipart_get_number (CAMEL_MULTIPART (content));
	if (n_parts < 1)
		return;

	comp_uid = e_cal_component_get_uid (comp);
	g_return_if_fail (comp_uid != NULL);

	tmp = g_strdup (comp_uid);
	e_util_make_safe_filename (tmp);
	filename_prefix = g_strconcat (tmp, "-", NULL);
	g_free (tmp);

	local_store = e_cal_client_get_local_attachment_store (client);
	destination = g_file_new_for_path (local_store);

	/* Create EAttachments from the MIME parts and add them to the
	 * attachment store. */

	multipart = CAMEL_MULTIPART (content);
	store = E_ATTACHMENT_STORE (e_attachment_store_new ());

	for (ii = 1; ii < n_parts; ii++) {
		EAttachment *attachment;
		CamelMimePart *mime_part;

		attachment = e_attachment_new ();
		mime_part = camel_multipart_get_part (multipart, ii);
		e_attachment_set_mime_part (attachment, mime_part);

		attachment_list = g_list_append (attachment_list, attachment);
	}

	e_flag_clear (cb_data.flag);

	e_attachment_store_load_async (
		store, attachment_list, (GAsyncReadyCallback)
		attachment_load_finished, &cb_data);

	/* Loading should be instantaneous since we already have
	 * the full content, but we need to wait for the callback.
	 */
	e_flag_wait (cb_data.flag);

	g_list_foreach (attachment_list, (GFunc) g_object_unref, NULL);
	g_list_free (attachment_list);

	cb_data.uris = NULL;
	e_flag_clear (cb_data.flag);

	e_attachment_store_save_async (
		store, destination, filename_prefix,
		(GAsyncReadyCallback) attachment_save_finished, &cb_data);

	g_free (filename_prefix);

	/* We can't return until we have results. */
	e_flag_wait (cb_data.flag);

	if (cb_data.uris == NULL) {
		e_flag_free (cb_data.flag);
		return;
	}

	/* Transfer the URI strings to the GSList. */
	for (ii = 0; cb_data.uris[ii] != NULL; ii++) {
		uri_list = g_slist_prepend (uri_list, i_cal_attach_new_from_url (cb_data.uris[ii]));
	}

	e_flag_free (cb_data.flag);
	g_strfreev (cb_data.uris);

	e_cal_component_set_attachments (comp, uri_list);

	g_slist_free_full (uri_list, g_object_unref);
	e_attachment_store_remove_all (store);
	g_object_unref (destination);
	g_object_unref (store);
}

static void
set_priority (ECalComponent *comp,
              CamelMimePart *part)
{
	const gchar *prio;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (part != NULL);

	prio = camel_medium_get_header (CAMEL_MEDIUM (part), "X-Priority");
	if (prio && atoi (prio) > 0)
		e_cal_component_set_priority (comp, 1);
}

struct _report_error
{
	gchar *format;
	gchar *param;
};

static gboolean
do_report_error (struct _report_error *err)
{
	if (err) {
		e_notice (NULL, GTK_MESSAGE_ERROR, err->format, err->param);
		g_free (err->format);
		g_free (err->param);
		g_slice_free (struct _report_error, err);
	}

	return FALSE;
}

static void
report_error_idle (const gchar *format,
                   const gchar *param)
{
	struct _report_error *err = g_slice_new (struct _report_error);

	err->format = g_strdup (format);
	err->param = g_strdup (param);

	g_usleep (250);
	g_idle_add ((GSourceFunc) do_report_error, err);
}

struct _manage_comp
{
	ECalClient *client;
	ECalComponent *comp;
	ICalComponent *stored_comp; /* the one in client already */
	GCond cond;
	GMutex mutex;
	gint mails_count;
	gint mails_done;
	gchar *editor_title;
	gboolean can_continue;
};

static void
free_manage_comp_struct (struct _manage_comp *mc)
{
	g_return_if_fail (mc != NULL);

	g_object_unref (mc->comp);
	g_object_unref (mc->client);
	g_clear_object (&mc->stored_comp);
	g_mutex_clear (&mc->mutex);
	g_cond_clear (&mc->cond);
	g_free (mc->editor_title);

	g_slice_free (struct _manage_comp, mc);
}

static gint
do_ask (const gchar *text,
        gboolean is_create_edit_add)
{
	gint res;
	GtkWidget *dialog = gtk_message_dialog_new (
		NULL,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION,
		is_create_edit_add ? GTK_BUTTONS_NONE : GTK_BUTTONS_YES_NO,
		"%s", text);

	if (is_create_edit_add) {
		gtk_dialog_add_buttons (
			GTK_DIALOG (dialog),
			/* Translators: Dialog button to Cancel edit of an existing event/memo/task */
			C_("mail-to-task", "_Cancel"), GTK_RESPONSE_CANCEL,
			/* Translators: Dialog button to Edit an existing event/memo/task */
			C_("mail-to-task", "_Edit"), GTK_RESPONSE_YES,
			/* Translators: Dialog button to create a New event/memo/task */
			C_("mail-to-task", "_New"), GTK_RESPONSE_NO,
			NULL);
	}

	res = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return res;
}

static const gchar *
get_question_edit_old (ECalClientSourceType source_type)
{
	const gchar *ask = NULL;

	switch (source_type) {
	case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		ask = _("Selected calendar contains event “%s” already. Would you like to edit the old event?");
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
		ask = _("Selected task list contains task “%s” already. Would you like to edit the old task?");
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
		ask = _("Selected memo list contains memo “%s” already. Would you like to edit the old memo?");
		break;
	default:
		g_warn_if_reached ();
		break;
	}

	return ask;
}

static const gchar *
get_question_add_all_mails (ECalClientSourceType source_type,
                            gint count)
{
	const gchar *ask = NULL;

	switch (source_type) {
	case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
		ask = ngettext (
			/* Translators: Note there are always more than 10 mails selected */
			"You have selected %d mails to be converted to events. Do you really want to add them all?",
			"You have selected %d mails to be converted to events. Do you really want to add them all?",
			count);
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
		ask = ngettext (
			/* Translators: Note there are always more than 10 mails selected */
			"You have selected %d mails to be converted to tasks. Do you really want to add them all?",
			"You have selected %d mails to be converted to tasks. Do you really want to add them all?",
			count);
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
		ask = ngettext (
			/* Translators: Note there are always more than 10 mails selected */
			"You have selected %d mails to be converted to memos. Do you really want to add them all?",
			"You have selected %d mails to be converted to memos. Do you really want to add them all?",
			count);
		break;
	default:
		g_warn_if_reached ();
		break;
	}

	return ask;
}

static void
comp_editor_closed (ECompEditor *comp_editor,
                    gboolean saved,
                    struct _manage_comp *mc)
{
	if (!mc)
		return;

	if (!saved && mc->mails_done < mc->mails_count)
		mc->can_continue = (do_ask (_("Do you wish to continue converting remaining mails?"), FALSE) == GTK_RESPONSE_YES);

	/* Signal the do_mail_to_event thread that editor was closed and editor
	 * for next event can be displayed (if any) */
	g_cond_signal (&mc->cond);
}

/*
 * This handler takes title of the editor window and
 * inserts information about number of processed mails and
 * number of all mails to process, so the window title
 * will look like "Appointment (3/10) — An appoitment name"
 */
static void
comp_editor_title_changed (GtkWidget *widget,
                           GParamSpec *pspec,
                           struct _manage_comp *mc)
{
	GtkWindow *editor = GTK_WINDOW (widget);
	const gchar *title = gtk_window_get_title (editor);
	gchar *new_title;
	gchar *splitter;
	gchar *comp_name, *task_name;

	if (!mc)
		return;

	/* Recursion prevence */
	if (mc->editor_title && g_utf8_collate (mc->editor_title, title) == 0)
		return;

	splitter = strstr (title, "—");
	if (!splitter)
		return;

	comp_name = g_strndup (title, splitter - title - 1);
	task_name = g_strdup (splitter + strlen ("—") + 1);
	new_title = g_strdup_printf (
		"%s (%d/%d) — %s",
		comp_name, mc->mails_done, mc->mails_count, task_name);

	/* Remember the new title, so that when gtk_window_set_title() causes
	 * this handler to be recursively called, we can recognize that and
	 * prevent endless recursion */
	g_free (mc->editor_title);
	mc->editor_title = new_title;

	gtk_window_set_title (editor, new_title);

	g_free (comp_name);
	g_free (task_name);
}

static gboolean
do_manage_comp_idle (struct _manage_comp *mc)
{
	GError *error = NULL;
	ECalClientSourceType source_type = E_CAL_CLIENT_SOURCE_TYPE_LAST;
	ECalComponent *edit_comp = NULL;

	g_return_val_if_fail (mc, FALSE);

	source_type = e_cal_client_get_source_type (mc->client);

	if (source_type == E_CAL_CLIENT_SOURCE_TYPE_LAST) {
		free_manage_comp_struct (mc);

		g_warning ("mail-to-task: Incorrect call of %s, no data given", G_STRFUNC);
		return FALSE;
	}

	if (mc->stored_comp) {
		const gchar *ask = get_question_edit_old (source_type);

		if (ask) {
			ICalProperty *prop;
			const gchar *summary;
			gchar *msg;
			gint chosen;

			prop = e_cal_util_component_find_property_for_locale (mc->stored_comp, I_CAL_SUMMARY_PROPERTY, NULL);
			summary = prop ? i_cal_property_get_summary (prop) : NULL;
			msg = g_strdup_printf (ask, summary && *summary ? summary : _("[No Summary]"));
			g_clear_object (&prop);
			chosen = do_ask (msg, TRUE);

			if (chosen == GTK_RESPONSE_YES) {
				edit_comp = e_cal_component_new ();
				if (!e_cal_component_set_icalcomponent (edit_comp, i_cal_component_clone (mc->stored_comp))) {
					g_object_unref (edit_comp);
					edit_comp = NULL;
					error = g_error_new (
						E_CAL_CLIENT_ERROR,
						E_CAL_CLIENT_ERROR_INVALID_OBJECT,
						"%s", _("Invalid object returned from a server"));

				}
			} else if (chosen == GTK_RESPONSE_NO) {
				/* user wants to create a new event, thus generate a new UID */
				gchar *new_uid = e_util_generate_uid ();
				edit_comp = mc->comp;
				e_cal_component_set_uid (edit_comp, new_uid);
				e_cal_component_set_recurid (edit_comp, NULL);
				g_free (new_uid);
			}
			g_free (msg);
		}
	} else {
		edit_comp = mc->comp;
	}

	if (edit_comp) {
		EShell *shell;
		ECompEditor *comp_editor;

		/* FIXME Pass in the EShell instance. */
		shell = e_shell_get_default ();
		comp_editor = get_component_editor (
			shell, mc->client, edit_comp,
			edit_comp == mc->comp, &error);

		if (comp_editor && !error) {
			comp_editor_title_changed (GTK_WIDGET (comp_editor), NULL, mc);

			e_signal_connect_notify (
				comp_editor, "notify::title",
				G_CALLBACK (comp_editor_title_changed), mc);
			g_signal_connect (
				comp_editor, "editor-closed",
				G_CALLBACK (comp_editor_closed), mc);

			gtk_window_present (GTK_WINDOW (comp_editor));

			if (edit_comp != mc->comp)
				g_object_unref (edit_comp);
		} else {
			g_warning ("Failed to create event editor: %s", error ? error->message : "Unknown error");
			g_cond_signal (&mc->cond);
		}
	} else {
		/* User canceled editing already existing event, so
		 * treat it as if he just closed the editor window. */
		comp_editor_closed (NULL, FALSE, mc);
	}

	if (error != NULL) {
		e_notice (
			NULL, GTK_MESSAGE_ERROR,
			_("An error occurred during processing: %s"),
			error->message);
		g_clear_error (&error);
	}

	return FALSE;
}

typedef struct {
	EClientCache *client_cache;
	ESource *source;
	const gchar *extension_name;
	ECalClientSourceType source_type;
	CamelFolder *folder;
	GPtrArray *uids;
	gchar *selected_text;
	gchar *default_charset;
	gchar *forced_charset;
	gboolean with_attendees;
} AsyncData;

static void
async_data_free (AsyncData *data)
{
	if (data) {
		g_free (data->selected_text);
		g_free (data->default_charset);
		g_free (data->forced_charset);
		g_object_unref (data->client_cache);
		g_object_unref (data->source);
		g_slice_free (AsyncData, data);
	}
}

static gboolean
do_mail_to_event (AsyncData *data)
{
	EClient *client;
	CamelFolder *folder = data->folder;
	GPtrArray *uids = data->uids;
	GError *error = NULL;

	client = e_client_cache_get_client_sync (data->client_cache,
		data->source, data->extension_name, E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS, NULL, &error);

	/* Sanity check. */
	g_return_val_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)), TRUE);

	if (error != NULL) {
		report_error_idle (_("Cannot open calendar. %s"), error->message);
	} else if (e_client_is_readonly (E_CLIENT (client))) {
		switch (data->source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			report_error_idle (_("Selected calendar is read only, thus cannot create event there. Select other calendar, please."), NULL);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			report_error_idle (_("Selected task list is read only, thus cannot create task there. Select other task list, please."), NULL);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			report_error_idle (_("Selected memo list is read only, thus cannot create memo there. Select other memo list, please."), NULL);
			break;
		default:
			g_warn_if_reached ();
			break;
		}
	} else {
		GSettings *settings;
		gint i;
		ECalComponentDateTime *dt, *dt2;
		ICalTime *tt, *tt2;
		gchar *tzid = NULL;
		struct _manage_comp *oldmc = NULL;

		#define cache_backend_prop(prop) { \
			gchar *val = NULL; \
			e_client_get_backend_property_sync (E_CLIENT (client), prop, &val, NULL, NULL); \
			g_free (val); \
		}

		/* precache backend properties, thus editor have them ready when needed */
		cache_backend_prop (E_CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS);
		cache_backend_prop (E_CAL_BACKEND_PROPERTY_ALARM_EMAIL_ADDRESS);
		cache_backend_prop (E_CAL_BACKEND_PROPERTY_DEFAULT_OBJECT);
		e_client_get_capabilities (E_CLIENT (client));

		#undef cache_backend_prop

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");

		if (data->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
			ICalTimezone *zone;
			gint time_divisions, shorten_time;

			time_divisions = g_settings_get_int (settings, "time-divisions");
			shorten_time = g_settings_get_int (settings, "shorten-time");
			zone = calendar_config_get_icaltimezone ();

			tt = i_cal_time_new_current_with_zone (zone);
			i_cal_time_adjust (tt, 1, 0, 0, -i_cal_time_get_second (tt));
			if ((i_cal_time_get_minute (tt) % time_divisions) != 0)
				i_cal_time_adjust (tt, 0, 0, time_divisions - (i_cal_time_get_minute (tt) % time_divisions), 0);
			tt2 = i_cal_time_clone (tt);
			i_cal_time_adjust (tt2, 0, 0, time_divisions, 0);

			if (shorten_time > 0 && shorten_time < time_divisions) {
				if (g_settings_get_boolean (settings, "shorten-time-end"))
					i_cal_time_adjust (tt2, 0, 0, -shorten_time, 0);
				else
					i_cal_time_adjust (tt, 0, 0, shorten_time, 0);
			}

			i_cal_time_normalize_inplace (tt);
			i_cal_time_normalize_inplace (tt2);

			if (zone)
				tzid = g_strdup (i_cal_timezone_get_tzid (zone));
		} else {
			/* Memos and Tasks will start "today" */
			tt = i_cal_time_new_today ();
			tt2 = i_cal_time_clone (tt);
			i_cal_time_adjust (tt2, 1, 0, 0, 0);
		}

		dt = e_cal_component_datetime_new_take (tt, g_strdup (tzid));
		dt2 = e_cal_component_datetime_new_take (tt2, tzid);

		for (i = 0; i < (uids ? uids->len : 0); i++) {
			CamelMimeMessage *message;
			ECalComponent *comp;
			ECalComponentText *text;
			ICalProperty *prop;
			ICalComponent *icomp;
			struct _manage_comp *mc;
			const gchar *message_uid = g_ptr_array_index (uids, i);

			/* retrieve the message from the CamelFolder */
			/* FIXME Not passing a GCancellable or GError. */
			message = camel_folder_get_message_sync (folder, message_uid, NULL, NULL);
			if (!message) {
				continue;
			}

			comp = cal_comp_event_new_with_defaults_sync (E_CAL_CLIENT (client), FALSE,
				data->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS &&
				g_settings_get_boolean (settings, "use-default-reminder"),
				g_settings_get_int (settings, "default-reminder-interval"),
				g_settings_get_enum (settings, "default-reminder-units"),
				NULL, &error);

			if (!comp) {
				report_error_idle (_("Cannot create component: %s"), error ? error->message : _("Unknown error"));
				break;
			}

			e_cal_component_set_uid (comp, camel_mime_message_get_message_id (message));
			e_cal_component_set_dtstart (comp, dt);

			if (data->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
				/* make it an all-day event */
				e_cal_component_set_dtend (comp, dt2);
			}

			/* set the summary */
			text = e_cal_component_text_new (camel_mime_message_get_subject (message), NULL);
			e_cal_component_set_summary (comp, text);
			e_cal_component_text_free (text);

			/* set all fields */
			if (data->selected_text) {
				GSList sl;

				text = e_cal_component_text_new (data->selected_text, NULL);

				sl.next = NULL;
				sl.data = text;

				e_cal_component_set_descriptions (comp, &sl);

				e_cal_component_text_free (text);
			} else
				set_description (comp, message, data->default_charset, data->forced_charset);

			if (data->with_attendees) {
				gchar *organizer;

				/* set actual user as organizer, to be able to change event's properties */
				organizer = set_organizer (comp, message, data->folder, message_uid);
				set_attendees (comp, message, organizer);
				g_free (organizer);
			}

			/* set attachment files */
			set_attachments (E_CAL_CLIENT (client), comp, message);

			/* priority */
			set_priority (comp, CAMEL_MIME_PART (message));

			/* no need to increment a sequence number, this is a new component */
			e_cal_component_abort_sequence (comp);

			icomp = e_cal_component_get_icalcomponent (comp);

			prop = i_cal_property_new_x ("1");
			i_cal_property_set_x_name (prop, "X-EVOLUTION-MOVE-CALENDAR");
			i_cal_component_take_property (icomp, prop);

			mc = g_slice_new0 (struct _manage_comp);
			mc->client = E_CAL_CLIENT (g_object_ref (client));
			mc->comp = g_object_ref (comp);
			g_mutex_init (&mc->mutex);
			g_cond_init (&mc->cond);
			mc->mails_count = uids->len;
			mc->mails_done = i + 1; /* Current task */
			mc->editor_title = NULL;
			mc->can_continue = TRUE;

			if (oldmc) {
				/* Wait for user to quit the editor created in previous iteration
				 * before displaying next one */
				gboolean can_continue;
				g_mutex_lock (&oldmc->mutex);
				g_cond_wait (&oldmc->cond, &oldmc->mutex);
				g_mutex_unlock (&oldmc->mutex);
				can_continue = oldmc->can_continue;
				free_manage_comp_struct (oldmc);
				oldmc = NULL;

				if (!can_continue)
					break;
			}

			e_cal_client_get_object_sync (
				E_CAL_CLIENT (client),
				i_cal_component_get_uid (icomp),
				NULL, &mc->stored_comp, NULL, NULL);

			/* Prioritize ahead of GTK+ redraws. */
			g_idle_add_full (
				G_PRIORITY_HIGH_IDLE,
				(GSourceFunc) do_manage_comp_idle, mc, NULL);

			oldmc = mc;

			g_object_unref (comp);
			g_object_unref (message);

		}

		/* Wait for the last editor and then clean up */
		if (oldmc) {
			g_mutex_lock (&oldmc->mutex);
			g_cond_wait (&oldmc->cond, &oldmc->mutex);
			g_mutex_unlock (&oldmc->mutex);
			free_manage_comp_struct (oldmc);
		}

		e_cal_component_datetime_free (dt);
		e_cal_component_datetime_free (dt2);
		g_clear_object (&settings);
	}

	g_clear_object (&client);
	g_ptr_array_unref (uids);
	g_object_unref (folder);

	async_data_free (data);
	g_clear_error (&error);

	return TRUE;
}

static gboolean
text_contains_nonwhitespace (const gchar *text,
                             gint len)
{
	const gchar *p;
	gunichar c = 0;

	if (!text || len <= 0)
		return FALSE;

	p = text;

	while (p && p - text < len) {
		c = g_utf8_get_char (p);
		if (!c)
			break;

		if (!g_unichar_isspace (c))
			break;

		p = g_utf8_next_char (p);
	}

	return p - text < len - 1 && c != 0;
}

static void
get_charsets (EMailReader *reader,
	      gchar **default_charset,
	      gchar **forced_charset)
{
	EMailDisplay *display;
	EMailFormatter *formatter;

	display = e_mail_reader_get_mail_display (reader);
	formatter = e_mail_display_get_formatter (display);

	*default_charset = e_mail_formatter_dup_default_charset (formatter);
	*forced_charset = e_mail_formatter_dup_charset (formatter);
}

static void
start_mail_to_event_thread (AsyncData *data)
{
	GThread *thread = NULL;
	GError *error = NULL;

	thread = g_thread_try_new (NULL, (GThreadFunc) do_mail_to_event, data, &error);

	if (error != NULL) {
		g_warning (G_STRLOC ": %s", error->message);
		g_error_free (error);
		async_data_free (data);
	} else {
		g_thread_unref (thread);
	}
}

static void
mail_to_task_got_selection_jsc_cb (GObject *source_object,
				   GAsyncResult *result,
				   gpointer user_data)
{
	AsyncData *data = user_data;
	GSList *texts = NULL;
	gchar *text;
	GError *error = NULL;

	g_return_if_fail (data != NULL);
	g_return_if_fail (E_IS_WEB_VIEW (source_object));

	if (!e_web_view_jsc_get_selection_finish (WEBKIT_WEB_VIEW (source_object), result, &texts, &error)) {
		texts = NULL;
		g_warning ("%s: Failed to get view selection: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	text = texts ? texts->data : NULL;

	if (text && !text_contains_nonwhitespace (text, strlen (text))) {
		text = NULL;
	} else {
		/* Steal the pointer */
		if (texts)
			texts->data = NULL;
	}

	data->selected_text = text;

	start_mail_to_event_thread (data);

	g_slist_free_full (texts, g_free);
	g_clear_error (&error);
}

static void
mail_to_event (ECalClientSourceType source_type,
               gboolean with_attendees,
               EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	ESourceRegistry *registry;
	GPtrArray *uids;
	ESource *source = NULL;
	ESource *default_source;
	GList *list, *iter;
	GtkWindow *parent;
	const gchar *extension_name;

	parent = e_mail_reader_get_window (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	/* Ask before converting 10 or more mails to events. */
	if (uids->len > 10) {
		gchar *question;
		gint response;

		question = g_strdup_printf (
			get_question_add_all_mails (source_type, uids->len), uids->len);
		response = do_ask (question, FALSE);
		g_free (question);

		if (response == GTK_RESPONSE_NO) {
			g_ptr_array_unref (uids);
			return;
		}
	}

	backend = e_mail_reader_get_backend (reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));
	registry = e_shell_get_registry (shell);

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			default_source = e_source_registry_ref_default_calendar (registry);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			default_source = e_source_registry_ref_default_memo_list (registry);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			default_source = e_source_registry_ref_default_task_list (registry);
			break;
		default:
			g_return_if_reached ();
	}

	list = e_source_registry_list_sources (registry, extension_name);

	/* If there is only one writable source, no need to prompt the user. */
	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		ESource *candidate = E_SOURCE (iter->data);

		if (e_source_get_writable (candidate)) {
			if (source == NULL)
				source = candidate;
			else {
				source = NULL;
				break;
			}
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (source == NULL) {
		GtkWidget *dialog;
		ESourceSelector *selector;

		/* ask the user which tasks list to save to */
		dialog = e_source_selector_dialog_new (
			parent, registry, extension_name);

		selector = e_source_selector_dialog_get_selector (
			E_SOURCE_SELECTOR_DIALOG (dialog));

		e_source_selector_set_primary_selection (
			selector, default_source);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
			source = e_source_selector_dialog_peek_primary_selection (
				E_SOURCE_SELECTOR_DIALOG (dialog));

		gtk_widget_destroy (dialog);
	}

	if (source) {
		/* if a source has been selected, perform the mail2event operation */
		AsyncData *data;
		EMailDisplay *mail_display;

		/* Fill the elements in AsynData */
		data = g_slice_new0 (AsyncData);
		data->client_cache = g_object_ref (e_shell_get_client_cache (shell));
		data->source = g_object_ref (source);
		data->extension_name = extension_name;
		data->source_type = source_type;
		data->folder = e_mail_reader_ref_folder (reader);
		data->uids = g_ptr_array_ref (uids);
		data->with_attendees = with_attendees;
		get_charsets (reader, &data->default_charset, &data->forced_charset);

		mail_display = e_mail_reader_get_mail_display (reader);

		if (uids->len == 1 && e_web_view_has_selection (E_WEB_VIEW (mail_display))) {
			e_web_view_jsc_get_selection (WEBKIT_WEB_VIEW (mail_display), E_TEXT_FORMAT_PLAIN, NULL,
				mail_to_task_got_selection_jsc_cb, data);
		} else {
			data->selected_text = NULL;

			start_mail_to_event_thread (data);
		}
	}

	g_object_unref (default_source);
	g_ptr_array_unref (uids);
}

static void
action_mail_convert_to_event_cb (EUIAction *action,
				 GVariant *parameter,
				 gpointer user_data)
{
	EMailReader *reader = user_data;
	mail_to_event (E_CAL_CLIENT_SOURCE_TYPE_EVENTS, FALSE, reader);
}

static void
action_mail_convert_to_meeting_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	EMailReader *reader = user_data;
	mail_to_event (E_CAL_CLIENT_SOURCE_TYPE_EVENTS, TRUE, reader);
}

static void
action_mail_convert_to_memo_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EMailReader *reader = user_data;
	mail_to_event (E_CAL_CLIENT_SOURCE_TYPE_MEMOS, FALSE, reader);
}

static void
action_mail_convert_to_task_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	EMailReader *reader = user_data;
	mail_to_event (E_CAL_CLIENT_SOURCE_TYPE_TASKS, FALSE, reader);
}

static void
update_actions_cb (EMailReader *reader,
		   guint32 state,
		   EUIManager *ui_manager)
{
	EUIActionGroup *action_group;
	gboolean sensitive;

	sensitive =
		(state & E_MAIL_READER_SELECTION_SINGLE) ||
		(state & E_MAIL_READER_SELECTION_MULTIPLE);

	action_group = e_ui_manager_get_action_group (ui_manager, "mail-convert-any");
	e_ui_action_group_set_sensitive (action_group, sensitive);

	sensitive = (state & E_MAIL_READER_SELECTION_SINGLE);

	action_group = e_ui_manager_get_action_group (ui_manager, "mail-convert-one");
	e_ui_action_group_set_sensitive (action_group, sensitive);
}

static void
setup_actions (EMailReader *reader,
               EUIManager *ui_manager)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<placeholder id='custom-menus'>"
		      "<submenu action='mail-message-menu'>"
			"<submenu action='mail-create-menu'>"
			  "<placeholder id='mail-conversion-actions'>"
			    "<item action='mail-convert-to-appointment'/>"
			    "<item action='mail-convert-to-meeting'/>"
			    "<item action='mail-convert-to-task'/>"
			    "<item action='mail-convert-to-memo'/>"
			  "</placeholder>"
			"</submenu>"
		      "</submenu>"
		    "</placeholder>"
		  "</menu>"
		  "<menu id='mail-message-popup'>"
		    "<submenu action='mail-create-menu'>"
		      "<placeholder id='mail-conversion-actions'>"
			"<item action='mail-convert-to-appointment'/>"
			"<item action='mail-convert-to-meeting'/>"
			"<item action='mail-convert-to-task'/>"
			"<item action='mail-convert-to-memo'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		  "<menu id='mail-preview-popup'>"
		    "<placeholder id='mail-preview-popup-actions'>"
		      "<item action='mail-convert-to-appointment'/>"
		      "<item action='mail-convert-to-meeting'/>"
		      "<item action='mail-convert-to-task'/>"
		      "<item action='mail-convert-to-memo'/>"
		    "</placeholder>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry multi_selection_entries[] = {

		{ "mail-convert-to-appointment",
		  "appointment-new",
		  N_("Create an _Appointment"),
		  NULL,
		  N_("Create a new event from the selected message"),
		  action_mail_convert_to_event_cb, NULL, NULL, NULL },

		{ "mail-convert-to-memo",
		  "stock_insert-note",
		  N_("Create a Mem_o"),
		  NULL,
		  N_("Create a new memo from the selected message"),
		  action_mail_convert_to_memo_cb, NULL, NULL, NULL },

		{ "mail-convert-to-task",
		  "stock_todo",
		  N_("Create a _Task"),
		  NULL,
		  N_("Create a new task from the selected message"),
		  action_mail_convert_to_task_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry single_selection_entries[] = {

		{ "mail-convert-to-meeting",
		  "stock_people",
		  N_("Create a _Meeting"),
		  NULL,
		  N_("Create a new meeting from the selected message"),
		  action_mail_convert_to_meeting_cb, NULL, NULL, NULL }
	};

	e_ui_manager_add_actions (ui_manager, "mail-convert-any", NULL,
		multi_selection_entries, G_N_ELEMENTS (multi_selection_entries), reader);
	e_ui_manager_add_actions_with_eui_data (ui_manager, "mail-convert-one", NULL,
		single_selection_entries, G_N_ELEMENTS (single_selection_entries), reader, eui);

	g_signal_connect_object (
		reader, "update-actions",
		G_CALLBACK (update_actions_cb), ui_manager, 0);
}

gboolean
mail_to_task_mail_browser_init (EUIManager *ui_manager,
				EMailBrowser *browser)
{
	setup_actions (E_MAIL_READER (browser), ui_manager);

	return TRUE;
}

gboolean
mail_to_task_mail_shell_view_init (EUIManager *ui_manager,
				   EShellView *shell_view)
{
	EShellContent *shell_content;
	EMailView *mail_view = NULL;

	shell_content = e_shell_view_get_shell_content (shell_view);
	g_object_get (shell_content, "mail-view", &mail_view, NULL);

	if (mail_view) {
		setup_actions (E_MAIL_READER (mail_view), ui_manager);
		g_clear_object (&mail_view);
	}

	return TRUE;
}
