/*
 * e-mail-sidebar.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_SIDEBAR_H
#define E_MAIL_SIDEBAR_H

#include <mail/em-folder-tree.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SIDEBAR \
	(e_mail_sidebar_get_type ())
#define E_MAIL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SIDEBAR, EMailSidebar))
#define E_MAIL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SIDEBAR, EMailSidebarClass))
#define E_IS_MAIL_SIDEBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SIDEBAR))
#define E_IS_MAIL_SIDEBAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SIDEBAR))
#define E_MAIL_SIDEBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SIDEBAR, EMailSidebarClass))

G_BEGIN_DECLS

typedef struct _EMailSidebar EMailSidebar;
typedef struct _EMailSidebarClass EMailSidebarClass;
typedef struct _EMailSidebarPrivate EMailSidebarPrivate;

/* Flags describing the selected folder. */
enum {
	E_MAIL_SIDEBAR_FOLDER_ALLOWS_CHILDREN = 1 << 0,
	E_MAIL_SIDEBAR_FOLDER_CAN_DELETE = 1 << 1,
	E_MAIL_SIDEBAR_FOLDER_IS_JUNK = 1 << 2,
	E_MAIL_SIDEBAR_FOLDER_IS_OUTBOX = 1 << 3,
	E_MAIL_SIDEBAR_FOLDER_IS_STORE = 1 << 4,
	E_MAIL_SIDEBAR_FOLDER_IS_TRASH = 1 << 5,
	E_MAIL_SIDEBAR_FOLDER_IS_VIRTUAL = 1 << 6,
	E_MAIL_SIDEBAR_STORE_IS_BUILTIN = 1 << 7,
	E_MAIL_SIDEBAR_STORE_IS_SUBSCRIBABLE = 1 << 8,
	E_MAIL_SIDEBAR_STORE_CAN_BE_DISABLED = 1 << 9
};

struct _EMailSidebar {
	EMFolderTree parent;
	EMailSidebarPrivate *priv;
};

struct _EMailSidebarClass {
	EMFolderTreeClass parent;

	/* Methods */
	guint32		(*check_state)		(EMailSidebar *sidebar);

	/* Signals */
	void		(*key_file_changed)	(EMailSidebar *sidebar);
};

GType		e_mail_sidebar_get_type		(void);
GtkWidget *	e_mail_sidebar_new		(EMailSession *session,
						 EAlertSink *alert_sink);
GKeyFile *	e_mail_sidebar_get_key_file	(EMailSidebar *sidebar);
void		e_mail_sidebar_set_key_file	(EMailSidebar *sidebar,
						 GKeyFile *key_file);
guint32		e_mail_sidebar_check_state	(EMailSidebar *sidebar);
void		e_mail_sidebar_key_file_changed	(EMailSidebar *sidebar);

G_END_DECLS

#endif /* E_MAIL_SIDEBAR_H */
