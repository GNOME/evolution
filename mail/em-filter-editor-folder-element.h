/*
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
 *
 * Authors:
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeelementrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_FILTER_EDITOR_FOLDER_ELEMENT_H
#define EM_FILTER_EDITOR_FOLDER_ELEMENT_H

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define EM_TYPE_FILTER_EDITOR_FOLDER_ELEMENT \
	(em_filter_editor_folder_element_get_type ())
#define EM_FILTER_EDITOR_FOLDER_ELEMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FILTER_EDITOR_FOLDER_ELEMENT, EMFilterEditorFolderElement))
#define EM_FILTER_EDITOR_FOLDER_ELEMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FILTER_EDITOR_FOLDER_ELEMENT, EMFilterEditorFolderElementClass))
#define EM_IS_FILTER_EDITOR_FOLDER_ELEMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FILTER_EDITOR_FOLDER_ELEMENT))
#define EM_IS_FILTER_EDITOR_FOLDER_ELEMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FILTER_EDITOR_FOLDER_ELEMENT))
#define EM_FILTER_EDITOR_FOLDER_ELEMENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FILTER_EDITOR_FOLDER_ELEMENT, EMFilterEditorFolderElementClass))

G_BEGIN_DECLS

typedef struct _EMFilterEditorFolderElement EMFilterEditorFolderElement;
typedef struct _EMFilterEditorFolderElementClass EMFilterEditorFolderElementClass;
typedef struct _EMFilterEditorFolderElementPrivate EMFilterEditorFolderElementPrivate;

struct _EMFilterEditorFolderElement {
	EMFilterFolderElement parent;
	EMFilterEditorFolderElementPrivate *priv;
};

struct _EMFilterEditorFolderElementClass {
	EMFilterFolderElementClass parent_class;
};

GType		em_filter_editor_folder_element_get_type (void);
EFilterElement *em_filter_editor_folder_element_new	(EMailSession *session);
EMailSession *	em_filter_editor_folder_element_get_session
						(EMFilterEditorFolderElement *element);

G_END_DECLS

#endif /* EM_FILTER_EDITOR_FOLDER_ELEMENT_H */
