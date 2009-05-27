/*
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
 * Authors:
 *		Sankar P <psankar@novell.com>
 *		Michael Zucchi <notzed@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _ORG_GNOME_EVOLUTION_MONO_H
#define _ORG_GNOME_EVOLUTION_MONO_H

#include "e-util/e-plugin.h"

/* ********************************************************************** */
/* This is ALL private */

typedef struct _EPluginMono EPluginMono;
typedef struct _EPluginMonoClass EPluginMonoClass;

struct _EPluginMono {
	EPlugin plugin;

	struct _EPluginMonoPrivate *priv;

	gchar *location;		/* location */
	gchar *handler;		/* handler class */
};

struct _EPluginMonoClass {
	EPluginClass plugin_class;
};

gpointer org_gnome_evolution_mono_get_type(gpointer a, gpointer b);

#endif /* ! _ORG_GNOME_EVOLUTION_MONO_H */
