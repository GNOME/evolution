/*
 * Authors: David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 2004 Novell, Inc (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#ifndef __CALENDAR_SETUP_H__
#define __CALENDAR_SETUP_H__

struct _GtkWindow;
struct _ESource;

#ifdef __cplusplus
extern "C" {
#pragma }
#endif

void calendar_setup_edit_calendar  (struct _GtkWindow *parent, struct _ESource *source, struct _ESourceGroup *group);
void calendar_setup_new_calendar   (struct _GtkWindow *parent);

void calendar_setup_edit_task_list (struct _GtkWindow *parent, struct _ESource *source);
void calendar_setup_new_task_list  (struct _GtkWindow *parent);

#ifdef __cplusplus
}
#endif

#endif /* __CALENDAR_SETUP_H__ */
