/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_PLUGIN_UI_H
#define E_PLUGIN_UI_H

#include <gtk/gtk.h>
#include "e-plugin.h"

/* Standard GObject macros */
#define E_TYPE_PLUGIN_UI_HOOK \
	(e_plugin_ui_hook_get_type ())
#define E_PLUGIN_UI_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PLUGIN_UI_HOOK, EPluginUIHook))
#define E_PLUGIN_UI_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PLUGIN_UI_HOOK, EPluginUIHookClass))
#define E_IS_PLUGIN_UI_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PLUGIN_UI_HOOK))
#define E_IS_PLUGIN_UI_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PLUGIN_UI_HOOK))
#define E_PLUGIN_UI_HOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PLUGIN_UI_HOOK, EPluginUIHookClass))

G_BEGIN_DECLS

typedef struct _EPluginUIHook EPluginUIHook;
typedef struct _EPluginUIHookClass EPluginUIHookClass;
typedef struct _EPluginUIHookPrivate EPluginUIHookPrivate;

struct _EPluginUIHook {
	EPluginHook parent;
	EPluginUIHookPrivate *priv;
};

struct _EPluginUIHookClass {
	EPluginHookClass parent_class;
};

/* Plugins with "org.gnome.evolution.ui" hooks should define a
 * function named e_plugin_ui_init() having this signature. */
typedef gboolean	(*EPluginUIInitFunc)	(GtkUIManager *manager,
						 gpointer user_data);

GType		e_plugin_ui_hook_get_type	(void);

void		e_plugin_ui_register_manager	(const gchar *id,
						 GtkUIManager *manager,
						 gpointer user_data);
const gchar *	e_plugin_ui_get_manager_id	(GtkUIManager *manager);

G_END_DECLS

#endif /* E_PLUGIN_UI_H */
