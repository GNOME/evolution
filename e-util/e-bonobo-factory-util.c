/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-bonobo-factory-util.c
 *
 * Copyright (C) 2001 Ximian, Inc.
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-bonobo-factory-util.h"

#include <X11/Xlib.h>
#include <gdk/gdkprivate.h>


BonoboGenericFactory *
e_bonobo_generic_factory_multi_display_new (const char *factory_iid,
					    GnomeFactoryCallback factory_callback,
					    void *factory_callback_data)
{
	BonoboGenericFactory *factory;
	char *registration_id;
	char *display_string;

	g_return_val_if_fail (factory_iid != NULL, NULL);
	g_return_val_if_fail (factory_callback != NULL, NULL);

	display_string = DisplayString (gdk_display);
        registration_id = oaf_make_registration_id (factory_iid, display_string);
	factory = bonobo_generic_factory_new_multi (registration_id, factory_callback, factory_callback_data);

	g_free (registration_id);
	XFree (display_string);

	return factory;
}
