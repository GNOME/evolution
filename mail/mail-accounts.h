/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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


#ifndef __MAIL_ACCOUNTS_TAB_H__
#define __MAIL_ACCOUNTS_TAB_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

#include <gtk/gtkvbox.h>
#include <gtk/gtkbutton.h>
#include <glade/glade.h>

#include <gal/e-table/e-table.h>

#include "evolution-config-control.h"

#include <shell/Evolution.h>


#define MAIL_ACCOUNTS_TAB_TYPE        (mail_accounts_tab_get_type ())
#define MAIL_ACCOUNTS_TAB(o)          (GTK_CHECK_CAST ((o), MAIL_ACCOUNTS_TAB_TYPE, MailAccountsTab))
#define MAIL_ACCOUNTS_TAB_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MAIL_ACCOUNTS_TAB_TYPE, MailAccountsTabClass))
#define IS_MAIL_ACCOUNTS_TAB(o)       (GTK_CHECK_TYPE ((o), MAIL_ACCOUNTS_TAB_TYPE))
#define IS_MAIL_ACCOUNTS_TAB_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MAIL_ACCOUNTS_TAB_TYPE))

typedef struct _MailAccountsTab MailAccountsTab;
typedef struct _MailAccountsTabClass MailAccountsTabClass;

struct _MailAccountsTab {
	GtkVBox parent_object;
	
	GNOME_Evolution_Shell shell;
	
	GladeXML *gui;
	
	GtkWidget *druid;
	GtkWidget *editor;
	
	ETable *table;
	ETableModel *model;
	
	GtkButton *mail_add;
	GtkButton *mail_edit;
	GtkButton *mail_delete;
	GtkButton *mail_default;
	GtkButton *mail_able;
};

struct _MailAccountsTabClass {
	GtkVBoxClass parent_class;
	
	/* signals */
	
};


GtkType mail_accounts_tab_get_type (void);

GtkWidget *mail_accounts_tab_new (GNOME_Evolution_Shell shell);

void mail_accounts_tab_apply (MailAccountsTab *accounts);

/* needed by global config */
#define MAIL_ACCOUNTS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_Accounts_ConfigControl"

#ifdef __cplusplus
}
#endif

#endif /* __MAIL_ACCOUNTS_TAB_H__ */
