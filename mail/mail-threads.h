/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Peter Williams (peterw@helixcode.com)
 *
 *  Copyright 2000, Helix Code, Inc. (http://www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _MAIL_THREADS_H_
#define _MAIL_THREADS_H_

#include <camel/camel-exception.h>
#include <stdlib.h>		/*size_t */

/* Returns a g_strdup'ed string that describes what's going to happen,
 * tersely but specifically.
 */
typedef gchar *(*mail_op_describe_func) (gpointer /*input_data*/, gboolean /*gerund*/);
typedef void (*mail_op_func) (gpointer, gpointer, CamelException *);

typedef struct _mail_operation_spec
{
	mail_op_describe_func describe;
	size_t datasize;
	mail_op_func setup;
	mail_op_func callback;
	mail_op_func cleanup;
}
mail_operation_spec;

/* Schedule to operation to happen eventually */

gboolean mail_operation_queue (const mail_operation_spec * spec,
			       gpointer input, gboolean free_in_data);

/* User interface hooks for the other thread */

void mail_op_set_percentage (gfloat percentage);
void mail_op_hide_progressbar (void);
void mail_op_show_progressbar (void);
void
mail_op_set_message (gchar * fmt, ...) G_GNUC_PRINTF (1, 2);
void mail_op_error (gchar * fmt, ...) G_GNUC_PRINTF (1, 2);
gboolean mail_op_get_password (gchar * prompt, gboolean secret,
			       gchar ** dest);

/* Wait for the async operations to finish */
void mail_operation_wait_for_finish (void);
gboolean mail_operations_are_executing (void);
void mail_operations_terminate (void);

#endif /* defined _MAIL_THREADS_H_ */
