/*
 * e-conflict-search-selector.h
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CONFLICT_SEARCH_SELECTOR_H
#define E_CONFLICT_SEARCH_SELECTOR_H

#include <libedataserver/libedataserver.h>
#include <e-util/e-source-selector.h>

/* Standard GObject macros */
#define E_TYPE_CONFLICT_SEARCH_SELECTOR \
	(e_conflict_search_selector_get_type ())
#define E_CONFLICT_SEARCH_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONFLICT_SEARCH_SELECTOR, EConflictSearchSelector))
#define E_CONFLICT_SEARCH_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONFLICT_SEARCH_SELECTOR, EConflictSearchSelectorClass))
#define E_IS_CONFLICT_SEARCH_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONFLICT_SEARCH_SELECTOR))
#define E_IS_CONFLICT_SEARCH_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONFLICT_SEARCH_SELECTOR))
#define E_CONFLICT_SEARCH_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONFLICT_SEARCH_SELECTOR, EConflictSearchSelectorClass))

G_BEGIN_DECLS

typedef struct _EConflictSearchSelector EConflictSearchSelector;
typedef struct _EConflictSearchSelectorClass EConflictSearchSelectorClass;
typedef struct _EConflictSearchSelectorPrivate EConflictSearchSelectorPrivate;

struct _EConflictSearchSelector {
	ESourceSelector parent;
	EConflictSearchSelectorPrivate *priv;
};

struct _EConflictSearchSelectorClass {
	ESourceSelectorClass parent_class;
};

GType		e_conflict_search_selector_get_type
						(void) G_GNUC_CONST;
GtkWidget *	e_conflict_search_selector_new	(ESourceRegistry *registry);

G_END_DECLS

#endif /* E_CONFLICT_SEARCH_SELECTOR_H */
