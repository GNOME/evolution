/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-addressbook-view.h
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
#ifndef __E_ADDRESSBOOK_VIEW_H__
#define __E_ADDRESSBOOK_VIEW_H__

#include <gnome.h>
#include <bonobo.h>
#include "addressbook/backend/ebook/e-book.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* EAddressbookView - A card displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 */

#define E_ADDRESSBOOK_VIEW_TYPE			(e_addressbook_view_get_type ())
#define E_ADDRESSBOOK_VIEW(obj)			(GTK_CHECK_CAST ((obj), E_ADDRESSBOOK_VIEW_TYPE, EAddressbookView))
#define E_ADDRESSBOOK_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_ADDRESSBOOK_VIEW_TYPE, EAddressbookViewClass))
#define E_IS_ADDRESSBOOK_VIEW(obj)		(GTK_CHECK_TYPE ((obj), E_ADDRESSBOOK_VIEW_TYPE))
#define E_IS_ADDRESSBOOK_VIEW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_ADDRESSBOOK_VIEW_TYPE))

typedef enum {
	E_ADDRESSBOOK_VIEW_NONE, /* initialized to this */
	E_ADDRESSBOOK_VIEW_TABLE,
	E_ADDRESSBOOK_VIEW_MINICARD
} EAddressbookViewType;


typedef struct _EAddressbookView       EAddressbookView;
typedef struct _EAddressbookViewClass  EAddressbookViewClass;

struct _EAddressbookView
{
	GtkTable parent;
	
	/* item specific fields */
	EAddressbookViewType view_type;

	EBook *book;
	char  *query;
	guint editable : 1;

	GtkObject *object;
	GtkWidget *widget;

	GtkWidget *vbox;
};

struct _EAddressbookViewClass
{
	GtkTableClass parent_class;

	/*
	 * Signals
	 */
	void (*status_message) (EAddressbookView *view, const gchar *message);
};

GtkWidget *e_addressbook_view_new               (void);
GtkType    e_addressbook_view_get_type          (void);

void       e_addressbook_view_setup_menus       (EAddressbookView  *view,
						 BonoboUIComponent *uic);

void       e_addressbook_view_print             (EAddressbookView  *view);
void       e_addressbook_view_delete_selection  (EAddressbookView  *view);
void       e_addressbook_view_show_all          (EAddressbookView  *view);
void       e_addressbook_view_stop              (EAddressbookView  *view);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_ADDRESSBOOK_VIEW_H__ */
