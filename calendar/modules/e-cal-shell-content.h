/*
 * e-cal-shell-content.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_CAL_SHELL_CONTENT_H
#define E_CAL_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-memo-table.h>
#include <calendar/gui/gnome-cal.h>
#include <widgets/menus/gal-view-instance.h>

/* Standard GObject macros */
#define E_TYPE_CAL_SHELL_CONTENT \
	(e_cal_shell_content_get_type ())
#define E_CAL_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_SHELL_CONTENT, ECalShellContent))
#define E_CAL_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_SHELL_CONTENT, ECalShellContentClass))
#define E_IS_CAL_SHELL_CONTENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_SHELL_CONTENT))
#define E_IS_CAL_SHELL_CONTENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_SHELL_CONTENT))
#define E_CAL_SHELL_CONTENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_SHELL_CONTENT, ECalShellContentClass))

G_BEGIN_DECLS

typedef struct _ECalShellContent ECalShellContent;
typedef struct _ECalShellContentClass ECalShellContentClass;
typedef struct _ECalShellContentPrivate ECalShellContentPrivate;

struct _ECalShellContent {
	EShellContent parent;
	ECalShellContentPrivate *priv;
};

struct _ECalShellContentClass {
	EShellContentClass parent_class;
};

GType		e_cal_shell_content_get_type	(void);
GtkWidget *	e_cal_shell_content_new	(EShellView *shell_view);
GnomeCalendar *	e_cal_shell_content_get_calendar(ECalShellContent *cal_shell_content);
EMemoTable *	e_cal_shell_content_get_memo_table
						(ECalShellContent *cal_shell_content);
ECalendarTable *e_cal_shell_content_get_task_table
						(ECalShellContent *cal_shell_content);
icaltimezone *	e_cal_shell_content_get_timezone(ECalShellContent *cal_shell_content);
GalViewInstance *
		e_cal_shell_content_get_view_instance
						(ECalShellContent *cal_shell_content);
void		e_cal_shell_content_copy_clipboard
						(ECalShellContent *cal_shell_content);
void		e_cal_shell_content_cut_clipboard
						(ECalShellContent *cal_shell_content);
void		e_cal_shell_content_paste_clipboard
						(ECalShellContent *cal_shell_content);
void		e_cal_shell_content_delete_selection
						(ECalShellContent *cal_shell_content);
void		e_cal_shell_content_delete_selected_occurrence
						(ECalShellContent *cal_shell_content);

G_END_DECLS

#endif /* E_CAL_SHELL_CONTENT_H */
