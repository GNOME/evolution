/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#ifndef _TASK_EDITOR_H_
#define _TASK_EDITOR_H_

#include <gtk/gtkobject.h>
#include <libgnome/gnome-defs.h>
#include <bonobo.h>

BEGIN_GNOME_DECLS


#define TASK_EDITOR(obj)          GTK_CHECK_CAST (obj, task_editor_get_type (), TaskEditor)
#define TASK_EDITOR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, task_editor_get_type (), TaskEditorClass)
#define IS_TASK_EDITOR(obj)       GTK_CHECK_TYPE (obj, task_editor_get_type ())


typedef struct _TaskEditor       TaskEditor;
typedef struct _TaskEditorClass  TaskEditorClass;

struct _TaskEditor
{
	GtkObject object;

	/* Private data */
	gpointer priv;
};

struct _TaskEditorClass
{
	GtkObjectClass parent_class;
};


GtkType	    task_editor_get_type	(void);
TaskEditor* task_editor_construct	(TaskEditor	*tedit);
TaskEditor* task_editor_new		(void);

void	    task_editor_set_cal_client	(TaskEditor	*tedit,
					 CalClient	*client);
void	    task_editor_set_todo_object	(TaskEditor	*tedit,
					 CalComponent	*comp);


END_GNOME_DECLS

#endif /* _TASK_EDITOR_H_ */
