
/* Copyright (C) 2004 Michael Zucchi */

/* This file is licensed under the GNU GPL v2 or later */

/* Add 'copy to clipboard' things to various menu's.

   Uh, so far only to copy mail addresses from mail content */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>

#include <gconf/gconf-client.h>
#include <libecal/e-cal.h>
#include <libedataserverui/e-source-selector-dialog.h>
#include "camel/camel-folder.h"
#include "camel/camel-mime-message.h"
#include "mail/em-popup.h"

static void
add_attendee_cb (gpointer key, gpointer value, gpointer user_data)
{
	ECalComponentAttendee *ca;
	const char *str, *name;
	GList **attendees = user_data;

	if (!camel_internet_address_get (value, 0, &name, &str))
		return;

	ca = g_new0 (ECalComponentAttendee, 1);
	ca->value = str;
	ca->cn = name;
	/* FIXME: missing many fields */

	*attendees = g_slist_append (*attendees, ca);
}

static void
set_attendees (ECalComponent *comp, CamelMimeMessage *message)
{
	GSList *attendees = NULL, *l;

	g_hash_table_foreach (message->recipients, (GHFunc) add_attendee_cb, &attendees);
	e_cal_component_set_attendee_list (comp, attendees);

	for (l = attendees; l != NULL; l = l->next)
		g_free (l->data);
	g_slist_free (attendees);
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

static void
do_mail_to_task (EMPopupTargetSelect *t, ESource *tasks_source)
{
	ECal *client;

	/* open the task client */
	client = e_cal_new (tasks_source, E_CAL_SOURCE_TYPE_TODO);
	if (e_cal_open (client, FALSE, NULL)) {
		int i;

		for (i = 0; i < (t->uids ? t->uids->len : 0); i++) {
			CamelMimeMessage *message;
			ECalComponent *comp;
			ECalComponentText text;
			GSList sl;
			char *str;

			/* retrieve the message from the CamelFolder */
			message = camel_folder_get_message (t->folder, g_ptr_array_index (t->uids, i), NULL);
			if (!message)
				continue;

			comp = e_cal_component_new ();
			e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_TODO);
			e_cal_component_set_uid (comp, camel_mime_message_get_message_id (message));

			/* set the task's summary */
			text.value = camel_mime_message_get_subject (message);
			text.altrep = NULL;
			e_cal_component_set_summary (comp, &text);

			/* FIXME: a better way to get the full body */
			str = camel_mime_message_build_mbox_from (message);
			text.value = str;
			sl.next = NULL;
			sl.data = &text;
			e_cal_component_set_description_list (comp, &sl);

			g_free (str);

			/* set the organizer, and the attendees */
			set_organizer (comp, message);
			set_attendees (comp, message);

			/* save the task to the selected source */
			e_cal_create_object (client, e_cal_component_get_icalcomponent (comp), NULL, NULL);

			g_object_unref (comp);
		}
	}

	/* free memory */
	g_object_unref (client);
}

void org_gnome_mail_to_task (void *ep, EMPopupTargetSelect *t);

void
org_gnome_mail_to_task (void *ep, EMPopupTargetSelect *t)
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
		if (source)
			do_mail_to_task (t, source);
	}

	g_object_unref (conf_client);
	g_object_unref (source_list);
	gtk_widget_destroy (dialog);
}

int e_plugin_lib_enable(EPluginLib *ep, int enable);

int
e_plugin_lib_enable(EPluginLib *ep, int enable)
{
	return 0;
}
