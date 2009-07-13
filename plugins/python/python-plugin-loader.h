/*
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
 * Authors:
 *		Johnny Jacob <jjohnny@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _ORG_GNOME_EVOLUTION_PYTHON_H
#define _ORG_GNOME_EVOLUTION_PYTHON_H

#include "e-util/e-plugin.h"

typedef struct _EPluginPython EPluginPython;
typedef struct _EPluginPythonClass EPluginPythonClass;

struct _EPluginPython {
	EPlugin plugin;

	struct _EPluginPythonPrivate *priv;

	gchar *location;		/* location */
	gchar *pClass;		/* handler class */
        gchar *module_name;
};

struct _EPluginPythonClass {
	EPluginClass plugin_class;
};

gpointer org_gnome_evolution_python_get_type(gpointer a, gpointer b);

#endif /* ! _ORG_GNOME_EVOLUTION_PYTHON_H */
