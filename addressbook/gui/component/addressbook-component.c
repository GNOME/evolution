/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-component.c
 *
 * Copyright (C) 2003  Ettore Perazzoli
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
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

/* EPFIXME: Add autocompletion setting.  */


#include <config.h>

#include "addressbook-component.h"
#include "addressbook-migrate.h"

#include "addressbook.h"
#include "addressbook-config.h"

#include "widgets/misc/e-source-selector.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/gui/merging/eab-contact-merging.h"
#include "addressbook/util/eab-book-util.h"


#include "e-task-bar.h"

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtklabel.h>	/* FIXME */
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkstock.h>
#include <gconf/gconf-client.h>
#include <gal/util/e-util.h>

#ifdef ENABLE_SMIME
#include "smime/gui/component.h"
#endif


#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _AddressbookComponentPrivate {
	GConfClient *gconf_client;
	ESourceList *source_list;
	GtkWidget *source_selector;
	char *base_directory;

	EActivityHandler *activity_handler;
};

enum DndTargetType {
	DND_TARGET_TYPE_VCARD_LIST,
};
#define VCARD_TYPE "text/x-vcard"
static GtkTargetEntry drag_types[] = {
	{ VCARD_TYPE, 0, DND_TARGET_TYPE_VCARD_LIST }
};
static gint num_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);

/* Utility functions.  */

static void
load_uri_for_selection (ESourceSelector *selector,
			BonoboControl *view_control)
{
	ESource *selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (selector));

	if (selected_source != NULL) {
		bonobo_control_set_property (view_control, NULL, "source_uid", TC_CORBA_string,
					     e_source_peek_uid (selected_source), NULL);
	}
}

static ESource *
find_first_source (ESourceList *source_list)
{
	GSList *groups, *sources, *l, *m;
			
	groups = e_source_list_peek_groups (source_list);
	for (l = groups; l; l = l->next) {
		ESourceGroup *group = l->data;
				
		sources = e_source_group_peek_sources (group);
		for (m = sources; m; m = m->next) {
			ESource *source = m->data;

			return source;
		}				
	}

	return NULL;
}

static void
save_primary_selection (AddressbookComponent *addressbook_component)
{
	AddressbookComponentPrivate *priv;
	ESource *source;

	priv = addressbook_component->priv;
	
	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->source_selector));
	if (!source)
		return;

	/* Save the selection for next time we start up */
	gconf_client_set_string (priv->gconf_client,
				 "/apps/evolution/addressbook/display/primary_addressbook",
				 e_source_peek_uid (source), NULL);
}

static ESource *
get_primary_source (AddressbookComponent *addressbook_component)
{
	AddressbookComponentPrivate *priv;
	ESource *source;
	char *uid;

	priv = addressbook_component->priv;

	uid = gconf_client_get_string (priv->gconf_client,
				       "/apps/evolution/addressbook/display/primary_addressbook",
				       NULL);
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);
	} else {
		/* Try to create a default if there isn't one */
		source = find_first_source (priv->source_list);
	}

	return source;
}

static void
load_primary_selection (AddressbookComponent *addressbook_component)
{
	AddressbookComponentPrivate *priv;
	ESource *source;

	priv = addressbook_component->priv;

	source = get_primary_source (addressbook_component);
	if (source)
		e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (priv->source_selector), source);
}

/* Folder popup menu callbacks */

static void
add_popup_menu_item (GtkMenu *menu, const char *label, const char *pixmap,
		     GCallback callback, gpointer user_data, gboolean sensitive)
{
	GtkWidget *item, *image;

	if (pixmap) {
		item = gtk_image_menu_item_new_with_label (label);

		/* load the image */
		if (g_file_test (pixmap, G_FILE_TEST_EXISTS))
			image = gtk_image_new_from_file (pixmap);
		else
			image = gtk_image_new_from_stock (pixmap, GTK_ICON_SIZE_MENU);

		if (image) {
			gtk_widget_show (image);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		}
	} else {
		item = gtk_menu_item_new_with_label (label);
	}

	if (callback)
		g_signal_connect (G_OBJECT (item), "activate", callback, user_data);

	if (!sensitive)
		gtk_widget_set_sensitive (item, FALSE);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static void
delete_addressbook_cb (GtkWidget *widget, AddressbookComponent *comp)
{
	ESource *selected_source;
	AddressbookComponentPrivate *priv;
	GtkWidget *dialog;
	EBook  *book;
	gboolean removed = FALSE;
	GError *error = NULL;

	priv = comp->priv;
	
	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->source_selector));
	if (!selected_source)
		return;

	/* Create the confirmation dialog */
	dialog = gtk_message_dialog_new (
		GTK_WINDOW (gtk_widget_get_toplevel (widget)),
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_YES_NO,
		_("Address book '%s' will be removed. Are you sure you want to continue?"),
		e_source_peek_name (selected_source));
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_YES) {
		gtk_widget_destroy (dialog);
		return;
	}

	/* Remove local data */
	book = e_book_new ();
	if (e_book_load_source (book, selected_source, TRUE, &error))
		removed = e_book_remove (book, &error);

	if (removed) {
		/* Remove source */
		if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (priv->source_selector),
							  selected_source))
			e_source_selector_unselect_source (E_SOURCE_SELECTOR (priv->source_selector),
							   selected_source);
		
		e_source_group_remove_source (e_source_peek_group (selected_source), selected_source);

		e_source_list_sync (priv->source_list, NULL);
	} else {
		GtkWidget *error_dialog;

		error_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (widget)),
						       GTK_DIALOG_MODAL,
						       GTK_MESSAGE_ERROR,
						       GTK_BUTTONS_CLOSE,
						       "Error removing address book: %s",
						       error->message);
		gtk_dialog_run (GTK_DIALOG (error_dialog));
		gtk_widget_destroy (error_dialog);

		g_error_free (error);
	}

	g_object_unref (book);
	gtk_widget_destroy (dialog);
}

static void
new_addressbook_cb (GtkWidget *widget, AddressbookComponent *comp)
{
	addressbook_config_create_new_source (gtk_widget_get_toplevel (widget));
}

static void
edit_addressbook_cb (GtkWidget *widget, AddressbookComponent *comp)
{
	AddressbookComponentPrivate *priv;
	ESource *selected_source;

	priv = comp->priv;

	selected_source =
		e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->source_selector));
	if (!selected_source)
		return;

	addressbook_config_edit_source (gtk_widget_get_toplevel (widget), selected_source);
}

/* Callbacks.  */

static void
primary_source_selection_changed_callback (ESourceSelector *selector,
					   BonoboControl *view_control)
{
	load_uri_for_selection (selector, view_control);
	save_primary_selection (addressbook_component_peek ());
}


static void
fill_popup_menu_callback (ESourceSelector *selector, GtkMenu *menu, AddressbookComponent *comp)
{
	gboolean sensitive;
	gboolean local_addressbook;
	ESource *selected_source;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (comp->priv->source_selector));
	sensitive = selected_source ? TRUE : FALSE;

	local_addressbook =  (!strcmp ("system", e_source_peek_relative_uri (selected_source)));
		
	add_popup_menu_item (menu, _("New Address Book"), NULL, G_CALLBACK (new_addressbook_cb), comp, TRUE);
	add_popup_menu_item (menu, _("Delete"), GTK_STOCK_DELETE, G_CALLBACK (delete_addressbook_cb), comp, sensitive && !local_addressbook);
	add_popup_menu_item (menu, _("Properties..."), NULL, G_CALLBACK (edit_addressbook_cb), comp, sensitive);
}

static gboolean
selector_tree_drag_drop (GtkWidget *widget, 
			 GdkDragContext *context, 
			 int x, 
			 int y, 
			 guint time, 
			 AddressbookComponent *component)
{
	GtkTreeViewColumn *column;
	int cell_x;
	int cell_y;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gpointer data;
	
	if (!gtk_tree_view_get_path_at_pos  (GTK_TREE_VIEW (widget), x, y, &path, &column, &cell_x, &cell_y))
		return FALSE;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_model_get (model, &iter, 0, &data, -1);
	
	if (E_IS_SOURCE_GROUP (data)) {
		g_object_unref (data);
		gtk_tree_path_free (path);
		return FALSE;
	}
	
	gtk_tree_path_free (path);
	return TRUE;
}
	
static gboolean
selector_tree_drag_motion (GtkWidget *widget,
			   GdkDragContext *context,
			   int x,
			   int y)
{
	GtkTreePath *path = NULL;
	gpointer data = NULL;
	GtkTreeViewDropPosition pos;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GdkDragAction action;
	
	if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						x, y, &path, &pos))
		goto finish;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	
	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto finish;
	
	gtk_tree_model_get (model, &iter, 0, &data, -1);

	if (E_IS_SOURCE_GROUP (data) || e_source_get_readonly (data))
		goto finish;
	
	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW (widget), path, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
	action = context->suggested_action;

 finish:
	if (path)
		gtk_tree_path_free (path);
	if (data)
		g_object_unref (data);
      
	gdk_drag_status (context, action, time);
	return TRUE;
}

static gboolean 
selector_tree_drag_data_received (GtkWidget *widget, 
				  GdkDragContext *context, 
				  gint x, 
				  gint y, 
				  GtkSelectionData *data,
				  guint info,
				  guint time,
				  gpointer user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;
	gpointer source = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean success = FALSE;

	EBook *book;
	GList *contactlist;
	GList *l;

	if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						x, y, &path, &pos))
		goto finish;
	
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	
	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto finish;
	
	gtk_tree_model_get (model, &iter, 0, &source, -1);

	if (E_IS_SOURCE_GROUP (source) || e_source_get_readonly (source))
		goto finish;
	
	book = e_book_new ();
	if (!book) {
		g_message (G_STRLOC ":Couldn't create EBook.");
		return FALSE;
	}
	e_book_load_source (book, source, TRUE, NULL);
	contactlist = eab_contact_list_from_string (data->data);
	
	for (l = contactlist; l; l = l->next) {
		EContact *contact = l->data;
		
		/* XXX NULL for a callback /sigh */
		if (contact)
			eab_merging_book_add_contact (book, contact, NULL /* XXX */, NULL);
		success = TRUE;
	}
	
	g_list_foreach (contactlist, (GFunc)g_object_unref, NULL);
	g_list_free (contactlist);
	g_object_unref (book);

 finish:
	if (path)
		gtk_tree_path_free (path);
	if (source)
		g_object_unref (source);
		       
	gtk_drag_finish (context, success, context->action == GDK_ACTION_MOVE, time);

	return TRUE;
}	

static void
selector_tree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, gpointer data)
{
	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW (widget), NULL, GTK_TREE_VIEW_DROP_BEFORE);
}

/* Evolution::Component CORBA methods.  */

static void
impl_createControls (PortableServer_Servant servant,
		     Bonobo_Control *corba_sidebar_control,
		     Bonobo_Control *corba_view_control,
		     Bonobo_Control *corba_statusbar_control,
		     CORBA_Environment *ev)
{
	AddressbookComponent *addressbook_component = ADDRESSBOOK_COMPONENT (bonobo_object_from_servant (servant));
	GtkWidget *selector;
	GtkWidget *selector_scrolled_window;
	GtkWidget *statusbar_widget;
	BonoboControl *sidebar_control;
	BonoboControl *view_control;
	BonoboControl *statusbar_control;

	selector = e_source_selector_new (addressbook_component->priv->source_list);

	g_signal_connect (selector, "drag-motion", G_CALLBACK (selector_tree_drag_motion), addressbook_component);
	g_signal_connect (selector, "drag-leave", G_CALLBACK (selector_tree_drag_leave), addressbook_component);
	g_signal_connect (selector, "drag-drop", G_CALLBACK (selector_tree_drag_drop), addressbook_component);
	g_signal_connect (selector, "drag-data-received", G_CALLBACK (selector_tree_drag_data_received), addressbook_component);
	/*
	g_signal_connect (selector, "drag-begin", G_CALLBACK (drag_begin_callback), addressbook_component);
	g_signal_connect (selector, "drag-data-get", G_CALLBACK (drag_data_get_callback), addressbook_component);
	g_signal_connect (selector, "drag-end", G_CALLBACK (drag_end_callback), addressbook_component);
	//gtk_drag_source_set(selector, GDK_BUTTON1_MASK, drag_types, num_drop_types, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	*/

	gtk_drag_dest_set(selector, GTK_DEST_DEFAULT_ALL, drag_types, num_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
	gtk_widget_show (selector);

	addressbook_component->priv->source_selector = selector;

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (selector_scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), selector);
	gtk_widget_show (selector_scrolled_window);

	sidebar_control = bonobo_control_new (selector_scrolled_window);

	view_control = addressbook_new_control ();
	g_signal_connect_object (selector, "primary_selection_changed",
				 G_CALLBACK (primary_source_selection_changed_callback),
				 G_OBJECT (view_control), 0);
	g_signal_connect_object (selector, "fill_popup_menu",
				 G_CALLBACK (fill_popup_menu_callback),
				 G_OBJECT (addressbook_component), 0);

	load_primary_selection (addressbook_component);
	load_uri_for_selection (E_SOURCE_SELECTOR (selector), view_control);

	statusbar_widget = e_task_bar_new ();
	gtk_widget_show (statusbar_widget);
	statusbar_control = bonobo_control_new (statusbar_widget);

	e_activity_handler_attach_task_bar (addressbook_component->priv->activity_handler,
					    E_TASK_BAR (statusbar_widget));

	*corba_sidebar_control = CORBA_Object_duplicate (BONOBO_OBJREF (sidebar_control), ev);
	*corba_view_control = CORBA_Object_duplicate (BONOBO_OBJREF (view_control), ev);
	*corba_statusbar_control = CORBA_Object_duplicate (BONOBO_OBJREF (statusbar_control), ev);
}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 2;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = "contact";
	list->_buffer[0].description = _("New Contact");
	list->_buffer[0].menuDescription = _("_Contact");
	list->_buffer[0].tooltip = _("Create a new contact");
	list->_buffer[0].menuShortcut = 'c';
	list->_buffer[0].iconName = "evolution-contacts-mini.png";

	list->_buffer[1].id = "contact_list";
	list->_buffer[1].description = _("New Contact List");
	list->_buffer[1].menuDescription = _("Contact _List");
	list->_buffer[1].tooltip = _("Create a new contact list");
	list->_buffer[1].menuShortcut = 'l';
	list->_buffer[1].iconName = "contact-list-16.png";

	return list;
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	AddressbookComponent *addressbook_component = ADDRESSBOOK_COMPONENT (bonobo_object_from_servant (servant));
	AddressbookComponentPrivate *priv;
	EBook *book;
	EContact *contact = e_contact_new ();
	ESource *selected_source;
	gchar *uri;

	priv = addressbook_component->priv;

	selected_source = get_primary_source (addressbook_component);
	if (!selected_source) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_Failed, NULL);
		return;
	}

	uri = e_source_get_uri (selected_source);
	if (!uri) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_Failed, NULL);
		return;
	}

	book = e_book_new ();
	if (!e_book_load_uri (book, uri, TRUE, NULL)) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_Failed, NULL);
		g_object_unref (book);
		g_free (uri);
		return;
	}

	contact = e_contact_new ();

	if (!item_type_name) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UnknownType, NULL);
	} else if (!strcmp (item_type_name, "contact")) {
		eab_show_contact_editor (book, contact, TRUE, TRUE);
	} else if (!strcmp (item_type_name, "contact_list")) {
		eab_show_contact_list_editor (book, contact, TRUE, TRUE);
	} else {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UnknownType, NULL);
	}

	g_object_unref (book);
	g_object_unref (contact);
	g_free (uri);
}

static CORBA_boolean
impl_upgradeFromVersion (PortableServer_Servant servant, short major, short minor, short revision, CORBA_Environment *ev)
{
	return addressbook_migrate (addressbook_component_peek (), major, minor, revision);
}

static CORBA_boolean
impl_requestQuit (PortableServer_Servant servant, CORBA_Environment *ev)
{
	return eab_editor_request_close_all ();
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	AddressbookComponentPrivate *priv = ADDRESSBOOK_COMPONENT (object)->priv;

	if (priv->source_selector != NULL) {
		g_object_unref (priv->source_selector);
		priv->source_selector = NULL;
	}

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	if (priv->gconf_client != NULL) {
		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
	}

	if (priv->activity_handler != NULL) {
		g_object_unref (priv->activity_handler);
		priv->activity_handler = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	AddressbookComponentPrivate *priv = ADDRESSBOOK_COMPONENT (object)->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
addressbook_component_class_init (AddressbookComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	epv->createControls          = impl_createControls;
	epv->_get_userCreatableItems = impl__get_userCreatableItems;
	epv->requestCreateItem       = impl_requestCreateItem;
	epv->upgradeFromVersion      = impl_upgradeFromVersion;
	epv->requestQuit             = impl_requestQuit;

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);
}

static void
addressbook_component_init (AddressbookComponent *component)
{
	AddressbookComponentPrivate *priv;

	priv = g_new0 (AddressbookComponentPrivate, 1);

	/* EPFIXME: Should use a custom one instead? */
	priv->gconf_client = gconf_client_get_default ();

	priv->source_list = e_source_list_new_for_gconf (priv->gconf_client,
							 "/apps/evolution/addressbook/sources");

	priv->activity_handler = e_activity_handler_new ();
	priv->base_directory = g_build_filename (g_get_home_dir (), ".evolution", NULL);

	component->priv = priv;

#ifdef ENABLE_SMIME
	smime_component_init ();
#endif
}


/* Public API.  */

AddressbookComponent *
addressbook_component_peek (void)
{
	static AddressbookComponent *component = NULL;

	if (component == NULL)
		component = g_object_new (addressbook_component_get_type (), NULL);

	return component;
}

GConfClient*
addressbook_component_peek_gconf_client (AddressbookComponent *component)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_COMPONENT (component), NULL);

	return component->priv->gconf_client;
}

const char *
addressbook_component_peek_base_directory (AddressbookComponent *component)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_COMPONENT (component), NULL);

	return component->priv->base_directory;
}

ESourceList *
addressbook_component_peek_source_list (AddressbookComponent *component)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_COMPONENT (component), NULL);

	return component->priv->source_list;
}


EActivityHandler *
addressbook_component_peek_activity_handler (AddressbookComponent *component)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_COMPONENT (component), NULL);

	return component->priv->activity_handler;
}


BONOBO_TYPE_FUNC_FULL (AddressbookComponent, GNOME_Evolution_Component, PARENT_TYPE, addressbook_component)
