/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-sidebar.c
 *
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
 */

#include "e-memo-shell-sidebar.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/e-cal.h>

#include "e-util/e-error.h"
#include "calendar/common/authentication.h"
#include "calendar/gui/calendar-config.h"
#include "calendar/gui/e-calendar-selector.h"
#include "calendar/gui/misc.h"

#include "e-memo-shell-view.h"

#define E_MEMO_SHELL_SIDEBAR_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MEMO_SHELL_SIDEBAR, EMemoShellSidebarPrivate))

struct _EMemoShellSidebarPrivate {
	GtkWidget *selector;

	/* UID -> Client */
	GHashTable *client_table;
};

enum {
	PROP_0,
	PROP_SELECTOR
};

enum {
	CLIENT_ADDED,
	CLIENT_REMOVED,
	STATUS_MESSAGE,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

static void
memo_shell_sidebar_emit_client_added (EMemoShellSidebar *memo_shell_sidebar,
                                      ECal *client)
{
	guint signal_id = signals[CLIENT_ADDED];

	g_signal_emit (memo_shell_sidebar, signal_id, 0, client);
}

static void
memo_shell_sidebar_emit_client_removed (EMemoShellSidebar *memo_shell_sidebar,
                                        ECal *client)
{
	guint signal_id = signals[CLIENT_REMOVED];

	g_signal_emit (memo_shell_sidebar, signal_id, 0, client);
}

static void
memo_shell_sidebar_emit_status_message (EMemoShellSidebar *memo_shell_sidebar,
                                        const gchar *status_message)
{
	guint signal_id = signals[STATUS_MESSAGE];

	g_signal_emit (memo_shell_sidebar, signal_id, 0, status_message);
}

static void
memo_shell_sidebar_update_timezone (EMemoShellSidebar *memo_shell_sidebar)
{
	/* FIXME */
}

static void
memo_shell_sidebar_backend_died_cb (EMemoShellSidebar *memo_shell_sidebar,
                                    ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	client_table = memo_shell_sidebar->priv->client_table;

	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	source = e_cal_get_source (client);
	uid = e_source_peek_uid (source);

	g_object_ref (source);

	g_hash_table_remove (client_table, uid);
	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, NULL);

	e_error_run (
		GTK_WINDOW (shell_window),
		"calendar:memos-crashed", NULL);

	g_object_unref (source);
}

static void
memo_shell_sidebar_backend_error_cb (EMemoShellSidebar *memo_shell_sidebar,
                                     const gchar *message,
                                     ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	GtkWidget *dialog;
	const gchar *uri;
	gchar *uri_no_passwd;

	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	uri = e_cal_get_uri (client);
	uri_no_passwd = get_uri_without_password (uri);

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		_("Error on %s\n%s"),
		uri_no_passwd, message);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (uri_no_passwd);
}

static void
memo_shell_sidebar_client_opened_cb (EMemoShellSidebar *memo_shell_sidebar,
                                     ECalendarStatus status,
                                     ECal *client)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSidebar *shell_sidebar;
	ESource *source;

	source = e_cal_get_source (client);

	shell_sidebar = E_SHELL_SIDEBAR (memo_shell_sidebar);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	shell_window = e_shell_view_get_shell_window (shell_view);

	switch (status) {
		case E_CALENDAR_STATUS_OK:
			g_signal_handlers_disconnect_matched (
				client, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
				memo_shell_sidebar_client_opened_cb, NULL);

			memo_shell_sidebar_emit_status_message (
				memo_shell_sidebar, _("Loading memos"));
			memo_shell_sidebar_emit_client_added (
				memo_shell_sidebar, client);
			memo_shell_sidebar_emit_status_message (
				memo_shell_sidebar, NULL);
			break;

		case E_CALENDAR_STATUS_BUSY:
			break;

		case E_CALENDAR_STATUS_REPOSITORY_OFFLINE:
			e_error_run (
				GTK_WINDOW (shell_window),
				"calendar:prompt-no-contents-offline-memos",
				NULL);
			break;

		default:
			memo_shell_sidebar_emit_client_removed (
				memo_shell_sidebar, client);
			break;
	}
}

static void
memo_shell_sidebar_selection_changed_cb (EMemoShellSidebar *memo_shell_sidebar,
                                         ESourceSelector *selector)
{
	/* FIXME */
}

static void
memo_shell_sidebar_primary_selection_changed_cb (EMemoShellSidebar *memo_shell_sidebar,
                                                 ESourceSelector *selector)
{
	ESource *source;
	const gchar *uid;

	source = e_source_selector_peek_primary_selection (selector);
	if (source == NULL)
		return;

	uid = e_source_peek_uid (source);
	calendar_config_set_primary_memos (uid);
}

static void
memo_shell_sidebar_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTOR:
			g_value_set_object (
				value, e_memo_shell_sidebar_get_selector (
				E_MEMO_SHELL_SIDEBAR (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
memo_shell_sidebar_dispose (GObject *object)
{
	EMemoShellSidebarPrivate *priv;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (object);

	if (priv->selector != NULL) {
		g_object_unref (priv->selector);
		priv->selector = NULL;
	}

	g_hash_table_remove_all (priv->client_table);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
memo_shell_sidebar_finalize (GObject *object)
{
	EMemoShellSidebarPrivate *priv;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (object);

	g_hash_table_destroy (priv->client_table);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
memo_shell_sidebar_constructed (GObject *object)
{
	EMemoShellSidebarPrivate *priv;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	EMemoShellView *memo_shell_view;
	ESourceSelector *selector;
	ESourceList *source_list;
	ESource *source;
	GtkContainer *container;
	GtkWidget *widget;
	GSList *list, *iter;
	gchar *uid;

	priv = E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

	shell_sidebar = E_SHELL_SIDEBAR (object);
	shell_view = e_shell_sidebar_get_shell_view (shell_sidebar);
	memo_shell_view = E_MEMO_SHELL_VIEW (shell_view);
	source_list = e_memo_shell_view_get_source_list (memo_shell_view);

	container = GTK_CONTAINER (shell_sidebar);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (widget),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_container_add (container, widget);
	gtk_widget_show (widget);

	container = GTK_CONTAINER (widget);

	widget = e_calendar_selector_new (source_list);
	e_source_selector_set_select_new (E_SOURCE_SELECTOR (widget), TRUE);
	gtk_container_add (container, widget);
	priv->selector = g_object_ref (widget);
	gtk_widget_show (widget);

	g_signal_connect_swapped (
		widget, "selection-changed",
		G_CALLBACK (memo_shell_sidebar_selection_changed_cb),
		object);

	g_signal_connect_swapped (
		widget, "primary-selection-changed",
		G_CALLBACK (memo_shell_sidebar_primary_selection_changed_cb),
		object);

	/* Restore the primary selection from the last session. */

	selector = E_SOURCE_SELECTOR (priv->selector);
	uid = calendar_config_get_primary_memos ();
	source = NULL;

	if (uid != NULL)
		source = e_source_list_peek_source_by_uid (source_list, uid);
	if (source == NULL)
		source = e_source_list_peek_source_any (source_list);
	if (source != NULL)
		e_source_selector_set_primary_selection (selector, source);

	g_free (uid);

	/* Restore the selected sources from last session. */

	list = calendar_config_get_memos_selected ();
	for (iter = list; iter != NULL; iter = iter->next) {
		uid = iter->data;
		source = e_source_list_peek_source_by_uid (source_list, uid);
		g_free (uid);

		if (source == NULL)
			continue;

		e_source_selector_select_source (
			E_SOURCE_SELECTOR (priv->selector), source);
	}
	g_slist_free (list);
}

static void
memo_shell_sidebar_client_added (EMemoShellSidebar *memo_shell_sidebar,
                                 ECal *client)
{
	memo_shell_sidebar_update_timezone (memo_shell_sidebar);
}

static void
memo_shell_sidebar_client_removed (EMemoShellSidebar *memo_shell_sidebar,
                                   ECal *client)
{
	ESourceSelector *selector;
	GHashTable *client_table;
	ESource *source;
	const gchar *uid;

	selector = E_SOURCE_SELECTOR (memo_shell_sidebar->priv->selector);
	client_table = memo_shell_sidebar->priv->client_table;

	g_signal_handlers_disconnect_matched (
		client, G_SIGNAL_MATCH_DATA, 0, 0,
		NULL, NULL, memo_shell_sidebar);

	source = e_cal_get_source (client);
	e_source_selector_unselect_source (selector, source);

	uid = e_source_peek_uid (source);
	g_hash_table_remove (client_table, uid);

	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, NULL);
}

static void
memo_shell_sidebar_class_init (EMemoShellSidebarClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMemoShellSidebarPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = memo_shell_sidebar_get_property;
	object_class->dispose = memo_shell_sidebar_dispose;
	object_class->finalize = memo_shell_sidebar_finalize;
	object_class->constructed = memo_shell_sidebar_constructed;

	class->client_added = memo_shell_sidebar_client_added;
	class->client_removed = memo_shell_sidebar_client_removed;

	g_object_class_install_property (
		object_class,
		PROP_SELECTOR,
		g_param_spec_object (
			"selector",
			_("Source Selector Widget"),
			_("This widget displays groups of memo lists"),
			E_TYPE_SOURCE_SELECTOR,
			G_PARAM_READABLE));

	signals[CLIENT_ADDED] = g_signal_new (
		"client-added",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMemoShellSidebarClass, client_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL);

	signals[CLIENT_REMOVED] = g_signal_new (
		"client-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMemoShellSidebarClass, client_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_CAL);

	signals[STATUS_MESSAGE] = g_signal_new (
		"status-message",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMemoShellSidebarClass, status_message),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1,
		G_TYPE_STRING);
}

static void
memo_shell_sidebar_init (EMemoShellSidebar *memo_shell_sidebar)
{
	GHashTable *client_table;

	client_table = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	memo_shell_sidebar->priv =
		E_MEMO_SHELL_SIDEBAR_GET_PRIVATE (memo_shell_sidebar);

	memo_shell_sidebar->priv->client_table = client_table;

	/* Postpone widget construction until we have a shell view. */
}

GType
e_memo_shell_sidebar_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMemoShellSidebarClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) memo_shell_sidebar_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMemoShellSidebar),
			0,     /* n_preallocs */
			(GInstanceInitFunc) memo_shell_sidebar_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_SIDEBAR, "EMemoShellSidebar",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_memo_shell_sidebar_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_MEMO_SHELL_SIDEBAR,
		"shell-view", shell_view, NULL);
}

ESourceSelector *
e_memo_shell_sidebar_get_selector (EMemoShellSidebar *memo_shell_sidebar)
{
	g_return_val_if_fail (
		E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar), NULL);

	return E_SOURCE_SELECTOR (memo_shell_sidebar->priv->selector);
}

void
e_memo_shell_sidebar_add_source (EMemoShellSidebar *memo_shell_sidebar,
                                 ESource *source)
{
	ESourceSelector *selector;
	GHashTable *client_table;
	ECal *client;
	const gchar *uid;
	const gchar *uri;
	gchar *message;

	g_return_if_fail (E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	client_table = memo_shell_sidebar->priv->client_table;
	selector = E_SOURCE_SELECTOR (memo_shell_sidebar->priv->selector);

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client != NULL)
		return;

	client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
	g_return_if_fail (client != NULL);

	g_signal_connect_swapped (
		client, "backend-died",
		G_CALLBACK (memo_shell_sidebar_backend_died_cb),
		memo_shell_sidebar);

	g_signal_connect_swapped (
		client, "backend-error",
		G_CALLBACK (memo_shell_sidebar_backend_error_cb),
		memo_shell_sidebar);

	g_hash_table_insert (client_table, g_strdup (uid), client);
	e_source_selector_select_source (selector, source);

	uri = e_cal_get_uri (client);
	message = g_strdup_printf (_("Opening memos at %s"), uri);
	memo_shell_sidebar_emit_status_message (memo_shell_sidebar, message);
	g_free (message);

	g_signal_connect_swapped (
		client, "cal-opened",
		G_CALLBACK (memo_shell_sidebar_client_opened_cb),
		memo_shell_sidebar);

	e_cal_open_async (client, FALSE);
}

void
e_memo_shell_sidebar_remove_source (EMemoShellSidebar *memo_shell_sidebar,
                                    ESource *source)
{
	ESourceSelector *selector;
	GHashTable *client_table;
	ECal *client;
	const gchar *uid;

	g_return_if_fail (E_IS_MEMO_SHELL_SIDEBAR (memo_shell_sidebar));
	g_return_if_fail (E_IS_SOURCE (source));

	client_table = memo_shell_sidebar->priv->client_table;
	selector = E_SOURCE_SELECTOR (memo_shell_sidebar->priv->selector);

	uid = e_source_peek_uid (source);
	client = g_hash_table_lookup (client_table, uid);

	if (client == NULL)
		return;

	memo_shell_sidebar_emit_client_removed (memo_shell_sidebar, client);
}
