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

#define E_ERROR_WARNING "system:warning"
#define E_ERROR_WARNING_PRIMARY "system:warning-primary"
#define E_ERROR_ERROR "system:error"
#define E_ERROR_ERROR_PRIMARY "system:error-primary"

struct _GtkWidget *e_error_new(struct _GtkWindow *parent, const char *tag, const char *arg0, ...);
struct _GtkWidget *e_error_newv(struct _GtkWindow *parent, const char *tag, const char *arg0, va_list ap);

int e_error_run(struct _GtkWindow *parent, const char *tag, const char *arg0, ...);
int e_error_runv(struct _GtkWindow *parent, const char *tag, const char *arg0, va_list ap);

#endif /* !_E_ERROR_H */
