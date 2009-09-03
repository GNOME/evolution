/*
 * e-book-config-hook.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-book-config-hook.h"

#include "e-util/e-config.h"
#include "addressbook/gui/widgets/eab-config.h"

static const EConfigHookTargetMask no_masks[] = {
	{ NULL }
};

static const EConfigHookTargetMap targets[] = {
	{ "source", EAB_CONFIG_TARGET_SOURCE, no_masks },
	{ NULL }
};

static void
book_config_hook_class_init (EConfigHookClass *class)
{
	EPluginHookClass *plugin_hook_class;
	gint ii;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = "org.gnome.evolution.addressbook.config:1.0";

	class->config_class = g_type_class_ref (eab_config_get_type ());

	for (ii = 0; targets[ii].type != NULL; ii++)
		e_config_hook_class_add_target_map (
			(EConfigHookClass *) class, &targets[ii]);
}

void
e_book_config_hook_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (EConfigHookClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) book_config_hook_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EConfigHook),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, e_config_hook_get_type (),
		"EBookConfigHook", &type_info, 0);
}
