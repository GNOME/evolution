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
 *		Damon Chaplin <damon@ximian.com>
 *		Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_MEMO_TABLE_H_
#define _E_MEMO_TABLE_H_

#include <shell/e-shell-view.h>
#include <table/e-table-scrolled.h>
#include <table/e-cell-date-edit.h>
#include "e-cal-model.h"

/*
 * EMemoTable - displays the iCalendar objects in a table (an ETable).
 * Used for memo events and tasks.
 *
 * XXX We should look at merging this back into ECalendarTable, or at
 *     least making ECalendarTable subclassable so we don't have so
 *     much duplicate code.
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
	GtkTable parent;

	GtkWidget *etable;

	/* The ECell used to view & edit dates. */
	ECellDateEdit *dates_cell;

	/* Fields used for cut/copy/paste */
	icalcomponent *tmp_vcal;

	EMemoTablePrivate *priv;
};

struct _EMemoTableClass {
	GtkTableClass parent_class;

	/* Signals */
	void	(*open_component)		(EMemoTable *memo_table,
						 ECalModelComponent *comp_data);
	void	(*popup_event)			(EMemoTable *memo_table,
						 GdkEvent *event);
	void	(*status_message)		(EMemoTable *memo_table,
						 const gchar *message,
						 gdouble percent);
	void	(*user_created)			(EMemoTable *memo_table);
};

GType		e_memo_table_get_type		(void);
GtkWidget *	e_memo_table_new		(EShellView *shell_view,
						 ECalModel *model);
ECalModel *	e_memo_table_get_model		(EMemoTable *memo_table);
ETable *	e_memo_table_get_table		(EMemoTable *memo_table);
EShellView *	e_memo_table_get_shell_view	(EMemoTable *memo_table);
void		e_memo_table_delete_selected	(EMemoTable *memo_table);
GSList *	e_memo_table_get_selected	(EMemoTable *memo_table);

/* Clipboard related functions */
void		e_memo_table_cut_clipboard	(EMemoTable *memo_table);
void		e_memo_table_copy_clipboard	(EMemoTable *memo_table);
void		e_memo_table_paste_clipboard	(EMemoTable *memo_table);

/* These load and save the state of the table (headers shown etc.) to/from
   the given file. */
void		e_memo_table_load_state		(EMemoTable *memo_table,
						 const gchar *filename);
void		e_memo_table_save_state		(EMemoTable *memo_table,
						 const gchar *filename);

G_END_DECLS

#endif /* _E_MEMO_TABLE_H_ */
