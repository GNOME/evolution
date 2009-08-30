/*
 * e-plugin-mono.h
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

#ifndef E_PLUGIN_MONO_H
#define E_PLUGIN_MONO_H

#include <e-util/e-plugin.h>

/* Standard GObject macros */
#define E_TYPE_PLUGIN_MONO \
	(e_plugin_mono_get_type ())
#define E_PLUGIN_MONO(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PLUGIN_MONO, EPluginMono))
#define E_PLUGIN_MONO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PLUGIN_MONO, EPluginMonoClass))
#define E_IS_PLUGIN_MONO(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PLUGIN_MONO))
#define E_IS_PLUGIN_MONO_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PLUGIN_MONO))
#define E_PLUGIN_MONO_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PLUGIN_MONO, EPluginMonoClass))

G_BEGIN_DECLS

typedef struct _EPluginMono EPluginMono;
typedef struct _EPluginMonoClass EPluginMonoClass;
typedef struct _EPluginMonoPrivate EPluginMonoPrivate;

struct _EPluginMono {
	EPlugin parent;
	EPluginMonoPrivate *priv;

	gchar *location;
	gchar *handler;
};

struct _EPluginMonoClass {
	EPluginClass parent_class;
};

GType		e_plugin_mono_get_type		(void);
void		e_plugin_mono_register_type	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_PLUGIN_MONO_H */
