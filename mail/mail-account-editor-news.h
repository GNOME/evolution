/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Cloned from mail-account-editor by Sam Creasey <sammy@oh.verio.com> 
 *
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2001 Helix Code, Inc. (www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef MAIL_ACCOUNT_EDITOR_NEWS_H
#define MAIL_ACCOUNT_EDITOR_NEWS_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-file-entry.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>
#include <camel/camel-provider.h>

#include "mail-config.h"

#define MAIL_ACCOUNT_EDITOR_NEWS_TYPE        (mail_account_editor_news_get_type ())
#define MAIL_ACCOUNT_EDITOR_NEWS(o)          (GTK_CHECK_CAST ((o), MAIL_ACCOUNT_EDITOR_NEWS_TYPE, MailAccountEditorNews))
#define MAIL_ACCOUNT_EDITOR_NEWS_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), MAIL_ACCOUNT_EDITOR_NEWS_TYPE, MailAccountEditorNewsClass))
#define MAIL_IS_ACCOUNT_EDITOR_NEWS(o)       (GTK_CHECK_TYPE ((o), MAIL_ACCOUNT_EDITOR_NEWS_TYPE))
#define MAIL_IS_ACCOUNT_EDITOR_NEWS_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), MAIL_ACCOUNT_EDITOR_NEWS_TYPE))

struct _MailAccountEditorNews {
	GnomeDialog parent;
	
	GladeXML *xml;
	MailConfigService *service;
	GtkNotebook *notebook;
};

typedef struct _MailAccountEditorNews MailAccountEditorNews;

typedef struct {
	GnomeDialogClass parent_class;
	
	/* signals */
	
} MailAccountEditorNewsClass;

GtkType mail_account_editor_news_get_type (void);

GtkWidget *mail_account_editor_news_new (MailConfigService *service);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_ACCOUNT_EDITOR_NEWS_H */
