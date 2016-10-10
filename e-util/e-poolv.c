/*
 * e-poolv.c
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

#include "evolution-config.h"

#include "e-poolv.h"

#include <string.h>
#include <camel/camel.h>

struct _EPoolv {
	guchar length;
	const gchar *s[1];
};

/**
 * e_poolv_new:
 * @size: The number of elements in the poolv, maximum of 254 elements.
 *
 * Create a new #EPoolv: a string vector which shares a global string
 * pool.  An #EPoolv can be used to work with arrays of strings which
 * save memory by eliminating duplicated allocations of the same string.
 *
 * This is useful when you have a log of read-only strings that do not
 * go away and are duplicated a lot, such as email headers.
 *
 * Returns: a new #EPoolv
 **/
EPoolv *
e_poolv_new (guint size)
{
	EPoolv *poolv;

	g_return_val_if_fail (size < 255, NULL);

	poolv = g_malloc0 (sizeof (*poolv) + (size - 1) * sizeof (gchar *));
	poolv->length = size;

	return poolv;
}

/**
 * e_poolv_set:
 * @poolv: pooled string vector
 * @index: index in vector of string
 * @str: string to set
 * @freeit: whether the caller is releasing its reference to the
 * string
 *
 * Set a string vector reference.  If the caller will no longer be
 * referencing the string, freeit should be TRUE.  Otherwise, this
 * will duplicate the string if it is not found in the pool.
 *
 * Returns: @poolv
 **/
EPoolv *
e_poolv_set (EPoolv *poolv,
             gint index,
             gchar *str,
             gint freeit)
{
	const gchar *old_str;

	g_return_val_if_fail (poolv != NULL, NULL);
	g_return_val_if_fail (index >= 0 && index < poolv->length, NULL);

	if (!str) {
		camel_pstring_free (poolv->s[index]);
		poolv->s[index] = NULL;
		return poolv;
	}

	old_str = poolv->s[index];
	poolv->s[index] = (gchar *) camel_pstring_add (str, freeit);

	camel_pstring_free (old_str);

	return poolv;
}

/**
 * e_poolv_get:
 * @poolv: pooled string vector
 * @index: index in vector of string
 *
 * Retrieve a string by index.  This could possibly just be a macro.
 *
 * Since the pool is never freed, this string does not need to be
 * duplicated, but should not be modified.
 *
 * Returns: string at that index.
 **/
const gchar *
e_poolv_get (EPoolv *poolv,
             gint index)
{
	g_return_val_if_fail (poolv != NULL, NULL);
	g_return_val_if_fail (index >= 0 && index < poolv->length, NULL);

	return poolv->s[index] ? poolv->s[index] : "";
}

/**
 * e_poolv_destroy:
 * @poolv: pooled string vector to free
 *
 * Free a pooled string vector.  This doesn't free the strings from
 * the vector, however.
 **/
void
e_poolv_destroy (EPoolv *poolv)
{
	gint ii;

	g_return_if_fail (poolv != NULL);

	for (ii = 0; ii < poolv->length; ii++) {
		camel_pstring_free (poolv->s[ii]);
	}

	g_free (poolv);
}
