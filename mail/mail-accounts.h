/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2001 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifndef MAIL_ACCOUNTS_H
#define MAIL_ACCOUNTS_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gnome.h>
#include <glade/glade.h>
#include <camel.h>
#include "shell/Evolution.h"

#define MAIL_ACCOUNTS_DIALOG_TYPE        (mail_accounts_dialog_get_type ())
#define MAIL_ACCOUNTS_DIALOG(o)          (GTK_CHECK_CAST ((o), MAIL_ACCOUNTS_DIALOG_TYPE, MailAccountsDialog))
#define MAIL_ACCOUNTS_DIALOG_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MAIL_ACCOUNTS_DIALOG_TYPE, MailAccountsDialogClass))
#define IS_MAIL_ACCOUNTS_DIALOG(o)       (GTK_CHECK_TYPE ((o), MAIL_ACCOUNTS_DIALOG_TYPE))
#define IS_MAIL_ACCOUNTS_DIALOG_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MAIL_ACCOUNTS_DIALOG_TYPE))

struct _MailAccountsDialog {
	GnomeDialog parent;
	
	GNOME_Evolution_Shell shell;
	
	GladeXML *gui;
	
	const GSList *accounts;
	gint accounts_row;
	
	GtkCList *mail_accounts;
	GtkButton *mail_add;
	GtkButton *mail_edit;
	GtkButton *mail_delete;
	GtkButton *mail_default;
	
	const GSList *news;
	gint news_row;
	
	GtkCList *news_accounts;
	GtkButton *news_add;
	GtkButton *news_edit;
	GtkButton *news_delete;
	
	/* "Other" widgets */
	GtkCheckButton *send_html;
	GtkSpinButton *timeout;
	GnomeFileEntry *pgp_path;
};

typedef struct _MailAccountsDialog MailAccountsDialog;

typedef struct {
	GnomeDialogClass parent_class;
	
	/* signals */
	
} MailAccountsDialogClass;

GtkType mail_accounts_dialog_get_type (void);

MailAccountsDialog *mail_accounts_dialog_new (GNOME_Evolution_Shell shell);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_ACCOUNTS_H */
