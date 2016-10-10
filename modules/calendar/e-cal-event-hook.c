/*
 * e-cal-event-hook.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-cal-event-hook.h"

#include "calendar/gui/e-cal-event.h"

static const EEventHookTargetMask masks[] = {
	{ "migration", E_CAL_EVENT_MODULE_MIGRATION },
	{ NULL }
};

static const EEventHookTargetMap targets[] = {
	{ "module", E_CAL_EVENT_TARGET_BACKEND, masks },
	{ NULL }
};

static void
cal_event_hook_class_init (EEventHookClass *class)
{
	EPluginHookClass *plugin_hook_class;
	gint ii;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = "org.gnome.evolution.calendar.events:1.0";

	class->event = (EEvent *) e_cal_event_peek ();

	for (ii = 0; targets[ii].type != NULL; ii++)
		e_event_hook_class_add_target_map (
			(EEventHookClass *) class, &targets[ii]);
}

void
e_cal_event_hook_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (EEventHookClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) cal_event_hook_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EEventHook),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, e_event_hook_get_type (),
		"ECalEventHook", &type_info, 0);
}
