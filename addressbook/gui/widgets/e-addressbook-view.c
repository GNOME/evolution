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
 *		Chris Lahey <clahey@ximian.com>
 *      Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-addressbook-view.h"

#include "e-util/e-util.h"
#include "shell/e-shell-sidebar.h"

#include "addressbook/printing/e-contact-print.h"
#include "ea-addressbook.h"

#include "gal-view-minicard.h"

#include "e-addressbook-model.h"
#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include "e-addressbook-table-adapter.h"
#include "eab-contact-merging.h"

#define E_ADDRESSBOOK_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ADDRESSBOOK_VIEW, EAddressbookViewPrivate))

#define d(x)

static void	status_message			(EAddressbookView *view,
						 const gchar *status, gint percent);
static void	search_result			(EAddressbookView *view,
						 const GError *error);
static void	folder_bar_message		(EAddressbookView *view,
						 const gchar *status);
static void	stop_state_changed		(GObject *object,
						 EAddressbookView *view);
static void	command_state_change		(EAddressbookView *view);

struct _EAddressbookViewPrivate {
	gpointer shell_view;  /* weak pointer */

	EAddressbookModel *model;
	EActivity *activity;

	ESource *source;

	GObject *object;

	GalViewInstance *view_instance;

	/* stored search setup for this view */
	gint filter_id;
	gchar *search_text;
	gint search_id;
	EFilterRule *advanced_search;

	GtkTargetList *copy_target_list;
	GtkTargetList *paste_target_list;
};

enum {
	PROP_0,
	PROP_COPY_TARGET_LIST,
	PROP_MODEL,
	PROP_PASTE_TARGET_LIST,
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

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_addressbook_view_selectable_init
					(ESelectableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EAddressbookView,
	e_addressbook_view,
	GTK_TYPE_SCROLLED_WINDOW,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_SELECTABLE,
		e_addressbook_view_selectable_init))

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
	/* Grab focus so that EFocusTracker asks us to update the
	 * selection-related actions before showing the popup menu.
	 * Apparently ETable doesn't automatically grab focus on
	 * right-clicks (is that a bug?). */
	gtk_widget_grab_focus (GTK_WIDGET (view));

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
table_double_click (ETable *table,
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
table_right_click (ETable *table,
                   gint row,
                   gint col,
                   GdkEvent *event,
                   EAddressbookView *view)
{
	addressbook_view_emit_popup_event (view, event);

	return TRUE;
}

static gint
table_white_space_event (ETable *table,
                         GdkEvent *event,
                         EAddressbookView *view)
{
	guint event_button = 0;

	gdk_event_get_button (event, &event_button);

	if (event->type == GDK_BUTTON_PRESS && event_button == 3) {
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
	EBookClient *book_client;
	GSList *contact_list;
	GdkAtom target;
	gchar *value;

	if (!E_IS_ADDRESSBOOK_TABLE_ADAPTER (view->priv->object))
		return;

	model = e_addressbook_view_get_model (view);
	book_client = e_addressbook_model_get_client (model);

	contact_list = e_addressbook_view_get_selected (view);
	target = gtk_selection_data_get_target (selection_data);

	switch (info) {
		case DND_TARGET_TYPE_VCARD:
			value = eab_contact_list_to_string (contact_list);

			gtk_selection_data_set (
				selection_data, target, 8,
				(guchar *) value, strlen (value));

			g_free (value);
			break;

		case DND_TARGET_TYPE_SOURCE_VCARD:
			value = eab_book_and_contact_list_to_string (
				book_client, contact_list);

			gtk_selection_data_set (
				selection_data, target, 8,
				(guchar *) value, strlen (value));

			g_free (value);
			break;
	}

	g_slist_free_full (contact_list, (GDestroyNotify) g_object_unref);
}

static void
addressbook_view_create_table_view (EAddressbookView *view,
                                    GalViewEtable *gal_view)
{
	ETableModel *adapter;
	ETableExtras *extras;
	ETableSpecification *specification;
	ECell *cell;
	GtkWidget *widget;
	gchar *etspecfile;
	GError *local_error = NULL;

	adapter = e_addressbook_table_adapter_new (view->priv->model);

	extras = e_table_extras_new ();

	/* Set proper format component for a default 'date' cell renderer. */
	cell = e_table_extras_get_cell (extras, "date");
	e_cell_date_set_format_component (E_CELL_DATE (cell), "addressbook");

	etspecfile = g_build_filename (
		EVOLUTION_ETSPECDIR, "e-addressbook-view.etspec", NULL);
	specification = e_table_specification_new (etspecfile, &local_error);

	/* Failure here is fatal. */
	if (local_error != NULL) {
		g_error ("%s: %s", etspecfile, local_error->message);
		g_assert_not_reached ();
	}

	/* Here we create the table.  We give it the three pieces of
	 * the table we've created, the header, the model, and the
	 * initial layout.  It does the rest.  */
	widget = e_table_new (adapter, extras, specification);
	gtk_container_add (GTK_CONTAINER (view), widget);

	g_object_unref (specification);
	g_free (etspecfile);

	view->priv->object = G_OBJECT (adapter);

	g_signal_connect (
		widget, "double_click",
		G_CALLBACK (table_double_click), view);
	g_signal_connect (
		widget, "right_click",
		G_CALLBACK (table_right_click), view);
	g_signal_connect (
		widget, "white_space_event",
		G_CALLBACK (table_white_space_event), view);
	g_signal_connect_swapped (
		widget, "selection_change",
		G_CALLBACK (addressbook_view_emit_selection_change), view);

	e_table_drag_source_set (
		E_TABLE (widget), GDK_BUTTON1_MASK,
		drag_types, G_N_ELEMENTS (drag_types),
		GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect (
		E_TABLE (widget), "table_drag_data_get",
		G_CALLBACK (table_drag_data_get), view);

	gtk_widget_show (widget);

	gal_view_etable_attach_table (gal_view, E_TABLE (widget));
}

static void
addressbook_view_create_minicard_view (EAddressbookView *view,
                                       GalViewMinicard *gal_view)
{
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

	view->priv->object = G_OBJECT (minicard_view);

	gtk_container_add (GTK_CONTAINER (view), minicard_view);
	gtk_widget_show (minicard_view);

	e_reflow_model_changed (E_REFLOW_MODEL (adapter));

	gal_view_minicard_attach (gal_view, view);
}

static void
addressbook_view_display_view_cb (GalViewInstance *view_instance,
                                  GalView *gal_view,
                                  EAddressbookView *view)
{
	EShellView *shell_view;
	GtkWidget *child;

	child = gtk_bin_get_child (GTK_BIN (view));
	if (child != NULL)
		gtk_container_remove (GTK_CONTAINER (view), child);
	view->priv->object = NULL;

	if (GAL_IS_VIEW_ETABLE (gal_view))
		addressbook_view_create_table_view (
			view, GAL_VIEW_ETABLE (gal_view));
	else if (GAL_IS_VIEW_MINICARD (gal_view))
		addressbook_view_create_minicard_view (
			view, GAL_VIEW_MINICARD (gal_view));

	shell_view = e_addressbook_view_get_shell_view (view);
	e_shell_view_set_view_instance (shell_view, view_instance);

	command_state_change (view);
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
		case PROP_COPY_TARGET_LIST:
			g_value_set_boxed (
				value,
				e_addressbook_view_get_copy_target_list (
				E_ADDRESSBOOK_VIEW (object)));
			return;

		case PROP_MODEL:
			g_value_set_object (
				value,
				e_addressbook_view_get_model (
				E_ADDRESSBOOK_VIEW (object)));
			return;

		case PROP_PASTE_TARGET_LIST:
			g_value_set_boxed (
				value,
				e_addressbook_view_get_paste_target_list (
				E_ADDRESSBOOK_VIEW (object)));
			return;

		case PROP_SHELL_VIEW:
			g_value_set_object (
				value,
				e_addressbook_view_get_shell_view (
				E_ADDRESSBOOK_VIEW (object)));
			return;

		case PROP_SOURCE:
			g_value_set_object (
				value,
				e_addressbook_view_get_source (
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
		e_activity_set_state (priv->activity, E_ACTIVITY_COMPLETED);
		g_object_unref (priv->activity);
		priv->activity = NULL;
	}

	if (priv->source != NULL) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}

	if (priv->view_instance != NULL) {
		g_object_unref (priv->view_instance);
		priv->view_instance = NULL;
	}

	priv->filter_id = 0;
	priv->search_id = 0;

	if (priv->search_text) {
		g_free (priv->search_text);
		priv->search_text = NULL;
	}

	if (priv->advanced_search) {
		g_object_unref (priv->advanced_search);
		priv->advanced_search = NULL;
	}

	if (priv->copy_target_list != NULL) {
		gtk_target_list_unref (priv->copy_target_list);
		priv->copy_target_list = NULL;
	}

	if (priv->paste_target_list != NULL) {
		gtk_target_list_unref (priv->paste_target_list);
		priv->paste_target_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_addressbook_view_parent_class)->dispose (object);
}

static void
addressbook_view_constructed (GObject *object)
{
	EAddressbookView *view = E_ADDRESSBOOK_VIEW (object);
	GalViewInstance *view_instance;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EClientCache *client_cache;
	ESource *source;
	const gchar *uid;

	shell_view = e_addressbook_view_get_shell_view (view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);
	client_cache = e_shell_get_client_cache (shell);

	source = e_addressbook_view_get_source (view);
	uid = e_source_get_uid (source);

	view->priv->model = e_addressbook_model_new (client_cache);

	view_instance = e_shell_view_new_view_instance (shell_view, uid);
	g_signal_connect (
		view_instance, "display-view",
		G_CALLBACK (addressbook_view_display_view_cb), view);
	view->priv->view_instance = view_instance;

	/* Do not call gal_view_instance_load() here.  EBookShellContent
	 * must first obtain a reference to this EAddressbookView so that
	 * e_book_shell_content_get_current_view() returns the correct
	 * view in GalViewInstance::loaded signal handlers. */

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_addressbook_view_parent_class)->constructed (object);
}

static void
addressbook_view_update_actions (ESelectable *selectable,
                                 EFocusTracker *focus_tracker,
                                 GdkAtom *clipboard_targets,
                                 gint n_clipboard_targets)
{
	EAddressbookView *view;
	EAddressbookModel *model;
	ESelectionModel *selection_model;
	GtkAction *action;
	GtkTargetList *target_list;
	gboolean can_paste = FALSE;
	gboolean source_is_editable;
	gboolean sensitive;
	const gchar *tooltip;
	gint n_contacts;
	gint n_selected;
	gint ii;

	view = E_ADDRESSBOOK_VIEW (selectable);
	model = e_addressbook_view_get_model (view);
	selection_model = e_addressbook_view_get_selection_model (view);

	source_is_editable = e_addressbook_model_get_editable (model);
	n_contacts = (selection_model != NULL) ?
		e_selection_model_row_count (selection_model) : 0;
	n_selected = (selection_model != NULL) ?
		e_selection_model_selected_count (selection_model) : 0;

	target_list = e_selectable_get_paste_target_list (selectable);
	for (ii = 0; ii < n_clipboard_targets && !can_paste; ii++)
		can_paste = gtk_target_list_find (
			target_list, clipboard_targets[ii], NULL);

	action = e_focus_tracker_get_cut_clipboard_action (focus_tracker);
	sensitive = source_is_editable && (n_selected > 0);
	tooltip = _("Cut selected contacts to the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_copy_clipboard_action (focus_tracker);
	sensitive = (n_selected > 0);
	tooltip = _("Copy selected contacts to the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_paste_clipboard_action (focus_tracker);
	sensitive = source_is_editable && can_paste;
	tooltip = _("Paste contacts from the clipboard");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_delete_selection_action (focus_tracker);
	sensitive = source_is_editable && (n_selected > 0);
	tooltip = _("Delete selected contacts");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);

	action = e_focus_tracker_get_select_all_action (focus_tracker);
	sensitive = (n_contacts > 0);
	tooltip = _("Select all visible contacts");
	gtk_action_set_sensitive (action, sensitive);
	gtk_action_set_tooltip (action, tooltip);
}

static void
addressbook_view_cut_clipboard (ESelectable *selectable)
{
	EAddressbookView *view;

	view = E_ADDRESSBOOK_VIEW (selectable);

	e_selectable_copy_clipboard (selectable);
	e_addressbook_view_delete_selection (view, FALSE);
}

static void
addressbook_view_copy_clipboard (ESelectable *selectable)
{
	EAddressbookView *view;
	GtkClipboard *clipboard;
	GSList *contact_list;
	gchar *string;

	view = E_ADDRESSBOOK_VIEW (selectable);
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	contact_list = e_addressbook_view_get_selected (view);

	string = eab_contact_list_to_string (contact_list);
	e_clipboard_set_directory (clipboard, string, -1);
	g_free (string);

	g_slist_free_full (contact_list, (GDestroyNotify) g_object_unref);
}

static void
addressbook_view_paste_clipboard (ESelectable *selectable)
{
	EBookClient *book_client;
	EAddressbookView *view;
	EAddressbookModel *model;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	GtkClipboard *clipboard;
	GSList *contact_list, *iter;
	gchar *string;

	view = E_ADDRESSBOOK_VIEW (selectable);
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);

	if (!e_clipboard_wait_is_directory_available (clipboard))
		return;

	model = e_addressbook_view_get_model (view);
	client_cache = e_addressbook_model_get_client_cache (model);
	book_client = e_addressbook_model_get_client (model);

	string = e_clipboard_wait_for_directory (clipboard);
	contact_list = eab_contact_list_from_string (string);
	g_free (string);

	registry = e_client_cache_ref_registry (client_cache);

	for (iter = contact_list; iter != NULL; iter = iter->next) {
		EContact *contact = iter->data;

		eab_merging_book_add_contact (
			registry, book_client, contact, NULL, NULL);
	}

	g_object_unref (registry);

	g_slist_free_full (contact_list, (GDestroyNotify) g_object_unref);
}

static void
addressbook_view_delete_selection (ESelectable *selectable)
{
	EAddressbookView *view;

	view = E_ADDRESSBOOK_VIEW (selectable);

	e_addressbook_view_delete_selection (view, TRUE);
}

static void
addressbook_view_select_all (ESelectable *selectable)
{
	EAddressbookView *view;
	ESelectionModel *selection_model;

	view = E_ADDRESSBOOK_VIEW (selectable);
	selection_model = e_addressbook_view_get_selection_model (view);

	if (selection_model != NULL)
		e_selection_model_select_all (selection_model);
}

static void
e_addressbook_view_class_init (EAddressbookViewClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EAddressbookViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = addressbook_view_set_property;
	object_class->get_property = addressbook_view_get_property;
	object_class->dispose = addressbook_view_dispose;
	object_class->constructed = addressbook_view_constructed;

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_COPY_TARGET_LIST,
		"copy-target-list");

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			"Model",
			NULL,
			E_TYPE_ADDRESSBOOK_MODEL,
			G_PARAM_READABLE));

	/* Inherited from ESelectableInterface */
	g_object_class_override_property (
		object_class,
		PROP_PASTE_TARGET_LIST,
		"paste-target-list");

	g_object_class_install_property (
		object_class,
		PROP_SHELL_VIEW,
		g_param_spec_object (
			"shell-view",
			"Shell View",
			NULL,
			E_TYPE_SHELL_VIEW,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			"Source",
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

	/* init the accessibility support for e_addressbook_view */
	eab_view_a11y_init ();
}

static void
e_addressbook_view_init (EAddressbookView *view)
{
	GtkTargetList *target_list;

	view->priv = E_ADDRESSBOOK_VIEW_GET_PRIVATE (view);

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_directory_targets (target_list, 0);
	view->priv->copy_target_list = target_list;

	target_list = gtk_target_list_new (NULL, 0);
	e_target_list_add_directory_targets (target_list, 0);
	view->priv->paste_target_list = target_list;

	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (view),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (view),
		GTK_SHADOW_IN);
}

static void
e_addressbook_view_selectable_init (ESelectableInterface *iface)
{
	iface->update_actions = addressbook_view_update_actions;
	iface->cut_clipboard = addressbook_view_cut_clipboard;
	iface->copy_clipboard = addressbook_view_copy_clipboard;
	iface->paste_clipboard = addressbook_view_paste_clipboard;
	iface->delete_selection = addressbook_view_delete_selection;
	iface->select_all = addressbook_view_select_all;
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
	g_signal_connect (
		view->priv->model, "stop_state_changed",
		G_CALLBACK (stop_state_changed), view);
	g_signal_connect_swapped (
		view->priv->model, "writable-status",
		G_CALLBACK (command_state_change), view);

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

/* Helper for e_addressbook_view_get_selected() */
static void
add_to_list (gint model_row,
             gpointer closure)
{
	GSList **list = closure;
	*list = g_slist_prepend (*list, GINT_TO_POINTER (model_row));
}

GSList *
e_addressbook_view_get_selected (EAddressbookView *view)
{
	GSList *list, *iter;
	ESelectionModel *selection;

	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	list = NULL;
	selection = e_addressbook_view_get_selection_model (view);
	e_selection_model_foreach (selection, add_to_list, &list);

	for (iter = list; iter != NULL; iter = iter->next)
		iter->data = e_addressbook_model_get_contact (
			view->priv->model, GPOINTER_TO_INT (iter->data));
	return g_slist_reverse (list);
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
		GtkWidget *child;

		child = gtk_bin_get_child (GTK_BIN (view));
		model = e_table_get_selection_model (E_TABLE (child));

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

GtkTargetList *
e_addressbook_view_get_copy_target_list (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->copy_target_list;
}

GtkTargetList *
e_addressbook_view_get_paste_target_list (EAddressbookView *view)
{
	g_return_val_if_fail (E_IS_ADDRESSBOOK_VIEW (view), NULL);

	return view->priv->paste_target_list;
}

static void
status_message (EAddressbookView *view,
                const gchar *status,
                gint percent)
{
	EActivity *activity;
	EShellView *shell_view;
	EShellBackend *shell_backend;

	activity = view->priv->activity;
	shell_view = e_addressbook_view_get_shell_view (view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	if (status == NULL || *status == '\0') {
		if (activity != NULL) {
			view->priv->activity = NULL;
			e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
			g_object_unref (activity);
		}

	} else if (activity == NULL) {
		activity = e_activity_new ();
		view->priv->activity = activity;
		e_activity_set_text (activity, status);
		if (percent >= 0)
			e_activity_set_percent (activity, percent);
		e_shell_backend_add_activity (shell_backend, activity);
	} else {
		e_activity_set_text (activity, status);
		if (percent >= 0)
			e_activity_set_percent (activity, percent);
	}
}

static void
search_result (EAddressbookView *view,
               const GError *error)
{
	EShellView *shell_view;
	EAlertSink *alert_sink;

	shell_view = e_addressbook_view_get_shell_view (view);
	alert_sink = E_ALERT_SINK (e_shell_view_get_shell_content (shell_view));

	eab_search_result_dialog (alert_sink, error);
}

static void
folder_bar_message (EAddressbookView *view,
                    const gchar *message)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	const gchar *display_name;

	shell_view = e_addressbook_view_get_shell_view (view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	if (view->priv->source == NULL)
		return;

	display_name = e_source_get_display_name (view->priv->source);
	e_shell_sidebar_set_primary_text (shell_sidebar, display_name);
	e_shell_sidebar_set_secondary_text (shell_sidebar, message);
}

static void
stop_state_changed (GObject *object,
                    EAddressbookView *view)
{
	command_state_change (view);
}

static void
command_state_change (EAddressbookView *view)
{
	g_signal_emit (view, signals[COMMAND_STATE_CHANGE], 0);
}

static void
contact_print_button_draw_page (GtkPrintOperation *operation,
                                GtkPrintContext *context,
                                gint page_nr,
                                EPrintable *printable)
{
	GtkPageSetup *setup;
	gdouble top_margin, page_width;
	cairo_t *cr;

	setup = gtk_print_context_get_page_setup (context);
	top_margin = gtk_page_setup_get_top_margin (setup, GTK_UNIT_POINTS);
	page_width = gtk_page_setup_get_page_width (setup, GTK_UNIT_POINTS);

	cr = gtk_print_context_get_cairo_context (context);

	e_printable_reset (printable);

	while (e_printable_data_left (printable)) {
		cairo_save (cr);
		contact_page_draw_footer (operation,context,page_nr++);
		e_printable_print_page (
			printable, context, page_width - 16, top_margin + 10, TRUE);
		cairo_restore (cr);
	}
}

static void
e_contact_print_button (EPrintable *printable,
                        GtkPrintOperationAction action)
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
                          gboolean selection_only,
                          GtkPrintOperationAction action)
{
	GalView *gal_view;
	GalViewInstance *view_instance;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	view_instance = e_addressbook_view_get_view_instance (view);
	gal_view = gal_view_instance_get_current_view (view_instance);

	/* Print the selected contacts. */
	if (GAL_IS_VIEW_MINICARD (gal_view) && selection_only) {
		GSList *contact_list;

		contact_list = e_addressbook_view_get_selected (view);
		e_contact_print (NULL, NULL, contact_list, action);
		g_slist_free_full (
			contact_list,
			(GDestroyNotify) g_object_unref);

	/* Print the latest query results. */
	} else if (GAL_IS_VIEW_MINICARD (gal_view)) {
		EAddressbookModel *model;
		EBookClient *book_client;
		EBookQuery *query;
		const gchar *query_string;

		model = e_addressbook_view_get_model (view);
		book_client = e_addressbook_model_get_client (model);
		query_string = e_addressbook_model_get_query (model);

		if (query_string != NULL)
			query = e_book_query_from_string (query_string);
		else
			query = NULL;

		e_contact_print (book_client, query, NULL, action);

		if (query != NULL)
			e_book_query_unref (query);

	/* XXX Does this print the entire table or just selected? */
	} else if (GAL_IS_VIEW_ETABLE (gal_view)) {
		EPrintable *printable;
		GtkWidget *widget;

		widget = gtk_bin_get_child (GTK_BIN (view));
		printable = e_table_get_printable (E_TABLE (widget));
		g_object_ref_sink (printable);

		e_contact_print_button (printable, action);

		g_object_unref (printable);
	}
}

static void
report_and_free_error_if_any (GError *error)
{
	if (!error)
		return;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return;
	}

	if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_PERMISSION_DENIED)) {
		e_alert_run_dialog_for_args (
			e_shell_get_active_window (NULL),
			"addressbook:contact-delete-error-perm", NULL);
	} else {
		eab_error_dialog (NULL, NULL, _("Failed to delete contact"), error);
	}

	g_error_free (error);
}

/* callback function to handle removal of contacts for
 * which a user doesnt have write permission
 */
static void
remove_contacts_cb (GObject *source_object,
                    GAsyncResult *result,
                    gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	GError *error = NULL;

	e_book_client_remove_contacts_finish (book_client, result, &error);

	report_and_free_error_if_any (error);
}

static void
remove_contact_cb (GObject *source_object,
                   GAsyncResult *result,
                   gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	GError *error = NULL;

	e_book_client_remove_contact_finish (book_client, result, &error);

	report_and_free_error_if_any (error);
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
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Delete"), GTK_RESPONSE_ACCEPT,
		NULL);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (message);

	return (response == GTK_RESPONSE_ACCEPT);
}

void
e_addressbook_view_delete_selection (EAddressbookView *view,
                                     gboolean is_delete)
{
	GSList *list, *l;
	gboolean plural = FALSE, is_list = FALSE;
	EContact *contact;
	ETable *etable = NULL;
	EAddressbookModel *model;
	EBookClient *book_client;
	ESelectionModel *selection_model = NULL;
	GalViewInstance *view_instance;
	GalView *gal_view;
	GtkWidget *widget;
	gchar *name = NULL;
	gint row = 0, select;

	model = e_addressbook_view_get_model (view);
	book_client = e_addressbook_model_get_client (model);

	view_instance = e_addressbook_view_get_view_instance (view);
	gal_view = gal_view_instance_get_current_view (view_instance);

	list = e_addressbook_view_get_selected (view);
	contact = list->data;

	if (g_slist_next (list))
		plural = TRUE;
	else
		name = e_contact_get (contact, E_CONTACT_FILE_AS);

	if (e_contact_get (contact, E_CONTACT_IS_LIST))
		is_list = TRUE;

	widget = gtk_bin_get_child (GTK_BIN (view));

	if (GAL_IS_VIEW_MINICARD (gal_view)) {
		selection_model = e_addressbook_view_get_selection_model (view);
		row = e_selection_model_cursor_row (selection_model);
	}

	else if (GAL_IS_VIEW_ETABLE (gal_view)) {
		etable = E_TABLE (widget);
		row = e_table_get_cursor_row (E_TABLE (etable));
	}

	/* confirm delete */
	if (is_delete && !addressbook_view_confirm_delete (
			GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
			plural, is_list, name)) {
		g_free (name);
		g_slist_free_full (list, (GDestroyNotify) g_object_unref);
		return;
	}

	if (e_client_check_capability (E_CLIENT (book_client), "bulk-remove")) {
		GSList *ids = NULL;

		for (l = list; l; l = g_slist_next (l)) {
			const gchar *uid;

			contact = l->data;

			uid = e_contact_get_const (contact, E_CONTACT_UID);
			ids = g_slist_prepend (ids, (gpointer) uid);
		}

		/* Remove the cards all at once. */
		e_book_client_remove_contacts (
			book_client, ids, NULL, remove_contacts_cb, NULL);

		g_slist_free (ids);
	} else {
		for (l = list; l; l = g_slist_next (l)) {
			contact = l->data;

			/* Remove the card. */
			e_book_client_remove_contact (
				book_client, contact, NULL,
				remove_contact_cb, NULL);
		}
	}

	/* Sets the cursor, at the row after the deleted row */
	if (GAL_IS_VIEW_MINICARD (gal_view) && row != 0 && selection_model) {
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
		if (select == e_table_model_row_count (E_TABLE (etable)->model) - 1)
			select = select - 1;
		else
			select = select + 1;

		row = e_table_view_to_model_row (E_TABLE (etable), select);
		e_table_set_cursor_row (E_TABLE (etable), row);
	}
	g_slist_free_full (list, (GDestroyNotify) g_object_unref);
}

void
e_addressbook_view_view (EAddressbookView *view)
{
	GSList *list, *iter;
	gint response;
	guint length;

	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	list = e_addressbook_view_get_selected (view);
	length = g_slist_length (list);
	response = GTK_RESPONSE_YES;

	if (length > 5) {
		GtkWidget *dialog;

		/* XXX Use e_alert_new(). */
		/* XXX Provide a parent window. */
		dialog = gtk_message_dialog_new (
			NULL, 0,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, ngettext (
			/* Translators: This is shown for > 5 contacts. */
			"Opening %d contacts will open %d new windows "
			"as well.\nDo you really want to display all of "
			"these contacts?",
			"Opening %d contacts will open %d new windows "
			"as well.\nDo you really want to display all of "
			"these contacts?", length), length, length);
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

	g_slist_free_full (list, (GDestroyNotify) g_object_unref);
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

struct TransferContactsData
{
	gboolean delete_from_source;
	EAddressbookView *view;
};

static void
all_contacts_ready_cb (GObject *source_object,
                       GAsyncResult *result,
                       gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	struct TransferContactsData *tcd = user_data;
	EAddressbookModel *model;
	EClientCache *client_cache;
	EShellView *shell_view;
	EShellContent *shell_content;
	EAlertSink *alert_sink;
	GSList *contacts = NULL;
	GError *error = NULL;

	g_return_if_fail (book_client != NULL);
	g_return_if_fail (tcd != NULL);

	e_book_client_get_contacts_finish (
		book_client, result, &contacts, &error);

	shell_view = e_addressbook_view_get_shell_view (tcd->view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	alert_sink = E_ALERT_SINK (shell_content);

	model = e_addressbook_view_get_model (tcd->view);
	client_cache = e_addressbook_model_get_client_cache (model);

	if (error != NULL) {
		e_alert_submit (
			alert_sink, "addressbook:search-error",
			error->message, NULL);
		g_error_free (error);

	} else if (contacts != NULL) {
		ESourceRegistry *registry;

		registry = e_client_cache_ref_registry (client_cache);

		eab_transfer_contacts (
			registry, book_client, contacts,
			tcd->delete_from_source, alert_sink);

		g_object_unref (registry);
	}

	g_object_unref (tcd->view);
	g_free (tcd);
}

static void
view_transfer_contacts (EAddressbookView *view,
                        gboolean delete_from_source,
                        gboolean all)
{
	EAddressbookModel *model;
	EBookClient *book_client;
	EClientCache *client_cache;

	model = e_addressbook_view_get_model (view);
	book_client = e_addressbook_model_get_client (model);
	client_cache = e_addressbook_model_get_client_cache (model);

	if (all) {
		EBookQuery *query;
		gchar *query_str;
		struct TransferContactsData *tcd;

		query = e_book_query_any_field_contains ("");
		query_str = e_book_query_to_string (query);
		e_book_query_unref (query);

		tcd = g_new0 (struct TransferContactsData, 1);
		tcd->delete_from_source = delete_from_source;
		tcd->view = g_object_ref (view);

		e_book_client_get_contacts (
			book_client, query_str, NULL,
			all_contacts_ready_cb, tcd);
	} else {
		GSList *contacts = NULL;
		EShellView *shell_view;
		EShellContent *shell_content;
		EAlertSink *alert_sink;
		ESourceRegistry *registry;

		shell_view = e_addressbook_view_get_shell_view (view);
		shell_content = e_shell_view_get_shell_content (shell_view);
		alert_sink = E_ALERT_SINK (shell_content);

		contacts = e_addressbook_view_get_selected (view);

		registry = e_client_cache_ref_registry (client_cache);

		eab_transfer_contacts (
			registry, book_client, contacts,
			delete_from_source, alert_sink);

		g_object_unref (registry);
	}
}

void
e_addressbook_view_copy_to_folder (EAddressbookView *view,
                                   gboolean all)
{
	view_transfer_contacts (view, FALSE, all);
}

void
e_addressbook_view_move_to_folder (EAddressbookView *view,
                                   gboolean all)
{
	view_transfer_contacts (view, TRUE, all);
}

void
e_addressbook_view_set_search (EAddressbookView *view,
                               gint filter_id,
                               gint search_id,
                               const gchar *search_text,
                               EFilterRule *advanced_search)
{
	EAddressbookViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));

	priv = view->priv;

	if (priv->search_text)
		g_free (priv->search_text);
	if (priv->advanced_search)
		g_object_unref (priv->advanced_search);

	priv->filter_id = filter_id;
	priv->search_id = search_id;
	priv->search_text = g_strdup (search_text);

	if (advanced_search != NULL)
		priv->advanced_search = e_filter_rule_clone (advanced_search);
	else
		priv->advanced_search = NULL;
}

/* Free returned values for search_text and advanced_search,
 * if not NULL, as these are new copies. */
void
e_addressbook_view_get_search (EAddressbookView *view,
                               gint *filter_id,
                               gint *search_id,
                               gchar **search_text,
                               EFilterRule **advanced_search)
{
	EAddressbookViewPrivate *priv;

	g_return_if_fail (view != NULL);
	g_return_if_fail (E_IS_ADDRESSBOOK_VIEW (view));
	g_return_if_fail (filter_id != NULL);
	g_return_if_fail (search_id != NULL);
	g_return_if_fail (search_text != NULL);
	g_return_if_fail (advanced_search != NULL);

	priv = view->priv;

	*filter_id = priv->filter_id;
	*search_id = priv->search_id;
	*search_text = g_strdup (priv->search_text);

	if (priv->advanced_search != NULL)
		*advanced_search = e_filter_rule_clone (priv->advanced_search);
	else
		*advanced_search = NULL;
}
