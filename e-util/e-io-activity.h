/*
 * e-io-activity.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_IO_ACTIVITY_H
#define E_IO_ACTIVITH_H

#include <gio/gio.h>
#include <e-util/e-activity.h>

/* Standard GObject macros */
#define E_TYPE_IO_ACTIVITY \
	(e_io_activity_get_type ())
#define E_IO_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_IO_ACTIVITY, EIOActivity))
#define E_IO_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_IO_ACTIVITY, EIOActivityClass))
#define E_IS_IO_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_IO_ACTIVITY))
#define E_IS_IO_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_IO_ACTIVITY))
#define E_IS_IO_ACTIVITY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_IO_ACTIVITY, EIOActivityClass))

G_BEGIN_DECLS

typedef struct _EIOActivity EIOActivity;
typedef struct _EIOActivityClass EIOActivityClass;
typedef struct _EIOActivityPrivate EIOActivityPrivate;

struct _EIOActivity {
	EActivity parent;
	EIOActivityPrivate *priv;
};

struct _EIOActivityClass {
	EActivityClass parent_class;
};

GType		e_io_activity_get_type		(void);
EActivity *	e_io_activity_new		(const gchar *primary_text,
						 GAsyncResult *async_result,
						 GCancellable *cancellable);
GAsyncResult *	e_io_activity_get_async_result	(EIOActivity *io_activity);
void		e_io_activity_set_async_result	(EIOActivity *io_activity,
						 GAsyncResult *async_result);
GCancellable *	e_io_activity_get_cancellable	(EIOActivity *io_activity);
void		e_io_activity_set_cancellable	(EIOActivity *io_activity,
						 GCancellable *cancellable);

G_END_DECLS

#endif /* E_IO_ACTIVITY_H */
