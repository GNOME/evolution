/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-addressbook-view.h
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
#ifndef __E_ADDRESSBOOK_VIEW_H__
#define __E_ADDRESSBOOK_VIEW_H__

#include <gtk/gtktable.h>
#include <bonobo/bonobo-ui-component.h>
#include <gal/menus/gal-view-instance.h>
#include "e-addressbook-model.h"
#include "widgets/menus/gal-view-menus.h"
#include "addressbook/backend/ebook/e-book.h"
#include <gal/unicode/gunicode.h>

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

	EAddressbookModel   *model;

	GtkWidget *invisible;
	GList *clipboard_cards;

	EBook *book;
	char  *query;
	guint editable : 1;

	GtkObject *object;
	GtkWidget *widget;
	GtkWidget *current_alphabet_widget;

	GtkWidget *vbox;

	/* Menus handler and the view instance */
	GalViewInstance *view_instance;
	GalViewMenus *view_menus;
	GalView *current_view;
	BonoboUIComponent *uic;
};

struct _EAddressbookViewClass
{
	GtkTableClass parent_class;

	/*
	 * Signals
	 */
	void (*status_message)        (EAddressbookView *view, const gchar *message);
	void (*folder_bar_message)    (EAddressbookView *view, const gchar *message);
	void (*command_state_change)  (EAddressbookView *view);
	void (*alphabet_state_change) (EAddressbookView *view, gunichar letter);
};

GtkWidget *e_addressbook_view_new                 (void);
GtkType    e_addressbook_view_get_type            (void);

void       e_addressbook_view_setup_menus         (EAddressbookView  *view,
						   BonoboUIComponent *uic);

void       e_addressbook_view_discard_menus       (EAddressbookView  *view);

void       e_addressbook_view_save_as             (EAddressbookView  *view);
void       e_addressbook_view_view                (EAddressbookView  *view);
void       e_addressbook_view_send                (EAddressbookView  *view);
void       e_addressbook_view_send_to             (EAddressbookView  *view);
void       e_addressbook_view_print               (EAddressbookView  *view);
void       e_addressbook_view_print_preview       (EAddressbookView  *view);
void       e_addressbook_view_delete_selection    (EAddressbookView  *view);
void       e_addressbook_view_cut                 (EAddressbookView  *view);
void       e_addressbook_view_copy                (EAddressbookView  *view);
void       e_addressbook_view_paste               (EAddressbookView  *view);
void       e_addressbook_view_select_all          (EAddressbookView  *view);
void       e_addressbook_view_show_all            (EAddressbookView  *view);
void       e_addressbook_view_stop                (EAddressbookView  *view);
void       e_addressbook_view_copy_to_folder      (EAddressbookView  *view);
void       e_addressbook_view_move_to_folder      (EAddressbookView  *view);

gboolean   e_addressbook_view_can_create          (EAddressbookView  *view);
gboolean   e_addressbook_view_can_print           (EAddressbookView  *view);
gboolean   e_addressbook_view_can_save_as         (EAddressbookView  *view);
gboolean   e_addressbook_view_can_view            (EAddressbookView  *view);
gboolean   e_addressbook_view_can_send            (EAddressbookView  *view);
gboolean   e_addressbook_view_can_send_to         (EAddressbookView  *view);
gboolean   e_addressbook_view_can_delete          (EAddressbookView  *view);
gboolean   e_addressbook_view_can_cut             (EAddressbookView  *view);
gboolean   e_addressbook_view_can_copy            (EAddressbookView  *view);
gboolean   e_addressbook_view_can_paste           (EAddressbookView  *view);
gboolean   e_addressbook_view_can_select_all      (EAddressbookView  *view);
gboolean   e_addressbook_view_can_stop            (EAddressbookView  *view);
gboolean   e_addressbook_view_can_copy_to_folder  (EAddressbookView  *view);
gboolean   e_addressbook_view_can_move_to_folder  (EAddressbookView  *view);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_ADDRESSBOOK_VIEW_H__ */
