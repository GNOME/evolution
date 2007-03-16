/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 2001-2004 Novell, Inc. */

#ifndef __EXCHANGE_PERMISSIONS_DIALOG_H__
#define __EXCHANGE_PERMISSIONS_DIALOG_H__

#include <gtk/gtkdialog.h>
#include "e-folder.h"
#include "exchange-types.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EXCHANGE_PERMISSIONS_DIALOG_H__ */
