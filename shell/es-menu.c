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

#include "es-menu.h"

static GObjectClass *esm_parent;

static void
esm_init(GObject *o)
{
	/*ESMenu *esm = (ESMenu *)o; */
}

static void
esm_finalise(GObject *o)
{
	((GObjectClass *)esm_parent)->finalize(o);
}

static void
esm_target_free(EMenu *ep, EMenuTarget *t)
{
	switch (t->type) {
	case ES_MENU_TARGET_SHELL: {
		ESMenuTargetShell *s = (ESMenuTargetShell *)t;

		s = s;
		break; }
	}

	((EMenuClass *)esm_parent)->target_free(ep, t);
}

static void
esm_class_init(GObjectClass *klass)
{
	printf("es menu class init\n");

	klass->finalize = esm_finalise;
	((EMenuClass *)klass)->target_free = esm_target_free;
}

GType
es_menu_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(ESMenuClass),
			NULL, NULL,
			(GClassInitFunc)esm_class_init,
			NULL, NULL,
			sizeof(ESMenu), 0,
			(GInstanceInitFunc)esm_init
		};
		esm_parent = g_type_class_ref(e_menu_get_type());
		type = g_type_register_static(e_menu_get_type(), "ESMenu", &info, 0);
	}

	return type;
}

ESMenu *es_menu_new(const char *menuid)
{
	ESMenu *esm = g_object_new(es_menu_get_type(), 0);

	e_menu_construct(&esm->menu, menuid);

	return esm;
}

/**
 * es_menu_target_new_shell:
 * @esm:
 * @flags:
 * 
 * Create a new menu target for the shell.
 * 
 * Return value: 
 **/
ESMenuTargetShell *
es_menu_target_new_shell(ESMenu *esm, guint32 flags)
{
	ESMenuTargetShell *t = e_menu_target_new(&esm->menu, ES_MENU_TARGET_SHELL, sizeof(*t));
	guint32 mask = ~0;

	mask &= ~ flags;
	t->target.mask = mask;

	return t;
}

/* ********************************************************************** */


static void *esph_parent_class;
#define esph ((ESMenuHook *)eph)

static const EMenuHookTargetMask esph_shell_masks[] = {
	{ "online", ES_MENU_SHELL_ONLINE },
	{ "offline", ES_MENU_SHELL_OFFLINE },
	{ 0 }
};

static const EMenuHookTargetMap esph_targets[] = {
	{ "shell", ES_MENU_TARGET_SHELL, esph_shell_masks },
	{ 0 }
};

static void
esph_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)esph_parent_class)->finalize(o);
}

static void
esph_class_init(EPluginHookClass *klass)
{
	int i;

	/** @HookClass: Shell Main Menu
	 * @Id: org.gnome.evolution.shell.bonobomenu:1.0
	 * @Target: ESMenuTargetShell
	 * 
	 * A hook for the main menus from the shell component.
	 *
	 * These menu's will be available from all components, but
	 * will have no context for the current component.
	 **/

	((GObjectClass *)klass)->finalize = esph_finalise;
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.shell.bonobomenu:1.0";

	for (i=0;esph_targets[i].type;i++)
		e_menu_hook_class_add_target_map((EMenuHookClass *)klass, &esph_targets[i]);

	/* FIXME: leaks parent set class? */
	((EMenuHookClass *)klass)->menu_class = g_type_class_ref(es_menu_get_type());
}

GType
es_menu_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(ESMenuHookClass), NULL, NULL, (GClassInitFunc) esph_class_init, NULL, NULL,
			sizeof(ESMenuHook), 0, (GInstanceInitFunc) NULL,
		};

		esph_parent_class = g_type_class_ref(e_menu_hook_get_type());
		type = g_type_register_static(e_menu_hook_get_type(), "ESMenuHook", &info, 0);
	}
	
	return type;
}
