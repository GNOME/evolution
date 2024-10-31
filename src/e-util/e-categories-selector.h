/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CATEGORIES_SELECTOR_H
#define E_CATEGORIES_SELECTOR_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_CATEGORIES_SELECTOR \
	(e_categories_selector_get_type ())
#define E_CATEGORIES_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CATEGORIES_SELECTOR, ECategoriesSelector))
#define E_CATEGORIES_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CATEGORIES_SELECTOR, ECategoriesSelectorClass))
#define E_IS_CATEGORIES_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CATEGORIES_SELECTOR))
#define E_IS_CATEGORIES_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CATEGORIES_SELECTOR))
#define E_CATEGORIES_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CATEGORIES_SELECTOR, ECategoriesSelectorClass))

G_BEGIN_DECLS

typedef struct _ECategoriesSelector ECategoriesSelector;
typedef struct _ECategoriesSelectorClass ECategoriesSelectorClass;
typedef struct _ECategoriesSelectorPrivate ECategoriesSelectorPrivate;

/**
 * ECategoriesSelector:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 *
 * Since: 3.2
 **/
struct _ECategoriesSelector {
	GtkTreeView parent;
	ECategoriesSelectorPrivate *priv;
};

struct _ECategoriesSelectorClass {
	GtkTreeViewClass parent_class;

	void		(*category_checked)	(ECategoriesSelector *selector,
						 const gchar *category,
						 gboolean checked);

	void		(*selection_changed)	(ECategoriesSelector *selector,
						 GtkTreeSelection *selection);
};

GType		e_categories_selector_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_categories_selector_new	(void);
gchar *		e_categories_selector_get_checked
						(ECategoriesSelector *selector);
void		e_categories_selector_set_checked
						(ECategoriesSelector *selector,
						 const gchar *categories);
gboolean	e_categories_selector_get_items_checkable
						(ECategoriesSelector *selector);
void		e_categories_selector_set_items_checkable
						(ECategoriesSelector *selectr,
						 gboolean checkable);
void		e_categories_selector_delete_selection
						(ECategoriesSelector *selector);
gchar *		e_categories_selector_get_selected
						(ECategoriesSelector *selector);
gboolean	e_categories_selector_get_use_inconsistent
						(ECategoriesSelector *selector);
void		e_categories_selector_set_use_inconsistent
						(ECategoriesSelector *selector,
						 gboolean use_inconsistent);
void		e_categories_selector_get_changes
						(ECategoriesSelector *selector,
						 GHashTable **out_checked, /* gchar * ~> NULL */
						 GHashTable **out_unchecked); /* gchar * ~> NULL */

gchar *		e_categories_selector_util_apply_changes
						(const gchar *in_categories,
						 GHashTable *checked,
						 GHashTable *unchecked);

G_END_DECLS

#endif /* E_CATEGORIES_SELECTOR_H */
