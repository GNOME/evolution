/*
 * e-html-editor-web-extension.h
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

#include "config.h"

#include "e-html-editor-web-extension.h"

#include <string.h>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <webkit2/webkit-web-extension.h>

#include <camel/camel.h>

#include <e-util/e-misc-utils.h>

#include "e-composer-private-dom-functions.h"
#include "e-dom-utils.h"
#include "e-html-editor-actions-dom-functions.h"
#include "e-html-editor-cell-dialog-dom-functions.h"
#include "e-html-editor-dom-functions.h"
#include "e-html-editor-hrule-dialog-dom-functions.h"
#include "e-html-editor-image-dialog-dom-functions.h"
#include "e-html-editor-link-dialog-dom-functions.h"
#include "e-html-editor-selection-dom-functions.h"
#include "e-html-editor-spell-check-dialog-dom-functions.h"
#include "e-html-editor-table-dialog-dom-functions.h"
#include "e-html-editor-view-dom-functions.h"
#include "e-msg-composer-dom-functions.h"

#define E_HTML_EDITOR_WEB_EXTENSION_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_HTML_EDITOR_WEB_EXTENSION, EHTMLEditorWebExtensionPrivate))

struct _EHTMLEditorWebExtensionPrivate {
	WebKitWebExtension *wk_extension;

	GDBusConnection *dbus_connection;
	guint registration_id;

	/* These properties show the actual state of EHTMLEditorView */
	EHTMLEditorSelectionAlignment alignment;
	EHTMLEditorSelectionBlockFormat block_format;
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
	gboolean changed;
	gboolean inline_spelling;
	gboolean magic_links;
	gboolean magic_smileys;
	gboolean html_mode;
	gboolean return_key_pressed;
	gboolean space_key_pressed;
	gint word_wrap_length;

	gboolean convert_in_situ;
	gboolean body_input_event_removed;
	gboolean is_message_from_draft;
	gboolean is_from_new_message;
	gboolean is_message_from_edit_as_new;
	gboolean is_message_from_selection;
	gboolean remove_initial_input_line;

	GHashTable *inline_images;
};

static CamelDataCache *emd_global_http_cache = NULL;

static const char introspection_xml[] =
"<node>"
"  <interface name='org.gnome.Evolution.WebExtension'>"
"<!-- ********************************************************* -->"
"<!--                       PROPERTIES                          -->"
"<!-- ********************************************************* -->"
"    <property type='b' name='ForceImageLoad' access='readwrite'/>"
"    <property type='b' name='InlineSpelling' access='readwrite'/>"
"    <property type='b' name='MagicLinks' access='readwrite'/>"
"    <property type='b' name='HTMLMode' access='readwrite'/>"
"    <property type='b' name='IsMessageFromEditAsNew' access='readwrite'/>"
"    <property type='b' name='IsMessageFromDraft' access='readwrite'/>"
"    <property type='b' name='IsMessageFromSelection' access='readwrite'/>"
"    <property type='b' name='IsFromNewMessage' access='readwrite'/>"
"    <property type='b' name='RemoveInitialInputLine' access='readwrite'/>"
"<!-- ********************************************************* -->"
"<!-- These properties show the actual state of EHTMLEditorView -->"
"<!-- ********************************************************* -->"
"    <property type='b' name='Alignment' access='readwrite'/>"
"    <property type='s' name='BackgroundColor' access='readwrite'/>"
"    <property type='u' name='BlockFormat' access='readwrite'/>"
"    <property type='b' name='Bold' access='readwrite'/>"
"    <property type='s' name='FontColor' access='readwrite'/>"
"    <property type='s' name='FontName' access='readwrite'/>"
"    <property type='u' name='FontSize' access='readwrite'/>"
"    <property type='b' name='Indented' access='readwrite'/>"
"    <property type='b' name='Italic' access='readwrite'/>"
"    <property type='b' name='Monospaced' access='readwrite'/>"
"    <property type='b' name='Strikethrough' access='readwrite'/>"
"    <property type='b' name='Subscript' access='readwrite'/>"
"    <property type='b' name='Superscript' access='readwrite'/>"
"    <property type='b' name='Underline' access='readwrite'/>"
"    <property type='s' name='Text' access='readwrite'/>"
"<!-- ********************************************************* -->"
"<!--                          METHODS                          -->"
"<!-- ********************************************************* -->"
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
"    <method name='EHTMLEditorCellDialogSetElementVAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementAlign'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementNoWrap'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementHeaderStyle'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementWidth'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementColSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementRowSpan'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"    <method name='EHTMLEditorCellDialogSetElementBgColor'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='value' direction='in'/>"
"      <arg type='u' name='scope' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorHRuleDialog      -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorHRuleDialogFindHRule'>"
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
"<!-- ********************************************************* -->"
"<!--   Functions that are used in EHTMLEditorSpellCheckDialog  -->"
"<!-- ********************************************************* -->"
"    <method name='EHTMLEditorSpellCheckDialogNext'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='word' direction='in'/>"
"      <arg type='s' name='next_word' direction='out'/>"
"    </method>"
"    <method name='EHTMLEditorSpellCheckDialogPrev'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='word' direction='in'/>"
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
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorView            -->"
"<!-- ********************************************************* -->"
"    <method name='DOMForceSpellCheck'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMTurnSpellCheckOff'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMCheckMagicLinks'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMScrollToCaret'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMEmbedStyleSheet'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='style_sheet_content' direction='in'/>"
"    </method>"
"    <method name='DOMRemoveEmbedStyleSheet'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMSaveSelection'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMRestoreSelection'>"
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
"    <method name='DOMProcessOnKeyPress'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='u' name='key_val' direction='in'/>"
"      <arg type='b' name='stop_handlers' direction='out'/>"
"    </method>"
"    <method name='DOMCheckIfConversionNeeded'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='conversion_needed' direction='out'/>"
"    </method>"
"    <method name='DOMConvertWhenChangingComposerMode'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMProcessContentAfterModeChange'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMProcessContentForHTML'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='from_domain' direction='in'/>"
"      <arg type='s' name='content' direction='out'/>"
"    </method>"
"    <method name='DOMProcessContentForDraft'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='content' direction='out'/>"
"    </method>"
"    <method name='DOMProcessContentForPlainText'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='content' direction='out'/>"
"    </method>"
"    <method name='DOMInsertHTML'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='html' direction='in'/>"
"    </method>"
"    <method name='DOMConvertContent'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='preffered_text' direction='in'/>"
"    </method>"
"    <method name='DOMGetInlineImagesData'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='uid_domain' direction='in'/>"
"      <arg type='a*' name='image_data' direction='out'/>"
"    </method>"
"    <method name='DOMAddNewInlineImageIntoList'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='filename' direction='in'/>"
"      <arg type='s' name='cid_src' direction='in'/>"
"      <arg type='s' name='src' direction='in'/>"
"    </method>"
"    <method name='DOMReplaceBase64ImageSrc'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='s' name='selector' direction='in'/>"
"      <arg type='s' name='base64_content' direction='in'/>"
"      <arg type='s' name='filename' direction='in'/>"
"      <arg type='s' name='uri' direction='in'/>"
"    </method>"
"    <method name='DOMDragAndDropEnd'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMMoveSelectionOnPoint'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='i' name='x' direction='in'/>"
"      <arg type='i' name='y' direction='in'/>"
"    </method>"
"<!-- ********************************************************* -->"
"<!--     Functions that are used in EHTMLEditorSelection       -->"
"<!-- ********************************************************* -->"
"    <method name='DOMSelectionIndent'>"
"      <arg type='t' name='page_id' direction='in'/>"
"    </method>"
"    <method name='DOMSelectionUnindent'>"
"      <arg type='t' name='page_id' direction='in'/>"
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
"      <arg type='s' name='signature_html' direction='in'/>"
"      <arg type='b' name='top_signature' direction='in'/>"
"      <arg type='b' name='start_bottom' direction='in'/>"
"    </method>"
"    <method name='DOMCleanAfterDragAndDrop'>"
"      <arg type='t' name='page_id' direction='in'/>"
"      <arg type='b' name='remove_inserted_uri_on_drop' direction='in'/>"
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
			"Invalid page ID: %"G_GUINT64_FORMAT, page_id);
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

	if (g_strcmp0 (method_name, "ElementHasAttribute") == 0) {
		gboolean value = FALSE;
		const gchar *element_id, *attribute;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &attribute);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

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
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_element_get_attribute (element, attribute);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "ElementGetAttributeBySelector") == 0) {
		const gchar *attribute, *selector;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &selector, &attribute);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_query_selector (document, selector, NULL);
		if (element)
			value = webkit_dom_element_get_attribute (element, attribute);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "ElementRemoveAttribute") == 0) {
		const gchar *element_id, *attribute;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s&s)", &page_id, &element_id, &attribute);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

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
			return;

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
			return;

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
			parameters, "(t&s&s)", &page_id, &selector, &attribute, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_query_selector (document, selector, NULL);
		if (element)
			webkit_dom_element_set_attribute (
				element, attribute, value, NULL);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "ElementGetTagName") == 0) {
		const gchar *element_id;
		gchar *value = NULL;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_element_get_tag_name (element);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "RemoveImageAttributesFromElementBySelector") == 0) {
		const gchar *selector;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &selector);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

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
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_mark_current_cell_element (document, element_id);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementVAlign") == 0) {
		const gchar *value;
		guint scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_v_align (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementAlign") == 0) {
		const gchar *value;
		guint scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_align (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementNoWrap") == 0) {
		gboolean value;
		guint scope;

		g_variant_get (parameters, "(tbu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_no_wrap (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementHeaderStyle") == 0) {
		gboolean value;
		guint scope;

		g_variant_get (parameters, "(tbu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_header_style (
			document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementWidth") == 0) {
		const gchar *value;
		guint scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_width (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementColSpan") == 0) {
		glong value;
		guint scope;

		g_variant_get (parameters, "(tiu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_col_span (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementRowSpan") == 0) {
		glong value;
		guint scope;

		g_variant_get (parameters, "(tiu)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_row_span (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorCellDialogSetElementBgColor") == 0) {
		const gchar *value;
		guint scope;

		g_variant_get (parameters, "(t&su)", &page_id, &value, &scope);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_cell_dialog_set_element_bg_color (document, value, scope);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorHRuleDialogFindHRule") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_hrule_dialog_find_hrule (document);

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
			return;

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
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_hr_element_get_no_shade (
				WEBKIT_DOM_HTML_HR_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_boolean (value));
	} else if (g_strcmp0 (method_name, "EHTMLEditorImageDialogSetElementUrl") == 0) {
		const gchar *value;

		g_variant_get (parameters, "(t&s)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_image_dialog_set_element_url (document, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorImageDialogGetElementUrl") == 0) {
		gchar *value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_image_dialog_get_element_url (document);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "ImageElementSetWidth") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

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
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "ImageElementSetHeight") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

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
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_height (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "ImageElementGetNaturalWidth") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_natural_width (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "ImageElementGetNaturalHeight") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_natural_height (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "ImageElementSetHSpace") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

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
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_hspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "ImageElementSetVSpace") == 0) {
		const gchar *element_id;
		glong value;
		WebKitDOMElement *element;

		g_variant_get (
			parameters, "(t&si)", &page_id, &element_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

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
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_image_element_get_vspace (
				WEBKIT_DOM_HTML_IMAGE_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "EHTMLEditorLinkDialogOk") == 0) {
		const gchar *url, *inner_text;

		g_variant_get (parameters, "(t&s&s)", &page_id, &url, &inner_text);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_link_dialog_ok (document, url, inner_text);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorSpellCheckDialogNext") == 0) {
		const gchar *word;
		gchar *value = NULL;

		g_variant_get (parameters, "(t&s)", &page_id, &word);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_spell_check_dialog_next (document, word);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorSpellCheckDialogPrev") == 0) {
		const gchar *word;
		gchar *value = NULL;

		g_variant_get (parameters, "(t&s)", &page_id, &word);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_spell_check_dialog_prev (document, word);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogSetRowCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(tu)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_table_dialog_set_row_count (document, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogGetRowCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_table_dialog_get_row_count (document);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_uint32 (value));
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogSetColumnCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(tu)", &page_id, &value);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		e_html_editor_table_dialog_set_column_count (document, value);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogGetColumnCount") == 0) {
		gulong value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = e_html_editor_table_dialog_get_column_count (document);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_uint32 (value));
	} else if (g_strcmp0 (method_name, "EHTMLEditorTableDialogShow") == 0) {
		gboolean created_new_table;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		created_new_table = e_html_editor_table_dialog_show (document);

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_boolean (created_new_table));
	} else if (g_strcmp0 (method_name, "TableCellElementGetNoWrap") == 0) {
		const gchar *element_id;
		gboolean value = FALSE;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_no_wrap (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_boolean (value));
	} else if (g_strcmp0 (method_name, "TableCellElementGetRowSpan") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_row_span (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "TableCellElementGetColSpan") == 0) {
		const gchar *element_id;
		glong value = 0;
		WebKitDOMElement *element;

		g_variant_get (parameters, "(t&s)", &page_id, &element_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		element = webkit_dom_document_get_element_by_id (document, element_id);
		if (element)
			value = webkit_dom_html_table_cell_element_get_col_span (
				WEBKIT_DOM_HTML_TABLE_CELL_ELEMENT (element));

		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_int32 (value));
	} else if (g_strcmp0 (method_name, "DOMSaveSelection") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_save (document);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMRestoreSelection") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_restore (document);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMTurnSpellCheckOff") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_turn_spell_check_off (document);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMQuoteAndInsertTextIntoSelection") == 0) {
		const gchar *text;

		g_variant_get (parameters, "(t&s)", &page_id, &text);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

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
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_convert_and_insert_html_into_selection (document, extension, text, is_html);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMEmbedStyleSheet") == 0) {
		const gchar *style_sheet_content;

		g_variant_get (parameters, "(t&s)", &page_id, &style_sheet_content);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_embed_style_sheet (document, style_sheet_content);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMRemoveEmbedStyleSheet") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_remove_embed_style_sheet (document);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMForceSpellCheck") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_force_spell_check (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMCheckMagicLinks") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_check_magic_links (document, extension, FALSE);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMProcessOnKeyPress") == 0) {
		gboolean stop_handlers;
		guint key_val;

		g_variant_get (parameters, "(tu)", &page_id, &key_val);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		stop_handlers = dom_process_on_key_press (document, extension, key_val);
		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_boolean (stop_handlers));
	} else if (g_strcmp0 (method_name, "DOMCheckIfConversionNeeded") == 0) {
		gboolean conversion_needed;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		conversion_needed = dom_check_if_conversion_needed (document);
		g_dbus_method_invocation_return_value (
			invocation, g_variant_new_boolean (conversion_needed));
	} else if (g_strcmp0 (method_name, "DOMConvertWhenChangingComposerMode") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_convert_when_changing_composer_mode (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMProcessContentAfterModeChange") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_process_content_after_mode_change (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMProcessContentForDraft") == 0) {
		gchar *value = NULL;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = dom_process_content_for_draft (document);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "DOMProcessContentForHTML") == 0) {
		const gchar *from_domain;
		gchar *value = NULL;

		g_variant_get (parameters, "(t&s)", &page_id, &from_domain);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = dom_process_content_for_html (document, extension, from_domain);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "DOMProcessContentForPlainText") == 0) {
		gchar *value = NULL;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = dom_process_content_for_plain_text (document, extension);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "DOMInsertHTML") == 0) {
		const gchar *html;

		g_variant_get (parameters, "(t&s)", &page_id, &html);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_insert_html (document, extension, html);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMConvertContent") == 0) {
		const gchar *preferred_text;

		g_variant_get (parameters, "(t&s)", &page_id, &preferred_text);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_convert_content (document, extension, preferred_text);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMGetInlineImagesData") == 0) {
		const gchar *uid_domain;
		GVariant *images_data;

		g_variant_get (parameters, "(t&s)", &page_id, &uid_domain);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		images_data = dom_get_inline_images_data (document, extension, uid_domain);
		g_dbus_method_invocation_return_value (invocation, images_data);
	} else if (g_strcmp0 (method_name, "DOMAddNewInlineImageIntoList") == 0) {
		const gchar *cid_uri, *src, *filename;

		g_variant_get (parameters, "(t&s&s&s)", &page_id, &filename, &cid_uri, &src);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		e_html_editor_web_extension_add_new_inline_image_into_list (
			extension, cid_uri, src);

		document = webkit_web_page_get_dom_document (web_page);
		dom_insert_base64_image (document, extension, filename, cid_uri, src);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMReplaceBase64ImageSrc") == 0) {
		const gchar *selector, *base64_content, *filename, *uri;

		g_variant_get (parameters, "(t&s&s)", &page_id, &selector, &base64_content, &filename, &uri);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_replace_base64_image_src (document, selector, base64_content, filename, uri);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMDragAndDropEnd") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_drag_and_drop_end (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMMoveSelectionOnPoint") == 0) {
		gint x, y;

		g_variant_get (parameters, "(tii)", &page_id, &x, &y);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_set_on_point (document, x, y);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionIndent") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_indent (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMSelectionUnindent") == 0) {
		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_selection_unindent (document, extension);
		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMRemoveSignatures") == 0) {
		gboolean top_signature;
		gchar *active_signature;

		g_variant_get (parameters, "(tb)", &page_id, &top_signature);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		active_signature = dom_remove_signatures (document, extension, top_signature);

		g_dbus_method_invocation_return_value (
			invocation,
			active_signature ? g_variant_new_take_string (active_signature) : NULL);
	} else if (g_strcmp0 (method_name, "DOMInsertSignature") == 0) {
		gboolean top_signature, start_bottom;
		const gchar *signature_html;

		g_variant_get (
			parameters, "(t&sbb)", &page_id, &signature_html, &top_signature, &start_bottom);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_insert_signature (document, extension, signature_html, top_signature, start_bottom);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMCleanAfterDragAndDrop") == 0) {
		g_variant_get (
			parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		dom_clean_after_drag_and_drop (document, extension);

		g_dbus_method_invocation_return_value (invocation, NULL);
	} else if (g_strcmp0 (method_name, "DOMGetActiveSignatureUid") == 0) {
		gchar *value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = dom_get_active_signature_uid (document);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "DOMGetRawBodyContentWithoutSignature") == 0) {
		gchar *value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = dom_get_raw_body_content_without_signature (document);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	} else if (g_strcmp0 (method_name, "DOMGetRawBodyContent") == 0) {
		gchar *value;

		g_variant_get (parameters, "(t)", &page_id);

		web_page = get_webkit_web_page_or_return_dbus_error (
			invocation, web_extension, page_id);
		if (!web_page)
			return;

		document = webkit_web_page_get_dom_document (web_page);
		value = dom_get_raw_body_content (document);

		g_dbus_method_invocation_return_value (
			invocation,
			value ? g_variant_new_take_string (value) : NULL);
	}
}

static GVariant *
handle_get_property (GDBusConnection *connection,
                     const gchar *sender,
                     const gchar *object_path,
                     const gchar *interface_name,
                     const gchar *property_name,
                     GError **error,
                     gpointer user_data)
{
	EHTMLEditorWebExtension *extension = E_HTML_EDITOR_WEB_EXTENSION (user_data);
	GVariant *variant;

	if (g_strcmp0 (property_name, "ForceImageLoad") == 0)
		variant = g_variant_new_boolean (extension->priv->force_image_load);
	else if (g_strcmp0 (property_name, "InlineSpelling") == 0)
		variant = g_variant_new_boolean (extension->priv->inline_spelling);
	else if (g_strcmp0 (property_name, "MagicLinks") == 0)
		variant = g_variant_new_boolean (extension->priv->magic_links);
	else if (g_strcmp0 (property_name, "HTMLMode") == 0)
		variant = g_variant_new_boolean (extension->priv->html_mode);
	else if (g_strcmp0 (property_name, "IsFromNewMessage") == 0)
		variant = g_variant_new_boolean (extension->priv->is_from_new_message);
	else if (g_strcmp0 (property_name, "IsMessageFromEditAsNew") == 0)
		variant = g_variant_new_boolean (extension->priv->is_message_from_edit_as_new);
	else if (g_strcmp0 (property_name, "IsMessageFromDraft") == 0)
		variant = g_variant_new_boolean (extension->priv->is_message_from_draft);
	else if (g_strcmp0 (property_name, "IsMessageFromSelection") == 0)
		variant = g_variant_new_boolean (extension->priv->is_message_from_selection);
	else if (g_strcmp0 (property_name, "RemoveInitialInputLine") == 0)
		variant = g_variant_new_boolean (extension->priv->remove_initial_input_line);
	else if (g_strcmp0 (property_name, "Alignment") == 0)
		variant = g_variant_new_int32 (extension->priv->alignment);
	else if (g_strcmp0 (property_name, "BackgroundColor") == 0)
		variant = g_variant_new_string (extension->priv->background_color);
	else if (g_strcmp0 (property_name, "BlockFormat") == 0)
		variant = g_variant_new_int32 (extension->priv->block_format);
	else if (g_strcmp0 (property_name, "Bold") == 0)
		variant = g_variant_new_boolean (extension->priv->bold);
	else if (g_strcmp0 (property_name, "FontColor") == 0)
		variant = g_variant_new_string (extension->priv->font_color);
	else if (g_strcmp0 (property_name, "FontName") == 0)
		variant = g_variant_new_string (extension->priv->font_name);
	else if (g_strcmp0 (property_name, "FontSize") == 0)
		variant = g_variant_new_int32 (extension->priv->font_size);
	else if (g_strcmp0 (property_name, "Indented") == 0)
		variant = g_variant_new_boolean (extension->priv->indented);
	else if (g_strcmp0 (property_name, "Italic") == 0)
		variant = g_variant_new_boolean (extension->priv->italic);
	else if (g_strcmp0 (property_name, "Monospaced") == 0)
		variant = g_variant_new_boolean (extension->priv->monospaced);
	else if (g_strcmp0 (property_name, "Strikethrough") == 0)
		variant = g_variant_new_boolean (extension->priv->strikethrough);
	else if (g_strcmp0 (property_name, "Subscript") == 0)
		variant = g_variant_new_boolean (extension->priv->subscript);
	else if (g_strcmp0 (property_name, "Superscript") == 0)
		variant = g_variant_new_boolean (extension->priv->superscript);
	else if (g_strcmp0 (property_name, "Superscript") == 0)
		variant = g_variant_new_boolean (extension->priv->superscript);
	else if (g_strcmp0 (property_name, "Underline") == 0)
		variant = g_variant_new_boolean (extension->priv->underline);
	else if (g_strcmp0 (property_name, "Text") == 0)
		variant = g_variant_new_string (extension->priv->text);

	return variant;
}

static gboolean
handle_set_property (GDBusConnection *connection,
                     const gchar *sender,
                     const gchar *object_path,
                     const gchar *interface_name,
                     const gchar *property_name,
                     GVariant *variant,
                     GError **error,
                     gpointer user_data)
{
	EHTMLEditorWebExtension *extension = E_HTML_EDITOR_WEB_EXTENSION (user_data);
	GError *local_error = NULL;
	GVariantBuilder *builder;

	builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);

	if (g_strcmp0 (property_name, "ForceImageLoad") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->force_image_load)
			goto exit;

		extension->priv->force_image_load = value;

		g_variant_builder_add (builder,
			"{sv}",
			"ForceImageLoad",
			g_variant_new_boolean (extension->priv->force_image_load));
	} else if (g_strcmp0 (property_name, "IsMessageFromDraft") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->is_message_from_draft)
			goto exit;

		extension->priv->is_message_from_draft = value;

		g_variant_builder_add (builder,
			"{sv}",
			"IsMessageFromDraft",
			g_variant_new_boolean (extension->priv->is_message_from_draft));
	} else if (g_strcmp0 (property_name, "RemoveInitialInputLine") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->remove_initial_input_line)
			goto exit;

		extension->priv->remove_initial_input_line = value;

		g_variant_builder_add (builder,
			"{sv}",
			"RemoveInitialInputLine",
			g_variant_new_boolean (extension->priv->remove_initial_input_line));
	} else if (g_strcmp0 (property_name, "IsMessageFromSelection") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->is_message_from_selection)
			goto exit;

		extension->priv->is_message_from_selection = value;

		g_variant_builder_add (builder,
			"{sv}",
			"IsMessageFromSelection",
			g_variant_new_boolean (extension->priv->is_message_from_selection));
	} else if (g_strcmp0 (property_name, "IsFromNewMessage") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->is_from_new_message)
			goto exit;

		extension->priv->is_from_new_message = value;

		g_variant_builder_add (builder,
			"{sv}",
			"IsFromNewMessage",
			g_variant_new_boolean (extension->priv->is_from_new_message));
	} else if (g_strcmp0 (property_name, "IsMessageFromEditAsNew") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->is_message_from_edit_as_new)
			goto exit;

		extension->priv->is_message_from_edit_as_new = value;

		g_variant_builder_add (builder,
			"{sv}",
			"IsMessageFromEditAsNew",
			g_variant_new_boolean (extension->priv->is_message_from_edit_as_new));
	} else if (g_strcmp0 (property_name, "HTMLMode") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->html_mode)
			goto exit;

		extension->priv->html_mode = value;

		g_variant_builder_add (builder,
			"{sv}",
			"HTMLMode",
			g_variant_new_boolean (extension->priv->html_mode));
	} else if (g_strcmp0 (property_name, "MagicLinks") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->magic_links)
			goto exit;

		extension->priv->magic_links = value;

		g_variant_builder_add (builder,
			"{sv}",
			"MagicLinks",
			g_variant_new_boolean (extension->priv->magic_links));
	} else if (g_strcmp0 (property_name, "InlineSpelling") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->inline_spelling)
			goto exit;

		extension->priv->inline_spelling = value;

		g_variant_builder_add (builder,
			"{sv}",
			"InlineSpelling",
			g_variant_new_boolean (extension->priv->inline_spelling));
	} else if (g_strcmp0 (property_name, "Alignment") == 0) {
		gint32 value = g_variant_get_int32 (variant);

		if (value == extension->priv->alignment)
			goto exit;

		extension->priv->alignment = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Alignment",
			g_variant_new_int32 (extension->priv->alignment));
	} else if (g_strcmp0 (property_name, "BackgroundColor") == 0) {
		const gchar *value = g_variant_get_string (variant, NULL);

		if (g_strcmp0 (value, extension->priv->background_color) != 0)
			goto exit;

		g_free (extension->priv->background_color);
		extension->priv->background_color = g_strdup (value);

		g_variant_builder_add (builder,
			"{sv}",
			"BackgroundColor",
			g_variant_new_string (extension->priv->background_color));
	} else if (g_strcmp0 (property_name, "BlockFormat") == 0) {
		gint32 value = g_variant_get_int32 (variant);

		if (value == extension->priv->block_format)
			goto exit;

		extension->priv->block_format = value;

		g_variant_builder_add (builder,
			"{sv}",
			"BlockFormat",
			g_variant_new_int32 (extension->priv->block_format));
	} else if (g_strcmp0 (property_name, "Bold") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->bold)
			goto exit;

		extension->priv->bold = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Bold",
			g_variant_new_boolean (extension->priv->bold));
	} else if (g_strcmp0 (property_name, "FontColor") == 0) {
		const gchar *value = g_variant_get_string (variant, NULL);

		if (g_strcmp0 (value, extension->priv->font_color) != 0)
			goto exit;

		g_free (extension->priv->font_color);
		extension->priv->font_color = g_strdup (value);

		g_variant_builder_add (builder,
			"{sv}",
			"FontColor",
			g_variant_new_string (extension->priv->font_color));
	} else if (g_strcmp0 (property_name, "FontName") == 0) {
		const gchar *value = g_variant_get_string (variant, NULL);

		if (g_strcmp0 (value, extension->priv->font_name) != 0)
			goto exit;

		g_free (extension->priv->font_name);
		extension->priv->font_name = g_strdup (value);

		g_variant_builder_add (builder,
			"{sv}",
			"FontName",
			g_variant_new_string (extension->priv->font_name));
	} else if (g_strcmp0 (property_name, "FontSize") == 0) {
		gint32 value = g_variant_get_int32 (variant);

		if (value == extension->priv->font_size)
			goto exit;

		extension->priv->font_size = value;

		g_variant_builder_add (builder,
			"{sv}",
			"FontSize",
			g_variant_new_int32 (extension->priv->font_size));
	} else if (g_strcmp0 (property_name, "Indented") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->indented)
			goto exit;

		extension->priv->indented = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Indented",
			g_variant_new_boolean (extension->priv->indented));
	} else if (g_strcmp0 (property_name, "Italic") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->italic)
			goto exit;

		extension->priv->italic = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Italic",
			g_variant_new_boolean (extension->priv->italic));
	} else if (g_strcmp0 (property_name, "Monospaced") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->monospaced)
			goto exit;

		extension->priv->monospaced = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Monospaced",
			g_variant_new_boolean (extension->priv->monospaced));
	} else if (g_strcmp0 (property_name, "Strikethrough") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->strikethrough)
			goto exit;

		extension->priv->strikethrough = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Strikethrough",
			g_variant_new_boolean (extension->priv->strikethrough));
	} else if (g_strcmp0 (property_name, "Subscript") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->subscript)
			goto exit;

		extension->priv->subscript = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Subscript",
			g_variant_new_boolean (extension->priv->subscript));
	} else if (g_strcmp0 (property_name, "Superscript") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->superscript)
			goto exit;

		extension->priv->superscript = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Superscript",
			g_variant_new_boolean (extension->priv->superscript));
	} else if (g_strcmp0 (property_name, "Underline") == 0) {
		gboolean value = g_variant_get_boolean (variant);

		if (value == extension->priv->underline)
			goto exit;

		extension->priv->underline = value;

		g_variant_builder_add (builder,
			"{sv}",
			"Undeline",
			g_variant_new_boolean (extension->priv->underline));
	} else if (g_strcmp0 (property_name, "Text") == 0) {
		const gchar *value = g_variant_get_string (variant, NULL);

		if (g_strcmp0 (value, extension->priv->text) != 0)
			goto exit;

		g_free (extension->priv->text);
		extension->priv->text = g_strdup (value);

		g_variant_builder_add (builder,
			"{sv}",
			"Text",
			g_variant_new_string (extension->priv->text));
	}

	g_dbus_connection_emit_signal (connection,
		NULL,
		object_path,
		"org.freedesktop.DBus.Properties",
		"PropertiesChanged",
		g_variant_new (
			"(sa{sv}as)",
			interface_name,
			builder,
			NULL),
		&local_error);

	g_assert_no_error (local_error);

 exit:
	g_variant_builder_unref (builder);

	return TRUE;
}

static const GDBusInterfaceVTable interface_vtable = {
	handle_method_call,
	handle_get_property,
	handle_set_property
};

static void
e_html_editor_web_extension_dispose (GObject *object)
{
	EHTMLEditorWebExtension *extension = E_HTML_EDITOR_WEB_EXTENSION (object);

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

	g_clear_object (&extension->priv->wk_extension);

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
	extension->priv = G_TYPE_INSTANCE_GET_PRIVATE (extension, E_TYPE_HTML_EDITOR_WEB_EXTENSION, EHTMLEditorWebExtensionPrivate);

	extension->priv->bold = FALSE;
	extension->priv->background_color = NULL;
	extension->priv->font_color = NULL;
	extension->priv->font_name = NULL;
	extension->priv->text = NULL;
	extension->priv->font_size = E_HTML_EDITOR_SELECTION_FONT_SIZE_NORMAL;
	extension->priv->indented = FALSE;
	extension->priv->italic = FALSE;
	extension->priv->monospaced = FALSE;
	extension->priv->strikethrough = FALSE;
	extension->priv->subscript = FALSE;
	extension->priv->superscript = FALSE;
	extension->priv->underline = FALSE;
	extension->priv->alignment = E_HTML_EDITOR_SELECTION_ALIGNMENT_LEFT;
	extension->priv->block_format = E_HTML_EDITOR_SELECTION_BLOCK_FORMAT_PARAGRAPH;
	extension->priv->changed = FALSE;
	extension->priv->force_image_load = FALSE;
	extension->priv->inline_spelling = FALSE;
	extension->priv->magic_links = FALSE;
	extension->priv->magic_smileys = FALSE;
	extension->priv->html_mode = FALSE;
	extension->priv->return_key_pressed = FALSE;
	extension->priv->space_key_pressed = FALSE;
	extension->priv->body_input_event_removed = FALSE;
	extension->priv->word_wrap_length = 71;

	extension->priv->convert_in_situ = FALSE;
	extension->priv->body_input_event_removed = FALSE;
	extension->priv->is_message_from_draft = FALSE;
	extension->priv->is_message_from_edit_as_new = FALSE;
	extension->priv->is_from_new_message = FALSE;
	extension->priv->is_message_from_selection = FALSE;
	extension->priv->remove_initial_input_line = FALSE;

	extension->priv->inline_images = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);
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

	g_return_val_if_fail (emd_global_http_cache != NULL, FALSE);

	hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, image_uri, -1);
	filename = camel_data_cache_get_filename (
		emd_global_http_cache, "http", hash);

	if (filename != NULL) {
		exists = g_file_test (filename, G_FILE_TEST_EXISTS);
		g_free (filename);
	}

	g_free (hash);

	return exists;
}

static EImageLoadingPolicy
get_image_loading_policy (void)
{
	GSettings *settings;
	EImageLoadingPolicy image_policy;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	image_policy = g_settings_get_enum (settings, "image-loading-policy");
	g_object_unref (settings);

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
	image_policy = get_image_loading_policy ();
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

	document = webkit_web_page_get_dom_document (web_page);

	dom_process_content_after_load (document, web_extension);
}

static void
web_page_created_cb (WebKitWebExtension *wk_extension,
                     WebKitWebPage *web_page,
                     EHTMLEditorWebExtension *extension)
{
	g_signal_connect_object (
		web_page, "send-request",
		G_CALLBACK (web_page_send_request_cb),
		extension, 0);

	g_signal_connect_object (
		web_page, "document-loaded",
		G_CALLBACK (web_page_document_loaded_cb),
		extension, 0);
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

		/* cache expiry - 2 hour access, 1 day max */
		camel_data_cache_set_expire_age (
			emd_global_http_cache, 24 * 60 * 60);
		camel_data_cache_set_expire_access (
			emd_global_http_cache, 2 * 60 * 60);
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
				(gpointer *)&extension->priv->dbus_connection);
		}
	}
}

void
set_dbus_property_boolean (EHTMLEditorWebExtension *extension,
                           const gchar *name,
                           gboolean value)
{
	g_dbus_connection_call (
		extension->priv->dbus_connection,
		E_HTML_EDITOR_WEB_EXTENSION_SERVICE_NAME,
		E_HTML_EDITOR_WEB_EXTENSION_OBJECT_PATH,
		"org.freedesktop.DBus.Properties",
		"Set",
		g_variant_new (
			"(ssv)",
			E_HTML_EDITOR_WEB_EXTENSION_INTERFACE,
			name,
			g_variant_new_boolean (value)),
		NULL,
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		NULL,
		NULL);
}

void
e_html_editor_web_extension_set_content_changed (EHTMLEditorWebExtension *extension)
{
	set_dbus_property_boolean (extension, "Changed", TRUE);
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
e_html_editor_web_extension_get_strikethrough (EHTMLEditorWebExtension *extension)
{
	return extension->priv->strikethrough;
}

gint
e_html_editor_web_extension_get_font_size (EHTMLEditorWebExtension *extension)
{
	return extension->priv->font_size;
}

EHTMLEditorSelectionAlignment
e_html_editor_web_extension_get_alignment (EHTMLEditorWebExtension *extension)
{
	return extension->priv->alignment;
}

gboolean
e_html_editor_web_extension_is_message_from_edit_as_new (EHTMLEditorWebExtension *extension)
{
	return extension->priv->is_message_from_edit_as_new;
}

gboolean
e_html_editor_web_extension_get_remove_initial_input_line (EHTMLEditorWebExtension *extension)
{
	return extension->priv->remove_initial_input_line;
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
e_html_editor_web_extension_get_magic_smileys_enabled (EHTMLEditorWebExtension *extension)
{
	return extension->priv->magic_smileys;
}

void
e_html_editor_web_extension_set_inline_spelling (EHTMLEditorWebExtension *extension,
                                                 gboolean value)
{
	g_return_if_fail (E_IS_HTML_EDITOR_WEB_EXTENSION (extension));

	if (extension->priv->inline_spelling == value)
		return;

	extension->priv->inline_spelling = value;
/* FIXME WK2
	if (inline_spelling)
		e_html_editor_view_force_spell_check (view);
	else
		e_html_editor_view_turn_spell_check_off (view);*/
}

gboolean
e_html_editor_web_extension_get_inline_spelling_enabled (EHTMLEditorWebExtension *extension)
{
	return extension->priv->inline_spelling;
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
	g_hash_table_insert (extension->priv->inline_images, g_strdup(cid_src), g_strdup(src));
}

gboolean
e_html_editor_web_extension_is_message_from_draft (EHTMLEditorWebExtension *extension)
{
	return extension->priv->is_message_from_draft;
}

gboolean
e_html_editor_web_extension_is_from_new_message (EHTMLEditorWebExtension *extension)
{
	return extension->priv->is_from_new_message;
}

