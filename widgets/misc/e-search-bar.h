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

struct _ESearchBar
{
	GtkHBox parent;
	ESearchBarPrivate *priv;
};

struct _ESearchBarClass
{
	GtkHBoxClass parent_class;
};

GType		e_search_bar_get_type		(void);
GtkWidget *	e_search_bar_new		(void);
GtkActionGroup *e_search_bar_get_action_group	(ESearchBar *search_bar);
RuleContext *	e_search_bar_get_context	(ESearchBar *search_bar);
void		e_search_bar_set_context	(ESearchBar *search_bar,
						 RuleContext *context);
GtkRadioAction *e_search_bar_get_filter_action	(ESearchBar *search_bar);
void		e_search_bar_set_filter_action	(ESearchBar *search_bar,
						 GtkRadioAction *action);
gint		e_search_bar_get_filter_value	(ESearchBar *search_bar);
void		e_search_bar_set_filter_value	(ESearchBar *search_bar,
						 gint value);
gboolean	e_search_bar_get_filter_visible	(ESearchBar *search_bar);
void		e_search_bar_set_filter_visible	(ESearchBar *search_bar,
						 gboolean visible);
GtkRadioAction *e_search_bar_get_search_action	(ESearchBar *search_bar);
void		e_search_bar_set_search_action	(ESearchBar *search_bar,
						 GtkRadioAction *action);
const gchar *	e_search_bar_get_search_text	(ESearchBar *search_bar);
void		e_search_bar_set_search_text	(ESearchBar *search_bar,
						 const gchar *text);
gint		e_search_bar_get_search_value	(ESearchBar *search_bar);
void		e_search_bar_set_search_value	(ESearchBar *search_bar,
						 gint value);
gboolean	e_search_bar_get_search_visible	(ESearchBar *search_bar);
void		e_search_bar_set_search_visible	(ESearchBar *search_bar,
						 gboolean visible);
GtkRadioAction *e_search_bar_get_scope_action	(ESearchBar *search_bar);
void		e_search_bar_set_scope_action	(ESearchBar *search_bar,
						 GtkRadioAction *action);
gint		e_search_bar_get_scope_value	(ESearchBar *search_bar);
void		e_search_bar_set_scope_value	(ESearchBar *search_bar,
						 gint value);
gboolean	e_search_bar_get_scope_visible	(ESearchBar *search_bar);
void		e_search_bar_set_scope_visible	(ESearchBar *search_bar,
						 gboolean visible);
void		e_search_bar_save_search_dialog	(ESearchBar *search_bar,
						 const gchar *filename);

G_END_DECLS

#endif /* E_SEARCH_BAR_H */
