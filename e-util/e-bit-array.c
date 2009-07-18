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

#include <config.h>

#include <gtk/gtk.h>

#include "e-bit-array.h"
#include "e-util.h"

#define ONES ((guint32) 0xffffffff)

#define BOX(n) ((n) / 32)
#define OFFSET(n) (31 - ((n) % 32))
#define BITMASK(n) ((guint32)(((guint32) 0x1) << OFFSET((n))))
#define BITMASK_LEFT(n) ((((n) % 32) == 0) ? 0 : (ONES << (32 - ((n) % 32))))
#define BITMASK_RIGHT(n) ((guint32)(((guint32) ONES) >> ((n) % 32)))

G_DEFINE_TYPE (EBitArray, e_bit_array, G_TYPE_OBJECT)

static void
e_bit_array_insert_real(EBitArray *eba, gint row)
{
	gint box;
	gint i;
	if (eba->bit_count >= 0) {
		/* Add another word if needed. */
		if ((eba->bit_count & 0x1f) == 0) {
			eba->data = g_renew(guint32, eba->data, (eba->bit_count >> 5) + 1);
			eba->data[eba->bit_count >> 5] = 0;
		}

		/* The box is the word that our row is in. */
		box = BOX(row);
		/* Shift all words to the right of our box right one bit. */
		for (i = eba->bit_count >> 5; i > box; i--) {
			eba->data[i] = (eba->data[i] >> 1) | (eba->data[i - 1] << 31);
		}

		/* Shift right half of box one bit to the right. */
		eba->data[box] = (eba->data[box] & BITMASK_LEFT(row)) | ((eba->data[box] & BITMASK_RIGHT(row)) >> 1);
		eba->bit_count ++;
	}
}

static void
e_bit_array_delete_real(EBitArray *eba, gint row, gboolean move_selection_mode)
{
	gint box;
	gint i;
	gint last;
	gint selected = FALSE;
	if (eba->bit_count >= 0) {
		guint32 bitmask;
		box = row >> 5;
		last = eba->bit_count >> 5;

		/* Build bitmasks for the left and right half of the box */
		bitmask = BITMASK_RIGHT(row) >> 1;
		if (move_selection_mode)
			selected = e_bit_array_value_at(eba, row);
		/* Shift right half of box one bit to the left. */
		eba->data[box] = (eba->data[box] & BITMASK_LEFT(row))| ((eba->data[box] & bitmask) << 1);

		/* Shift all words to the right of our box left one bit. */
		if (box < last) {
			eba->data[box] &= eba->data[box + 1] >> 31;

			for (i = box + 1; i < last; i++) {
				eba->data[i] = (eba->data[i] << 1) | (eba->data[i + 1] >> 31);
			}
			/* this over-runs our memory! */
			/*eba->data[i] = eba->data[i] << 1; */
		}
		eba->bit_count --;
		/* Remove the last word if not needed. */
		if ((eba->bit_count & 0x1f) == 0) {
			eba->data = g_renew(guint32, eba->data, eba->bit_count >> 5);
		}
		if (move_selection_mode && selected && eba->bit_count > 0) {
			e_bit_array_select_single_row (eba, row == eba->bit_count ? row - 1 : row);
		}
	}
}

/* FIXME : Improve efficiency here. */
void
e_bit_array_delete(EBitArray *eba, gint row, gint count)
{
	gint i;
	for (i = 0; i < count; i++)
		e_bit_array_delete_real(eba, row, FALSE);
}

/* FIXME : Improve efficiency here. */
void
e_bit_array_delete_single_mode(EBitArray *eba, gint row, gint count)
{
	gint i;
	for (i = 0; i < count; i++)
		e_bit_array_delete_real(eba, row, TRUE);
}

/* FIXME : Improve efficiency here. */
void
e_bit_array_insert(EBitArray *eba, gint row, gint count)
{
	gint i;
	for (i = 0; i < count; i++)
		e_bit_array_insert_real(eba, row);
}

/* FIXME: Implement this more efficiently. */
void
e_bit_array_move_row(EBitArray *eba, gint old_row, gint new_row)
{
	e_bit_array_delete_real(eba, old_row, FALSE);
	e_bit_array_insert_real(eba, new_row);
}

static void
eba_dispose (GObject *object)
{
	EBitArray *eba;

	eba = E_BIT_ARRAY (object);

	if (eba->data)
		g_free(eba->data);
	eba->data = NULL;

	if (G_OBJECT_CLASS (e_bit_array_parent_class)->dispose)
		(* G_OBJECT_CLASS (e_bit_array_parent_class)->dispose) (object);
}

/**
 * e_selection_model_is_row_selected
 * @selection: #EBitArray to check
 * @n: The row to check
 *
 * This routine calculates whether the given row is selected.
 *
 * Returns: %TRUE if the given row is selected
 */
gboolean
e_bit_array_value_at (EBitArray *eba,
		      gint             n)
{
	if (eba->bit_count < n || eba->bit_count == 0)
		return 0;
	else
		return (eba->data[BOX(n)] >> OFFSET(n)) & 0x1;
}

/**
 * e_selection_model_foreach
 * @selection: #EBitArray to traverse
 * @callback: The callback function to call back.
 * @closure: The closure
 *
 * This routine calls the given callback function once for each
 * selected row, passing closure as the closure.
 */
void
e_bit_array_foreach (EBitArray *eba,
		     EForeachFunc     callback,
		     gpointer         closure)
{
	gint i;
	gint last = (eba->bit_count + 31) / 32;
	for (i = 0; i < last; i++) {
		if (eba->data[i]) {
			gint j;
			guint32 value = eba->data[i];
			for (j = 0; j < 32; j++) {
				if (value & 0x80000000) {
					callback(i * 32 + j, closure);
				}
				value <<= 1;
			}
		}
	}
}

/**
 * e_selection_model_clear
 * @selection: #EBitArray to clear
 *
 * This routine clears the selection to no rows selected.
 */
void
e_bit_array_clear(EBitArray *eba)
{
	g_free(eba->data);
	eba->data = NULL;
	eba->bit_count = 0;
}

#define PART(x,n) (((x) & (0x01010101 << n)) >> n)
#define SECTION(x, n) (((x) >> (n * 8)) & 0xff)

/**
 * e_selection_model_selected_count
 * @selection: #EBitArray to count
 *
 * This routine calculates the number of rows selected.
 *
 * Returns: The number of rows selected in the given model.
 */
gint
e_bit_array_selected_count (EBitArray *eba)
{
	gint count;
	gint i;
	gint last;

	if (!eba->data)
		return 0;

	count = 0;

	last = BOX(eba->bit_count - 1);

	for (i = 0; i <= last; i++) {
		gint j;
		guint32 thiscount = 0;
		for (j = 0; j < 8; j++)
			thiscount += PART(eba->data[i], j);
		for (j = 0; j < 4; j++)
			count += SECTION(thiscount, j);
	}

	return count;
}

/**
 * e_selection_model_select_all
 * @selection: #EBitArray to select all
 *
 * This routine selects all the rows in the given
 * #EBitArray.
 */
void
e_bit_array_select_all (EBitArray *eba)
{
	gint i;

	if (!eba->data)
		eba->data = g_new0 (guint32, (eba->bit_count + 31) / 32);

	for (i = 0; i < (eba->bit_count + 31) / 32; i ++) {
		eba->data[i] = ONES;
	}

	/* need to zero out the bits corresponding to the rows not
	   selected in the last full 32 bit mask */
	if (eba->bit_count % 32) {
		gint unselected_mask = 0;
		gint num_unselected_in_last_byte = 32 - eba->bit_count % 32;

		for (i = 0; i < num_unselected_in_last_byte; i ++)
			unselected_mask |= 1 << i;

		eba->data[(eba->bit_count + 31) / 32 - 1] &= ~unselected_mask;
	}
}

/**
 * e_selection_model_invert_selection
 * @selection: #EBitArray to invert
 *
 * This routine inverts all the rows in the given
 * #EBitArray.
 */
void
e_bit_array_invert_selection (EBitArray *eba)
{
	gint i;

	if (!eba->data)
		eba->data = g_new0 (guint32, (eba->bit_count + 31) / 32);

	for (i = 0; i < (eba->bit_count + 31) / 32; i ++) {
		eba->data[i] = ~eba->data[i];
	}
}

gint
e_bit_array_bit_count (EBitArray *eba)
{
	return eba->bit_count;
}

gboolean
e_bit_array_cross_and           (EBitArray    *eba)
{
	gint i;
	for (i = 0; i < eba->bit_count / 32; i++) {
		if (eba->data[i] != ONES)
			return FALSE;
	}
	if ((eba->bit_count % 32) && ((eba->data[i] & BITMASK_LEFT(eba->bit_count)) != BITMASK_LEFT(eba->bit_count)))
		return FALSE;
	return TRUE;
}

gboolean
e_bit_array_cross_or            (EBitArray    *eba)
{
	gint i;
	for (i = 0; i < eba->bit_count / 32; i++) {
		if (eba->data[i] != 0)
			return TRUE;
	}
	if ((eba->bit_count % 32) && ((eba->data[i] & BITMASK_LEFT(eba->bit_count)) != 0))
		return TRUE;
	return FALSE;
}

#define OPERATE(object, i,mask,grow) ((grow) ? (((object)->data[(i)]) |= ((guint32) ~(mask))) : (((object)->data[(i)]) &= (mask)))

void
e_bit_array_change_one_row(EBitArray *eba, gint row, gboolean grow)
{
	gint i;
	i = BOX(row);

	OPERATE(eba, i, ~BITMASK(row), grow);
}

void
e_bit_array_change_range(EBitArray *eba, gint start, gint end, gboolean grow)
{
	gint i, last;
	if (start != end) {
		i = BOX(start);
		last = BOX(end);

		if (i == last) {
			OPERATE(eba, i, BITMASK_LEFT(start) | BITMASK_RIGHT(end), grow);
		} else {
			OPERATE(eba, i, BITMASK_LEFT(start), grow);
			if (grow)
				for (i ++; i < last; i++)
					eba->data[i] = ONES;
			else
				for (i ++; i < last; i++)
					eba->data[i] = 0;
			OPERATE(eba, i, BITMASK_RIGHT(end), grow);
		}
	}
}

void
e_bit_array_select_single_row (EBitArray *eba, gint row)
{
	gint i;
	for (i = 0; i < ((eba->bit_count + 31) / 32); i++) {
		if (!((i == BOX(row) && eba->data[i] == BITMASK(row)) ||
		      (i != BOX(row) && eba->data[i] == 0))) {
			g_free(eba->data);
			eba->data = g_new0(guint32, (eba->bit_count + 31) / 32);
			eba->data[BOX(row)] = BITMASK(row);

			break;
		}
	}
}

void
e_bit_array_toggle_single_row (EBitArray *eba, gint row)
{
	if (eba->data[BOX(row)] & BITMASK(row))
		eba->data[BOX(row)] &= ~BITMASK(row);
	else
		eba->data[BOX(row)] |= BITMASK(row);
}

static void
e_bit_array_init (EBitArray *eba)
{
	eba->data = NULL;
	eba->bit_count = 0;
}

static void
e_bit_array_class_init (EBitArrayClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	object_class->dispose = eba_dispose;
}

EBitArray *
e_bit_array_new (gint count)
{
	EBitArray *eba = g_object_new (E_BIT_ARRAY_TYPE, NULL);
	eba->bit_count = count;
	eba->data = g_new0(guint32, (eba->bit_count + 31) / 32);
	return eba;
}
