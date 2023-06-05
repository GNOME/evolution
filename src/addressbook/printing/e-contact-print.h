/*
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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
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
