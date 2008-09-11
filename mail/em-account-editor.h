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

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

struct _EAccount;

typedef struct _EMAccountEditor EMAccountEditor;
typedef struct _EMAccountEditorClass EMAccountEditorClass;

typedef enum {
	EMAE_NOTEBOOK,
	EMAE_DRUID,
} em_account_editor_t;

struct _EMAccountEditor {
	GObject gobject;

	struct _EMAccountEditorPrivate *priv;

	em_account_editor_t type;
	struct _GtkWidget *editor; /* gtknotebook or druid, depending on type */

	struct _EMConfig *config; /* driver object */

	struct _EAccount *account; /* working account, must instant apply to this */
	struct _EAccount *original; /* original account, not changed unless commit is invoked */

	guint do_signature:1;	/* allow editing signature */
};

struct _EMAccountEditorClass {
	GObjectClass gobject_class;
};

GType em_account_editor_get_type(void);

EMAccountEditor *em_account_editor_new(struct _EAccount *account, em_account_editor_t type, char *id);

gboolean em_account_editor_save (EMAccountEditor *gui);
void em_account_editor_destroy (EMAccountEditor *gui);

gboolean em_account_editor_identity_complete (EMAccountEditor *gui, struct _GtkWidget **incomplete);
gboolean em_account_editor_source_complete (EMAccountEditor *gui, struct _GtkWidget **incomplete);
gboolean em_account_editor_transport_complete (EMAccountEditor *gui, struct _GtkWidget **incomplete);
gboolean em_account_editor_management_complete (EMAccountEditor *gui, struct _GtkWidget **incomplete);

void em_account_editor_build_extra_conf (EMAccountEditor *gui, const char *url);

void em_account_editor_auto_detect_extra_conf (EMAccountEditor *gui);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_ACCOUNT_EDITOR_H */
