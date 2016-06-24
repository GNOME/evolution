/*
 * e-html-editor-web-extension.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit2/webkit-web-extension.h>
#include <camel/camel.h>

#include <web-extensions/e-dom-utils.h>

#include "e-composer-private-dom-functions.h"
#include "e-html-editor-actions-dom-functions.h"
#include "e-html-editor-cell-dialog-dom-functions.h"
#include "e-html-editor-hrule-dialog-dom-functions.h"
#include "e-html-editor-image-dialog-dom-functions.h"
#include "e-html-editor-link-dialog-dom-functions.h"
#include "e-html-editor-page-dialog-dom-functions.h"
#include "e-html-editor-selection-dom-functions.h"
#include "e-html-editor-spell-check-dialog-dom-functions.h"
#include "e-html-editor-table-dialog-dom-functions.h"
#include "e-html-editor-test-dom-functions.h"
#include "e-html-editor-view-dom-functions.h"
#include "e-msg-composer-dom-functions.h"

#include "e-html-editor-web-extension.h"

#define E_HTML_EDITOR_WEB_EXTENSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_WEB_EXTENSION, EHTMLEditorWebExtensionPrivate))

struct _EHTMLEditorWebExtensionPrivate {
	WebKitWebExtension *wk_extension;

	GDBusConnection *dbus_connection;
	guint registration_id;
	guint spell_check_on_scroll_event_source_id;
	gboolean selection_changed_callbacks_blocked;

	/* These properties show the actual state of EHTMLEditorView */
	EContentEditorAlignment alignment;
	EContentEditorBlockFormat block_format;
	gchar *background_color;
	gchar *font_color;
	gchar *font_name;
	gchar *text;
	gint font_size;
	gboolean bold;
	gboolean indented;
	gboolean italic;
	gboolean monospaced;
	gboolean strikethrough;
	gboolean subscript;
	gboolean superscript;
	gboolean underline;

	gboolean force_image_load;
	gboolean html_mode;
	gboolean return_key_pressed;
	gboolean space_key_pressed;
	gboolean smiley_written;
	gint word_wrap_length;

	gboolean convert_in_situ;
	gboolean body_input_event_removed;
	gboolean dont_save_history_in_body_input;
	gboolean composition_in_progress;
	gboolean is_pasting_content_from_itself;
	gboolean renew_history_after_coordinates;

	GHashTable *inline_images;

	WebKitDOMNode *node_under_mouse_click;

	GSettings *mail_settings;

	EContentEditorContentFlags content_flags;

	GHashTable *undo_redo_managers /* EHTMLEditorUndoRedoManager * ~> WebKitWebPage * */;
	GSList *web_pages;

	ESpellChecker *spell_checker;
};

static CamelDataCache *emd_global_http_cache = NULL;

static const char introspection_xml[] =
"<node>"
"  <interface name='" E_HTML_EDITOR_WEB_EXTENSION_INTERFACE "'>"
"<!-- ********************************************************* -->"
"<!--                          SIGNALS                          -->"
"<!-- ********************************************************* -->"
"    <signal name='SelectionChanged'>"
"      <arg type='i' name='alignment' direction='out'/>"
"      <arg type='i' name='block_format' direction='out'/>"
"      <arg type='b' name='indented' direction='out'/>"
"      <arg type='b' name='bold' direction='out'/>"
"      <arg type='b' name='italic' direction='out'/>"
"      <arg type='b' name='underline' direction='out'/>"
"      <arg type='b' name='strikethrough' direction='out'/>"
"      <arg type='b' name='monospaced' direction='out'/>"
"      <arg type='b' name='subscript' direction='out'/>"
"      <arg type='b' name='superscript' direction='out'/>"
"      <arg type='b' name='underline' direction='out'/>"
"      <arg type='i' name='font_size' direction='out'/>"
"      <arg type='s' name='font_color' direction='out'/>"
"    </signal>"
"    <signal name='ContentChanged'>"
"    </signal>"
"    <signal name='UndoRedoStateChanged'>"
"      <arg type='t' name='page_id' direction='out'/>"
"      <arg type='b' name='can_undo' direction='out'/>"
"      <arg type='b' name='can_redo' direction='out'/>"
"    </signal>"
"<!-- ********************************************************* -->"
"<!--                          METHODS                          -->"
"<!-- ********************************************************* -->"
"<!-- ********************************************************* -->"
"<!--                       FOR TESTING ONLY                    -->"
"<!-- ********************************************************* -->"
"    <method name='TestHtmlEqual'>"
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
"<!--     Functions that are used in EHTMLEditorCellDialog      -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorCellDialogMarkCurrentCellElement'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSaveHistoryOnExit'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementVAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementNoWrap'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementHeaderStyle'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementColSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementRowSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementBgColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='i' name='scope' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorHRuleDialog      -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorHRuleDialogFindHRule'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='created_new_hr' direction='out'/>"
"    </method>"
"    <method name='EHTMLEditorHRuleDialogSaveHistoryOnExit'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='HRElementSetNoShade'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"    </method>"
"    <method name='HRElementGetNoShade'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='element_id' direction='in'/>"
"      <arg type='b' name='value' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorImageDialog     -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorImageDialogMarkImage'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorImageDialogSaveHistoryOnExit'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorImageDialogSetElementUrl'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorImageDialogGetElementUrl'>"
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
"<!--     Functions that are used in EHTMLEditorLinkDialog      -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorLinkDialogOk'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='url' direction='in'/>"
"      <arg type='s' name='inner_text' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorLinkDialogShow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='url' direction='out'/>"
"      <arg type='s' name='inner_text' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorPageDialog     -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorPageDialogSaveHistory'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorPageDialogSaveHistoryOnExit'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--   Functions that are used in EHTMLEditorSpellCheckDialog  -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorSpellCheckDialogNext'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='word' direction='in'/>"
"      <arg type='as' name='languages' direction='in'/>"
"      <arg type='s' name='next_word' direction='out'/>"
"    </method>"
"    <method name='EHTMLEditorSpellCheckDialogPrev'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='word' direction='in'/>"
"      <arg type='as' name='languages' direction='in'/>"
"      <arg type='s' name='prev_word' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorTableDialog     -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorTableDialogSetRowCount'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='value' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorTableDialogGetRowCount'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='value' direction='out'/>"
"    </method>"
"    <method name='EHTMLEditorTableDialogSetColumnCount'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='value' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorTableDialogGetColumnCount'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='value' direction='out'/>"
"    </method>"
"    <method name='EHTMLEditorTableDialogShow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='created_new_table' direction='out'/>"
"    </method>"
"    <method name='EHTMLEditorTableDialogSaveHistoryOnExit'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorActions         -->"
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
"    <method name='EHTMLEditorDialogDeleteCellContents'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorDialogDeleteColumn'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorDialogDeleteRow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorDialogDeleteTable'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorDialogInsertColumnAfter'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorDialogInsertColumnBefore'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorDialogInsertRowAbove'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorDialogInsertRowBelow'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorDialogDOMUnlink'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorDialogSaveHistoryForCut'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorView            -->"
"<!-- ********************************************************* -->"
"    <method name='SetCurrentContentFlags'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='content_flags' direction='in'/>"
"    </method>"
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
"<!--     Functions that are used in EHTMLEditorSelection       -->"
"<!-- ********************************************************* -->"
"    <method name='DOMSelectionIndent'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionSave'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionRestore'>"
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
"    <method name='DOMGetCaretWord'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='word' direction='out'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EComposerPrivate           -->"
"<!-- ********************************************************* -->"
"    <method name='DOMRemoveSignatures'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='top_signature' direction='in'/>"
"      <arg type='s' name='active_signature' direction='out'/>"
"    </method>"
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

G_DEFINE_TYPE (EHTMLEditorWebExtension, e_html_editor_web_extension, G_TYPE_OBJECT)

static WebKitWebPage *
get_webkit_web_page_or_return_dbus_error (GDBusMethodInvocation *invocation,
                                          WebKitWebExtension *web_extension,
                                          guint64 page_id)
{
	WebKitWebPage *web_page = webkit_web_extension_get_page (web_extension, page_id);
	if (!web_page) {
		g_dbus_method_invocation_return_error (
			invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
			"Invalid page ID: %" G_GUINT64_FORMAT, page_id);
	}
	return web_page;
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
        EHTMLEditorWebExtension *extension = E_HTML_EDITOR_WEB_EXTENSION (user_data);
	WebKitDOMDocument *document;
	WebKitWebExtension *web_extension = extension->priv->wk_extension;
	WebKitWebPage *web_page;

	if (g_strcmp0 (interface_name, E_HTML_EDITOR_WEB_EXTENSION_INTERFACE) != 0)
		return;

	if (g_strcmp0 (method_name, "TestHtmlEqual") == 0) {
		gboolean equal = FALSE;
		const gchar *html1 = NULL, *html2 = NULL;

		g_variant_get (parameters, "(t&s&s)", &page_id, &html1, &html2);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		equal = dom_test_html_equal (document, html1, html2);

		g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", equal));
	} else if (g_strcmp0 (method_name, "ElementHasAttribute") == 0) {
		gboolean value = FALSE;
		const gchar *element_id, *attribute;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &attribute);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_element_remove_attribute (element, attribute);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementRemoveAttributeBySelector") == 0) {
		const gchar *attribute, *selector;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &selector, &attribute);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_query_selector (document, selector, NULL);
		if (element) {
			if (g_strcmp0 (selector, "body") == 0 &&
			    g_strcmp0 (attribute, "link") == 0)
				dom_set_link_color (document, value);
			else if (g_strcmp0 (selector, "body") == 0 &&
			         g_strcmp0 (attribute, "vlink") == 0)
				dom_set_visited_link_color (document, value);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		g_variant_get (
			parameters, "(t&s)", &selector);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_query_selector (document, selector, NULL);
		if (element) {
			webkit_dom_element_remove_attribute (element, "background");
			webkit_dom_element_remove_attribute (element, "data-uri");
			webkit_dom_element_remove_attribute (element, "data-inline");
			webkit_dom_element_remove_attribute (element, "data-name");
		}

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogMarkCurrentCellElement") == 0) {
		const gchar *element_id;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_mark_current_cell_element (document, extension, element_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSaveHistoryOnExit") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_save_history_on_exit (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementVAlign") == 0) {
		const gchar *value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(t&si)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_v_align (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementAlign") == 0) {
		const gchar *value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_align (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementNoWrap") == 0) {
		gboolean value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(tbu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_no_wrap (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementHeaderStyle") == 0) {
		gboolean value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(tbu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_header_style (
			document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementWidth") == 0) {
		const gchar *value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_width (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementColSpan") == 0) {
		glong value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(tiu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_col_span (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementRowSpan") == 0) {
		glong value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(tiu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_row_span (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementBgColor") == 0) {
		const gchar *value;
		EContentEditorScope scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_bg_color (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorHRuleDialogFindHRule") == 0) {
		gboolean created_new_hr = FALSE;
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		created_new_hr = e_html_editor_hrule_dialog_find_hrule (
			document, extension, extension->priv->node_under_mouse_click);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", created_new_hr));
	} else if (g_strcmp0 (method_name, "EHTMLEditorHRuleDialogSaveHistoryOnExit") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_hrule_dialog_save_history_on_exit (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "HRElementSetNoShade") == 0) {
		gboolean value = FALSE;
		const gchar *element_id;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&sb)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			webkit_dom_html_hr_element_set_no_shade (
				WEBKIT_DOM_HTML_HR_ELEMENT (element), value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "HRElementGetNoShade") == 0) {
		gboolean value = FALSE;
		const gchar *element_id;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_hr_element_get_no_shade (
				WEBKIT_DOM_HTML_HR_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", value));
	} else if (g_strcmp0 (method_name, "EHTMLEditorImageDialogMarkImage") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_image_dialog_mark_image (
			document, extension, extension->priv->node_under_mouse_click);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorImageDialogSaveHistoryOnExit") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_image_dialog_save_history_on_exit (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorImageDialogSetElementUrl") == 0) {
		const gchar *value;

		g_variant_get (parameters, "(t&s)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_image_dialog_set_element_url (document, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorImageDialogGetElementUrl") == 0) {
		gchar *value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_image_dialog_get_element_url (document);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "ImageElementSetWidth") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "ImageElementSetHeight") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_natural_height (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "ImageElementSetHSpace") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_hspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "ImageElementSetVSpace") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_vspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "EHTMLEditorLinkDialogOk") == 0) {
		const gchar *url, *inner_text;

		g_variant_get (parameters, "(t&s&s)", &page_id, &url, &inner_text);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_link_dialog_ok (document, extension, url, inner_text);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorLinkDialogShow") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);

		g_dbus_method_invocation_return_value (
			invocation,
			e_html_editor_link_dialog_show (document));
	} else if (g_strcmp0 (method_name, "EHTMLEditorPageDialogSaveHistory") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_page_dialog_save_history (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorPageDialogSaveHistoryOnExit") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_page_dialog_save_history_on_exit (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorSpellCheckDialogNext") == 0) {
		const gchar *from_word = NULL;
		const gchar * const *languages = NULL;
		gchar *value = NULL;

		g_variant_get (parameters, "(t&s^as)", &page_id, &from_word, &languages);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_spell_check_dialog_next (document, extension, from_word, languages);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "EHTMLEditorSpellCheckDialogPrev") == 0) {
		const gchar *from_word = NULL;
		const gchar * const *languages = NULL;
		gchar *value = NULL;

		g_variant_get (parameters, "(t&s^as)", &page_id, &from_word, &languages);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_spell_check_dialog_prev (document, extension, from_word, languages);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogSetRowCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(tu)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_table_dialog_set_row_count (document, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogGetRowCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_table_dialog_get_row_count (document);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(u)", value));
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogSetColumnCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(tu)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_table_dialog_set_column_count (document, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogGetColumnCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_table_dialog_get_column_count (document);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(u)", value));
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogShow") == 0) {
		gboolean created_new_table;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		created_new_table = e_html_editor_table_dialog_show (document, extension);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", created_new_table));
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogSaveHistoryOnExit") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_table_dialog_save_history_on_exit (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogDeleteCellContents") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_dialog_delete_cell_contents (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogDeleteColumn") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_dialog_delete_column (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogDeleteRow") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_dialog_delete_row (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogDeleteTable") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_dialog_delete_table (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogDeleteTable") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_dialog_delete_cell_contents (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogInsertColumnAfter") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_dialog_insert_column_after (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogInsertColumnBefore") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_dialog_insert_column_before (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogInsertRowAbove") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_dialog_insert_row_above (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogInsertRowBelow") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_dialog_insert_row_below (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogDOMUnlink") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_unlink (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogSaveHistoryForCut") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_unlink (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorDialogSaveHistoryForCut") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_save_history_for_cut (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "TableCellElementGetNoWrap") == 0) {
		const gchar *element_id;
		gboolean value = FALSE;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_col_span (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(i)", value));
	} else if (g_strcmp0 (method_name, "DOMSaveSelection") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_save (document);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMRestoreSelection") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_restore (document);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMUndo") == 0) {
		EHTMLEditorUndoRedoManager *manager;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		manager = e_html_editor_web_extension_get_undo_redo_manager (extension, document);

		e_html_editor_undo_redo_manager_undo (manager);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMRedo") == 0) {
		EHTMLEditorUndoRedoManager *manager;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		manager = e_html_editor_web_extension_get_undo_redo_manager (extension, document);

		e_html_editor_undo_redo_manager_redo (manager);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMTurnSpellCheckOff") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_turn_spell_check_off (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMQuoteAndInsertTextIntoSelection") == 0) {
		const gchar *text;

		g_variant_get (parameters, "(t&s)", &page_id, &text);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_quote_and_insert_text_into_selection (document, extension, text);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMConvertAndInsertHTMLIntoSelection") == 0) {
		gboolean is_html;
		const gchar *text;

		g_variant_get (parameters, "(t&sb)", &page_id, &text, &is_html);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_convert_and_insert_html_into_selection (document, extension, text, is_html);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMEmbedStyleSheet") == 0) {
		const gchar *style_sheet_content;

		g_variant_get (parameters, "(t&s)", &page_id, &style_sheet_content);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_embed_style_sheet (document, style_sheet_content);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMRemoveEmbeddedStyleSheet") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_remove_embedded_style_sheet (document);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetCurrentContentFlags") == 0) {
		g_variant_get (parameters, "(ti)", &page_id, &extension->priv->content_flags);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetPastingContentFromItself") == 0) {
		g_variant_get (
			parameters,
			"(tb)",
			&page_id,
			&extension->priv->is_pasting_content_from_itself);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetEditorHTMLMode") == 0) {
		gboolean html_mode = FALSE;
		gboolean convert = FALSE;

		g_variant_get (parameters, "(tbb)", &page_id, &html_mode, &convert);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);

		convert = convert && extension->priv->html_mode && !html_mode;
		extension->priv->html_mode = html_mode;

		if (convert)
			dom_convert_when_changing_composer_mode (document, extension);
		else
			dom_process_content_after_mode_change (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "SetConvertInSitu") == 0) {
		g_variant_get (
			parameters,
			"(tb)",
			&page_id,
			&extension->priv->convert_in_situ);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMForceSpellCheck") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_force_spell_check (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMCheckIfConversionNeeded") == 0) {
		gboolean conversion_needed;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		conversion_needed = dom_check_if_conversion_needed (document);
		g_dbus_method_invocation_return_value (
			invocation, g_variant_new ("(b)", conversion_needed));
	} else if (g_strcmp0 (method_name, "DOMGetContent") == 0) {
		EContentEditorGetContentFlags flags;
		const gchar *from_domain;
		gchar *value = NULL;
		GVariant *inline_images = NULL;

		g_variant_get (parameters, "(t&si)", &page_id, &from_domain, &flags);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);

		if ((flags & E_CONTENT_EDITOR_GET_INLINE_IMAGES) && from_domain && *from_domain)
			inline_images = dom_get_inline_images_data (document, extension, from_domain);

		if ((flags & E_CONTENT_EDITOR_GET_TEXT_HTML) &&
		    !(flags & E_CONTENT_EDITOR_GET_PROCESSED)) {
			value = dom_process_content_for_draft (
				document, extension, (flags & E_CONTENT_EDITOR_GET_BODY));
		} else if ((flags & E_CONTENT_EDITOR_GET_TEXT_HTML) &&
			   (flags & E_CONTENT_EDITOR_GET_PROCESSED) &&
			   !(flags & E_CONTENT_EDITOR_GET_BODY)) {
			value = dom_process_content_for_html (document, extension);
		} else if ((flags & E_CONTENT_EDITOR_GET_TEXT_PLAIN) &&
			   (flags & E_CONTENT_EDITOR_GET_PROCESSED) &&
			   !(flags & E_CONTENT_EDITOR_GET_BODY)) {
			value = dom_process_content_for_plain_text (document, extension);
		} else if ((flags & E_CONTENT_EDITOR_GET_TEXT_PLAIN) &&
		           (flags & E_CONTENT_EDITOR_GET_BODY) &&
		           !(flags & E_CONTENT_EDITOR_GET_PROCESSED)) {
			if (flags & E_CONTENT_EDITOR_GET_EXCLUDE_SIGNATURE)
				value = dom_get_raw_body_content_without_signature  (document);
			else
				value = dom_get_raw_body_content (document);
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

		if ((flags & E_CONTENT_EDITOR_GET_INLINE_IMAGES) && from_domain && *from_domain && inline_images) {
			dom_restore_images (document, inline_images);
			g_object_unref (inline_images);
		}
	} else if (g_strcmp0 (method_name, "DOMInsertHTML") == 0) {
		const gchar *html;

		g_variant_get (parameters, "(t&s)", &page_id, &html);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_insert_html (document, extension, html);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMConvertContent") == 0) {
		const gchar *preferred_text;

		g_variant_get (parameters, "(t&s)", &page_id, &preferred_text);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_convert_content (document, extension, preferred_text);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMAddNewInlineImageIntoList") == 0) {
		const gchar *cid_uri, *src, *filename;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &filename, &cid_uri, &src);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		e_html_editor_web_extension_add_new_inline_image_into_list (
			extension, cid_uri, src);

		document = webkit_web_page_get_dom_document (web_page);
		dom_insert_base64_image (document, extension, filename, cid_uri, src);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMReplaceImageSrc") == 0) {
		const gchar *selector, *uri;

		g_variant_get (parameters, "(t&s&s)", &page_id, &selector, &uri);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_replace_image_src (document, extension, selector, uri);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMDragAndDropEnd") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_drag_and_drop_end (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMInsertSmiley") == 0) {
		const gchar *smiley_name;

		g_variant_get (parameters, "(t&s)", &page_id, &smiley_name);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_insert_smiley_by_name (document, extension, smiley_name);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMMoveSelectionOnPoint") == 0) {
		gboolean cancel_if_not_collapsed;
		gint x, y;

		g_variant_get (parameters, "(tiib)", &page_id, &x, &y, &cancel_if_not_collapsed);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		if (cancel_if_not_collapsed) {
			if (dom_selection_is_collapsed (document))
				dom_selection_set_on_point (document, x, y);
		} else
			dom_selection_set_on_point (document, x, y);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionIndent") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_indent (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSave") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_save (document);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionRestore") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_restore (document);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionInsertImage") == 0) {
		const gchar *uri;

		g_variant_get (parameters, "(t&s)", &page_id, &uri);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_insert_image (document, extension, uri);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionReplace") == 0) {
		const gchar *replacement;

		g_variant_get (parameters, "(t&s)", &page_id, &replacement);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_replace (document, extension, replacement);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetAlignment") == 0) {
		EContentEditorAlignment alignment;

		g_variant_get (parameters, "(ti)", &page_id, &alignment);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_alignment (document, extension, alignment);
		extension->priv->alignment = alignment;
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetBold") == 0) {
		gboolean bold;

		g_variant_get (parameters, "(tb)", &page_id, &bold);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_bold (document, extension, bold);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetBlockFormat") == 0) {
		EContentEditorBlockFormat block_format;

		g_variant_get (parameters, "(ti)", &page_id, &block_format);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_block_format (document, extension, block_format);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetFontColor") == 0) {
		const gchar *color;

		g_variant_get (parameters, "(t&s)", &page_id, &color);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_font_color (document, extension, color);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetFontSize") == 0) {
		EContentEditorFontSize font_size;

		g_variant_get (parameters, "(ti)", &page_id, &font_size);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_font_size (document, extension, font_size);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetItalic") == 0) {
		gboolean italic;

		g_variant_get (parameters, "(tb)", &page_id, &italic);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_italic (document, extension, italic);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetMonospaced") == 0) {
		gboolean monospaced;

		g_variant_get (parameters, "(tb)", &page_id, &monospaced);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_monospaced (document, extension, monospaced);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetStrikethrough") == 0) {
		gboolean strikethrough;

		g_variant_get (parameters, "(tb)", &page_id, &strikethrough);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_strikethrough (document, extension, strikethrough);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetSubscript") == 0) {
		gboolean subscript;

		g_variant_get (parameters, "(tb)", &page_id, &subscript);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_subscript (document, extension, subscript);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetSuperscript") == 0) {
		gboolean superscript;

		g_variant_get (parameters, "(tb)", &page_id, &superscript);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_superscript (document, extension, superscript);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionSetUnderline") == 0) {
		gboolean underline;

		g_variant_get (parameters, "(tb)", &page_id, &underline);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_underline (document, extension, underline);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionUnindent") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_unindent (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionWrap") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_wrap (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMGetCaretWord") == 0) {
		gchar *word;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		word = dom_get_caret_word (document);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					word ? word : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "DOMRemoveSignatures") == 0) {
		gboolean top_signature;
		gchar *active_signature;

		g_variant_get (parameters, "(tb)", &page_id, &top_signature);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		active_signature = dom_remove_signatures (document, extension, top_signature);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					active_signature ? active_signature : g_strdup (""))));
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		new_signature_id = dom_insert_signature (
			document,
			extension,
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

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_save_drag_and_drop_history (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMCleanAfterDragAndDrop") == 0) {
		g_variant_get (
			parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		dom_clean_after_drag_and_drop (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMGetActiveSignatureUid") == 0) {
		gchar *value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		value = dom_get_active_signature_uid (document);

		g_dbus_method_invocation_return_value (
			invocation,
			g_variant_new (
				"(@s)",
				g_variant_new_take_string (
					value ? value : g_strdup (""))));
	} else if (g_strcmp0 (method_name, "DOMGetCaretPosition") == 0) {
		guint32 value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		value = dom_get_caret_position (document);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_uint32 (value) : NULL);
	} else if (g_strcmp0 (method_name, "DOMGetCaretOffset") == 0) {
		guint32 value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		document = webkit_web_page_get_dom_document (web_page);
		value = dom_get_caret_offset (document, extension);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_uint32 (value) : NULL);
	} else if (g_strcmp0 (method_name, "DOMClearUndoRedoHistory") == 0) {
		EHTMLEditorUndoRedoManager *manager;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (invocation, web_extension, page_id);
		if (!web_page)
			goto error;

		manager = e_html_editor_web_extension_get_undo_redo_manager (extension,
			webkit_web_page_get_dom_document (web_page));
		if (manager)
			e_html_editor_undo_redo_manager_clean_history (manager);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else {
		g_warning ("UNKNOWN METHOD '%s:i'", method_name);
	}

	return;

 error:
	g_warning ("Cannot obtain WebKitWebPage for '%ld'", page_id);
}

static void
web_page_gone_cb (gpointer user_data,
		  GObject *gone_web_page)
{
	EHTMLEditorWebExtension *extension = user_data;
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension));

	extension->priv->web_pages = g_slist_remove (extension->priv->web_pages, gone_web_page);

	g_hash_table_iter_init (&iter, extension->priv->undo_redo_managers);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		if (value == gone_web_page) {
			g_hash_table_remove (extension->priv->undo_redo_managers, key);
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
e_html_editor_web_extension_dispose (GObject *object)
{
	EHTMLEditorWebExtension *extension = E_HTML_EDITOR_WEB_EXTENSION (object);

	if (extension->priv->spell_check_on_scroll_event_source_id > 0) {
		g_source_remove (extension->priv->spell_check_on_scroll_event_source_id);
		extension->priv->spell_check_on_scroll_event_source_id = 0;
	}

	if (extension->priv->dbus_connection) {
		g_dbus_connection_unregister_object (
			extension->priv->dbus_connection,
			extension->priv->registration_id);
		extension->priv->registration_id = 0;
		extension->priv->dbus_connection = NULL;
	}

	if (extension->priv->background_color != NULL) {
		g_free (extension->priv->background_color);
		extension->priv->background_color = NULL;
	}

	if (extension->priv->font_color != NULL) {
		g_free (extension->priv->font_color);
		extension->priv->font_color = NULL;
	}

	if (extension->priv->font_name != NULL) {
		g_free (extension->priv->font_name);
		extension->priv->font_name = NULL;
	}

	if (extension->priv->text != NULL) {
		g_free (extension->priv->text);
		extension->priv->text = NULL;
	}

	if (extension->priv->undo_redo_managers != NULL) {
		g_hash_table_destroy (extension->priv->undo_redo_managers);
		extension->priv->undo_redo_managers = NULL;
	}

	if (extension->priv->web_pages) {
		GSList *link;

		for (link = extension->priv->web_pages; link; link = g_slist_next (link)) {
			WebKitWebPage *page = link->data;

			if (!page)
				continue;

			g_object_weak_unref (G_OBJECT (page), web_page_gone_cb, extension);
		}

		g_slist_free (extension->priv->web_pages);
		extension->priv->web_pages = NULL;
	}

	if (extension->priv->mail_settings != NULL) {
		g_signal_handlers_disconnect_by_data (extension->priv->mail_settings, object);
		g_object_unref (extension->priv->mail_settings);
		extension->priv->mail_settings = NULL;
	}

	g_clear_object (&extension->priv->wk_extension);
	g_clear_object (&extension->priv->spell_checker);

	g_hash_table_remove_all (extension->priv->inline_images);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_html_editor_web_extension_parent_class)->dispose (object);
}
static void
e_html_editor_web_extension_finalize (GObject *object)
{
	EHTMLEditorWebExtension *extension = E_HTML_EDITOR_WEB_EXTENSION (object);

	g_hash_table_destroy (extension->priv->inline_images);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_html_editor_web_extension_parent_class)->finalize (object);
}

static void
e_html_editor_web_extension_class_init (EHTMLEditorWebExtensionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = e_html_editor_web_extension_dispose;
	object_class->finalize = e_html_editor_web_extension_finalize;

	g_type_class_add_private (object_class, sizeof(EHTMLEditorWebExtensionPrivate));
}

static void
e_html_editor_web_extension_init (EHTMLEditorWebExtension *extension)
{
	GSettings *g_settings;

	extension->priv = E_HTML_EDITOR_WEB_EXTENSION_GET_PRIVATE (extension);

	extension->priv->bold = FALSE;
	extension->priv->background_color = g_strdup ("");
	extension->priv->font_color = g_strdup ("");
	extension->priv->font_name = g_strdup ("");
	extension->priv->text = g_strdup ("");
	extension->priv->font_size = E_CONTENT_EDITOR_FONT_SIZE_NORMAL;
	extension->priv->indented = FALSE;
	extension->priv->italic = FALSE;
	extension->priv->monospaced = FALSE;
	extension->priv->strikethrough = FALSE;
	extension->priv->subscript = FALSE;
	extension->priv->superscript = FALSE;
	extension->priv->underline = FALSE;
	extension->priv->alignment = E_CONTENT_EDITOR_ALIGNMENT_LEFT;
	extension->priv->block_format = E_CONTENT_EDITOR_BLOCK_FORMAT_PARAGRAPH;
	extension->priv->force_image_load = FALSE;
	extension->priv->html_mode = FALSE;
	extension->priv->return_key_pressed = FALSE;
	extension->priv->space_key_pressed = FALSE;
	extension->priv->smiley_written = FALSE;

	extension->priv->convert_in_situ = FALSE;
	extension->priv->body_input_event_removed = TRUE;
	extension->priv->dont_save_history_in_body_input = FALSE;
	extension->priv->is_pasting_content_from_itself = FALSE;
	extension->priv->composition_in_progress = FALSE;
	extension->priv->renew_history_after_coordinates = TRUE;

	extension->priv->content_flags = 0;

	extension->priv->spell_check_on_scroll_event_source_id = 0;

	g_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	extension->priv->mail_settings = g_settings;

	extension->priv->word_wrap_length = g_settings_get_int (
		extension->priv->mail_settings, "composer-word-wrap-length");

	extension->priv->undo_redo_managers = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);

	extension->priv->inline_images = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	extension->priv->selection_changed_callbacks_blocked = FALSE;
	extension->priv->spell_checker = e_spell_checker_new ();
}

static gpointer
e_html_editor_web_extension_create_instance(gpointer data)
{
	return g_object_new (E_TYPE_HTML_EDITOR_WEB_EXTENSION, NULL);
}

EHTMLEditorWebExtension *
e_html_editor_web_extension_get (void)
{
	static GOnce once_init = G_ONCE_INIT;
	return E_HTML_EDITOR_WEB_EXTENSION (g_once (&once_init, e_html_editor_web_extension_create_instance, NULL));
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

static EImageLoadingPolicy
get_image_loading_policy (EHTMLEditorWebExtension *extension)
{
	EImageLoadingPolicy image_policy;

	image_policy = g_settings_get_enum (extension->priv->mail_settings, "image-loading-policy");

	return image_policy;
}

static void
redirect_http_uri (EHTMLEditorWebExtension *extension,
                   WebKitWebPage *web_page,
                   WebKitURIRequest *request)
{
	const gchar *uri;
	gchar *new_uri;
	SoupURI *soup_uri;
	gboolean image_exists;
	EImageLoadingPolicy image_policy;

	uri = webkit_uri_request_get_uri (request);

	/* Check Evolution's cache */
	image_exists = image_exists_in_cache (uri);

	/* If the URI is not cached and we are not allowed to load it
	 * then redirect to invalid URI, so that webkit would display
	 * a native placeholder for it. */
	image_policy = get_image_loading_policy (extension);
	if (!image_exists && !extension->priv->force_image_load &&
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
                          EHTMLEditorWebExtension *extension)
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
                             EHTMLEditorWebExtension *web_extension)
{
	WebKitDOMDocument *document;
	EHTMLEditorUndoRedoManager *manager;

	document = webkit_web_page_get_dom_document (web_page);
	manager = e_html_editor_web_extension_get_undo_redo_manager (web_extension, document);

	g_warn_if_fail (manager != NULL);
	e_html_editor_undo_redo_manager_set_document (manager, document);

	web_extension->priv->body_input_event_removed = TRUE;

	dom_process_content_after_load (document, web_extension);
}

static gboolean
web_page_context_menu_cb (WebKitWebPage *web_page,
		          WebKitContextMenu *context_menu,
			  WebKitWebHitTestResult *hit_test_result,
                          EHTMLEditorWebExtension *web_extension)
{
	WebKitDOMNode *node;
	EContentEditorNodeFlags flags = 0;
	GVariant *variant;

	node = webkit_web_hit_test_result_get_node (hit_test_result);
	web_extension->priv->node_under_mouse_click = node;

	if (WEBKIT_DOM_IS_HTML_HR_ELEMENT (node))
		flags |= E_CONTENT_EDITOR_NODE_IS_H_RULE;

	if (WEBKIT_DOM_IS_HTML_ANCHOR_ELEMENT (node) ||
	    (dom_node_find_parent_element (node, "A") != NULL))
		flags |= E_CONTENT_EDITOR_NODE_IS_ANCHOR;

	if (WEBKIT_DOM_IS_HTML_IMAGE_ELEMENT (node) ||
	    (dom_node_find_parent_element (node, "IMG") != NULL))
		flags |= E_CONTENT_EDITOR_NODE_IS_IMAGE;

	if (WEBKIT_DOM_IS_HTML_TABLE_CELL_ELEMENT (node) ||
	    (dom_node_find_parent_element (node, "TD") != NULL) ||
	    (dom_node_find_parent_element (node, "TH") != NULL))
		flags |= E_CONTENT_EDITOR_NODE_IS_TABLE_CELL;

	if (flags & E_CONTENT_EDITOR_NODE_IS_TABLE_CELL &&
	    (WEBKIT_DOM_IS_HTML_TABLE_ELEMENT (node) ||
	    dom_node_find_parent_element (node, "TABLE") != NULL))
		flags |= E_CONTENT_EDITOR_NODE_IS_TABLE;

	if (flags == 0)
		flags |= E_CONTENT_EDITOR_NODE_IS_TEXT;

	variant = g_variant_new_int32 (flags);
	webkit_context_menu_set_user_data (context_menu, variant);

	return FALSE;
}

static GVariant *
create_parameters_and_update_font_properties (WebKitDOMDocument *document,
                                              EHTMLEditorWebExtension *extension)
{
	extension->priv->alignment = dom_selection_get_alignment (document, extension);
	extension->priv->block_format = dom_selection_get_block_format (document, extension);
	extension->priv->indented = dom_selection_is_indented (document);

	if (extension->priv->html_mode) {
		extension->priv->bold = dom_selection_is_bold (document, extension);
		extension->priv->italic = dom_selection_is_italic (document, extension);
		extension->priv->underline = dom_selection_is_underline (document, extension);
		extension->priv->strikethrough = dom_selection_is_strikethrough (document, extension);
		extension->priv->monospaced = dom_selection_is_monospaced (document, extension);
		extension->priv->subscript = dom_selection_is_subscript (document, extension);
		extension->priv->superscript = dom_selection_is_superscript (document, extension);
		extension->priv->underline = dom_selection_is_underline (document, extension);
		extension->priv->font_size = dom_selection_get_font_size (document, extension);
		g_free (extension->priv->font_color);
		extension->priv->font_color = dom_selection_get_font_color (document, extension);
	}

	return g_variant_new ("(iibbbbbbbbbis)",
		(gint32) extension->priv->alignment,
		(gint32) extension->priv->block_format,
		extension->priv->indented,
		extension->priv->bold,
		extension->priv->italic,
		extension->priv->underline,
		extension->priv->strikethrough,
		extension->priv->monospaced,
		extension->priv->subscript,
		extension->priv->superscript,
		extension->priv->underline,
		(gint32) extension->priv->font_size,
		extension->priv->font_color ? extension->priv->font_color : "");
}

static void
web_editor_selection_changed_cb (WebKitWebEditor *editor,
                                 EHTMLEditorWebExtension *extension)
{
	WebKitWebPage *page;
	WebKitDOMDocument *document;
	WebKitDOMRange *range;
	GError *error = NULL;
	GVariant *parameters;

	if (extension->priv->selection_changed_callbacks_blocked)
		return;

	if (!extension->priv->dbus_connection)
		return;

	page = webkit_web_editor_get_page (editor);
	document = webkit_web_page_get_dom_document (page);
	range = dom_get_current_range (document);
	if (!range)
		return;
	g_object_unref (range);

	/*
	g_object_notify (G_OBJECT (selection), "background-color");
	g_object_notify (G_OBJECT (selection), "font-name");
	*/
	parameters = create_parameters_and_update_font_properties (document, extension);

	g_dbus_connection_emit_signal (
		extension->priv->dbus_connection,
		NULL,
		E_HTML_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		E_HTML_EDITOR_WEB_EXTENSION_INTERFACE,
		"SelectionChanged",
		parameters,
		&error);

	if (error) {
		g_warning ("Error emitting signal SelectionChanged: %s\n", error->message);
		g_error_free (error);
	}
}

static void
web_editor_can_undo_redo_notify_cb (EHTMLEditorUndoRedoManager *manager,
				    GParamSpec *param,
				    EHTMLEditorWebExtension *extension)
{
	GError *error = NULL;

	g_return_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension));

	g_dbus_connection_emit_signal (
		extension->priv->dbus_connection,
		NULL,
		E_HTML_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		E_HTML_EDITOR_WEB_EXTENSION_INTERFACE,
		"UndoRedoStateChanged",
		g_variant_new ("(tbb)",
			e_html_editor_web_extension_get_undo_redo_manager_page_id (extension, manager),
			e_html_editor_undo_redo_manager_can_undo (manager),
			e_html_editor_undo_redo_manager_can_redo (manager)),
		&error);

	if (error)
		g_warning ("%s: Failed to emit signal: %s", G_STRFUNC, error->message);
	g_clear_error (&error);
}

static void
web_page_created_cb (WebKitWebExtension *wk_extension,
                     WebKitWebPage *web_page,
                     EHTMLEditorWebExtension *extension)
{
	EHTMLEditorUndoRedoManager *manager;
	WebKitWebEditor *web_editor;

	extension->priv->web_pages = g_slist_prepend (extension->priv->web_pages, web_page);
	g_object_weak_ref (G_OBJECT (web_page), web_page_gone_cb, extension);

	manager = e_html_editor_undo_redo_manager_new (extension);
	g_hash_table_insert (extension->priv->undo_redo_managers, manager, web_page);

	g_signal_connect (
		manager, "notify::can-undo",
		G_CALLBACK (web_editor_can_undo_redo_notify_cb), extension);

	g_signal_connect (
		manager, "notify::can-redo",
		G_CALLBACK (web_editor_can_undo_redo_notify_cb), extension);

	g_signal_connect (
		web_page, "send-request",
		G_CALLBACK (web_page_send_request_cb), extension);

	g_signal_connect (
		web_page, "document-loaded",
		G_CALLBACK (web_page_document_loaded_cb), extension);

	g_signal_connect (
		web_page, "context-menu",
		G_CALLBACK (web_page_context_menu_cb), extension);

	web_editor = webkit_web_page_get_editor (web_page);

	g_signal_connect (
		web_editor, "selection-changed",
		G_CALLBACK (web_editor_selection_changed_cb), extension);
}

void
e_html_editor_web_extension_initialize (EHTMLEditorWebExtension *extension,
                                        WebKitWebExtension *wk_extension)
{
	g_return_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension));

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
e_html_editor_web_extension_dbus_register (EHTMLEditorWebExtension *extension,
                                           GDBusConnection *connection)
{
	GError *error = NULL;
	static GDBusNodeInfo *introspection_data = NULL;

	g_return_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension));
	g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

	if (!introspection_data) {
		introspection_data =
			g_dbus_node_info_new_for_xml (introspection_xml, NULL);

		extension->priv->registration_id =
			g_dbus_connection_register_object (
				connection,
				E_HTML_EDITOR_WEB_EXTENSION_OBJECT_PATH,
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

void
e_html_editor_web_extension_set_content_changed (EHTMLEditorWebExtension *extension)
{
	GError *error = NULL;

	if (!extension->priv->dbus_connection)
		return;

	g_dbus_connection_emit_signal (
		extension->priv->dbus_connection,
		NULL,
		E_HTML_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		E_HTML_EDITOR_WEB_EXTENSION_INTERFACE,
		"ContentChanged",
		NULL,
		&error);

	if (error) {
		g_warning ("Error emitting signal ContentChanged: %s\n", error->message);
		g_error_free (error);
	}
}

gboolean
e_html_editor_web_extension_get_html_mode (EHTMLEditorWebExtension *extension)
{
	return extension->priv->html_mode;
}

GDBusConnection *
e_html_editor_web_extension_get_connection (EHTMLEditorWebExtension *extension)
{
	return extension->priv->dbus_connection;
}

gint
e_html_editor_web_extension_get_word_wrap_length (EHTMLEditorWebExtension *extension)
{
	return extension->priv->word_wrap_length;
}

const gchar *
e_html_editor_web_extension_get_selection_text (EHTMLEditorWebExtension *extension)
{
	return extension->priv->text;
}

gboolean
e_html_editor_web_extension_get_bold (EHTMLEditorWebExtension *extension)
{
	return extension->priv->bold;
}

gboolean
e_html_editor_web_extension_get_italic (EHTMLEditorWebExtension *extension)
{
	return extension->priv->italic;
}

gboolean
e_html_editor_web_extension_get_underline (EHTMLEditorWebExtension *extension)
{
	return extension->priv->underline;
}

gboolean
e_html_editor_web_extension_get_monospaced (EHTMLEditorWebExtension *extension)
{
	return extension->priv->monospaced;
}

gboolean
e_html_editor_web_extension_get_strikethrough (EHTMLEditorWebExtension *extension)
{
	return extension->priv->strikethrough;
}

guint
e_html_editor_web_extension_get_font_size (EHTMLEditorWebExtension *extension)
{
	return extension->priv->font_size;
}

const gchar *
e_html_editor_web_extension_get_font_color (EHTMLEditorWebExtension *extension)
{
	return extension->priv->font_color;
}

EContentEditorAlignment
e_html_editor_web_extension_get_alignment (EHTMLEditorWebExtension *extension)
{
	return extension->priv->alignment;
}

EContentEditorContentFlags
e_html_editor_web_extension_get_current_content_flags (EHTMLEditorWebExtension *extension)
{
	return extension->priv->content_flags;
}

gboolean
e_html_editor_web_extension_get_return_key_pressed (EHTMLEditorWebExtension *extension)
{
	return extension->priv->return_key_pressed;
}

void
e_html_editor_web_extension_set_return_key_pressed (EHTMLEditorWebExtension *extension,
                                                    gboolean value)
{
	extension->priv->return_key_pressed = value;
}

gboolean
e_html_editor_web_extension_get_space_key_pressed (EHTMLEditorWebExtension *extension)
{
	return extension->priv->space_key_pressed;
}

void
e_html_editor_web_extension_set_space_key_pressed (EHTMLEditorWebExtension *extension,
                                                   gboolean value)
{
	extension->priv->space_key_pressed = value;
}

gboolean
e_html_editor_web_extension_get_magic_links_enabled (EHTMLEditorWebExtension *extension)
{
	return g_settings_get_boolean (
		extension->priv->mail_settings, "composer-magic-links");
}

gboolean
e_html_editor_web_extension_get_magic_smileys_enabled (EHTMLEditorWebExtension *extension)
{
	return g_settings_get_boolean (
		extension->priv->mail_settings, "composer-magic-smileys");
}

gboolean
e_html_editor_web_extension_get_unicode_smileys_enabled (EHTMLEditorWebExtension *extension)
{
	return g_settings_get_boolean (
		extension->priv->mail_settings, "composer-unicode-smileys");
}

gboolean
e_html_editor_web_extension_get_inline_spelling_enabled (EHTMLEditorWebExtension *extension)
{
	return g_settings_get_boolean (
		extension->priv->mail_settings, "composer-inline-spelling");
}

gboolean
e_html_editor_web_extension_check_word_spelling (EHTMLEditorWebExtension *extension,
						 const gchar *word,
						 const gchar * const *languages)
{
	g_return_val_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension), TRUE);

	if (!word || !languages || !*languages)
		return TRUE;

	e_spell_checker_set_active_languages (extension->priv->spell_checker, languages);

	return e_spell_checker_check_word (extension->priv->spell_checker, word, -1);
}

gboolean
e_html_editor_web_extension_get_body_input_event_removed (EHTMLEditorWebExtension *extension)
{
	return extension->priv->body_input_event_removed;
}

void
e_html_editor_web_extension_set_body_input_event_removed (EHTMLEditorWebExtension *extension,
                                                          gboolean value)
{
	extension->priv->body_input_event_removed = value;
}

gboolean
e_html_editor_web_extension_get_convert_in_situ (EHTMLEditorWebExtension *extension)
{
	return extension->priv->convert_in_situ;
}

void
e_html_editor_web_extension_set_convert_in_situ (EHTMLEditorWebExtension *extension,
                                                 gboolean value)
{
	extension->priv->convert_in_situ = value;
}

GHashTable *
e_html_editor_web_extension_get_inline_images (EHTMLEditorWebExtension *extension)
{
	return extension->priv->inline_images;
}

void
e_html_editor_web_extension_add_new_inline_image_into_list (EHTMLEditorWebExtension *extension,
                                                            const gchar *cid_src,
                                                            const gchar *src)
{
	g_hash_table_insert (extension->priv->inline_images, g_strdup (cid_src), g_strdup (src));
}

gboolean
e_html_editor_web_extension_get_is_smiley_written (EHTMLEditorWebExtension *extension)
{
	return extension->priv->smiley_written;
}

void
e_html_editor_web_extension_set_is_smiley_written (EHTMLEditorWebExtension *extension,
                                                   gboolean value)
{
	extension->priv->smiley_written = value;
}

gboolean
e_html_editor_web_extension_get_dont_save_history_in_body_input (EHTMLEditorWebExtension *extension)
{
	return extension->priv->dont_save_history_in_body_input;
}

void
e_html_editor_web_extension_set_dont_save_history_in_body_input (EHTMLEditorWebExtension *extension,
                                                                 gboolean value)
{
	extension->priv->dont_save_history_in_body_input = value;
}

gboolean
e_html_editor_web_extension_is_pasting_content_from_itself (EHTMLEditorWebExtension *extension)
{
	return extension->priv->is_pasting_content_from_itself;
}

gboolean
e_html_editor_web_extension_get_renew_history_after_coordinates (EHTMLEditorWebExtension *extension)
{
	return extension->priv->renew_history_after_coordinates;
}

void
e_html_editor_web_extension_set_renew_history_after_coordinates (EHTMLEditorWebExtension *extension,
								 gboolean renew_history_after_coordinates)
{
	extension->priv->renew_history_after_coordinates = renew_history_after_coordinates;
}

guint64
e_html_editor_web_extension_get_undo_redo_manager_page_id (EHTMLEditorWebExtension *extension,
							   EHTMLEditorUndoRedoManager *manager)
{
	WebKitWebPage *web_page;

	g_return_val_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension), 0);
	g_return_val_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (manager), 0);

	web_page = g_hash_table_lookup (extension->priv->undo_redo_managers, manager);
	g_return_val_if_fail (web_page != NULL, 0);
	g_return_val_if_fail (WEBKIT_IS_WEB_PAGE (web_page), 0);

	return webkit_web_page_get_id (web_page);
}

EHTMLEditorUndoRedoManager *
e_html_editor_web_extension_get_undo_redo_manager (EHTMLEditorWebExtension *extension,
						   WebKitDOMDocument *document)
{
	GHashTableIter iter;
	gpointer key, value;

	g_return_val_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension), NULL);
	g_return_val_if_fail (WEBKIT_DOM_IS_DOCUMENT (document), NULL);

	g_hash_table_iter_init (&iter, extension->priv->undo_redo_managers);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_warn_if_fail (E_IS_HTML_EDITOR_UNDO_REDO_MANAGER (key));
		g_warn_if_fail (WEBKIT_IS_WEB_PAGE (value));

		if (document == webkit_web_page_get_dom_document (value))
			return key;
	}

	g_warn_if_reached ();

	return NULL;
}

gboolean
e_html_editor_web_extension_is_composition_in_progress (EHTMLEditorWebExtension *extension)
{
	return extension->priv->composition_in_progress;
}


void
e_html_editor_web_extension_set_composition_in_progress (EHTMLEditorWebExtension *extension,
                                                         gboolean value)
{
	extension->priv->composition_in_progress = value;
}

guint
e_html_editor_web_extension_get_spell_check_on_scroll_event_source_id (EHTMLEditorWebExtension *extension)
{
	return extension->priv->spell_check_on_scroll_event_source_id;
}

void
e_html_editor_web_extension_set_spell_check_on_scroll_event_source_id (EHTMLEditorWebExtension *extension,
                                                                       guint value)
{
	extension->priv->spell_check_on_scroll_event_source_id = value;
}

void
e_html_editor_web_extension_block_selection_changed_callback (EHTMLEditorWebExtension *extension)
{
	if (!extension->priv->selection_changed_callbacks_blocked)
		extension->priv->selection_changed_callbacks_blocked = TRUE;
}

void
e_html_editor_web_extension_unblock_selection_changed_callback (EHTMLEditorWebExtension *extension)
{
	if (extension->priv->selection_changed_callbacks_blocked)
		extension->priv->selection_changed_callbacks_blocked = FALSE;
}
