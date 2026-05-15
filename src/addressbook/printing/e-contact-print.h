/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Chris Lahey <clahey@ximian.com>
 */

#ifndef E_CONTACT_PRINT_H
#define E_CONTACT_PRINT_H

#include <gtk/gtk.h>
#include <libebook/libebook.h>

#include "e-contact-print-types.h"

void            e_contact_print               (EBookClient *book_client,
					       EBookQuery *query,
					       GPtrArray *contacts,
					       GtkPrintOperationAction action);
void		contact_page_draw_footer      (GtkPrintOperation *operation,
						GtkPrintContext *context,
						gint page_nr);

#endif /* E_CONTACT_PRINT_H */
