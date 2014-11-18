/*
 * e-html-editor-view-dom-functions.c
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

#include "e-html-editor-view-dom-functions.h"

/**
 * e_html_editor_view_dom_exec_command:
 * @document: a #WebKitDOMDocument
 * @command: an #EHTMLEditorViewCommand to execute
 * @value: value of the command (or @NULL if the command does not require value)
 *
 * The function will fail when @value is @NULL or empty but the current @command
 * requires a value to be passed. The @value is ignored when the @command does
 * not expect any value.
 *
 * Returns: @TRUE when the command was succesfully executed, @FALSE otherwise.
 */
gboolean
dom_exec_command (WebKitDOMDocument *document,
                  EHTMLEditorViewCommand command,
                  const gchar *value)
{
	const gchar *cmd_str = 0;
	gboolean has_value;

#define CHECK_COMMAND(cmd,str,val) case cmd:\
	if (val) {\
		g_return_val_if_fail (value && *value, FALSE);\
	}\
	has_value = val; \
	cmd_str = str;\
	break;

	switch (command) {
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_BACKGROUND_COLOR, "BackColor", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_BOLD, "Bold", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_COPY, "Copy", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_CREATE_LINK, "CreateLink", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_CUT, "Cut", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_DEFAULT_PARAGRAPH_SEPARATOR, "DefaultParagraphSeparator", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_DELETE, "Delete", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FIND_STRING, "FindString", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FONT_NAME, "FontName", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE, "FontSize", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FONT_SIZE_DELTA, "FontSizeDelta", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FORE_COLOR, "ForeColor", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FORMAT_BLOCK, "FormatBlock", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_FORWARD_DELETE, "ForwardDelete", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_HILITE_COLOR, "HiliteColor", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INDENT, "Indent", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_HORIZONTAL_RULE, "InsertHorizontalRule", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_HTML, "InsertHTML", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_IMAGE, "InsertImage", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_LINE_BREAK, "InsertLineBreak", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_NEW_LINE_IN_QUOTED_CONTENT, "InsertNewlineInQuotedContent", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_ORDERED_LIST, "InsertOrderedList", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_PARAGRAPH, "InsertParagraph", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_TEXT, "InsertText", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_INSERT_UNORDERED_LIST, "InsertUnorderedList", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_ITALIC, "Italic", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_CENTER, "JustifyCenter", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_FULL, "JustifyFull", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_LEFT, "JustifyLeft", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_NONE, "JustifyNone", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_JUSTIFY_RIGHT, "JustifyRight", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_OUTDENT, "Outdent", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_PASTE, "Paste", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_PASTE_AND_MATCH_STYLE, "PasteAndMatchStyle", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_PASTE_AS_PLAIN_TEXT, "PasteAsPlainText", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_PRINT, "Print", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_REDO, "Redo", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_REMOVE_FORMAT, "RemoveFormat", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_SELECT_ALL, "SelectAll", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_STRIKETHROUGH, "Strikethrough", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_STYLE_WITH_CSS, "StyleWithCSS", TRUE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_SUBSCRIPT, "Subscript", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_SUPERSCRIPT, "Superscript", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_TRANSPOSE, "Transpose", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_UNDERLINE, "Underline", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_UNDO, "Undo", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_UNLINK, "Unlink", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_UNSELECT, "Unselect", FALSE)
		CHECK_COMMAND (E_HTML_EDITOR_VIEW_COMMAND_USE_CSS, "UseCSS", TRUE)
	}

	return webkit_dom_document_exec_command (
		document, cmd_str, FALSE, has_value ? value : "" );
}
