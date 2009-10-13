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
 *		Chris Lahey <clahey@ximian.com>
 *      Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include <table/e-table-scrolled.h>
#include <table/e-table-model.h>
#include <table/e-cell-date.h>
#include <misc/e-gui-utils.h>
#include <widgets/menus/gal-view-factory-etable.h>
#include <filter/rule-editor.h>
#include <widgets/menus/gal-view-etable.h>
#include <shell/e-shell-sidebar.h>

#include "addressbook/printing/e-contact-print.h"
#include "ea-addressbook.h"

#include "e-util/e-print.h"
#include "e-util/e-util.h"
#include "libedataserver/e-sexp.h"
#include <libedataserver/e-categories.h>

#include "gal-view-minicard.h"
#include "gal-view-factory-minicard.h"

#include "e-addressbook-view.h"
#include "e-addressbook-model.h"
#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include "e-addressbook-table-adapter.h"
#include "eab-contact-merging.h"

#include "e-util/e-error.h"
#include "e-util/e-util-private.h"

#include <gdk/gdkkeysyms.h>
#include <ctype.h>
#include <string.h>

#define E_ADDRESSBOOK_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ADDRESSBOOK_VIEW, EAddressbookViewPrivate))

#define d(x)

static void status_message     (EAddressbookView *view, const gchar *status);
static void search_result      (EAddressbookView *view, EBookViewStatus status);
static void folder_bar_message (EAddressbookView *view, const gchar *status);
static void stop_state_changed (GtkObject *object, EAddressbookView *view);
static void backend_died       (EAddressbookView *view);

static void command_state_change (EAddressbookView *view);

struct _EAddressbookViewPrivate {
	gpointer shell_view;  /* weak pointer */

	EAddressbookModel *model;
	EActivity *activity;

	GList *clipboard_contacts;
	ESource *source;

	GObject *object;
	GtkWidget *widget;

	GalViewInstance *view_instance;

	GtkWidget *invisible;
};

enum {
	PROP_0,
	PROP_MODEL,
	PROP_SHELL_VIEW,
	PROP_SOURCE
};

enum {
	OPEN_CONTACT,
	POPUP_EVENT,
	COMMAND_STATE_CHANGE,
	SELECTION_CHANGE,
	LAST_SIGNAL
};

enum {
	DND_TARGET_TYPE_SOURCE_VCARD,
	DND_TARGET_TYPE_VCARD
};

static GtkTargetEntry drag_types[] = {
	{ (gchar *) "text/x-source-vcard", 0, DND_TARGET_TYPE_SOURCE_VCARD },
	{ (gchar *) "text/x-vcard", 0, DND_TARGET_TYPE_VCARD }
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];
static GdkAtom clipboard_atom = GDK_NONE;

static void
addressbook_view_emit_open_contact (EAddressbookView *view,
                                    EContact *contact,
                                    gboolean is_new_contact)
{
	g_signal_emit (view, signals[OPEN_CONTACT], 0, contact, is_new_contact);
}

static void
addressbook_view_emit_popup_event (EAddressbookView *view,
                                   GdkEvent *event)
{
	g_signal_emit (view, signals[POPUP_EVENT], 0, event);
}

static void
addressbook_view_emit_selection_change (EAddressbookView *view)
{
	g_signal_emit (view, signals[SELECTION_CHANGE], 0);
}

static void
addressbook_view_open_contact (EAddressbookView *view,
                               EContact *contact)
{
	addressbook_view_emit_open_contact (view, contact, FALSE);
}

static void
addressbook_view_create_contact (EAddressbookView *view)
{
	EContact *contact;

	contact = e_contact_new ();
	addressbook_view_emit_open_contact (view, contact, TRUE);
	g_object_unref (contact);
}

static void
addressbook_view_create_contact_list (EAddressbookView *view)
{
	EContact *contact;

	contact = e_contact_new ();
	e_contact_set (contact, E_CONTACT_IS_LIST, GINT_TO_POINTER (TRUE));
	addressbook_view_emit_open_contact (view, contact, TRUE);
	g_object_unref (contact);
}

static void
table_double_click (ETableScrolled *table,
                    gint row,
                    gint col,
                    GdkEvent *event,
                    EAddressbookView *view)
{
	EAddressbookModel *model;
	EContact *contact;

	if (!E_IS_ADDRESSBOOK_TABLE_ADAPTER (view->priv->object))
		return;

	model = e_addressbook_view_get_model (view);
	contact = e_addressbook_model_get_contact (model, row);
	addressbook_view_emit_open_contact (view, contact, FALSE);
	g_object_unref (contact);
}

static gint
table_right_click (ETableScrolled *table,
                   gint row,
                   gint col,
                   GdkEvent *event,
                   EAddressbookView *view)
{
	addressbook_view_emit_popup_event (view, event);

	return TRUE;
}

static gint
table_white_space_event (ETableScrolled *table,
                         GdkEvent *event,
                         EAddressbookView *view)
{
	gint button = ((GdkEventButton *) event)->button;

	if (event->type == GDK_BUTTON_PRESS && button == 3) {
		addressbook_view_emit_popup_event (view, event);
		return TRUE;
	}

	return FALSE;
}

static void
table_drag_data_get (ETable *table,
                     gint row,
                     gint col,
                     GdkDragContext *context,
                     GtkSelectionData *selection_data,
                     guint info,
                     guint time,
                     gpointer user_data)
{
	EAddressbookView *view = user_data;
	EAddressbookModel *model;
	EBook *book;
	GList *contact_list;
	gchar *value;

	if (!E_IS_ADDRESSBOOK_TABLE_ADAPTER (view->priv->object))
		return;

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_book (model);

	contact_list = e_addressbook_view_get_selected (view);

	switch (info) {
		case DND_TARGET_TYPE_VCARD:
			value = eab_contact_list_to_string (contact_list);

			gtk_selection_data_set (
				selection_data, selection_data->target,
				8, (guchar *)value, strlen (value));

			g_free (value);
			break;

		case DND_TARGET_TYPE_SOURCE_VCARD:
			value = eab_book_and_contact_list_to_string (
				book, contact_list);

			gtk_selection_data_set (
				selection_data, selection_data->target,
				8, (guchar *)value, strlen (value));

			g_free (value);
			break;
	}

	g_list_foreach (contact_list, (GFunc) g_object_unref, NULL);
	g_list_free (contact_list);
}

static void
addressbook_view_create_table_view (EAddressbookView *view)
{
	ETableModel *adapter;
	ETableExtras *extras;
	ECell *cell;
	ETable *table;
	GtkWidget *widget;
	gchar *etspecfile;

	adapter = eab_table_adapter_new (view->priv->model);

	extras = e_table_extras_new ();

	/* Set proper format component for a default 'date' cell renderer. */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "addressbook");

	/* Here we create the table.  We give it the three pieces of
	   the table we've created, the header, the model, and the
	   initial layout.  It does the rest.  */
	etspecfile = g_build_filename (
		EVOLUTION_ETSPECDIR, "e-addressbook-view.etspec", NULL);
	widget = e_table_scrolled_new_from_spec_file (
		adapter, extras, etspecfile, NULL);
	table = E_TABLE (E_TABLE_SCROLLED (widget)->table);
	g_free (etspecfile);

	view->priv->object = G_OBJECT (adapter);
	view->priv->widget = widget;

	g_signal_connect (
		table, "double_click",
		G_CALLBACK(table_double_click), view);
	g_signal_connect (
		table, "right_click",
		G_CALLBACK(table_right_click), view);
	g_signal_connect (
		table, "white_space_event",
		G_CALLBACK(table_white_space_event), view);
	g_signal_connect_swapped (
		table, "selection_change",
		G_CALLBACK (addressbook_view_emit_selection_change), view);

	e_table_drag_source_set (
		table, GDK_BUTTON1_MASK,
		drag_types, G_N_ELEMENTS (drag_types),
		GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect (
		table, "table_drag_data_get",
		G_CALLBACK (table_drag_data_get), view);

	gtk_box_pack_start (GTK_BOX (view), widget, TRUE, TRUE, 0);

	gtk_widget_show (widget);
}

static void
addressbook_view_create_minicard_view (EAddressbookView *view)
{
	GtkWidget *scrolled_window;
	GtkWidget *minicard_view;
	EAddressbookReflowAdapter *adapter;

	adapter = E_ADDRESSBOOK_REFLOW_ADAPTER (
		e_addressbook_reflow_adapter_new (view->priv->model));
	minicard_view = e_minicard_view_widget_new (adapter);

	g_signal_connect_swapped (
		adapter, "open-contact",
		G_CALLBACK (addressbook_view_open_contact), view);

	g_signal_connect_swapped (
		minicard_view, "create-contact",
		G_CALLBACK (addressbook_view_create_contact), view);

	g_signal_connect_swapped (
		minicard_view, "create-contact-list",
		G_CALLBACK (addressbook_view_create_contact_list), view);

	g_signal_connect_swapped (
		minicard_view, "selection_change",
		G_CALLBACK (addressbook_view_emit_selection_change), view);

	g_signal_connect_swapped (
		minicard_view, "right_click",
		G_CALLBACK (addressbook_view_emit_popup_event), view);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	view->priv->object = G_OBJECT (minicard_view);
	view->priv->widget = scrolled_window;

	gtk_container_add (GTK_CONTAINER (scrolled_window), minicard_view);
	gtk_widget_show (minicard_view);

	gtk_widget_show_all (scrolled_window);

	gtk_box_pack_start (GTK_BOX (view), scrolled_window, TRUE, TRUE, 0);

	e_reflow_model_changed (E_REFLOW_MODEL (adapter));
}

static void
addressbook_view_display_view_cb (EAddressbookView *view,
                                  GalView *gal_view)
{
	if (view->priv->widget != NULL) {
		gtk_container_remove (
			GTK_CONTAINER (view),
			view->priv->widget);
		view->priv->widget = NULL;
	}
	view->priv->object = NULL;

	if (GAL_IS_VIEW_ETABLE (gal_view)) {
		addressbook_view_create_table_view (view);
		gal_view_etable_attach_table (
			GAL_VIEW_ETABLE (gal_view),
			e_table_scrolled_get_table (
			E_TABLE_SCROLLED (view->priv->widget)));
	}
	else if (GAL_IS_VIEW_MINICARD (gal_view)) {
		addressbook_view_create_minicard_view (view);
		gal_view_minicard_attach (
			GAL_VIEW_MINICARD (gal_view), view);
	}

	command_state_change (view);
}

static void
addressbook_view_selection_get_cb (EAddressbookView *view,
                                   GtkSelectionData *selection_data,
                                   guint info,
                                   guint time_stamp)
{
	gchar *string;

	string = eab_contact_list_to_string (view->priv->clipboard_contacts);

	gtk_selection_data_set (
		selection_data, GDK_SELECTION_TYPE_STRING,
		8, (guchar *) string, strlen (string));

	g_free (string);
}

static void
addressbook_view_selection_clear_event_cb (EAddressbookView *view,
                                           GdkEventSelection *event)
{
	GList *list;

	list = view->priv->clipboard_contacts;
	view->priv->clipboard_contacts = NULL;

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
addressbook_view_selection_received_cb (EAddressbookView *view,
                                        GtkSelectionData *selection_data,
                                        guint time)
{
	EAddressbookModel *model;
	GList *list, *iter;
	EBook *book;

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_book (model);

	if (selection_data->length <= 0)
		return;

	if (selection_data->type != GDK_SELECTION_TYPE_STRING)
		return;

	if (selection_data->data[selection_data->length - 1] != 0) {
		gchar *string;

		string = g_malloc0 (selection_data->length + 1);
		memcpy (string, selection_data->data, selection_data->length);
		list = eab_contact_list_from_string (string);
		g_free (string);
	} else
		list = eab_contact_list_from_string (
			(gchar *) selection_data->data);

	for (iter = list; iter != NULL; iter = iter->next) {
		EContact *contact = iter->data;

		/* XXX NULL for a callback /sigh */
		eab_merging_book_add_contact (
			book, contact, NULL /* XXX */, NULL);
	}

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

static void
addressbook_view_set_shell_view (EAddressbookView *view,
                                 EShellView *shell_view)
{
	g_return_if_fail (view->priv->shell_view == NULL);

	view->priv->shell_view = shell_view;

	g_object_add_weak_pointer (
		G_OBJECT (shell_view),
		&view->priv->shell_view);
}

static void
addressbook_view_set_source (EAddressbookView *view,
                             ESource *source)
{
	g_return_if_fail (view->priv->source == NULL);

	view->priv->source = g_object_ref (source);
}

static void
addressbook_view_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SHELL_VIEW:
			addressbook_view_set_shell_view (
				E_ADDRESSBOOK_VIEW (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE:
			addressbook_view_set_source (
				E_ADDRESSBOOK_VIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
addressbook_view_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MODEL:
			g_value_set_object (
				value, e_addressbook_view_get_model (
				E_ADDRESSBOOK_VIEW (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value, e_addressbook_view_get_shell_view (
				E_ADDRESSBOOK_VIEW (object)));
			return;

		case PROP_SOURCE:
			g_value_set_object (
				value, e_addressbook_view_get_source (
				E_ADDRESSBOOK_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
addressbook_view_dispose (GObject *object)
{
	EAddressbookViewPrivate *priv;

	priv = E_ADDRESSBOOK_VIEW_GET_PRIVATE (object);

	if (priv->shell_view != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->shell_view),
			&priv->shell_view);
		priv->shell_view = NULL;
	}

	if (priv->model != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->model, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->model);
		priv->model = NULL;
	}

	if (priv->activity != NULL) {
		/* XXX Activity is not cancellable. */
		e_activity_complete (priv->activity);
		g_object_unref (priv->activity);
		priv->activity = NULL;
	}

	if (priv->invisible != NULL) {
		gtk_widget_destroy (priv->invisible);
		priv->invisible = NULL;
	}

	if (priv->source != NULL) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}

	if (priv->view_instance != NULL) {
		g_object_unref (priv->view_instance);
		priv->view_instance = NULL;
	}

	g_list_foreach (
		priv->clipboard_contacts,
		(GFunc) g_object_unref, NULL);
	g_list_free (priv->clipboard_contacts);
	priv->clipboard_contacts = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
addressbook_view_constructed (GObject *object)
{
	EAddressbookView *view = E_ADDRESSBOOK_VIEW (object);
	GalViewInstance *view_instance;
	EShellView *shell_view;
	ESource *source;
	gchar *uri;

	shell_view = e_addressbook_view_get_shell_view (view);
	source = e_addressbook_view_get_source (view);
	uri = e_source_get_uri (source);

	view_instance = e_shell_view_new_view_instance (shell_view, uri);
	g_signal_connect_swapped (
		view_instance, "display-view",
		G_CALLBACK (addressbook_view_display_view_cb), view);
	gal_view_instance_load (view_instance);
	view->priv->view_instance = view_instance;

	g_free (uri);
}

static void
addressbook_view_class_init (EAddressbookViewClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAddressbookViewPrivate));

	object_class = G_OBJECT_CLASS(class);
	object_class->set_property = addressbook_view_set_property;
	object_class->get_property = addressbook_view_get_property;
	object_class->dispose = addressbook_view_dispose;
	object_class->constructed = addressbook_view_constructed;

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			_("Model"),
			NULL,
			E_TYPE_ADDRESSBOOK_MODEL,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			_("Shell View"),
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			_("Source"),
			NULL,
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	signals[OPEN_CONTACT] = g_signal_new (
		"open-contact",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookViewClass, open_contact),
		NULL, NULL,
		e_marshal_VOID__OBJECT_BOOLEAN,
		G_TYPE_NONE, 2,
		E_TYPE_CONTACT,
		G_TYPE_BOOLEAN);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookViewClass, popup_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[COMMAND_STATE_CHANGE] = g_signal_new (
		"command-state-change",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookViewClass, command_state_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SELECTION_CHANGE] = g_signal_new (
		"selection-change",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAddressbookViewClass, selection_change),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	if (clipboard_atom == NULL)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);

	/* init the accessibility support for e_addressbook_view */
	eab_view_a11y_init ();
}

static void
addressbook_view_init (EAddressbookView *view)
{
	view->priv = E_ADDRESSBOOK_VIEW_GET_PRIVATE (view);

	view->priv->model = e_addressbook_model_new ();

	view->priv->invisible = gtk_invisible_new ();

	gtk_selection_add_target (
		view->priv->invisible, clipboard_atom,
		GDK_SELECTION_TYPE_STRING, 0);

	g_signal_connect_swapped (
		view->priv->invisible, "selection-get",
		G_CALLBACK (addressbook_view_selection_get_cb), view);
	g_signal_connect_swapped (
		view->priv->invisible, "selection-clear-event",
		G_CALLBACK (addressbook_view_selection_clear_event_cb), view);
	g_signal_connect_swapped (
		view->priv->invisible, "selection-received",
		G_CALLBACK (addressbook_view_selection_received_cb), view);
}

GType
e_addressbook_view_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info =  {
			sizeof (EAddressbookViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) addressbook_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAddressbookView),
			0,     /* n_preallocs */
			(GInstanceInitFunc) addressbook_view_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_VBOX, "EAddressbookView", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_addressbook_view_new (EShellView *shell_view,
                        ESource *source)
{
	GtkWidget *widget;
	EAddressbookView *view;

	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	widget = g_object_new (
		E_TYPE_ADDRESSBOOK_VIEW, "shell-view",
		shell_view, "source", source, NULL);

	view = E_ADDRESSBOOK_VIEW (widget);

	g_signal_connect_swapped (
		view->priv->model, "status_message",
		G_CALLBACK (status_message), view);
	g_signal_connect_swapped (
		view->priv->model, "search_result",
		G_CALLBACK (search_result), view);
	g_signal_connect_swapped (
		view->priv->model, "folder_bar_message",
		G_CALLBACK (folder_bar_message), view);
	g_signal_connect (view->priv->model, "stop_state_changed",
			  G_CALLBACK (stop_state_changed), view);
	g_signal_connect_swapped (
		view->priv->model, "writable-status",
		G_CALLBACK (command_state_change), view);
	g_signal_connect_swapped (
		view->priv->model, "backend_died",
		G_CALLBACK (backend_died), view);

	return widget;
}

EAddressbookModel *
e_addressbook_view_get_model (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->model;
}

GalViewInstance *
e_addressbook_view_get_view_instance (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->view_instance;
}

GObject *
e_addressbook_view_get_view_object (EAddressbookView *view)
{
	/* XXX Find a more descriptive name for this. */

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->object;
}

GtkWidget *
e_addressbook_view_get_view_widget (EAddressbookView *view)
{
	/* XXX Find a more descriptive name for this. */

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->widget;
}

/* Helper for e_addressbook_view_get_selected() */
static void
add_to_list (gint model_row, gpointer closure)
{
	GList **list = closure;
	*list = g_list_prepend (*list, GINT_TO_POINTER (model_row));
}

GList *
e_addressbook_view_get_selected (EAddressbookView *view)
{
	GList *list, *iter;
	ESelectionModel *selection;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	list = NULL;
	selection = e_addressbook_view_get_selection_model (view);
	e_selection_model_foreach (selection, add_to_list, &list);

	for (iter = list; iter != NULL; iter = iter->next)
		iter->data = e_addressbook_model_get_contact (
			view->priv->model, GPOINTER_TO_INT (iter->data));
	list = g_list_reverse (list);

	return list;
}

ESelectionModel *
e_addressbook_view_get_selection_model (EAddressbookView *view)
{
	GalView *gal_view;
	GalViewInstance *view_instance;
	ESelectionModel *model = NULL;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	view_instance = e_addressbook_view_get_view_instance (view);
	gal_view = gal_view_instance_get_current_view (view_instance);

	if (GAL_IS_VIEW_ETABLE (gal_view)) {
		ETableScrolled *scrolled_table;
		ETable *table;

		scrolled_table = E_TABLE_SCROLLED (view->priv->widget);
		table = e_table_scrolled_get_table (scrolled_table);

		model = e_table_get_selection_model (table);

	} else if (GAL_IS_VIEW_MINICARD (gal_view)) {
		EMinicardViewWidget *widget;

		widget = E_MINICARD_VIEW_WIDGET (view->priv->object);

		model = e_minicard_view_widget_get_selection_model (widget);
	}

	return model;
}

EShellView *
e_addressbook_view_get_shell_view (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->shell_view;
}

ESource *
e_addressbook_view_get_source (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->source;
}

static void
status_message (EAddressbookView *view,
                const gchar *status)
{
	EActivity *activity;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	activity = view->priv->activity;
	shell_view = e_addressbook_view_get_shell_view (view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	if (status == NULL || *status == '\0') {
		if (activity != NULL) {
			e_activity_complete (activity);
			g_object_unref (activity);
			view->priv->activity = NULL;
		}

	} else if (activity == NULL) {
		activity = e_activity_new (status);
		view->priv->activity = activity;
		e_shell_backend_add_activity (shell_backend, activity);

	} else
		e_activity_set_primary_text (activity, status);
}

static void
search_result (EAddressbookView *view,
               EBookViewStatus status)
{
	EShellView *shell_view;
	EShellWindow *shell_window;

	shell_view = e_addressbook_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	eab_search_result_dialog (GTK_WIDGET (shell_window), status);
}

static void
folder_bar_message (EAddressbookView *view,
                    const gchar *message)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	const gchar *name;

	shell_view = e_addressbook_view_get_shell_view (view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	if (view->priv->source == NULL)
		return;

	name = e_source_peek_name (view->priv->source);
	e_shell_sidebar_set_primary_text (shell_sidebar, name);
	e_shell_sidebar_set_secondary_text (shell_sidebar, message);
}

static void
stop_state_changed (GtkObject *object, EAddressbookView *view)
{
	command_state_change (view);
}

static void
command_state_change (EAddressbookView *view)
{
	g_signal_emit (view, signals[COMMAND_STATE_CHANGE], 0);
}

static void
backend_died (EAddressbookView *view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EAddressbookModel *model;
	EBook *book;

	shell_view = e_addressbook_view_get_shell_view (view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_book (model);

	e_error_run (
		GTK_WINDOW (shell_window),
		"addressbook:backend-died",
		e_book_get_uri (book), NULL);
}

static void
contact_print_button_draw_page (GtkPrintOperation *operation,
                                GtkPrintContext *context,
                                gint page_nr,
                                EPrintable *printable)
{
	GtkPageSetup *setup;
	gdouble top_margin;
	cairo_t *cr;

	setup = gtk_print_context_get_page_setup (context);
	top_margin = gtk_page_setup_get_top_margin (setup, GTK_UNIT_POINTS);

	cr = gtk_print_context_get_cairo_context (context);

	e_printable_reset (printable);

	while (e_printable_data_left (printable)) {
		cairo_save (cr);
		e_printable_print_page (
			printable, context, 6.5 * 72, top_margin + 10, TRUE);
		cairo_restore (cr);
	}
}

static void
e_contact_print_button (EPrintable *printable, GtkPrintOperationAction action)
{
	GtkPrintOperation *operation;

	operation = e_print_operation_new ();
	gtk_print_operation_set_n_pages (operation, 1);

	g_signal_connect (
		operation, "draw_page",
		G_CALLBACK (contact_print_button_draw_page), printable);

	gtk_print_operation_run (operation, action, NULL, NULL);

	g_object_unref (operation);
}

void
e_addressbook_view_print (EAddressbookView *view,
                          GtkPrintOperationAction action)
{
	GalView *gal_view;
	GalViewInstance *view_instance;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	view_instance = e_addressbook_view_get_view_instance (view);
	gal_view = gal_view_instance_get_current_view (view_instance);

	if (GAL_IS_VIEW_MINICARD (gal_view)) {
		EAddressbookModel *model;
		EBook *book;
		EBookQuery *query;
		gchar *query_string;
		GList *contact_list;

		model = e_addressbook_view_get_model (view);
		book = e_addressbook_model_get_book (model);
		query_string = e_addressbook_model_get_query (model);

		if (query_string != NULL)
			query = e_book_query_from_string (query_string);
		else
			query = NULL;
		g_free (query_string);

		contact_list = e_addressbook_view_get_selected (view);
		e_contact_print (book, query, contact_list, action);
		g_list_foreach (contact_list, (GFunc) g_object_unref, NULL);
		g_list_free (contact_list);

		if (query != NULL)
			e_book_query_unref (query);

	} else if (GAL_IS_VIEW_ETABLE (gal_view)) {
		EPrintable *printable;
		ETable *table;

		g_object_get (view->priv->widget, "table", &table, NULL);
		printable = e_table_get_printable (table);
		g_object_ref_sink (printable);
		g_object_unref (table);

		e_contact_print_button (printable, action);

		g_object_unref (printable);
	}
}

/* callback function to handle removal of contacts for
 * which a user doesnt have write permission
 */
static void
delete_contacts_cb (EBook *book,  EBookStatus status,  gpointer closure)
{
	switch (status) {
		case E_BOOK_ERROR_OK :
		case E_BOOK_ERROR_CANCELLED :
			break;
		case E_BOOK_ERROR_PERMISSION_DENIED :
			e_error_run (e_shell_get_active_window (NULL), "addressbook:contact-delete-error-perm", NULL);
			break;
		default :
			/* Unknown error */
			eab_error_dialog (_("Failed to delete contact"), status);
			break;
	}
}

static gboolean
addressbook_view_confirm_delete (GtkWindow *parent,
                                 gboolean plural,
                                 gboolean is_list,
                                 const gchar *name)
{
	GtkWidget *dialog;
	gchar *message;
	gint response;

	if (is_list) {
		if (plural) {
			message = g_strdup (
				_("Are you sure you want to "
				  "delete these contact lists?"));
		} else if (name == NULL) {
			message = g_strdup (
				_("Are you sure you want to "
				  "delete this contact list?"));
		} else {
			message = g_strdup_printf (
				_("Are you sure you want to delete "
				  "this contact list (%s)?"), name);
		}
	} else {
		if (plural) {
			message = g_strdup (
				_("Are you sure you want to "
				  "delete these contacts?"));
		} else if (name == NULL) {
			message = g_strdup (
				_("Are you sure you want to "
				  "delete this contact?"));
		} else {
			message = g_strdup_printf (
				_("Are you sure you want to delete "
				  "this contact (%s)?"), name);
		}
	}

	dialog = gtk_message_dialog_new (
		parent, 0, GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_NONE, "%s", message);
	gtk_dialog_add_buttons (
		GTK_DIALOG (dialog),
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT,
		NULL);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (message);

	return (response == GTK_RESPONSE_ACCEPT);
}

void
e_addressbook_view_delete_selection(EAddressbookView *view, gboolean is_delete)
{
	GList *list, *l;
	gboolean plural = FALSE, is_list = FALSE;
	EContact *contact;
	ETable *etable = NULL;
	EAddressbookModel *model;
	EBook *book;
	EMinicardView *card_view;
	ESelectionModel *selection_model = NULL;
	GalViewInstance *view_instance;
	GalView *gal_view;
	gchar *name = NULL;
	gint row = 0, select;

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_book (model);

	view_instance = e_addressbook_view_get_view_instance (view);
	gal_view = gal_view_instance_get_current_view (view_instance);

	list = e_addressbook_view_get_selected (view);
	contact = list->data;

	if (g_list_next(list))
		plural = TRUE;
	else
		name = e_contact_get (contact, E_CONTACT_FILE_AS);

	if (e_contact_get (contact, E_CONTACT_IS_LIST))
		is_list = TRUE;

	if (GAL_IS_VIEW_MINICARD (gal_view)) {
		card_view = e_minicard_view_widget_get_view (E_MINICARD_VIEW_WIDGET(view->priv->object));
		selection_model = e_addressbook_view_get_selection_model (view);
		row = e_selection_model_cursor_row (selection_model);
	}

	else if (GAL_IS_VIEW_ETABLE (gal_view)) {
		etable = e_table_scrolled_get_table (
			E_TABLE_SCROLLED(view->priv->widget));
		row = e_table_get_cursor_row (E_TABLE (etable));
	}

	/* confirm delete */
	if (is_delete && !addressbook_view_confirm_delete (
			GTK_WINDOW (gtk_widget_get_toplevel (
			view->priv->widget)), plural, is_list, name)) {
		g_free (name);
		g_list_foreach (list, (GFunc) g_object_unref, NULL);
		g_list_free (list);
		return;
	}

	if (e_book_check_static_capability (book, "bulk-remove")) {
		GList *ids = NULL;

		for (l=list;l;l=g_list_next(l)) {
			contact = l->data;

			ids = g_list_prepend (ids, (gchar *)e_contact_get_const (contact, E_CONTACT_UID));
		}

		/* Remove the cards all at once. */
		e_book_async_remove_contacts (book,
					      ids,
					      delete_contacts_cb,
					      NULL);

		g_list_free (ids);
	}
	else {
		for (l=list;l;l=g_list_next(l)) {
			contact = l->data;
			/* Remove the card. */
			e_book_async_remove_contact (book,
						     contact,
						     delete_contacts_cb,
						     NULL);
		}
	}

	/* Sets the cursor, at the row after the deleted row */
	if (GAL_IS_VIEW_MINICARD (gal_view) && row != 0) {
		select = e_sorter_model_to_sorted (selection_model->sorter, row);

	/* Sets the cursor, before the deleted row if its the last row */
		if (select == e_selection_model_row_count (selection_model) - 1)
			select = select - 1;
		else
			select = select + 1;

		row = e_sorter_sorted_to_model (selection_model->sorter, select);
		e_selection_model_cursor_changed (selection_model, row, 0);
	}

	/* Sets the cursor, at the row after the deleted row */
	else if (GAL_IS_VIEW_ETABLE (gal_view) && row != 0) {
		select = e_table_model_to_view_row (E_TABLE (etable), row);

	/* Sets the cursor, before the deleted row if its the last row */
		if (select == e_table_model_row_count (E_TABLE(etable)->model) - 1)
			select = select - 1;
		else
			select = select + 1;

		row = e_table_view_to_model_row (E_TABLE (etable), select);
		e_table_set_cursor_row (E_TABLE (etable), row);
	}
	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

void
e_addressbook_view_save_as (EAddressbookView *view,
                            gboolean all)
{
	GList *list = NULL;
	EBook *book;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	book = e_addressbook_model_get_book (view->priv->model);

	if (all) {
		EBookQuery *query;

		query = e_book_query_any_field_contains ("");
		e_book_get_contacts (book, query, &list, NULL);
		e_book_query_unref (query);
	} else
		list = e_addressbook_view_get_selected (view);

	if (list != NULL) {
		eab_contact_list_save (_("Save as vCard..."), list, NULL);
		g_list_foreach (list, (GFunc) g_object_unref, NULL);
		g_list_free (list);
	}
}

void
e_addressbook_view_view (EAddressbookView *view)
{
	EAddressbookModel *model;
	EBook *book;
	GList *list, *iter;
	gboolean editable;
	gint response;
	guint length;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	model = e_addressbook_view_get_model (view);
	book = e_addressbook_model_get_book (model);
	editable = e_addressbook_model_get_editable (model);

	list = e_addressbook_view_get_selected (view);
	length = g_list_length (list);
	response = GTK_RESPONSE_YES;

	if (length > 5) {
		GtkWidget *dialog;

		/* XXX Use e_error_new(). */
		/* XXX Provide a parent window. */
		dialog = gtk_message_dialog_new (
			NULL, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
			_("Opening %d contacts will open %d new windows as "
			  "well.\nDo you really want to display all of these "
			  "contacts?"), length, length);
		gtk_dialog_add_buttons (
			GTK_DIALOG (dialog),
			_("_Don't Display"), GTK_RESPONSE_NO,
			_("Display _All Contacts"), GTK_RESPONSE_YES,
			NULL);
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	if (response == GTK_RESPONSE_YES)
		for (iter = list; iter != NULL; iter = iter->next)
			addressbook_view_emit_open_contact (
				view, iter->data, FALSE);

	g_list_foreach (list, (GFunc) g_object_unref, NULL);
	g_list_free (list);
}

void
e_addressbook_view_cut (EAddressbookView *view)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	e_addressbook_view_copy (view);
	e_addressbook_view_delete_selection (view, FALSE);
}

void
e_addressbook_view_copy (EAddressbookView *view)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	view->priv->clipboard_contacts = e_addressbook_view_get_selected (view);

	gtk_selection_owner_set (
		view->priv->invisible,
		clipboard_atom, GDK_CURRENT_TIME);
}

void
e_addressbook_view_paste (EAddressbookView *view)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	gtk_selection_convert (
		view->priv->invisible, clipboard_atom,
		GDK_SELECTION_TYPE_STRING, GDK_CURRENT_TIME);
}

void
e_addressbook_view_select_all (EAddressbookView *view)
{
	ESelectionModel *model;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	model = e_addressbook_view_get_selection_model (view);
	g_return_if_fail (model);

	e_selection_model_select_all (model);
}

void
e_addressbook_view_show_all (EAddressbookView *view)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	e_addressbook_model_set_query (view->priv->model, "");
}

void
e_addressbook_view_stop (EAddressbookView *view)
{
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	e_addressbook_model_stop (view->priv->model);
}

static void
view_transfer_contacts (EAddressbookView *view, gboolean delete_from_source, gboolean all)
{
	EBook *book;
	GList *contacts = NULL;
	GtkWindow *parent_window;

	book = e_addressbook_model_get_book (view->priv->model);

	if (all) {
		EBookQuery *query = e_book_query_any_field_contains("");
		e_book_get_contacts(book, query, &contacts, NULL);
		e_book_query_unref(query);
	}
	else {
		contacts = e_addressbook_view_get_selected (view);
	}
	parent_window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view)));

	eab_transfer_contacts (book, contacts, delete_from_source, parent_window);
	g_object_unref(book);
}

void
e_addressbook_view_copy_to_folder (EAddressbookView *view, gboolean all)
{
	view_transfer_contacts (view, FALSE, all);
}

void
e_addressbook_view_move_to_folder (EAddressbookView *view, gboolean all)
{
	view_transfer_contacts (view, TRUE, all);
}
