/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "em-event.h"
#include "e-util/e-msgport.h"
#include <e-util/e-icon-factory.h>

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

		g_free(s->uri);
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
		em_event = g_object_new(em_event_get_type(), 0);
		e_event_construct(&em_event->popup, "com.ximian.evolution.mail.events");
	}

	return em_event;
}

EMEventTargetFolder *
em_event_target_new_folder (EMEvent *eme, const char *uri, guint32 flags)
{
	EMEventTargetFolder *t = e_event_target_new(&eme->popup, EM_EVENT_TARGET_FOLDER, sizeof(*t));

	t->uri = g_strdup(uri);
	t->target.mask = ~flags;

	return t;
}

/* ********************************************************************** */

static void *emeh_parent_class;
#define emeh ((EMEventHook *)eph)

static const EEventHookTargetMask emeh_folder_masks[] = {
	{ "newmail", EM_EVENT_FOLDER_NEWMAIL },
	{ 0 }
};
static const EEventHookTargetMap emeh_targets[] = {
	{ "folder", EM_EVENT_TARGET_FOLDER, emeh_folder_masks },
	{ 0 }
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
	int i;

	((GObjectClass *)klass)->finalize = emeh_finalise;
	((EPluginHookClass *)klass)->id = "com.ximian.evolution.mail.events:1.0";

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
