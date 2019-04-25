/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Damon Chaplin <damon@ximian.com>
 *		Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MEMO_TABLE_H
#define E_MEMO_TABLE_H

#include <shell/e-shell-view.h>

#include "e-cal-model.h"

/*
 * EMemoTable - displays the iCalendar objects in a table (an ETable).
 *
 * XXX EMemoTable and ETaskTable have lots of duplicate code.  We should
 *     look at merging them, or at least bringing back ECalendarTable as
 *     a common base class.
 */

/* Standard GObject macros */
#define E_TYPE_MEMO_TABLE \
	(e_memo_table_get_type ())
#define E_MEMO_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MEMO_TABLE, EMemoTable))
#define E_MEMO_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MEMO_TABLE, EMemoTableClass))
#define E_IS_MEMO_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MEMO_TABLE))
#define E_IS_MEMO_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MEMO_TABLE))
#define E_MEMO_TABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MEMO_TABLE, EMemoTableClass))

G_BEGIN_DECLS

typedef struct _EMemoTable EMemoTable;
typedef struct _EMemoTableClass EMemoTableClass;
typedef struct _EMemoTablePrivate EMemoTablePrivate;

struct _EMemoTable {
	ETable parent;

	EMemoTablePrivate *priv;
};

struct _EMemoTableClass {
	ETableClass parent_class;

	/* Signals */
	void	(*open_component)		(EMemoTable *memo_table,
						 ECalModelComponent *comp_data);
	void	(*popup_event)			(EMemoTable *memo_table,
						 GdkEvent *event);
};

GType		e_memo_table_get_type		(void);
GtkWidget *	e_memo_table_new		(EShellView *shell_view,
						 ECalModel *model);
ECalModel *	e_memo_table_get_model		(EMemoTable *memo_table);
EShellView *	e_memo_table_get_shell_view	(EMemoTable *memo_table);
ICalTimezone *	e_memo_table_get_timezone	(EMemoTable *memo_table);
void		e_memo_table_set_timezone	(EMemoTable *memo_table,
						 const ICalTimezone *timezone);
gboolean	e_memo_table_get_use_24_hour_format
						(EMemoTable *memo_table);
void		e_memo_table_set_use_24_hour_format
						(EMemoTable *memo_table,
						 gboolean use_24_hour_format);
GSList *	e_memo_table_get_selected	(EMemoTable *memo_table);
GtkTargetList *	e_memo_table_get_copy_target_list
						(EMemoTable *memo_table);
GtkTargetList *	e_memo_table_get_paste_target_list
						(EMemoTable *memo_table);

G_END_DECLS

#endif /* E_MEMO_TABLE_H */
