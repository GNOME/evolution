/*
 * e-search-bar.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_SEARCH_BAR_H
#define E_SEARCH_BAR_H

#include <gtk/gtk.h>

#include <e-util/e-web-view.h>

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

G_BEGIN_DECLS

typedef struct _ESearchBar ESearchBar;
typedef struct _ESearchBarClass ESearchBarClass;
typedef struct _ESearchBarPrivate ESearchBarPrivate;

struct _ESearchBar {
	GtkBox parent;
	ESearchBarPrivate *priv;
};

struct _ESearchBarClass {
	GtkBoxClass parent_class;

	/* Signals */
	void		(*changed)		(ESearchBar *search_bar);
	void		(*clear)		(ESearchBar *search_bar);
};

GType		e_search_bar_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_search_bar_new		(EWebView *web_view);
void		e_search_bar_clear		(ESearchBar *search_bar);
void		e_search_bar_changed		(ESearchBar *search_bar);
EWebView *	e_search_bar_get_web_view	(ESearchBar *search_bar);
gboolean	e_search_bar_get_active_search	(ESearchBar *search_bar);
gboolean	e_search_bar_get_case_sensitive	(ESearchBar *search_bar);
void		e_search_bar_set_case_sensitive	(ESearchBar *search_bar,
						 gboolean case_sensitive);
gchar *		e_search_bar_get_text		(ESearchBar *search_bar);
void		e_search_bar_set_text		(ESearchBar *search_bar,
						 const gchar *text);
gboolean	e_search_bar_get_can_hide	(ESearchBar *search_bar);
void		e_search_bar_set_can_hide	(ESearchBar *search_bar,
						 gboolean can_hide);
void		e_search_bar_focus_entry	(ESearchBar *search_bar);

G_END_DECLS

#endif /* E_SEARCH_BAR_H */
