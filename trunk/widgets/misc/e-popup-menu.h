/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-popup-menu.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Miguel de Icaza <miguel@ximian.com>
 *   Jody Goldberg (jgoldberg@home.com)
 *   Jeffrey Stedfast <fejj@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef E_POPUP_MENU_H
#define E_POPUP_MENU_H

#include <gtk/gtkmenu.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define E_POPUP_SEPARATOR  { "", NULL, (NULL), NULL, 0 }
#define E_POPUP_TERMINATOR { NULL, NULL, (NULL), NULL, 0 }


/* In the following, CC = custom closure */

#define E_POPUP_ITEM(name,fn,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), NULL, NULL, 0, 0, 0, 0 }
#define E_POPUP_ITEM_CC(name,fn,closure,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), NULL, (closure), 0, 0, 0, 1 }
#define E_POPUP_SUBMENU(name,submenu,disable_mask) { (name), NULL, NULL, (submenu), (disable_mask), NULL, NULL, 0, 0, 0, 0 }

#define E_POPUP_PIXMAP_ITEM(name,pixmap,fn,disable_mask) { (name), (pixmap), (fn), NULL, (disable_mask), NULL, NULL, 0, 0, 0, 0 }
#define E_POPUP_PIXMAP_ITEM_CC(name,pixmap,fn,closure,disable_mask) { (name), (pixmap), (fn), NULL, (disable_mask), NULL, (closure), 0, 0, 0, 1 }
#define E_POPUP_PIXMAP_SUBMENU(name,pixmap,submenu,disable_mask) { (name), (pixmap), NULL, (submenu), (disable_mask), NULL, NULL, 0, 0, 0, 0 }

#define E_POPUP_PIXMAP_WIDGET_ITEM(name,pixmap_widget,fn,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), (pixmap_widget), NULL, 0, 0, 0, 0 }
#define E_POPUP_PIXMAP_WIDGET_ITEM_CC(name,pixmap_widget,fn,closure,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), (pixmap_widget), (closure), 0, 0, 0, 1 }
#define E_POPUP_PIXMAP_WIDGET_SUBMENU(name,pixmap_widget,submenu,disable_mask) { (name), NULL, NULL, (submenu), (disable_mask), (pixmap_widget), NULL, 0, 0, 0, 0 }


#define E_POPUP_TOGGLE_ITEM(name,fn,disable_mask,value) { (name), NULL, (fn), NULL, (disable_mask), NULL, NULL, 1, 0, value, 0 }
#define E_POPUP_TOGGLE_ITEM_CC(name,fn,closure,disable_mask,value) { (name), NULL, (fn), NULL, (disable_mask), NULL, (closure), 1, 0, value, 1 }

#define E_POPUP_SEPARATOR  { "", NULL, (NULL), NULL, 0 }
#define E_POPUP_TERMINATOR { NULL, NULL, (NULL), NULL, 0 }


/* In the following, CC = custom closure */

#define E_POPUP_ITEM(name,fn,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), NULL, NULL, 0, 0, 0, 0 }
#define E_POPUP_ITEM_CC(name,fn,closure,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), NULL, (closure), 0, 0, 0, 1 }
#define E_POPUP_SUBMENU(name,submenu,disable_mask) { (name), NULL, NULL, (submenu), (disable_mask), NULL, NULL, 0, 0, 0, 0 }

#define E_POPUP_PIXMAP_ITEM(name,pixmap,fn,disable_mask) { (name), (pixmap), (fn), NULL, (disable_mask), NULL, NULL, 0, 0, 0, 0 }
#define E_POPUP_PIXMAP_ITEM_CC(name,pixmap,fn,closure,disable_mask) { (name), (pixmap), (fn), NULL, (disable_mask), NULL, (closure), 0, 0, 0, 1 }
#define E_POPUP_PIXMAP_SUBMENU(name,pixmap,submenu,disable_mask) { (name), (pixmap), NULL, (submenu), (disable_mask), NULL, NULL, 0, 0, 0, 0 }

#define E_POPUP_PIXMAP_WIDGET_ITEM(name,pixmap_widget,fn,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), (pixmap_widget), NULL, 0, 0, 0, 0 }
#define E_POPUP_PIXMAP_WIDGET_ITEM_CC(name,pixmap_widget,fn,closure,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), (pixmap_widget), (closure), 0, 0, 0, 1 }
#define E_POPUP_PIXMAP_WIDGET_SUBMENU(name,pixmap_widget,submenu,disable_mask) { (name), NULL, NULL, (submenu), (disable_mask), (pixmap_widget), NULL, 0, 0, 0, 0 }


#define E_POPUP_TOGGLE_ITEM(name,fn,disable_mask,value) { (name), NULL, (fn), NULL, (disable_mask), NULL, NULL, 1, 0, value, 0 }
#define E_POPUP_TOGGLE_ITEM_CC(name,fn,closure,disable_mask,value) { (name), NULL, (fn), NULL, (disable_mask), NULL, (closure), 1, 0, value, 1 }

#define E_POPUP_TOGGLE_PIXMAP_ITEM(name,pixmap,fn,disable_mask) { (name), (pixmap), (fn), NULL, (disable_mask), NULL, NULL, 1, 0, value, 0 }
#define E_POPUP_TOGGLE_PIXMAP_ITEM_CC(name,pixmap,fn,closure,disable_mask) { (name), (pixmap), (fn), NULL, (disable_mask), NULL, (closure), 1, 0, value, 1 }

#define E_POPUP_TOGGLE_PIXMAP_WIDGET_ITEM(name,pixmap_widget,fn,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), (pixmap_widget), NULL, 1, 0, value, 0 }
#define E_POPUP_TOGGLE_PIXMAP_WIDGET_ITEM_CC(name,pixmap_widget,fn,closure,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), (pixmap_widget), (closure), 1, 0, value, 1 }


#define E_POPUP_RADIO_ITEM(name,fn,disable_mask,value) { (name), NULL, (fn), NULL, (disable_mask), NULL, NULL, 0, 1, value, 0 }
#define E_POPUP_RADIO_ITEM_CC(name,fn,closure,disable_mask,value) { (name), NULL, (fn), NULL, (disable_mask), NULL, (closure), 0, 1, value, 1 }

#define E_POPUP_RADIO_PIXMAP_ITEM(name,pixmap,fn,disable_mask) { (name), (pixmap), (fn), NULL, (disable_mask), NULL, NULL, 0, 1, value, 0 }
#define E_POPUP_RADIO_PIXMAP_ITEM_CC(name,pixmap,fn,closure,disable_mask) { (name), (pixmap), (fn), NULL, (disable_mask), NULL, (closure), 0, 1, value, 1 }

#define E_POPUP_RADIO_PIXMAP_WIDGET_ITEM(name,pixmap_widget,fn,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), (pixmap_widget), NULL, 0, 1, value, 0 }
#define E_POPUP_RADIO_PIXMAP_WIDGET_ITEM_CC(name,pixmap_widget,fn,closure,disable_mask) { (name), NULL, (fn), NULL, (disable_mask), (pixmap_widget), (closure), 0, 1, value, 1 }


typedef struct _EPopupMenu EPopupMenu;

struct _EPopupMenu {
	char *name;
	char *pixname;
	GtkSignalFunc fn;

	EPopupMenu *submenu;
	guint32 disable_mask;

	/* Added post 0.19 */
	GtkWidget *pixmap_widget;
	void *closure;

	guint is_toggle : 1;
	guint is_radio : 1;
	guint is_active : 1;

	guint use_custom_closure : 1;
};

GtkMenu    *e_popup_menu_create              (EPopupMenu       *menu_list,
					      guint32           disable_mask,
					      guint32           hide_mask,
					      void             *default_closure);
GtkMenu    *e_popup_menu_create_with_domain  (EPopupMenu       *menu_list,
					      guint32           disable_mask,
					      guint32           hide_mask,
					      void             *default_closure,
					      const char       *domain);
void        e_popup_menu_run                 (EPopupMenu       *menu_list,
					      GdkEvent         *event,
					      guint32           disable_mask,
					      guint32           hide_mask,
					      void             *default_closure);

/* Doesn't copy or free the memory.  Just the contents. */
void        e_popup_menu_copy_1              (EPopupMenu       *destination,
					      const EPopupMenu *menu_item);
void        e_popup_menu_free_1              (EPopupMenu       *menu_item);

/* Copies or frees the entire structure. */
EPopupMenu *e_popup_menu_copy                (const EPopupMenu *menu_item);
void        e_popup_menu_free                (EPopupMenu       *menu_item);

G_END_DECLS

#endif /* E_POPUP_MENU_H */
