/*
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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef URL_EDITOR_DIALOG_H
#define URL_EDITOR_DIALOG_H

#include <gtk/gtk.h>
#include "publish-location.h"

G_BEGIN_DECLS

#define URL_EDITOR_DIALOG_TYPE            (url_editor_dialog_get_type ())
#define URL_EDITOR_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), URL_EDITOR_DIALOG_TYPE, UrlEditorDialog))
#define URL_EDITOR_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), URL_EDITOR_DIALOG_TYPE, UrlEditorDialogClass))
#define IS_URL_EDITOR_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), URL_EDITOR_DIALOG_TYPE))
#define IS_URL_EDITOR_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), URL_EDITOR_DIALOG_TYPE))

enum {
	URL_LIST_ENABLED_COLUMN,
	URL_LIST_LOCATION_COLUMN,
	URL_LIST_URL_COLUMN,
	URL_LIST_N_COLUMNS
};

enum {
	TYPE_SFTP,
	TYPE_ANON_FTP,
	TYPE_FTP,
	TYPE_SMB,
	TYPE_DAV,
	TYPE_DAVS,
	TYPE_URI
};

typedef struct _UrlEditorDialog UrlEditorDialog;
typedef struct _UrlEditorDialogClass UrlEditorDialogClass;

struct _UrlEditorDialog {
	GtkDialog parent;

	GtkTreeModel *url_list_model;
	EPublishUri *uri;

	GtkBuilder *builder;

	GtkWidget *type_selector;
	GtkWidget *fb_duration_label;
	GtkWidget *fb_duration_spin;
	GtkWidget *fb_duration_combo;
	GtkWidget *publish_frequency;

	GtkWidget *events_swin;

	GtkWidget *events_selector;

	GtkWidget *publish_service;
	GtkWidget *server_entry;
	GtkWidget *file_entry;

	GtkWidget *port_entry;
	GtkWidget *username_entry;
	GtkWidget *password_entry;
	GtkWidget *remember_pw;

	GtkWidget *optional_label;

	GtkWidget *port_hbox;
	GtkWidget *username_hbox;
	GtkWidget *password_hbox;
	GtkWidget *server_hbox;
	GtkWidget *file_hbox;

	GtkWidget *port_label;
	GtkWidget *username_label;
	GtkWidget *password_label;
	GtkWidget *server_label;
	GtkWidget *file_label;

	GtkWidget *ok;
	GtkWidget *cancel;
};

struct _UrlEditorDialogClass {
	GtkDialogClass parent_class;
};

GtkWidget *url_editor_dialog_new (GtkTreeModel *url_list_model, EPublishUri *uri);
GType      url_editor_dialog_get_type (void);
gboolean   url_editor_dialog_run (UrlEditorDialog *dialog);

G_END_DECLS

#endif /* _URL_EDITOR_DIALOG_H_ */
