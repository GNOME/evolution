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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_MULTI_CONFIG_DIALOG_H_
#define _E_MULTI_CONFIG_DIALOG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-config-page.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define E_TYPE_MULTI_CONFIG_DIALOG			(e_multi_config_dialog_get_type ())
#define E_MULTI_CONFIG_DIALOG(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MULTI_CONFIG_DIALOG, EMultiConfigDialog))
#define E_MULTI_CONFIG_DIALOG_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_MULTI_CONFIG_DIALOG, EMultiConfigDialogClass))
#define E_IS_MULTI_CONFIG_DIALOG(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MULTI_CONFIG_DIALOG))
#define E_IS_MULTI_CONFIG_DIALOG_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_MULTI_CONFIG_DIALOG))


typedef struct _EMultiConfigDialog        EMultiConfigDialog;
typedef struct _EMultiConfigDialogPrivate EMultiConfigDialogPrivate;
typedef struct _EMultiConfigDialogClass   EMultiConfigDialogClass;

struct _EMultiConfigDialog {
	GtkDialog parent;

	EMultiConfigDialogPrivate *priv;
};

struct _EMultiConfigDialogClass {
	GtkDialogClass parent_class;
};


GType      e_multi_config_dialog_get_type (void);
GtkWidget *e_multi_config_dialog_new      (void);

void  e_multi_config_dialog_add_page  (EMultiConfigDialog *dialog,
				       const gchar         *title,
				       const gchar         *description,
				       GdkPixbuf          *icon,
				       EConfigPage        *page);
void  e_multi_config_dialog_show_page (EMultiConfigDialog *dialog,
				       gint                 page);

G_END_DECLS

#endif /* _E_MULTI_CONFIG_DIALOG_H_ */
