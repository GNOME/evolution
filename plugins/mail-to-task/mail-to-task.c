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

static char *
clean_name(const unsigned char *s)
{
	GString *out = g_string_new("");
	guint32 c;
	char *r;

	while ((c = camel_utf8_getc ((const unsigned char **)&s)))
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
set_attendees (ECalComponent *comp, CamelMimeMessage *message, const char *organizer)
{
	GSList *attendees = NULL, *to_free = NULL;
	ECalComponentAttendee *ca;
	const CamelInternetAddress *from = NULL, *to, *cc, *bcc, *arr[4];
	int len, i, j;

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
			const char *name, *addr;

			if (camel_internet_address_get (arr[j], i, &name, &addr)) {
				char *temp;

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
	char *str, *convert_str = NULL;
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

	str = g_strndup ((const gchar*)((CamelStreamMem *) mem)->buffer->data, ((CamelStreamMem *) mem)->buffer->len);
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

static char *
set_organizer (ECalComponent *comp)
{
	EAccount *account;
	const char *str, *name;
	ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};
	char *res;

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
	int parts, i;
	GSList *list = NULL;
	const char *uid;
	const char *store_uri;
	char *store_dir;
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
		char *filename, *path, *tmp;
		const char *orig_filename;
		CamelMimePart *mime_part;

		mime_part = camel_multipart_get_part (CAMEL_MULTIPART (content), i);

		orig_filename = camel_mime_part_get_filename (mime_part);
		if (!orig_filename)
			continue;

		tmp = clean_name ((const unsigned char *)orig_filename);
		filename = g_strdup_printf ("%s-%s", uid, tmp);
		path = g_build_filename (store_dir, filename, NULL);

		if (em_utils_save_part_to_file (NULL, path, mime_part))
		{
			char *uri;
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
	char *format;
	char *param;
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
report_error_idle (const char *format, const char *param)
{
	struct _report_error *err = g_new (struct _report_error, 1);

	err->format = g_strdup (format);
	err->param = g_strdup (param);

	g_usleep (250);
	g_idle_add ((GSourceFunc)do_report_error, err);
}

typedef struct {
	ECal *client;
	struct _CamelFolder *folder;
	GPtrArray *uids;
	char *selected_text;
	gboolean with_attendees;
}AsyncData;

static gboolean
do_mail_to_event (AsyncData *data)
{
	ECal *client = data->client;
	struct _CamelFolder *folder = data->folder;
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
		int i;
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
				char *organizer;

				/* set actual user as organizer, to be able to change event's properties */
				organizer = set_organizer (comp);
				set_attendees (comp, message, organizer);
				g_free (organizer);
			}

			/* set attachment files */
			set_attachments (client, comp, message);

			icalcomp = e_cal_component_get_icalcomponent (comp);

			icalprop = icalproperty_new_x ("1");
			icalproperty_set_x_name (icalprop, "X-EVOLUTION-MOVE-CALENDAR");
			icalcomponent_add_property (icalcomp, icalprop);

			/* save the task to the selected source */
			if (!e_cal_create_object (client, icalcomp, NULL, &err)) {
				report_error_idle (_("Could not create object. %s"), err ? err->message : _("Unknown error"));

				if (err)
					g_error_free (err);
				err = NULL;
			}

			g_object_unref (comp);
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
copy_uids (char *uid, GPtrArray *uid_array)
{
	g_ptr_array_add (uid_array, g_strdup (uid));
}

static gboolean
text_contains_nonwhitespace (const char *text, gint len)
{
	const char *p;
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
static char *
get_selected_text (EMFolderView *emfv)
{
	char *text = NULL;
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
			char *uri = e_source_get_uri (source);

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

int e_plugin_lib_enable (EPluginLib *ep, int enable);
void org_gnome_mail_to_event (void *ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_event_menu (EPlugin *ep, EMMenuTargetSelect *t);
void org_gnome_mail_to_meeting (void *ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_meeting_menu (EPlugin *ep, EMMenuTargetSelect *t);
void org_gnome_mail_to_task (void *ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_task_menu (EPlugin *ep, EMMenuTargetSelect *t);
void org_gnome_mail_to_memo (void *ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_memo_menu (EPlugin *ep, EMMenuTargetSelect *t);

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	return 0;
}

void
org_gnome_mail_to_event (void *ep, EMPopupTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_EVENT, FALSE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_event_menu (EPlugin *ep, EMMenuTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_EVENT, FALSE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_meeting (void *ep, EMPopupTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_EVENT, TRUE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_meeting_menu (EPlugin *ep, EMMenuTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_EVENT, TRUE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_task (void *ep, EMPopupTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_TODO, TRUE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_task_menu (EPlugin *ep, EMMenuTargetSelect *t)
{
	mail_to_event (E_CAL_SOURCE_TYPE_TODO, TRUE, t->uids, t->folder, (EMFolderView *) t->target.widget);
}

void
org_gnome_mail_to_memo (void *ep, EMPopupTargetSelect *t)
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
