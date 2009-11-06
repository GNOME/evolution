/*
 * e-timeout-activity.h
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

#ifndef E_TIMEOUT_ACTIVITY_H
#define E_TIMEOUT_ACTIVITY_H

#include <e-util/e-activity.h>

/* Standard GObject macros */
#define E_TYPE_TIMEOUT_ACTIVITY \
	(e_timeout_activity_get_type ())
#define E_TIMEOUT_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_TIMEOUT_ACTIVITY, ETimeoutActivity))
#define E_TIMEOUT_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_TIMEOUT_ACTIVITY, ETimeoutActivityClass))
#define E_IS_TIMEOUT_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_TIMEOUT_ACTIVITY))
#define E_IS_TIMEOUT_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_TIMEOUT_ACTIVITY))
#define E_TIMEOUT_ACTIVITY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_TIMEOUT_ACTIVITY, ETimeoutActivityClass))

G_BEGIN_DECLS

typedef struct _ETimeoutActivity ETimeoutActivity;
typedef struct _ETimeoutActivityClass ETimeoutActivityClass;
typedef struct _ETimeoutActivityPrivate ETimeoutActivityPrivate;

struct _ETimeoutActivity {
	EActivity parent;
	ETimeoutActivityPrivate *priv;
};

struct _ETimeoutActivityClass {
	EActivityClass parent_class;

	/* Signals */
	void		(*timeout)	(ETimeoutActivity *timeout_activity);
};

GType		e_timeout_activity_get_type	(void);
EActivity *	e_timeout_activity_new		(const gchar *primary_text);
EActivity *	e_timeout_activity_newv		(const gchar *format,
						 ...) G_GNUC_PRINTF (1, 2);
void		e_timeout_activity_set_timeout	(ETimeoutActivity *timeout_activity,
						 guint seconds);

G_END_DECLS

#endif /* E_TIMEOUT_ACTIVITY_H */
