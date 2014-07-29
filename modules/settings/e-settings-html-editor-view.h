/*
 * e-settings-html-editor-view.h
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

#ifndef E_SETTINGS_HTML_EDITOR_VIEW_H
#define E_SETTINGS_HTML_EDITOR_VIEW_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_HTML_EDITOR_VIEW \
	(e_settings_html_editor_view_get_type ())
#define E_SETTINGS_HTML_EDITOR_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_HTML_EDITOR_VIEW, ESettingsHTMLEditorView))
#define E_SETTINGS_HTML_EDITOR_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_HTML_EDITOR_VIEW, ESettingsHTMLEditorViewClass))
#define E_IS_SETTINGS_HTML_EDITOR_VIEW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_HTML_EDITOR_VIEW))
#define E_IS_SETTINGS_HTML_EDITOR_VIEW_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_HTML_EDITOR_VIEW))
#define E_SETTINGS_HTML_EDITOR_VIEW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_HTML_EDITOR_VIEW, ESettingsHTMLEditorViewClass))

G_BEGIN_DECLS

typedef struct _ESettingsHTMLEditorView ESettingsHTMLEditorView;
typedef struct _ESettingsHTMLEditorViewClass ESettingsHTMLEditorViewClass;
typedef struct _ESettingsHTMLEditorViewPrivate ESettingsHTMLEditorViewPrivate;

struct _ESettingsHTMLEditorView {
	EExtension parent;
	ESettingsHTMLEditorViewPrivate *priv;
};

struct _ESettingsHTMLEditorViewClass {
	EExtensionClass parent_class;
};

GType		e_settings_html_editor_view_get_type
						(void) G_GNUC_CONST;
void		e_settings_html_editor_view_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_HTML_EDITOR_VIEW_H */
