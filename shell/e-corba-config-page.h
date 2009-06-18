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

#ifndef _E_CORBA_CONFIG_PAGE_H_
#define _E_CORBA_CONFIG_PAGE_H_

#include "e-config-page.h"

#include "Evolution.h"

#include <bonobo/bonobo-object.h>

G_BEGIN_DECLS

#define E_TYPE_CORBA_CONFIG_PAGE			(e_corba_config_page_get_type ())
#define E_CORBA_CONFIG_PAGE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CORBA_CONFIG_PAGE, ECorbaConfigPage))
#define E_CORBA_CONFIG_PAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CORBA_CONFIG_PAGE, ECorbaConfigPageClass))
#define E_IS_CORBA_CONFIG_PAGE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CORBA_CONFIG_PAGE))
#define E_IS_CORBA_CONFIG_PAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CORBA_CONFIG_PAGE))


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


GType      e_corba_config_page_get_type         (void);
GtkWidget *e_corba_config_page_new_from_objref  (GNOME_Evolution_ConfigControl  objref);
gboolean   e_corba_config_page_construct        (ECorbaConfigPage              *corba_config_page,
						 GNOME_Evolution_ConfigControl  objref);

G_END_DECLS

#endif /* _E_CORBA_CONFIG_PAGE_H_ */
