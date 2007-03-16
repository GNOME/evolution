/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-component-utils.h
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __EVOLUTION_SHELL_COMPONENT_UTILS_H__
#define __EVOLUTION_SHELL_COMPONENT_UTILS_H__

#include <bonobo/bonobo-ui-component.h>
#include <gtk/gtkwindow.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

typedef struct _EPixmap {
	const char *path;
	const char *name;
	gint       size;
	char       *pixbuf;
} EPixmap;

#define E_PIXMAP(path,name,size)	{ (path), (name), (size), NULL }
#define E_PIXMAP_END			{ NULL, NULL, 0, NULL }

/* Takes an array of pixmaps, terminated by E_PIXMAP_END, and loads into uic */
void e_pixmaps_update (BonoboUIComponent *uic, EPixmap *pixcache);

char *e_get_activation_failure_msg  (CORBA_Environment *ev);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_SHELL_COMPONENT_UTILS_H__ */
