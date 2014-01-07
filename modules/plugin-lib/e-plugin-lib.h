/*
 * e-plugin-lib.h
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

#ifndef E_PLUGIN_LIB_H
#define E_PLUGIN_LIB_H

#include <gmodule.h>
#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_PLUGIN_LIB \
	(e_plugin_lib_get_type ())
#define E_PLUGIN_LIB(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PLUGIN_LIB, EPluginLib))
#define E_PLUGIN_LIB_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PLUGIN_LIB, EPluginLibClass))
#define E_IS_PLUGIN_LIB(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PLUGIN_LIB))
#define E_IS_PLUGIN_LIB_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PLUGIN_LIB))
#define E_PLUGIN_LIB_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PLUGIN_LIB, EPluginLibClass))

G_BEGIN_DECLS

typedef struct _EPluginLib EPluginLib;
typedef struct _EPluginLibClass EPluginLibClass;

/* The callback signature used for epluginlib methods */
typedef gpointer (*EPluginLibFunc) (EPlugin *ep, gpointer data);

/* The setup method, this will be called when the plugin is
 * initialized.  In the future it may also be called when the plugin
 * is disabled. */
typedef gint (*EPluginLibEnableFunc) (EPlugin *ep, gint enable);

typedef GtkWidget * (*EPluginLibGetConfigureWidgetFunc) (EPlugin *ep);

/**
 * struct _EPluginLib -
 *
 * @plugin: Superclass.
 * @location: The filename of the shared object.
 * @module: The GModule once it is loaded.
 *
 * This is a concrete EPlugin class.  It loads and invokes dynamically
 * loaded libraries using GModule.  The shared object isn't loaded
 * until the first callback is invoked.
 *
 * When the plugin is loaded, and if it exists, "e_plugin_lib_enable"
 * will be invoked to initialize the plugin.
 **/
struct _EPluginLib {
	EPlugin parent;

	gchar *location;
	GModule *module;
};

struct _EPluginLibClass {
	EPluginClass parent_class;
};

GType		e_plugin_lib_get_type		(void);
void		e_plugin_lib_register_type	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_PLUGIN_LIB_H */
