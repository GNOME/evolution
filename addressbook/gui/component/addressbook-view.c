/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* addressbook-view.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Chris Toshok (toshok@ximian.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-href.h>
#include <libgnomeui/gnome-uidefs.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>
#include <e-util/e-util.h>
#include <libedataserverui/e-source-selector.h>
#include <libedataserverui/e-passwords.h>

#include "e-util/e-error.h"
#include "e-util/e-request.h"
#include "misc/e-task-bar.h"
#include "misc/e-info-label.h"

#include "e-util/e-icon-factory.h"
#include "e-util/e-util-private.h"
#include "shell/e-user-creatable-items-handler.h"

#include "evolution-shell-component-utils.h"
#include "e-activity-handler.h"
#include "e-contact-editor.h"
#include "addressbook-config.h"
#include "addressbook.h"
#include "addressbook-view.h"
#include "addressbook-component.h"
#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/gui/merging/eab-contact-merging.h"
#include "addressbook/printing/e-contact-print.h"
#include "addressbook/util/eab-book-util.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "addressbook/gui/widgets/eab-menu.h"

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

#define d(x)

struct _AddressbookViewPrivate {
	GtkWidget *notebook;
	BonoboControl *folder_view_control;

	GtkWidget *statusbar_widget;
	EActivityHandler *activity_handler;

	GtkWidget *info_widget;
	GtkWidget *sidebar_widget;
	GtkWidget *selector;

	GConfClient *gconf_client;

	GHashTable *uid_to_view;
	GHashTable *uid_to_editor;

	EBook *book;
	guint activity_id;
	ESourceList *source_list;
	char *passwd;
	EUserCreatableItemsHandler *creatable_items_handler;

	EABMenu *menu;
};

static void set_status_message (EABView *eav, const char *message, AddressbookView *view);

static void activate_source (AddressbookView *view, ESource *source);

static void addressbook_view_init	(AddressbookView      *view);
static void addressbook_view_class_init	(AddressbookViewClass *klass);

static void
control_activate (BonoboControl     *control,
		  BonoboUIComponent *uic,
		  AddressbookView   *view)
{
	AddressbookViewPrivate *priv = view->priv;
	Bonobo_UIContainer remote_ui_container;
	EABView *v = get_current_view (view);
	char *xmlfile;

	remote_ui_container = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, remote_ui_container, NULL);
	bonobo_object_release_unref (remote_ui_container, NULL);

	bonobo_ui_component_freeze (uic, NULL);

	xmlfile = g_build_filename (EVOLUTION_UIDIR,
				    "evolution-addressbook.xml",
				    NULL);
	bonobo_ui_util_set_ui (uic, PREFIX,
			       xmlfile,
			       "evolution-addressbook", NULL);
	g_free (xmlfile);

	if (v)
		eab_view_setup_menus (v, uic);

	e_user_creatable_items_handler_activate (priv->creatable_items_handler, uic);

	bonobo_ui_component_thaw (uic, NULL);

	if (v)
		update_command_state (v, view);
}

static void
control_activate_cb (BonoboControl *control,
		     gboolean activate,
		     AddressbookView *view)
{
	BonoboUIComponent *uic;
	EABView *v = get_current_view (view);

	uic = bonobo_control_get_ui_component (control);
	g_return_if_fail (uic != NULL);

	if (activate) {
		control_activate (control, uic, view);
		e_menu_activate((EMenu *)view->priv->menu, uic, activate);
		if (activate && v && v->model)
			eab_model_force_folder_bar_message (v->model);
	} else {
		e_menu_activate((EMenu *)view->priv->menu, uic, activate);
		bonobo_ui_component_unset_container (uic, NULL);
		eab_view_discard_menus (v);
	}
}

static void
load_uri_for_selection (ESourceSelector *selector,
			AddressbookView *view,
			gboolean force)
{
	ESource *selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (selector));
	ESource *primary = get_primary_source (view);

	if (selected_source != NULL &&
	    ((primary && (!g_str_equal (e_source_peek_uid (primary),e_source_peek_uid (selected_source) )))||force))
		activate_source (view, selected_source);
}

/* Folder popup menu callbacks */
typedef struct {
	AddressbookView *view;
	ESource *selected_source;
	GtkWidget *toplevel;
} BookRemovedClosure;

static void
addressbook_view_init (AddressbookView *view)
{
	AddressbookViewPrivate *priv;
	GtkWidget *selector_scrolled_window;
	AtkObject *a11y;

	priv->menu = eab_menu_new("org.gnome.evolution.addressbook.view");

	g_signal_connect (priv->folder_view_control, "activate",
			  G_CALLBACK (control_activate_cb), view);

	load_uri_for_selection (E_SOURCE_SELECTOR (priv->selector), view, TRUE);
}

static void
destroy_editor (char *key,
		gpointer value,
		gpointer nada)
{
	EditorUidClosure *closure = value;

	g_object_weak_unref (G_OBJECT (closure->editor),
			     editor_weak_notify, closure);

	gtk_widget_destroy (GTK_WIDGET (closure->editor));
}

void
addressbook_view_edit_contact (AddressbookView* view,
			       const char* source_uid,
			       const char* contact_uid)
{
	AddressbookViewPrivate *priv = view->priv;

	ESource* source = NULL;
	EContact* contact = NULL;
	EBook* book = NULL;

	if (!source_uid || !contact_uid)
		return;

	source = e_source_list_peek_source_by_uid (priv->source_list, source_uid);
	if (!source)
		return;

	/* FIXME: Can I unref this book? */
	book = e_book_new (source, NULL);
	if (!book)
		return;

	if (!e_book_open (book, TRUE, NULL)) {
		g_object_unref (book);
		return;
	}

	e_book_get_contact (book, contact_uid, &contact, NULL);

	if (!contact) {
		g_object_unref (book);
		return;
	}
	eab_show_contact_editor (book, contact, FALSE, FALSE);
	g_object_unref (contact);
	g_object_unref (book);
}
