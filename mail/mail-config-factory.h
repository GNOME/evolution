/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __MAIL_CONFIG_FACTORY_H__
#define __MAIL_CONFIG_FACTORY_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <bonobo/bonobo-generic-factory.h>
#include "evolution-config-control.h"

#include <shell/Evolution.h>

gboolean mail_config_register_factory (GNOME_Evolution_Shell shell);

BonoboObject *mail_config_control_factory_cb (BonoboGenericFactory *factory, const char *component_id, void *user_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MAIL_CONFIG_FACTORY_H__ */
