 /* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Author: Johnny Jacob <jjohnny@novell.com>
 *
 *  Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

	char *location;		/* location */
	char *pClass;		/* handler class */
        char *module_name;
};

struct _EPluginPythonClass {
	EPluginClass plugin_class;
};

void *org_gnome_evolution_python_get_type(void *a, void *b);


#endif /* ! _ORG_GNOME_EVOLUTION_PYTHON_H */
