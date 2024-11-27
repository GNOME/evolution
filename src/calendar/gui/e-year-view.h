/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_YEAR_VIEW_H
#define E_YEAR_VIEW_H

#include <calendar/gui/e-calendar-view.h>

/* Standard GObject macros */
#define E_TYPE_YEAR_VIEW \
	(e_year_view_get_type ())
#define E_YEAR_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_YEAR_VIEW, EYearView))
#define E_YEAR_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_YEAR_VIEW, EYearViewClass))
#define E_IS_YEAR_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_YEAR_VIEW))
#define E_IS_YEAR_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_YEAR_VIEW))
#define E_YEAR_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_YEAR_VIEW, EYearViewClass))

G_BEGIN_DECLS

typedef struct _EYearView EYearView;
typedef struct _EYearViewClass EYearViewClass;
typedef struct _EYearViewPrivate EYearViewPrivate;

struct _EYearView {
	ECalendarView parent;
	EYearViewPrivate *priv;
};

struct _EYearViewClass {
	ECalendarViewClass parent_class;
};

GType		e_year_view_get_type			(void) G_GNUC_CONST;
ECalendarView *	e_year_view_new				(ECalModel *model);
void		e_year_view_set_preview_visible		(EYearView *self,
							 gboolean value);
gboolean	e_year_view_get_preview_visible		(EYearView *self);
void		e_year_view_set_preview_orientation	(EYearView *self,
							 GtkOrientation value);
GtkOrientation	e_year_view_get_preview_orientation	(EYearView *self);
void		e_year_view_set_use_24hour_format	(EYearView *self,
							 gboolean value);
gboolean	e_year_view_get_use_24hour_format	(EYearView *self);
void		e_year_view_set_highlight_today		(EYearView *self,
							 gboolean value);
gboolean	e_year_view_get_highlight_today		(EYearView *self);
void		e_year_view_update_actions		(EYearView *self);

G_END_DECLS

#endif /* E_YEAR_VIEW_H */
