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

#ifndef _E_CONFIG_PAGE_H_
#define _E_CONFIG_PAGE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

G_BEGIN_DECLS

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


GType      e_config_page_get_type    (void);
GtkWidget *e_config_page_new         (void);

G_END_DECLS

#endif /* _E_CONFIG_PAGE_H_ */
