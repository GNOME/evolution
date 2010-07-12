/*
 * e-mail-shell-content.c
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

#include "e-mail-shell-content.h"

#include <glib/gi18n.h>
#include <libedataserver/e-data-server-util.h>

#include "e-util/e-util-private.h"
#include "e-util/e-binding.h"
#include "e-util/gconf-bridge.h"
#include "widgets/menus/gal-view-etable.h"
#include "widgets/menus/gal-view-instance.h"
#include "widgets/misc/e-paned.h"
#include "widgets/misc/e-preview-pane.h"
#include "widgets/misc/e-search-bar.h"

#include "em-utils.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "message-list.h"

#include "e-mail-view.h"
#include "e-mail-paned-view.h"
#include "e-mail-notebook-view.h"
#include "e-mail-reader.h"
#include "e-mail-reader-utils.h"
#include "e-mail-shell-backend.h"
#include "e-mail-shell-view-actions.h"

#define E_MAIL_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SHELL_CONTENT, EMailShellContentPrivate))

struct _EMailShellContentPrivate {
	int temp;
};


static gpointer parent_class;
static GType mail_shell_content_type;

static void
mail_shell_content_dispose (GObject *object)
{
	EMailShellContentPrivate *priv;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
mail_shell_content_constructed (GObject *object)
{
	EMailShellContentPrivate *priv;
	EShellContent *shell_content;
	EShellBackend *shell_backend;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GtkWidget *container;
	GtkWidget *widget;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_content = E_SHELL_CONTENT (object);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	/* Build content widgets. */

	container = GTK_WIDGET (object);

	widget = e_mail_notebook_view_new (E_SHELL_CONTENT(object));
	E_MAIL_SHELL_CONTENT(object)->view = (EMailView *)widget;
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

}

static guint32
mail_shell_content_check_state (EShellContent *shell_content)
{
	return e_mail_reader_check_state (E_MAIL_READER (E_MAIL_SHELL_CONTENT(shell_content)->view));
}

static void
mail_shell_content_focus_search_results (EShellContent *shell_content)
{
	EMailShellContentPrivate *priv;

	priv = E_MAIL_SHELL_CONTENT_GET_PRIVATE (shell_content);

	gtk_widget_grab_focus (e_mail_reader_get_message_list(E_MAIL_READER (E_MAIL_SHELL_CONTENT(shell_content)->view)));
}

static GtkActionGroup *
mail_shell_content_get_action_group (EMailReader *reader)
{
	EShellContent *shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_content = E_SHELL_CONTENT (reader);
	shell_view = e_shell_content_get_shell_view (shell_content);
	shell_window = e_shell_view_get_shell_window (shell_view);

	return E_SHELL_WINDOW_ACTION_GROUP_MAIL (shell_window);	
}

static EMFormatHTML *
mail_shell_content_get_formatter (EMailReader *reader)
{
	return e_mail_reader_get_formatter (E_MAIL_READER(E_MAIL_SHELL_CONTENT(reader)->view));
}

static gboolean
mail_shell_content_get_hide_deleted (EMailReader *reader)
{
	return e_mail_reader_get_hide_deleted (E_MAIL_READER(E_MAIL_SHELL_CONTENT(reader)->view));
}

static GtkWidget *
mail_shell_content_get_message_list (EMailReader *reader)
{
	return e_mail_reader_get_message_list (E_MAIL_READER(E_MAIL_SHELL_CONTENT(reader)->view));
}

static GtkMenu *
mail_shell_content_get_popup_menu (EMailReader *reader)
{
	return e_mail_reader_get_popup_menu (E_MAIL_READER(E_MAIL_SHELL_CONTENT(reader)->view));
}

static EShellBackend *
mail_shell_content_get_shell_backend (EMailReader *reader)
{
	return e_mail_reader_get_shell_backend (E_MAIL_READER(E_MAIL_SHELL_CONTENT(reader)->view));
}

static GtkWindow *
mail_shell_content_get_window (EMailReader *reader)
{
	return e_mail_reader_get_window (E_MAIL_READER(E_MAIL_SHELL_CONTENT(reader)->view));
}

static void
mail_shell_content_set_folder (EMailReader *reader,
                               CamelFolder *folder,
                               const gchar *folder_uri)
{
	return e_mail_reader_set_folder (E_MAIL_READER(E_MAIL_SHELL_CONTENT(reader)->view), 
					 folder,
					 folder_uri);
}

static void
mail_shell_content_show_search_bar (EMailReader *reader)
{
	e_mail_reader_show_search_bar (E_MAIL_READER(E_MAIL_SHELL_CONTENT(reader)->view));
}

static void
mail_shell_content_class_init (EMailShellContentClass *class)
{
	GObjectClass *object_class;
	EShellContentClass *shell_content_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_shell_content_dispose;
	object_class->constructed = mail_shell_content_constructed;

	shell_content_class = E_SHELL_CONTENT_CLASS (class);
	shell_content_class->check_state = mail_shell_content_check_state;
	shell_content_class->focus_search_results = mail_shell_content_focus_search_results;


}

static void
mail_shell_content_init (EMailShellContent *mail_shell_content)
{
	mail_shell_content->priv =
		E_MAIL_SHELL_CONTENT_GET_PRIVATE (mail_shell_content);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_mail_shell_content_get_type (void)
{
	return mail_shell_content_type;
}

static void
mail_shell_content_reader_init (EMailReaderIface *iface)
{
	iface->get_action_group = mail_shell_content_get_action_group;
	iface->get_formatter = mail_shell_content_get_formatter;
	iface->get_hide_deleted = mail_shell_content_get_hide_deleted;
	iface->get_message_list = mail_shell_content_get_message_list;
	iface->get_popup_menu = mail_shell_content_get_popup_menu;
	iface->get_shell_backend = mail_shell_content_get_shell_backend;
	iface->get_window = mail_shell_content_get_window;
	iface->set_folder = mail_shell_content_set_folder;
	iface->show_search_bar = mail_shell_content_show_search_bar;
}

void
e_mail_shell_content_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EMailShellContentClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) mail_shell_content_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EMailShellContent),
		0,     /* n_preallocs */
		(GInstanceInitFunc) mail_shell_content_init,
		NULL   /* value_table */
	};

	static const GInterfaceInfo reader_info = {
		(GInterfaceInitFunc) mail_shell_content_reader_init,
		(GInterfaceFinalizeFunc) NULL,
		NULL  /* interface_data */
	};

	mail_shell_content_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_CONTENT,
		"EMailShellContent", &type_info, 0);

	g_type_module_add_interface (
		type_module, mail_shell_content_type,
		E_TYPE_MAIL_READER, &reader_info);	

}

GtkWidget *
e_mail_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MAIL_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}

EShellSearchbar *
e_mail_shell_content_get_searchbar (EMailShellContent *mail_shell_content)
{
	return e_mail_view_get_searchbar (mail_shell_content->view);
}

void
e_mail_shell_content_set_search_strings (EMailShellContent *mail_shell_content,
					 GSList *search_strings)
{
	e_mail_view_set_search_strings (mail_shell_content->view, search_strings);
}

GalViewInstance *
e_mail_shell_content_get_view_instance (EMailShellContent *mail_shell_content)
{
	return e_mail_view_get_view_instance (mail_shell_content->view);
}

void
e_mail_shell_content_update_view_instance (EMailShellContent *mail_shell_content)
{
	e_mail_view_update_view_instance (mail_shell_content->view);
}

