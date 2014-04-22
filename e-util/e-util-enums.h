/*
 * e-util-enums.h
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
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UTIL_ENUMS_H
#define E_UTIL_ENUMS_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * EActivityState:
 * @E_ACTIVITY_RUNNING:
 *   The #EActivity is running.
 * @E_ACTIVITY_WAITING:
 *   The #EActivity is waiting to be run.
 * @E_ACTIVITY_CANCELLED:
 *   The #EActivity has been cancelled.
 * @E_ACTIVITY_COMPLETED:
 *   The #EActivity has completed.
 *
 * Various states of an #EActivity.
 **/
typedef enum {
	E_ACTIVITY_RUNNING,
	E_ACTIVITY_WAITING,
	E_ACTIVITY_CANCELLED,
	E_ACTIVITY_COMPLETED
} EActivityState;

/**
 * EAutomaticActionPolicy:
 * @E_AUTOMATIC_ACTION_POLICY_ASK:
 *   Ask the user whether to perform the action.
 * @E_AUTOMATIC_ACTION_POLICY_ALWAYS:
 *   Perform the action without interrupting the user.
 * @E_AUTOMATIC_ACTION_POLICY_NEVER:
 *   Do not perform the action and do not interrupt the user.
 *
 * Used for automatable actions based on the user's preference.  The user
 * is initially asked whether to perform the action automatically, and is
 * given either-or choices like "Yes, Always" or "No, Never".  The user's
 * response is then remembered for future sessions.
 **/
typedef enum {
	E_AUTOMATIC_ACTION_POLICY_ASK,
	E_AUTOMATIC_ACTION_POLICY_ALWAYS,
	E_AUTOMATIC_ACTION_POLICY_NEVER
} EAutomaticActionPolicy;

/**
 * EDateWeekday:
 * @E_DATE_BAD_WEEKDAY:
 *   Invalid value
 * @E_DATE_MONDAY:
 *   Monday
 * @E_DATE_TUESDAY:
 *   Tuesday
 * @E_DATE_WEDNESDAY:
 *   Wednesday
 * @E_DATE_THURSDAY:
 *   Thursday
 * @E_DATE_FRIDAY:
 *   Friday
 * @E_DATE_SATURDAY:
 *   Saturday
 * @E_DATE_SUNDAY:
 *   Sunday
 *
 * Enumeration representing a day of the week; @E_DATE_MONDAY,
 * @E_DATE_TUESDAY, etc.  @G_DATE_BAD_WEEKDAY is an invalid weekday.
 *
 * This enum type is intentionally compatible with #GDateWeekday.
 * It exists only because GLib does not provide a #GEnumClass for
 * #GDateWeekday.  If that ever changes, this enum can go away.
 **/
/* XXX Be pedantic with the value assignments to ensure compatibility. */
typedef enum {
	E_DATE_BAD_WEEKDAY = G_DATE_BAD_WEEKDAY,
	E_DATE_MONDAY = G_DATE_MONDAY,
	E_DATE_TUESDAY = G_DATE_TUESDAY,
	E_DATE_WEDNESDAY = G_DATE_WEDNESDAY,
	E_DATE_THURSDAY = G_DATE_THURSDAY,
	E_DATE_FRIDAY = G_DATE_FRIDAY,
	E_DATE_SATURDAY = G_DATE_SATURDAY,
	E_DATE_SUNDAY = G_DATE_SUNDAY
} EDateWeekday;

/**
 * EDurationType:
 * @E_DURATION_MINUTES:
 *   Duration value is in minutes.
 * @E_DURATION_HOURS:
 *   Duration value is in hours.
 * @E_DURATION_DAYS:
 *   Duration value is in days.
 *
 * Possible units for a duration or interval value.
 *
 * This enumeration is typically used where the numeric value and the
 * units of the value are shown or recorded separately.
 **/
typedef enum {
	E_DURATION_MINUTES,
	E_DURATION_HOURS,
	E_DURATION_DAYS
} EDurationType;

typedef enum {
	E_EDITOR_SELECTION_BLOCK_FORMAT_NONE = 0,
	E_EDITOR_SELECTION_BLOCK_FORMAT_H1,
	E_EDITOR_SELECTION_BLOCK_FORMAT_H2,
	E_EDITOR_SELECTION_BLOCK_FORMAT_H3,
	E_EDITOR_SELECTION_BLOCK_FORMAT_H4,
	E_EDITOR_SELECTION_BLOCK_FORMAT_H5,
	E_EDITOR_SELECTION_BLOCK_FORMAT_H6,
	E_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH,
	E_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE,
	E_EDITOR_SELECTION_BLOCK_FORMAT_PRE,
	E_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS,
	E_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST,
	E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST,
	E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN,
	E_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA,
} EEditorSelectionBlockFormat;

/* The values match the actual size in <font size="n"> */
typedef enum {
	E_EDITOR_SELECTION_FONT_SIZE_TINY	= 1,
	E_EDITOR_SELECTION_FONT_SIZE_SMALL	= 2,
	E_EDITOR_SELECTION_FONT_SIZE_NORMAL	= 3,
	E_EDITOR_SELECTION_FONT_SIZE_BIG	= 4,
	E_EDITOR_SELECTION_FONT_SIZE_BIGGER	= 5,
	E_EDITOR_SELECTION_FONT_SIZE_LARGE	= 6,
	E_EDITOR_SELECTION_FONT_SIZE_VERY_LARGE	= 7
} EEditorSelectionFontSize;

typedef enum {
	E_EDITOR_SELECTION_ALIGNMENT_LEFT,
	E_EDITOR_SELECTION_ALIGNMENT_CENTER,
	E_EDITOR_SELECTION_ALIGNMENT_RIGHT
} EEditorSelectionAlignment;

typedef enum {
	E_EDITOR_SELECTION_GRANULARITY_CHARACTER,
	E_EDITOR_SELECTION_GRANULARITY_WORD
} EEditorSelectionGranularity;

G_END_DECLS

#endif /* E_UTIL_ENUMS_H */
