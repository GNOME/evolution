/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
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

#ifndef MAIL_ACCOUNT_EDITOR_H
#define MAIL_ACCOUNT_EDITOR_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gtk/gtkdialog.h>

struct _GtkNotebook;
struct _MailAccountGui;
struct _GtkWindow;
struct _EMAccountPrefs;

#define MAIL_ACCOUNT_EDITOR_TYPE        (mail_account_editor_get_type ())
#define MAIL_ACCOUNT_EDITOR(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), MAIL_ACCOUNT_EDITOR_TYPE, MailAccountEditor))
#define MAIL_ACCOUNT_EDITOR_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), MAIL_ACCOUNT_EDITOR_TYPE, MailAccountEditorClass))
#define MAIL_IS_ACCOUNT_EDITOR(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAIL_ACCOUNT_EDITOR_TYPE))
#define MAIL_IS_ACCOUNT_EDITOR_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), MAIL_ACCOUNT_EDITOR_TYPE))

struct _MailAccountEditor {
	GtkDialog parent_object;
	
	struct _MailAccountGui *gui;
	struct _GtkNotebook *notebook;
};

typedef struct _MailAccountEditor MailAccountEditor;

typedef struct {
	GtkDialogClass parent_class;
	
	/* signals */
	
} MailAccountEditorClass;

GType mail_account_editor_get_type (void);

MailAccountEditor *mail_account_editor_new (struct _EAccount *account, struct _GtkWindow *parent, struct _EMAccountPrefs *dialog);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MAIL_ACCOUNT_EDITOR_H */
