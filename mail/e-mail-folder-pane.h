/*
 * e-mail-folder-pane.h
 *
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
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#ifndef E_MAIL_FOLDER_PANE_H
#define E_MAIL_FOLDER_PANE_H

#include <mail/e-mail-paned-view.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_FOLDER_PANE \
	(e_mail_folder_pane_get_type ())
#define E_MAIL_FOLDER_PANE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_FOLDER_PANE, EMailFolderPane))
#define E_MAIL_FOLDER_PANE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FOLDER_PANE, EMailFolderPaneClass))
#define E_IS_MAIL_FOLDER_PANE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_FOLDER_PANE))
#define E_IS_MAIL_FOLDER_PANE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_FOLDER_PANE))
#define E_MAIL_FOLDER_PANE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_FOLDER_PANE, EMailFolderPaneClass))

G_BEGIN_DECLS

typedef struct _EMailFolderPane EMailFolderPane;
typedef struct _EMailFolderPaneClass EMailFolderPaneClass;
typedef struct _EMailFolderPanePrivate EMailFolderPanePrivate;

struct _EMailFolderPane {
	EMailPanedView parent;
	EMailFolderPanePrivate *priv;
};

struct _EMailFolderPaneClass {
	EMailPanedViewClass parent_class;
};

GType		e_mail_folder_pane_get_type	(void);
EMailView *	e_mail_folder_pane_new		(EShellView *shell_view);

G_END_DECLS

#endif /* E_MAIL_FOLDER_PANE_H */
