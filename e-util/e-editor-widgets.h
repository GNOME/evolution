/* e-editor-widgets.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_EDITOR_WIDGETS_H
#define E_EDITOR_WIDGETS_H

#define E_EDITOR_WIDGETS(editor, name) \
	(e_editor_get_widget ((editor), (name)))

/* Cell Properties Window */
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_CELL_RADIO_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-cell-radio-button")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_COLOR_COMBO(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-color-combo")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_COLUMN_RADIO_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-column-radio-button")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_COLUMN_SPAN_SPIN_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-column-span-spin-button")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_HEADER_STYLE_CHECK_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-header-style-check-button")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_HORIZONTAL_COMBO_BOX(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-horizontal-combo-box")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_IMAGE_FILE_CHOOSER(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-image-file-chooser")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_ROW_RADIO_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-row-radio-button")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_ROW_SPAN_SPIN_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-row-span-spin-button")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_TABLE_RADIO_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-table-radio-button")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_VERTICAL_COMBO_BOX(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-vertical-combo-box")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_WIDTH_CHECK_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-width-check-button")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_WIDTH_COMBO_BOX(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-width-combo-box")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_WIDTH_SPIN_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-width-spin-button")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_WINDOW(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-window")
#define E_EDITOR_WIDGETS_CELL_PROPERTIES_WRAP_TEXT_CHECK_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "cell-properties-wrap-text-check-button")

/* Image Properties Window */
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_ALIGNMENT_COMBO_BOX(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-alignment-combo-box")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_BORDER_SPIN_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-border-spin-button")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_DESCRIPTION_ENTRY(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-description-entry")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_HEIGHT_COMBO_BOX(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-height-combo-box")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_HEIGHT_SPIN_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-height-spin-button")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_SOURCE_FILE_CHOOSER(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-source-file-chooser")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_URL_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-url-button")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_URL_ENTRY(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-url-entry")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_WIDTH_COMBO_BOX(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-width-combo-box")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_WIDTH_SPIN_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-width-spin-button")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_WINDOW(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-window")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_X_PADDING_SPIN_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-x-padding-spin-button")
#define E_EDITOR_WIDGETS_IMAGE_PROPERTIES_Y_PADDING_SPIN_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "image-properties-y-padding-spin-button")

/* Page Properties Window */
#define E_EDITOR_WIDGETS_PAGE_PROPERTIES_BACKGROUND_COLOR_COMBO(editor) \
	E_EDITOR_WIDGETS ((editor), "page-properties-background-color-combo")
#define E_EDITOR_WIDGETS_PAGE_PROPERTIES_CUSTOM_FILE_CHOOSER(editor) \
	E_EDITOR_WIDGETS ((editor), "page-properties-custom-file-chooser")
#define E_EDITOR_WIDGETS_PAGE_PROPERTIES_LINK_COLOR_COMBO(editor) \
	E_EDITOR_WIDGETS ((editor), "page-properties-link-color-combo")
#define E_EDITOR_WIDGETS_PAGE_PROPERTIES_TEMPLATE_COMBO_BOX(editor) \
	E_EDITOR_WIDGETS ((editor), "page-properties-template-combo-box")
#define E_EDITOR_WIDGETS_PAGE_PROPERTIES_TEXT_COLOR_COMBO(editor) \
	E_EDITOR_WIDGETS ((editor), "page-properties-text-color-combo")
#define E_EDITOR_WIDGETS_PAGE_PROPERTIES_WINDOW(editor) \
	E_EDITOR_WIDGETS ((editor), "page-properties-window")

/* Paragraph Properties Window */
#define E_EDITOR_WIDGETS_PARAGRAPH_PROPERTIES_CENTER_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "paragraph-properties-center-button")
#define E_EDITOR_WIDGETS_PARAGRAPH_PROPERTIES_LEFT_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "paragraph-properties-left-button")
#define E_EDITOR_WIDGETS_PARAGRAPH_PROPERTIES_RIGHT_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "paragraph-properties-right-button")
#define E_EDITOR_WIDGETS_PARAGRAPH_PROPERTIES_STYLE_COMBO_BOX(editor) \
	E_EDITOR_WIDGETS ((editor), "paragraph-properties-style-combo-box")
#define E_EDITOR_WIDGETS_PARAGRAPH_PROPERTIES_WINDOW(editor) \
	E_EDITOR_WIDGETS ((editor), "paragraph-properties-window")

/* Spell Check Window */
#define E_EDITOR_WIDGETS_SPELL_WINDOW(editor) \
	E_EDITOR_WIDGETS ((editor), "spell-window")

/* Text Properties Window */
#define E_EDITOR_WIDGETS_TEXT_PROPERTIES_BOLD_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "text-properties-bold-button")
#define E_EDITOR_WIDGETS_TEXT_PROPERTIES_COLOR_COMBO(editor) \
	E_EDITOR_WIDGETS ((editor), "text-properties-color-combo")
#define E_EDITOR_WIDGETS_TEXT_PROPERTIES_ITALIC_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "text-properties-italic-button")
#define E_EDITOR_WIDGETS_TEXT_PROPERTIES_SIZE_COMBO_BOX(editor) \
	E_EDITOR_WIDGETS ((editor), "text-properties-size-combo-box")
#define E_EDITOR_WIDGETS_TEXT_PROPERTIES_STRIKETHROUGH_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "text-properties-strikethrough-button")
#define E_EDITOR_WIDGETS_TEXT_PROPERTIES_UNDERLINE_BUTTON(editor) \
	E_EDITOR_WIDGETS ((editor), "text-properties-underline-button")
#define E_EDITOR_WIDGETS_TEXT_PROPERTIES_WINDOW(editor) \
	E_EDITOR_WIDGETS ((editor), "text-properties-window")

#endif /* E_EDITOR_WIDGETS_H */
