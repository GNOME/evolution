/*
 * e-settings-content-editor.h
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

#ifndef E_SETTINGS_CONTENT_EDITOR_H
#define E_SETTINGS_CONTENT_EDITOR_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_CONTENT_EDITOR \
	(e_settings_content_editor_get_type ())
#define E_SETTINGS_CONTENT_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_CONTENT_EDITOR, ESettingsContentEditor))
#define E_SETTINGS_CONTENT_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_CONTENT_EDITOR, ESettingsContentEditorClass))
#define E_IS_SETTINGS_CONTENT_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_CONTENT_EDITOR))
#define E_IS_SETTINGS_CONTENT_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_CONTENT_EDITOR))
#define E_SETTINGS_CONTENT_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_CONTENT_EDITOR, ESettingsContentEditorClass))

G_BEGIN_DECLS

typedef struct _ESettingsContentEditor ESettingsContentEditor;
typedef struct _ESettingsContentEditorClass ESettingsContentEditorClass;
typedef struct _ESettingsContentEditorPrivate ESettingsContentEditorPrivate;

struct _ESettingsContentEditor {
	EExtension parent;
	ESettingsContentEditorPrivate *priv;
};

struct _ESettingsContentEditorClass {
	EExtensionClass parent_class;
};

GType		e_settings_content_editor_get_type
						(void) G_GNUC_CONST;
void		e_settings_content_editor_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_CONTENT_EDITOR_H */
