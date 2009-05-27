/*
 * e-file-activity.h
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

#ifndef E_FILE_ACTIVITY_H
#define E_FILE_ACTIVITY_H

#include <gio/gio.h>
#include <widgets/misc/e-activity.h>

/* Standard GObject macros */
#define E_TYPE_FILE_ACTIVITY \
	(e_file_activity_get_type ())
#define E_FILE_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILE_ACTIVITY, EFileActivity))
#define E_FILE_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILE_ACTIVITY, EFileActivityClass))
#define E_IS_FILE_ACTIVITY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILE_ACTIVITY))
#define E_IS_FILE_ACTIVITY_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILE_ACTIVITY))
#define E_FILE_ACTIVITY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILE_ACTIVITY, EFileActivityClass))

G_BEGIN_DECLS

typedef struct _EFileActivity EFileActivity;
typedef struct _EFileActivityClass EFileActivityClass;
typedef struct _EFileActivityPrivate EFileActivityPrivate;

struct _EFileActivity {
	EActivity parent;
	EFileActivityPrivate *priv;
};

struct _EFileActivityClass {
	EActivityClass parent_class;
};

GType		e_file_activity_get_type	(void);
EActivity *	e_file_activity_new		(const gchar *primary_text);
EActivity *	e_file_activity_newv		(const gchar *format,
						 ...) G_GNUC_PRINTF (1, 2);
GCancellable *	e_file_activity_get_cancellable	(EFileActivity *file_activity);
void		e_file_activity_set_cancellable (EFileActivity *file_activity,
						 GCancellable *cancellable);
GFile *		e_file_activity_get_file	(EFileActivity *file_activity);
void		e_file_activity_set_file	(EFileActivity *file_activity,
						 GFile *file);
GAsyncResult *	e_file_activity_get_result	(EFileActivity *file_activity);
void		e_file_activity_set_result	(EFileActivity *file_activity,
						 GAsyncResult *result);

/* This can be used as a GFileProgressCallback. */
void		e_file_activity_progress	(goffset current_num_bytes,
						 goffset total_num_bytes,
						 gpointer activity);

G_END_DECLS

#endif /* E_FILE_ACTIVITY_H */
