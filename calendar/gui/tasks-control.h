/*
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
 * Authors:
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _TASKS_CONTROL_H_
#define _TASKS_CONTROL_H_

#include "e-tasks.h"

BonoboControl *tasks_control_new                (void);
void           tasks_control_activate           (BonoboControl *control, ETasks *tasks);
void           tasks_control_deactivate         (BonoboControl *control, ETasks *tasks);
void           tasks_control_sensitize_commands (BonoboControl *control, ETasks *tasks, gint n_selected);

#endif /* _TASKS_CONTROL_H_ */
