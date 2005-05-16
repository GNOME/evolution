/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- *
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _E_ERROR_H
#define _E_ERROR_H

#include <stdarg.h>

struct _GtkWindow;

/*
 * Some standard errors, if these are altered or added to,
 * update devel-docs/misc/errors.txt
 *
 * Several more basic ones are needed.
 */

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
struct _GtkWidget *e_error_new(struct _GtkWindow *parent, const char *tag, const char *arg0, ...);
struct _GtkWidget *e_error_newv(struct _GtkWindow *parent, const char *tag, const char *arg0, va_list ap);

int e_error_run(struct _GtkWindow *parent, const char *tag, const char *arg0, ...);
int e_error_runv(struct _GtkWindow *parent, const char *tag, const char *arg0, va_list ap);

void e_error_default_parent(struct _GtkWindow *parent);

#endif /* !_E_ERROR_H */
