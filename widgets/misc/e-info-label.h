/*
 *
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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_INFO_LABEL_H
#define _E_INFO_LABEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_INFO_LABEL_GET_CLASS(emfv)  ((EInfoLabelClass *) G_OBJECT_GET_CLASS (emfv))

typedef struct _EInfoLabel EInfoLabel;
typedef struct _EInfoLabelClass EInfoLabelClass;

struct _EInfoLabel {
	GtkHBox parent;

	GtkWidget *location;
	GtkWidget *info;
};

struct _EInfoLabelClass {
	GtkHBoxClass parent_class;
};

GType e_info_label_get_type(void);

GtkWidget *e_info_label_new(const gchar *icon);
void e_info_label_set_info(EInfoLabel *, const gchar *loc, const gchar *info);

G_END_DECLS

#endif /* ! _E_INFO_LABEL_H */
