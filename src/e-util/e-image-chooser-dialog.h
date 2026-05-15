/*
 * SPDX-FileCopyrightText: (C) 2012 Dan Vrátil <dvratil@redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_IMAGE_CHOOSER_DIALOG_H
#define E_IMAGE_CHOOSER_DIALOG_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_IMAGE_CHOOSER_DIALOG \
	(e_image_chooser_dialog_get_type ())
#define E_IMAGE_CHOOSER_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_IMAGE_CHOOSER_DIALOG, EImageChooserDialog))
#define E_IMAGE_CHOOSER_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_IMAGE_CHOOSER_DIALOG, EImageChooserDialogClass))
#define E_IS_IMAGE_CHOOSER_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_IMAGE_CHOOSER_DIALOG))
#define E_IS_IMAGE_CHOOSER_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_IMAGE_CHOOSER_DIALOG))
#define E_IMAGE_CHOOSER_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_IMAGE_CHOOSER_DIALOG, EImageChooserDialogClass))

G_BEGIN_DECLS

typedef struct _EImageChooserDialog EImageChooserDialog;
typedef struct _EImageChooserDialogClass EImageChooserDialogClass;
typedef struct _EImageChooserDialogPrivate EImageChooserDialogPrivate;

struct _EImageChooserDialog {
	GtkFileChooserDialog parent;
	EImageChooserDialogPrivate *priv;
};

struct _EImageChooserDialogClass {
	GtkFileChooserDialogClass parent_class;
};

GType		e_image_chooser_dialog_get_type
					(void) G_GNUC_CONST;
GtkWidget *	e_image_chooser_dialog_new
					(const gchar *title,
					 GtkWindow *parent);
GFile *		e_image_chooser_dialog_run
					(EImageChooserDialog *dialog);

G_END_DECLS

#endif /* E_IMAGE_CHOOSER_DIALOG_H */
