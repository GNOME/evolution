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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-config-page.h"

G_DEFINE_TYPE (EConfigPage, e_config_page, GTK_TYPE_EVENT_BOX)

/* GObject methods.  */

static void
e_config_page_class_init (EConfigPageClass *class)
{
}

static void
e_config_page_init (EConfigPage *page)
{
}

GtkWidget *
e_config_page_new (void)
{
	return g_object_new (e_config_page_get_type (), NULL);
}

