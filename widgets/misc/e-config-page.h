/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-config-page.h
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

#ifndef _E_CONFIG_PAGE_H_
#define _E_CONFIG_PAGE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_CONFIG_PAGE			(e_config_page_get_type ())
#define E_CONFIG_PAGE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CONFIG_PAGE, EConfigPage))
#define E_CONFIG_PAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CONFIG_PAGE, EConfigPageClass))
#define E_IS_CONFIG_PAGE(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CONFIG_PAGE))
#define E_IS_CONFIG_PAGE_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_CONFIG_PAGE))


typedef struct _EConfigPage        EConfigPage;
typedef struct _EConfigPagePrivate EConfigPagePrivate;
typedef struct _EConfigPageClass   EConfigPageClass;

struct _EConfigPage {
	GtkEventBox parent;

	EConfigPagePrivate *priv;
};

struct _EConfigPageClass {
	GtkEventBoxClass parent_class;
	
};


GtkType    e_config_page_get_type    (void);
GtkWidget *e_config_page_new         (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_CONFIG_PAGE_H_ */
