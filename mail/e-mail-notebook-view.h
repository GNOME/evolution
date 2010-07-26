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

#ifndef E_MAIL_NOTEBOOK_VIEW_H
#define E_MAIL_NOTEBOOK_VIEW_H

#include <mail/e-mail-view.h>
#include <shell/e-shell-searchbar.h>
#include <menus/gal-view-instance.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_NOTEBOOK_VIEW \
	(e_mail_notebook_view_get_type ())
#define E_MAIL_NOTEBOOK_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_NOTEBOOK_VIEW, EMailNotebookView))
#define E_MAIL_NOTEBOOK_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_NOTEBOOK_VIEW, EMailNotebookViewClass))
#define E_IS_MAIL_NOTEBOOK_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_NOTEBOOK_VIEW))
#define E_IS_MAIL_NOTEBOOK_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_NOTEBOOK_VIEW))
#define E_MAIL_NOTEBOOK_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_NOTEBOOK_VIEW, EMailNotebookViewClass))

G_BEGIN_DECLS

typedef struct _EMailNotebookView EMailNotebookView;
typedef struct _EMailNotebookViewClass EMailNotebookViewClass;
typedef struct _EMailNotebookViewPrivate EMailNotebookViewPrivate;

struct _EMailNotebookView {
	EMailView parent;
	EMailNotebookViewPrivate *priv;
};

struct _EMailNotebookViewClass {
	EMailViewClass parent_class;
};

GType		e_mail_notebook_view_get_type	(void);
GtkWidget *	e_mail_notebook_view_new	(EShellView *shell_view);
EShellSearchbar *
		e_mail_notebook_view_get_searchbar
						(EMailNotebookView *view);
void		e_mail_notebook_view_set_search_strings
						(EMailNotebookView *view,
						 GSList *search_strings);
GalViewInstance *
		e_mail_notebook_view_get_view_instance
						(EMailNotebookView *view);
void		e_mail_notebook_view_update_view_instance
						(EMailNotebookView *view);
void		e_mail_notebook_view_set_show_deleted
						(EMailNotebookView *view,
						 gboolean show_deleted);
gboolean	e_mail_notebook_view_get_show_deleted
						(EMailNotebookView *view);
void		e_mail_notebook_view_set_preview_visible
						(EMailNotebookView *view,
						 gboolean preview_visible);
gboolean	e_mail_notebook_view_get_preview_visible
						(EMailNotebookView *view);
void		e_mail_notebook_view_set_orientation
						(EMailNotebookView *view,
						 GtkOrientation orientation);
GtkOrientation	e_mail_notebook_view_get_orientation
						(EMailNotebookView *view);

G_END_DECLS

#endif /* E_MAIL_NOTEBOOK_VIEW_H */
