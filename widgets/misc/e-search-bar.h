/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-search-bar.h
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
#ifndef __E_SEARCH_BAR_H__
#define __E_SEARCH_BAR_H__

#include <gtk/gtkhbox.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

/* ESearchBar - A card displaying information about a contact.
 *
 * The following arguments are available:
 *
 * name		 type		read/write	description
 * ---------------------------------------------------------------------------------
 * option_choice int            RW              Which option choice is currently selected.
 * text          string         RW              Text in the entry box.
 */

#define E_SEARCH_BAR_TYPE			(e_search_bar_get_type ())
#define E_SEARCH_BAR(obj)			(GTK_CHECK_CAST ((obj), E_SEARCH_BAR_TYPE, ESearchBar))
#define E_SEARCH_BAR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_SEARCH_BAR_TYPE, ESearchBarClass))
#define E_IS_SEARCH_BAR(obj)		(GTK_CHECK_TYPE ((obj), E_SEARCH_BAR_TYPE))
#define E_IS_SEARCH_BAR_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_SEARCH_BAR_TYPE))

typedef struct {
	char *text;
	int id;
} ESearchBarItem;

typedef struct _ESearchBar       ESearchBar;
typedef struct _ESearchBarClass  ESearchBarClass;

struct _ESearchBar
{
	GtkHBox parent;
	
	/* item specific fields */
	GtkWidget *dropdown;
	GtkWidget *option;
	GtkWidget *entry;

	/* PRIVATE */
	GtkWidget *dropdown_holder;	/* holds the dropdown */
	GtkWidget *option_menu;
	GtkWidget *dropdown_menu;

	int        option_choice;
};

struct _ESearchBarClass
{
	GtkHBoxClass parent_class;

	void (*set_menu)       (ESearchBar *, ESearchBarItem *);
	void (*set_option)     (ESearchBar *, ESearchBarItem *);

	void (*query_changed)  (ESearchBar *search);
	void (*menu_activated) (ESearchBar *search, int item);
};


GtkType    e_search_bar_get_type   (void);
void       e_search_bar_set_menu   (ESearchBar *search_bar, ESearchBarItem *menu_items);
void	   e_search_bar_add_menu   (ESearchBar *search_bar, ESearchBarItem *menu_item);

void       e_search_bar_set_option (ESearchBar *search_bar, ESearchBarItem *option_items);
void       e_search_bar_construct  (ESearchBar     *search_bar,
				    ESearchBarItem *menu_items,
				    ESearchBarItem *option_items);
GtkWidget *e_search_bar_new        (ESearchBarItem *menu_items,
				    ESearchBarItem *option_items);

void       e_search_bar_set_menu_sensitive(ESearchBar *search_bar, int id, gboolean state);

int        e_search_bar_get_option_choice (ESearchBar *search_bar);
char      *e_search_bar_get_text          (ESearchBar *search_bar);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __E_SEARCH_BAR_H__ */
