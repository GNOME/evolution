/*
 * e-shell-importer.h
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_SHELL_IMPORTER_H
#define E_SHELL_IMPORTER_H

#include "e-shell-common.h"

/* Standard GObject macros */
#define E_TYPE_SHELL_IMPORTER \
	(e_shell_importer_get_type ())
#define E_SHELL_IMPORTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SHELL_IMPORTER, EShellImporter))
#define E_SHELL_IMPORTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SHELL_IMPORTER, EShellImporterClass))
#define E_IS_SHELL_IMPORTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SHELL_IMPORTER))
#define E_IS_SHELL_IMPORTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SHELL_IMPORTER))
#define E_SHELL_IMPORTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SHELL_IMPORTER, EShellImporterClass))

G_BEGIN_DECLS

typedef struct _EShellImporter EShellImporter;
typedef struct _EShellImporterClass EShellImporterClass;
typedef struct _EShellImporterPrivate EShellImporterPrivate;

struct _EShellImporter {
	GtkAssistant parent;
	EShellImporterPrivate *priv;
};

struct _EShellImporterClass {
	GtkAssistantClass parent_class;
};

GType		e_shell_importer_get_type	(void);
GtkWidget *	e_shell_importer_new		(GtkWindow *parent);

G_END_DECLS

#endif /* E_SHELL_IMPORTER_H */
