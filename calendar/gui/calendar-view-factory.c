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

static void
gal_view_factory_calendar_day_class_init (GalViewFactoryClass *class)
{
	class->gal_view_type = GAL_TYPE_VIEW_CALENDAR_DAY;
}

static void
gal_view_factory_calendar_day_init (GalViewFactory *factory)
{
}

static void
gal_view_factory_calendar_work_week_class_init (GalViewFactoryClass *class)
{
	class->gal_view_type = GAL_TYPE_VIEW_CALENDAR_WORK_WEEK;
}

static void
gal_view_factory_calendar_work_week_init (GalViewFactory *factory)
{
}

static void
gal_view_factory_calendar_week_class_init (GalViewFactoryClass *class)
{
	class->gal_view_type = GAL_TYPE_VIEW_CALENDAR_WEEK;
}

static void
gal_view_factory_calendar_week_init (GalViewFactory *factory)
{
}

static void
gal_view_factory_calendar_month_class_init (GalViewFactoryClass *class)
{
	class->gal_view_type = GAL_TYPE_VIEW_CALENDAR_MONTH;
}

static void
gal_view_factory_calendar_month_init (GalViewFactory *factory)
{
}

