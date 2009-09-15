/*
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
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_BIT_ARRAY_H_
#define _E_BIT_ARRAY_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define E_BIT_ARRAY_TYPE        (e_bit_array_get_type ())
#define E_BIT_ARRAY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_BIT_ARRAY_TYPE, EBitArray))
#define E_BIT_ARRAY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_BIT_ARRAY_TYPE, EBitArrayClass))
#define E_IS_BIT_ARRAY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_BIT_ARRAY_TYPE))
#define E_IS_BIT_ARRAY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_BIT_ARRAY_TYPE))

#ifndef _E_FOREACH_FUNC_H_
#define _E_FOREACH_FUNC_H_
typedef void (*EForeachFunc) (gint model_row,
			      gpointer closure);
#endif

typedef struct {
	GObject base;

	gint bit_count;
        guint32 *data;
} EBitArray;

typedef struct {
	GObjectClass parent_class;
} EBitArrayClass;

GType      e_bit_array_get_type            (void);
EBitArray *e_bit_array_new                 (gint           count);

gboolean   e_bit_array_value_at            (EBitArray    *selection,
					    gint          n);
void       e_bit_array_foreach             (EBitArray    *selection,
					    EForeachFunc  callback,
					    gpointer      closure);
gint       e_bit_array_selected_count      (EBitArray    *selection);
void       e_bit_array_select_all          (EBitArray    *selection);
void       e_bit_array_invert_selection    (EBitArray    *selection);
gint       e_bit_array_bit_count           (EBitArray    *selection);
void       e_bit_array_change_one_row      (EBitArray    *selection,
					    gint           row,
					    gboolean      grow);
void       e_bit_array_change_range        (EBitArray    *selection,
					    gint           start,
					    gint           end,
					    gboolean      grow);
void       e_bit_array_select_single_row   (EBitArray    *eba,
					    gint           row);
void       e_bit_array_toggle_single_row   (EBitArray    *eba,
					    gint           row);

void       e_bit_array_insert              (EBitArray    *esm,
					    gint           row,
					    gint           count);
void       e_bit_array_delete              (EBitArray    *esm,
					    gint           row,
					    gint           count);
void       e_bit_array_delete_single_mode  (EBitArray    *esm,
					    gint           row,
					    gint           count);
void       e_bit_array_move_row            (EBitArray    *esm,
					    gint           old_row,
					    gint           new_row);

G_END_DECLS

#endif /* _E_BIT_ARRAY_H_ */
