/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UI_CUSTOMIZE_DIALOG_H
#define E_UI_CUSTOMIZE_DIALOG_H

#include <gtk/gtk.h>

#include <e-util/e-ui-customizer.h>

G_BEGIN_DECLS

#define E_TYPE_UI_CUSTOMIZE_DIALOG e_ui_customize_dialog_get_type ()

G_DECLARE_FINAL_TYPE (EUICustomizeDialog, e_ui_customize_dialog, E, UI_CUSTOMIZE_DIALOG, GtkDialog)

EUICustomizeDialog *
		e_ui_customize_dialog_new	(GtkWindow *parent);
void		e_ui_customize_dialog_add_customizer
						(EUICustomizeDialog *self,
						 EUICustomizer *customizer);
GPtrArray *	e_ui_customize_dialog_get_customizers
						(EUICustomizeDialog *self);
void		e_ui_customize_dialog_run	(EUICustomizeDialog *self,
						 const gchar *preselect_id);

G_END_DECLS

#endif /* E_UI_CUSTOMIZE_DIALOG_H */
