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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_BIT_ARRAY_TYPE        (e_bit_array_get_type ())
#define E_BIT_ARRAY(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), E_BIT_ARRAY_TYPE, EBitArray))
#define E_BIT_ARRAY_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), E_BIT_ARRAY_TYPE, EBitArrayClass))
#define E_IS_BIT_ARRAY(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_BIT_ARRAY_TYPE))
#define E_IS_BIT_ARRAY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), E_BIT_ARRAY_TYPE))

#ifndef _E_FOREACH_FUNC_H_
#define _E_FOREACH_FUNC_H_
typedef void (*EForeachFunc) (int model_row,
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
EBitArray *e_bit_array_new                 (int           count);

gboolean   e_bit_array_value_at            (EBitArray    *selection,
					    gint          n);
void       e_bit_array_foreach             (EBitArray    *selection,
					    EForeachFunc  callback,
					    gpointer      closure);
void       e_bit_array_clear               (EBitArray    *selection);
gint       e_bit_array_selected_count      (EBitArray    *selection);
void       e_bit_array_select_all          (EBitArray    *selection);
void       e_bit_array_invert_selection    (EBitArray    *selection);
int        e_bit_array_bit_count           (EBitArray    *selection);
void       e_bit_array_change_one_row      (EBitArray    *selection,
					    int           row,
					    gboolean      grow);
void       e_bit_array_change_range        (EBitArray    *selection,
					    int           start,
					    int           end,
					    gboolean      grow);
void       e_bit_array_select_single_row   (EBitArray    *eba,
					    int           row);
void       e_bit_array_toggle_single_row   (EBitArray    *eba,
					    int           row);

void       e_bit_array_insert              (EBitArray    *esm,
					    int           row,
					    int           count);
void       e_bit_array_delete              (EBitArray    *esm,
					    int           row,
					    int           count);
void       e_bit_array_delete_single_mode  (EBitArray    *esm,
					    int           row,
					    int           count);
void       e_bit_array_move_row            (EBitArray    *esm,
					    int           old_row,
					    int           new_row);
gint       e_bit_array_bit_count           (EBitArray    *esm);

gboolean   e_bit_array_cross_and           (EBitArray    *esm);
gboolean   e_bit_array_cross_or            (EBitArray    *esm);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _E_BIT_ARRAY_H_ */
