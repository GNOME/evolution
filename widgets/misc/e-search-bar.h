/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-search-bar.h
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
#ifndef __E_SEARCH_BAR_H__
#define __E_SEARCH_BAR_H__

#include <gtk/gtkhbox.h>

#include <bonobo/bonobo-ui-component.h>

G_BEGIN_DECLS

/* ESearchBar - A card displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		 type		read/write	description
 * ---------------------------------------------------------------------------------
 * item_id       int            RW              Which option item is currently selected.
 * subitem_id    int            RW              Which option subitem is currently selected.
 * text          string         RW              Text in the entry box.
 */

#define E_SEARCH_BAR_TYPE		(e_search_bar_get_type ())
#define E_SEARCH_BAR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_SEARCH_BAR_TYPE, ESearchBar))
#define E_SEARCH_BAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_SEARCH_BAR_TYPE, ESearchBarClass))
#define E_IS_SEARCH_BAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_SEARCH_BAR_TYPE))
#define E_IS_SEARCH_BAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_SEARCH_BAR_TYPE))

typedef struct {
	char *text;
	int id;
	gboolean translate; /* whether to translate the text */
} ESearchBarSubitem;
	
typedef struct {
	char *text;
	int id;
	ESearchBarSubitem *subitems;
} ESearchBarItem;

typedef struct _ESearchBar       ESearchBar;
typedef struct _ESearchBarClass  ESearchBarClass;

struct _ESearchBar
{
	GtkHBox parent;

	BonoboUIComponent *ui_component;

	GSList *menu_items;
	
	/* item specific fields */
	GtkWidget *option;
	GtkWidget *entry;
	GtkWidget *suboption; /* an option menu for the choices associated with some options */
	
	/* PRIVATE */
	GtkWidget *dropdown_holder;	/* holds the dropdown */
	GtkWidget *option_menu;
	GtkWidget *suboption_menu;
	GtkWidget *activate_button;
	GtkWidget *clear_button;
	GtkWidget *entry_box;
	guint      pending_activate;

	/* The currently-selected item & subitem */
	int        item_id;
	int        subitem_id; /* < 0 if the entry widget is active */
};

struct _ESearchBarClass
{
	GtkHBoxClass parent_class;

	void (*set_menu)         (ESearchBar *, ESearchBarItem *);
	void (*set_option)       (ESearchBar *, ESearchBarItem *);

	/* signals */
	void (*search_activated) (ESearchBar *search);
	void (*query_changed)    (ESearchBar *search);
	void (*menu_activated)   (ESearchBar *search, int item);
};

enum {
	E_SEARCHBAR_FIND_NOW_ID = -1,
	E_SEARCHBAR_CLEAR_ID    = -2
};


GType      e_search_bar_get_type   (void);
void       e_search_bar_construct  (ESearchBar        *search_bar,
				    ESearchBarItem    *menu_items,
				    ESearchBarItem    *option_items);
GtkWidget *e_search_bar_new        (ESearchBarItem    *menu_items,
				    ESearchBarItem    *option_items);

void  e_search_bar_set_ui_component  (ESearchBar        *search_bar,
				      BonoboUIComponent *ui_component);

void  e_search_bar_set_menu  (ESearchBar     *search_bar,
			      ESearchBarItem *menu_items);
void  e_search_bar_add_menu  (ESearchBar     *search_bar,
			      ESearchBarItem *menu_item);

void  e_search_bar_set_option     (ESearchBar        *search_bar,
				   ESearchBarItem    *option_items);
void  e_search_bar_set_suboption  (ESearchBar        *search_bar,
				   int                option_id,
				   ESearchBarSubitem *subitems);

void  e_search_bar_set_menu_sensitive  (ESearchBar *search_bar,
					int         id,
					gboolean    state);

void  e_search_bar_set_item_id  (ESearchBar *search_bar,
				 int         id);
int   e_search_bar_get_item_id  (ESearchBar *search_bar);

void  e_search_bar_set_subitem_id  (ESearchBar *search_bar,
				    int         id);
int   e_search_bar_get_subitem_id  (ESearchBar *search_bar);

void  e_search_bar_set_ids  (ESearchBar *search_bar,
			     int         item_id,
			     int         subitem_id);

void  e_search_bar_set_text  (ESearchBar *search_bar,
			      const char *text);
char *e_search_bar_get_text  (ESearchBar *search_bar);

G_END_DECLS


#endif /* __E_SEARCH_BAR_H__ */
