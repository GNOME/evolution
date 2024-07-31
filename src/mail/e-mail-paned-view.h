/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#ifndef E_MAIL_PANED_VIEW_H
#define E_MAIL_PANED_VIEW_H

#include <mail/e-mail-view.h>

#include <shell/e-shell-searchbar.h>
#include <shell/e-shell-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PANED_VIEW \
	(e_mail_paned_view_get_type ())
#define E_MAIL_PANED_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PANED_VIEW, EMailPanedView))
#define E_MAIL_PANED_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PANED_VIEW, EMailPanedViewClass))
#define E_IS_MAIL_PANED_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PANED_VIEW))
#define E_IS_MAIL_PANED_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PANED_VIEW))
#define E_MAIL_PANED_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PANED_VIEW, EMailPanedViewClass))

G_BEGIN_DECLS

typedef struct _EMailPanedView EMailPanedView;
typedef struct _EMailPanedViewClass EMailPanedViewClass;
typedef struct _EMailPanedViewPrivate EMailPanedViewPrivate;

struct _EMailPanedView {
	EMailView parent;
	EMailPanedViewPrivate *priv;
};

struct _EMailPanedViewClass {
	EMailViewClass parent_class;

	guint		(*open_selected_mail)	(EMailPanedView *view);
};

GType		e_mail_paned_view_get_type	(void);
GtkWidget *	e_mail_paned_view_new		(EShellView *shell_view);
void		e_mail_paned_view_hide_message_list_pane
						(EMailPanedView *view,
						 gboolean visible);
GtkWidget *	e_mail_paned_view_get_preview	(EMailPanedView *view);
gboolean	e_mail_paned_view_get_preview_toolbar_visible
						(EMailPanedView *view);
void		e_mail_paned_view_set_preview_toolbar_visible
						(EMailPanedView *view,
						 gboolean value);
void		e_mail_paned_view_take_preview_toolbar
						(EMailPanedView *self,
						 GtkWidget *toolbar);

G_END_DECLS

#endif /* E_MAIL_PANED_VIEW_H */
