/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* tasks-control.h
 *
 * Copyright (C) 2000, 2001, 2002, 2003  Ximian, Inc.
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
 * Authors: Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 */

#ifndef _TASKS_CONTROL_H_
#define _TASKS_CONTROL_H_

#include "e-tasks.h"

BonoboControl *tasks_control_new                (void);
void           tasks_control_activate           (BonoboControl *control, ETasks *tasks);
void           tasks_control_deactivate         (BonoboControl *control, ETasks *tasks);
void           tasks_control_sensitize_commands (BonoboControl *control, ETasks *tasks, int n_selected);

#endif /* _TASKS_CONTROL_H_ */
