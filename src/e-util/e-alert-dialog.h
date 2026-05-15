/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-FileCopyrightText: (C) 2009 Intel Corporation
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Michael Zucchi <notzed@ximian.com>
 * SPDX-FileContributor: Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_ALERT_DIALOG_H
#define E_ALERT_DIALOG_H

#include <gtk/gtk.h>

#include <e-util/e-alert.h>

/* Standard GObject macros */
#define E_TYPE_ALERT_DIALOG \
	(e_alert_dialog_get_type ())
#define E_ALERT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ALERT_DIALOG, EAlertDialog))
#define E_ALERT_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ALERT_DIALOG, EAlertDialogClass))
#define E_IS_ALERT_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ALERT_DIALOG))
#define E_IS_ALERT_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ALERT_DIALOG))
#define E_ALERT_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ALERT_DIALOG, EAlertDialogClass))

G_BEGIN_DECLS

typedef struct _EAlertDialog EAlertDialog;
typedef struct _EAlertDialogClass EAlertDialogClass;
typedef struct _EAlertDialogPrivate EAlertDialogPrivate;

struct _EAlertDialog {
	GtkDialog parent;
	EAlertDialogPrivate *priv;
};

struct _EAlertDialogClass {
	GtkDialogClass parent_class;
};

GType		e_alert_dialog_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_alert_dialog_new		(GtkWindow *parent,
						 EAlert *alert);
GtkWidget *	e_alert_dialog_new_for_args	(GtkWindow *parent,
						 const gchar *tag,
						 ...) G_GNUC_NULL_TERMINATED;
gint		e_alert_run_dialog		(GtkWindow *parent,
						 EAlert *alert);
gint		e_alert_run_dialog_for_args	(GtkWindow *parent,
						 const gchar *tag,
						 ...) G_GNUC_NULL_TERMINATED;
EAlert *	e_alert_dialog_get_alert	(EAlertDialog *dialog);
GtkWidget *	e_alert_dialog_get_content_area	(EAlertDialog *dialog);

G_END_DECLS

#endif /* E_ALERT_DIALOG_H */
