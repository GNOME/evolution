/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * An auto-completer for addresses from the address book.
 *
 * Author:
 *   Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 2001 Ximian, Inc.
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef E_ADDRESS_COMPLETION_H
#define E_ADDRESS_COMPLETION_H

#include "e-book.h"
#include <gal/e-text/e-completion.h>

BEGIN_GNOME_DECLS

#define E_ADDRESS_COMPLETION_TYPE        (e_address_completion_get_type ())
#define E_ADDRESS_COMPLETION(o)          (GTK_CHECK_CAST ((o), E_ADDRESS_COMPLETION_TYPE, EAddressCompletion))
#define E_ADDRESS_COMPLETION_CLASS(k)    (GTK_CHECK_CLASS_CAST ((k), E_ADDRESS_COMPLETION_TYPE, EAddressCompletionClass))
#define E_IS_ADDRESS_COMPLETION(o)       (GTK_CHECK_TYPE ((o), E_ADDRESS_COMPLETION_TYPE))
#define E_IS_ADDRESS_COMPLETION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_ADDRESS_COMPLETION_TYPE))

typedef struct _EAddressCompletion EAddressCompletion;
typedef struct _EAddressCompletionClass EAddressCompletionClass;

struct _EAddressCompletion {
	ECompletion parent;

	EBook *book;
	guint seq_no;
};

struct _EAddressCompletionClass {
	ECompletionClass parent_class;
};

GtkType      e_address_completion_get_type  (void);

void         e_address_completion_construct (EAddressCompletion *addr_comp, EBook *book);
ECompletion *e_address_completion_new       (EBook *book);




#endif /* E_ADDRESS_COMPLETION_H */

