/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_CAL_SHELL_CONTENT_H
#define E_CAL_SHELL_CONTENT_H

#include <shell/e-shell-content.h>
#include <shell/e-shell-searchbar.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-memo-table.h>
#include <calendar/gui/e-task-table.h>
#include <calendar/gui/e-calendar-view.h>

#include "e-cal-base-shell-content.h"

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

typedef enum {
	E_CAL_VIEW_KIND_DAY = 0,
	E_CAL_VIEW_KIND_WORKWEEK,
	E_CAL_VIEW_KIND_WEEK,
	E_CAL_VIEW_KIND_MONTH,
	E_CAL_VIEW_KIND_YEAR,
	E_CAL_VIEW_KIND_LIST,
	E_CAL_VIEW_KIND_LAST
} ECalViewKind;

typedef struct _ECalShellContent ECalShellContent;
typedef struct _ECalShellContentClass ECalShellContentClass;
typedef struct _ECalShellContentPrivate ECalShellContentPrivate;

struct _ECalShellContent {
	ECalBaseShellContent parent;
	ECalShellContentPrivate *priv;
};

struct _ECalShellContentClass {
	ECalBaseShellContentClass parent_class;
};

GType		e_cal_shell_content_get_type		(void);
void		e_cal_shell_content_type_register	(GTypeModule *type_module);
GtkWidget *	e_cal_shell_content_new			(EShellView *shell_view);

gboolean	e_cal_shell_content_get_initialized	(ECalShellContent *cal_shell_content);
GtkNotebook *	e_cal_shell_content_get_calendar_notebook
							(ECalShellContent *cal_shell_content);
EMemoTable *	e_cal_shell_content_get_memo_table	(ECalShellContent *cal_shell_content);
ETaskTable *	e_cal_shell_content_get_task_table	(ECalShellContent *cal_shell_content);
EShellSearchbar *
		e_cal_shell_content_get_searchbar	(ECalShellContent *cal_shell_content);
void		e_cal_shell_content_set_current_view_id	(ECalShellContent *cal_shell_content,
							 ECalViewKind view_kind);
ECalViewKind	e_cal_shell_content_get_current_view_id	(ECalShellContent *cal_shell_content);
ECalendarView *	e_cal_shell_content_get_calendar_view	(ECalShellContent *cal_shell_content,
							 ECalViewKind view_kind);
ECalendarView *	e_cal_shell_content_get_current_calendar_view
							(ECalShellContent *cal_shell_content);
void		e_cal_shell_content_save_state		(ECalShellContent *cal_shell_content);
void		e_cal_shell_content_get_current_range	(ECalShellContent *cal_shell_content,
							 time_t *range_start,
							 time_t *range_end);
void		e_cal_shell_content_get_current_range_dates
							(ECalShellContent *cal_shell_content,
							 GDate *range_start,
							 GDate *range_end);
void		e_cal_shell_content_move_view_range	(ECalShellContent *cal_shell_content,
							 ECalendarViewMoveType move_type,
							 time_t exact_date);
void		e_cal_shell_content_update_filters	(ECalShellContent *cal_shell_content,
							 const gchar *cal_filter,
							 time_t start_range,
							 time_t end_range);
void		e_cal_shell_content_update_tasks_filter	(ECalShellContent *cal_shell_content,
							 const gchar *cal_filter);
ECalDataModel *	e_cal_shell_content_get_list_view_data_model
							(ECalShellContent *cal_shell_content);
void		e_cal_shell_content_set_show_tag_vpane	(ECalShellContent *cal_shell_content,
							 gboolean show);
gboolean	e_cal_shell_content_get_show_tag_vpane	(ECalShellContent *cal_shell_content);

G_END_DECLS

#endif /* E_CAL_SHELL_CONTENT_H */
