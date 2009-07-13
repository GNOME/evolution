/*
 *
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
 *		Raghavendran R <raghavguru7@gmail.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EXCHANGE_SENDOPTIONS_DIALOG_H__
#define __EXCHANGE_SENDOPTIONS_DIALOG_H__

#include <gtk/gtk.h>

#define EXCHANGE_TYPE_SENDOPTIONS_DIALOG       (exchange_sendoptions_dialog_get_type ())
#define EXCHANGE_SENDOPTIONS_DIALOG(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXCHANGE_TYPE_SENDOPTIONS_DIALOG, ExchangeSendOptionsDialog))
#define EXCHANGE_SENDOPTIONS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), EXCHANGE_TYPE_SENDOPTIONS_DIALOG, ExchangeSendOptionsDialogClass))
#define EXCHANGE_IS_SENDOPTIONS_DIALOG(obj)    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXCHANGE_TYPE_SENDOPTIONS_DIALOG))
#define EXCHANGE_IS_SENDOPTIONS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EXCHANGE_TYPE_SENDOPTIONS_DIALOG))

typedef struct _ExchangeSendOptionsDialog		ExchangeSendOptionsDialog;
typedef struct _ExchangeSendOptionsDialogClass		ExchangeSendOptionsDialogClass;
typedef struct _ExchangeSendOptionsDialogPrivate	ExchangeSendOptionsDialogPrivate;

typedef enum {
	E_IMP_NORMAL,
	E_IMP_HIGH,
	E_IMP_LOW
} ExchangeSendOptionsImp;

typedef enum {
	E_SENSITIVITY_NORMAL,
	E_SENSITIVITY_PERSONAL,
	E_SENSITIVITY_PRIVATE,
	E_SENSITIVITY_CONFIDENTIAL
} ExchangeSendOptionsSensitivity;

/* We require the delegate_email and delegate_name to store the address of the delegator selected into
   the destination store.
*/
typedef struct {
	ExchangeSendOptionsImp importance;
	ExchangeSendOptionsSensitivity sensitivity;
	gboolean send_as_del_enabled;
	gboolean delivery_enabled;
	gboolean read_enabled;
	const gchar *delegate_name;
	const gchar *delegate_email;
	const gchar *delegate_address;
} ExchangeSendOptions;

struct _ExchangeSendOptionsDialog {
	GObject object;

	ExchangeSendOptions *options;
	/* Private data */
	ExchangeSendOptionsDialogPrivate *priv;
};

struct _ExchangeSendOptionsDialogClass {
	GObjectClass parent_class;
	void (* esod_response) (ExchangeSendOptionsDialog *esd, gint status);
};

GType  exchange_sendoptions_dialog_get_type     (void);
ExchangeSendOptionsDialog *exchange_sendoptions_dialog_new (void);
gboolean exchange_sendoptions_dialog_run (ExchangeSendOptionsDialog *sod, GtkWidget *parent);
#endif

