/* Evolution calendar - iCalendar file backend for tasks
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <libgnome/gnome-i18n.h>
#include "cal-backend-file-todos.h"



/* Private part of the CalBackendFileTodos structure */
struct _CalBackendFileTodosPrivate {
};



static void cal_backend_file_todos_class_init (CalBackendFileTodosClass *class);
static void cal_backend_file_todos_init (CalBackendFileTodos *cbfile, CalBackendFileTodosClass *class);
static void cal_backend_file_todos_dispose (GObject *object);
static void cal_backend_file_todos_finalize (GObject *object);

static CalBackendFileClass *parent_class;



/**
 * cal_backend_file_todos_get_type:
 * @void: 
 * 
 * Registers the #CalBackendFileTodos class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #CalBackendFileTodos class.
 **/
GType
cal_backend_file_todos_get_type (void)
{
	static GType cal_backend_file_todos_type = 0;

	if (!cal_backend_file_todos_type) {
		static GTypeInfo info = {
                        sizeof (CalBackendFileTodosClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) cal_backend_file_todos_class_init,
                        NULL, NULL,
                        sizeof (CalBackendFileTodos),
                        0,
                        (GInstanceInitFunc) cal_backend_file_todos_init
                };
		cal_backend_file_todos_type = g_type_register_static (CAL_BACKEND_FILE_TYPE,
								      "CalBackendFileTodos", &info, 0);
	}

	return cal_backend_file_todos_type;
}

/* Class initialization function for the file backend */
static void
cal_backend_file_todos_class_init (CalBackendFileTodosClass *klass)
{
	GObjectClass *object_class;
	CalBackendClass *backend_class;

	object_class = G_OBJECT_CLASS (klass);
	backend_class = CAL_BACKEND_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->dispose = cal_backend_file_todos_dispose;
	object_class->finalize = cal_backend_file_todos_finalize;

//	backend_class->get_uri = cal_backend_file_todos_get_uri;
}

/* Object initialization function for the file backend */
static void
cal_backend_file_todos_init (CalBackendFileTodos *cbfile, CalBackendFileTodosClass *class)
{
	CalBackendFileTodosPrivate *priv;

	priv = g_new0 (CalBackendFileTodosPrivate, 1);
	cbfile->priv = priv;

	cal_backend_file_set_file_name (CAL_BACKEND_FILE (cbfile), "tasks.ics");
}

/* Dispose handler for the file backend */
static void
cal_backend_file_todos_dispose (GObject *object)
{
	CalBackendFileTodos *cbfile;
	CalBackendFileTodosPrivate *priv;

	cbfile = CAL_BACKEND_FILE_TODOS (object);
	priv = cbfile->priv;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

/* Finalize handler for the file backend */
static void
cal_backend_file_todos_finalize (GObject *object)
{
	CalBackendFileTodos *cbfile;
	CalBackendFileTodosPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAL_BACKEND_FILE_TODOS (object));

	cbfile = CAL_BACKEND_FILE_TODOS (object);
	priv = cbfile->priv;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

