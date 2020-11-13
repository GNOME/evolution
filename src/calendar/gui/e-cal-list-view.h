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
 *		Hans Petter Jansson  <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CAL_LIST_VIEW_H_
#define _E_CAL_LIST_VIEW_H_

#include <time.h>
#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include "e-calendar-view.h"

/*
 * ECalListView - displays calendar events in an ETable.
 */

/* Standard GObject macros */
#define E_TYPE_CAL_LIST_VIEW \
	(e_cal_list_view_get_type ())
#define E_CAL_LIST_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CAL_LIST_VIEW, ECalListView))
#define E_CAL_LIST_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CAL_LIST_VIEW, ECalListViewClass))
#define E_IS_CAL_LIST_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CAL_LIST_VIEW))
#define E_IS_CAL_LIST_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CAL_LIST_VIEW))
#define E_CAL_LIST_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CAL_LIST_VIEW, ECalListViewClass))

G_BEGIN_DECLS

typedef struct _ECalListView ECalListView;
typedef struct _ECalListViewClass ECalListViewClass;
typedef struct _ECalListViewPrivate ECalListViewPrivate;

struct _ECalListView {
	ECalendarView parent;

	ECalListViewPrivate *priv;
};

struct _ECalListViewClass {
	ECalendarViewClass parent_class;
};

GType		e_cal_list_view_get_type	(void);
ECalendarView *	e_cal_list_view_new		(ECalModel *cal_model);
ETable *	e_cal_list_view_get_table	(ECalListView *cal_list_view);
gboolean	e_cal_list_view_is_editing	(ECalListView *eclv);

G_END_DECLS

#endif /* _E_CAL_LIST_VIEW_H_ */
