/*
 *
 * Uniform print setting/dialog routines for Evolution
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		JP Rosevear <jpr@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef __E_PRINT__
#define __E_PRINT__

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkPrintOperation *	e_print_operation_new		(void);
void			e_print_run_page_setup_dialog	(GtkWindow *parent);

void			e_print_load_settings		(GtkPrintSettings **out_settings,
							 GtkPageSetup **out_page_setup);
void			e_print_save_settings		(GtkPrintSettings *settings,
							 GtkPageSetup *page_setup);

G_END_DECLS

#endif
