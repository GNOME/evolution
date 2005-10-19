/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@ximian.com>
 *  Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright 2000, Ximian, Inc.
 * Copyright 2000, Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#ifndef _E_MEMO_TABLE_H_
#define _E_MEMO_TABLE_H_

#include <gtk/gtktable.h>
#include <table/e-table-scrolled.h>
#include <widgets/misc/e-cell-date-edit.h>
#include "e-activity-handler.h"
#include "e-cal-model.h"

G_BEGIN_DECLS

/*
 * EMemoTable - displays the iCalendar objects in a table (an ETable).
 * Used for memo events and tasks.
 */


#define E_MEMO_TABLE(obj)          GTK_CHECK_CAST (obj, e_memo_table_get_type (), EMemoTable)
#define E_MEMO_TABLE_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, e_memo_table_get_type (), EMemoTableClass)
#define E_IS_MEMO_TABLE(obj)       GTK_CHECK_TYPE (obj, e_memo_table_get_type ())


typedef struct _EMemoTable       EMemoTable;
typedef struct _EMemoTableClass  EMemoTableClass;


struct _EMemoTable {
	GtkTable table;

	/* The model that we use */
	ECalModel *model;

	GtkWidget *etable;

	/* Fields used for cut/copy/paste */
	icalcomponent *tmp_vcal;

	/* Activity ID for the EActivityHandler (i.e. the status bar).  */
	EActivityHandler *activity_handler;
	guint activity_id;
};

struct _EMemoTableClass {
	GtkTableClass parent_class;

	/* Notification signals */
	void (* user_created) (EMemoTable *memo_table);
};


GtkType	   e_memo_table_get_type (void);
GtkWidget* e_memo_table_new	(void);

ECalModel *e_memo_table_get_model (EMemoTable *memo_table);

ETable    *e_memo_table_get_table (EMemoTable *memo_table);

void       e_memo_table_open_selected (EMemoTable *memo_table);
void       e_memo_table_delete_selected (EMemoTable *memo_table);

GSList    *e_memo_table_get_selected (EMemoTable *memo_table);

/* Clipboard related functions */
void       e_memo_table_cut_clipboard       (EMemoTable *memo_table);
void       e_memo_table_copy_clipboard      (EMemoTable *memo_table);
void       e_memo_table_paste_clipboard     (EMemoTable *memo_table);

/* These load and save the state of the table (headers shown etc.) to/from
   the given file. */
void	   e_memo_table_load_state		(EMemoTable *memo_table,
						 gchar		*filename);
void	   e_memo_table_save_state		(EMemoTable *memo_table,
						 gchar		*filename);

void       e_memo_table_set_activity_handler (EMemoTable *memo_table,
					      EActivityHandler *activity_handler);
void       e_memo_table_set_status_message (EMemoTable *memo_table,
					    const gchar *message);

G_END_DECLS

#endif /* _E_MEMO_TABLE_H_ */
