/*
 * e-import-assistant.h
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_IMPORT_ASSISTANT_H
#define E_IMPORT_ASSISTANT_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_IMPORT_ASSISTANT \
	(e_import_assistant_get_type ())
#define E_IMPORT_ASSISTANT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_IMPORT_ASSISTANT, EImportAssistant))
#define E_IMPORT_ASSISTANT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_IMPORT_ASSISTANT, EImportAssistantClass))
#define E_IS_IMPORT_ASSISTANT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_IMPORT_ASSISTANT))
#define E_IS_IMPORT_ASSISTANT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_IMPORT_ASSISTANT))
#define E_IMPORT_ASSISTANT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_IMPORT_ASSISTANT, EImportAssistantClass))

G_BEGIN_DECLS

typedef struct _EImportAssistant EImportAssistant;
typedef struct _EImportAssistantClass EImportAssistantClass;
typedef struct _EImportAssistantPrivate EImportAssistantPrivate;

struct _EImportAssistant {
	GtkAssistant parent;
	EImportAssistantPrivate *priv;
};

struct _EImportAssistantClass {
	GtkAssistantClass parent_class;
};

GType		e_import_assistant_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_import_assistant_new		(GtkWindow *parent);
GtkWidget *	e_import_assistant_new_simple	(GtkWindow *parent,
						 const gchar * const *uris);

G_END_DECLS

#endif /* E_IMPORT_ASSISTANT_H */
