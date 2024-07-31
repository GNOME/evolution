/*
 * e-mail-browser.h
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

#ifndef E_MAIL_BROWSER_H
#define E_MAIL_BROWSER_H

#include <e-util/e-util.h>
#include <mail/e-mail-backend.h>
#include <mail/e-mail-display.h>

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
GtkWidget *	e_mail_browser_new		(EMailBackend *backend,
						 EMailFormatterMode display_mode);
void		e_mail_browser_close		(EMailBrowser *browser);
void		e_mail_browser_ask_close_on_reply
						(EMailBrowser *browser);
EAutomaticActionPolicy
		e_mail_browser_get_close_on_reply_policy
						(EMailBrowser *browser);
void		e_mail_browser_set_close_on_reply_policy
						(EMailBrowser *browser,
						 EAutomaticActionPolicy policy);
EMailFormatterMode
		e_mail_browser_get_display_mode	(EMailBrowser *browser);
EFocusTracker *	e_mail_browser_get_focus_tracker
						(EMailBrowser *browser);
gboolean	e_mail_browser_get_show_deleted	(EMailBrowser *browser);
void		e_mail_browser_set_show_deleted (EMailBrowser *browser,
						 gboolean show_deleted);
gboolean	e_mail_browser_get_show_junk	(EMailBrowser *browser);
void		e_mail_browser_set_show_junk	(EMailBrowser *browser,
						 gboolean show_junk);
gboolean	e_mail_browser_get_close_on_delete_or_junk
						(EMailBrowser *browser);
void		e_mail_browser_set_close_on_delete_or_junk
						(EMailBrowser *browser,
						 gboolean close_on_delete_or_junk);

G_END_DECLS

#endif /* E_MAIL_BROWSER_H */
