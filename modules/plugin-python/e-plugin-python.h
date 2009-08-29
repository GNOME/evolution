/*
 * e-plugin-python.h
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

#ifndef E_PLUGIN_PYTHON_H
#define E_PLUGIN_PYTHON_H

#include <e-util/e-plugin.h>

/* Standard GObject macros */
#define E_TYPE_PLUGIN_PYTHON \
	(e_plugin_python_get_type ())
#define E_PLUGIN_PYTHON(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PLUGIN_PYTHON, EPluginPython))
#define E_PLUGIN_PYTHON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PLUGIN_PYTHON, EPluginPythonClass))
#define E_IS_PLUGIN_PYTHON(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PLUGIN_PYTHON))
#define E_IS_PLUGIN_PYTHON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PLUGIN_PYTHON))
#define E_PLUGIN_PYTHON_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PLUGIN_PYTHON, EPluginPythonClass))

G_BEGIN_DECLS

typedef struct _EPluginPython EPluginPython;
typedef struct _EPluginPythonClass EPluginPythonClass;
typedef struct _EPluginPythonPrivate EPluginPythonPrivate;

struct _EPluginPython {
	EPlugin parent;
	EPluginPythonPrivate *priv;

	gchar *location;
	gchar *pClass;
	gchar *module_name;
};

struct _EPluginPythonClass {
	EPluginClass parent_class;
};

GType		e_plugin_python_get_type	(void);
void		e_plugin_python_register_type	(GTypeModule *type_module);

G_END_DECLS

#endif /* E_PLUGIN_PYTHON_H */
