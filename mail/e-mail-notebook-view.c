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
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 2010 Intel corporation. (www.intel.com)
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


#include <glib.h>
#include <glib/gi18n.h>

#include "mail/e-mail-reader.h"
#include "e-mail-notebook-view.h"
#include "e-mail-paned-view.h"

#include <shell/e-shell-window-actions.h>

struct _EMailNotebookViewPrivate {
	GtkNotebook *book;
	EMailView *current_view;
	GHashTable *views;
};

#define E_SHELL_WINDOW_ACTION_GROUP_MAIL(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "mail")

static EMailViewClass *parent_class;
static GType mail_notebook_view_type;

static void
mail_notebook_view_init (EMailNotebookView  *shell)
{
	shell->priv = g_new0(EMailNotebookViewPrivate, 1);

	shell->priv->views = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
e_mail_notebook_view_finalize (GObject *object)
{
	/* EMailNotebookView *shell = (EMailNotebookView *)object; */

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
mnv_page_changed (GtkNotebook *book, GtkNotebookPage *page,
		  guint page_num, EMailNotebookView *view)
{
	EMailView *mview = gtk_notebook_get_nth_page (book, page_num);

	view->priv->current_view = mview;
	g_signal_emit_by_name (view, "changed");
}

static void
mail_notebook_view_constructed (GObject *object)
{
	GtkWidget *widget, *container;
	EMailNotebookViewPrivate *priv;

	priv = E_MAIL_NOTEBOOK_VIEW (object)->priv;

	container = GTK_WIDGET(object);

	widget = gtk_notebook_new ();
	priv->book = (GtkNotebook *)widget;
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX(container), widget, TRUE, TRUE, 0);

	g_signal_connect (widget, "switch-page", G_CALLBACK(mnv_page_changed), object);

	priv->current_view = e_mail_paned_view_new (E_MAIL_VIEW(object)->content);
	gtk_widget_show (priv->current_view);
	gtk_notebook_append_page (priv->book, priv->current_view, gtk_label_new ("Please select a folder"));
	
}

static void
mail_notebook_view_class_init (EMailViewClass *klass)
{
	GObjectClass * object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->constructed = mail_notebook_view_constructed;
	
	object_class->finalize = e_mail_notebook_view_finalize;

	klass->get_searchbar = e_mail_notebook_view_get_searchbar;
	klass->set_search_strings = e_mail_notebook_view_set_search_strings;
	klass->get_view_instance = e_mail_notebook_view_get_view_instance;
	klass->update_view_instance = e_mail_notebook_view_update_view_instance;


}



GtkWidget *
e_mail_notebook_view_new (EShellContent *content)
{
	g_return_val_if_fail (E_IS_SHELL_CONTENT (content), NULL);

	return g_object_new (
		E_MAIL_NOTEBOOK_VIEW_TYPE,
		"shell-content", content, NULL);
}

static GtkActionGroup *
mail_notebook_view_get_action_group (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_content = E_MAIL_VIEW (reader)->content;
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return E_SHELL_WINDOW_ACTION_GROUP_MAIL (shell_window);	
/*	
	if (!priv->current_view)
		return NULL;

	return e_mail_reader_get_action_group (E_MAIL_READER(priv->current_view));*/
}

static EMFormatHTML *
mail_notebook_view_get_formatter (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	
	if (!priv->current_view)
		return NULL;

	return e_mail_reader_get_formatter (E_MAIL_READER(priv->current_view));
}

static gboolean
mail_notebook_view_get_hide_deleted (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	
	if (!priv->current_view)
		return FALSE;

	return e_mail_reader_get_hide_deleted (E_MAIL_READER(priv->current_view));
}

static GtkWidget *
mail_notebook_view_get_message_list (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	
	if (!priv->current_view)
		return NULL;

	return e_mail_reader_get_message_list (E_MAIL_READER(priv->current_view));	
}

static GtkMenu *
mail_notebook_view_get_popup_menu (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	
	if (!priv->current_view)
		return NULL;

	return e_mail_reader_get_popup_menu (E_MAIL_READER(priv->current_view));	
}

static EShellBackend *
mail_notebook_view_get_shell_backend (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellView *shell_view;

	shell_content = E_MAIL_VIEW (reader)->content;
	shell_view = e_shell_content_get_shell_view (shell_content);

	return e_shell_view_get_shell_backend (shell_view);
}

static GtkWindow *
mail_notebook_view_get_window (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_content = E_MAIL_VIEW (reader)->content;
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return GTK_WINDOW (shell_window);
}

static int
emnv_get_page_num (EMailNotebookView *view,
		   GtkWidget *widget)
{
	EMailNotebookViewPrivate *priv = view->priv;
	int i, n;
	
	n = gtk_notebook_get_n_pages (priv->book);

	for (i=0; i<n; i++) {
		GtkWidget *curr = gtk_notebook_get_nth_page (priv->book, i);
		if (curr == widget)
			return i;
	}

	g_warn_if_reached ();
}

static void
reconnect_changed_event (EMailReader *child, EMailReader *parent)
{
	g_signal_emit_by_name (parent, "changed");
}

static void
reconnect_folder_loaded_event (EMailReader *child, EMailReader *parent)
{
	g_signal_emit_by_name (parent, "folder-loaded");
}

static void
mail_notebook_view_set_folder (EMailReader *reader,
                               CamelFolder *folder,
                               const gchar *folder_uri)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
	GtkWidget *new_view;

	if (!folder_uri)
		return;

	new_view = g_hash_table_lookup (priv->views, folder_uri);
	if (new_view) {
		int curr = emnv_get_page_num (E_MAIL_NOTEBOOK_VIEW (reader), new_view);
		priv->current_view = (EMailView *)new_view;
		gtk_notebook_set_current_page (priv->book, curr);
		return;
	}

	if (folder || folder_uri) {
		int page;

		new_view = e_mail_paned_view_new (E_MAIL_VIEW(reader)->content);
		priv->current_view = (EMailView *)new_view;
		gtk_widget_show (new_view);
		page = gtk_notebook_append_page (priv->book, new_view, gtk_label_new (camel_folder_get_full_name(folder)));
		e_mail_reader_set_folder (E_MAIL_READER(new_view), folder, folder_uri);
		gtk_notebook_set_current_page (priv->book, page);
		g_hash_table_insert (priv->views, g_strdup(folder_uri), new_view);
		g_signal_connect ( E_MAIL_READER(new_view), "changed",
				   G_CALLBACK (reconnect_changed_event),
				   reader);
		g_signal_connect ( E_MAIL_READER (new_view), "folder-loaded",
				   G_CALLBACK (reconnect_folder_loaded_event),
				   reader);
	}
}

static void
mail_notebook_view_show_search_bar (EMailReader *reader)
{
	EMailNotebookViewPrivate *priv = E_MAIL_NOTEBOOK_VIEW (reader)->priv;
		
	e_mail_reader_show_search_bar (E_MAIL_READER(priv->current_view));	
}

EShellSearchbar *
e_mail_notebook_view_get_searchbar (EMailView *view)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	GtkWidget *widget;

	g_return_val_if_fail (
		E_IS_MAIL_NOTEBOOK_VIEW (view), NULL);

	shell_content = E_MAIL_VIEW (view)->content;
	shell_view = e_shell_content_get_shell_view (shell_content);
	widget = e_shell_view_get_searchbar (shell_view);

	return E_SHELL_SEARCHBAR (widget);	
/*	
	if (!E_MAIL_NOTEBOOK_VIEW(view)->priv->current_view)
		return NULL;
	return e_mail_view_get_searchbar (E_MAIL_NOTEBOOK_VIEW(view)->priv->current_view); */
}

void
e_mail_notebook_view_set_search_strings (EMailView *view,
					 GSList *search_strings)
{
	e_mail_view_set_search_strings (E_MAIL_NOTEBOOK_VIEW (view)->priv->current_view, search_strings);
}

GalViewInstance *
e_mail_notebook_view_get_view_instance (EMailView *view)
{
	if (!E_MAIL_NOTEBOOK_VIEW(view)->priv->current_view)
		return NULL;

	return e_mail_view_get_view_instance (E_MAIL_NOTEBOOK_VIEW (view)->priv->current_view);
}

void
e_mail_notebook_view_update_view_instance (EMailView *view)
{
	e_mail_view_update_view_instance (E_MAIL_NOTEBOOK_VIEW (view)->priv->current_view);
}

static void
mail_notebook_view_reader_init (EMailReaderIface *iface)
{
	iface->get_action_group = mail_notebook_view_get_action_group;
	iface->get_formatter = mail_notebook_view_get_formatter;
	iface->get_hide_deleted = mail_notebook_view_get_hide_deleted;
	iface->get_message_list = mail_notebook_view_get_message_list;
	iface->get_popup_menu = mail_notebook_view_get_popup_menu;
	iface->get_shell_backend = mail_notebook_view_get_shell_backend;
	iface->get_window = mail_notebook_view_get_window;
	iface->set_folder = mail_notebook_view_set_folder;
	iface->show_search_bar = mail_notebook_view_show_search_bar;
}

GType
e_mail_notebook_view_get_type (void)
{
	return mail_notebook_view_type;
}

void
e_mail_notebook_view_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EMailNotebookViewClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_notebook_view_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMailNotebookView),
		0,     /* n_preallocs */
		(GInstanceInitFunc) mail_notebook_view_init,
		NULL   /* value_table */
	};

	static const GInterfaceInfo reader_info = {
		(GInterfaceInitFunc) mail_notebook_view_reader_init,
		(GInterfaceFinalizeFunc) NULL,
		NULL  /* interface_data */
	};

	mail_notebook_view_type = g_type_module_register_type (
		type_module, E_MAIL_VIEW_TYPE,
		"EMailNotebookView", &type_info, 0);

	g_type_module_add_interface (
		type_module, mail_notebook_view_type,
		E_TYPE_MAIL_READER, &reader_info);
}
