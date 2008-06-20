/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-history.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_HISTORY_H_
#define _E_HISTORY_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_HISTORY			(e_history_get_type ())
#define E_HISTORY(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_HISTORY, EHistory))
#define E_HISTORY_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_HISTORY, EHistoryClass))
#define E_IS_HISTORY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_HISTORY))
#define E_IS_HISTORY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_HISTORY))


typedef struct _EHistory        EHistory;
typedef struct _EHistoryPrivate EHistoryPrivate;
typedef struct _EHistoryClass   EHistoryClass;

struct _EHistory {
	GtkObject parent;

	EHistoryPrivate *priv;
};

struct _EHistoryClass {
	GtkObjectClass parent_class;
};


typedef void (* EHistoryItemFreeFunc) (void *data);


GType      e_history_get_type (void);

void      e_history_construct  (EHistory             *history,
				EHistoryItemFreeFunc  item_free_function);
EHistory *e_history_new        (EHistoryItemFreeFunc  item_free_function);

void     *e_history_prev         (EHistory *history);
gboolean  e_history_has_prev     (EHistory *history);

void     *e_history_next         (EHistory *history);
gboolean  e_history_has_next     (EHistory *history);

void     *e_history_get_current  (EHistory *history);

void      e_history_add          (EHistory *history,
				  void     *data);

void  e_history_remove_matching  (EHistory     *history,
				  const void   *data,
				  GCompareFunc  compare_func);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_HISTORY_H_ */
