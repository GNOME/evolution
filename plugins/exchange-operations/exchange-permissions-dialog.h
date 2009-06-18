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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EXCHANGE_PERMISSIONS_DIALOG_H__
#define __EXCHANGE_PERMISSIONS_DIALOG_H__

#include <gtk/gtk.h>
#include "e-folder.h"
#include "exchange-types.h"

G_BEGIN_DECLS

#define EXCHANGE_TYPE_PERMISSIONS_DIALOG            (exchange_permissions_dialog_get_type ())
#define EXCHANGE_PERMISSIONS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXCHANGE_TYPE_PERMISSIONS_DIALOG, ExchangePermissionsDialog))
#define EXCHANGE_PERMISSIONS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXCHANGE_TYPE_PERMISSIONS_DIALOG, ExchangePermissionsDialogClass))
#define EXCHANGE_IS_PERMISSIONS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXCHANGE_TYPE_PERMISSIONS_DIALOG))
#define EXCHANGE_IS_PERMISSIONS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EXCHANGE_TYPE_PERMISSIONS_DIALOG))

struct _ExchangePermissionsDialog {
	GtkDialog parent;

	ExchangePermissionsDialogPrivate *priv;
};

struct _ExchangePermissionsDialogClass {
	GtkDialogClass parent_class;

};

GType      exchange_permissions_dialog_get_type (void);

void       exchange_permissions_dialog_new      (ExchangeAccount *account,
						 EFolder         *folder,
						 GtkWidget       *parent);

G_END_DECLS

#endif /* __EXCHANGE_PERMISSIONS_DIALOG_H__ */
