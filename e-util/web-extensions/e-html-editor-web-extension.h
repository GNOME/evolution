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

#ifndef E_HTML_EDITOR_WEB_EXTENSION_H
#define E_HTML_EDITOR_WEB_EXTENSION_H

#include "config.h"
#include "e-util-enums.h"

#include "e-html-editor-web-extension-names.h"

#include <webkit2/webkit-web-extension.h>
#include <glib-object.h>

/* Standard GObject macros */
#define E_TYPE_HTML_EDITOR_WEB_EXTENSION \
	(e_html_editor_web_extension_get_type ())
#define E_HTML_EDITOR_WEB_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_HTML_EDITOR_WEB_EXTENSION, EHTMLEditorWebExtension))
#define E_HTML_EDITOR_WEB_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_HTML_EDITOR_WEB_EXTENSION, EHTMLEditorWebExtensionClass))
#define E_IS_HTML_EDITOR_WEB_EXTENSION(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_HTML_EDITOR_WEB_EXTENSION))
#define E_IS_HTML_EDITOR_WEB_EXTENSION_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_HTML_EDITOR_WEB_EXTENSION))
#define E_HTML_EDITOR_WEB_EXTENSION_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_HTML_EDITOR_WEB_EXTENSION, EHTMLEditorWebExtensionClass))

G_BEGIN_DECLS

typedef struct _EHTMLEditorWebExtension EHTMLEditorWebExtension;
typedef struct _EHTMLEditorWebExtensionClass EHTMLEditorWebExtensionClass;
typedef struct _EHTMLEditorWebExtensionPrivate EHTMLEditorWebExtensionPrivate;

struct _EHTMLEditorWebExtension {
	GObject parent;
	EHTMLEditorWebExtensionPrivate *priv;
};

struct _EHTMLEditorWebExtensionClass
{
	GObjectClass parent_class;
};

GType		e_html_editor_web_extension_get_type
						(void) G_GNUC_CONST;

EHTMLEditorWebExtension *
		e_html_editor_web_extension_get	(void);

void		e_html_editor_web_extension_initialize
						(EHTMLEditorWebExtension *extension,
						WebKitWebExtension *wk_extension);

void		e_html_editor_web_extension_dbus_register
						(EHTMLEditorWebExtension *extension,
						 GDBusConnection *connection);

void		set_dbus_property_boolean	(EHTMLEditorWebExtension *extension,
						 const gchar *name,
						 gboolean value);

void		e_html_editor_web_extension_set_content_changed
						(EHTMLEditorWebExtension *extension);

gboolean	e_html_editor_web_extension_get_html_mode
						(EHTMLEditorWebExtension *extension);

GDBusConnection *
		e_html_editor_web_extension_get_connection
						(EHTMLEditorWebExtension *extension);

gint		e_html_editor_web_extension_get_word_wrap_length
						(EHTMLEditorWebExtension *extension);

const gchar *	e_html_editor_web_extension_get_selection_text
						(EHTMLEditorWebExtension *extension);

gboolean	e_html_editor_web_extension_get_bold
						(EHTMLEditorWebExtension *extension);

gboolean	e_html_editor_web_extension_get_italic
						(EHTMLEditorWebExtension *extension);

gboolean	e_html_editor_web_extension_get_underline
						(EHTMLEditorWebExtension *extension);

gboolean	e_html_editor_web_extension_get_strikethrough
						(EHTMLEditorWebExtension *extension);

gint		e_html_editor_web_extension_get_font_size
						(EHTMLEditorWebExtension *extension);

EHTMLEditorSelectionAlignment
		e_html_editor_web_extension_get_alignment
						(EHTMLEditorWebExtension *extension);

gboolean	e_html_editor_web_extension_is_message_from_edit_as_new
						(EHTMLEditorWebExtension *extension);

gboolean	e_html_editor_web_extension_get_remove_initial_input_line
						(EHTMLEditorWebExtension *extension);

gboolean	e_html_editor_web_extension_get_return_key_pressed
						(EHTMLEditorWebExtension *extension);

void		e_html_editor_web_extension_set_return_key_pressed
						(EHTMLEditorWebExtension *extension,
						 gboolean value);

gboolean	e_html_editor_web_extension_get_space_key_pressed
						(EHTMLEditorWebExtension *extension);

void		e_html_editor_web_extension_set_space_key_pressed
						(EHTMLEditorWebExtension *extension,
						 gboolean value);

#endif /* E_HTML_EDITOR_WEB_EXTENSION_H */
