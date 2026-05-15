/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: JP Rosevear <jpr@novell.com>
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
