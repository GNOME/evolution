/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* wombat-interface-check.h
 *
 * Copyright (C) 2002  Ximian, Inc.
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

#ifndef _WOMBAT_INTERFACE_CHECK_H_
#define _WOMBAT_INTERFACE_CHECK_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-xobject.h>
#include "Evolution-Wombat.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define WOMBAT_TYPE_INTERFACE_CHECK		(wombat_interface_check_get_type ())
#define WOMBAT_INTERFACE_CHECK(obj)		(GTK_CHECK_CAST ((obj), WOMBAT_TYPE_INTERFACE_CHECK, WombatInterfaceCheck))
#define WOMBAT_INTERFACE_CHECK_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), WOMBAT_TYPE_INTERFACE_CHECK, WombatInterfaceCheckClass))
#define WOMBAT_IS_INTERFACE_CHECK(obj)		(GTK_CHECK_TYPE ((obj), WOMBAT_TYPE_INTERFACE_CHECK))
#define WOMBAT_IS_INTERFACE_CHECK_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), WOMBAT_TYPE_INTERFACE_CHECK))


typedef struct _WombatInterfaceCheck        WombatInterfaceCheck;
typedef struct _WombatInterfaceCheckPrivate WombatInterfaceCheckPrivate;
typedef struct _WombatInterfaceCheckClass   WombatInterfaceCheckClass;

struct _WombatInterfaceCheck {
	BonoboXObject parent;
};

struct _WombatInterfaceCheckClass {
	BonoboXObjectClass parent_class;

	POA_GNOME_Evolution_WombatInterfaceCheck__epv epv;
};


GtkType               wombat_interface_check_get_type  (void);
WombatInterfaceCheck *wombat_interface_check_new       (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _WOMBAT_INTERFACE_CHECK_H_ */
