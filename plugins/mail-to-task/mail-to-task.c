
/* Copyright (C) 2004 Novell, Inc */
/* Authors: Michael Zucchi
            Rodrigo Moya */

/* This file is licensed under the GNU GPL v2 or later */

/* Convert a mail message into a task */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>

#include <gconf/gconf-client.h>
#include <libecal/e-cal.h>
#include <libedataserverui/e-source-selector-dialog.h>
#include <camel/camel-folder.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include "mail/em-menu.h"
#include "mail/em-popup.h"

typedef struct {
	ECal *client;
	struct _CamelFolder *folder;
	GPtrArray *uids;
}AsyncData;

static void
set_attendees (ECalComponent *comp, CamelMimeMessage *message)
{
	GSList *attendees = NULL, *l;
	ECalComponentAttendee *ca;
	const CamelInternetAddress *to, *cc, *bcc, *arr[3];
	int len, i, j;

	to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	bcc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);

	arr[0] = to, arr[1] = cc, arr[2] = bcc;

	for(j = 0; j < 3; j++)
	{
		len = CAMEL_ADDRESS (arr[j])->addresses->len;
		for (i = 0; i < len; i++) {
			const char *name, *addr;

			if (camel_internet_address_get (arr[j], i, &name, &addr)) {
				ca = g_new0 (ECalComponentAttendee, 1);
				ca->value = addr;
				ca->cn = name;
				/* FIXME: missing many fields */

				attendees = g_slist_append (attendees, ca);
			}
		}
	}

	e_cal_component_set_attendee_list (comp, attendees);

	for (l = attendees; l != NULL; l = l->next)
		g_free (l->data);

	g_slist_free (attendees);
}

static void
set_description (ECalComponent *comp, CamelMimeMessage *message)
{
	CamelDataWrapper *content;
	CamelStream *mem;
	ECalComponentText text;
	GSList sl;
	char *str, *convert_str = NULL;
	int bytes_read, bytes_written;

	content = camel_medium_get_content_object ((CamelMedium *) message);
	if (!content)
		return;

	mem = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream (content, mem);

	str = g_strndup (((CamelStreamMem *) mem)->buffer->data, ((CamelStreamMem *) mem)->buffer->len);
	camel_object_unref (mem);

	/* convert to UTF-8 string */
	if (str && content->mime_type->params->value)
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

static void
set_organizer (ECalComponent *comp, CamelMimeMessage *message)
{
	const CamelInternetAddress *address;
	const char *str, *name;
	ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};

	if (message->reply_to)
		address = message->reply_to;
	else if (message->from)
		address = message->from;
	else
		return;

	if (!camel_internet_address_get (address, 0, &name, &str))
		return;

	organizer.value = str;
	organizer.cn = name;
	e_cal_component_set_organizer (comp, &organizer);
}

static gboolean 
do_mail_to_task (AsyncData *data)
{
	ECal *client = data->client;
	struct _CamelFolder *folder = data->folder;
	GPtrArray *uids = data->uids;

	/* open the task client */
	if (e_cal_open (client, FALSE, NULL)) {
		int i;
			
		for (i = 0; i < (uids ? uids->len : 0); i++) {
			CamelMimeMessage *message;
			ECalComponent *comp;
			ECalComponentText text;

			/* retrieve the message from the CamelFolder */
			message = camel_folder_get_message (folder, g_ptr_array_index (uids, i), NULL);
			if (!message) {
				continue;
			}

			comp = e_cal_component_new ();
			e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_TODO);
			e_cal_component_set_uid (comp, camel_mime_message_get_message_id (message));

			/* set the task's summary */
			text.value = camel_mime_message_get_subject (message);
			text.altrep = NULL;
			e_cal_component_set_summary (comp, &text);

			/* set all fields */
			set_description (comp, message);
			set_organizer (comp, message);
			set_attendees (comp, message);

			/* save the task to the selected source */
			e_cal_create_object (client, e_cal_component_get_icalcomponent (comp), NULL, NULL);

			g_object_unref (comp);
		}
	}
	
	/* free memory */
	g_object_unref (data->client);
	g_ptr_array_free (data->uids, TRUE);
	g_free (data);
	data = NULL;

	return TRUE;
}

void org_gnome_mail_to_task (void *ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_task_menu (EPlugin *ep, EMMenuTargetSelect *target);

static void
copy_uids (char *uid, GPtrArray *uid_array) 
{
	g_ptr_array_add (uid_array, g_strdup (uid));
}

static void
convert_to_task (GPtrArray *uid_array, struct _CamelFolder *folder)
{
	GtkWidget *dialog;
	GConfClient *conf_client;
	ESourceList *source_list;
	
	/* ask the user which tasks list to save to */
	conf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (conf_client, "/apps/evolution/tasks/sources");

	dialog = e_source_selector_dialog_new (NULL, source_list);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		ESource *source;

		/* if a source has been selected, perform the mail2task operation */
		source = e_source_selector_dialog_peek_primary_selection (E_SOURCE_SELECTOR_DIALOG (dialog));
		if (source) {
			ECal *client = NULL;
			AsyncData *data = NULL;
			GThread *thread = NULL;
			GError *error = NULL;
			
			client = e_cal_new (source, E_CAL_SOURCE_TYPE_TODO);
			if (!client) {
				char *uri = e_source_get_uri (source);
				
				g_warning ("Could not create the client: %s \n", uri);

				g_free (uri);
				g_object_unref (conf_client);
				g_object_unref (source_list);
				gtk_widget_destroy (dialog);
				return;
			}
			
			/* Fill the elements in AsynData */
			data = g_new0 (AsyncData, 1);
			data->client = client;
			data->folder = folder; 
			data->uids = uid_array;

			thread = g_thread_create ((GThreadFunc) do_mail_to_task, data, FALSE, &error);
			if (!thread) {
				g_warning (G_STRLOC ": %s", error->message);
				g_error_free (error);
			}

		}
	}

	g_object_unref (conf_client);
	g_object_unref (source_list);
	gtk_widget_destroy (dialog);
	
}

void
org_gnome_mail_to_task (void *ep, EMPopupTargetSelect *t)
{
	GPtrArray *uid_array = NULL;

	if (t->uids->len > 0) {
		/* FIXME Some how in the thread function the values inside t->uids gets freed 
		   and are corrupted which needs to be fixed, this is sought of work around fix for
		   the gui inresponsiveness */
		uid_array = g_ptr_array_new ();
		g_ptr_array_foreach (t->uids, (GFunc)copy_uids, (gpointer) uid_array);
	} else {
		return;
	}

	convert_to_task (uid_array, t->folder);
}

void org_gnome_mail_to_task_menu (EPlugin *ep, EMMenuTargetSelect *t)
{
	GPtrArray *uid_array = NULL;

	if (t->uids->len > 0) {
		/* FIXME Some how in the thread function the values inside t->uids gets freed 
		   and are corrupted which needs to be fixed, this is sought of work around fix for
		   the gui inresponsiveness */
		uid_array = g_ptr_array_new ();
		g_ptr_array_foreach (t->uids, (GFunc)copy_uids, (gpointer) uid_array);
	} else {
		return;
	}

	convert_to_task (uid_array, t->folder);
}

int e_plugin_lib_enable(EPluginLib *ep, int enable);

int
e_plugin_lib_enable(EPluginLib *ep, int enable)
{
	return 0;
}
