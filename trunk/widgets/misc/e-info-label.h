/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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

#ifndef _E_INFO_LABEL_H
#define _E_INFO_LABEL_H

#include <gtk/gtkhbox.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_INFO_LABEL_GET_CLASS(emfv)  ((EInfoLabelClass *) G_OBJECT_GET_CLASS (emfv))

typedef struct _EInfoLabel EInfoLabel;
typedef struct _EInfoLabelClass EInfoLabelClass;

struct _EInfoLabel {
	GtkHBox parent;

	struct _GtkWidget *location;
	struct _GtkWidget *info;
};

struct _EInfoLabelClass {
	GtkHBoxClass parent_class;
};

GType e_info_label_get_type(void);

GtkWidget *e_info_label_new(const char *icon);
void e_info_label_set_info(EInfoLabel *, const char *loc, const char *info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _E_INFO_LABEL_H */
