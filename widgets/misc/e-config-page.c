/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-config-page.c
 *
 * Copyright (C) 2002  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-config-page.h"

G_DEFINE_TYPE (EConfigPage, e_config_page, GTK_TYPE_EVENT_BOX)

/* GObject methods.  */

static void
e_config_page_class_init (EConfigPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
}

static void
e_config_page_init (EConfigPage *page)
{
}

GtkWidget *
e_config_page_new (void)
{
	return gtk_type_new (e_config_page_get_type ());
}


