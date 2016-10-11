/*
 * e-emoticon-action.h
 *
 * Copyright (C) 2008 Novell, Inc.
 * Copyright (C) 2012 Dan Vrátil <dvratil@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_EMOTICON_ACTION_H
#define E_EMOTICON_ACTION_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_EMOTICON_ACTION \
	(e_emoticon_action_get_type ())
#define E_EMOTICON_ACTION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_EMOTICON_ACTION, EEmoticonAction))
#define E_EMOTICON_ACTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_EMOTICON_ACTION, EEmoticonActionClass))
#define E_IS_EMOTICON_ACTION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_EMOTICON_ACTION))
#define E_IS_EMOTICON_ACTION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_EMOTICON_ACTION))
#define E_EMOTICON_ACTION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_EMOTICON_ACTION, EEmoticonActionClass))

G_BEGIN_DECLS

typedef struct _EEmoticonAction EEmoticonAction;
typedef struct _EEmoticonActionClass EEmoticonActionClass;
typedef struct _EEmoticonActionPrivate EEmoticonActionPrivate;

struct _EEmoticonAction {
	GtkAction parent;
	EEmoticonActionPrivate *priv;
};

struct _EEmoticonActionClass {
	GtkActionClass parent_class;
};

GType		e_emoticon_action_get_type	(void) G_GNUC_CONST;
GtkAction *	e_emoticon_action_new		(const gchar *name,
						 const gchar *label,
						 const gchar *tooltip,
						 const gchar *stock_id);

G_END_DECLS

#endif /* E_EMOTICON_ACTION_H */
