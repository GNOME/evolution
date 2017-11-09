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

/**
 * EContentEditorInsertContentFlags:
 * @E_CONTENT_EDITOR_INSERT_NONE:
 * @E_CONTENT_EDITOR_INSERT_CONVERT:
 * @E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT:
 * @E_CONTENT_EDITOR_INSERT_REPLACE_ALL:
 * @E_CONTENT_EDITOR_INSERT_TEXT_HTML:
 * @E_CONTENT_EDITOR_INSERT_TEXT_PLAIN:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_INSERT_NONE		= 0,
	E_CONTENT_EDITOR_INSERT_CONVERT		= 1 << 0,
	E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT	= 1 << 1,
	E_CONTENT_EDITOR_INSERT_REPLACE_ALL	= 1 << 2,
	E_CONTENT_EDITOR_INSERT_TEXT_HTML	= 1 << 3,
	E_CONTENT_EDITOR_INSERT_TEXT_PLAIN	= 1 << 4,
} EContentEditorInsertContentFlags;

/**
 * EContentEditorGetContentFlags:
 * @E_CONTENT_EDITOR_GET_BODY:
 * @E_CONTENT_EDITOR_GET_INLINE_IMAGES:
 * @E_CONTENT_EDITOR_GET_PROCESSED: raw or processed
 * @E_CONTENT_EDITOR_GET_TEXT_HTML:
 * @E_CONTENT_EDITOR_GET_TEXT_PLAIN:
 * @E_CONTENT_EDITOR_GET_EXCLUDE_SIGNATURE:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_GET_BODY		= 1 << 0,
	E_CONTENT_EDITOR_GET_INLINE_IMAGES	= 1 << 1,
	E_CONTENT_EDITOR_GET_PROCESSED		= 1 << 2, /* raw or processed */
	E_CONTENT_EDITOR_GET_TEXT_HTML		= 1 << 3,
	E_CONTENT_EDITOR_GET_TEXT_PLAIN		= 1 << 4,
	E_CONTENT_EDITOR_GET_EXCLUDE_SIGNATURE	= 1 << 5
} EContentEditorGetContentFlags;

/**
 * EContentEditorNodeFlags:
 * @E_CONTENT_EDITOR_NODE_UNKNOWN: None from the below, aka when cannot determine.
 * @E_CONTENT_EDITOR_NODE_IS_ANCHOR:
 * @E_CONTENT_EDITOR_NODE_IS_H_RULE:
 * @E_CONTENT_EDITOR_NODE_IS_IMAGE:
 * @E_CONTENT_EDITOR_NODE_IS_TABLE:
 * @E_CONTENT_EDITOR_NODE_IS_TABLE_CELL:
 * @E_CONTENT_EDITOR_NODE_IS_TEXT:
 * @E_CONTENT_EDITOR_NODE_IS_TEXT_COLLAPSED:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_NODE_UNKNOWN		= 0,
	E_CONTENT_EDITOR_NODE_IS_ANCHOR		= 1 << 0,
	E_CONTENT_EDITOR_NODE_IS_H_RULE		= 1 << 1,
	E_CONTENT_EDITOR_NODE_IS_IMAGE		= 1 << 2,
	E_CONTENT_EDITOR_NODE_IS_TABLE		= 1 << 3,
	E_CONTENT_EDITOR_NODE_IS_TABLE_CELL	= 1 << 4,
	E_CONTENT_EDITOR_NODE_IS_TEXT		= 1 << 5,
	E_CONTENT_EDITOR_NODE_IS_TEXT_COLLAPSED	= 1 << 6
} EContentEditorNodeFlags;

/**
 * EContentEditorStyleFlags:
 * @E_CONTENT_EDITOR_STYLE_NONE: None from the below.
 * @E_CONTENT_EDITOR_STYLE_IS_BOLD:
 * @E_CONTENT_EDITOR_STYLE_IS_ITALIC:
 * @E_CONTENT_EDITOR_STYLE_IS_UNDERLINE:
 * @E_CONTENT_EDITOR_STYLE_IS_STRIKETHROUGH:
 * @E_CONTENT_EDITOR_STYLE_IS_MONOSPACE:
 * @E_CONTENT_EDITOR_STYLE_IS_SUBSCRIPT:
 * @E_CONTENT_EDITOR_STYLE_IS_SUPERSCRIPT:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_STYLE_NONE		= 0,
	E_CONTENT_EDITOR_STYLE_IS_BOLD		= 1 << 0,
	E_CONTENT_EDITOR_STYLE_IS_ITALIC	= 1 << 1,
	E_CONTENT_EDITOR_STYLE_IS_UNDERLINE	= 1 << 2,
	E_CONTENT_EDITOR_STYLE_IS_STRIKETHROUGH	= 1 << 3,
	E_CONTENT_EDITOR_STYLE_IS_MONOSPACE	= 1 << 4,
	E_CONTENT_EDITOR_STYLE_IS_SUBSCRIPT	= 1 << 5,
	E_CONTENT_EDITOR_STYLE_IS_SUPERSCRIPT	= 1 << 6
} EContentEditorStyleFlags;

/**
 * EContentEditorBlockFormat:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_NONE:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_PRE:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_H1:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_H2:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_H3:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_H4:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_H5:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_H6:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN:
 * @E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_BLOCK_FORMAT_NONE = 0,
	E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH,
	E_CONTENT_EDITOR_BLOCK_FORMAT_PRE,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ADDRESS,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H1,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H2,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H3,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H4,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H5,
	E_CONTENT_EDITOR_BLOCK_FORMAT_H6,
	E_CONTENT_EDITOR_BLOCK_FORMAT_UNORDERED_LIST,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ROMAN,
	E_CONTENT_EDITOR_BLOCK_FORMAT_ORDERED_LIST_ALPHA
} EContentEditorBlockFormat;

/**
 * EContentEditorFontSize:
 * @E_CONTENT_EDITOR_FONT_SIZE_TINY:
 * @E_CONTENT_EDITOR_FONT_SIZE_SMALL:
 * @E_CONTENT_EDITOR_FONT_SIZE_NORMAL:
 * @E_CONTENT_EDITOR_FONT_SIZE_BIG:
 * @E_CONTENT_EDITOR_FONT_SIZE_BIGGER:
 * @E_CONTENT_EDITOR_FONT_SIZE_LARGE:
 * @E_CONTENT_EDITOR_FONT_SIZE_VERY_LARGE:
 *
 * Note: The values match the actual size in &lt;font size="n"&gt;
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_FONT_SIZE_TINY		= 1,
	E_CONTENT_EDITOR_FONT_SIZE_SMALL	= 2,
	E_CONTENT_EDITOR_FONT_SIZE_NORMAL	= 3,
	E_CONTENT_EDITOR_FONT_SIZE_BIG		= 4,
	E_CONTENT_EDITOR_FONT_SIZE_BIGGER	= 5,
	E_CONTENT_EDITOR_FONT_SIZE_LARGE	= 6,
	E_CONTENT_EDITOR_FONT_SIZE_VERY_LARGE	= 7
} EContentEditorFontSize;

/**
 * EContentEditorAlignment:
 * @E_CONTENT_EDITOR_ALIGNMENT_LEFT:
 * @E_CONTENT_EDITOR_ALIGNMENT_CENTER:
 * @E_CONTENT_EDITOR_ALIGNMENT_RIGHT:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_ALIGNMENT_LEFT = 0,
	E_CONTENT_EDITOR_ALIGNMENT_CENTER,
	E_CONTENT_EDITOR_ALIGNMENT_RIGHT
} EContentEditorAlignment;

/**
 * EContentEditorGranularity:
 * @E_CONTENT_EDITOR_GRANULARITY_CHARACTER:
 * @E_CONTENT_EDITOR_GRANULARITY_WORD:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_GRANULARITY_CHARACTER = 0,
	E_CONTENT_EDITOR_GRANULARITY_WORD
} EContentEditorGranularity;

/**
 * EContentEditorCommand:
 * @E_CONTENT_EDITOR_COMMAND_BACKGROUND_COLOR:
 *   Sets background color to given value.
 * @E_CONTENT_EDITOR_COMMAND_BOLD:
 *   Toggles bold formatting of current selection.
 * @E_CONTENT_EDITOR_COMMAND_COPY:
 *   Copies current selection to clipboard.
 * @E_CONTENT_EDITOR_COMMAND_CREATE_LINK:
 *   Converts current selection to a link that points to URL in value
 * @E_CONTENT_EDITOR_COMMAND_CUT:
 *   Cuts current selection to clipboard.
 * @E_CONTENT_EDITOR_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR:
 *   (XXX Explain me!)
 * @E_CONTENT_EDITOR_COMMAND_DELETE:
 *   Deletes current selection.
 * @E_CONTENT_EDITOR_COMMAND_FIND_STRING:
 *   Highlights given string.
 * @E_CONTENT_EDITOR_COMMAND_FONT_NAME:
 *   Sets font name to given value.
 * @E_CONTENT_EDITOR_COMMAND_FONT_SIZE:
 *   Sets font point size to given value (no units, just number)
 * @E_CONTENT_EDITOR_COMMAND_FONT_SIZE_DELTA:
 *   Changes font size by given delta value (no units, just number)
 * @E_CONTENT_EDITOR_COMMAND_FORE_COLOR:
 *   Sets font color to given value
 * @E_CONTENT_EDITOR_COMMAND_FORMAT_BLOCK:
 *   Sets block type of current paragraph to given format. Allowed formats
 *   are "BLOCKQUOTE", "H1", "H2", "H3", "H4", "H5", "H6", "P", "PRE" and
 *   "ADDRESS".
 * @E_CONTENT_EDITOR_COMMAND_FORWARD_DELETE:
 *   (XXX Explain me!)
 * @E_CONTENT_EDITOR_COMMAND_HILITE_COLOR:
 *   Sets color in which results of "FindString" command should be
 *   highlighted to given value.
 * @E_CONTENT_EDITOR_COMMAND_INDENT:
 *   Indents current paragraph by one level.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_HTML:
 *   Inserts give HTML code into document.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_HORIZONTAL_RULE:
 *   Inserts a horizontal rule (&lt;HR&gt;) on current line.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_IMAGE:
 *   Inserts an image with given source file.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_LINE_BREAK:
 *   Breaks line at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT:
 *   Breaks citation at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_ORDERED_LIST:
 *   Creates an ordered list environment at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_PARAGRAPH:
 *   Inserts a new paragraph at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_TEXT:
 *   Inserts given text at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_INSERT_UNORDERED_LIST:
 *   Creates an undordered list environment at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_ITALIC:
 *   Toggles italic formatting of current selection.
 * @E_CONTENT_EDITOR_COMMAND_JUSTIFY_CENTER:
 *   Aligns current paragraph to center.
 * @E_CONTENT_EDITOR_COMMAND_JUSTIFY_FULL:
 *   Justifies current paragraph to block.
 * @E_CONTENT_EDITOR_COMMAND_JUSTIFY_NONE:
 *   Removes any justification or alignment of current paragraph.
 * @E_CONTENT_EDITOR_COMMAND_JUSTIFY_RIGHT:
 *   Aligns current paragraph to right.
 * @E_CONTENT_EDITOR_COMMAND_OUTDENT:
 *   Outdents current paragraph by one level.
 * @E_CONTENT_EDITOR_COMMAND_PASTE:
 *   Pastes clipboard content at current cursor position.
 * @E_CONTENT_EDITOR_COMMAND_PASTE_AND_MATCH_STYLE:
 *   Pastes clipboard content and matches its style to style at current
 *   cursor position.
 * @E_CONTENT_EDITOR_COMMAND_PASTE_AS_PLAIN_TEXT:
 *   Pastes clipboard content at current cursor position removing any HTML
 *   formatting.
 * @E_CONTENT_EDITOR_COMMAND_PRINT:
 *   Print current document.
 * @E_CONTENT_EDITOR_COMMAND_REDO:
 *   Redoes last action.
 * @E_CONTENT_EDITOR_COMMAND_REMOVE_FORMAT:
 *   Removes any formatting of current selection.
 * @E_CONTENT_EDITOR_COMMAND_SELECT_ALL:
 *   Extends selects to the entire document.
 * @E_CONTENT_EDITOR_COMMAND_STRIKETHROUGH:
 *   Toggles strikethrough formatting.
 * @E_CONTENT_EDITOR_COMMAND_STYLE_WITH_CSS:
 *   Toggles whether style should be defined in CSS "style" attribute of
 *   elements or whether to use deprecated &lt;FONT&gt; tags. Depends on
 *   whether given value is "true" or "false".
 * @E_CONTENT_EDITOR_COMMAND_SUBSCRIPT:
 *   Toggles subscript of current selection.
 * @E_CONTENT_EDITOR_COMMAND_SUPERSCRIPT:
 *   Toggles superscript of current selection.
 * @E_CONTENT_EDITOR_COMMAND_TRANSPOSE:
 *   (XXX Explain me!)
 * @E_CONTENT_EDITOR_COMMAND_UNDERLINE:
 *   Toggles underline formatting of current selection.
 * @E_CONTENT_EDITOR_COMMAND_UNDO:
 *   Undoes last action.
 * @E_CONTENT_EDITOR_COMMAND_UNLINK:
 *   Removes active links (&lt;A&gt;) from current selection (if there's any).
 * @E_CONTENT_EDITOR_COMMAND_UNSELECT:
 *   Cancels current selection.
 * @E_CONTENT_EDITOR_COMMAND_USE_CSS:
 *   Whether to allow use of CSS or not depending on whether given value is
 *   "true" or "false".
 *
 * Specifies the DOM command to execute in e_editor_widget_exec_command().
 * Some commands require value to be passed in, which is always stated in the
 * documentation.
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_COMMAND_BACKGROUND_COLOR,
	E_CONTENT_EDITOR_COMMAND_BOLD,
	E_CONTENT_EDITOR_COMMAND_COPY,
	E_CONTENT_EDITOR_COMMAND_CREATE_LINK,
	E_CONTENT_EDITOR_COMMAND_CUT,
	E_CONTENT_EDITOR_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR,
	E_CONTENT_EDITOR_COMMAND_DELETE,
	E_CONTENT_EDITOR_COMMAND_FIND_STRING,
	E_CONTENT_EDITOR_COMMAND_FONT_NAME,
	E_CONTENT_EDITOR_COMMAND_FONT_SIZE,
	E_CONTENT_EDITOR_COMMAND_FONT_SIZE_DELTA,
	E_CONTENT_EDITOR_COMMAND_FORE_COLOR,
	E_CONTENT_EDITOR_COMMAND_FORMAT_BLOCK,
	E_CONTENT_EDITOR_COMMAND_FORWARD_DELETE,
	E_CONTENT_EDITOR_COMMAND_HILITE_COLOR,
	E_CONTENT_EDITOR_COMMAND_INDENT,
	E_CONTENT_EDITOR_COMMAND_INSERT_HTML,
	E_CONTENT_EDITOR_COMMAND_INSERT_HORIZONTAL_RULE,
	E_CONTENT_EDITOR_COMMAND_INSERT_IMAGE,
	E_CONTENT_EDITOR_COMMAND_INSERT_LINE_BREAK,
	E_CONTENT_EDITOR_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT,
	E_CONTENT_EDITOR_COMMAND_INSERT_ORDERED_LIST,
	E_CONTENT_EDITOR_COMMAND_INSERT_PARAGRAPH,
	E_CONTENT_EDITOR_COMMAND_INSERT_TEXT,
	E_CONTENT_EDITOR_COMMAND_INSERT_UNORDERED_LIST,
	E_CONTENT_EDITOR_COMMAND_ITALIC,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_CENTER,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_FULL,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_LEFT,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_NONE,
	E_CONTENT_EDITOR_COMMAND_JUSTIFY_RIGHT,
	E_CONTENT_EDITOR_COMMAND_OUTDENT,
	E_CONTENT_EDITOR_COMMAND_PASTE,
	E_CONTENT_EDITOR_COMMAND_PASTE_AND_MATCH_STYLE,
	E_CONTENT_EDITOR_COMMAND_PASTE_AS_PLAIN_TEXT,
	E_CONTENT_EDITOR_COMMAND_PRINT,
	E_CONTENT_EDITOR_COMMAND_REDO,
	E_CONTENT_EDITOR_COMMAND_REMOVE_FORMAT,
	E_CONTENT_EDITOR_COMMAND_SELECT_ALL,
	E_CONTENT_EDITOR_COMMAND_STRIKETHROUGH,
	E_CONTENT_EDITOR_COMMAND_STYLE_WITH_CSS,
	E_CONTENT_EDITOR_COMMAND_SUBSCRIPT,
	E_CONTENT_EDITOR_COMMAND_SUPERSCRIPT,
	E_CONTENT_EDITOR_COMMAND_TRANSPOSE,
	E_CONTENT_EDITOR_COMMAND_UNDERLINE,
	E_CONTENT_EDITOR_COMMAND_UNDO,
	E_CONTENT_EDITOR_COMMAND_UNLINK,
	E_CONTENT_EDITOR_COMMAND_UNSELECT,
	E_CONTENT_EDITOR_COMMAND_USE_CSS
} EContentEditorCommand;

/**
 * EContentEditorScope:
 * @E_CONTENT_EDITOR_SCOPE_CELL:
 * @E_CONTENT_EDITOR_SCOPE_ROW:
 * @E_CONTENT_EDITOR_SCOPE_COLUMN:
 * @E_CONTENT_EDITOR_SCOPE_TABLE:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_SCOPE_CELL = 0,
	E_CONTENT_EDITOR_SCOPE_ROW,
	E_CONTENT_EDITOR_SCOPE_COLUMN,
	E_CONTENT_EDITOR_SCOPE_TABLE
} EContentEditorScope;

/**
 * EContentEditorUnit:
 * @E_CONTENT_EDITOR_UNIT_AUTO:
 * @E_CONTENT_EDITOR_UNIT_PIXEL:
 * @E_CONTENT_EDITOR_UNIT_PERCENTAGE:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_UNIT_AUTO = 0,
	E_CONTENT_EDITOR_UNIT_PIXEL,
	E_CONTENT_EDITOR_UNIT_PERCENTAGE
} EContentEditorUnit;

/**
 * EContentEditorCommand:
 * @E_CONTENT_EDITOR_FIND_NEXT: Search for the next occurrence of the text.
 *    This is the default. It's mutually exclusive with @E_CONTENT_EDITOR_FIND_PREVIOUS.
 * @E_CONTENT_EDITOR_FIND_PREVIOUS: Search for the previous occurrence of the text.
 *    It's mutually exclusive with @E_CONTENT_EDITOR_FIND_NEXT.
 * @E_CONTENT_EDITOR_FIND_MODE_BACKWARDS: The search mode is backwards. If not set,
 *    then the mode is forward.
 * @E_CONTENT_EDITOR_FIND_CASE_INSENSITIVE: Search case insensitively.
 * @E_CONTENT_EDITOR_FIND_WRAP_AROUND: Wrap around when searching.
 *
 * Flags to use to modify behaviour of the search for the text.
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_FIND_NEXT		= 1 << 0,
	E_CONTENT_EDITOR_FIND_PREVIOUS		= 1 << 1,
	E_CONTENT_EDITOR_FIND_MODE_BACKWARDS	= 1 << 2,
	E_CONTENT_EDITOR_FIND_CASE_INSENSITIVE	= 1 << 3,
	E_CONTENT_EDITOR_FIND_WRAP_AROUND	= 1 << 4
} EContentEditorFindFlags;

/**
 * EClipboardFlags:
 * @E_CLIPBOARD_CAN_COPY: It's possible to copy the currently selected content.
 *
 * Specifies clipboard's current state.
 *
 * Since: 3.24
 **/
typedef enum {
	E_CLIPBOARD_CAN_COPY	= 1 << 0
	/* E_CLIPBOARD_CAN_CUT	= 1 << 1,
	E_CLIPBOARD_CAN_PASTE	= 1 << 2 */
} EClipboardFlags;

/**
 * EDnDTargetType:
 * @DND_TARGET_TYPE_TEXT_URI_LIST: text/uri-list
 * @DND_TARGET_TYPE_MOZILLA_URL: _NETSCAPE_URL
 * @DND_TARGET_TYPE_TEXT_HTML: text/html
 * @DND_TARGET_TYPE_UTF8_STRING: UTF8_STRING
 * @DND_TARGET_TYPE_TEXT_PLAIN: text/plain
 * @DND_TARGET_TYPE_STRING: STRING
 * @DND_TARGET_TYPE_TEXT_PLAIN_UTF8: text/plain;charser=utf-8
 *
 * Drag and drop targets supported by EContentEditor.
 *
 * Since: 3.26
 **/
typedef enum {
	E_DND_TARGET_TYPE_TEXT_URI_LIST = 0,
	E_DND_TARGET_TYPE_MOZILLA_URL,
	E_DND_TARGET_TYPE_TEXT_HTML,
	E_DND_TARGET_TYPE_UTF8_STRING,
	E_DND_TARGET_TYPE_TEXT_PLAIN,
	E_DND_TARGET_TYPE_STRING,
	E_DND_TARGET_TYPE_TEXT_PLAIN_UTF8
} EDnDTargetType;

/**
 * EConfigLookupSourceKind:
 * @E_CONFIG_LOOKUP_SOURCE_UNKNOWN: unknown source kind
 * @E_CONFIG_LOOKUP_SOURCE_COLLECTION: collection source
 * @E_CONFIG_LOOKUP_SOURCE_MAIL_ACCOUNT: mail account source
 * @E_CONFIG_LOOKUP_SOURCE_MAIL_IDENTITY: mail identity source
 * @E_CONFIG_LOOKUP_SOURCE_MAIL_TRANSPORT: mail transport source
 *
 * Defines what source kind to get in call of e_config_lookup_get_source().
 *
 * Since: 3.26
 **/
typedef enum {
	E_CONFIG_LOOKUP_SOURCE_UNKNOWN,
	E_CONFIG_LOOKUP_SOURCE_COLLECTION,
	E_CONFIG_LOOKUP_SOURCE_MAIL_ACCOUNT,
	E_CONFIG_LOOKUP_SOURCE_MAIL_IDENTITY,
	E_CONFIG_LOOKUP_SOURCE_MAIL_TRANSPORT
} EConfigLookupSourceKind;

/**
 * EConfigLookupResultKind:
 * @E_CONFIG_LOOKUP_RESULT_UNKNOWN: unknown kind
 * @E_CONFIG_LOOKUP_RESULT_COLLECTION: collection kind, which can serve one or more of the other kinds
 * @E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE: configures mail receive
 * @E_CONFIG_LOOKUP_RESULT_MAIL_SEND: configures mail send
 * @E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK: configures address book
 * @E_CONFIG_LOOKUP_RESULT_CALENDAR: configures calendar
 * @E_CONFIG_LOOKUP_RESULT_MEMO_LIST: configures memo list
 * @E_CONFIG_LOOKUP_RESULT_TASK_LIST: configures task list
 *
 * Defines config lookup result kind, which is used to distinguish
 * which part the result configures.
 *
 * Since: 3.26
 **/
typedef enum {
	E_CONFIG_LOOKUP_RESULT_UNKNOWN,
	E_CONFIG_LOOKUP_RESULT_COLLECTION,
	E_CONFIG_LOOKUP_RESULT_MAIL_RECEIVE,
	E_CONFIG_LOOKUP_RESULT_MAIL_SEND,
	E_CONFIG_LOOKUP_RESULT_ADDRESS_BOOK,
	E_CONFIG_LOOKUP_RESULT_CALENDAR,
	E_CONFIG_LOOKUP_RESULT_MEMO_LIST,
	E_CONFIG_LOOKUP_RESULT_TASK_LIST
} EConfigLookupResultKind;

/**
 * @E_CONFIG_LOOKUP_RESULT_LAST_KIND:
 * The last known %EConfigLookupResultKind.
 *
 * Since: 3.28
 **/
#define E_CONFIG_LOOKUP_RESULT_LAST_KIND E_CONFIG_LOOKUP_RESULT_TASK_LIST

G_END_DECLS

#endif /* E_UTIL_ENUMS_H */
