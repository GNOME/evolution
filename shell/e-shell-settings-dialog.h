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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_SHELL_SETTINGS_DIALOG_H_
#define _E_SHELL_SETTINGS_DIALOG_H_

#include "e-multi-config-dialog.h"

G_BEGIN_DECLS

#define E_TYPE_SHELL_SETTINGS_DIALOG			(e_shell_settings_dialog_get_type ())
#define E_SHELL_SETTINGS_DIALOG(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SHELL_SETTINGS_DIALOG, EShellSettingsDialog))
#define E_SHELL_SETTINGS_DIALOG_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SHELL_SETTINGS_DIALOG, EShellSettingsDialogClass))
#define E_IS_SHELL_SETTINGS_DIALOG(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SHELL_SETTINGS_DIALOG))
#define E_IS_SHELL_SETTINGS_DIALOG_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SHELL_SETTINGS_DIALOG))


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


GType      e_shell_settings_dialog_get_type  (void);
GtkWidget *e_shell_settings_dialog_new       (void);
void       e_shell_settings_dialog_show_type (EShellSettingsDialog *dialog,
					      const gchar           *type);

G_END_DECLS

#endif /* _E_SHELL_SETTINGS_DIALOG_H_ */
