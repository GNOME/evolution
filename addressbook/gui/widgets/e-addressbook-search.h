/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-addressbook-search.h
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#ifndef __E_ADDRESSBOOK_SEARCH_H__
#define __E_ADDRESSBOOK_SEARCH_H__

#include <gnome.h>
#include "addressbook/backend/ebook/e-book.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EAddressbookSearch - A card displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_ADDRESSBOOK_SEARCH_TYPE			(e_addressbook_search_get_type ())
#define E_ADDRESSBOOK_SEARCH(obj)			(GTK_CHECK_CAST ((obj), E_ADDRESSBOOK_SEARCH_TYPE, EAddressbookSearch))
#define E_ADDRESSBOOK_SEARCH_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_ADDRESSBOOK_SEARCH_TYPE, EAddressbookSearchClass))
#define E_IS_ADDRESSBOOK_SEARCH(obj)		(GTK_CHECK_TYPE ((obj), E_ADDRESSBOOK_SEARCH_TYPE))
#define E_IS_ADDRESSBOOK_SEARCH_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_ADDRESSBOOK_SEARCH_TYPE))

typedef enum {
	E_ADDRESSBOOK_SEARCH_NONE, /* initialized to this */
	E_ADDRESSBOOK_SEARCH_TABLE,
	E_ADDRESSBOOK_SEARCH_MINICARD
} EAddressbookSearchType;


typedef struct _EAddressbookSearch       EAddressbookSearch;
typedef struct _EAddressbookSearchClass  EAddressbookSearchClass;

struct _EAddressbookSearch
{
	GtkHBox parent;
	
	/* item specific fields */
	GtkWidget *entry;
	GtkWidget *option;
	int        option_choice;
};

struct _EAddressbookSearchClass
{
	GtkHBoxClass parent_class;

	void (*query_changed) (EAddressbookSearch *search);
};

GtkWidget *e_addressbook_search_new        (void);
GtkType    e_addressbook_search_get_type   (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_ADDRESSBOOK_SEARCH_H__ */
