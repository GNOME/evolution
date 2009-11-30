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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_ALERT_H
#define _E_ALERT_H

#include <stdarg.h>
#include <gtk/gtk.h>

/*
 * Some standard alerts, if these are altered or added to,
 * update devel-docs/misc/errors.txt
 *
 * Several more basic ones are needed.
 */

#define E_ALERT_INFO "builtin:info"
#define E_ALERT_INFO_PRIMARY "builtin:info-primary"
#define E_ALERT_WARNING "builtin:warning"
#define E_ALERT_WARNING_PRIMARY "builtin:warning-primary"
#define E_ALERT_ERROR "builtin:error"
#define E_ALERT_ERROR_PRIMARY "builtin:error-primary"

/* takes filename, returns OK if yes */
#define E_ALERT_ASK_FILE_EXISTS_OVERWRITE "system:ask-save-file-exists-overwrite"
/* takes filename, reason */
#define E_ALERT_NO_SAVE_FILE "system:no-save-file"
/* takes filename, reason */
#define E_ALERT_NO_LOAD_FILE "system:no-save-file"

typedef struct _EAlert EAlert;

EAlert *e_alert_new(const gchar *tag, const gchar *arg0, ...);
EAlert *e_alert_newv(const gchar *tag, const gchar *arg0, va_list ap);

void e_alert_free (EAlert *alert);

/* Convenience functions for displaying the alert in a GtkDialog */
GtkWidget *e_alert_new_dialog(GtkWindow *parent, EAlert *alert);
GtkWidget *e_alert_new_dialog_for_args (GtkWindow *parent, const gchar *tag, const gchar *arg0, ...);

gint e_alert_run_dialog(GtkWindow *parent, EAlert *alert);
gint e_alert_run_dialog_for_args (GtkWindow *parent, const gchar *tag, const gchar *arg0, ...);

guint e_alert_dialog_count_buttons (GtkDialog *dialog);

#endif /* !_E_ALERT_H */
