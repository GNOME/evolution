/*
 * Copyright (C) 2016 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef E_WEBKIT_CONTENT_EDITOR_H
#define E_WEBKIT_CONTENT_EDITOR_H

#include <webkit2/webkit2.h>

/* Standard GObject macros */
#define E_TYPE_WEBKIT_CONTENT_EDITOR \
	(e_webkit_content_editor_get_type ())
#define E_WEBKIT_CONTENT_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEBKIT_CONTENT_EDITOR, EWebKitContentEditor))
#define E_WEBKIT_CONTENT_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEBKIT_CONTENT_EDITOR, EWebKitContentEditorClass))
#define E_IS_WEBKIT_CONTENT_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEBKIT_CONTENT_EDITOR))
#define E_IS_WEBKIT_CONTENT_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEBKIT_CONTENT_EDITOR))
#define E_WEBKIT_CONTENT_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEBKIT_CONTENT_EDITOR, EWebKitContentEditorClass))

G_BEGIN_DECLS

typedef struct _EWebKitContentEditor EWebKitContentEditor;
typedef struct _EWebKitContentEditorClass EWebKitContentEditorClass;
typedef struct _EWebKitContentEditorPrivate EWebKitContentEditorPrivate;

struct _EWebKitContentEditor {
	WebKitWebView parent;
	EWebKitContentEditorPrivate *priv;
};

struct _EWebKitContentEditorClass {
	WebKitWebViewClass parent_class;

	gboolean	(*popup_event)		(EWebKitContentEditor *wk_editor,
						 GdkEventButton *event);
	void		(*paste_primary_clipboard)
						(EWebKitContentEditor *wk_editor);
};

GType		e_webkit_content_editor_get_type 	(void) G_GNUC_CONST;

EWebKitContentEditor *
		e_webkit_content_editor_new		(void);

G_END_DECLS

#endif /* E_WEBKIT_CONTENT_EDITOR_H */
