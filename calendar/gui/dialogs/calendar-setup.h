/* Evolution calendar - Calendar properties dialogs.
 *
 * Copyright (C) 2004 Novell, Inc.
 *
 * Author: Hans Petter Jansson <hpj@ximian.com>
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

#ifndef CALENDAR_CONFIG_H
#define CALENDAR_CONFIG_H

#include <gtk/gtkwindow.h>

gboolean calendar_setup_new_calendar   (GtkWindow *parent);
gboolean calendar_setup_edit_calendar  (GtkWindow *parent, ESource *source);

gboolean calendar_setup_new_task_list  (GtkWindow *parent);
gboolean calendar_setup_edit_task_list (GtkWindow *parent, ESource *source);

#endif
