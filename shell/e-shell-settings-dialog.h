/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-settings-dialog.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_SHELL_SETTINGS_DIALOG_H
#define E_SHELL_SETTINGS_DIALOG_H

#include "e-multi-config-dialog.h"

G_BEGIN_DECLS

/* Standard GObject macros */
#define E_TYPE_SHELL_SETTINGS_DIALOG \
	(e_shell_settings_dialog_get_type ())
#define E_SHELL_SETTINGS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_SETTINGS_DIALOG, EShellSettingsDialog))
#define E_SHELL_SETTINGS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_SETTINGS_DIALOG, EShellSettingsDialogClass))
#define E_IS_SHELL_SETTINGS_DIALOG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_SETTINGS_DIALOG))
#define E_IS_SHELL_SETTINGS_DIALOG_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_SETTINGS_DIALOG))
#define E_SHELL_SETTINGS_DIALOG_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_SETTINGS_DIALOG, EShellSettingsDialogClass))

typedef struct _EShellSettingsDialog EShellSettingsDialog;
typedef struct _EShellSettingsDialogClass EShellSettingsDialogClass;
typedef struct _EShellSettingsDialogPrivate EShellSettingsDialogPrivate;

struct _EShellSettingsDialog {
	EMultiConfigDialog parent;
	EShellSettingsDialogPrivate *priv;
};

struct _EShellSettingsDialogClass {
	EMultiConfigDialogClass parent_class;
};

GType		e_shell_settings_dialog_get_type(void);
GtkWidget *	e_shell_settings_dialog_new	(void);
void		e_shell_settings_dialog_show_type
						(EShellSettingsDialog *dialog,
						 const gchar *type);

G_END_DECLS

#endif /* E_SHELL_SETTINGS_DIALOG_H */
