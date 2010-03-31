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
 *		Srinivasa Ragavan <sragavan@novell.com>
 *
 * Copyright (C) 2009 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _ANJAL_MAIL_VIEW_H_
#define _ANJAL_MAIL_VIEW_H_

#include <gtk/gtk.h>
#include <mail/em-folder-tree.h>

#define ANJAL_MAIL_VIEW_TYPE        (anjal_mail_view_get_type ())
#define ANJAL_MAIL_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), ANJAL_MAIL_VIEW_TYPE, AnjalMailView))
#define ANJAL_MAIL_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), ANJAL_MAIL_VIEW_TYPE, AnjalMailViewClass))
#define ANJAL_IS_MAIL_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), ANJAL_MAIL_VIEW_TYPE))
#define ANJAL_IS_MAIL_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), ANJAL_MAIL_VIEW_TYPE))
#define ANJAL_MAIL_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), ANJAL_MAIL_VIEW_TYPE, AnjalMailViewClass))

typedef struct _AnjalMailViewPrivate AnjalMailViewPrivate;

typedef struct _AnjalMailView {
	GtkNotebook parent;

	AnjalMailViewPrivate *priv;
} AnjalMailView;

typedef struct _AnjalMailViewClass {
	GtkNotebookClass parent_class;
	void (*set_folder_uri) (AnjalMailView *mail_view,
                                const gchar *uri);
	void (*set_folder_tree_widget) (AnjalMailView *mail_view,
				GtkWidget *tree);
	void (*set_folder_tree) (AnjalMailView *mail_view,
				EMFolderTree *tree);
	void (*set_search) (AnjalMailView *mail_view,
				const gchar *search);
	void (* init_search) (AnjalMailView *mail_view, GtkWidget *search);
} AnjalMailViewClass;

GType anjal_mail_view_get_type (void);
AnjalMailView * anjal_mail_view_new (void);
void  anjal_mail_view_set_folder_uri (AnjalMailView *mv, const gchar *uri);
void anjal_mail_view_set_folder_tree_widget (AnjalMailView *mv, GtkWidget *tree);
void anjal_mail_view_set_folder_tree (AnjalMailView *mv, GtkWidget *tree);
void anjal_mail_view_set_search (AnjalMailView *view, const gchar *search);
void anjal_mail_view_init_search (AnjalMailView *mv, GtkWidget *search);
#endif
