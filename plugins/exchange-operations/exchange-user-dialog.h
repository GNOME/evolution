/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef __E2K_USER_DIALOG_H__
#define __E2K_USER_DIALOG_H__

#include <gtk/gtkdialog.h>

#define E2K_TYPE_USER_DIALOG		(e2k_user_dialog_get_type ())
#define E2K_USER_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E2K_TYPE_USER_DIALOG, E2kUserDialog))
#define E2K_USER_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E2K_TYPE_USER_DIALOG,	\
					 E2kUserDialogClass))
#define E2K_IS_USER_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E2K_TYPE_USER_DIALOG))
#define E2K_IS_USER_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), E2K_TYPE_USER_DIALOG))

typedef struct _E2kUserDialog		E2kUserDialog;
typedef struct _E2kUserDialogClass	E2kUserDialogClass;
typedef struct _E2kUserDialogPrivate	E2kUserDialogPrivate;

struct _E2kUserDialog {
	GtkDialog parent;

	/* Private data */
	E2kUserDialogPrivate *priv;
};

struct _E2kUserDialogClass {
	GtkDialogClass parent_class;
};

GType      e2k_user_dialog_get_type (void);
GtkWidget *e2k_user_dialog_new      (GtkWidget     *parent_window,
				     const char    *label_text,
				     const char    *section_name);
char      *e2k_user_dialog_get_user (E2kUserDialog *dialog);
GList      *e2k_user_dialog_get_user_list (E2kUserDialog *dialog);

#endif /* __E2K_USER_DIALOG_H__ */
