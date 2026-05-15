/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Ashish <shashish@novell.com>
 */

#ifndef EMAIL_CUSTOM_HEADEROPTIONS_DIALOG_H
#define EMAIL_CUSTOM_HEADEROPTIONS_DIALOG_H

#include <gtk/gtk.h>

#define E_TYPE_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG \
	(custom_header_options_dialog_get_type ())
#define E_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG, CustomHeaderOptionsDialog))
#define E_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG, CustomHeaderOptionsDialogClass))
#define E_IS_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG))
#define E_IS_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_CUSTOM_HEADER_OPTIONS_DIALOG))

typedef struct _CustomHeaderOptionsDialog		CustomHeaderOptionsDialog;
typedef struct _CustomHeaderOptionsDialogClass		CustomHeaderOptionsDialogClass;
typedef struct _CustomHeaderOptionsDialogPrivate	CustomHeaderOptionsDialogPrivate;

struct _CustomHeaderOptionsDialog {
	GObject parent;
	CustomHeaderOptionsDialogPrivate *priv;
};

typedef struct {
	gint number_of_header;
	gint number_of_subtype_header;
	GString *header_type_value;
	GArray *sub_header_type_value;
} EmailCustomHeaderDetails;

typedef struct {
	GString *sub_header_string_value;
} CustomSubHeader;

typedef struct {
	GtkWidget *header_value_combo_box;
} HeaderValueComboBox;

struct _CustomHeaderOptionsDialogClass {
	GObjectClass parent_class;
	void (* emch_response) (CustomHeaderOptionsDialog *esd, gint status);
};

enum {
        MCH_RESPONSE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

CustomHeaderOptionsDialog *epech_dialog_new (void);
static gboolean epech_dialog_run (CustomHeaderOptionsDialog *mch, GtkWidget *parent);
static void epech_load_from_settings (GSettings *settings, const gchar *path, CustomHeaderOptionsDialog *mch);
#endif
