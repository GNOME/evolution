/*
 * e-sorter.h
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

#ifndef E_SORTER_H
#define E_SORTER_H

#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_SORTER \
	(e_sorter_get_type ())
#define E_SORTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SORTER, ESorter))
#define E_IS_SORTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SORTER))
#define E_SORTER_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_SORTER, ESorterInterface))

G_BEGIN_DECLS

typedef struct _ESorter ESorter;
typedef struct _ESorterInterface ESorterInterface;

struct _ESorterInterface {
	GTypeInterface parent_interface;

	gint		(*model_to_sorted)	(ESorter *sorter,
						 gint row);
	gint		(*sorted_to_model)	(ESorter *sorter,
						 gint row);

	void		(*get_model_to_sorted_array)
						(ESorter *sorter,
						 gint **array,
						 gint *count);
	void		(*get_sorted_to_model_array)
						(ESorter *sorter,
						 gint **array,
						 gint *count);

	gboolean	(*needs_sorting)	(ESorter *sorter);
};

GType		e_sorter_get_type		(void) G_GNUC_CONST;
gint		e_sorter_model_to_sorted	(ESorter *sorter,
						 gint row);
gint		e_sorter_sorted_to_model	(ESorter *sorter,
						 gint row);
void		e_sorter_get_model_to_sorted_array
						(ESorter *sorter,
						 gint **array,
						 gint *count);
void		e_sorter_get_sorted_to_model_array
						(ESorter *sorter,
						 gint **array,
						 gint *count);
gboolean	e_sorter_needs_sorting		(ESorter *sorter);

G_END_DECLS

#endif /* E_SORTER_H */
