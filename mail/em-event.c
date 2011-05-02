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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "em-event.h"
#include "composer/e-msg-composer.h"

static EMEvent *em_event;

G_DEFINE_TYPE (EMEvent, em_event, E_TYPE_EVENT)

static void
eme_target_free (EEvent *ep, EEventTarget *t)
{
	switch (t->type) {
	case EM_EVENT_TARGET_FOLDER: {
		EMEventTargetFolder *s = (EMEventTargetFolder *)t;
		g_free (s->name);
		g_free (s->uri);
		g_free (s->msg_uid);
		g_free (s->msg_sender);
		g_free (s->msg_subject);
		break; }
	case EM_EVENT_TARGET_MESSAGE: {
		EMEventTargetMessage *s = (EMEventTargetMessage *)t;

		if (s->folder)
			g_object_unref (s->folder);
		if (s->message)
			g_object_unref (s->message);
		g_free (s->uid);
		if (s->composer)
			g_object_unref (s->composer);
		break; }
	case EM_EVENT_TARGET_COMPOSER : {
		EMEventTargetComposer *s = (EMEventTargetComposer *)t;

		if (s->composer)
			g_object_unref (s->composer);
		break; }
	}

	/* Chain up to parent's target_free() method. */
	E_EVENT_CLASS (em_event_parent_class)->target_free (ep, t);
}

static void
em_event_class_init (EMEventClass *class)
{
	EEventClass *event_class;

	event_class = E_EVENT_CLASS (class);
	event_class->target_free = eme_target_free;
}

static void
em_event_init (EMEvent *event)
{
}

/**
 * em_event_peek:
 * @void:
 *
 * Get the singular instance of the mail event handler.
 *
 * Return value:
 **/
EMEvent *
em_event_peek (void)
{
	if (em_event == NULL) {
		em_event = g_object_new (em_event_get_type (), NULL);
		e_event_construct (
			&em_event->popup,
			"org.gnome.evolution.mail.events");
	}

	return em_event;
}

EMEventTargetFolder *
em_event_target_new_folder (EMEvent *eme,
                            const gchar *uri,
                            guint new,
                            const gchar *msg_uid,
                            const gchar *msg_sender,
                            const gchar *msg_subject)
{
	EMEventTargetFolder *t;
	guint32 flags = new ? EM_EVENT_FOLDER_NEWMAIL : 0;

	t = e_event_target_new (
		&eme->popup, EM_EVENT_TARGET_FOLDER, sizeof (*t));

	t->uri = g_strdup (uri);
	t->target.mask = ~flags;
	t->new = new;
	t->msg_uid = g_strdup (msg_uid);
	t->msg_sender = g_strdup (msg_sender);
	t->msg_subject = g_strdup (msg_subject);

	return t;
}

EMEventTargetComposer *
em_event_target_new_composer (EMEvent *eme,
                              EMsgComposer *composer,
                              guint32 flags)
{
	EMEventTargetComposer *t;

	t = e_event_target_new (
		&eme->popup, EM_EVENT_TARGET_COMPOSER, sizeof (*t));

	t->composer = g_object_ref (composer);
	t->target.mask = ~flags;

	return t;
}

EMEventTargetMessage *
em_event_target_new_message (EMEvent *eme,
                             CamelFolder *folder,
                             CamelMimeMessage *message,
                             const gchar *uid,
                             guint32 flags,
                             EMsgComposer *composer)
{
	EMEventTargetMessage *t;

	t = e_event_target_new (
		&eme->popup, EM_EVENT_TARGET_MESSAGE, sizeof (*t));

	t->uid = g_strdup (uid);
	t->folder = folder;
	if (folder)
		g_object_ref (folder);
	t->message = message;
	if (message)
		g_object_ref (message);
	t->target.mask = ~flags;
	if (composer)
		t->composer = g_object_ref (G_OBJECT (composer));

	return t;
}

EMEventTargetSendReceive *
em_event_target_new_send_receive (EMEvent *eme,
                                  GtkWidget *table,
                                  gpointer data,
                                  gint row,
                                  guint32 flags)
{
	EMEventTargetSendReceive *t;

	t = e_event_target_new (
		&eme->popup, EM_EVENT_TARGET_SEND_RECEIVE, sizeof (*t));

	t->table = table;
	t->data = data;
	t->row = row;
	t->target.mask = ~flags;

	return t;
}

EMEventTargetCustomIcon *
em_event_target_new_custom_icon (EMEvent *eme,
                                 GtkTreeStore *store,
                                 GtkTreeIter *iter,
                                 const gchar *folder_name,
                                 guint32 flags)
{
	EMEventTargetCustomIcon *t;

	t = e_event_target_new (
		&eme->popup, EM_EVENT_TARGET_CUSTOM_ICON, sizeof (*t));

	t->store = store;
	t->iter = iter;
	t->folder_name = folder_name;
	t->target.mask = ~flags;

	return t;
}
