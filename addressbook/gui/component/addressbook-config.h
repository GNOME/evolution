/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* addressbook-storage.h
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Authors:
 * Chris Toshok <toshok@ximian.com>
 * Chris Lahey <clahey@ximian.com>
 **/

#ifndef __ADDRESSBOOK_CONFIG_H__
#define __ADDRESSBOOK_CONFIG_H__

#include "addressbook-storage.h"

BonoboObject *addressbook_config_control_new (void);
void  addressbook_config_create_new_source  (const char *new_source,
					     GtkWidget  *parent);

#endif /* __ADDRESSBOOK_CONFIG_H__ */
