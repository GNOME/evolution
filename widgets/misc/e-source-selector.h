/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-source-selector.h
 *
 * Copyright (C) 2003  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
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
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_SOURCE_SELECTOR_H_
#define _E_SOURCE_SELECTOR_H_

#include <gtk/gtkmenu.h>
#include <gtk/gtktreeview.h>
#include <libedataserver/e-source-list.h>

#define E_TYPE_SOURCE_SELECTOR		(e_source_selector_get_type ())
#define E_SOURCE_SELECTOR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_SOURCE_SELECTOR, ESourceSelector))
#define E_SOURCE_SELECTOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_SOURCE_SELECTOR, ESourceSelectorClass))
#define E_IS_SOURCE_SELECTOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_SOURCE_SELECTOR))
#define E_IS_SOURCE_SELECTOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_SOURCE_SELECTOR))


typedef struct _ESourceSelector        ESourceSelector;
typedef struct _ESourceSelectorPrivate ESourceSelectorPrivate;
typedef struct _ESourceSelectorClass   ESourceSelectorClass;

struct _ESourceSelector {
	GtkTreeView parent;

	ESourceSelectorPrivate *priv;
};

struct _ESourceSelectorClass {
	GtkTreeViewClass parent_class;

	void (* selection_changed) (ESourceSelector *selector);
	void (* primary_selection_changed) (ESourceSelector *selector);
	void (* fill_popup_menu) (ESourceSelector *selector, GtkMenu *menu);
};


GType  e_source_selector_get_type  (void);

GtkWidget *e_source_selector_new  (ESourceList *list);

void      e_source_selector_select_source       (ESourceSelector *selector,
						 ESource         *source);
void      e_source_selector_unselect_source     (ESourceSelector *selector,
						 ESource         *source);
gboolean  e_source_selector_source_is_selected  (ESourceSelector *selector,
						 ESource         *source);

GSList *e_source_selector_get_selection   (ESourceSelector *selector);
void    e_source_selector_free_selection  (GSList          *list);

void      e_source_selector_show_selection   (ESourceSelector *selector,
					      gboolean         show);
gboolean  e_source_selector_selection_shown  (ESourceSelector *selector);

void     e_source_selector_set_toggle_selection(ESourceSelector *selector, gboolean state);

void e_source_selector_set_select_new (ESourceSelector *selector, gboolean state);

ESource *e_source_selector_peek_primary_selection  (ESourceSelector *selector);
void     e_source_selector_set_primary_selection   (ESourceSelector *selector,
						    ESource         *source);


#endif /* _E_SOURCE_SELECTOR_H_ */
