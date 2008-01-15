/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-corba-config-page.h
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_CORBA_CONFIG_PAGE_H_
#define _E_CORBA_CONFIG_PAGE_H_

#include "e-config-page.h"

#include "Evolution.h"

#include <bonobo/bonobo-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_CORBA_CONFIG_PAGE			(e_corba_config_page_get_type ())
#define E_CORBA_CONFIG_PAGE(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_CORBA_CONFIG_PAGE, ECorbaConfigPage))
#define E_CORBA_CONFIG_PAGE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_CORBA_CONFIG_PAGE, ECorbaConfigPageClass))
#define E_IS_CORBA_CONFIG_PAGE(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_CORBA_CONFIG_PAGE))
#define E_IS_CORBA_CONFIG_PAGE_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_CORBA_CONFIG_PAGE))


typedef struct _ECorbaConfigPage        ECorbaConfigPage;
typedef struct _ECorbaConfigPagePrivate ECorbaConfigPagePrivate;
typedef struct _ECorbaConfigPageClass   ECorbaConfigPageClass;

struct _ECorbaConfigPage {
	EConfigPage parent;

	ECorbaConfigPagePrivate *priv;
};

struct _ECorbaConfigPageClass {
	EConfigPageClass parent_class;
};


GtkType    e_corba_config_page_get_type         (void);
GtkWidget *e_corba_config_page_new_from_objref  (GNOME_Evolution_ConfigControl  objref);
gboolean   e_corba_config_page_construct        (ECorbaConfigPage              *corba_config_page,
						 GNOME_Evolution_ConfigControl  objref);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_CORBA_CONFIG_PAGE_H_ */
