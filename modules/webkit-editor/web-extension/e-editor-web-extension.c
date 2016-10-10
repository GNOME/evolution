/*
 * e-editor-web-extension.c
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

#include "evolution-config.h"

#include <string.h>

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit2/webkit-web-extension.h>
#include <camel/camel.h>

#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDocumentUnstable.h>
#include <webkitdom/WebKitDOMDOMSelection.h>
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>
#undef WEBKIT_DOM_USE_UNSTABLE_API

#include "web-extensions/e-dom-utils.h"

#include "e-editor-page.h"
#include "e-composer-dom-functions.h"
#include "e-dialogs-dom-functions.h"
#include "e-editor-dom-functions.h"
#include "e-editor-undo-redo-manager.h"

#include "e-editor-web-extension.h"

#define E_EDITOR_WEB_EXTENSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_EDITOR_WEB_EXTENSION, EEditorWebExtensionPrivate))

struct _EEditorWebExtensionPrivate {
	WebKitWebExtension *wk_extension;

	GDBusConnection *dbus_connection;
	guint registration_id;

	GHashTable *editor_pages; /* guint64 *webpage_id ~> EEditorPage * */
};

static CamelDataCache *emd_global_http_cache = NULL;

static const gchar *introspection_xml =
"<node>"
"  <interface name='" E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE "'>"
"<!-- ********************************************************* -->"
"<!--                          SIGNALS                          -->"
"<!-- ********************************************************* -->"
"    <signal name='SelectionChanged'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='i' name='alignment' direction='out'/>"
"      <arg type='i' name='block_format' direction='out'/>"
"      <arg type='b' name='indented' direction='out'/>"
"      <arg type='i' name='style_flags' direction='out'/>"
"      <arg type='i' name='font_size' direction='out'/>"
"      <arg type='s' name='font_color' direction='out'/>"
"    </signal>"
"    <signal name='ContentChanged'>"
"      <arg type='t' name='page_id' direction='out'/>"
"    </signal>"
"    <signal name='UndoRedoStateChanged'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='b' name='can_undo' direction='out'/>"
"      <arg type='b' name='can_redo' direction='out'/>"
"    </signal>"
"    <signal name='UserChangedDefaultColors'>"
"      <arg type='b' name='suppress_color_changes' direction='out'/>"
"    </signal>"
"<!-- ********************************************************* -->"
"<!--                          METHODS                          -->"
"<!-- ********************************************************* -->"
"<!-- ********************************************************* -->"
"<!--                       FOR TESTING ONLY                    -->"
"<!-- ********************************************************* -->"
"    <method name='TestHTMLEqual'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='html1' direction='in'/>"
"      <arg type='s' name='html2' direction='in'/>"
"      <arg type='b' name='equal' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--                          GENERIC                          -->"
"<!-- ********************************************************* -->"
"    <method name='ElementHasAttribute'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"      <arg type='b' name='has_attribute' direction='out'/>"
"    </method>"
"    <method name='ElementGetAttribute'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='ElementGetAttributeBySelector'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='selector' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='ElementRemoveAttribute'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"    </method>"
"    <method name='ElementRemoveAttributeBySelector'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='selector' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"    </method>"
"    <method name='ElementSetAttribute'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='ElementSetAttributeBySelector'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='selector' direction='in'/>"
"      <arg type='s' name='attribute' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='ElementGetTagName'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='s' name='tag_name' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are specific to composer               -->"
"<!-- ********************************************************* -->"
"    <method name='RemoveImageAttributesFromElementBySelector'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='selector' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EEditorCellDialog      -->"
"<!-- ********************************************************* -->"
"    <method name='EEditorCellDialogMarkCurrentCellElement'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='EEditorCellDialogSaveHistoryOnExit'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorCellDialogSetElementVAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EEditorCellDialogSetElementAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EEditorCellDialogSetElementNoWrap'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EEditorCellDialogSetElementHeaderStyle'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EEditorCellDialogSetElementWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EEditorCellDialogSetElementColSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EEditorCellDialogSetElementRowSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EEditorCellDialogSetElementBgColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EEditorHRuleDialog      -->"
"<!-- ********************************************************* -->"
"    <method name='EEditorHRuleDialogFindHRule'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='created_new_hr' direction='out'/>"
"    </method>"
"    <method name='EEditorHRuleDialogOnClose'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EEditorImageDialog     -->"
"<!-- ********************************************************* -->"
"    <method name='EEditorImageDialogMarkImage'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorImageDialogSaveHistoryOnExit'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorImageDialogSetElementUrl'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='EEditorImageDialogGetElementUrl'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetHeight'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetHeight'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementGetNaturalWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementGetNaturalHeight'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetHSpace'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetHSpace'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"    <method name='ImageElementSetVSpace'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"    </method>"
"    <method name='ImageElementGetVSpace'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='value' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EEditorLinkDialog      -->"
"<!-- ********************************************************* -->"
"    <method name='EEditorLinkDialogOk'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='url' direction='in'/>"
"      <arg type='s' name='inner_text' direction='in'/>"
"    </method>"
"    <method name='EEditorLinkDialogShow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='url' direction='out'/>"
"      <arg type='s' name='inner_text' direction='out'/>"
"    </method>"
"    <method name='EEditorLinkDialogOnOpen'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorLinkDialogOnClose'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorLinkDialogUnlink'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EEditorPageDialog     -->"
"<!-- ********************************************************* -->"
"    <method name='EEditorPageDialogSaveHistory'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorPageDialogSaveHistoryOnExit'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--   Functions that are used in EEditorSpellCheckDialog  -->"
"<!-- ********************************************************* -->"
"    <method name='EEditorSpellCheckDialogNext'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='word' direction='in'/>"
"      <arg type='as' name='languages' direction='in'/>"
"      <arg type='s' name='next_word' direction='out'/>"
"    </method>"
"    <method name='EEditorSpellCheckDialogPrev'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='word' direction='in'/>"
"      <arg type='as' name='languages' direction='in'/>"
"      <arg type='s' name='prev_word' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EEditorTableDialog     -->"
"<!-- ********************************************************* -->"
"    <method name='EEditorTableDialogSetRowCount'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='value' direction='in'/>"
"    </method>"
"    <method name='EEditorTableDialogGetRowCount'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='value' direction='out'/>"
"    </method>"
"    <method name='EEditorTableDialogSetColumnCount'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='value' direction='in'/>"
"    </method>"
"    <method name='EEditorTableDialogGetColumnCount'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='value' direction='out'/>"
"    </method>"
"    <method name='EEditorTableDialogShow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='created_new_table' direction='out'/>"
"    </method>"
"    <method name='EEditorTableDialogSaveHistoryOnExit'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EEditorActions         -->"
"<!-- ********************************************************* -->"
"    <method name='TableCellElementGetNoWrap'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='no_wrap' direction='out'/>"
"    </method>"
"    <method name='TableCellElementGetRowSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='row_span' direction='out'/>"
"    </method>"
"    <method name='TableCellElementGetColSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='i' name='col_span' direction='out'/>"
"    </method>"
"    <method name='EEditorDialogDeleteCellContents'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorDialogDeleteColumn'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorDialogDeleteRow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorDialogDeleteTable'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorDialogInsertColumnAfter'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorDialogInsertColumnBefore'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorDialogInsertRowAbove'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorDialogInsertRowBelow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EEditorActionsSaveHistoryForCut'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EEditorView            -->"
"<!-- ********************************************************* -->"
"    <method name='SetPastingContentFromItself'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"    </method>"
"    <method name='SetEditorHTMLMode'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='html_mode' direction='in'/>"
"      <arg type='b' name='convert' direction='in'/>"
"    </method>"
"    <method name='SetConvertInSitu'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"    </method>"
"    <method name='DOMForceSpellCheck'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMTurnSpellCheckOff'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMScrollToCaret'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMEmbedStyleSheet'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='style_sheet_content' direction='in'/>"
"    </method>"
"    <method name='DOMRemoveEmbeddedStyleSheet'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMSaveSelection'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMRestoreSelection'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMUndo'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMRedo'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMQuoteAndInsertTextIntoSelection'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='text' direction='in'/>"
"      <arg type='b' name='is_html' direction='in'/>"
"    </method>"
"    <method name='DOMConvertAndInsertHTMLIntoSelection'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='text' direction='in'/>"
"      <arg type='b' name='is_html' direction='in'/>"
"    </method>"
"    <method name='DOMCheckIfConversionNeeded'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='conversion_needed' direction='out'/>"
"    </method>"
"    <method name='DOMGetContent'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='from_domain' direction='in'/>"
"      <arg type='i' name='flags' direction='in'/>"
"      <arg type='s' name='content' direction='out'/>"
"      <arg type='v' name='inline_images' direction='out'/>"
"    </method>"
"    <method name='DOMInsertHTML'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='html' direction='in'/>"
"    </method>"
"    <method name='DOMConvertContent'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='preffered_text' direction='in'/>"
"    </method>"
"    <method name='DOMAddNewInlineImageIntoList'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='filename' direction='in'/>"
"      <arg type='s' name='cid_src' direction='in'/>"
"      <arg type='s' name='src' direction='in'/>"
"    </method>"
"    <method name='DOMReplaceImageSrc'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='selector' direction='in'/>"
"      <arg type='s' name='uri' direction='in'/>"
"    </method>"
"    <method name='DOMDragAndDropEnd'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMMoveSelectionOnPoint'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='x' direction='in'/>"
"      <arg type='i' name='y' direction='in'/>"
"      <arg type='b' name='cancel_if_not_collapsed' direction='in'/>"
"    </method>"
"    <method name='DOMInsertSmiley'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='smiley_name' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EEditorSelection       -->"
"<!-- ********************************************************* -->"
"    <method name='DOMSelectionIndent'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionInsertImage'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='uri' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionReplace'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='replacement' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetAlignment'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='alignment' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetBold'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='bold' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetBlockFormat'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='block_format' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetFontColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='color' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetFontSize'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='font_size' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetItalic'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='italic' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetMonospaced'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='monospaced' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetStrikethrough'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='strikethrough' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetSubscript'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='subscript' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetSuperscript'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='superscript' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSetUnderline'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='underline' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionUnindent'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionWrap'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMGetCaretWord'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='word' direction='out'/>"
"    </method>"
"    <method name='DOMReplaceCaretWord'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='replacement' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EComposerPrivate           -->"
"<!-- ********************************************************* -->"
"    <method name='DOMInsertSignature'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='content' direction='in'/>"
"      <arg type='b' name='is_html' direction='in'/>"
"      <arg type='s' name='signature_id' direction='in'/>"
"      <arg type='b' name='set_signature_from_message' direction='in'/>"
"      <arg type='b' name='check_if_signature_is_changed' direction='in'/>"
"      <arg type='b' name='ignore_next_signature_change' direction='in'/>"
"      <arg type='s' name='new_signature_id' direction='out'/>"
"      <arg type='b' name='out_set_signature_from_message' direction='out'/>"
"      <arg type='b' name='out_check_if_signature_is_changed' direction='out'/>"
"      <arg type='b' name='out_ignore_next_signature_change' direction='out'/>"
"    </method>"
"    <method name='DOMSaveDragAndDropHistory'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMCleanAfterDragAndDrop'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMGetActiveSignatureUid'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='uid' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in External Editor plugin     -->"
"<!-- ********************************************************* -->"
"    <method name='DOMGetCaretPosition'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='position' direction='out'/>"
"    </method>"
"    <method name='DOMGetCaretOffset'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='offset' direction='out'/>"
"    </method>"
"    <method name='DOMClearUndoRedoHistory'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"  </interface>"
"</node>";

G_DEFINE_TYPE (EEditorWebExtension, e_editor_web_extension, G_TYPE_OBJECT)

static EEditorPage *
get_editor_page (EEditorWebExtension *extension,
                 guint64 page_id)
{
	g_return_val_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension), NULL);

	return g_hash_table_lookup (extension->priv->editor_pages, &page_id);
}

static EEditorPage *
get_editor_page_or_return_dbus_error (GDBusMethodInvocation *invocation,
                                      EEditorWebExtension *extension,
                                      guint64 page_id)
{
	WebKitWebPage *web_page;
	EEditorPage *editor_page;

	g_return_val_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension), NULL);

	web_page = webkit_web_extension_get_page (extension->priv->wk_extension, page_id);
	if (!web_page) {
		g_dbus_method_invocation_return_error (
			invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"Invalid page ID: %" G_GUINT64_FORMAT, page_id);

		return NULL;
	}

	editor_page = get_editor_page (extension, page_id);
	if (!editor_page) {
		g_dbus_method_invocation_return_error (
			invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"Invalid page ID: %" G_GUINT64_FORMAT, page_id);
	}

	return editor_page;
}

static void
handle_method_call (GDBusConnection *connection,
                    const char *sender,
                    const char *object_path,
                    const char *interface_name,
                    const char *method_name,
                    GVariant *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer user_data)
{
	guint64 page_id;
        EEditorWebExtension *extension = E_EDITOR_WEB_EXTENSION (user_data);
	WebKitDOMDocument *document;
	EEditorPage *editor_page;

	if (g_strcmp0 (interface_name, E_WEBKIT_EDITOR_WEB_EXTENSION_INTERFACE) != 0)
		return;

	if (g_strcmp0 (method_name, "TestHTMLEqual") == 0) {
		gboolean equal = FALSE;
		const gchar *html1 = NULL, *html2 = NULL;

		g_variant_get (parameters, "(t&s&s)", &page_id, &html1, &html2);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		equal = e_editor_dom_test_html_equal (document, html1, html2);

		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", equal));
	} else if (g_strcmp0 (method_name, "ElementHasAttribute") == 0) {
		gboolean value = FALSE;
		const gchar *element_id, *attribute;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &attribute);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_element_has_attribute (element, attribute);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", value));
	} else if (g_strcmp0 (method_name, "ElementGetAttribute") == 0) {
		const gchar *element_id, *attribute;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &attribute);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_element_get_attribute (element, attribute);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "ElementGetAttributeBySelector") == 0) {
		const gchar *attribute, *selector;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &selector, &attribute);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_query_selector (document, selector, NULL);
		if (element)
			value = webkit_dom_element_get_attribute (element, attribute);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "ElementRemoveAttribute") == 0) {
		const gchar *element_id, *attribute;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &attribute);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_element_remove_attribute (element, attribute);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementRemoveAttributeBySelector") == 0) {
		const gchar *attribute, *selector;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &selector, &attribute);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_query_selector (document, selector, NULL);
		if (element)
			webkit_dom_element_remove_attribute (element, attribute);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementSetAttribute") == 0) {
		const gchar *element_id, *attribute, *value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters,
			"(t&s&s&s)",
			&page_id, &element_id, &attribute, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_element_set_attribute (
				element, attribute, value, NULL);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementSetAttributeBySelector") == 0) {
		const gchar *attribute, *selector, *value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s&s)", &page_id, &selector, &attribute, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_query_selector (document, selector, NULL);
		if (element) {
			if (g_strcmp0 (selector, "body") == 0 &&
			    g_strcmp0 (attribute, "link") == 0)
				e_editor_dom_set_link_color (editor_page, value);
			else if (g_strcmp0 (selector, "body") == 0 &&
			         g_strcmp0 (attribute, "vlink") == 0)
				e_editor_dom_set_visited_link_color (editor_page, value);
			else
				webkit_dom_element_set_attribute (
					element, attribute, value, NULL);
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementGetTagName") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_element_get_tag_name (element);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "RemoveImageAttributesFromElementBySelector") == 0) {
		const gchar *selector;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &selector);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_query_selector (document, selector, NULL);
		if (element) {
			webkit_dom_element_remove_attribute (element, "background");
			webkit_dom_element_remove_attribute (element, "data-uri");
			webkit_dom_element_remove_attribute (element, "data-inline");
			webkit_dom_element_remove_attribute (element, "data-name");
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogMarkCurrentCellElement") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_mark_current_cell_element (editor_page, element_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogSaveHistoryOnExit") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_save_history_on_exit (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogSetElementVAlign") == 0) {
		const gchar *value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(t&si)", &page_id, &value, &scope);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_set_element_v_align (editor_page, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogSetElementAlign") == 0) {
		const gchar *value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(t&si)", &page_id, &value, &scope);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_set_element_align (editor_page, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogSetElementNoWrap") == 0) {
		gboolean value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(tbi)", &page_id, &value, &scope);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_set_element_no_wrap (editor_page, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogSetElementHeaderStyle") == 0) {
		gboolean value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(tbi)", &page_id, &value, &scope);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_set_element_header_style (editor_page, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogSetElementWidth") == 0) {
		const gchar *value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(t&si)", &page_id, &value, &scope);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_set_element_width (editor_page, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogSetElementColSpan") == 0) {
		glong value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(tii)", &page_id, &value, &scope);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_set_element_col_span (editor_page, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogSetElementRowSpan") == 0) {
		glong value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(tii)", &page_id, &value, &scope);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_set_element_row_span (editor_page, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorCellDialogSetElementBgColor") == 0) {
		const gchar *value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(t&si)", &page_id, &value, &scope);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_cell_set_element_bg_color (editor_page, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorHRuleDialogFindHRule") == 0) {
		gboolean created_new_hr = FALSE;
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		created_new_hr = e_dialogs_dom_h_rule_find_hrule (editor_page);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", created_new_hr));
	} else if (g_strcmp0 (method_name, "EEditorHRuleDialogOnClose") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_h_rule_dialog_on_close (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorImageDialogMarkImage") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_image_mark_image (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorImageDialogSaveHistoryOnExit") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_image_save_history_on_exit (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorImageDialogSetElementUrl") == 0) {
		const gchar *value;

		g_variant_get (parameters, "(t&s)", &page_id, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_image_set_element_url (editor_page, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorImageDialogGetElementUrl") == 0) {
		gchar *value;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		value = e_dialogs_dom_image_get_element_url (editor_page);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "ImageElementSetWidth") == 0) {
		const gchar *element_id;
		gint32 value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetWidth") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "ImageElementSetHeight") == 0) {
		const gchar *element_id;
		gint32 value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetHeight") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_height (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "ImageElementGetNaturalWidth") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_natural_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "ImageElementGetNaturalHeight") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_natural_height (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "ImageElementSetHSpace") == 0) {
		const gchar *element_id;
		gint32 value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_hspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetHSpace") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_hspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "ImageElementSetVSpace") == 0) {
		const gchar *element_id;
		gint32 value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_image_element_set_vspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ImageElementGetVSpace") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_vspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "EEditorLinkDialogOk") == 0) {
		const gchar *url, *inner_text;

		g_variant_get (parameters, "(t&s&s)", &page_id, &url, &inner_text);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_link_commit (editor_page, url, inner_text);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorLinkDialogShow") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		g_dbus_method_invocation_return_value (
			invocation, e_dialogs_dom_link_show (editor_page));
	} else if (g_strcmp0 (method_name, "EEditorPageDialogSaveHistory") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_page_save_history (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorPageDialogSaveHistoryOnExit") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_page_save_history_on_exit (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorSpellCheckDialogNext") == 0) {
		const gchar *from_word = NULL;
		const gchar * const *languages = NULL;
		gchar *value = NULL;

		g_variant_get (parameters, "(t&s^as)", &page_id, &from_word, &languages);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		value = e_dialogs_dom_spell_check_next (editor_page, from_word, languages);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "EEditorSpellCheckDialogPrev") == 0) {
		const gchar *from_word = NULL;
		const gchar * const *languages = NULL;
		gchar *value = NULL;

		g_variant_get (parameters, "(t&s^as)", &page_id, &from_word, &languages);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		value = e_dialogs_dom_spell_check_prev (editor_page, from_word, languages);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "EEditorTableDialogSetRowCount") == 0) {
		guint32 value;

		g_variant_get (parameters, "(tu)", &page_id, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_table_set_row_count (editor_page, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorTableDialogGetRowCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		value = e_dialogs_dom_table_get_row_count (editor_page);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(u)", value));
	} else if (g_strcmp0 (method_name, "EEditorTableDialogSetColumnCount") == 0) {
		guint32 value;

		g_variant_get (parameters, "(tu)", &page_id, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_table_set_column_count (editor_page, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorTableDialogGetColumnCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		value = e_dialogs_dom_table_get_column_count (editor_page);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(u)", value));
	} else if (g_strcmp0 (method_name, "EEditorTableDialogShow") == 0) {
		gboolean created_new_table;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		created_new_table = e_dialogs_dom_table_show (editor_page);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", created_new_table));
	} else if (g_strcmp0 (method_name, "EEditorTableDialogSaveHistoryOnExit") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_table_save_history_on_exit (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorDialogDeleteCellContents") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_delete_cell_contents (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorDialogDeleteColumn") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_delete_column (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorDialogDeleteRow") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_delete_row (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorDialogDeleteTable") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_delete_table (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorDialogInsertColumnAfter") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_insert_column_after (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorDialogInsertColumnBefore") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_insert_column_before (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorDialogInsertRowAbove") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_insert_row_above (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorDialogInsertRowBelow") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_insert_row_below (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorLinkDialogOnOpen") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_link_dialog_on_open (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorLinkDialogOnClose") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_dialogs_dom_link_dialog_on_close (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorLinkDialogUnlink") == 0) {
		EEditorUndoRedoManager *manager;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		manager = e_editor_page_get_undo_redo_manager (editor_page);
		/* Remove the history event that was saved when the dialog was opened */
		e_editor_undo_redo_manager_remove_current_history_event (manager);

		e_editor_dom_selection_unlink (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EEditorActionsSaveHistoryForCut") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_save_history_for_cut (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "TableCellElementGetNoWrap") == 0) {
		const gchar *element_id;
		gboolean value = FALSE;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_no_wrap (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", value));
	} else if (g_strcmp0 (method_name, "TableCellElementGetRowSpan") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_row_span (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "TableCellElementGetColSpan") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		document = e_editor_page_get_document (editor_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_col_span (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "DOMSaveSelection") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_save (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMRestoreSelection") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_restore (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMUndo") == 0) {
		EEditorUndoRedoManager *manager;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		manager = e_editor_page_get_undo_redo_manager (editor_page);

		e_editor_undo_redo_manager_undo (manager);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMRedo") == 0) {
		EEditorUndoRedoManager *manager;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		manager = e_editor_page_get_undo_redo_manager (editor_page);

		e_editor_undo_redo_manager_redo (manager);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMTurnSpellCheckOff") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_turn_spell_check_off (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMQuoteAndInsertTextIntoSelection") == 0) {
		gboolean is_html = FALSE;
		const gchar *text;

		g_variant_get (parameters, "(t&sb)", &page_id, &text, &is_html);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_quote_and_insert_text_into_selection (editor_page, text, is_html);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMConvertAndInsertHTMLIntoSelection") == 0) {
		gboolean is_html;
		const gchar *text;

		g_variant_get (parameters, "(t&sb)", &page_id, &text, &is_html);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_convert_and_insert_html_into_selection (editor_page, text, is_html);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMEmbedStyleSheet") == 0) {
		const gchar *style_sheet_content;

		g_variant_get (parameters, "(t&s)", &page_id, &style_sheet_content);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_embed_style_sheet (editor_page, style_sheet_content);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMRemoveEmbeddedStyleSheet") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_remove_embedded_style_sheet (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetPastingContentFromItself") == 0) {
		gboolean value = FALSE;

		g_variant_get (parameters, "(tb)", &page_id, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_page_set_pasting_content_from_itself (editor_page, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetEditorHTMLMode") == 0) {
		gboolean html_mode = FALSE;
		gboolean convert = FALSE;

		g_variant_get (parameters, "(tbb)", &page_id, &html_mode, &convert);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		convert = convert && e_editor_page_get_html_mode (editor_page) && !html_mode;
		e_editor_page_set_html_mode (editor_page, html_mode);

		if (convert)
			e_editor_dom_convert_when_changing_composer_mode (editor_page);
		else
			e_editor_dom_process_content_after_mode_change (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetConvertInSitu") == 0) {
		gboolean value = FALSE;

		g_variant_get (parameters, "(tb)", &page_id, &value);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_page_set_convert_in_situ (editor_page, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMForceSpellCheck") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_force_spell_check (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMCheckIfConversionNeeded") == 0) {
		gboolean conversion_needed;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		conversion_needed = e_editor_dom_check_if_conversion_needed (editor_page);
		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", conversion_needed));
	} else if (g_strcmp0 (method_name, "DOMGetContent") == 0) {
		EContentEditorGetContentFlags flags;
		const gchar *from_domain;
		gchar *value = NULL;
		GVariant *inline_images = NULL;

		g_variant_get (parameters, "(t&si)", &page_id, &from_domain, &flags);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		if ((flags & E_CONTENT_EDITOR_GET_INLINE_IMAGES) && from_domain && *from_domain)
			inline_images = e_editor_dom_get_inline_images_data (editor_page, from_domain);

		if ((flags & E_CONTENT_EDITOR_GET_TEXT_HTML) &&
		    !(flags & E_CONTENT_EDITOR_GET_PROCESSED)) {
			value = e_editor_dom_process_content_for_draft (
				editor_page, (flags & E_CONTENT_EDITOR_GET_BODY));
		} else if ((flags & E_CONTENT_EDITOR_GET_TEXT_HTML) &&
			   (flags & E_CONTENT_EDITOR_GET_PROCESSED) &&
			   !(flags & E_CONTENT_EDITOR_GET_BODY)) {
			value = e_editor_dom_process_content_to_html_for_exporting (editor_page);
		} else if ((flags & E_CONTENT_EDITOR_GET_TEXT_PLAIN) &&
			   (flags & E_CONTENT_EDITOR_GET_PROCESSED) &&
			   !(flags & E_CONTENT_EDITOR_GET_BODY)) {
			value = e_editor_dom_process_content_to_plain_text_for_exporting (editor_page);
		} else if ((flags & E_CONTENT_EDITOR_GET_TEXT_PLAIN) &&
		           (flags & E_CONTENT_EDITOR_GET_BODY) &&
		           !(flags & E_CONTENT_EDITOR_GET_PROCESSED)) {
			if (flags & E_CONTENT_EDITOR_GET_EXCLUDE_SIGNATURE)
				value = e_composer_dom_get_raw_body_content_without_signature (editor_page);
			else
				value = e_composer_dom_get_raw_body_content (editor_page);
		} else {
			g_warning ("Unsupported flags combination (%d) in (%s)", flags, G_STRFUNC);
		}

		/* If no inline images are requested we still have to return
		 * something even it won't be used at all. */
		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(sv)",
				value ? value : "",
				inline_images ? inline_images : g_variant_new_int32 (0)));

		g_free (value);

		if ((flags & E_CONTENT_EDITOR_GET_INLINE_IMAGES) && from_domain && *from_domain && inline_images)
			e_editor_dom_restore_images (editor_page, inline_images);
	} else if (g_strcmp0 (method_name, "DOMInsertHTML") == 0) {
		const gchar *html;

		g_variant_get (parameters, "(t&s)", &page_id, &html);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_insert_html (editor_page, html);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMConvertContent") == 0) {
		const gchar *preferred_text;

		g_variant_get (parameters, "(t&s)", &page_id, &preferred_text);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_convert_content (editor_page, preferred_text);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMAddNewInlineImageIntoList") == 0) {
		const gchar *cid_uri, *src, *filename;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &filename, &cid_uri, &src);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_page_add_new_inline_image_into_list (
			editor_page, cid_uri, src);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMReplaceImageSrc") == 0) {
		const gchar *selector, *uri;

		g_variant_get (parameters, "(t&s&s)", &page_id, &selector, &uri);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_replace_image_src (editor_page, selector, uri);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMDragAndDropEnd") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_drag_and_drop_end (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMInsertSmiley") == 0) {
		const gchar *smiley_name;

		g_variant_get (parameters, "(t&s)", &page_id, &smiley_name);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_insert_smiley_by_name (editor_page, smiley_name);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMMoveSelectionOnPoint") == 0) {
		gboolean cancel_if_not_collapsed;
		gint x, y;

		g_variant_get (parameters, "(tiib)", &page_id, &x, &y, &cancel_if_not_collapsed);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		if (cancel_if_not_collapsed) {
			if (e_editor_dom_selection_is_collapsed (editor_page))
				e_editor_dom_selection_set_on_point (editor_page, x, y);
		} else
			e_editor_dom_selection_set_on_point (editor_page, x, y);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionIndent") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_indent (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSave") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_save (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionRestore") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_restore (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionInsertImage") == 0) {
		const gchar *uri;

		g_variant_get (parameters, "(t&s)", &page_id, &uri);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_insert_image (editor_page, uri);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionReplace") == 0) {
		const gchar *replacement;

		g_variant_get (parameters, "(t&s)", &page_id, &replacement);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_replace (editor_page, replacement);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetAlignment") == 0) {
		EContentEditorAlignment alignment;

		g_variant_get (parameters, "(ti)", &page_id, &alignment);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_set_alignment (editor_page, alignment);
		e_editor_page_set_alignment (editor_page, alignment);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetBold") == 0) {
		gboolean bold;

		g_variant_get (parameters, "(tb)", &page_id, &bold);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_page_set_bold (editor_page, bold);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetBlockFormat") == 0) {
		EContentEditorBlockFormat block_format;

		g_variant_get (parameters, "(ti)", &page_id, &block_format);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_set_block_format (editor_page, block_format);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetFontColor") == 0) {
		const gchar *color;

		g_variant_get (parameters, "(t&s)", &page_id, &color);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_set_font_color (editor_page, color);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetFontSize") == 0) {
		EContentEditorFontSize font_size;

		g_variant_get (parameters, "(ti)", &page_id, &font_size);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_set_font_size (editor_page, font_size);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetItalic") == 0) {
		gboolean italic;

		g_variant_get (parameters, "(tb)", &page_id, &italic);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_page_set_italic (editor_page, italic);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetMonospaced") == 0) {
		gboolean monospaced;

		g_variant_get (parameters, "(tb)", &page_id, &monospaced);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_page_set_monospace (editor_page, monospaced);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetStrikethrough") == 0) {
		gboolean strikethrough;

		g_variant_get (parameters, "(tb)", &page_id, &strikethrough);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_page_set_strikethrough (editor_page, strikethrough);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetSubscript") == 0) {
		gboolean subscript;

		g_variant_get (parameters, "(tb)", &page_id, &subscript);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_set_subscript (editor_page, subscript);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetSuperscript") == 0) {
		gboolean superscript;

		g_variant_get (parameters, "(tb)", &page_id, &superscript);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_set_superscript (editor_page, superscript);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetUnderline") == 0) {
		gboolean underline;

		g_variant_get (parameters, "(tb)", &page_id, &underline);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_page_set_underline (editor_page, underline);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionUnindent") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_unindent (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionWrap") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_selection_wrap (editor_page);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMGetCaretWord") == 0) {
		gchar *word;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		word = e_editor_dom_get_caret_word (editor_page);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					word ? word : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "DOMReplaceCaretWord") == 0) {
		const gchar *replacement = NULL;

		g_variant_get (parameters, "(t&s)", &page_id, &replacement);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_editor_dom_replace_caret_word (editor_page, replacement);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMInsertSignature") == 0) {
		gboolean is_html, set_signature_from_message;
		gboolean check_if_signature_is_changed, ignore_next_signature_change;
		const gchar *content, *signature_id;
		gchar *new_signature_id = NULL;

		g_variant_get (
			parameters,
			"(t&sb&sbbb)",
			&page_id,
			&content,
			&is_html,
			&signature_id,
			&set_signature_from_message,
			&check_if_signature_is_changed,
			&ignore_next_signature_change);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		new_signature_id = e_composer_dom_insert_signature (
			editor_page,
			content,
			is_html,
			signature_id,
			&set_signature_from_message,
			&check_if_signature_is_changed,
			&ignore_next_signature_change);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(sbbb)",
				new_signature_id ? new_signature_id : "",
				set_signature_from_message,
				check_if_signature_is_changed,
				ignore_next_signature_change));

		g_free (new_signature_id);
	} else if (g_strcmp0 (method_name, "DOMSaveDragAndDropHistory") == 0) {
		g_variant_get (
			parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_composer_dom_save_drag_and_drop_history (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMCleanAfterDragAndDrop") == 0) {
		g_variant_get (
			parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		e_composer_dom_clean_after_drag_and_drop (editor_page);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMGetActiveSignatureUid") == 0) {
		gchar *value;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		value = e_composer_dom_get_active_signature_uid (editor_page);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "DOMGetCaretPosition") == 0) {
		guint32 value;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		value = e_editor_dom_get_caret_position (editor_page);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new ("(u)", value));
	} else if (g_strcmp0 (method_name, "DOMGetCaretOffset") == 0) {
		guint32 value;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		value = e_editor_dom_get_caret_offset (editor_page);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new ("(u)", value));
	} else if (g_strcmp0 (method_name, "DOMClearUndoRedoHistory") == 0) {
		EEditorUndoRedoManager *manager;

		g_variant_get (parameters, "(t)", &page_id);

		editor_page = get_editor_page_or_return_dbus_error (invocation, extension, page_id);
		if (!editor_page)
			goto error;

		manager = e_editor_page_get_undo_redo_manager (editor_page);
		if (manager)
			e_editor_undo_redo_manager_clean_history (manager);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		g_warning ("UNKNOWN METHOD '%s'", method_name);
	}

	return;

 error:
	g_warning ("Cannot obtain WebKitWebPage for '%ld'", page_id);
}

static void
web_page_gone_cb (gpointer user_data,
                  GObject *gone_web_page)
{
	EEditorWebExtension *extension = user_data;
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension));

	g_hash_table_iter_init (&iter, extension->priv->editor_pages);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		if (value == gone_web_page) {
			g_hash_table_remove (extension->priv->editor_pages, key);
			break;
		}
	}
}

static const GDBusInterfaceVTable interface_vtable = {
	handle_method_call,
	NULL,
	NULL
};

static void
e_editor_web_extension_dispose (GObject *object)
{
	EEditorWebExtension *extension = E_EDITOR_WEB_EXTENSION (object);

	if (extension->priv->dbus_connection) {
		g_dbus_connection_unregister_object (
			extension->priv->dbus_connection,
			extension->priv->registration_id);
		extension->priv->registration_id = 0;
		extension->priv->dbus_connection = NULL;
	}

	g_hash_table_remove_all (extension->priv->editor_pages);

	g_clear_object (&extension->priv->wk_extension);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_editor_web_extension_parent_class)->dispose (object);
}

static void
e_editor_web_extension_finalize (GObject *object)
{
	EEditorWebExtension *extension = E_EDITOR_WEB_EXTENSION (object);

	if (extension->priv->editor_pages) {
		g_hash_table_destroy (extension->priv->editor_pages);
		extension->priv->editor_pages = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_editor_web_extension_parent_class)->finalize (object);
}

static void
e_editor_web_extension_class_init (EEditorWebExtensionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = e_editor_web_extension_dispose;
	object_class->finalize = e_editor_web_extension_finalize;

	g_type_class_add_private (object_class, sizeof(EEditorWebExtensionPrivate));
}

static void
e_editor_web_extension_init (EEditorWebExtension *extension)
{
	extension->priv = E_EDITOR_WEB_EXTENSION_GET_PRIVATE (extension);
	extension->priv->editor_pages = g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, g_object_unref);
}

static gpointer
e_editor_web_extension_create_instance(gpointer data)
{
	return g_object_new (E_TYPE_EDITOR_WEB_EXTENSION, NULL);
}

EEditorWebExtension *
e_editor_web_extension_get_default (void)
{
	static GOnce once_init = G_ONCE_INIT;
	return E_EDITOR_WEB_EXTENSION (g_once (&once_init, e_editor_web_extension_create_instance, NULL));
}

static gboolean
image_exists_in_cache (const gchar *image_uri)
{
	gchar *filename;
	gchar *hash;
	gboolean exists = FALSE;

	if (!emd_global_http_cache)
		return FALSE;

	hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, image_uri, -1);
	filename = camel_data_cache_get_filename (
		emd_global_http_cache, "http", hash);

	if (filename != NULL) {
		struct stat st;

		exists = g_file_test (filename, G_FILE_TEST_EXISTS);
		if (exists && g_stat (filename, &st) == 0) {
			exists = st.st_size != 0;
		} else {
			exists = FALSE;
		}
		g_free (filename);
	}

	g_free (hash);

	return exists;
}

static void
redirect_http_uri (EEditorWebExtension *extension,
                   WebKitWebPage *web_page,
                   WebKitURIRequest *request)
{
	const gchar *uri;
	gchar *new_uri;
	SoupURI *soup_uri;
	gboolean image_exists;
	EEditorPage *editor_page;
	EImageLoadingPolicy image_policy;

	editor_page = get_editor_page (extension, webkit_web_page_get_id (web_page));
	g_return_if_fail (E_IS_EDITOR_PAGE (editor_page));

	uri = webkit_uri_request_get_uri (request);

	/* Check Evolution's cache */
	image_exists = image_exists_in_cache (uri);

	/* If the URI is not cached and we are not allowed to load it
	 * then redirect to invalid URI, so that webkit would display
	 * a native placeholder for it. */
	image_policy = e_editor_page_get_image_loading_policy (editor_page);
	if (!image_exists && !e_editor_page_get_force_image_load (editor_page) &&
	    (image_policy == E_IMAGE_LOADING_POLICY_NEVER)) {
		webkit_uri_request_set_uri (request, "about:blank");
		return;
	}

	new_uri = g_strconcat ("evo-", uri, NULL);
	soup_uri = soup_uri_new (new_uri);
	g_free (new_uri);

	new_uri = soup_uri_to_string (soup_uri, FALSE);
	webkit_uri_request_set_uri (request, new_uri);
	soup_uri_free (soup_uri);

	g_free (new_uri);
}

static gboolean
web_page_send_request_cb (WebKitWebPage *web_page,
                          WebKitURIRequest *request,
                          WebKitURIResponse *redirected_response,
                          EEditorWebExtension *extension)
{
	const char *request_uri;
	const char *page_uri;
	gboolean uri_is_http;

	request_uri = webkit_uri_request_get_uri (request);
	page_uri = webkit_web_page_get_uri (web_page);

	/* Always load the main resource. */
	if (g_strcmp0 (request_uri, page_uri) == 0)
		return FALSE;

	uri_is_http =
		g_str_has_prefix (request_uri, "http:") ||
		g_str_has_prefix (request_uri, "https:") ||
		g_str_has_prefix (request_uri, "evo-http:") ||
		g_str_has_prefix (request_uri, "evo-https:");

	if (uri_is_http)
		redirect_http_uri (extension, web_page, request);

	return FALSE;
}

static void
web_page_document_loaded_cb (WebKitWebPage *web_page,
                             gpointer user_data)
{
	WebKitDOMDocument *document;
	WebKitDOMRange *range = NULL;
	WebKitDOMDOMWindow *dom_window;
	WebKitDOMDOMSelection *dom_selection;

	g_return_if_fail (WEBKIT_IS_WEB_PAGE (web_page));

	document = webkit_web_page_get_dom_document (web_page);
	if (!document)
		return;

	dom_window = webkit_dom_document_get_default_view (document);
	dom_selection = webkit_dom_dom_window_get_selection (dom_window);

	/* Make sure there is a cursor located in the body after the document loads. */
	if (!webkit_dom_dom_selection_get_anchor_node (dom_selection) &&
	    !webkit_dom_dom_selection_get_focus_node (dom_selection)) {
		range = webkit_dom_document_caret_range_from_point (document, 0, 0);
		webkit_dom_dom_selection_remove_all_ranges (dom_selection);
		webkit_dom_dom_selection_add_range (dom_selection, range);
	}

	g_clear_object (&range);
	g_clear_object (&dom_selection);
	g_clear_object (&dom_window);
}

static void
web_page_created_cb (WebKitWebExtension *wk_extension,
                     WebKitWebPage *web_page,
                     EEditorWebExtension *extension)
{
	EEditorPage *editor_page;
	guint64 *ppage_id;

	g_return_if_fail (WEBKIT_IS_WEB_PAGE (web_page));
	g_return_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension));

	ppage_id = g_new (guint64, 1);
	*ppage_id = webkit_web_page_get_id (web_page);

	editor_page = e_editor_page_new (web_page, extension);
	g_hash_table_insert (extension->priv->editor_pages, ppage_id, editor_page);

	g_object_weak_ref (G_OBJECT (web_page), web_page_gone_cb, extension);

	g_signal_connect (
		web_page, "send-request",
		G_CALLBACK (web_page_send_request_cb), extension);

	g_signal_connect (
		web_page, "document-loaded",
		G_CALLBACK (web_page_document_loaded_cb), NULL);
}

void
e_editor_web_extension_initialize (EEditorWebExtension *extension,
                                   WebKitWebExtension *wk_extension)
{
	g_return_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension));

	extension->priv->wk_extension = g_object_ref (wk_extension);

	if (emd_global_http_cache == NULL) {
		emd_global_http_cache = camel_data_cache_new (
			e_get_user_cache_dir (), NULL);

		if (emd_global_http_cache) {
			/* cache expiry - 2 hour access, 1 day max */
			camel_data_cache_set_expire_age (
				emd_global_http_cache, 24 * 60 * 60);
			camel_data_cache_set_expire_access (
				emd_global_http_cache, 2 * 60 * 60);
		}
	}

	g_signal_connect (
		wk_extension, "page-created",
		G_CALLBACK (web_page_created_cb), extension);
}

void
e_editor_web_extension_dbus_register (EEditorWebExtension *extension,
                                      GDBusConnection *connection)
{
	GError *error = NULL;
	static GDBusNodeInfo *introspection_data = NULL;

	g_return_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension));
	g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

	if (!introspection_data) {
		introspection_data =
			g_dbus_node_info_new_for_xml (introspection_xml, NULL);

		extension->priv->registration_id =
			g_dbus_connection_register_object (
				connection,
				E_WEBKIT_EDITOR_WEB_EXTENSION_OBJECT_PATH,
				introspection_data->interfaces[0],
				&interface_vtable,
				extension,
				NULL,
				&error);

		if (!extension->priv->registration_id) {
			g_warning ("Failed to register object: %s\n", error->message);
			g_error_free (error);
		} else {
			extension->priv->dbus_connection = connection;
			g_object_add_weak_pointer (
				G_OBJECT (connection),
				(gpointer *) &extension->priv->dbus_connection);
		}
	}
}

GDBusConnection *
e_editor_web_extension_get_connection (EEditorWebExtension *extension)
{
	g_return_val_if_fail (E_IS_EDITOR_WEB_EXTENSION (extension), NULL);

	return extension->priv->dbus_connection;
}
