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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __MAIL_CONFIG_FACTORY_H__
#define __MAIL_CONFIG_FACTORY_H__

G_BEGIN_DECLS

#include <bonobo/bonobo-generic-factory.h>
#include "evolution-config-control.h"

#include <shell/Evolution.h>

gboolean mail_config_register_factory (GNOME_Evolution_Shell shell);

BonoboObject *mail_config_control_factory_cb (BonoboGenericFactory *factory, const gchar *component_id, gpointer user_data);

G_END_DECLS

#endif /* __MAIL_CONFIG_FACTORY_H__ */
