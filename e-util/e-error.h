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

#ifndef _E_ERROR_H
#define _E_ERROR_H

#include <stdarg.h>
#include <gtk/gtk.h>

/*
 * Some standard errors, if these are altered or added to,
 * update devel-docs/misc/errors.txt
 *
 * Several more basic ones are needed.
 */

#define E_ERROR_INFO "builtin:info"
#define E_ERROR_INFO_PRIMARY "builtin:info-primary"
#define E_ERROR_WARNING "builtin:warning"
#define E_ERROR_WARNING_PRIMARY "builtin:warning-primary"
#define E_ERROR_ERROR "builtin:error"
#define E_ERROR_ERROR_PRIMARY "builtin:error-primary"

/* takes filename, returns OK if yes */
#define E_ERROR_ASK_FILE_EXISTS_OVERWRITE "system:ask-save-file-exists-overwrite"
/* takes filename, reason */
#define E_ERROR_NO_SAVE_FILE "system:no-save-file"
/* takes filename, reason */
#define E_ERROR_NO_LOAD_FILE "system:no-save-file"

/* Note that all errors returned are standard GtkDialoge's */
GtkWidget *e_error_new(GtkWindow *parent, const gchar *tag, const gchar *arg0, ...);
GtkWidget *e_error_newv(GtkWindow *parent, const gchar *tag, const gchar *arg0, va_list ap);

gint e_error_run(GtkWindow *parent, const gchar *tag, const gchar *arg0, ...);
gint e_error_runv(GtkWindow *parent, const gchar *tag, const gchar *arg0, va_list ap);

guint e_error_count_buttons (GtkDialog *dialog);

void e_error_default_parent(GtkWindow *parent);

#endif /* !_E_ERROR_H */
