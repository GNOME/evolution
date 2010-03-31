/*
 * e-poolv.c
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
 */

#include "e-poolv.h"

#include <string.h>
#include <camel/camel.h>

struct _EPoolv {
	guchar length;
	gchar *s[1];
};

static GHashTable *poolv_pool;
static CamelMemPool *poolv_mempool;

G_LOCK_DEFINE_STATIC (poolv);

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

	G_LOCK (poolv);

	if (!poolv_pool)
		poolv_pool = g_hash_table_new (g_str_hash, g_str_equal);

	if (!poolv_mempool)
		poolv_mempool = camel_mempool_new (
			32 * 1024, 512, CAMEL_MEMPOOL_ALIGN_BYTE);

	G_UNLOCK (poolv);

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
e_poolv_set (EPoolv *poolv, gint index, gchar *str, gint freeit)
{
	g_return_val_if_fail (poolv != NULL, NULL);
	g_return_val_if_fail (index >= 0 && index < poolv->length, NULL);

	if (!str) {
		poolv->s[index] = NULL;
		return poolv;
	}

	G_LOCK (poolv);

	if ((poolv->s[index] = g_hash_table_lookup (poolv_pool, str)) != NULL) {
	} else {
		poolv->s[index] = camel_mempool_strdup (poolv_mempool, str);
		g_hash_table_insert (poolv_pool, poolv->s[index], poolv->s[index]);
	}

	G_UNLOCK (poolv);

	if (freeit)
		g_free (str);

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
e_poolv_get (EPoolv *poolv, gint index)
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
	g_return_if_fail (poolv != NULL);

	g_free (poolv);
}
