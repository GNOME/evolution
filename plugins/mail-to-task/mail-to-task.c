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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include <libecal/libecal.h>

#include <shell/e-shell-view.h>
#include <shell/e-shell-window-actions.h>

#include <mail/e-mail-browser.h>
#include <mail/em-utils.h>
#include <mail/message-list.h>

#include <calendar/gui/dialogs/comp-editor.h>
#include <calendar/gui/dialogs/event-editor.h>
#include <calendar/gui/dialogs/memo-editor.h>
#include <calendar/gui/dialogs/task-editor.h>

#define E_SHELL_WINDOW_ACTION_CONVERT_TO_APPOINTMENT(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-convert-to-appointment")
#define E_SHELL_WINDOW_ACTION_CONVERT_TO_MEETING(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-convert-to-meeting")
#define E_SHELL_WINDOW_ACTION_CONVERT_TO_MEMO(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-convert-to-memo")
#define E_SHELL_WINDOW_ACTION_CONVERT_TO_TASK(window) \
	E_SHELL_WINDOW_ACTION ((window), "mail-convert-to-task")

gboolean	mail_browser_init		(GtkUIManager *ui_manager,
						 EMailBrowser *browser);
gboolean	mail_shell_view_init		(GtkUIManager *ui_manager,
						 EShellView *shell_view);

static CompEditor *
get_component_editor (EShell *shell,
                      ECalClient *client,
                      ECalComponent *comp,
                      gboolean is_new,
                      GError **error)
{
	ECalComponentId *id;
	CompEditorFlags flags = 0;
	CompEditor *editor = NULL;
	ESourceRegistry *registry;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), NULL);

	registry = e_shell_get_registry (shell);

	id = e_cal_component_get_id (comp);
	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (id->uid != NULL, NULL);

	if (is_new) {
		flags |= COMP_EDITOR_NEW_ITEM;
	} else {
		editor = comp_editor_find_instance (id->uid);
	}

	if (!editor) {
		if (itip_organizer_is_user (registry, comp, client))
			flags |= COMP_EDITOR_USER_ORG;

		switch (e_cal_component_get_vtype (comp)) {
		case E_CAL_COMPONENT_EVENT:
			if (e_cal_component_has_attendees (comp))
				flags |= COMP_EDITOR_MEETING;

			editor = event_editor_new (client, shell, flags);

			if (flags & COMP_EDITOR_MEETING)
				event_editor_show_meeting (EVENT_EDITOR (editor));
			break;
		case E_CAL_COMPONENT_TODO:
			if (e_cal_component_has_attendees (comp))
				flags |= COMP_EDITOR_IS_ASSIGNED;

			editor = task_editor_new (client, shell, flags);

			if (flags & COMP_EDITOR_IS_ASSIGNED)
				task_editor_show_assignment (TASK_EDITOR (editor));
			break;
		case E_CAL_COMPONENT_JOURNAL:
			if (e_cal_component_has_organizer (comp))
				flags |= COMP_EDITOR_IS_SHARED;

			editor = memo_editor_new (client, shell, flags);
			break;
		default:
			g_warn_if_reached ();
			break;
		}

		if (editor) {
			comp_editor_edit_comp (editor, comp);

			/* request save for new events */
			comp_editor_set_changed (editor, is_new);
		}
	}

	e_cal_component_free_id (id);

	return editor;
}

static void
set_attendees (ECalComponent *comp,
               CamelMimeMessage *message,
               const gchar *organizer)
{
	GSList *attendees = NULL, *to_free = NULL;
	ECalComponentAttendee *ca;
	CamelInternetAddress *from = NULL, *to, *cc, *bcc, *arr[4];
	gint len, i, j;

	if (message->reply_to)
		from = message->reply_to;
	else if (message->from)
		from = message->from;

	to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	bcc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);

	arr[0] = from; arr[1] = to; arr[2] = cc; arr[3] = bcc;

	for (j = 0; j < 4; j++) {
		if (!arr[j])
			continue;

		len = CAMEL_ADDRESS (arr[j])->addresses->len;
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

				ca = g_new0 (ECalComponentAttendee, 1);

				ca->value = temp;
				ca->cn = name;
				ca->cutype = ICAL_CUTYPE_INDIVIDUAL;
				ca->status = ICAL_PARTSTAT_NEEDSACTION;
				if (j == 0) {
					/* From */
					ca->role = ICAL_ROLE_CHAIR;
				} else if (j == 2) {
					/* BCC  */
					ca->role = ICAL_ROLE_OPTPARTICIPANT;
				} else {
					/* all other */
					ca->role = ICAL_ROLE_REQPARTICIPANT;
				}

				to_free = g_slist_prepend (to_free, temp);

				attendees = g_slist_append (attendees, ca);
			}
		}
	}

	e_cal_component_set_attendee_list (comp, attendees);

	g_slist_foreach (attendees, (GFunc) g_free, NULL);
	g_slist_foreach (to_free, (GFunc) g_free, NULL);

	g_slist_free (to_free);
	g_slist_free (attendees);
}

static const gchar *
prepend_from (CamelMimeMessage *message,
              gchar **text)
{
	gchar *res, *tmp, *addr = NULL;
	const gchar *name = NULL, *eml = NULL;
	CamelInternetAddress *from = NULL;

	g_return_val_if_fail (message != NULL, NULL);
	g_return_val_if_fail (text != NULL, NULL);

	if (message->reply_to)
		from = message->reply_to;
	else if (message->from)
		from = message->from;

	if (from && camel_internet_address_get (from, 0, &name, &eml))
		addr = camel_internet_address_format_address (name, eml);

	/* To Translators: The full sentence looks like: "Created from a mail by John Doe <john.doe@myco.example>" */
	tmp = g_strdup_printf (_("Created from a mail by %s"), addr ? addr : "");

	res = g_strconcat (tmp, "\n", *text, NULL);

	g_free (tmp);
	g_free (*text);

	*text = res;

	return res;
}

static void
set_description (ECalComponent *comp,
                 CamelMimeMessage *message)
{
	CamelDataWrapper *content;
	CamelStream *stream;
	CamelContentType *type;
	CamelMimePart *mime_part = CAMEL_MIME_PART (message);
	ECalComponentText *text = NULL;
	GByteArray *byte_array;
	GSList *sl = NULL;
	gchar *str, *convert_str = NULL;
	gsize bytes_read, bytes_written;
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
	if (!camel_content_type_is (type, "text", "plain"))
		return;

	byte_array = g_byte_array_new ();
	stream = camel_stream_mem_new_with_byte_array (byte_array);
	camel_data_wrapper_decode_to_stream_sync (content, stream, NULL, NULL);
	str = g_strndup ((gchar *) byte_array->data, byte_array->len);
	g_object_unref (stream);

	/* convert to UTF-8 string */
	if (str && content->mime_type->params && content->mime_type->params->value) {
		convert_str = g_convert (
			str, strlen (str),
			"UTF-8", content->mime_type->params->value,
			&bytes_read, &bytes_written, NULL);
	}

	text = g_new0 (ECalComponentText, 1);
	if (convert_str)
		text->value = prepend_from (message, &convert_str);
	else
		text->value = prepend_from (message, &str);
	text->altrep = NULL;
	sl = g_slist_append (sl, text);

	e_cal_component_set_description_list (comp, sl);

	g_free (str);
	if (convert_str)
		g_free (convert_str);
	e_cal_component_free_text_list (sl);
}

static gchar *
set_organizer (ECalComponent *comp,
               CamelFolder *folder)
{
	EShell *shell;
	ESource *source = NULL;
	ESourceRegistry *registry;
	ESourceMailIdentity *extension;
	const gchar *extension_name;
	const gchar *address, *name;
	ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};
	gchar *mailto = NULL;

	shell = e_shell_get_default ();
	registry = e_shell_get_registry (shell);

	if (folder != NULL) {
		CamelStore *store;

		store = camel_folder_get_parent_store (folder);
		source = em_utils_ref_mail_identity_for_store (registry, store);
	}

	if (source == NULL)
		source = e_source_registry_ref_default_mail_identity (registry);

	g_return_val_if_fail (source != NULL, NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	extension = e_source_get_extension (source, extension_name);

	name = e_source_mail_identity_get_name (extension);
	address = e_source_mail_identity_get_address (extension);

	if (name != NULL && address != NULL) {
		mailto = g_strconcat ("mailto:", address, NULL);
		organizer.value = mailto;
		organizer.cn = name;
		e_cal_component_set_organizer (comp, &organizer);
	}

	g_object_unref (source);

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

	e_cal_component_get_uid (comp, &comp_uid);
	g_return_if_fail (comp_uid != NULL);

	tmp = g_strdup (comp_uid);
	e_filename_make_safe (tmp);
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
		g_warning ("No attachment URIs retrieved.");
		return;
	}

	/* Transfer the URI strings to the GSList. */
	for (ii = 0; cb_data.uris[ii] != NULL; ii++) {
		uri_list = g_slist_prepend (uri_list, cb_data.uris[ii]);
		cb_data.uris[ii] = NULL;
	}

	e_flag_free (cb_data.flag);
	g_free (cb_data.uris);

	/* XXX Does this take ownership of the list? */
	e_cal_component_set_attachment_list (comp, uri_list);

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

	prio = camel_header_raw_find (& (part->headers), "X-Priority", NULL);
	if (prio && atoi (prio) > 0) {
		gint priority = 1;

		e_cal_component_set_priority (comp, &priority);
	}
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
		g_free (err);
	}

	return FALSE;
}

static void
report_error_idle (const gchar *format,
                   const gchar *param)
{
	struct _report_error *err = g_new (struct _report_error, 1);

	err->format = g_strdup (format);
	err->param = g_strdup (param);

	g_usleep (250);
	g_idle_add ((GSourceFunc) do_report_error, err);
}

struct _manage_comp
{
	ECalClient *client;
	ECalComponent *comp;
	icalcomponent *stored_comp; /* the one in client already */
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
	if (mc->stored_comp)
		icalcomponent_free (mc->stored_comp);
	g_mutex_clear (&mc->mutex);
	g_cond_clear (&mc->cond);
	if (mc->editor_title)
		g_free (mc->editor_title);

	g_free (mc);
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
		ask = _("Selected calendar contains event '%s' already. Would you like to edit the old event?");
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
		ask = _("Selected task list contains task '%s' already. Would you like to edit the old task?");
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
		ask = _("Selected memo list contains memo '%s' already. Would you like to edit the old memo?");
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
		/* Translators: Note there are always more than 10 mails selected */
		ask = ngettext (
			"You have selected %d mails to be converted to events. Do you really want to add them all?",
			"You have selected %d mails to be converted to events. Do you really want to add them all?",
			count);
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
		/* Translators: Note there are always more than 10 mails selected */
		ask = ngettext (
			"You have selected %d mails to be converted to tasks. Do you really want to add them all?",
			"You have selected %d mails to be converted to tasks. Do you really want to add them all?",
			count);
		break;
	case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
		/* Translators: Note there are always more than 10 mails selected */
		ask = ngettext (
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
comp_editor_closed (CompEditor *editor,
                    gboolean accepted,
                    struct _manage_comp *mc)
{
	if (!mc)
		return;

	if (!accepted && mc->mails_done < mc->mails_count)
		mc->can_continue = (do_ask (_("Do you wish to continue converting remaining mails?"), FALSE) == GTK_RESPONSE_YES);

	/* Signal the do_mail_to_event thread that editor was closed and editor
	 * for next event can be displayed (if any) */
	g_cond_signal (&mc->cond);
}

/*
 * This handler takes title of the editor window and
 * inserts information about number of processed mails and
 * number of all mails to process, so the window title
 * will look like "Appointment (3/10) - An appoitment name"
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

	splitter = strchr (title, '-');
	if (!splitter)
		return;

	comp_name = g_strndup (title, splitter - title - 1);
	task_name = g_strdup (splitter + 2);
	new_title = g_strdup_printf (
		"%s (%d/%d) - %s",
		comp_name, mc->mails_done, mc->mails_count, task_name);

	/* Remember the new title, so that when gtk_window_set_title() causes
	 * this handler to be recursively called, we can recognize that and
	 * prevent endless recursion */
	if (mc->editor_title)
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
			gchar *msg = g_strdup_printf (ask, icalcomponent_get_summary (mc->stored_comp) ? icalcomponent_get_summary (mc->stored_comp) : _("[No Summary]"));
			gint chosen;

			chosen = do_ask (msg, TRUE);

			if (chosen == GTK_RESPONSE_YES) {
				edit_comp = e_cal_component_new ();
				if (!e_cal_component_set_icalcomponent (edit_comp, icalcomponent_new_clone (mc->stored_comp))) {
					g_object_unref (edit_comp);
					edit_comp = NULL;
					error = g_error_new (
						E_CAL_CLIENT_ERROR,
						E_CAL_CLIENT_ERROR_INVALID_OBJECT,
						"%s", _("Invalid object returned from a server"));

				}
			} else if (chosen == GTK_RESPONSE_NO) {
				/* user wants to create a new event, thus generate a new UID */
				gchar *new_uid = e_cal_component_gen_uid ();
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
		CompEditor *editor;

		/* FIXME Pass in the EShell instance. */
		shell = e_shell_get_default ();
		editor = get_component_editor (
			shell, mc->client, edit_comp,
			edit_comp == mc->comp, &error);

		if (editor && !error) {
			/* Force editor's title change */
			comp_editor_title_changed (GTK_WIDGET (editor), NULL, mc);

			e_signal_connect_notify (
				editor, "notify::title",
				G_CALLBACK (comp_editor_title_changed), mc);
			g_signal_connect (
				editor, "comp_closed",
				G_CALLBACK (comp_editor_closed), mc);

			gtk_window_present (GTK_WINDOW (editor));

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
	const gchar *selected_text;
	gboolean with_attendees;
}AsyncData;

static gboolean
do_mail_to_event (AsyncData *data)
{
	EClient *client;
	CamelFolder *folder = data->folder;
	GPtrArray *uids = data->uids;
	GError *error = NULL;

	client = e_client_cache_get_client_sync (data->client_cache,
		data->source, data->extension_name, 30, NULL, &error);

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
		gint i;
		ECalComponentDateTime dt, dt2;
		struct icaltimetype tt, tt2;
		struct _manage_comp *oldmc = NULL;

		#define cache_backend_prop(prop) { \
			gchar *val = NULL; \
			e_client_get_backend_property_sync (E_CLIENT (client), prop, &val, NULL, NULL); \
			g_free (val); \
		}

		/* precache backend properties, thus editor have them ready when needed */
		cache_backend_prop (CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS);
		cache_backend_prop (CAL_BACKEND_PROPERTY_ALARM_EMAIL_ADDRESS);
		cache_backend_prop (CAL_BACKEND_PROPERTY_DEFAULT_OBJECT);
		e_client_get_capabilities (E_CLIENT (client));

		#undef cache_backend_prop

		/* set start day of the event as today, without time - easier than looking for a calendar's time zone */
		tt = icaltime_today ();
		dt.value = &tt;
		dt.tzid = NULL;

		tt2 = tt;
		icaltime_adjust (&tt2, 1, 0, 0, 0);
		dt2.value = &tt2;
		dt2.tzid = NULL;

		for (i = 0; i < (uids ? uids->len : 0); i++) {
			CamelMimeMessage *message;
			ECalComponent *comp;
			ECalComponentText text;
			icalproperty *icalprop;
			icalcomponent *icalcomp;
			struct _manage_comp *mc;

			/* retrieve the message from the CamelFolder */
			/* FIXME Not passing a GCancellable or GError. */
			message = camel_folder_get_message_sync (
				folder, g_ptr_array_index (uids, i),
				NULL, NULL);
			if (!message) {
				continue;
			}

			comp = e_cal_component_new ();

			switch (data->source_type) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_TODO);
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_JOURNAL);
				break;
			default:
				g_warn_if_reached ();
				break;
			}

			e_cal_component_set_uid (comp, camel_mime_message_get_message_id (message));
			e_cal_component_set_dtstart (comp, &dt);

			if (data->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
				/* make it an all-day event */
				e_cal_component_set_dtend (comp, &dt2);
			}

			/* set the summary */
			text.value = camel_mime_message_get_subject (message);
			text.altrep = NULL;
			e_cal_component_set_summary (comp, &text);

			/* set all fields */
			if (data->selected_text) {
				GSList sl;

				text.value = data->selected_text;
				text.altrep = NULL;
				sl.next = NULL;
				sl.data = &text;

				e_cal_component_set_description_list (comp, &sl);
			} else
				set_description (comp, message);

			if (data->with_attendees) {
				gchar *organizer;

				/* set actual user as organizer, to be able to change event's properties */
				organizer = set_organizer (comp, data->folder);
				set_attendees (comp, message, organizer);
				g_free (organizer);
			}

			/* set attachment files */
			set_attachments (E_CAL_CLIENT (client), comp, message);

			/* priority */
			set_priority (comp, CAMEL_MIME_PART (message));

			/* no need to increment a sequence number, this is a new component */
			e_cal_component_abort_sequence (comp);

			icalcomp = e_cal_component_get_icalcomponent (comp);

			icalprop = icalproperty_new_x ("1");
			icalproperty_set_x_name (icalprop, "X-EVOLUTION-MOVE-CALENDAR");
			icalcomponent_add_property (icalcomp, icalprop);

			mc = g_new0 (struct _manage_comp, 1);
			mc->client = g_object_ref (client);
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
				icalcomponent_get_uid (icalcomp),
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
	}

	/* free memory */
	if (client != NULL)
		g_object_unref (client);
	g_ptr_array_unref (uids);
	g_object_unref (folder);

	g_object_unref (data->client_cache);
	g_object_unref (data->source);
	g_free (data);
	data = NULL;

	if (error != NULL)
		g_error_free (error);

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

static const gchar *
get_selected_text (EMailReader *reader)
{
	EMailDisplay *display;
	const gchar *text = NULL;

	display = e_mail_reader_get_mail_display (reader);

	if (!e_web_view_is_selection_active (E_WEB_VIEW (display)))
		return NULL;

	text = e_mail_display_get_selection_plain_text_sync (display, NULL, NULL);

	if (text == NULL || !text_contains_nonwhitespace (text, strlen (text)))
		return NULL;

	return text;
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
		AsyncData *data = NULL;
		GThread *thread = NULL;
		GError *error = NULL;

		/* Fill the elements in AsynData */
		data = g_new0 (AsyncData, 1);
		data->client_cache = g_object_ref (e_shell_get_client_cache (shell));
		data->source = g_object_ref (source);
		data->extension_name = extension_name;
		data->source_type = source_type;
		data->folder = e_mail_reader_ref_folder (reader);
		data->uids = g_ptr_array_ref (uids);
		data->with_attendees = with_attendees;

		/* FIXME XXX WK2 move data->selected_text to const gchar * */
		if (uids->len == 1)
			data->selected_text = get_selected_text (reader);
		else
			data->selected_text = NULL;

		thread = g_thread_try_new (
			NULL, (GThreadFunc) do_mail_to_event, data, &error);
		if (error != NULL) {
			g_warning (G_STRLOC ": %s", error->message);
			g_error_free (error);
		} else {
			g_thread_unref (thread);
		}
	}

	g_object_unref (default_source);
	g_ptr_array_unref (uids);
}

static void
action_mail_convert_to_event_cb (GtkAction *action,
                                 EMailReader *reader)
{
	mail_to_event (E_CAL_CLIENT_SOURCE_TYPE_EVENTS, FALSE, reader);
}

static void
action_mail_convert_to_meeting_cb (GtkAction *action,
                                   EMailReader *reader)
{
	mail_to_event (E_CAL_CLIENT_SOURCE_TYPE_EVENTS, TRUE, reader);
}

static void
action_mail_convert_to_memo_cb (GtkAction *action,
                                EMailReader *reader)
{
	mail_to_event (E_CAL_CLIENT_SOURCE_TYPE_MEMOS, FALSE, reader);
}

static void
action_mail_convert_to_task_cb (GtkAction *action,
                                EMailReader *reader)
{
	mail_to_event (E_CAL_CLIENT_SOURCE_TYPE_TASKS, FALSE, reader);
}

/* Note, we're not using EPopupActions here because we update the state
 * of entire actions groups instead of individual actions.  EPopupActions
 * just proxy the state of individual actions. */

static GtkActionEntry multi_selection_entries[] = {

	{ "mail-convert-to-appointment",
	  "appointment-new",
	  N_("Create an _Appointment"),
	  NULL,
	  N_("Create a new event from the selected message"),
	  G_CALLBACK (action_mail_convert_to_event_cb) },

	{ "mail-convert-to-memo",
	  "stock_insert-note",
	  N_("Create a Mem_o"),
	  NULL,
	  N_("Create a new memo from the selected message"),
	  G_CALLBACK (action_mail_convert_to_memo_cb) },

	{ "mail-convert-to-task",
	  "stock_todo",
	  N_("Create a _Task"),
	  NULL,
	  N_("Create a new task from the selected message"),
	  G_CALLBACK (action_mail_convert_to_task_cb) }
};

static GtkActionEntry single_selection_entries[] = {

	{ "mail-convert-to-meeting",
	  "stock_new-meeting",
	  N_("Create a _Meeting"),
	  NULL,
	  N_("Create a new meeting from the selected message"),
	  G_CALLBACK (action_mail_convert_to_meeting_cb) }
};

static void
update_actions_any_cb (EMailReader *reader,
                       guint32 state,
                       GtkActionGroup *action_group)
{
	gboolean sensitive;

	sensitive =
		(state & E_MAIL_READER_SELECTION_SINGLE) ||
		(state & E_MAIL_READER_SELECTION_MULTIPLE);

	gtk_action_group_set_sensitive (action_group, sensitive);
}

static void
update_actions_one_cb (EMailReader *reader,
                       guint32 state,
                       GtkActionGroup *action_group)
{
	gboolean sensitive;

	sensitive = (state & E_MAIL_READER_SELECTION_SINGLE);

	gtk_action_group_set_sensitive (action_group, sensitive);
}

static void
setup_actions (EMailReader *reader,
               GtkUIManager *ui_manager)
{
	GtkActionGroup *action_group;
	const gchar *domain = GETTEXT_PACKAGE;

	action_group = gtk_action_group_new ("mail-convert-any");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, multi_selection_entries,
		G_N_ELEMENTS (multi_selection_entries), reader);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	/* GtkUIManager now owns the action group reference.
	 * The signal we're connecting to will only be emitted
	 * during the GtkUIManager's lifetime, so the action
	 * group will not disappear on us. */

	g_signal_connect (
		reader, "update-actions",
		G_CALLBACK (update_actions_any_cb), action_group);

	action_group = gtk_action_group_new ("mail-convert-one");
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, single_selection_entries,
		G_N_ELEMENTS (single_selection_entries), reader);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	/* GtkUIManager now owns the action group reference.
	 * The signal we're connecting to will only be emitted
	 * during the GtkUIManager's lifetime, so the action
	 * group will not disappear on us. */

	g_signal_connect (
		reader, "update-actions",
		G_CALLBACK (update_actions_one_cb), action_group);
}

gboolean
mail_browser_init (GtkUIManager *ui_manager,
                   EMailBrowser *browser)
{
	setup_actions (E_MAIL_READER (browser), ui_manager);

	return TRUE;
}

gboolean
mail_shell_view_init (GtkUIManager *ui_manager,
                      EShellView *shell_view)
{
	EShellContent *shell_content;

	shell_content = e_shell_view_get_shell_content (shell_view);

	setup_actions (E_MAIL_READER (shell_content), ui_manager);

	return TRUE;
}
