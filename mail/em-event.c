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
#include "libedataserver/e-msgport.h"

#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-mime-utils.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-url.h>

#include <camel/camel-vee-folder.h>
#include <camel/camel-vtrash-folder.h>

static GObjectClass *eme_parent;
static EMEvent *em_event;

static void
eme_init(GObject *o)
{
	/*EMEvent *eme = (EMEvent *)o; */
}

static void
eme_finalise(GObject *o)
{
	((GObjectClass *)eme_parent)->finalize(o);
}

static void
eme_target_free(EEvent *ep, EEventTarget *t)
{
	switch (t->type) {
	case EM_EVENT_TARGET_FOLDER: {
		EMEventTargetFolder *s = (EMEventTargetFolder *)t;
		g_free (s->name);
		g_free(s->uri);
		break; }
	case EM_EVENT_TARGET_MESSAGE: {
		EMEventTargetMessage *s = (EMEventTargetMessage *)t;

		if (s->folder)
			camel_object_unref(s->folder);
		if (s->message)
			camel_object_unref(s->message);
		g_free(s->uid);
		if (s->composer)
			g_object_unref (s->composer);
		break; }
	case EM_EVENT_TARGET_COMPOSER : {
		EMEventTargetComposer *s = (EMEventTargetComposer *)t;

		if (s->composer)
			g_object_unref (s->composer);
		break; }
	}

	((EEventClass *)eme_parent)->target_free(ep, t);
}

static void
eme_class_init(GObjectClass *klass)
{
	klass->finalize = eme_finalise;
	((EEventClass *)klass)->target_free = eme_target_free;
}

GType
em_event_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMEventClass),
			NULL, NULL,
			(GClassInitFunc)eme_class_init,
			NULL, NULL,
			sizeof(EMEvent), 0,
			(GInstanceInitFunc)eme_init
		};
		eme_parent = g_type_class_ref(e_event_get_type());
		type = g_type_register_static(e_event_get_type(), "EMEvent", &info, 0);
	}

	return type;
}

/**
 * em_event_peek:
 * @void:
 *
 * Get the singular instance of the mail event handler.
 *
 * Return value:
 **/
EMEvent *em_event_peek(void)
{
	if (em_event == NULL) {
		em_event = g_object_new(em_event_get_type(), NULL);
		e_event_construct(&em_event->popup, "org.gnome.evolution.mail.events");
	}

	return em_event;
}

EMEventTargetFolder *
em_event_target_new_folder (EMEvent *eme, const gchar *uri, guint new)
{
	EMEventTargetFolder *t = e_event_target_new(&eme->popup, EM_EVENT_TARGET_FOLDER, sizeof(*t));
	guint32 flags = new ? EM_EVENT_FOLDER_NEWMAIL : 0;

	t->uri = g_strdup(uri);
	t->target.mask = ~flags;
	t->new = new;

	return t;
}

EMEventTargetFolderBrowser *
em_event_target_new_folder_browser (EMEvent *eme, EMFolderBrowser *emfb)
{
	EMEventTargetFolderBrowser *t = e_event_target_new(&eme->popup, EM_EVENT_TARGET_FOLDER_BROWSER, sizeof(*t));

	t->emfb = emfb;

	return t;
}

EMEventTargetComposer *
em_event_target_new_composer (EMEvent *eme, const EMsgComposer *composer, guint32 flags)
{
	EMEventTargetComposer *t = e_event_target_new(&eme->popup, EM_EVENT_TARGET_COMPOSER, sizeof(*t));

	t->composer = g_object_ref(G_OBJECT(composer));
	t->target.mask = ~flags;

	return t;
}

EMEventTargetMessage *
em_event_target_new_message(EMEvent *eme, CamelFolder *folder, CamelMimeMessage *message, const gchar *uid, guint32 flags, EMsgComposer *composer)
{
	EMEventTargetMessage *t = e_event_target_new(&eme->popup, EM_EVENT_TARGET_MESSAGE, sizeof(*t));

	t->uid = g_strdup (uid);
	t->folder = folder;
	if (folder)
		camel_object_ref(folder);
	t->message = message;
	if (message)
		camel_object_ref(message);
	t->target.mask = ~flags;
	if (composer)
		t->composer = g_object_ref(G_OBJECT(composer));

	return t;
}

EMEventTargetSendReceive *
em_event_target_new_send_receive(EMEvent *eme, GtkWidget *table, gpointer data, gint row, guint32 flags)
{
	EMEventTargetSendReceive *t = e_event_target_new(&eme->popup, EM_EVENT_TARGET_SEND_RECEIVE, sizeof(*t));

	t->table = table;
	t->data = data;
	t->row = row;
	t->target.mask = ~flags;

	return t;
}

EMEventTargetCustomIcon *
em_event_target_new_custom_icon(EMEvent *eme, GtkTreeStore *store, GtkTreeIter *iter, const gchar *folder_name, guint32 flags)
{
	EMEventTargetCustomIcon *t = e_event_target_new(&eme->popup, EM_EVENT_TARGET_CUSTOM_ICON, sizeof(*t));

	t->store = store;
	t->iter = iter;
	t->folder_name = folder_name;
	t->target.mask = ~flags;

	return t;
}

/* ********************************************************************** */

static gpointer emeh_parent_class;
#define emeh ((EMEventHook *)eph)

static const EEventHookTargetMask emeh_folder_masks[] = {
	{ "newmail", EM_EVENT_FOLDER_NEWMAIL },
	{ NULL }
};

static const EEventHookTargetMask emeh_folder_browser_masks[] = {
	{ "folderbrowser", EM_EVENT_FOLDER_BROWSER },
	{ NULL }
};

static const EEventHookTargetMask emeh_composer_masks[] = {
	{ "sendoption", EM_EVENT_COMPOSER_SEND_OPTION },
	{ NULL }
};

static const EEventHookTargetMask emeh_message_masks[] = {
	{ "replyall", EM_EVENT_MESSAGE_REPLY_ALL },
	{ "reply", EM_EVENT_MESSAGE_REPLY },
	{ NULL }
};

static const EEventHookTargetMask emeh_send_receive_masks[] = {
	{ "sendreceive", EM_EVENT_SEND_RECEIVE },
	{ NULL }
};

static const EEventHookTargetMask emeh_custom_icon_masks[] = {
	{ "customicon", EM_EVENT_CUSTOM_ICON },
	{ NULL }
};

static const EEventHookTargetMap emeh_targets[] = {
	{ "folder", EM_EVENT_TARGET_FOLDER, emeh_folder_masks },
	{ "folderbrowser", EM_EVENT_TARGET_FOLDER_BROWSER, emeh_folder_browser_masks },
	{ "message", EM_EVENT_TARGET_MESSAGE, emeh_message_masks },
	{ "composer", EM_EVENT_TARGET_COMPOSER, emeh_composer_masks},
	{ "sendreceive", EM_EVENT_TARGET_SEND_RECEIVE, emeh_send_receive_masks},
	{ "customicon", EM_EVENT_TARGET_CUSTOM_ICON, emeh_custom_icon_masks},
	{ NULL }
};

static void
emeh_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)emeh_parent_class)->finalize(o);
}

static void
emeh_class_init(EPluginHookClass *klass)
{
	gint i;

	((GObjectClass *)klass)->finalize = emeh_finalise;
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.mail.events:1.0";

	for (i=0;emeh_targets[i].type;i++)
		e_event_hook_class_add_target_map((EEventHookClass *)klass, &emeh_targets[i]);

	((EEventHookClass *)klass)->event = (EEvent *)em_event_peek();
}

GType
em_event_hook_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMEventHookClass), NULL, NULL, (GClassInitFunc) emeh_class_init, NULL, NULL,
			sizeof(EMEventHook), 0, (GInstanceInitFunc) NULL,
		};

		emeh_parent_class = g_type_class_ref(e_event_hook_get_type());
		type = g_type_register_static(e_event_hook_get_type(), "EMEventHook", &info, 0);
	}

	return type;
}
