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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EVOLUTION_SHELL_COMPONENT_UTILS_H__
#define __EVOLUTION_SHELL_COMPONENT_UTILS_H__

#include <bonobo/bonobo-ui-component.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EPixmap {
	const gchar *path;
	const gchar *name;
	GtkIconSize size;
	gchar       *pixbuf;
} EPixmap;

#define E_PIXMAP(path,name,size)	{ (path), (name), (size), NULL }
#define E_PIXMAP_END			{ NULL, NULL, 0, NULL }

/* Takes an array of pixmaps, terminated by E_PIXMAP_END, and loads into uic */
void e_pixmaps_update (BonoboUIComponent *uic, EPixmap *pixcache);

gchar *e_get_activation_failure_msg  (CORBA_Environment *ev);

G_END_DECLS

#endif /* __EVOLUTION_SHELL_COMPONENT_UTILS_H__ */
