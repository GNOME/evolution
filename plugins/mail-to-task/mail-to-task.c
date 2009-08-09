/*
 *
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
 *		Michael Zucchi <notzed@novell.com>
 *		Rodrigo Moya <rodrigo@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* Convert a mail message into a task */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>

#include <gconf/gconf-client.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-account.h>
#include <libedataserverui/e-source-selector-dialog.h>
#include <camel/camel-folder.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-utf8.h>
#include "mail/em-menu.h"
#include "mail/em-popup.h"
#include "mail/em-utils.h"
#include "mail/em-folder-view.h"
#include "mail/em-format-html.h"
#include "mail/mail-config.h"
#include "e-util/e-dialog-utils.h"
#include <gtkhtml/gtkhtml.h>
#include <calendar/common/authentication.h>
#include <calendar/gui/cal-editor-utils.h>

static gchar *
clean_name(const guchar *s)
{
	GString *out = g_string_new("");
	guint32 c;
	gchar *r;

	while ((c = camel_utf8_getc ((const guchar **)&s)))
	{
		if (!g_unichar_isprint (c) || ( c < 0x7f && strchr (" /'\"`&();|<>$%{}!", c )))
			c = '_';
		g_string_append_u (out, c);
	}

	r = g_strdup (out->str);
	g_string_free (out, TRUE);

	return r;
}

static void
set_attendees (ECalComponent *comp, CamelMimeMessage *message, const gchar *organizer)
{
	GSList *attendees = NULL, *to_free = NULL;
	ECalComponentAttendee *ca;
	const CamelInternetAddress *from = NULL, *to, *cc, *bcc, *arr[4];
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

static void
set_description (ECalComponent *comp, CamelMimeMessage *message)
{
	CamelDataWrapper *content;
	CamelStream *mem;
	CamelContentType *type;
	CamelMimePart *mime_part = CAMEL_MIME_PART (message);
	ECalComponentText text;
	GSList sl;
	gchar *str, *convert_str = NULL;
	gsize bytes_read, bytes_written;
	gint count = 2;

	content = camel_medium_get_content_object ((CamelMedium *) message);
	if (!content)
		return;

	/*
	 * Get non-multipart content from multipart message.
	 */
	while (CAMEL_IS_MULTIPART (content) && count > 0)
	{
		mime_part = camel_multipart_get_part (CAMEL_MULTIPART (content), 0);
		content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
		count--;
	}

	if (!mime_part)
		return;

	type = camel_mime_part_get_content_type (mime_part);
	if (!camel_content_type_is (type, "text", "plain"))
		return;

	mem = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream (content, mem);

	str = g_strndup ((const gchar *)((CamelStreamMem *) mem)->buffer->data, ((CamelStreamMem *) mem)->buffer->len);
	camel_object_unref (mem);

	/* convert to UTF-8 string */
	if (str && content->mime_type->params && content->mime_type->params->value)
	{
		convert_str = g_convert (str, strlen (str),
					 "UTF-8", content->mime_type->params->value,
					 &bytes_read, &bytes_written, NULL);
	}

	if (convert_str)
		text.value = convert_str;
	else
		text.value = str;
	text.altrep = NULL;
	sl.next = NULL;
	sl.data = &text;

	e_cal_component_set_description_list (comp, &sl);

	g_free (str);
	if (convert_str)
		g_free (convert_str);
}

static gchar *
set_organizer (ECalComponent *comp)
{
	EAccount *account;
	const gchar *str, *name;
	ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};
	gchar *res;

	account = mail_config_get_default_account ();
	if (!account)
		return NULL;

	str = e_account_get_string (account, E_ACCOUNT_ID_ADDRESS);
	name = e_account_get_string (account, E_ACCOUNT_ID_NAME);

	if (!str)
		return NULL;

	res = g_strconcat ("mailto:", str, NULL);
	organizer.value = res;
	organizer.cn = name;
	e_cal_component_set_organizer (comp, &organizer);

	return res;
}

static void
set_attachments (ECal *client, ECalComponent *comp, CamelMimeMessage *message)
{
	gint parts, i;
	GSList *list = NULL;
	const gchar *uid;
	const gchar *store_uri;
	gchar *store_dir;
	CamelDataWrapper *content;

	content = camel_medium_get_content_object ((CamelMedium *) message);
	if (!content || !CAMEL_IS_MULTIPART (content))
		return;

	parts = camel_multipart_get_number (CAMEL_MULTIPART (content));
	if (parts < 1)
		return;

	e_cal_component_get_uid (comp, &uid);
	store_uri = e_cal_get_local_attachment_store (client);
	if (!store_uri)
		return;
	store_dir = g_filename_from_uri (store_uri, NULL, NULL);

	for (i = 1; i < parts; i++)
	{
		gchar *filename, *path, *tmp;
		const gchar *orig_filename;
		CamelMimePart *mime_part;

		mime_part = camel_multipart_get_part (CAMEL_MULTIPART (content), i);

		orig_filename = camel_mime_part_get_filename (mime_part);
		if (!orig_filename)
			continue;

		tmp = clean_name ((const guchar *)orig_filename);
		filename = g_strdup_printf ("%s-%s", uid, tmp);
		path = g_build_filename (store_dir, filename, NULL);

		if (em_utils_save_part_to_file (NULL, path, mime_part))
		{
			gchar *uri;
			uri = g_filename_to_uri (path, NULL, NULL);
			list = g_slist_append (list, g_strdup (uri));
			g_free (uri);
		}

		g_free (tmp);
		g_free (filename);
		g_free (path);
	}

	g_free (store_dir);

	e_cal_component_set_attachment_list (comp, list);
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
report_error_idle (const gchar *format, const gchar *param)
{
	struct _report_error *err = g_new (struct _report_error, 1);

	err->format = g_strdup (format);
	err->param = g_strdup (param);

	g_usleep (250);
	g_idle_add ((GSourceFunc)do_report_error, err);
}

struct _manage_comp
{
	ECal *client;
	ECalComponent *comp;
	icalcomponent *stored_comp; /* the one in client already */
};

static void
free_manage_comp_struct (struct _manage_comp *mc)
{
	g_return_if_fail (mc != NULL);

	g_object_unref (mc->comp);
	g_object_unref (mc->client);
	if (mc->stored_comp)
		icalcomponent_free (mc->stored_comp);
	g_free (mc);
}

static gint
do_ask (const gchar *text, gboolean is_create_edit_add)
{
	gint res;
	GtkWidget *dialog = gtk_message_dialog_new (NULL,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION,
		is_create_edit_add ? GTK_BUTTONS_NONE : GTK_BUTTONS_YES_NO,
		"%s", text);

	if (is_create_edit_add) {
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_EDIT, GTK_RESPONSE_YES,
			GTK_STOCK_NEW, GTK_RESPONSE_NO,
			NULL);
	}

	res = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return res;
}

static const gchar *
get_question_edit_old (ECalSourceType source_type)
{
	const gchar *ask = NULL;

	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		ask = _("Selected calendar contains event '%s' already. Would you like to edit the old event?");
		break;
	case E_CAL_SOURCE_TYPE_TODO:
		ask = _("Selected task list contains task '%s' already. Would you like to edit the old task?");
		break;
	case E_CAL_SOURCE_TYPE_JOURNAL:
		ask = _("Selected memo list contains memo '%s' already. Would you like to edit the old memo?");
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return ask;
}

static const gchar *
get_question_create_new (ECalSourceType source_type)
{
	const gchar *ask = NULL;

	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		ask = _("Selected calendar contains some events for the given mails already. Would you like to create new events anyway?");
		break;
	case E_CAL_SOURCE_TYPE_TODO:
		ask = _("Selected task list contains some tasks for the given mails already. Would you like to create new tasks anyway?");
		break;
	case E_CAL_SOURCE_TYPE_JOURNAL:
		ask = _("Selected memo list contains some memos for the given mails already. Would you like to create new memos anyway?");
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return ask;
}

static const gchar *
get_question_create_new_n (ECalSourceType source_type, gint count)
{
	const gchar *ask = NULL;

	switch (source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		ask = ngettext (
			"Selected calendar contains an event for the given mail already. Would you like to create new event anyway?",
			"Selected calendar contains events for the given mails already. Would you like to create new events anyway?",
			count);
		break;
	case E_CAL_SOURCE_TYPE_TODO:
		ask = ngettext (
			"Selected task list contains a task for the given mail already. Would you like to create new task anyway?",
			"Selected task list contains tasks for the given mails already. Would you like to create new tasks anyway?",
			count);
		break;
	case E_CAL_SOURCE_TYPE_JOURNAL:
		ask = ngettext (
			"Selected memo list contains a memo for the given mail already. Would you like to create new memo anyway?",
			"Selected memo list contains memos for the given mails already. Would you like to create new memos anyway?",
			count);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return ask;
}

static gboolean
do_manage_comp_idle (GSList *manage_comp_datas)
{
	GError *error = NULL;
	guint with_old = 0;
	gboolean need_editor = FALSE;
	ECalSourceType source_type = E_CAL_SOURCE_TYPE_LAST;
	GSList *l;

	g_return_val_if_fail (manage_comp_datas != NULL, FALSE);

	if (manage_comp_datas->data) {
		struct _manage_comp *mc = manage_comp_datas->data;

		if (mc->comp && (e_cal_component_has_attendees (mc->comp) || e_cal_component_has_organizer (mc->comp)))
			need_editor = TRUE;

		source_type = e_cal_get_source_type (mc->client);
	}

	if (source_type == E_CAL_SOURCE_TYPE_LAST) {
		g_slist_foreach (manage_comp_datas, (GFunc) free_manage_comp_struct, NULL);
		g_slist_free (manage_comp_datas);

		g_warning ("mail-to-task: Incorrect call of %s, no data given", G_STRFUNC);
		return FALSE;
	}

	for (l = manage_comp_datas; l; l = l->next) {
		struct _manage_comp *mc = l->data;

		if (mc && mc->stored_comp)
			with_old++;
	}

	if (need_editor) {
		for (l = manage_comp_datas; l && !error; l = l->next) {
			ECalComponent *edit_comp = NULL;
			struct _manage_comp *mc = l->data;

			if (!mc)
				continue;

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

							error = g_error_new (E_CALENDAR_ERROR, E_CALENDAR_STATUS_INVALID_OBJECT, "%s", _("Invalid object returned from a server"));
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
				open_component_editor (mc->client, edit_comp, edit_comp == mc->comp, &error);
				if (edit_comp != mc->comp)
					g_object_unref (edit_comp);
			}
		}
	} else {
		gboolean can = TRUE;

		if (with_old > 0) {
			const gchar *ask = NULL;

			can = FALSE;

			if (with_old == g_slist_length (manage_comp_datas)) {
				ask = get_question_create_new_n (source_type, with_old);
			} else {
				ask = get_question_create_new (source_type);
			}

			if (ask)
				can = do_ask (ask, FALSE) == GTK_RESPONSE_YES;
		}

		if (can) {
			for (l = manage_comp_datas; l && !error; l = l->next) {
				struct _manage_comp *mc = l->data;

				if (!mc)
					continue;

				if (mc->stored_comp) {
					gchar *new_uid = e_cal_component_gen_uid ();

					e_cal_component_set_uid (mc->comp, new_uid);
					e_cal_component_set_recurid (mc->comp, NULL);

					g_free (new_uid);
				}

				e_cal_create_object (mc->client, e_cal_component_get_icalcomponent (mc->comp), NULL, &error);
			}
		}
	}

	if (error) {
		e_notice (NULL, GTK_MESSAGE_ERROR, _("An error occurred during processing: %s"), error->message);
		g_error_free (error);
	}

	g_slist_foreach (manage_comp_datas, (GFunc) free_manage_comp_struct, NULL);
	g_slist_free (manage_comp_datas);

	return FALSE;
}

typedef struct {
	ECal *client;
	CamelFolder *folder;
	GPtrArray *uids;
	gchar *selected_text;
	gboolean with_attendees;
}AsyncData;

static gboolean
do_mail_to_event (AsyncData *data)
{
	ECal *client = data->client;
	CamelFolder *folder = data->folder;
	GPtrArray *uids = data->uids;
	GError *err = NULL;
	gboolean readonly = FALSE;

	/* open the task client */
	if (!e_cal_open (client, FALSE, &err)) {
		report_error_idle (_("Cannot open calendar. %s"), err ? err->message : _("Unknown error."));
	} else if (!e_cal_is_read_only (client, &readonly, &err) || readonly) {
		if (err)
			report_error_idle ("Check readonly failed. %s", err->message);
		else {
			switch (e_cal_get_source_type (client)) {
			case E_CAL_SOURCE_TYPE_EVENT:
				report_error_idle (_("Selected source is read only, thus cannot create event there. Select other source, please."), NULL);
				break;
			case E_CAL_SOURCE_TYPE_TODO:
				report_error_idle (_("Selected source is read only, thus cannot create task there. Select other source, please."), NULL);
				break;
			case E_CAL_SOURCE_TYPE_JOURNAL:
				report_error_idle (_("Selected source is read only, thus cannot create memo there. Select other source, please."), NULL);
				break;
			default:
				g_assert_not_reached ();
				break;
			}
		}
	} else {
		GSList *mcs = NULL;
		gint i;
		ECalSourceType source_type = e_cal_get_source_type (client);
		ECalComponentDateTime dt, dt2;
		struct icaltimetype tt, tt2;

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
			message = camel_folder_get_message (folder, g_ptr_array_index (uids, i), NULL);
			if (!message) {
				continue;
			}

			comp = e_cal_component_new ();

			switch (source_type) {
			case E_CAL_SOURCE_TYPE_EVENT:
				e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_EVENT);
				break;
			case E_CAL_SOURCE_TYPE_TODO:
				e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_TODO);
				break;
			case E_CAL_SOURCE_TYPE_JOURNAL:
				e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_JOURNAL);
				break;
			default:
				g_assert_not_reached ();
				break;
			}

			e_cal_component_set_uid (comp, camel_mime_message_get_message_id (message));
			e_cal_component_set_dtstart (comp, &dt);

			if (source_type == E_CAL_SOURCE_TYPE_EVENT) {
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
				organizer = set_organizer (comp);
				set_attendees (comp, message, organizer);
				g_free (organizer);
			}

			/* set attachment files */
			set_attachments (client, comp, message);

			/* no need to increment a sequence number, this is a new component */
			e_cal_component_abort_sequence (comp);

			icalcomp = e_cal_component_get_icalcomponent (comp);

			icalprop = icalproperty_new_x ("1");
			icalproperty_set_x_name (icalprop, "X-EVOLUTION-MOVE-CALENDAR");
			icalcomponent_add_property (icalcomp, icalprop);

			mc = g_new0 (struct _manage_comp, 1);
			mc->client = g_object_ref (client);
			mc->comp = g_object_ref (comp);

			if (!e_cal_get_object (client, icalcomponent_get_uid (icalcomp), NULL, &(mc->stored_comp), NULL))
				mc->stored_comp = NULL;

			mcs = g_slist_append (mcs, mc);

			g_object_unref (comp);
		}

		if (mcs) {
			/* process this in the main thread, as we may ask user too */
			g_idle_add ((GSourceFunc)do_manage_comp_idle, mcs);
		}
	}

	/* free memory */
	g_object_unref (data->client);
	g_ptr_array_free (data->uids, TRUE);
	g_free (data->selected_text);
	g_free (data);
	data = NULL;

	if (err)
		g_error_free (err);

	return TRUE;
}

static void
copy_uids (gchar *uid, GPtrArray *uid_array)
{
	g_ptr_array_add (uid_array, g_strdup (uid));
}

static gboolean
text_contains_nonwhitespace (const gchar *text, gint len)
{
	const gchar *p;
	gunichar c = 0;

	if (!text || len<=0)
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

/* should be freed with g_free after done with it */
static gchar *
get_selected_text (EMFolderView *emfv)
{
	gchar *text = NULL;
	gint len;

	if (!emfv || !emfv->preview || !gtk_html_command (((EMFormatHTML *)emfv->preview)->html, "is-selection-active"))
		return NULL;

	if (gtk_html_command (((EMFormatHTML *)emfv->preview)->html, "is-selection-active")
	    && (text = gtk_html_get_selection_plain_text (((EMFormatHTML *)emfv->preview)->html, &len))
	    && len && text && text[0] && text_contains_nonwhitespace (text, len)) {
		/* selection is ok, so use it as returned from gtkhtml widget */
	} else {
		g_free (text);
		text = NULL;
	}

	return text;
}

static void
mail_to_event (ECalSourceType source_type, gboolean with_attendees, GPtrArray *uids, CamelFolder *folder, EMFolderView *emfv)
{
	GPtrArray *uid_array = NULL;
	ESourceList *source_list = NULL;
	gboolean done = FALSE;
	GSList *groups, *p;
	ESource *source = NULL;
	GError *error = NULL;

	g_return_if_fail (uids != NULL);
	g_return_if_fail (folder != NULL);
	g_return_if_fail (emfv != NULL);

	if (uids->len > 0) {
		uid_array = g_ptr_array_new ();
		g_ptr_array_foreach (uids, (GFunc)copy_uids, (gpointer) uid_array);
	} else {
		/* nothing selected */
		return;
	}

	if (!e_cal_get_sources (&source_list, source_type, &error)) {
		e_notice (NULL, GTK_MESSAGE_ERROR, _("Cannot get source list. %s"), error ? error->message : _("Unknown error."));

		if (error)
			g_error_free (error);

		return;
	}

	/* Check if there is only one writeable source, if so do not ask user to pick it */
	groups = e_source_list_peek_groups (source_list);
	for (p = groups; p != NULL && !done; p = p->next) {
		ESourceGroup *group = E_SOURCE_GROUP (p->data);
		GSList *sources, *q;

		sources = e_source_group_peek_sources (group);
		for (q = sources; q != NULL; q = q->next) {
			ESource *s = E_SOURCE (q->data);

			if (s && !e_source_get_readonly (s)) {
				if (source) {
					source = NULL;
					done = TRUE;
					break;
				}

				source = s;
			}
		}
	}

	if (!source) {
		GtkWidget *dialog;

		/* ask the user which tasks list to save to */
		dialog = e_source_selector_dialog_new (NULL, source_list);

		e_source_selector_dialog_select_default_source (E_SOURCE_SELECTOR_DIALOG (dialog));

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
			source = e_source_selector_dialog_peek_primary_selection (E_SOURCE_SELECTOR_DIALOG (dialog));

		gtk_widget_destroy (dialog);
	}

	if (source) {
		/* if a source has been selected, perform the mail2event operation */
		ECal *client = NULL;
		AsyncData *data = NULL;
		GThread *thread = NULL;

		client = auth_new_cal_from_source (source, source_type);
		if (!client) {
			gchar *uri = e_source_get_uri (source);

			e_notice (NULL, GTK_MESSAGE_ERROR, "Could not create the client: %s", uri);

			g_free (uri);
			g_object_unref (source_list);
			return;
		}

		/* Fill the elements in AsynData */
		data = g_new0 (AsyncData, 1);
		data->client = client;
		data->folder = folder;
		data->uids = uid_array;
		data->with_attendees = with_attendees;

		if (uid_array->len == 1)
			data->selected_text = get_selected_text (emfv);
		else
			data->selected_text = NULL;

		thread = g_thread_create ((GThreadFunc) do_mail_to_event, data, FALSE, &error);
		if (!thread) {
			g_warning (G_STRLOC ": %s", error->message);
			g_error_free (error);
		}
	}

	g_object_unref (source_list);
}

/* ************************************************************************* */

gint e_plugin_lib_enable (EPluginLib *ep, gint enable);
void org_gnome_mail_to_event (gpointer ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_event_menu (EPlugin *ep, EMMenuTargetSelect *t);
void org_gnome_mail_to_meeting (gpointer ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_meeting_menu (EPlugin *ep, EMMenuTargetSelect *t);
void org_gnome_mail_to_task (gpointer ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_task_menu (EPlugin *ep, EMMenuTargetSelect *t);
void org_gnome_mail_to_memo (gpointer ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_memo_menu (EPlugin *ep, EMMenuTargetSelect *t);

gint
e_plugin_lib_enable (EPluginLib *ep, gint enable)
{
	return 0;
}

void
org_gnome_mail_to_event (gpointer ep, EMPopupTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_EVENT, FALSE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_event_menu (EPlugin *ep, EMMenuTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_EVENT, FALSE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_meeting (gpointer ep, EMPopupTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_EVENT, TRUE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_meeting_menu (EPlugin *ep, EMMenuTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_EVENT, TRUE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_task (gpointer ep, EMPopupTargetSelect *t)
{
	/* do not create assigned tasks */
	mail_to_event (E_CAL_SOURCE_TYPE_TODO, FALSE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_task_menu (EPlugin *ep, EMMenuTargetSelect *t)
{
	/* do not create assigned tasks */
	mail_to_event (E_CAL_SOURCE_TYPE_TODO, FALSE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_memo (gpointer ep, EMPopupTargetSelect *t)
{
	/* do not set organizer and attendees for memos */
	mail_to_event (E_CAL_SOURCE_TYPE_JOURNAL, FALSE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_memo_menu (EPlugin *ep, EMMenuTargetSelect *t)
{
	/* do not set organizer and attendees for memos */
	mail_to_event (E_CAL_SOURCE_TYPE_JOURNAL, FALSE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}
