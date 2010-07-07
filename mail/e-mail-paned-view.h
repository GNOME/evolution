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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#ifndef _E_MAIL_PANED_VIEW_H_
#define _E_MAIL_PANED_VIEW_H_

#include <gtk/gtk.h>

#define E_MAIL_PANED_VIEW_TYPE        (e_mail_paned_view_get_type ())
#define E_MAIL_PANED_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_MAIL_PANED_VIEW_TYPE, MailFolderView))
#define E_MAIL_PANED_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_MAIL_PANED_VIEW_TYPE, MailFolderViewClass))
#define IS_E_MAIL_PANED_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_MAIL_PANED_VIEW_TYPE))
#define IS_E_MAIL_PANED_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_MAIL_PANED_VIEW_TYPE))
#define E_MAIL_PANED_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), E_MAIL_PANED_VIEW_TYPE, EMailPanedViewClass))


typedef struct _EMailPanedViewPrivate EMailPanedViewPrivate;

typedef struct _EMailPanedView {
	GtkVBox parent;

	EMailPanedViewPrivate *priv;
} EMailPanedView;

typedef struct _EMailPanedViewClass {
	GtkVBoxClass parent_class;

} EMailPanedViewClass;

#endif
