/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-multi-config-dialog.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_MULTI_CONFIG_DIALOG_H
#define E_MULTI_CONFIG_DIALOG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-config-page.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Standard GObject macros */
#define E_TYPE_MULTI_CONFIG_DIALOG \
	(e_multi_config_dialog_get_type ())
#define E_MULTI_CONFIG_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MULTI_CONFIG_DIALOG, EMultiConfigDialog))
#define E_MULTI_CONFIG_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MULTI_CONFIG_DIALOG, EMultiConfigDialogClass))
#define E_IS_MULTI_CONFIG_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MULTI_CONFIG_DIALOG))
#define E_IS_MULTI_CONFIG_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_MULTI_CONFIG_DIALOG))
#define E_MULTI_CONFIG_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_TYPE \
	((obj), E_TYPE_MULTI_CONFIG_DIALOG, EMultiConfigDialogClass))

typedef struct _EMultiConfigDialog EMultiConfigDialog;
typedef struct _EMultiConfigDialogClass EMultiConfigDialogClass;
typedef struct _EMultiConfigDialogPrivate EMultiConfigDialogPrivate;

struct _EMultiConfigDialog {
	GtkDialog parent;
	EMultiConfigDialogPrivate *priv;
};

struct _EMultiConfigDialogClass {
	GtkDialogClass parent_class;
};

GType		e_multi_config_dialog_get_type	(void);
GtkWidget *	e_multi_config_dialog_new	(void);

void		e_multi_config_dialog_add_page	(EMultiConfigDialog *dialog,
						 const gchar *caption,
						 const gchar *icon_name,
						 EConfigPage *page);
void		e_multi_config_dialog_show_page	(EMultiConfigDialog *dialog,
						 gint page);

G_END_DECLS

#endif /* E_MULTI_CONFIG_DIALOG_H */
