/*
 * e-month-view.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MONTH_VIEW_H
#define E_MONTH_VIEW_H

#include "e-week-view.h"

/* Standard GObject macros */
#define E_TYPE_MONTH_VIEW \
	(e_month_view_get_type ())
#define E_MONTH_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MONTH_VIEW, EMonthView))
#define E_MONTH_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((obj), E_TYPE_MONTH_VIEW, EMonthViewClass))
#define E_IS_MONTH_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MONTH_VIEW))
#define E_IS_MONTH_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MONTH_VIEW))
#define E_MONTH_VIEW_GET_CLASS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MONTH_VIEW, EMonthViewClass))

G_BEGIN_DECLS

typedef struct _EMonthView EMonthView;
typedef struct _EMonthViewClass EMonthViewClass;
typedef struct _EMonthViewPrivate EMonthViewPrivate;

struct _EMonthView {
	EWeekView parent;
	EMonthViewPrivate *priv;
};

struct _EMonthViewClass {
	EWeekViewClass parent_class;
};

GType		e_month_view_get_type		(void);
ECalendarView *	e_month_view_new		(ECalModel *model);

G_END_DECLS

#endif /* E_MONTH_VIEW_H */
