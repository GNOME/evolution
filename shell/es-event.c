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

#include "es-event.h"

static GObjectClass *eme_parent;
static ESEvent *es_event;

static void
eme_init(GObject *o)
{
	/*ESEvent *eme = (ESEvent *)o; */
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
	case ES_EVENT_TARGET_STATE: {
		ESEventTargetState *s = (ESEventTargetState *)t;

		s = s;
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
es_event_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(ESEventClass),
			NULL, NULL,
			(GClassInitFunc)eme_class_init,
			NULL, NULL,
			sizeof(ESEvent), 0,
			(GInstanceInitFunc)eme_init
		};
		eme_parent = g_type_class_ref(e_event_get_type());
		type = g_type_register_static(e_event_get_type(), "ESEvent", &info, 0);
	}

	return type;
}

/**
 * es_event_peek:
 * @void: 
 * 
 * Get the singular instance of the shell event handler.
 * 
 * Return value: 
 **/
ESEvent *es_event_peek(void)
{
	if (es_event == NULL) {
		es_event = g_object_new(es_event_get_type(), 0);
		/** @HookPoint: Shell Events Hookpoint
		 * Id: org.gnome.evolution.shell.events
		 *
		 * This is the hook point which emits shell events.
		 */
		e_event_construct(&es_event->event, "org.gnome.evolution.shell.events");
	}

	return es_event;
}

ESEventTargetState *
es_event_target_new_state(ESEvent *eme, int state)
{
	ESEventTargetState *t = e_event_target_new(&eme->event, ES_EVENT_TARGET_STATE, sizeof(*t));
	guint32 mask = ~0;

	t->state = state;

	if (state)
		mask &= ~ES_EVENT_STATE_ONLINE;
	else
		mask &= ~ES_EVENT_STATE_OFFLINE;

	t->target.mask = mask;

	return t;
}

/* ********************************************************************** */

static void *emeh_parent_class;
#define emeh ((ESEventHook *)eph)

static const EEventHookTargetMask emeh_state_masks[] = {
	{ "online", ES_EVENT_STATE_ONLINE },
	{ "offline", ES_EVENT_STATE_OFFLINE },
	{ 0 }
};

static const EEventHookTargetMap emeh_targets[] = {
	{ "state", ES_EVENT_TARGET_STATE, emeh_state_masks },
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

	/** @HookClass: Shell Main Menu
	 * @Id: org.gnome.evolution.shell.events:1.0
	 * @Target: ESEventTargetState
	 * 
	 * A hook for events coming from the shell.
	 **/

	((GObjectClass *)klass)->finalize = emeh_finalise;
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.shell.events:1.0";

	for (i=0;emeh_targets[i].type;i++)
		e_event_hook_class_add_target_map((EEventHookClass *)klass, &emeh_targets[i]);

	((EEventHookClass *)klass)->event = (EEvent *)es_event_peek();
}

GType
es_event_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(ESEventHookClass), NULL, NULL, (GClassInitFunc) emeh_class_init, NULL, NULL,
			sizeof(ESEventHook), 0, (GInstanceInitFunc) NULL,
		};

		emeh_parent_class = g_type_class_ref(e_event_hook_get_type());
		type = g_type_register_static(e_event_hook_get_type(), "ESEventHook", &info, 0);
	}
	
	return type;
}
