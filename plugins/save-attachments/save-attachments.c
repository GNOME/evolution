/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

/* This is prototype code only, this may, or may not, use undocumented
 * unstable or private internal function calls. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkframe.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkscrolledwindow.h>
#include <libgnomeui/gnome-file-entry.h>

#include <camel/camel-folder.h>
#include <camel/camel-exception.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-multipart.h>
#include <camel/camel-utf8.h>

#include "mail/em-menu.h"
#include "mail/em-utils.h"

/* these are sort of mail-internal */
#include "mail/mail-mt.h"
#include "mail/mail-ops.h"

void org_gnome_save_attachments_save(EPlugin *ep, EMMenuTargetSelect *target);

struct _save_data {
	CamelFolder *folder;
	char *uid;
	CamelMimeMessage *msg;

	char *path;
	char *base;

	GtkWidget *entry;
	GtkWidget *tree;
	GtkTreeStore *model;
};

static void
free_data(struct _save_data *data)
{
	if (data->msg)
		camel_object_unref(data->msg);
	g_free(data->base);
	g_free(data->path);
	g_free(data->uid);
	camel_object_unref(data->folder);
	g_free(data);
}

static char *
clean_name(const char *s)
{
	GString *out = g_string_new("");
	int c;
	char *r;

	while ( (c = camel_utf8_getc((const unsigned char **)&s)) ) {
		if (!g_unichar_isprint(c) || ( c < 0x7f && strchr(" /'\"`&();|<>$%{}!", c )))
			c = '_';
		g_string_append_u(out, c);
	}

	r = g_strdup(out->str);
	g_string_free(out, TRUE);

	return r;
}

static void
fill_model_rec(CamelMimeMessage *msg, CamelMimePart *part, GtkTreeStore *model, GtkTreeIter *parent, GString *name)
{
	CamelDataWrapper *containee;
	int parts, i;
	char *type;
	GtkTreeIter iter;
	int len = name->len;
	CamelContentType *mime;

	containee = camel_medium_get_content_object((CamelMedium *)part);
	if (containee == NULL)
		return;

	mime = ((CamelDataWrapper *)containee)->mime_type;
	type = camel_content_type_simple(mime);

	if (CAMEL_IS_MULTIPART(containee)) {
		gtk_tree_store_append(model, &iter, parent);
		g_string_append_printf(name, ".multipart");
		gtk_tree_store_set(model, &iter, 0, FALSE, 1, type, 2, name->str, 3, name->str, 4, part, -1);

		parts = camel_multipart_get_number((CamelMultipart *)containee);
		for (i = 0; i < parts; i++) {
			CamelMimePart *mpart = camel_multipart_get_part((CamelMultipart *)containee, i);

			g_string_truncate(name, len);
			g_string_append_printf(name, ".%d", i);
			fill_model_rec(msg, mpart, model, &iter, name);
		}
	} else if (CAMEL_IS_MIME_MESSAGE(containee)) {
		gtk_tree_store_append(model, &iter, parent);
		g_string_append_printf(name, ".msg");
		gtk_tree_store_set(model, &iter, 0, FALSE, 1, type, 2, name->str, 3, name->str, 4, part, -1);
		fill_model_rec(msg, (CamelMimePart *)containee, model, &iter, name);
	} else {
		char *filename = NULL;
		const char *ext = NULL, *tmp;
		int save = FALSE;

		gtk_tree_store_append(model, &iter, parent);
		tmp = camel_mime_part_get_filename(part);
		if (tmp) {
			filename = clean_name(tmp);
			ext = strrchr(filename, '.');
		}
		tmp = camel_mime_part_get_disposition(part);
		if (tmp && !strcmp(tmp, "attachment"))
			save = TRUE;

		if (camel_content_type_is(mime, "text", "*")) {
			if (ext == NULL) {
				if ((ext = mime->subtype) == NULL || !strcmp(ext, "plain"))
					ext = "text";
			}
		} else if (camel_content_type_is(mime, "image", "*")) {
			if (ext == NULL) {
				if ((ext = mime->subtype) == NULL)
					ext = "image";
			}
			save = TRUE;
		}

		g_string_append_printf(name, ".%s", ext);
		gtk_tree_store_set(model, &iter, 0, save, 1, type, 2, filename?filename:name->str, 3, filename?NULL:name->str, 4, part, -1);
		g_free(filename);
	}
	g_free(type);

	g_string_truncate(name, len);
}

static void
fill_model(CamelMimeMessage *msg, GtkTreeStore *model)
{
	GString *name = g_string_new("");
	GtkTreeIter iter;

	gtk_tree_store_append(model, &iter, NULL);
	gtk_tree_store_set(model, &iter, 0, FALSE, 1, "message/rfc822", 2, ".msg", 3, ".msg", 4, msg, -1);
	fill_model_rec(msg, (CamelMimePart *)msg, model, &iter, name);
	g_string_free(name, TRUE);
}

static gboolean
save_part(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, void *d)
{
	struct _save_data *data = d;
	char *filename, *ext, *save;
	CamelMimePart *part;
	gboolean doit;

	/* TODO: check for existing file */

	gtk_tree_model_get(model, iter, 0, &doit, -1);
	if (!doit)
		return FALSE;

	gtk_tree_model_get(model, iter, 2, &filename, 3, &ext, 4, &part, -1);
	if (ext == NULL)
		save = g_build_filename(data->path, filename, NULL);
	else
		save = g_strdup_printf("%s%s", data->base, ext);

	/* FIXME: if part == data->msg then we need to save this
	 * differently, not using the envelope MimePart */

	em_utils_save_part_to_file(NULL, save, part);

	g_free(ext);
	g_free(filename);

	return FALSE;
}

static void
save_response(GtkWidget *d, int id, struct _save_data *data)
{
	if (id == GTK_RESPONSE_OK) {
		char *tmp;

		data->base = gnome_file_entry_get_full_path((GnomeFileEntry *)data->entry, FALSE);
		data->path = g_strdup(data->base);
		tmp = strrchr(data->path, '/');
		if (tmp)
			*tmp = 0;
		gtk_tree_model_foreach((GtkTreeModel *)data->model, save_part, data);
	}

	gtk_widget_destroy(d);
	free_data(data);
}

static gboolean
entry_changed_update(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, void *d)
{
	const char *name = d;
	char *filename, *ext;

	gtk_tree_model_get(model, iter, 3, &ext, -1);
	if (ext) {
		filename = g_strdup_printf("%s%s", name, ext);
		gtk_tree_store_set((GtkTreeStore *)model, iter, 2, filename, -1);
		g_free(filename);
		g_free(ext);
	}

	return FALSE;
}

static void
entry_changed(GtkWidget *entry, struct _save_data *data)
{
	char *path;
	const char *file;
	struct stat st;

	path = gnome_file_entry_get_full_path((GnomeFileEntry *)data->entry, FALSE);
	if (path == NULL
	    || (file = strrchr(path, '/')) == NULL
	    || file[1] == 0
	    || (stat(path, &st) == 0 && S_ISDIR(st.st_mode)))
		file = "attachment";
	else
		file++;

	gtk_tree_model_foreach((GtkTreeModel *)data->model, entry_changed_update, (void *)file);
	g_free(path);
}

static void
toggle_changed(GtkWidget *entry, const char *spath, struct _save_data *data)
{
        GtkTreePath *path;
        GtkTreeIter iter;
        
        path = gtk_tree_path_new_from_string(spath);
        if (gtk_tree_model_get_iter((GtkTreeModel *)data->model, &iter, path)) {
		gboolean on;

                gtk_tree_model_get((GtkTreeModel *)data->model, &iter, 0, &on, -1);
		gtk_tree_store_set(data->model, &iter, 0, !on, -1);
        }
        
        gtk_tree_path_free (path);
}

static void
save_got_message(CamelFolder *folder, const char *uid, CamelMimeMessage *msg, void *d)
{
	struct _save_data *data = d;
	GtkDialog *dialog;
	GtkWidget *w, *tree;
	GtkTreeStore *model;
	GtkCellRenderer *renderer;

	/* not found, the mailer will show an error box for this */
	if (msg == NULL) {
		free_data(data);
		return;
	}

	data->msg = msg;
	camel_object_ref(msg);

	dialog = (GtkDialog *)gtk_dialog_new_with_buttons("Save attachments",
							  NULL, /* target->parent? */
							  0,
							  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							  GTK_STOCK_SAVE, GTK_RESPONSE_OK,
							  NULL);
	w = gnome_file_entry_new("save-attachments", _("Select save base name"));
	data->entry = w;
	g_object_set(w, "filechooser_action", GTK_FILE_CHOOSER_ACTION_SAVE, NULL);
	gtk_widget_show(w);
	gtk_box_pack_start((GtkBox *)dialog->vbox, w, FALSE, TRUE, 6);

	w = gnome_file_entry_gtk_entry((GnomeFileEntry *)data->entry);
	g_signal_connect(w, "changed", G_CALLBACK(entry_changed), data);

	model = gtk_tree_store_new(5, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	data->model = model;
	fill_model(msg, model);

	tree = gtk_tree_view_new_with_model((GtkTreeModel *)model);
	data->tree = tree;
	gtk_widget_show(tree);
	gtk_tree_view_expand_all((GtkTreeView *)tree);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes((GtkTreeView *)tree, -1,
						    _("MIME Type"), renderer, "text", 1, NULL);
	gtk_tree_view_set_expander_column((GtkTreeView *)tree, gtk_tree_view_get_column((GtkTreeView *)tree, 0));

	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(renderer, "activatable", TRUE, NULL);
	g_signal_connect(renderer, "toggled", G_CALLBACK(toggle_changed), data);

	gtk_tree_view_insert_column_with_attributes((GtkTreeView *)tree, -1,
						    _("Save"), renderer, "active", 0, NULL);	
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes((GtkTreeView *)tree, -1,
						    _("Name"), renderer, "text", 2, NULL);

	w = g_object_new(gtk_frame_get_type(),
			 "shadow_type", GTK_SHADOW_NONE,
			 "label_widget", g_object_new(gtk_label_get_type(),
						      "label", "<span weight=\"bold\">Attachments</span>",
						      "use_markup", TRUE,
						      "xalign", 0.0, NULL),
			 "child", g_object_new(gtk_alignment_get_type(),
					       "left_padding", 12,
					       "top_padding", 6,
					       "child", g_object_new(gtk_scrolled_window_get_type(),
								     "hscrollbar_policy", GTK_POLICY_AUTOMATIC,
								     "vscrollbar_policy", GTK_POLICY_AUTOMATIC,
								     "shadow_type", GTK_SHADOW_IN,
								     "child", tree,
								     NULL),
					       NULL),
			 NULL);
	gtk_widget_show_all(w);

	gtk_box_pack_start((GtkBox *)dialog->vbox, w, TRUE, TRUE, 0);
	g_signal_connect(dialog, "response", G_CALLBACK(save_response), data);
	gtk_window_set_default_size((GtkWindow *)dialog, 500, 500);
	gtk_widget_show((GtkWidget *)dialog);
}

void
org_gnome_save_attachments_save(EPlugin *ep, EMMenuTargetSelect *target)
{
	struct _save_data *data;

	if (target->uids->len != 1)
		return;

	data = g_malloc0(sizeof(*data));
	data->folder = target->folder;
	camel_object_ref(data->folder);
	data->uid = g_strdup(target->uids->pdata[0]);

	mail_get_message(data->folder, data->uid, save_got_message, data, mail_thread_new);
}
