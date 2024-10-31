/*
 * SPDX-FileCopyrightText: (C) 2022 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_MONTH_WIDGET_H
#define E_MONTH_WIDGET_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MONTH_WIDGET \
	(e_month_widget_get_type ())
#define E_MONTH_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MONTH_WIDGET, EMonthWidget))
#define E_MONTH_WIDGET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MONTH_WIDGET, EMonthWidgetClass))
#define E_IS_MONTH_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MONTH_WIDGET))
#define E_IS_MONTH_WIDGET_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MONTH_WIDGET))
#define E_MONTH_WIDGET_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MONTH_WIDGET, EMonthWidgetClass))

#define E_MONTH_WIDGET_CSS_CLASS_BOLD "emw-bold"
#define E_MONTH_WIDGET_CSS_CLASS_ITALIC "emw-italic"
#define E_MONTH_WIDGET_CSS_CLASS_UNDERLINE "emw-underline"
#define E_MONTH_WIDGET_CSS_CLASS_HIGHLIGHT "emw-highlight"

G_BEGIN_DECLS

typedef struct _EMonthWidget EMonthWidget;
typedef struct _EMonthWidgetClass EMonthWidgetClass;
typedef struct _EMonthWidgetPrivate EMonthWidgetPrivate;

struct _EMonthWidget {
	GtkEventBox parent;
	EMonthWidgetPrivate *priv;
};

struct _EMonthWidgetClass {
	GtkEventBoxClass parent_class;

	void	(* changed)		(EMonthWidget *self);
	void	(* day_clicked)		(EMonthWidget *self,
					 GdkEventButton *event,
					 guint year,
					 gint /* GDateMonth */ month,
					 guint day);

	/* Padding for future expansion */
	gpointer padding[12];
};

GType		e_month_widget_get_type			(void) G_GNUC_CONST;
GtkWidget *	e_month_widget_new			(void);
void		e_month_widget_set_month		(EMonthWidget *self,
							 GDateMonth month,
							 guint year);
void		e_month_widget_get_month		(EMonthWidget *self,
							 GDateMonth *out_month,
							 guint *out_year);
void		e_month_widget_set_week_start_day	(EMonthWidget *self,
							 GDateWeekday value);
GDateWeekday	e_month_widget_get_week_start_day	(EMonthWidget *self);
void		e_month_widget_set_show_week_numbers	(EMonthWidget *self,
							 gboolean value);
gboolean	e_month_widget_get_show_week_numbers	(EMonthWidget *self);
void		e_month_widget_set_show_day_names	(EMonthWidget *self,
							 gboolean value);
gboolean	e_month_widget_get_show_day_names	(EMonthWidget *self);
void		e_month_widget_set_day_selected		(EMonthWidget *self,
							 guint day,
							 gboolean selected);
gboolean	e_month_widget_get_day_selected		(EMonthWidget *self,
							 guint day);
void		e_month_widget_set_day_tooltip_markup	(EMonthWidget *self,
							 guint day,
							 const gchar *tooltip_markup);
const gchar *	e_month_widget_get_day_tooltip_markup	(EMonthWidget *self,
							 guint day);
void		e_month_widget_clear_day_tooltips	(EMonthWidget *self);
void		e_month_widget_add_day_css_class	(EMonthWidget *self,
							 guint day,
							 const gchar *name);
void		e_month_widget_remove_day_css_class	(EMonthWidget *self,
							 guint day,
							 const gchar *name);
void		e_month_widget_clear_day_css_classes	(EMonthWidget *self);
guint		e_month_widget_get_day_at_position	(EMonthWidget *self,
							 gdouble x_win,
							 gdouble y_win);

G_END_DECLS

#endif /* E_MONTH_WIDGET_H */
