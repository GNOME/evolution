/*
 * e-poolv.h
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

#ifndef E_POOLV_H
#define E_POOLV_H

/* This was moved out of libedataserver since only MessageList uses it. */

#include <glib.h>

G_BEGIN_DECLS

typedef struct _EPoolv EPoolv;

EPoolv *	e_poolv_new			(guint size);
EPoolv *	e_poolv_set			(EPoolv *poolv,
						 gint index,
						 gchar *str,
						 gint freeit);
const gchar *	e_poolv_get			(EPoolv *poolv,
						 gint index);
void		e_poolv_destroy			(EPoolv *poolv);

G_END_DECLS

#endif /* E_POOLV_H */
