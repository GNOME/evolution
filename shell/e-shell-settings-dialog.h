/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-settings-dialog.h
 *
 * Copyright (C) 2002  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#ifndef _E_SHELL_SETTINGS_DIALOG_H_
#define _E_SHELL_SETTINGS_DIALOG_H_

#include "e-multi-config-dialog.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_SHELL_SETTINGS_DIALOG			(e_shell_settings_dialog_get_type ())
#define E_SHELL_SETTINGS_DIALOG(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_SHELL_SETTINGS_DIALOG, EShellSettingsDialog))
#define E_SHELL_SETTINGS_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_SETTINGS_DIALOG, EShellSettingsDialogClass))
#define E_IS_SHELL_SETTINGS_DIALOG(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_SHELL_SETTINGS_DIALOG))
#define E_IS_SHELL_SETTINGS_DIALOG_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_SETTINGS_DIALOG))


typedef struct _EShellSettingsDialog        EShellSettingsDialog;
typedef struct _EShellSettingsDialogPrivate EShellSettingsDialogPrivate;
typedef struct _EShellSettingsDialogClass   EShellSettingsDialogClass;

struct _EShellSettingsDialog {
	EMultiConfigDialog parent;

	EShellSettingsDialogPrivate *priv;
};

struct _EShellSettingsDialogClass {
	EMultiConfigDialogClass parent_class;
};


GtkType    e_shell_settings_dialog_get_type  (void);
GtkWidget *e_shell_settings_dialog_new       (void);
void       e_shell_settings_dialog_show_type (EShellSettingsDialog *dialog,
					      const char           *type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_SHELL_SETTINGS_DIALOG_H_ */
