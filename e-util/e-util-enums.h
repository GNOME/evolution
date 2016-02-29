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

/* FIXME WK2 - the below cannot be enabled due to web-extensions/e-dom-utils.h using this header */
/*#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif*/

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
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_NONE = 0,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PRE,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ADDRESS,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_BLOCKQUOTE,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H1,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H2,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H3,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H4,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H5,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_H6,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_UNORDERED_LIST,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ROMAN,
	E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_ORDERED_LIST_ALPHA
} EHTMLEditorSelectionBlockFormat;

/* The values match the actual size in <font size="n"> */
typedef enum {
	E_HTML_EDITOR_SELECTION_FONT_SIZE_TINY		= 1,
	E_HTML_EDITOR_SELECTION_FONT_SIZE_SMALL		= 2,
	E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL	= 3,
	E_HTML_EDITOR_SELECTION_FONT_SIZE_BIG		= 4,
	E_HTML_EDITOR_SELECTION_FONT_SIZE_BIGGER	= 5,
	E_HTML_EDITOR_SELECTION_FONT_SIZE_LARGE		= 6,
	E_HTML_EDITOR_SELECTION_FONT_SIZE_VERY_LARGE	= 7
} EHTMLEditorSelectionFontSize;

typedef enum {
	E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT,
	E_HTML_EDITOR_SELECTION_ALIGNMENT_CENTER,
	E_HTML_EDITOR_SELECTION_ALIGNMENT_RIGHT
} EHTMLEditorSelectionAlignment;

typedef enum {
	E_HTML_EDITOR_SELECTION_GRANULARITY_CHARACTER,
	E_HTML_EDITOR_SELECTION_GRANULARITY_WORD
} EHTMLEditorSelectionGranularity;

/**
 * EHTMLEditorViewCommand:
 * @E_HTML_EDITOR_VIEW_COMMAND_BACKGROUND_COLOR:
 *   Sets background color to given value.
 * @E_HTML_EDITOR_VIEW_COMMAND_BOLD:
 *   Toggles bold formatting of current selection.
 * @E_HTML_EDITOR_VIEW_COMMAND_COPY:
 *   Copies current selection to clipboard.
 * @E_HTML_EDITOR_VIEW_COMMAND_CREATE_LINK:
 *   Converts current selection to a link that points to URL in value
 * @E_HTML_EDITOR_VIEW_COMMAND_CUT:
 *   Cuts current selection to clipboard.
 * @E_HTML_EDITOR_VIEW_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR:
 *   (XXX Explain me!)
 * @E_HTML_EDITOR_VIEW_COMMAND_DELETE:
 *   Deletes current selection.
 * @E_HTML_EDITOR_VIEW_COMMAND_FIND_STRING:
 *   Highlights given string.
 * @E_HTML_EDITOR_VIEW_COMMAND_FONT_NAME:
 *   Sets font name to given value.
 * @E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE:
 *   Sets font point size to given value (no units, just number)
 * @E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE_DELTA:
 *   Changes font size by given delta value (no units, just number)
 * @E_HTML_EDITOR_VIEW_COMMAND_FORE_COLOR:
 *   Sets font color to given value
 * @E_HTML_EDITOR_VIEW_COMMAND_FORMAT_BLOCK:
 *   Sets block type of current paragraph to given format. Allowed formats
 *   are "BLOCKQUOTE", "H1", "H2", "H3", "H4", "H5", "H6", "P", "PRE" and
 *   "ADDRESS".
 * @E_HTML_EDITOR_VIEW_COMMAND_FORWARD_DELETE:
 *   (XXX Explain me!)
 * @E_HTML_EDITOR_VIEW_COMMAND_HILITE_COLOR:
 *   Sets color in which results of "FindString" command should be
 *   highlighted to given value.
 * @E_HTML_EDITOR_VIEW_COMMAND_INDENT:
 *   Indents current paragraph by one level.
 * @E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML:
 *   Inserts give HTML code into document.
 * @E_HTML_EDITOR_VIEW_COMMAND_INSERT_HORIZONTAL_RULE:
 *   Inserts a horizontal rule (&lt;HR&gt;) on current line.
 * @E_HTML_EDITOR_VIEW_COMMAND_INSERT_IMAGE:
 *   Inserts an image with given source file.
 * @E_HTML_EDITOR_VIEW_COMMAND_INSERT_LINE_BREAK:
 *   Breaks line at current cursor position.
 * @E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT:
 *   Breaks citation at current cursor position.
 * @E_HTML_EDITOR_VIEW_COMMAND_INSERT_ORDERED_LIST:
 *   Creates an ordered list environment at current cursor position.
 * @E_HTML_EDITOR_VIEW_COMMAND_INSERT_PARAGRAPH:
 *   Inserts a new paragraph at current cursor position.
 * @E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT:
 *   Inserts given text at current cursor position.
 * @E_HTML_EDITOR_VIEW_COMMAND_INSERT_UNORDERED_LIST:
 *   Creates an undordered list environment at current cursor position.
 * @E_HTML_EDITOR_VIEW_COMMAND_ITALIC:
 *   Toggles italic formatting of current selection.
 * @E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_CENTER:
 *   Aligns current paragraph to center.
 * @E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_FULL:
 *   Justifies current paragraph to block.
 * @E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_NONE:
 *   Removes any justification or alignment of current paragraph.
 * @E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_RIGHT:
 *   Aligns current paragraph to right.
 * @E_HTML_EDITOR_VIEW_COMMAND_OUTDENT:
 *   Outdents current paragraph by one level.
 * @E_HTML_EDITOR_VIEW_COMMAND_PASTE:
 *   Pastes clipboard content at current cursor position.
 * @E_HTML_EDITOR_VIEW_COMMAND_PASTE_AND_MATCH_STYLE:
 *   Pastes clipboard content and matches its style to style at current
 *   cursor position.
 * @E_HTML_EDITOR_VIEW_COMMAND_PASTE_AS_PLAIN_TEXT:
 *   Pastes clipboard content at current cursor position removing any HTML
 *   formatting.
 * @E_HTML_EDITOR_VIEW_COMMAND_PRINT:
 *   Print current document.
 * @E_HTML_EDITOR_VIEW_COMMAND_REDO:
 *   Redoes last action.
 * @E_HTML_EDITOR_VIEW_COMMAND_REMOVE_FORMAT:
 *   Removes any formatting of current selection.
 * @E_HTML_EDITOR_VIEW_COMMAND_SELECT_ALL:
 *   Extends selects to the entire document.
 * @E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH:
 *   Toggles strikethrough formatting.
 * @E_HTML_EDITOR_VIEW_COMMAND_STYLE_WITH_CSS:
 *   Toggles whether style should be defined in CSS "style" attribute of
 *   elements or whether to use deprecated &lt;FONT&gt; tags. Depends on
 *   whether given value is "true" or "false".
 * @E_HTML_EDITOR_VIEW_COMMAND_SUBSCRIPT:
 *   Toggles subscript of current selection.
 * @E_HTML_EDITOR_VIEW_COMMAND_SUPERSCRIPT:
 *   Toggles superscript of current selection.
 * @E_HTML_EDITOR_VIEW_COMMAND_TRANSPOSE:
 *   (XXX Explain me!)
 * @E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE:
 *   Toggles underline formatting of current selection.
 * @E_HTML_EDITOR_VIEW_COMMAND_UNDO:
 *   Undoes last action.
 * @E_HTML_EDITOR_VIEW_COMMAND_UNLINK:
 *   Removes active links (&lt;A&gt;) from current selection (if there's any).
 * @E_HTML_EDITOR_VIEW_COMMAND_UNSELECT:
 *   Cancels current selection.
 * @E_HTML_EDITOR_VIEW_COMMAND_USE_CSS:
 *   Whether to allow use of CSS or not depending on whether given value is
 *   "true" or "false".
 *
 * Specifies the DOM command to execute in e_editor_widget_exec_command().
 * Some commands require value to be passed in, which is always stated in the
 * documentation.
 */
typedef enum {
	E_HTML_EDITOR_VIEW_COMMAND_BACKGROUND_COLOR,
	E_HTML_EDITOR_VIEW_COMMAND_BOLD,
	E_HTML_EDITOR_VIEW_COMMAND_COPY,
	E_HTML_EDITOR_VIEW_COMMAND_CREATE_LINK,
	E_HTML_EDITOR_VIEW_COMMAND_CUT,
	E_HTML_EDITOR_VIEW_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR,
	E_HTML_EDITOR_VIEW_COMMAND_DELETE,
	E_HTML_EDITOR_VIEW_COMMAND_FIND_STRING,
	E_HTML_EDITOR_VIEW_COMMAND_FONT_NAME,
	E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE,
	E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE_DELTA,
	E_HTML_EDITOR_VIEW_COMMAND_FORE_COLOR,
	E_HTML_EDITOR_VIEW_COMMAND_FORMAT_BLOCK,
	E_HTML_EDITOR_VIEW_COMMAND_FORWARD_DELETE,
	E_HTML_EDITOR_VIEW_COMMAND_HILITE_COLOR,
	E_HTML_EDITOR_VIEW_COMMAND_INDENT,
	E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML,
	E_HTML_EDITOR_VIEW_COMMAND_INSERT_HORIZONTAL_RULE,
	E_HTML_EDITOR_VIEW_COMMAND_INSERT_IMAGE,
	E_HTML_EDITOR_VIEW_COMMAND_INSERT_LINE_BREAK,
	E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT,
	E_HTML_EDITOR_VIEW_COMMAND_INSERT_ORDERED_LIST,
	E_HTML_EDITOR_VIEW_COMMAND_INSERT_PARAGRAPH,
	E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT,
	E_HTML_EDITOR_VIEW_COMMAND_INSERT_UNORDERED_LIST,
	E_HTML_EDITOR_VIEW_COMMAND_ITALIC,
	E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_CENTER,
	E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_FULL,
	E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_LEFT,
	E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_NONE,
	E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_RIGHT,
	E_HTML_EDITOR_VIEW_COMMAND_OUTDENT,
	E_HTML_EDITOR_VIEW_COMMAND_PASTE,
	E_HTML_EDITOR_VIEW_COMMAND_PASTE_AND_MATCH_STYLE,
	E_HTML_EDITOR_VIEW_COMMAND_PASTE_AS_PLAIN_TEXT,
	E_HTML_EDITOR_VIEW_COMMAND_PRINT,
	E_HTML_EDITOR_VIEW_COMMAND_REDO,
	E_HTML_EDITOR_VIEW_COMMAND_REMOVE_FORMAT,
	E_HTML_EDITOR_VIEW_COMMAND_SELECT_ALL,
	E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH,
	E_HTML_EDITOR_VIEW_COMMAND_STYLE_WITH_CSS,
	E_HTML_EDITOR_VIEW_COMMAND_SUBSCRIPT,
	E_HTML_EDITOR_VIEW_COMMAND_SUPERSCRIPT,
	E_HTML_EDITOR_VIEW_COMMAND_TRANSPOSE,
	E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE,
	E_HTML_EDITOR_VIEW_COMMAND_UNDO,
	E_HTML_EDITOR_VIEW_COMMAND_UNLINK,
	E_HTML_EDITOR_VIEW_COMMAND_UNSELECT,
	E_HTML_EDITOR_VIEW_COMMAND_USE_CSS
} EHTMLEditorViewCommand;

/**
 * EImageLoadingPolicy:
 * @E_IMAGE_LOADING_POLICY_NEVER:
 *   Never load images from a remote server.
 * @E_IMAGE_LOADING_POLICY_SOMETIMES:
 *   Only load images from a remote server if the sender is a known contact.
 * @E_IMAGE_LOADING_POLICY_ALWAYS:
 *   Always load images from a remote server.
 *
 * Policy for loading remote image URLs in email.  Allowing images to be
 * loaded from a remote server may have privacy implications.
 **/
typedef enum {
	E_IMAGE_LOADING_POLICY_NEVER,
	E_IMAGE_LOADING_POLICY_SOMETIMES,
	E_IMAGE_LOADING_POLICY_ALWAYS
} EImageLoadingPolicy;

G_END_DECLS

#endif /* E_UTIL_ENUMS_H */
