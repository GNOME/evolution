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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-cal-event.h"

static GObjectClass *ece_parent;

static void
ece_init (GObject *o)
{
}

static void
ece_finalize (GObject *o)
{
	((GObjectClass *) ece_parent)->finalize (o);
}

static void
ece_target_free (EEvent *ev, EEventTarget *t)
{
	switch (t->type) {
	case E_CAL_EVENT_TARGET_BACKEND: {
		ECalEventTargetBackend *s = (ECalEventTargetBackend *) t;
		if (s->shell_backend)
			g_object_unref (s->shell_backend);
		if (s->source_list)
			g_object_unref (s->source_list);
		break; }
	}

	((EEventClass *)ece_parent)->target_free (ev, t);
}

static void
ece_class_init (GObjectClass *klass)
{
	klass->finalize = ece_finalize;
	((EEventClass *)klass)->target_free = ece_target_free;
}

GType
e_cal_event_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (ECalEventClass),
			NULL, NULL,
			(GClassInitFunc) ece_class_init,
			NULL, NULL,
			sizeof (ECalEvent), 0,
			(GInstanceInitFunc) ece_init
		};
		ece_parent = g_type_class_ref (e_event_get_type ());
		type = g_type_register_static (e_event_get_type (), "ECalEvent", &info, 0);
	}

	return type;
}

ECalEvent *
e_cal_event_peek (void)
{
	static ECalEvent *e_cal_event = NULL;
	if (!e_cal_event) {
		e_cal_event = g_object_new (e_cal_event_get_type (), NULL);
		e_event_construct (&e_cal_event->event, "org.gnome.evolution.calendar.events");
	}
	return e_cal_event;
}

ECalEventTargetBackend *
e_cal_event_target_new_module (ECalEvent *ece, EShellBackend *shell_backend, ESourceList *source_list, guint32 flags)
{
	ECalEventTargetBackend *t = e_event_target_new (&ece->event, E_CAL_EVENT_TARGET_BACKEND, sizeof (*t));

	t->shell_backend = g_object_ref (shell_backend);
	t->source_list = g_object_ref (source_list);
	t->target.mask = ~flags;

	return t;
}
