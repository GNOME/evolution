/*
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
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_BIT_ARRAY_H
#define E_BIT_ARRAY_H

#include <e-util/e-misc-utils.h>

/* Standard GObject macros */
#define E_TYPE_BIT_ARRAY \
	(e_bit_array_get_type ())
#define E_BIT_ARRAY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_BIT_ARRAY, EBitArray))
#define E_BIT_ARRAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_BIT_ARRAY, EBitArrayClass))
#define E_IS_BIT_ARRAY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_BIT_ARRAY))
#define E_IS_BIT_ARRAY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_BIT_ARRAY))
#define E_BIT_ARRAY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_BIT_ARRAY, EBitArrayClass))

G_BEGIN_DECLS

typedef struct _EBitArray EBitArray;
typedef struct _EBitArrayClass EBitArrayClass;

struct _EBitArray {
	GObject parent;

	gint bit_count;
	guint32 *data;
};

struct _EBitArrayClass {
	GObjectClass parent_class;
};

GType		e_bit_array_get_type		(void) G_GNUC_CONST;
EBitArray *	e_bit_array_new			(gint count);

gboolean	e_bit_array_value_at		(EBitArray *bit_array,
						 gint n);
void		e_bit_array_foreach		(EBitArray *bit_array,
						 EForeachFunc callback,
						 gpointer closure);
gint		e_bit_array_selected_count	(EBitArray *bit_array);
void		e_bit_array_select_all		(EBitArray *bit_array);
gint		e_bit_array_bit_count		(EBitArray *bit_array);
void		e_bit_array_change_one_row	(EBitArray *bit_array,
						 gint row,
						 gboolean grow);
void		e_bit_array_change_range	(EBitArray *bit_array,
						 gint start,
						 gint end,
						 gboolean grow);
void		e_bit_array_select_single_row	(EBitArray *bit_array,
						 gint row);
void		e_bit_array_toggle_single_row	(EBitArray *bit_array,
						 gint row);

void		e_bit_array_insert		(EBitArray *bit_array,
						 gint row,
						 gint count);
void		e_bit_array_delete		(EBitArray *bit_array,
						 gint row,
						 gint count);
void		e_bit_array_delete_single_mode	(EBitArray *bit_array,
						 gint row,
						 gint count);
void		e_bit_array_move_row		(EBitArray *bit_array,
						 gint old_row,
						 gint new_row);

G_END_DECLS

#endif /* E_BIT_ARRAY_H */
