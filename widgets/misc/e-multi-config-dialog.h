/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-multi-config-dialog.h
 *
 * Copyright (C) 2002  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_MULTI_CONFIG_DIALOG_H_
#define _E_MULTI_CONFIG_DIALOG_H_

#include "e-config-page.h"

#include <gtk/gtkwindow.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_MULTI_CONFIG_DIALOG			(e_multi_config_dialog_get_type ())
#define E_MULTI_CONFIG_DIALOG(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_MULTI_CONFIG_DIALOG, EMultiConfigDialog))
#define E_MULTI_CONFIG_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_MULTI_CONFIG_DIALOG, EMultiConfigDialogClass))
#define E_IS_MULTI_CONFIG_DIALOG(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_MULTI_CONFIG_DIALOG))
#define E_IS_MULTI_CONFIG_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_MULTI_CONFIG_DIALOG))


typedef struct _EMultiConfigDialog        EMultiConfigDialog;
typedef struct _EMultiConfigDialogPrivate EMultiConfigDialogPrivate;
typedef struct _EMultiConfigDialogClass   EMultiConfigDialogClass;

struct _EMultiConfigDialog {
	GnomeDialog parent;

	EMultiConfigDialogPrivate *priv;
};

struct _EMultiConfigDialogClass {
	GnomeDialogClass parent_class;
};


GtkType    e_multi_config_dialog_get_type (void);
GtkWidget *e_multi_config_dialog_new      (void);

void  e_multi_config_dialog_add_page  (EMultiConfigDialog *dialog,
				       const char         *title,
				       const char         *description,
				       GdkPixbuf          *icon,
				       EConfigPage        *page);
void  e_multi_config_dialog_show_page (EMultiConfigDialog *dialog,
				       int                 page);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_MULTI_CONFIG_DIALOG_H_ */
