/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "calendar-view.h"

G_DEFINE_TYPE (
	GalViewCalendarDay,
	gal_view_calendar_day,
	GAL_TYPE_VIEW)

G_DEFINE_TYPE (
	GalViewCalendarWorkWeek,
	gal_view_calendar_work_week,
	GAL_TYPE_VIEW)

G_DEFINE_TYPE (
	GalViewCalendarWeek,
	gal_view_calendar_week,
	GAL_TYPE_VIEW)

G_DEFINE_TYPE (
	GalViewCalendarMonth,
	gal_view_calendar_month,
	GAL_TYPE_VIEW)

G_DEFINE_TYPE (
	GalViewCalendarYear,
	gal_view_calendar_year,
	GAL_TYPE_VIEW)

static void
gal_view_calendar_day_class_init (GalViewClass *class)
{
	class->type_code = "day-view";
}

static void
gal_view_calendar_day_init (GalView *view)
{
}

static void
gal_view_calendar_work_week_class_init (GalViewClass *class)
{
	class->type_code = "work_week_view";
}

static void
gal_view_calendar_work_week_init (GalView *view)
{
}

static void
gal_view_calendar_week_class_init (GalViewClass *class)
{
	class->type_code = "week_view";
}

static void
gal_view_calendar_week_init (GalView *view)
{
}

static void
gal_view_calendar_month_class_init (GalViewClass *class)
{
	class->type_code = "month_view";
}

static void
gal_view_calendar_month_init (GalView *view)
{
}

static void
gal_view_calendar_year_class_init (GalViewClass *class)
{
	class->type_code = "year_view";
}

static void
gal_view_calendar_year_init (GalView *view)
{
}
