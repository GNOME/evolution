/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-bar.h
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_TASK_BAR_H_
#define _E_TASK_BAR_H_

#include "e-task-widget.h"

#include <gtk/gtkhbox.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_TASK_BAR			(e_task_bar_get_type ())
#define E_TASK_BAR(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_TASK_BAR, ETaskBar))
#define E_TASK_BAR_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_TASK_BAR, ETaskBarClass))
#define E_IS_TASK_BAR(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_TASK_BAR))
#define E_IS_TASK_BAR_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_TASK_BAR))


typedef struct _ETaskBar        ETaskBar;
typedef struct _ETaskBarPrivate ETaskBarPrivate;
typedef struct _ETaskBarClass   ETaskBarClass;

struct _ETaskBar {
	GtkHBox          parent;
	
	ETaskBarPrivate *priv;
};

struct _ETaskBarClass {
	GtkHBoxClass parent_class;
};


GtkType      e_task_bar_get_type         (void);
void         e_task_bar_construct        (ETaskBar    *task_bar);
GtkWidget   *e_task_bar_new              (void);

void         e_task_bar_set_message      (ETaskBar    *task_bar,
					  const char  *message);
void         e_task_bar_unset_message    (ETaskBar    *task_bar);

void         e_task_bar_prepend_task     (ETaskBar    *task_bar,
					  ETaskWidget *task_widget);
void         e_task_bar_remove_task      (ETaskBar    *task_bar,
					  int          n);

ETaskWidget *e_task_bar_get_task_widget  (ETaskBar    *task_bar,
					  int          n);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_TASK_BAR_H_ */
