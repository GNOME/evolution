/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

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
