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

#ifndef _MAIL_ACCOUNT_VIEW_H_
#define _MAIL_ACCOUNT_VIEW_H_

#include <gtk/gtk.h>
#include "mail/em-account-editor.h"
#include <libedataserver/e-account-list.h>
#include "mail-view.h"

#define MAIL_ACCOUNT_VIEW_TYPE        (mail_account_view_get_type ())
#define MAIL_ACCOUNT_VIEW(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIL_ACCOUNT_VIEW_TYPE, MailFolderView))
#define MAIL_ACCOUNT_VIEW_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), MAIL_ACCOUNT_VIEW_TYPE, MailFolderViewClass))
#define IS_MAIL_ACCOUNT_VIEW(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIL_ACCOUNT_VIEW_TYPE))
#define IS_MAIL_ACCOUNT_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MAIL_ACCOUNT_VIEW_TYPE))
#define MAIL_ACCOUNT_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), MAIL_ACCOUNT_VIEW_TYPE, MailFolderViewClass))

typedef struct _MailAccountViewPrivate MailAccountViewPrivate;
typedef struct _MailAccountService MailAccountService;

typedef enum {
	MAV_IDENTITY_PAGE=0,
	MAV_RECV_PAGE,
	MAV_RECV_OPT_PAGE,
	MAV_SEND_PAGE,
	MAV_DEFAULTS_PAGE,
	MAV_REVIEW_PAGE,
	MAV_LAST,
} MAVPageType;

typedef struct _MAVPage {
	GtkWidget *box;
	GtkWidget *main;
	GtkWidget *error;
	GtkWidget *error_label;
	MAVPageType type;
	GtkWidget *next;
	GtkWidget *prev;
	gboolean done;
}MAVPage;

typedef struct _MailAccountView {
	GtkVBox parent;
	gint type;
	const gchar *uri;
	MailViewFlags flags;
	/* Base class of MailChildView ends */

	GtkWidget *scroll;
	GtkWidget *page_widget;

	MAVPage *pages[6];
	struct _EAccount *original;
	GtkWidget *wpages[6];
	gint current_page;
	struct _EMAccountEditor *edit;
	GtkWidget *password;

	MailAccountViewPrivate *priv;
} MailAccountView;

typedef struct _MailAccountViewClass {
	GtkVBoxClass parent_class;

	void (* view_close) (MailAccountView *);

} MailAccountViewClass;

GType mail_account_view_get_type (void);
MailAccountView *mail_account_view_new (EAccount *account);
GtkWidget * mail_account_view_get_tab_widget(MailAccountView *mcv);
void mail_account_view_activate (MailAccountView *mcv, GtkWidget *tree, GtkWidget *folder_tree, GtkWidget *check_mail, GtkWidget *sort_by, gboolean act);
#endif
