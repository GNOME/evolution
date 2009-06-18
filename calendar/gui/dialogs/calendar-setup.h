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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __CALENDAR_SETUP_H__
#define __CALENDAR_SETUP_H__

#include <gtk/gtk.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-group.h>

G_BEGIN_DECLS

void calendar_setup_edit_calendar  (GtkWindow *parent, ESource *source, ESourceGroup *group);
void calendar_setup_new_calendar   (GtkWindow *parent);

void calendar_setup_edit_task_list (GtkWindow *parent, ESource *source);
void calendar_setup_new_task_list  (GtkWindow *parent);

void calendar_setup_edit_memo_list (GtkWindow *parent, ESource *source);
void calendar_setup_new_memo_list (GtkWindow *parent);

G_END_DECLS

#endif /* __CALENDAR_SETUP_H__ */
