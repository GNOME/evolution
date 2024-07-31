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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_PLUGIN_UI_H
#define E_PLUGIN_UI_H

#include <gtk/gtk.h>

#include <e-util/e-plugin.h>
#include <e-util/e-ui-manager.h>

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
typedef gboolean	(*EPluginUIInitFunc)	(EUIManager *ui_manager,
						 gpointer user_data);

GType		e_plugin_ui_hook_get_type	(void) G_GNUC_CONST;

void		e_plugin_ui_register_manager	(EUIManager *ui_manager,
						 const gchar *id,
						 gpointer user_data);

G_END_DECLS

#endif /* E_PLUGIN_UI_H */
