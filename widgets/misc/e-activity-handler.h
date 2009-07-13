/*
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
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_ACTIVITY_HANDLER_H_
#define _E_ACTIVITY_HANDLER_H_

#include "e-task-bar.h"
#include "e-util/e-logger.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define E_TYPE_ACTIVITY_HANDLER			(e_activity_handler_get_type ())
#define E_ACTIVITY_HANDLER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_ACTIVITY_HANDLER, EActivityHandler))
#define E_ACTIVITY_HANDLER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_ACTIVITY_HANDLER, EActivityHandlerClass))
#define E_IS_ACTIVITY_HANDLER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_ACTIVITY_HANDLER))
#define E_IS_ACTIVITY_HANDLER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_ACTIVITY_HANDLER))

typedef struct _EActivityHandler        EActivityHandler;
typedef struct _EActivityHandlerPrivate EActivityHandlerPrivate;
typedef struct _EActivityHandlerClass   EActivityHandlerClass;

#define EAH_ICON_INFO  "stock_dialog-info"
#define EAH_ICON_ERROR "stock_dialog-warning"

struct _EActivityHandler {
	GObject parent;

	EActivityHandlerPrivate *priv;
};

struct _EActivityHandlerClass {
	GObjectClass parent_class;
};

GType    e_activity_handler_get_type  (void);

EActivityHandler *e_activity_handler_new  (void);

void  e_activity_handler_attach_task_bar  (EActivityHandler *activity_hanlder,
					   ETaskBar         *task_bar);

void  e_activity_handler_set_message   (EActivityHandler *activity_handler,
					const gchar       *message);

void  e_activity_handler_unset_message (EActivityHandler *activity_handler);

guint  e_activity_handler_operation_started  (EActivityHandler *activity_handler,
					      const gchar       *component_id,
					      const gchar       *information,
					      gboolean          cancellable);
guint  e_activity_handler_cancelable_operation_started  (EActivityHandler *activity_handler,
						      const gchar       *component_id,
						      const gchar       *information,
						      gboolean          cancellable,
						      void (*cancel_func)(gpointer),
						      gpointer user_data);

void  e_activity_handler_operation_progressing  (EActivityHandler *activity_handler,
						 guint             activity_id,
						 const gchar       *information,
						 double            progress);

void  e_activity_handler_operation_finished  (EActivityHandler *activity_handler,
					      guint             activity_id);

void e_activity_handler_set_logger (EActivityHandler *handler, ELogger *logger);
guint e_activity_handler_make_error (EActivityHandler *activity_handler,
				      const gchar *component_id,
				      gint error_type,
				      GtkWidget  *error);
void
e_activity_handler_operation_set_error (EActivityHandler *activity_handler,
                                          guint activity_id,
                                          GtkWidget *error);

void
e_activity_handler_set_error_flush_time (EActivityHandler *handler, gint time);

G_END_DECLS

#endif /* _E_ACTIVITY_HANDLER_H_ */
