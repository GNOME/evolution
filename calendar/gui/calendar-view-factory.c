/*
 * calendar-view-factory.c
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
 */

#include "calendar-view-factory.h"

#include <config.h>
#include <glib/gi18n.h>

#include "calendar-view.h"

G_DEFINE_TYPE (
	GalViewFactoryCalendarDay,
	gal_view_factory_calendar_day,
	GAL_TYPE_VIEW_FACTORY)

G_DEFINE_TYPE (
	GalViewFactoryCalendarWorkWeek,
	gal_view_factory_calendar_work_week,
	GAL_TYPE_VIEW_FACTORY)

G_DEFINE_TYPE (
	GalViewFactoryCalendarWeek,
	gal_view_factory_calendar_week,
	GAL_TYPE_VIEW_FACTORY)

G_DEFINE_TYPE (
	GalViewFactoryCalendarMonth,
	gal_view_factory_calendar_month,
	GAL_TYPE_VIEW_FACTORY)

static const gchar *
gal_view_factory_calendar_day_get_type_code (GalViewFactory *factory)
{
	return "day_view";
}

static GalView *
gal_view_factory_calendar_day_new_view (GalViewFactory *factory,
                                        const gchar *title)
{
	return g_object_new (
		GAL_TYPE_VIEW_CALENDAR_DAY,
		"title", title, NULL);
}

static void
gal_view_factory_calendar_day_class_init (GalViewFactoryClass *class)
{
	class->gal_view_type = GAL_TYPE_VIEW_CALENDAR_DAY;
	class->get_type_code = gal_view_factory_calendar_day_get_type_code;
	class->new_view = gal_view_factory_calendar_day_new_view;
}

static void
gal_view_factory_calendar_day_init (GalViewFactory *factory)
{
}

static const gchar *
gal_view_factory_calendar_work_week_get_type_code (GalViewFactory *factory)
{
	return "work_week_view";
}

static GalView *
gal_view_factory_calendar_work_week_new_view (GalViewFactory *factory,
                                              const gchar *title)
{
	return g_object_new (
		GAL_TYPE_VIEW_CALENDAR_WORK_WEEK,
		"title", title, NULL);
}

static void
gal_view_factory_calendar_work_week_class_init (GalViewFactoryClass *class)
{
	class->gal_view_type = GAL_TYPE_VIEW_CALENDAR_WORK_WEEK;
	class->get_type_code = gal_view_factory_calendar_work_week_get_type_code;
	class->new_view = gal_view_factory_calendar_work_week_new_view;
}

static void
gal_view_factory_calendar_work_week_init (GalViewFactory *factory)
{
}

static const gchar *
gal_view_factory_calendar_week_get_type_code (GalViewFactory *factory)
{
	return "week_view";
}

static GalView *
gal_view_factory_calendar_week_new_view (GalViewFactory *factory,
                                         const gchar *title)
{
	return g_object_new (
		GAL_TYPE_VIEW_CALENDAR_WEEK,
		"title", title, NULL);
}

static void
gal_view_factory_calendar_week_class_init (GalViewFactoryClass *class)
{
	class->gal_view_type = GAL_TYPE_VIEW_CALENDAR_WEEK;
	class->get_type_code = gal_view_factory_calendar_week_get_type_code;
	class->new_view = gal_view_factory_calendar_week_new_view;
}

static void
gal_view_factory_calendar_week_init (GalViewFactory *factory)
{
}

static const gchar *
gal_view_factory_calendar_month_get_type_code (GalViewFactory *factory)
{
	return "month_view";
}

static GalView *
gal_view_factory_calendar_month_new_view (GalViewFactory *factory,
                                          const gchar *title)
{
	return g_object_new (
		GAL_TYPE_VIEW_CALENDAR_MONTH,
		"title", title, NULL);
}

static void
gal_view_factory_calendar_month_class_init (GalViewFactoryClass *class)
{
	class->gal_view_type = GAL_TYPE_VIEW_CALENDAR_MONTH;
	class->get_type_code = gal_view_factory_calendar_month_get_type_code;
	class->new_view = gal_view_factory_calendar_month_new_view;
}

static void
gal_view_factory_calendar_month_init (GalViewFactory *factory)
{
}

