/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-cal-event.h"

G_DEFINE_TYPE (ECalEvent, e_cal_event, E_TYPE_EVENT)

static void
ece_target_free (EEvent *ev,
                 EEventTarget *t)
{
	switch (t->type) {
	case E_CAL_EVENT_TARGET_BACKEND: {
		ECalEventTargetBackend *s = (ECalEventTargetBackend *) t;
		if (s->shell_backend)
			g_object_unref (s->shell_backend);
		break; }
	}

	E_EVENT_CLASS (e_cal_event_parent_class)->target_free (ev, t);
}

static void
e_cal_event_class_init (ECalEventClass *class)
{
	EEventClass *event_class;

	event_class = E_EVENT_CLASS (class);
	event_class->target_free = ece_target_free;
}

static void
e_cal_event_init (ECalEvent *event)
{
}

ECalEvent *
e_cal_event_peek (void)
{
	static ECalEvent *e_cal_event = NULL;
	if (!e_cal_event) {
		e_cal_event = g_object_new (e_cal_event_get_type (), NULL);
		e_event_construct (
			&e_cal_event->event,
			"org.gnome.evolution.calendar.events");
	}
	return e_cal_event;
}

ECalEventTargetBackend *
e_cal_event_target_new_module (ECalEvent *ece,
                               EShellBackend *shell_backend,
                               guint32 flags)
{
	ECalEventTargetBackend *t;

	t = e_event_target_new (
		&ece->event, E_CAL_EVENT_TARGET_BACKEND, sizeof (*t));

	t->shell_backend = g_object_ref (shell_backend);
	t->target.mask = ~flags;

	return t;
}
