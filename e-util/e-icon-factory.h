/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-icon-factory.h - Icon factory for the Evolution shell.
 *
 * Copyright (C) 2002-2004 Novell, Inc.
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
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_ICON_FACTORY_H_
#define _E_ICON_FACTORY_H_

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkwidget.h>

enum {
	E_ICON_SIZE_MENU,
	E_ICON_SIZE_BUTTON,
	E_ICON_SIZE_SMALL_TOOLBAR,
	E_ICON_SIZE_LARGE_TOOLBAR,
	E_ICON_SIZE_DND,
	E_ICON_SIZE_DIALOG,
	E_ICON_NUM_SIZES
};

/* standard size for list/tree widgets (16x16) */
#define E_ICON_SIZE_LIST E_ICON_SIZE_MENU

/* standard size for status bar icons (16x16) */
#define E_ICON_SIZE_STATUS E_ICON_SIZE_MENU



void       e_icon_factory_init              (void);
void       e_icon_factory_shutdown          (void);

char      *e_icon_factory_get_icon_filename (const char *icon_name, int icon_size);

GdkPixbuf *e_icon_factory_get_icon          (const char *icon_name, int icon_size);

GtkWidget *e_icon_factory_get_image         (const char *icon_name, int icon_size);

GList     *e_icon_factory_get_icon_list     (const char *icon_name);

#endif /* _E_ICON_FACTORY_H_ */
