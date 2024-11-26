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
 * @E_CONTENT_EDITOR_INSERT_CONVERT_PREFER_PRE: Set when should convert plain text into &lt;pre&gt; instead of &lt;div&gt;. Since 3.40
 * @E_CONTENT_EDITOR_INSERT_CLEANUP_SIGNATURE_ID: Set when should cleanup signature ID in the body. Since 3.42
 * @E_CONTENT_EDITOR_INSERT_FROM_PLAIN_TEXT: Set when the HTML source is a plain text editor. Since: 3.48
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_INSERT_NONE			= 0,
	E_CONTENT_EDITOR_INSERT_CONVERT			= 1 << 0,
	E_CONTENT_EDITOR_INSERT_QUOTE_CONTENT		= 1 << 1,
	E_CONTENT_EDITOR_INSERT_REPLACE_ALL		= 1 << 2,
	E_CONTENT_EDITOR_INSERT_TEXT_HTML		= 1 << 3,
	E_CONTENT_EDITOR_INSERT_TEXT_PLAIN		= 1 << 4,
	E_CONTENT_EDITOR_INSERT_CONVERT_PREFER_PRE	= 1 << 5,
	E_CONTENT_EDITOR_INSERT_CLEANUP_SIGNATURE_ID	= 1 << 6,
	E_CONTENT_EDITOR_INSERT_FROM_PLAIN_TEXT 	= 1 << 7
} EContentEditorInsertContentFlags;

/**
 * EContentEditorGetContentFlags:
 * @E_CONTENT_EDITOR_GET_INLINE_IMAGES: Return also list of inline images
 * @E_CONTENT_EDITOR_GET_RAW_BODY_HTML: text/html version of the body only, as used by the editor
 * @E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN: text/plain version of the body only, as used by the editor
 * @E_CONTENT_EDITOR_GET_RAW_BODY_STRIPPED: text/plain version of the body only, without signature, quoted text and such
 * @E_CONTENT_EDITOR_GET_RAW_DRAFT: a version of the content, to use as draft message
 * @E_CONTENT_EDITOR_GET_TO_SEND_HTML: text/html version of the content, suitable to be sent
 * @E_CONTENT_EDITOR_GET_TO_SEND_PLAIN:	text/plain version of the content, suitable to be sent
 * @E_CONTENT_EDITOR_GET_ALL: a shortcut for all flags
 *
 * Influences what content should be returned. Each flag means one
 * version, or part, of the content.
 *
 * Since: 3.38
 **/
typedef enum {
	E_CONTENT_EDITOR_GET_INLINE_IMAGES	= 1 << 0,
	E_CONTENT_EDITOR_GET_RAW_BODY_HTML	= 1 << 1,
	E_CONTENT_EDITOR_GET_RAW_BODY_PLAIN	= 1 << 2,
	E_CONTENT_EDITOR_GET_RAW_BODY_STRIPPED	= 1 << 3,
	E_CONTENT_EDITOR_GET_RAW_DRAFT		= 1 << 4,
	E_CONTENT_EDITOR_GET_TO_SEND_HTML	= 1 << 5,
	E_CONTENT_EDITOR_GET_TO_SEND_PLAIN	= 1 << 6,
	E_CONTENT_EDITOR_GET_ALL		= ~0
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
 * @E_CONTENT_EDITOR_ALIGNMENT_NONE:
 * @E_CONTENT_EDITOR_ALIGNMENT_LEFT:
 * @E_CONTENT_EDITOR_ALIGNMENT_CENTER:
 * @E_CONTENT_EDITOR_ALIGNMENT_RIGHT:
 * @E_CONTENT_EDITOR_ALIGNMENT_JUSTIFY:
 *
 * Since: 3.22
 **/
typedef enum {
	E_CONTENT_EDITOR_ALIGNMENT_NONE = -1,
	E_CONTENT_EDITOR_ALIGNMENT_LEFT = 0,
	E_CONTENT_EDITOR_ALIGNMENT_CENTER,
	E_CONTENT_EDITOR_ALIGNMENT_RIGHT,
	E_CONTENT_EDITOR_ALIGNMENT_JUSTIFY
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
 * EContentEditorMode:
 * @E_CONTENT_EDITOR_MODE_UNKNOWN: unknown mode
 * @E_CONTENT_EDITOR_MODE_PLAIN_TEXT: plain text, expects export as text/plain
 * @E_CONTENT_EDITOR_MODE_HTML: HTML, expects export as text/html
 * @E_CONTENT_EDITOR_MODE_MARKDOWN: markdown, expects export as text/markdown
 * @E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT: markdown, expects export as text/plain
 * @E_CONTENT_EDITOR_MODE_MARKDOWN_HTML: markdown, expects export as text/html
 *
 * Editing mode of a content editor.
 *
 * Since: 3.44
 **/
typedef enum {
	E_CONTENT_EDITOR_MODE_UNKNOWN = -1,
	E_CONTENT_EDITOR_MODE_PLAIN_TEXT,
	E_CONTENT_EDITOR_MODE_HTML,
	E_CONTENT_EDITOR_MODE_MARKDOWN,
	E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT,
	E_CONTENT_EDITOR_MODE_MARKDOWN_HTML
} EContentEditorMode;

/**
 * EUndoRedoState:
 * @E_UNDO_REDO_STATE_NONE: Cannot undo, neither redo.
 * @E_UNDO_REDO_STATE_CAN_UNDO: Undo is available.
 * @E_UNDO_REDO_STATE_CAN_REDO: Redo is available.
 *
 * Flags in what state Undo/Redo stack is.
 *
 * Since: 3.38
 **/
typedef enum {
	E_UNDO_REDO_STATE_NONE		= 0,
	E_UNDO_REDO_STATE_CAN_UNDO	= 1 << 0,
	E_UNDO_REDO_STATE_CAN_REDO	= 1 << 1
} EUndoRedoState;

/**
 * EDnDTargetType:
 * @DND_TARGET_TYPE_TEXT_URI_LIST: text/uri-list
 * @DND_TARGET_TYPE_MOZILLA_URL: _NETSCAPE_URL
 * @DND_TARGET_TYPE_TEXT_HTML: text/html
 * @DND_TARGET_TYPE_UTF8_STRING: UTF8_STRING
 * @DND_TARGET_TYPE_TEXT_PLAIN: text/plain
 * @DND_TARGET_TYPE_STRING: STRING
 * @DND_TARGET_TYPE_TEXT_PLAIN_UTF8: text/plain;charser=utf-8
 * @E_DND_TARGET_TYPE_TEXT_X_MOZ_URL: text/x-moz-url ; Since:3.52
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
	E_DND_TARGET_TYPE_TEXT_PLAIN_UTF8,
	E_DND_TARGET_TYPE_TEXT_X_MOZ_URL
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

/**
 * EMarkdownHTMLToTextFlags:
 * @E_MARKDOWN_HTML_TO_TEXT_FLAG_NONE: no flag set
 * @E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT: disallow any HTML, save in pure plain text
 * @E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS: enable composer quirks to post-process the text
 * @E_MARKDOWN_HTML_TO_TEXT_FLAG_SIGNIFICANT_NL: whether new lines in the text are significant,
 *    aka whether they work the same as the &lt;br&gt; elements. Since: 3.48
 * @E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_INLINE: this flag is used only together with %E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT,
 *    and it converts links in a way that it shows href beside the text link, like: "label &lt;href&gt;". Since: 3.52
 * @E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE: this flag is used only together with %E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT,
 *    and it converts links in a way that it shows href as reference to the end of the text, like: "label [1] ...... [1] label href". Since: 3.52
 * @E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE_WITHOUT_LABEL: this flag is used only together with %E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT,
 *    and it converts links in a way that it shows href as reference to the end of the text without label, like: "label [1] ...... [1] href". Since: 3.52
 *
 * Flags used in e_markdown_util_html_to_text(). The %E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_INLINE,
 * %E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE and %E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE_WITHOUT_LABEL
 * are mutually exclusive and are used only together with the %E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT flag.
 *
 * Since: 3.44
 **/
typedef enum { /*< flags >*/
	E_MARKDOWN_HTML_TO_TEXT_FLAG_NONE				= 0,
	E_MARKDOWN_HTML_TO_TEXT_FLAG_PLAIN_TEXT				= 1 << 0,
	E_MARKDOWN_HTML_TO_TEXT_FLAG_COMPOSER_QUIRKS			= 1 << 1,
	E_MARKDOWN_HTML_TO_TEXT_FLAG_SIGNIFICANT_NL			= 1 << 2,
	E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_INLINE			= 1 << 3,
	E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE			= 1 << 4,
	E_MARKDOWN_HTML_TO_TEXT_FLAG_LINK_REFERENCE_WITHOUT_LABEL	= 1 << 5
} EMarkdownHTMLToTextFlags;

/**
 * EMarkdownTextToHTMLFlags:
 * @E_MARKDOWN_TEXT_TO_HTML_FLAG_NONE: no flag set
 * @E_MARKDOWN_TEXT_TO_HTML_FLAG_INCLUDE_SOURCEPOS: include source position in the generated HTML
 *
 * Flags used in e_markdown_util_text_to_html_full().
 *
 * Since: 3.48
 **/
typedef enum { /*< flags >*/
	E_MARKDOWN_TEXT_TO_HTML_FLAG_NONE		= 0,
	E_MARKDOWN_TEXT_TO_HTML_FLAG_INCLUDE_SOURCEPOS	= 1 << 0
} EMarkdownTextToHTMLFlags;

typedef enum {
	E_TOOLBAR_ICON_SIZE_DEFAULT	= 0,
	E_TOOLBAR_ICON_SIZE_SMALL	= 1,
	E_TOOLBAR_ICON_SIZE_LARGE	= 2
} EToolbarIconSize;

typedef enum {
	E_PREFER_SYMBOLIC_ICONS_NO	= 0,
	E_PREFER_SYMBOLIC_ICONS_YES	= 1,
	E_PREFER_SYMBOLIC_ICONS_AUTO	= 2
} EPreferSymbolicIcons;

/**
 * EHTMLLinkToText:
 * @E_HTML_LINK_TO_TEXT_NONE: do not store href in the plain text
 * @E_HTML_LINK_TO_TEXT_INLINE: show href beside the text link, like: "label &lt;href&gt;"
 * @E_HTML_LINK_TO_TEXT_REFERENCE: show href as reference to the end of the text, like: "label [1] ...... [1] label href"
 * @E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL: show href as reference to the end of the text without label, like: "label [1] ...... [1] href"
 *
 * How to convert links from HTML to plain text.
 *
 * Since: 3.52
 **/
typedef enum {
	E_HTML_LINK_TO_TEXT_NONE = 0,
	E_HTML_LINK_TO_TEXT_INLINE = 1,
	E_HTML_LINK_TO_TEXT_REFERENCE = 2,
	E_HTML_LINK_TO_TEXT_REFERENCE_WITHOUT_LABEL = 3
} EHTMLLinkToText;

/**
 * EUIElementKind:
 * @E_UI_ELEMENT_KIND_UNKNOWN: the kind is not known; used to indicate an error
 * @E_UI_ELEMENT_KIND_ROOT: a root, aka toplevel, element; it contains elements for headerbar, toolbar and menu usually
 * @E_UI_ELEMENT_KIND_HEADERBAR: a headerbar element
 * @E_UI_ELEMENT_KIND_TOOLBAR: a toolbar element
 * @E_UI_ELEMENT_KIND_MENU: a menu element
 * @E_UI_ELEMENT_KIND_SUBMENU: a submenu of a menu element
 * @E_UI_ELEMENT_KIND_PLACEHOLDER: a placeholder, into which can be added other elements
 * @E_UI_ELEMENT_KIND_SEPARATOR: a separator element
 * @E_UI_ELEMENT_KIND_START: a list of items to be packed at the start of a headerbar
 * @E_UI_ELEMENT_KIND_END: a list of items to be packed at the end of a headerbar
 * @E_UI_ELEMENT_KIND_ITEM: an item
 *
 * The UI element kinds. Depending on the kind, only respective functions can be
 * called for the element.
 *
 * Since: 3.56
 **/
typedef enum _EUIElementKind {
	E_UI_ELEMENT_KIND_UNKNOWN	= 0,
	E_UI_ELEMENT_KIND_ROOT		= 1 << 0,
	E_UI_ELEMENT_KIND_HEADERBAR	= 1 << 1,
	E_UI_ELEMENT_KIND_TOOLBAR	= 1 << 2,
	E_UI_ELEMENT_KIND_MENU		= 1 << 3,
	E_UI_ELEMENT_KIND_SUBMENU	= 1 << 4,
	E_UI_ELEMENT_KIND_PLACEHOLDER	= 1 << 5,
	E_UI_ELEMENT_KIND_SEPARATOR	= 1 << 6,
	E_UI_ELEMENT_KIND_START		= 1 << 7,
	E_UI_ELEMENT_KIND_END		= 1 << 8,
	E_UI_ELEMENT_KIND_ITEM		= 1 << 9
} EUIElementKind;

/**
 * EUIParserExportFlags:
 * @E_UI_PARSER_EXPORT_FLAG_NONE: no flag set
 * @E_UI_PARSER_EXPORT_FLAG_INDENT: indent the output; when not set a single line of text is exported
 *
 * Set of flags to influence output of the e_ui_parser_export().
 *
 * Since: 3.56
 **/
typedef enum _EUIParserExportFlags {  /*< flags >*/
	E_UI_PARSER_EXPORT_FLAG_NONE	= 0,
	E_UI_PARSER_EXPORT_FLAG_INDENT	= 1 << 0
} EUIParserExportFlags;

G_END_DECLS

#endif /* E_UTIL_ENUMS_H */
