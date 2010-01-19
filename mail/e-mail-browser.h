/*
 * e-mail-browser.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_BROWSER_H
#define E_MAIL_BROWSER_H

#include <gtk/gtk.h>
#include <misc/e-focus-tracker.h>
#include <shell/e-shell-backend.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_BROWSER \
	(e_mail_browser_get_type ())
#define E_MAIL_BROWSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_BROWSER, EMailBrowser))
#define E_MAIL_BROWSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_BROWSER, EMailBrowserClass))
#define E_IS_MAIL_BROWSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_BROWSER))
#define E_IS_MAIL_BROWSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_BROWSER))
#define E_MAIL_BROWSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_BROWSER, EMailBrowserClass))

G_BEGIN_DECLS

typedef struct _EMailBrowser EMailBrowser;
typedef struct _EMailBrowserClass EMailBrowserClass;
typedef struct _EMailBrowserPrivate EMailBrowserPrivate;

struct _EMailBrowser {
	GtkWindow parent;
	EMailBrowserPrivate *priv;
};

struct _EMailBrowserClass {
	GtkWindowClass parent_class;
};

GType		e_mail_browser_get_type		(void);
GtkWidget *	e_mail_browser_new		(EShellBackend *shell_backend);
void		e_mail_browser_close		(EMailBrowser *browser);
gboolean	e_mail_browser_get_show_deleted	(EMailBrowser *browser);
void		e_mail_browser_set_show_deleted (EMailBrowser *browser,
						 gboolean show_deleted);
EFocusTracker *	e_mail_browser_get_focus_tracker(EMailBrowser *browser);
GtkUIManager *	e_mail_browser_get_ui_manager	(EMailBrowser *browser);

G_END_DECLS

#endif /* E_MAIL_BROWSER_H */
