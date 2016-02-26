/*
 * e-source-conflict-search.h
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

#ifndef E_SOURCE_CONFLICT_SEARCH_H
#define E_SOURCE_CONFLICT_SEARCH_H

#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_SOURCE_CONFLICT_SEARCH \
	(e_source_conflict_search_get_type ())
#define E_SOURCE_CONFLICT_SEARCH(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_CONFLICT_SEARCH, ESourceConflictSearch))
#define E_SOURCE_CONFLICT_SEARCH_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_CONFLICT_SEARCH, ESourceConflictSearchClass))
#define E_IS_SOURCE_CONFLICT_SEARCH(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SOURCE_CONFLICT_SEARCH))
#define E_IS_SOURCE_CONFLICT_SEARCH_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SOURCE_CONFLICT_SEARCH))
#define E_SOURCE_CONFLICT_SEARCH_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_CONFLICT_SEARCH, ESourceConflictSearchClass))

/**
 * E_SOURCE_EXTENSION_CONFLICT_SEARCH:
 *
 * Pass this extension name to e_source_get_extension() to access
 * #ESourceConflictSearch.  This is also used as a group name in key files.
 *
 * Since: 3.6
 **/
#define E_SOURCE_EXTENSION_CONFLICT_SEARCH "Conflict Search"

G_BEGIN_DECLS

typedef struct _ESourceConflictSearch ESourceConflictSearch;
typedef struct _ESourceConflictSearchClass ESourceConflictSearchClass;
typedef struct _ESourceConflictSearchPrivate ESourceConflictSearchPrivate;

/**
 * ESourceConflictSearch:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 *
 * Since: 3.6
 **/
struct _ESourceConflictSearch {
	ESourceExtension parent;
	ESourceConflictSearchPrivate *priv;
};

struct _ESourceConflictSearchClass {
	ESourceExtensionClass parent_class;
};

GType		e_source_conflict_search_get_type
						(void) G_GNUC_CONST;
gboolean	e_source_conflict_search_get_include_me
						(ESourceConflictSearch *extension);
void		e_source_conflict_search_set_include_me
						(ESourceConflictSearch *extension,
						 gboolean include_me);

G_END_DECLS

#endif /* E_SOURCE_CONFLICT_SEARCH_H */
