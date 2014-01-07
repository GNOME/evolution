/*
 * e-sorter-array.h
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

#ifndef E_SORTER_ARRAY_H
#define E_SORTER_ARRAY_H

#include <e-util/e-sorter.h>

/* Standard GObject macros */
#define E_TYPE_SORTER_ARRAY \
	(e_sorter_array_get_type ())
#define E_SORTER_ARRAY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SORTER_ARRAY, ESorterArray))
#define E_SORTER_ARRAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SORTER_ARRAY, ESorterArrayClass))
#define E_IS_SORTER_ARRAY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SORTER_ARRAY))
#define E_IS_SORTER_ARRAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SORTER_ARRAY))
#define E_SORTER_ARRAY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SORTER_ARRAY, ESorterArrayClass))

G_BEGIN_DECLS

typedef struct _ESorterArray ESorterArray;
typedef struct _ESorterArrayClass ESorterArrayClass;

typedef gint (*ECompareRowsFunc) (gint row1,
				 gint row2,
				 GHashTable *cmp_cache,
				 gpointer closure);

typedef GHashTable * (*ECreateCmpCacheFunc) (gpointer closure);

struct _ESorterArray {
	GObject parent;

	GHashTable *cmp_cache;
	ECreateCmpCacheFunc create_cmp_cache;
	ECompareRowsFunc compare;
	gpointer closure;

	/* If needs_sorting is 0, then
	 * model_to_sorted and sorted_to_model are no-ops. */
	gint *sorted;
	gint *backsorted;

	gint rows;
};

struct _ESorterArrayClass {
	GObjectClass parent_class;
};

GType		e_sorter_array_get_type	(void) G_GNUC_CONST;
ESorterArray *	e_sorter_array_new	(ECreateCmpCacheFunc create_cmp_cache,
					 ECompareRowsFunc compare,
					 gpointer closure);
void		e_sorter_array_clean	(ESorterArray *sorter);
void		e_sorter_array_set_count
					(ESorterArray *sorter,
					 gint count);
void		e_sorter_array_append	(ESorterArray *sorter,
					 gint count);

G_END_DECLS

#endif /* E_SORTER_ARRAY_H */
