/*
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
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *		Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef EM_ACCOUNT_EDITOR_H
#define EM_ACCOUNT_EDITOR_H

#include <gtk/gtk.h>

#include <mail/em-config.h>

G_BEGIN_DECLS

typedef struct _EMAccountEditor EMAccountEditor;
typedef struct _EMAccountEditorClass EMAccountEditorClass;
typedef struct _EMAccountEditorPrivate EMAccountEditorPrivate;

typedef enum {
	EMAE_NOTEBOOK,
	EMAE_DRUID,
	EMAE_PAGES
} em_account_editor_t;

struct _EMAccountEditor {
	GObject gobject;

	EMAccountEditorPrivate *priv;

	em_account_editor_t type;
	GtkWidget *editor; /* gtknotebook or druid, depending on type */

	EMConfig *config; /* driver object */

	EAccount *account; /* working account, must instant apply to this */
	EAccount *original; /* original account, not changed unless commit is invoked */

	GtkWidget **pages; /* Pages for Anjal's page type editor */

	guint do_signature:1;	/* allow editing signature */
};

struct _EMAccountEditorClass {
	GObjectClass gobject_class;
};

GType em_account_editor_get_type(void);

EMAccountEditor *em_account_editor_new(EAccount *account, em_account_editor_t type, const gchar *id);
EMAccountEditor *em_account_editor_new_for_pages(EAccount *account, em_account_editor_t type, gchar *id, GtkWidget **pages);
void em_account_editor_commit (EMAccountEditor *emae);
gboolean em_account_editor_check (EMAccountEditor *emae, const gchar *page);

gboolean em_account_editor_save (EMAccountEditor *gui);
void em_account_editor_destroy (EMAccountEditor *gui);

gboolean em_account_editor_identity_complete (EMAccountEditor *gui, GtkWidget **incomplete);
gboolean em_account_editor_source_complete (EMAccountEditor *gui, GtkWidget **incomplete);
gboolean em_account_editor_transport_complete (EMAccountEditor *gui, GtkWidget **incomplete);
gboolean em_account_editor_management_complete (EMAccountEditor *gui, GtkWidget **incomplete);

void em_account_editor_build_extra_conf (EMAccountEditor *gui, const gchar *url);

void em_account_editor_auto_detect_extra_conf (EMAccountEditor *gui);

G_END_DECLS

#endif /* EM_ACCOUNT_EDITOR_H */
