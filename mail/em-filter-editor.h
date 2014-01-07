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
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_FILTER_EDITOR_H
#define EM_FILTER_EDITOR_H

#include <e-util/e-util.h>

#include "em-filter-context.h"

/* Standard GObject macros */
#define EM_TYPE_FILTER_EDITOR \
	(em_filter_editor_get_type ())
#define EM_FILTER_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FILTER_EDITOR, EMFilterEditor))
#define EM_FILTER_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FILTER_EDITOR, EMFilterEditorClass))
#define EM_IS_FILTER_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FILTER_EDITOR))
#define EM_IS_FILTER_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FILTER_EDITOR))
#define EM_FILTER_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FILTER_EDITOR, EMFilterEditorClass))

G_BEGIN_DECLS

typedef struct _EMFilterEditor EMFilterEditor;
typedef struct _EMFilterEditorClass EMFilterEditorClass;

typedef struct _EMFilterSource EMFilterSource;

struct _EMFilterSource {
	const gchar *source;
	const gchar *name;
};

struct _EMFilterEditor {
	ERuleEditor parent;
};

struct _EMFilterEditorClass {
	ERuleEditorClass parent_class;
};

GType		em_filter_editor_get_type	(void);
EMFilterEditor *em_filter_editor_new		(EMFilterContext *f,
						 const EMFilterSource *source_names);
void		em_filter_editor_construct	(EMFilterEditor *fe,
						 EMFilterContext *fc,
						 GtkBuilder *builder,
						 const EMFilterSource *source_names);

#endif /* EM_FILTER_EDITOR_H */
