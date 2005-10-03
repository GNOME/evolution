/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-print.h
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef E_CONTACT_PRINT_H
#define E_CONTACT_PRINT_H

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include "e-contact-print-types.h"

GtkWidget *e_contact_print_dialog_new (EBook *book, char *query, GList *list);
void e_contact_print_preview (EBook *book, char *query, GList *list);
GtkWidget *e_contact_print_contact_dialog_new (EContact *card);
GtkWidget *e_contact_print_contact_list_dialog_new(GList *list);

#endif /* E_CONTACT_PRINT_H */
