/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright(C) 2000, 2001, 2002, 2003 Ximian, Inc.
 *
 * Authors: Ettore Perazzoli
 *	    Michael Zucchi
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "em-folder-selector.h"

#include "shell/e-storage-set-view.h"
#include "shell/e-storage-set.h"

#include <libgnome/gnome-i18n.h>

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>

#include <gtk/gtkentry.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>

#include <camel/camel-url.h>

#include <string.h>

#define PARENT_TYPE (gtk_dialog_get_type())
static GtkDialogClass *parent_class = NULL;

static gboolean
check_folder_type_valid(EMFolderSelector *emfs)
{
	const char *selected;
	EFolder *folder;

	selected = e_storage_set_view_get_current_folder(emfs->essv);
	if (selected == NULL)
		return FALSE;

	folder = e_storage_set_get_folder(emfs->ess, selected);
	if (folder == NULL)
		return FALSE;

	return TRUE;
}

#if 0				/* EPFIXME */
static void
folder_creation_dialog_result_cb(EShell *shell,
				  EShellFolderCreationDialogResult result,
				  const char *path,
				  void *data)
{
	EMFolderSelector *dialog;

	dialog = EM_FOLDER_SELECTOR(data);

	if (result == E_SHELL_FOLDER_CREATION_DIALOG_RESULT_SUCCESS)
		e_storage_set_view_set_current_folder(E_STORAGE_SET_VIEW(priv->storage_set_view),
						       path);
}
#endif

static void
emfs_dispose(GObject *object)
{
	EMFolderSelector *emfs = (EMFolderSelector *)object;

	if (emfs->ess != NULL) {
		g_object_unref(emfs->ess);
		emfs->ess = NULL;
		emfs->essv = NULL;
	}

	(* G_OBJECT_CLASS(parent_class)->dispose)(object);
}

static void
emfs_finalize(GObject *object)
{
	/*EMFolderSelector *emfs = (EMFolderSelector *)object;*/

	(* G_OBJECT_CLASS(parent_class)->finalize)(object);
}

static void
emfs_response(GtkDialog *dialog, int response)
{
	EMFolderSelector *emfs = (EMFolderSelector *)dialog;
	const char *path;

	switch (response) {
	case EM_FOLDER_SELECTOR_RESPONSE_NEW:
		path = e_storage_set_view_get_current_folder(emfs->essv);

		printf("create new folder, default parent '%s'\n", path);
		break;
	}
}

static void
emfs_class_init(EMFolderSelectorClass *klass)
{
	GObjectClass *object_class;
	GtkDialogClass *dialog_class;

	parent_class = g_type_class_ref(PARENT_TYPE);
	object_class = G_OBJECT_CLASS(klass);
	dialog_class = GTK_DIALOG_CLASS(klass);

	object_class->dispose  = emfs_dispose;
	object_class->finalize = emfs_finalize;

	dialog_class->response = emfs_response;
}

static void
emfs_init(EMFolderSelector *emfs)
{
	emfs->flags = 0;
}

static void
folder_selected_cb(EStorageSetView *essv, const char *path, EMFolderSelector *emfs)
{
	if (check_folder_type_valid(emfs))
		gtk_dialog_set_response_sensitive(GTK_DIALOG(emfs), GTK_RESPONSE_OK, TRUE);
	else
		gtk_dialog_set_response_sensitive(GTK_DIALOG(emfs), GTK_RESPONSE_OK, FALSE);
}

static void
double_click_cb(EStorageSetView *essv, int row, ETreePath path, int col, GdkEvent *event, EMFolderSelector *emfs)
{
	if (check_folder_type_valid(emfs)) {
		/*g_signal_emit(emfs, signals[FOLDER_SELECTED], 0,
		  em_folder_selector_get_selected(emfs));*/
		printf("double clicked!\n");
	}
}

void
em_folder_selector_construct(EMFolderSelector *emfs, EStorageSet *ess, guint32 flags, const char *title, const char *text)
{
	GtkWidget *scrolled_window;
	GtkWidget *text_label;

	gtk_window_set_default_size(GTK_WINDOW(emfs), 350, 300);
	gtk_window_set_modal(GTK_WINDOW(emfs), TRUE);
	gtk_window_set_title(GTK_WINDOW(emfs), title);
	gtk_container_set_border_width(GTK_CONTAINER(emfs), 6);

	emfs->flags = flags;
	if (flags & EM_FOLDER_SELECTOR_CAN_CREATE)
		gtk_dialog_add_buttons(GTK_DIALOG(emfs), GTK_STOCK_NEW, EM_FOLDER_SELECTOR_RESPONSE_NEW, NULL);

	gtk_dialog_add_buttons(GTK_DIALOG(emfs),
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			       GTK_STOCK_OK, GTK_RESPONSE_OK,
			       NULL);

	gtk_dialog_set_response_sensitive(GTK_DIALOG(emfs), GTK_RESPONSE_OK, FALSE);
	gtk_dialog_set_default_response(GTK_DIALOG(emfs), GTK_RESPONSE_OK);

	emfs->ess = ess;
	g_object_ref(ess);

	emfs->essv = (EStorageSetView *)e_storage_set_create_new_view(ess, NULL);
	e_storage_set_view_set_allow_dnd(emfs->essv, FALSE);
	e_storage_set_view_enable_search(emfs->essv, TRUE);

	g_signal_connect(emfs->essv, "double_click", G_CALLBACK(double_click_cb), emfs);
	g_signal_connect(emfs->essv, "folder_selected", G_CALLBACK(folder_selected_cb), emfs);

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER(scrolled_window), (GtkWidget *)emfs->essv);
        
	gtk_box_pack_end(GTK_BOX(GTK_DIALOG(emfs)->vbox), scrolled_window, TRUE, TRUE, 6);
	gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(emfs)->vbox), 6); 
	
	gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(emfs)->vbox), 6);
	
	gtk_widget_show((GtkWidget *)emfs->essv);
	gtk_widget_show(scrolled_window);

	if (text != NULL) {
		text_label = gtk_label_new(text);
		gtk_label_set_justify(GTK_LABEL(text_label), GTK_JUSTIFY_LEFT); 
		gtk_widget_show(text_label);

		gtk_box_pack_end(GTK_BOX(GTK_DIALOG(emfs)->vbox), text_label, FALSE, TRUE, 6);
		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(emfs)->vbox), 6);
	}

	GTK_WIDGET_SET_FLAGS((GtkWidget *)emfs->essv, GTK_CAN_FOCUS);
	gtk_widget_grab_focus((GtkWidget *)emfs->essv);
}

GtkWidget *
em_folder_selector_new(EStorageSet *ess, guint32 flags, const char *title, const char *text)
{
	EMFolderSelector *emfs;

	g_return_val_if_fail(E_IS_STORAGE_SET(ess), NULL);

	emfs = g_object_new(em_folder_selector_get_type(), NULL);
	em_folder_selector_construct(emfs, ess, flags, title, text);

	return GTK_WIDGET(emfs);
}

static void
emfs_create_name_changed(GtkEntry *entry, EMFolderSelector *emfs)
{
	int active;

	active =  e_storage_set_view_get_current_folder(emfs->essv) != NULL
		&& emfs->name_entry->text_length > 0;

	gtk_dialog_set_response_sensitive((GtkDialog *)emfs, GTK_RESPONSE_OK, active);
}

static void
emfs_create_name_activate(GtkEntry *entry, EMFolderSelector *emfs)
{
	printf("entry activated, woop\n");
}

GtkWidget *
em_folder_selector_create_new(EStorageSet *ess, guint32 flags, const char *title, const char *text)
{
	EMFolderSelector *emfs;
	GtkWidget *hbox, *w;

	g_return_val_if_fail(E_IS_STORAGE_SET(ess), NULL);

	emfs = g_object_new(em_folder_selector_get_type(), NULL);
	em_folder_selector_construct(emfs, ess, flags, title, text);

	hbox = gtk_hbox_new(FALSE, 0);
	w = gtk_label_new_with_mnemonic(_("Folder _name"));
	gtk_box_pack_start((GtkBox *)hbox, w, FALSE, FALSE, 6);
	emfs->name_entry = (GtkEntry *)gtk_entry_new();
	g_signal_connect(emfs->name_entry, "changed", G_CALLBACK(emfs_create_name_changed), emfs);
	g_signal_connect(emfs->name_entry, "activate", G_CALLBACK(emfs_create_name_activate), emfs);
	gtk_box_pack_start((GtkBox *)hbox, (GtkWidget *)emfs->name_entry, TRUE, FALSE, 6);
	gtk_widget_show_all(hbox);

	gtk_box_pack_start((GtkBox *)((GtkDialog *)emfs)->vbox, hbox, FALSE, TRUE, 0);

	return GTK_WIDGET(emfs);
}

void
em_folder_selector_set_selected(EMFolderSelector *emfs, const char *path)
{
	e_storage_set_view_set_current_folder(emfs->essv, path);
}

void
em_folder_selector_set_selected_uri(EMFolderSelector *emfs, const char *uri)
{
	const char *path;

	path = e_storage_set_get_path_for_physical_uri(emfs->ess, uri);
	if (path)
		e_storage_set_view_set_current_folder(emfs->essv, path);
}

const char *
em_folder_selector_get_selected(EMFolderSelector *emfs)
{
	const char *path;

	path = e_storage_set_view_get_current_folder(emfs->essv);
	if (emfs->name_entry) {
		g_free(emfs->selected);
		emfs->selected = g_strdup_printf("%s/%s", path, gtk_entry_get_text(emfs->name_entry));
		path = emfs->selected;
	}

	return path;
}

const char *
em_folder_selector_get_selected_uri(EMFolderSelector *emfs)
{
	const char *path;
	EFolder *folder;

	path = e_storage_set_view_get_current_folder(emfs->essv);
	if (path == NULL) {
		printf("current folder is null?\n");
		return NULL;
	}

	folder = e_storage_set_get_folder(emfs->ess, path);
	if (folder == NULL) {
		printf("path ok, but can't get folder?\n");
		return NULL;
	}

	path = e_folder_get_physical_uri(folder);
	if (path && emfs->name_entry) {
		CamelURL *url;
		char *newpath;

		url = camel_url_new(path, NULL);
		newpath = g_strdup_printf("%s/%s", url->fragment?url->fragment:url->path, gtk_entry_get_text(emfs->name_entry));
		if (url->fragment)
			camel_url_set_fragment(url, newpath);
		else
			camel_url_set_path(url, newpath);
		g_free(emfs->selected_uri);
		emfs->selected_uri = camel_url_to_string(url, 0);
		camel_url_free(url);
		path = emfs->selected_uri;
	}

	return path;
}

E_MAKE_TYPE(em_folder_selector, "EMFolderSelector", EMFolderSelector, emfs_class_init, emfs_init, PARENT_TYPE)
