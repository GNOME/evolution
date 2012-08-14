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

/* Spell Check Window */
#define E_EDITOR_WIDGETS_SPELL_WINDOW(editor) \
	E_EDITOR_WIDGETS ((editor), "spell-window")

#endif /* E_EDITOR_WIDGETS_H */
