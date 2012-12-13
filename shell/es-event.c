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

#include "es-event.h"
#include "e-shell.h"

static ESEvent *es_event;

G_DEFINE_TYPE (ESEvent, es_event, E_TYPE_EVENT)

static void
es_event_class_init (ESEventClass *class)
{
}

static void
es_event_init (ESEvent *event)
{
}

/**
 * es_event_peek:
 *
 * Get the singular instance of the shell event handler.
 *
 * Return: the shell event handler
 **/
ESEvent *
es_event_peek (void)
{
	if (es_event == NULL) {
		es_event = g_object_new (es_event_get_type (), NULL);
		/* @HookPoint: Shell Events Hookpoint
		 * Id: org.gnome.evolution.shell.events
		 *
		 * This is the hook point which emits shell events.
		 */
		e_event_construct (&es_event->event, "org.gnome.evolution.shell.events");
	}

	return es_event;
}

ESEventTargetUpgrade *
es_event_target_new_upgrade (ESEvent *eme,
                             gint major,
                             gint minor,
                             gint revision)
{
	ESEventTargetUpgrade *t;

	t = e_event_target_new (
		&eme->event, ES_EVENT_TARGET_UPGRADE, sizeof (*t));
	t->major = major;
	t->minor = minor;
	t->revision = revision;

	return t;
}

/* ********************************************************************** */

G_DEFINE_TYPE (ESEventHook, es_event_hook, E_TYPE_EVENT_HOOK)

static const EEventHookTargetMap emeh_targets[] = {
	{ "upgrade", ES_EVENT_TARGET_UPGRADE, NULL },
	{ NULL }
};

static void
es_event_hook_class_init (ESEventHookClass *class)
{
	EPluginHookClass *plugin_hook_class;
	EEventHookClass *event_hook_class;
	gint i;

	/* @HookClass: Shell Main Menu
	 * @Id: org.gnome.evolution.shell.events:1.0
	 * @Target: ESEventTargetState
	 *
	 * A hook for events coming from the shell.
	 */

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = "org.gnome.evolution.shell.events:1.0";

	for (i = 0; emeh_targets[i].type; i++)
		e_event_hook_class_add_target_map (
			(EEventHookClass *) class, &emeh_targets[i]);

	event_hook_class = E_EVENT_HOOK_CLASS (class);
	event_hook_class->event = (EEvent *) es_event_peek ();
}

static void
es_event_hook_init (ESEventHook *hook)
{
}

