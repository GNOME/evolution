/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-save-as.h
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
#ifndef __E_CONTACT_SAVE_AS_H__
#define __E_CONTACT_SAVE_AS_H__

#include <gtk/gtkwindow.h>
#include <glade/glade.h>
#include "addressbook/backend/ebook/e-card.h"
#include "addressbook/backend/ebook/e-card-simple.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

void e_contact_save_as      (gchar *title, ECard *card, GtkWindow *parent_window);
void e_contact_list_save_as (gchar *title, GList *list, GtkWindow *parent_window);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_CONTACT_EDITOR_H__ */
