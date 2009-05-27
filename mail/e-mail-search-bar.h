/*
 * e-mail-search-bar.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MAIL_SEARCH_BAR_H
#define E_MAIL_SEARCH_BAR_H

#include <gtk/gtk.h>
#include <gtkhtml/gtkhtml.h>
#include <mail/e-searching-tokenizer.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_SEARCH_BAR \
	(e_mail_search_bar_get_type ())
#define E_MAIL_SEARCH_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_SEARCH_BAR, EMailSearchBar))
#define E_MAIL_SEARCH_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_SEARCH_BAR, EMailSearchBarClass))
#define E_IS_MAIL_SEARCH_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_SEARCH_BAR))
#define E_IS_MAIL_SEARCH_BAR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_SEARCH_BAR))
#define E_MAIL_SEARCH_BAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_SEARCH_BAR, EMailSearchBarClass))

G_BEGIN_DECLS

typedef struct _EMailSearchBar EMailSearchBar;
typedef struct _EMailSearchBarClass EMailSearchBarClass;
typedef struct _EMailSearchBarPrivate EMailSearchBarPrivate;

struct _EMailSearchBar {
	GtkHBox parent;
	EMailSearchBarPrivate *priv;
};

struct _EMailSearchBarClass {
	GtkHBoxClass parent_class;

	/* Signals */
	void		(*changed)		(EMailSearchBar *search_bar);
	void		(*clear)		(EMailSearchBar *search_bar);
};

GType		e_mail_search_bar_get_type	(void);
GtkWidget *	e_mail_search_bar_new		(GtkHTML *html);
void		e_mail_search_bar_clear		(EMailSearchBar *search_bar);
void		e_mail_search_bar_changed	(EMailSearchBar *search_bar);
GtkHTML *	e_mail_search_bar_get_html	(EMailSearchBar *search_bar);
ESearchingTokenizer *
		e_mail_search_bar_get_tokenizer	(EMailSearchBar *search_bar);
gboolean	e_mail_search_bar_get_case_sensitive
						(EMailSearchBar *search_bar);
void		e_mail_search_bar_set_case_sensitive
						(EMailSearchBar *search_bar,
						 gboolean case_sensitive);
gchar *		e_mail_search_bar_get_text	(EMailSearchBar *search_bar);
void		e_mail_search_bar_set_text	(EMailSearchBar *search_bar,
						 const gchar *text);

G_END_DECLS

#endif /* E_MAIL_SEARCH_BAR_H */
