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

/* Standard GObject macros */
#define EM_TYPE_ACCOUNT_EDITOR \
	(em_account_editor_get_type ())
#define EM_ACCOUNT_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_ACCOUNT_EDITOR, EMAccountEditor))
#define EM_ACCOUNT_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_ACCOUNT_EDITOR, EMAccountEditorClass))
#define EM_IS_ACCOUNT_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_ACCOUNT_EDITOR))
#define EM_IS_ACCOUNT_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_ACCOUNT_EDITOR))
#define EM_ACCOUNT_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_ACCOUNT_EDITOR, EMAccountEditorClass))

G_BEGIN_DECLS

typedef struct _EMAccountEditor EMAccountEditor;
typedef struct _EMAccountEditorClass EMAccountEditorClass;
typedef struct _EMAccountEditorPrivate EMAccountEditorPrivate;

typedef struct _server_data ServerData;
struct _server_data {
	const gchar *key;
	const gchar *recv;
	const gchar *send;
	const gchar *proto;
	const gchar *ssl;
	const gchar *send_user;
	const gchar *recv_user;
	const gchar *send_port;
	const gchar *recv_port;
};

typedef enum {
	EMAE_NOTEBOOK,
	EMAE_ASSISTANT,
	EMAE_PAGES
} EMAccountEditorType;

struct _EMAccountEditor {
	GObject parent;

	EMAccountEditorPrivate *priv;

	EMAccountEditorType type;
	GtkWidget *editor; /* gtknotebook or gtkassistant, depending on type */

	EMConfig *config; /* driver object */

	GtkWidget **pages; /* Pages for Anjal's page type editor */

	guint do_signature:1;	/* allow editing signature */
	ServerData * (*emae_check_servers) (const gchar *email);
};

struct _EMAccountEditorClass {
	GObjectClass parent_class;
};

GType		em_account_editor_get_type	(void);
EMAccountEditor *
		em_account_editor_new		(EAccount *account,
						 EMAccountEditorType type,
						 const gchar *id);
EMAccountEditor *
		em_account_editor_new_for_pages	(EAccount *account,
						 EMAccountEditorType type,
						 const gchar *id,
						 GtkWidget **pages);
EAccount *	em_account_editor_get_modified_account
						(EMAccountEditor *emae);
EAccount *	em_account_editor_get_original_account
						(EMAccountEditor *emae);
void		em_account_editor_commit	(EMAccountEditor *emae);
gboolean	em_account_editor_check		(EMAccountEditor *emae,
						 const gchar *page);
gboolean	em_account_editor_save		(EMAccountEditor *emae);
void		em_account_editor_destroy	(EMAccountEditor *emae);
gboolean	em_account_editor_identity_complete
						(EMAccountEditor *emae,
						 GtkWidget **incomplete);
gboolean	em_account_editor_source_complete
						(EMAccountEditor *emae,
						 GtkWidget **incomplete);
gboolean	EMAccountEditorTyperansport_complete
						(EMAccountEditor *emae,
						 GtkWidget **incomplete);
gboolean	em_account_editor_management_complete
						(EMAccountEditor *emae,
						 GtkWidget **incomplete);
void		em_account_editor_build_extra_conf
						(EMAccountEditor *emae,
						 const gchar *url);
void		em_account_editor_auto_detect_extra_conf
						(EMAccountEditor *emae);

G_END_DECLS

#endif /* EM_ACCOUNT_EDITOR_H */
