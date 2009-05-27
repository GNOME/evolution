/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-search-bar.h
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_SEARCH_BAR_H
#define E_SEARCH_BAR_H

#include <gtk/gtk.h>
#include <filter/rule-context.h>

/* Standard GObject macros */
#define E_TYPE_SEARCH_BAR \
	(e_search_bar_get_type ())
#define E_SEARCH_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SEARCH_BAR, ESearchBar))
#define E_SEARCH_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SEARCH_BAR, ESearchBarClass))
#define E_IS_SEARCH_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SEARCH_BAR))
#define E_IS_SEARCH_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SEARCH_BAR))
#define E_SEARCH_BAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SEARCH_BAR, ESearchBarClass))

/* Action Names */
#define E_SEARCH_BAR_ACTION_CLEAR	"search-clear"
#define E_SEARCH_BAR_ACTION_FIND	"search-find"
#define E_SEARCH_BAR_ACTION_TYPE	"search-type"

G_BEGIN_DECLS

typedef struct _ESearchBar ESearchBar;
typedef struct _ESearchBarClass ESearchBarClass;
typedef struct _ESearchBarPrivate ESearchBarPrivate;

struct _ESearchBar {
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
	GtkWidget *option_button;
	GtkWidget *clear_button;
	GtkWidget *entry_box;
	GtkWidget *icon_entry;

	/* show option widgets */
	GtkWidget *viewoption_box;
	GtkWidget *viewoption; /* an option menu for the choices associated with some search options */
	GtkWidget *viewoption_menu;

	/* search scope widgets */
	GtkWidget *scopeoption_box;
	GtkWidget *scopeoption; /* an option menu for the choices associated with scope search */
	GtkWidget *scopeoption_menu;

	guint      pending_activate;

	/* The currently-selected item & subitem */
	gint        item_id;
	gint        viewitem_id; /* Current View Id */
	gint        scopeitem_id; /* Scope of search */
	gint        last_search_option;

	gboolean block_search;
	gboolean lite;
};

struct _ESearchBarClass {
	GtkHBoxClass parent_class;
};


GType      e_search_bar_get_type   (void);
void       e_search_bar_construct  (ESearchBar        *search_bar,
				    ESearchBarItem    *menu_items,
				    ESearchBarItem    *option_items);
GtkWidget *e_search_bar_new        (ESearchBarItem    *menu_items,
				    ESearchBarItem    *option_items);
GtkWidget *e_search_bar_lite_new   (ESearchBarItem    *menu_items,
				    ESearchBarItem    *option_items);

void  e_search_bar_set_ui_component  (ESearchBar        *search_bar,
				      BonoboUIComponent *ui_component);

void  e_search_bar_set_menu  (ESearchBar     *search_bar,
			      ESearchBarItem *menu_items);
void  e_search_bar_add_menu  (ESearchBar     *search_bar,
			      ESearchBarItem *menu_item);

void  e_search_bar_set_option     (ESearchBar        *search_bar,
				   ESearchBarItem    *option_items);
void  e_search_bar_paint (ESearchBar *search_bar);
void e_search_bar_set_viewoption (ESearchBar *search_bar,
				    gint option_id,
				    ESearchBarItem *subitems);

void  e_search_bar_set_menu_sensitive  (ESearchBar *search_bar,
					gint         id,
					gboolean    state);

void  e_search_bar_set_item_id  (ESearchBar *search_bar,
				 gint         id);
void  e_search_bar_set_item_menu (ESearchBar *search_bar,
				  gint id);
gint   e_search_bar_get_item_id  (ESearchBar *search_bar);

G_END_DECLS

#endif /* E_SEARCH_BAR_H */
