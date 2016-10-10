/*
 * e-cal-config-hook.c
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

#include "e-cal-config-hook.h"

#include "calendar/gui/e-cal-config.h"

static const EConfigHookTargetMask no_masks[] = {
	{ NULL }
};

static const EConfigHookTargetMap targets[] = {
	{ "source", EC_CONFIG_TARGET_SOURCE, no_masks },
	{ "prefs", EC_CONFIG_TARGET_PREFS, no_masks },
	{ NULL }
};

static void
cal_config_hook_class_init (EConfigHookClass *class)
{
	EPluginHookClass *plugin_hook_class;
	gint ii;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = "org.gnome.evolution.calendar.config:1.0";

	class->config_class = g_type_class_ref (e_cal_config_get_type ());

	for (ii = 0; targets[ii].type != NULL; ii++)
		e_config_hook_class_add_target_map (
			(EConfigHookClass *) class, &targets[ii]);
}

void
e_cal_config_hook_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (EConfigHookClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) cal_config_hook_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EConfigHook),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, e_config_hook_get_type (),
		"ECalConfigHook", &type_info, 0);
}
