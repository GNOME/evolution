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

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-bit-array.h"

#define ONES ((guint32) 0xffffffff)

#define BOX(n) ((n) / 32)
#define OFFSET(n) (31 - ((n) % 32))
#define BITMASK(n) ((guint32)(((guint32) 0x1) << OFFSET((n))))
#define BITMASK_LEFT(n) ((((n) % 32) == 0) ? 0 : (ONES << (32 - ((n) % 32))))
#define BITMASK_RIGHT(n) ((guint32)(((guint32) ONES) >> ((n) % 32)))

G_DEFINE_TYPE (
	EBitArray,
	e_bit_array,
	G_TYPE_OBJECT)

static void
e_bit_array_insert_real (EBitArray *bit_array,
                         gint row)
{
	gint box;
	gint i;
	if (bit_array->bit_count >= 0) {
		/* Add another word if needed. */
		if ((bit_array->bit_count & 0x1f) == 0) {
			bit_array->data = g_renew (
				guint32, bit_array->data,
				(bit_array->bit_count >> 5) + 1);
			bit_array->data[bit_array->bit_count >> 5] = 0;
		}

		/* The box is the word that our row is in. */
		box = BOX (row);
		/* Shift all words to the right of our box right one bit. */
		for (i = bit_array->bit_count >> 5; i > box; i--) {
			bit_array->data[i] =
				(bit_array->data[i] >> 1) |
				(bit_array->data[i - 1] << 31);
		}

		/* Shift right half of box one bit to the right. */
		bit_array->data[box] =
			(bit_array->data[box] & BITMASK_LEFT (row)) |
			((bit_array->data[box] & BITMASK_RIGHT (row)) >> 1);
		bit_array->bit_count++;
	}
}

static void
e_bit_array_delete_real (EBitArray *bit_array,
                         gint row,
                         gboolean move_selection_mode)
{
	gint box;
	gint i;
	gint last;
	gint selected = FALSE;

	if (bit_array->bit_count > 0) {
		guint32 bitmask;
		box = row >> 5;
		last = (bit_array->bit_count - 1) >> 5;

		/* Build bitmasks for the left and right half of the box */
		bitmask = BITMASK_RIGHT (row) >> 1;
		if (move_selection_mode)
			selected = e_bit_array_value_at (bit_array, row);
		/* Shift right half of box one bit to the left. */
		bit_array->data[box] =
			(bit_array->data[box] & BITMASK_LEFT (row)) |
			((bit_array->data[box] & bitmask) << 1);

		/* Shift all words to the right of our box left one bit. */
		if (box < last) {
			bit_array->data[box] &= bit_array->data[box + 1] >> 31;

			for (i = box + 1; i < last; i++) {
				bit_array->data[i] =
					(bit_array->data[i] << 1) |
					(bit_array->data[i + 1] >> 31);
			}
			/* this over-runs our memory! */
			/*bit_array->data[i] = bit_array->data[i] << 1; */
		}
		bit_array->bit_count--;
		/* Remove the last word if not needed. */
		if ((bit_array->bit_count & 0x1f) == 0) {
			bit_array->data = g_renew (guint32, bit_array->data, bit_array->bit_count >> 5);
		}
		if (move_selection_mode && selected && bit_array->bit_count > 0) {
			e_bit_array_select_single_row (
				bit_array, row == bit_array->bit_count ? row - 1 : row);
		}
	}
}

/* FIXME : Improve efficiency here. */
void
e_bit_array_delete (EBitArray *bit_array,
                    gint row,
                    gint count)
{
	gint i;
	for (i = 0; i < count; i++)
		e_bit_array_delete_real (bit_array, row, FALSE);
}

/* FIXME : Improve efficiency here. */
void
e_bit_array_delete_single_mode (EBitArray *bit_array,
                                gint row,
                                gint count)
{
	gint i;
	for (i = 0; i < count; i++)
		e_bit_array_delete_real (bit_array, row, TRUE);
}

/* FIXME : Improve efficiency here. */
void
e_bit_array_insert (EBitArray *bit_array,
                    gint row,
                    gint count)
{
	gint i;
	for (i = 0; i < count; i++)
		e_bit_array_insert_real (bit_array, row);
}

/* FIXME: Implement this more efficiently. */
void
e_bit_array_move_row (EBitArray *bit_array,
                      gint old_row,
                      gint new_row)
{
	e_bit_array_delete_real (bit_array, old_row, FALSE);
	e_bit_array_insert_real (bit_array, new_row);
}

static void
bit_array_finalize (GObject *object)
{
	EBitArray *bit_array;

	bit_array = E_BIT_ARRAY (object);

	g_free (bit_array->data);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_bit_array_parent_class)->finalize (object);
}

/**
 * e_bit_array_value_at
 * @bit_array: #EBitArray to check
 * @n: The row to check
 *
 * This routine calculates whether the given row is selected.
 *
 * Returns: %TRUE if the given row is selected
 */
gboolean
e_bit_array_value_at (EBitArray *bit_array,
                      gint n)
{
	if (bit_array->bit_count < n || bit_array->bit_count == 0)
		return 0;
	else
		return (bit_array->data[BOX (n)] >> OFFSET (n)) & 0x1;
}

/**
 * e_bit_array_foreach
 * @bit_array: #EBitArray to traverse
 * @callback: The callback function to call back.
 * @closure: The closure
 *
 * This routine calls the given callback function once for each
 * selected row, passing closure as the closure.
 */
void
e_bit_array_foreach (EBitArray *bit_array,
                     EForeachFunc callback,
                     gpointer closure)
{
	gint i;
	gint last = (bit_array->bit_count + 31) / 32;
	for (i = 0; i < last; i++) {
		if (bit_array->data[i]) {
			gint j;
			guint32 value = bit_array->data[i];
			for (j = 0; j < 32; j++) {
				if (value & 0x80000000) {
					callback (i * 32 + j, closure);
				}
				value <<= 1;
			}
		}
	}
}

#define PART(x,n) ((guint32) ((((guint64) x) & (((guint64) 0x01010101) << n)) >> n))
#define SECTION(x, n) (((x) >> (n * 8)) & 0xff)

/**
 * e_bit_array_selected_count
 * @bit_array: #EBitArray to count
 *
 * This routine calculates the number of rows selected.
 *
 * Returns: The number of rows selected in the given model.
 */
gint
e_bit_array_selected_count (EBitArray *bit_array)
{
	gint count;
	gint i;
	gint last;

	if (!bit_array->data)
		return 0;

	count = 0;

	last = BOX (bit_array->bit_count - 1);

	for (i = 0; i <= last; i++) {
		gint j;
		guint32 thiscount = 0;
		for (j = 0; j < 8; j++)
			thiscount += PART (bit_array->data[i], j);
		for (j = 0; j < 4; j++)
			count += SECTION (thiscount, j);
	}

	return count;
}

/**
 * e_bit_array_select_all
 * @bit_array: #EBitArray to select all
 *
 * This routine selects all the rows in the given
 * #EBitArray.
 */
void
e_bit_array_select_all (EBitArray *bit_array)
{
	gint i;

	if (!bit_array->data)
		bit_array->data = g_new0 (guint32, (bit_array->bit_count + 31) / 32);

	for (i = 0; i < (bit_array->bit_count + 31) / 32; i++) {
		bit_array->data[i] = ONES;
	}

	/* need to zero out the bits corresponding to the rows not
	 * selected in the last full 32 bit mask */
	if (bit_array->bit_count % 32) {
		gint unselected_mask = 0;
		gint num_unselected_in_last_byte = 32 - bit_array->bit_count % 32;

		for (i = 0; i < num_unselected_in_last_byte; i++)
			unselected_mask |= 1 << i;

		bit_array->data[(bit_array->bit_count + 31) / 32 - 1] &= ~unselected_mask;
	}
}

gint
e_bit_array_bit_count (EBitArray *bit_array)
{
	return bit_array->bit_count;
}

#define OPERATE(object, i,mask,grow) \
	((grow) ? (((object)->data[(i)]) |= ((guint32) ~(mask))) : \
	(((object)->data[(i)]) &= (mask)))

void
e_bit_array_change_one_row (EBitArray *bit_array,
                            gint row,
                            gboolean grow)
{
	gint i;
	i = BOX (row);

	OPERATE (bit_array, i, ~BITMASK (row), grow);
}

void
e_bit_array_change_range (EBitArray *bit_array,
                          gint start,
                          gint end,
                          gboolean grow)
{
	gint i, last;
	if (start != end) {
		i = BOX (start);
		last = BOX (end);

		if (i == last) {
			OPERATE (
				bit_array, i, BITMASK_LEFT (start) |
				BITMASK_RIGHT (end), grow);
		} else {
			OPERATE (bit_array, i, BITMASK_LEFT (start), grow);
			if (grow)
				for (i++; i < last; i++)
					bit_array->data[i] = ONES;
			else
				for (i++; i < last; i++)
					bit_array->data[i] = 0;
			OPERATE (bit_array, i, BITMASK_RIGHT (end), grow);
		}
	}
}

void
e_bit_array_select_single_row (EBitArray *bit_array,
                               gint row)
{
	gint i;
	for (i = 0; i < ((bit_array->bit_count + 31) / 32); i++) {
		if (!((i == BOX (row) && bit_array->data[i] == BITMASK (row)) ||
		      (i != BOX (row) && bit_array->data[i] == 0))) {
			g_free (bit_array->data);
			bit_array->data = g_new0 (guint32, (bit_array->bit_count + 31) / 32);
			bit_array->data[BOX (row)] = BITMASK (row);

			break;
		}
	}
}

void
e_bit_array_toggle_single_row (EBitArray *bit_array,
                               gint row)
{
	if (bit_array->data[BOX (row)] & BITMASK (row))
		bit_array->data[BOX (row)] &= ~BITMASK (row);
	else
		bit_array->data[BOX (row)] |= BITMASK (row);
}

static void
e_bit_array_init (EBitArray *bit_array)
{
	bit_array->data = NULL;
	bit_array->bit_count = 0;
}

static void
e_bit_array_class_init (EBitArrayClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = bit_array_finalize;
}

EBitArray *
e_bit_array_new (gint count)
{
	EBitArray *bit_array;

	bit_array = g_object_new (E_TYPE_BIT_ARRAY, NULL);
	bit_array->bit_count = count;
	bit_array->data = g_new0 (guint32, (bit_array->bit_count + 31) / 32);

	return bit_array;
}
