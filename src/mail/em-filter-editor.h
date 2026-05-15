/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Not Zed <notzed@lostzed.mmc.com.au>
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
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
