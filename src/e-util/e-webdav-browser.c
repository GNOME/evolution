/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: e-webdav-browser
 * @include: e-util/e-util.h
 * @short_description: WebDAV server browser
 *
 * #EWebDAVBrowser allows to browse WebDAV servers and manage (create/edit/remove)
 * collections there, like calendars and address books, if the server supports it.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <libedataserver/libedataserver.h>
#include <libedataserverui/libedataserverui.h>

#include "e-activity.h"
#include "e-activity-bar.h"
#include "e-alert.h"
#include "e-alert-bar.h"
#include "e-alert-dialog.h"
#include "e-alert-sink.h"
#include "e-color-combo.h"
#include "e-misc-utils.h"
#include "e-spell-text-view.h"

#include "e-webdav-browser.h"

struct _EWebDAVBrowserPrivate {
	ECredentialsPrompter *credentials_prompter;

	GMutex property_lock;
	EWebDAVSession *session;
	GCancellable *cancellable;
	gboolean refresh_collection;

	guint update_ui_id;
	GSList *resources; /* ResourceData * */

	GHashTable *href_to_reference; /* gchar *href ~> GtkTreeRowReference * */

	GtkLabel *url_label;
	GtkWidget *tree_view;
	GtkWidget *create_book_button;
	GtkWidget *create_calendar_button;
	GtkWidget *create_collection_button;
	GtkWidget *edit_button;
	GtkWidget *delete_button;
	/* GtkWidget *permissions_button; */
	GtkWidget *refresh_button;

	EAlertBar *alert_bar;
	EActivityBar *activity_bar;

	GtkWidget *create_edit_popover;
	GtkWidget *create_edit_name_entry;
	GtkWidget *create_edit_color_label;
	GtkWidget *create_edit_color_combo;
	GtkWidget *create_edit_order_label;
	GtkWidget *create_edit_order_spin;
	GtkWidget *create_edit_support_label;
	GtkWidget *create_edit_event_check;
	GtkWidget *create_edit_memo_check;
	GtkWidget *create_edit_task_check;
	GtkWidget *create_edit_description_label;
	GtkWidget *create_edit_description_scrolled_window;
	GtkWidget *create_edit_description_textview;
	GtkWidget *create_edit_save_button;
	GtkWidget *create_edit_hint_popover;
	GtkWidget *create_edit_hint_label;
};

enum {
	PROP_0,
	PROP_CREDENTIALS_PROMPTER,
	PROP_SOURCE
};

static void webdav_browser_alert_sink_init (EAlertSinkInterface *iface);
static void webdav_browser_change_busy_state (EWebDAVBrowser *webdav_browser, gboolean is_busy);

G_DEFINE_TYPE_WITH_CODE (EWebDAVBrowser, e_webdav_browser, GTK_TYPE_GRID,
	G_ADD_PRIVATE (EWebDAVBrowser)
	G_IMPLEMENT_INTERFACE (E_TYPE_ALERT_SINK, webdav_browser_alert_sink_init))

typedef enum {
	E_EDITING_FLAG_NONE		= 0,
	E_EDITING_FLAG_IS_LOADING_ROW	= 1 << 0,
	E_EDITING_FLAG_HAS_OPTIONS	= 1 << 1,
	E_EDITING_FLAG_MKCOL		= 1 << 2,
	E_EDITING_FLAG_EXMKCOL		= 1 << 3,
	E_EDITING_FLAG_MKCALENDAR	= 1 << 4,
	E_EDITING_FLAG_CAN_BOOK		= 1 << 5,
	E_EDITING_FLAG_CAN_CALENDAR	= 1 << 6,
	E_EDITING_FLAG_CAN_ACL		= 1 << 7,
	E_EDITING_FLAG_CAN_DELETE	= 1 << 8,
	E_EDITING_FLAG_IS_BOOK		= 1 << 9,
	E_EDITING_FLAG_IS_CALENDAR	= 1 << 10,
	E_EDITING_FLAG_IS_COLLECTION	= 1 << 11,
	E_EDITING_FLAG_HAS_CALENDAR_COMPONENTS	= 1 << 12
} EEditingFlags;

enum {
	COLUMN_STRING_DISPLAY_NAME = 0,
	COLUMN_STRING_TYPE,
	COLUMN_STRING_HREF,
	COLUMN_STRING_DESCRIPTION,
	COLUMN_STRING_ICON_NAME,
	COLUMN_BOOL_ICON_VISIBLE,
	COLUMN_RGBA_COLOR,
	COLUMN_BOOL_COLOR_VISIBLE,
	COLUMN_BOOL_CHILDREN_LOADED,
	COLUMN_UINT_EDITING_FLAGS,
	COLUMN_UINT_SUPPORTS,
	COLUMN_STRING_TOOLTIP,
	COLUMN_INT_ORDER,
	N_COLUMNS
};

typedef struct _ResourceData {
	guint32 editing_flags; /* bit-or of EEditingFlags */
	EWebDAVResource *resource;
} ResourceData;

static void
resource_data_free (gpointer ptr)
{
	ResourceData *rd = ptr;

	if (rd) {
		e_webdav_resource_free (rd->resource);
		g_slice_free (ResourceData, rd);
	}
}

static gint
resource_data_compare (gconstpointer aa,
		       gconstpointer bb)
{
	const ResourceData *rda = aa, *rdb = bb;

	if (!rda || !rdb) {
		if (rda == rdb)
			return 0;
		if (rda)
			return -1;

		return 1;
	}

	g_return_val_if_fail (rda->resource != NULL, 0);
	g_return_val_if_fail (rdb->resource != NULL, 0);

	return g_strcmp0 (rda->resource->href, rdb->resource->href);
}

static void
webdav_browser_add_alert (EWebDAVBrowser *webdav_browser,
			  const gchar *primary_text,
			  const gchar *secondary_text)
{
	EAlert *alert;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (primary_text != NULL);

	alert = e_alert_new ("system:general-error", primary_text, secondary_text ? secondary_text : "", NULL);

	e_alert_bar_add_alert (webdav_browser->priv->alert_bar, alert);

	g_object_unref (alert);
}

static void
webdav_browser_refresh_collection_done_cb (GObject *source_object,
					   GAsyncResult *result,
					   gpointer user_data)
{
	ESourceRegistry *registry;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (source_object));

	registry = E_SOURCE_REGISTRY (source_object);

	e_source_registry_refresh_backend_finish (registry, result, &local_error);

	if (local_error && !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		g_warning ("%s: Failed to refresh collection: %s", G_STRFUNC, local_error->message);

	g_clear_error (&local_error);
}

static void
webdav_browser_refresh_collection (EWebDAVBrowser *webdav_browser)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));

	webdav_browser->priv->refresh_collection = FALSE;

	if (webdav_browser->priv->session) {
		ESource *source;

		source = e_soup_session_get_source (E_SOUP_SESSION (webdav_browser->priv->session));

		if (source) {
			ESourceRegistry *registry;

			registry = e_credentials_prompter_get_registry (webdav_browser->priv->credentials_prompter);

			if (registry) {
				ESource *collection_source;

				collection_source = e_source_registry_find_extension (registry, source, E_SOURCE_EXTENSION_COLLECTION);

				if (collection_source) {
					e_source_registry_refresh_backend (registry, e_source_get_uid (collection_source), NULL,
						webdav_browser_refresh_collection_done_cb, NULL);

					g_object_unref (collection_source);
				}
			}
		}
	}
}

typedef struct _LoginErrorsData {
	EWebDAVBrowser *webdav_browser;
	EWebDAVSession *session;
	GCancellable *cancellable;
	const GError *error;

	gboolean run_trust_prompt;
	gchar *certificate_pem;
	GTlsCertificateFlags certificate_errors;

	EFlag *flag;
	gboolean repeat;
} LoginErrorsData;

static void
webdav_browser_trust_prompt_done_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	ETrustPromptResponse response = E_TRUST_PROMPT_RESPONSE_UNKNOWN;
	ESource *source;
	LoginErrorsData *led = user_data;

	g_return_if_fail (E_IS_SOURCE (source_object));
	g_return_if_fail (led != NULL);

	source = E_SOURCE (source_object);
	if (e_trust_prompt_run_for_source_finish (source, result, &response, NULL) && (
	    response == E_TRUST_PROMPT_RESPONSE_ACCEPT ||
	    response == E_TRUST_PROMPT_RESPONSE_ACCEPT_TEMPORARILY)) {
		led->repeat = TRUE;
	}

	e_flag_set (led->flag);
}

static void
webdav_browser_credentials_prompt_done_cb (GObject *source_object,
					   GAsyncResult *result,
					   gpointer user_data)
{
	LoginErrorsData *led = user_data;
	ENamedParameters *credentials = NULL;
	ESource *source = NULL;

	g_return_if_fail (led != NULL);
	g_return_if_fail (E_IS_CREDENTIALS_PROMPTER (source_object));

	if (e_credentials_prompter_prompt_finish (E_CREDENTIALS_PROMPTER (source_object), result, &source, &credentials, NULL)) {
		e_soup_session_set_credentials (E_SOUP_SESSION (led->session), credentials);
		led->repeat = credentials != NULL;
	}

	e_named_parameters_free (credentials);

	e_flag_set (led->flag);
}

static gboolean
webdav_browser_manage_login_error_cb (gpointer user_data)
{
	LoginErrorsData *led = user_data;
	ESource *source;

	g_return_val_if_fail (led != NULL, FALSE);
	g_return_val_if_fail (led->flag != NULL, FALSE);

	source = e_soup_session_get_source (E_SOUP_SESSION (led->session));
	if (!E_IS_SOURCE (source)) {
		e_flag_set (led->flag);
		return FALSE;
	}

	if (led->run_trust_prompt) {
		GtkWindow *parent;
		GtkWidget *widget;

		widget = gtk_widget_get_toplevel (GTK_WIDGET (led->webdav_browser));
		parent = widget ? GTK_WINDOW (widget) : NULL;

		e_trust_prompt_run_for_source (parent, source, led->certificate_pem, led->certificate_errors,
			NULL, FALSE, led->cancellable, webdav_browser_trust_prompt_done_cb, led);
	} else {
		ENamedParameters *credentials;

		credentials = e_soup_session_dup_credentials (E_SOUP_SESSION (led->session));

		e_credentials_prompter_prompt (led->webdav_browser->priv->credentials_prompter, source,
			led->error ? led->error->message : NULL,
			credentials ? E_CREDENTIALS_PROMPTER_PROMPT_FLAG_NONE:
			E_CREDENTIALS_PROMPTER_PROMPT_FLAG_ALLOW_STORED_CREDENTIALS,
			webdav_browser_credentials_prompt_done_cb, led);

		e_named_parameters_free (credentials);
	}

	return FALSE;
}

static gboolean
webdav_browser_manage_login_errors (EWebDAVBrowser *webdav_browser,
				    EWebDAVSession *session,
				    GCancellable *cancellable,
				    const GError *error)
{
	LoginErrorsData led;

	g_return_val_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser), FALSE);
	g_return_val_if_fail (E_IS_WEBDAV_SESSION (session), FALSE);

	led.webdav_browser = webdav_browser;
	led.session = session;
	led.cancellable = cancellable;
	led.error = error;
	led.run_trust_prompt = FALSE;
	led.certificate_pem = NULL;
	led.certificate_errors = 0;
	led.flag = NULL;
	led.repeat = FALSE;

	if (g_error_matches (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE) &&
	    e_soup_session_get_ssl_error_details (E_SOUP_SESSION (session), &led.certificate_pem, &led.certificate_errors)) {
		led.run_trust_prompt = TRUE;
		led.flag = e_flag_new ();
	} else if (g_error_matches (error, E_SOUP_SESSION_ERROR, SOUP_STATUS_UNAUTHORIZED)) {
		led.flag = e_flag_new ();
	}

	if (led.flag) {
		g_timeout_add (100, webdav_browser_manage_login_error_cb, &led);

		e_flag_wait (led.flag);
		e_flag_free (led.flag);
	}

	return led.repeat;
}

/* This has the property_lock already locked */
static void
webdav_browser_update_ui (EWebDAVBrowser *webdav_browser)
{
	GtkTreeModel *model;
	GtkTreeModel *sort_model;
	GtkTreeStore *tree_store;
	GSList *added_iters = NULL, *link;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));

	sort_model = gtk_tree_view_get_model (GTK_TREE_VIEW (webdav_browser->priv->tree_view));
	model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (sort_model));
	tree_store = GTK_TREE_STORE (model);

	webdav_browser->priv->resources = g_slist_sort (webdav_browser->priv->resources, resource_data_compare);

	for (link = webdav_browser->priv->resources; link; link = g_slist_next (link)) {
		ResourceData *rd = link->data;
		GtkTreeRowReference *reference;
		GtkTreeIter parent_iter, iter, *piter;
		GtkTreePath *path;
		GdkRGBA rgba;
		GString *type_info;
		const gchar *icon_name = NULL, *description;
		gchar *parent_href, *ptr, *tmp = NULL, *tooltip;
		gboolean has_parent_iter = FALSE, has_color, is_loaded_row = FALSE, is_existing_row = FALSE;
		gint len;

		if (!rd || !rd->resource || !rd->resource->href)
			continue;

		parent_href = g_strdup (rd->resource->href);
		len = strlen (parent_href);

		if (len <= 0) {
			g_free (parent_href);
			continue;
		}

		parent_href[len - 1] = '\0';

		ptr = strrchr (parent_href, '/');
		if (!ptr) {
			g_free (parent_href);
			continue;
		}

		ptr[1] = '\0';

		reference = g_hash_table_lookup (webdav_browser->priv->href_to_reference, parent_href);
		if (reference) {
			path = gtk_tree_row_reference_get_path (reference);
			has_parent_iter = gtk_tree_model_get_iter (model, &parent_iter, path);
			g_warn_if_fail (has_parent_iter);
			gtk_tree_path_free (path);
		}

		reference = g_hash_table_lookup (webdav_browser->priv->href_to_reference, rd->resource->href);
		if (reference) {
			path = gtk_tree_row_reference_get_path (reference);
			if (gtk_tree_model_get_iter (model, &iter, path)) {
				is_existing_row = TRUE;
				gtk_tree_model_get (model, &iter, COLUMN_BOOL_CHILDREN_LOADED, &is_loaded_row, -1);
			} else
				gtk_tree_store_append (tree_store, &iter, has_parent_iter ? &parent_iter : NULL);
			gtk_tree_path_free (path);
		} else {
			gtk_tree_store_append (tree_store, &iter, has_parent_iter ? &parent_iter : NULL);
		}

		if (has_parent_iter) {
			gboolean is_loaded = FALSE;

			gtk_tree_model_get (model, &parent_iter, COLUMN_BOOL_CHILDREN_LOADED, &is_loaded, -1);

			if (!is_loaded) {
				GtkTreeIter child;
				GtkTreeIter sort_iter;

				gtk_tree_store_set (tree_store, &parent_iter, COLUMN_BOOL_CHILDREN_LOADED, TRUE, -1);

				gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (sort_model), &sort_iter, &parent_iter);

				path = gtk_tree_model_get_path (sort_model, &sort_iter);
				if (path) {
					gtk_tree_view_expand_row (GTK_TREE_VIEW (webdav_browser->priv->tree_view), path, FALSE);
					gtk_tree_path_free (path);
				}

				/* And remove "Loading…" row */
				if (gtk_tree_model_iter_nth_child (model, &child, &parent_iter, 0)) {
					do {
						guint flags = E_EDITING_FLAG_NONE;

						gtk_tree_model_get (model, &child, COLUMN_UINT_EDITING_FLAGS, &flags, -1);

						if (flags == E_EDITING_FLAG_IS_LOADING_ROW) {
							gtk_tree_store_remove (tree_store, &child);
							break;
						}
					} while (gtk_tree_model_iter_next (model, &child));
				}
			}

			if (!(rd->editing_flags & E_EDITING_FLAG_HAS_OPTIONS)) {
				guint parent_editing_flags = E_EDITING_FLAG_NONE;

				gtk_tree_model_get (model, &parent_iter, COLUMN_UINT_EDITING_FLAGS, &parent_editing_flags, -1);

				rd->editing_flags =
					(parent_editing_flags & ~(E_EDITING_FLAG_IS_BOOK | E_EDITING_FLAG_IS_CALENDAR | E_EDITING_FLAG_HAS_CALENDAR_COMPONENTS | E_EDITING_FLAG_IS_COLLECTION)) |
					(rd->editing_flags & (E_EDITING_FLAG_IS_BOOK | E_EDITING_FLAG_IS_CALENDAR | E_EDITING_FLAG_HAS_CALENDAR_COMPONENTS | E_EDITING_FLAG_IS_COLLECTION));
			}
		}

		if (!is_existing_row) {
			piter = g_new0 (GtkTreeIter, 1);
			*piter = iter;
			added_iters = g_slist_prepend (added_iters, piter);
		}

		path = gtk_tree_model_get_path (model, &iter);
		reference = gtk_tree_row_reference_new (model, path);
		gtk_tree_path_free (path);

		g_hash_table_insert (webdav_browser->priv->href_to_reference, g_strdup (rd->resource->href), reference);

		type_info = g_string_new ("");

		if (rd->resource->kind == E_WEBDAV_RESOURCE_KIND_ADDRESSBOOK) {
			icon_name = "x-office-address-book";
			g_string_append (type_info, _("Address book"));
		} else if (rd->resource->kind == E_WEBDAV_RESOURCE_KIND_CALENDAR ||
			rd->resource->kind == E_WEBDAV_RESOURCE_KIND_SCHEDULE_INBOX ||
			rd->resource->kind == E_WEBDAV_RESOURCE_KIND_SCHEDULE_OUTBOX) {

			#define append_if_set(_flag, _str) \
				if ((rd->resource->supports & (_flag)) != 0) { \
					if (type_info->len) \
						g_string_append_c (type_info, ' '); \
					g_string_append (type_info, _str); \
				}

			append_if_set (E_WEBDAV_RESOURCE_SUPPORTS_EVENTS, _("Events"));
			append_if_set (E_WEBDAV_RESOURCE_SUPPORTS_MEMOS, _("Memos"));
			append_if_set (E_WEBDAV_RESOURCE_SUPPORTS_TASKS, _("Tasks"));

			#undef append_if_set

			if (type_info->len) {
				g_string_prepend (type_info, " (");
				g_string_append_c (type_info, ')');
			}
			if (rd->resource->kind == E_WEBDAV_RESOURCE_KIND_CALENDAR) {
				icon_name = "x-office-calendar";
				if ((rd->resource->supports & E_WEBDAV_DISCOVER_SUPPORTS_CALENDAR_AUTO_SCHEDULE) != 0)
					g_string_prepend (type_info, _("Calendar handling meeting invitations"));
				else
					g_string_prepend (type_info, _("Calendar"));
			} else if (rd->resource->kind == E_WEBDAV_RESOURCE_KIND_SCHEDULE_INBOX) {
				icon_name = "mail-inbox";
				g_string_prepend (type_info, _("Scheduling Inbox"));
				/* Scheduling Inbox collections MUST NOT contain any types of collection resources. */
				is_loaded_row = 1;
			} else {
				icon_name = "mail-outbox";
				g_string_prepend (type_info, _("Scheduling Outbox"));
			}

		} else if (rd->resource->kind == E_WEBDAV_RESOURCE_KIND_COLLECTION) {
			icon_name = "folder";
			g_string_append (type_info, _("Collection"));
		}

		has_color = rd->resource->color && gdk_rgba_parse (&rgba, rd->resource->color);
		if (!has_color && rd->resource->color && strlen (rd->resource->color) == 9 && rd->resource->color[0] == '#') {
			rd->resource->color[7] = '\0';
			has_color = gdk_rgba_parse (&rgba, rd->resource->color);
		}

		if (rd->resource->kind == E_WEBDAV_RESOURCE_KIND_COLLECTION) {
			if (rd->resource->description && *rd->resource->description) {
				tmp = g_strconcat (rd->resource->description, "\n\n", rd->resource->href, NULL);
				description = tmp;
			} else {
				description = rd->resource->href;
			}
		} else {
			description = rd->resource->description;
		}

		tooltip = description ? g_markup_escape_text (description, -1) : NULL;

		gtk_tree_store_set (tree_store, &iter,
			COLUMN_STRING_DISPLAY_NAME, rd->resource->display_name,
			COLUMN_STRING_TYPE, type_info->str,
			COLUMN_STRING_HREF, rd->resource->href,
			COLUMN_STRING_DESCRIPTION, description,
			COLUMN_STRING_ICON_NAME, icon_name,
			COLUMN_BOOL_ICON_VISIBLE, icon_name != NULL,
			COLUMN_RGBA_COLOR, has_color ? &rgba : NULL,
			COLUMN_BOOL_COLOR_VISIBLE, has_color,
			COLUMN_BOOL_CHILDREN_LOADED, is_loaded_row,
			COLUMN_UINT_EDITING_FLAGS, rd->editing_flags,
			COLUMN_UINT_SUPPORTS, rd->resource->supports,
			COLUMN_STRING_TOOLTIP, tooltip,
			COLUMN_INT_ORDER, rd->resource->order,
			-1);

		g_string_free (type_info, TRUE);
		g_free (parent_href);
		g_free (tooltip);
		g_free (tmp);
	}

	g_slist_free_full (webdav_browser->priv->resources, resource_data_free);
	webdav_browser->priv->resources = NULL;

	for (link = added_iters; link; link = g_slist_next (link)) {
		GtkTreeIter *piter = link->data;
		gboolean is_loaded = TRUE;

		gtk_tree_model_get (model, piter, COLUMN_BOOL_CHILDREN_LOADED, &is_loaded, -1);

		if (!is_loaded) {
			GtkTreeIter iter;

			gtk_tree_store_append (tree_store, &iter, piter);
			gtk_tree_store_set (tree_store, &iter,
				COLUMN_STRING_DISPLAY_NAME, _("Loading…"),
				COLUMN_UINT_EDITING_FLAGS, E_EDITING_FLAG_IS_LOADING_ROW,
				-1);
		}
	}

	g_slist_free_full (added_iters, g_free);
}

typedef void (* UpdateUICallback) (EWebDAVBrowser *webdav_browser, gpointer user_data);

typedef struct _UpdateUIData {
	GWeakRef *webdav_browser_weakref;
	UpdateUICallback callback;
	gpointer user_data;
	GDestroyNotify free_user_data;
} UpdateUIData;

static void
update_ui_data_free (gpointer ptr)
{
	UpdateUIData *uud = ptr;

	if (uud) {
		e_weak_ref_free (uud->webdav_browser_weakref);
		if (uud->free_user_data)
			uud->free_user_data (uud->user_data);
		g_slice_free (UpdateUIData, uud);
	}
}

static gboolean
webdav_browser_update_ui_timeout_cb (gpointer user_data)
{
	UpdateUIData *uud = user_data;
	EWebDAVBrowser *webdav_browser;

	g_return_val_if_fail (uud != NULL, FALSE);

	if (g_source_is_destroyed (g_main_current_source ()))
		return FALSE;

	webdav_browser = g_weak_ref_get (uud->webdav_browser_weakref);
	if (!webdav_browser)
		return FALSE;

	g_mutex_lock (&webdav_browser->priv->property_lock);

	webdav_browser->priv->update_ui_id = 0;

	webdav_browser_update_ui (webdav_browser);

	if (uud->callback)
		uud->callback (webdav_browser, uud->user_data);

	g_mutex_unlock (&webdav_browser->priv->property_lock);

	webdav_browser_change_busy_state (webdav_browser, FALSE);

	g_object_unref (webdav_browser);

	return FALSE;
}

static EWebDAVSession *
webdav_browser_ref_session (EWebDAVBrowser *webdav_browser)
{
	EWebDAVSession *session;

	g_return_val_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser), NULL);

	g_mutex_lock (&webdav_browser->priv->property_lock);

	session = webdav_browser->priv->session;
	if (session)
		g_object_ref (session);

	g_mutex_unlock (&webdav_browser->priv->property_lock);

	return session;
}

static void
webdav_browser_schedule_ui_update (EWebDAVBrowser *webdav_browser,
				   UpdateUICallback callback,
				   gpointer user_data,
				   GDestroyNotify free_user_data)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));

	g_mutex_lock (&webdav_browser->priv->property_lock);

	/* This should not be called recursively/multiple times when one is waiting */
	g_warn_if_fail (!webdav_browser->priv->update_ui_id);

	if (!webdav_browser->priv->update_ui_id) {
		UpdateUIData *uud;

		uud = g_slice_new0 (UpdateUIData);
		uud->webdav_browser_weakref = e_weak_ref_new (webdav_browser);
		uud->callback = callback;
		uud->user_data = user_data;
		uud->free_user_data = free_user_data;

		webdav_browser->priv->update_ui_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 100,
			webdav_browser_update_ui_timeout_cb, uud, update_ui_data_free);
	}

	g_mutex_unlock (&webdav_browser->priv->property_lock);
}

typedef struct _SearchHomeData {
	GHashTable *covered_todo_hrefs;
	GHashTable *covered_home_hrefs;
	GSList *todo_hrefs;
	GSList *home_hrefs;
} SearchHomeData;

static gboolean
webdav_browser_search_home_hrefs_cb (EWebDAVSession *webdav,
				     xmlNodePtr prop_node,
				     const GUri *request_uri,
				     const gchar *href,
				     guint status_code,
				     gpointer user_data)
{
	SearchHomeData *shd = user_data;

	g_return_val_if_fail (shd != NULL, FALSE);

	if (status_code == SOUP_STATUS_OK) {
		xmlNodePtr home_set_node, node;
		const xmlChar *href_value;
		gchar *full_href;

		home_set_node = e_xml_find_child (prop_node, E_WEBDAV_NS_CARDDAV, "addressbook-home-set");

		if (home_set_node) {
			for (node = e_xml_find_child (home_set_node, E_WEBDAV_NS_DAV, "href");
			     node;
			     node = e_xml_find_next_sibling (node, E_WEBDAV_NS_DAV, "href")) {
				href_value = e_xml_get_node_text (node);

				if (href_value && *href_value) {
					full_href = e_webdav_session_ensure_full_uri (webdav, request_uri, (const gchar *) href_value);

					if (full_href && *full_href && !g_hash_table_contains (shd->covered_home_hrefs, full_href)) {
						shd->home_hrefs = g_slist_prepend (shd->home_hrefs, full_href);
						g_hash_table_insert (shd->covered_home_hrefs, g_strdup (full_href), NULL);
						full_href = NULL;
					}

					g_free (full_href);
				}
			}
		}

		home_set_node = e_xml_find_child (prop_node, E_WEBDAV_NS_CALDAV, "calendar-home-set");

		if (home_set_node) {
			for (node = e_xml_find_child (home_set_node, E_WEBDAV_NS_DAV, "href");
			     node;
			     node = e_xml_find_next_sibling (node, E_WEBDAV_NS_DAV, "href")) {
				href_value = e_xml_get_node_text (node);

				if (href_value && *href_value) {
					full_href = e_webdav_session_ensure_full_uri (webdav, request_uri, (const gchar *) href_value);

					if (full_href && *full_href && !g_hash_table_contains (shd->covered_home_hrefs, full_href)) {
						shd->home_hrefs = g_slist_prepend (shd->home_hrefs, full_href);
						g_hash_table_insert (shd->covered_home_hrefs, g_strdup (full_href), NULL);
						full_href = NULL;
					}

					g_free (full_href);
				}
			}
		}

		href_value = e_xml_get_node_text (e_xml_find_in_hierarchy (prop_node, E_WEBDAV_NS_DAV,"current-user-principal", E_WEBDAV_NS_DAV, "href", NULL, NULL));

		if (href_value && *href_value) {
			full_href = e_webdav_session_ensure_full_uri (webdav, request_uri, (const gchar *) href_value);

			if (full_href && *full_href &&
			    !g_hash_table_contains (shd->covered_todo_hrefs, full_href)) {
				g_hash_table_insert (shd->covered_todo_hrefs, full_href, NULL);
				shd->todo_hrefs = g_slist_prepend (shd->todo_hrefs, g_strdup (full_href));
				full_href = NULL;
			}

			g_free (full_href);

			return TRUE;
		}

		href_value = e_xml_get_node_text (e_xml_find_in_hierarchy (prop_node, E_WEBDAV_NS_DAV,"principal-URL", E_WEBDAV_NS_DAV, "href", NULL, NULL));

		if (href_value && *href_value) {
			full_href = e_webdav_session_ensure_full_uri (webdav, request_uri, (const gchar *) href_value);

			if (full_href && *full_href &&
			    !g_hash_table_contains (shd->covered_todo_hrefs, full_href)) {
				g_hash_table_insert (shd->covered_todo_hrefs, full_href, NULL);
				shd->todo_hrefs = g_slist_prepend (shd->todo_hrefs, g_strdup (full_href));
				full_href = NULL;
			}

			g_free (full_href);

			return TRUE;
		}
	}

	return TRUE;
}

static guint32
webdav_browser_options_to_editing_flags (GHashTable *capabilities,
					 GHashTable *allows)
{
	guint32 editing_flags = 0;

	if (!capabilities || !allows)
		return 0;

	editing_flags |= E_EDITING_FLAG_HAS_OPTIONS;

	if (g_hash_table_contains (allows, SOUP_METHOD_MKCOL)) {
		editing_flags |= E_EDITING_FLAG_MKCOL;

		if (g_hash_table_contains (capabilities, E_WEBDAV_CAPABILITY_EXTENDED_MKCOL))
			editing_flags |= E_EDITING_FLAG_EXMKCOL;
	}

	if (g_hash_table_contains (allows, "MKCALENDAR"))
		editing_flags |= E_EDITING_FLAG_MKCALENDAR;

	if (g_hash_table_contains (capabilities, E_WEBDAV_CAPABILITY_ADDRESSBOOK))
		editing_flags |= E_EDITING_FLAG_CAN_BOOK;

	if (g_hash_table_contains (capabilities, E_WEBDAV_CAPABILITY_CALENDAR_ACCESS))
		editing_flags |= E_EDITING_FLAG_CAN_CALENDAR;

	if (g_hash_table_contains (allows, "ACL"))
		editing_flags |= E_EDITING_FLAG_CAN_ACL;

	if (g_hash_table_contains (allows, SOUP_METHOD_DELETE))
		editing_flags |= E_EDITING_FLAG_CAN_DELETE;

	return editing_flags;
}

static gboolean
webdav_browser_gather_href_resources_sync (EWebDAVBrowser *webdav_browser,
					   EWebDAVSession *session,
					   const gchar *href,
					   gboolean options_first,
					   gboolean with_children,
					   GCancellable *cancellable,
					   GError **error)
{
	gboolean done = FALSE, success = TRUE;

	g_return_val_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser), FALSE);
	g_return_val_if_fail (E_IS_WEBDAV_SESSION (session), FALSE);
	g_return_val_if_fail (href != NULL, FALSE);

	while (!done && success) {
		GSList *resources = NULL;
		guint32 top_editing_flags = 0;
		GError *local_error = NULL;

		done = TRUE;

		if (options_first) {
			GHashTable *capabilities = NULL;
			GHashTable *allows = NULL;

			/* Some servers do not allow OPTIONS on each collection, thus ignore all but login errors */
			if (!e_webdav_session_options_sync (session, href, &capabilities, &allows, cancellable, &local_error)) {
				if (webdav_browser_manage_login_errors (webdav_browser, session, cancellable, local_error)) {
					done = FALSE;
					g_clear_error (&local_error);
					continue;
				}

				g_clear_error (&local_error);
			}

			top_editing_flags = webdav_browser_options_to_editing_flags (capabilities, allows);

			if (capabilities)
				g_hash_table_destroy (capabilities);
			if (allows)
				g_hash_table_destroy (allows);
		}

		if (e_webdav_session_list_sync (session, href, with_children ? E_WEBDAV_DEPTH_THIS_AND_CHILDREN : E_WEBDAV_DEPTH_THIS, E_WEBDAV_LIST_ALL,
			&resources, cancellable, &local_error)) {
			GSList *link;

			for (link = resources; link && !g_cancellable_is_cancelled (cancellable); link = g_slist_next (link)) {
				EWebDAVResource *resource = link->data;
				GHashTable *capabilities = NULL;
				GHashTable *allows = NULL;
				guint32 editing_flags = E_EDITING_FLAG_NONE;
				ResourceData *rd;

				if (!resource ||
				    !resource->href || (
				    resource->kind != E_WEBDAV_RESOURCE_KIND_ADDRESSBOOK &&
				    resource->kind != E_WEBDAV_RESOURCE_KIND_CALENDAR &&
				    resource->kind != E_WEBDAV_RESOURCE_KIND_SCHEDULE_INBOX &&
				    resource->kind != E_WEBDAV_RESOURCE_KIND_SCHEDULE_OUTBOX &&
				    resource->kind != E_WEBDAV_RESOURCE_KIND_COLLECTION &&
				    resource->kind != E_WEBDAV_RESOURCE_KIND_PRINCIPAL)) {
					continue;
				}

				/* Some servers do not allow OPTIONS on each collection, thus ignore all errors;
				   there might not be any login errors here, but even if, then bad luck. */
				if (e_webdav_session_options_sync (session, resource->href, &capabilities, &allows, cancellable, NULL)) {
					editing_flags = webdav_browser_options_to_editing_flags (capabilities, allows);
					if (capabilities && g_hash_table_contains (capabilities, E_WEBDAV_CAPABILITY_CALENDAR_AUTO_SCHEDULE))
						resource->supports |= E_WEBDAV_DISCOVER_SUPPORTS_CALENDAR_AUTO_SCHEDULE;

				}

				if (capabilities)
					g_hash_table_destroy (capabilities);
				if (allows)
					g_hash_table_destroy (allows);

				if (!(editing_flags & E_EDITING_FLAG_HAS_OPTIONS))
					editing_flags = top_editing_flags;

				if (resource->kind == E_WEBDAV_RESOURCE_KIND_ADDRESSBOOK)
					editing_flags |= E_EDITING_FLAG_IS_BOOK;

				if (resource->kind == E_WEBDAV_RESOURCE_KIND_CALENDAR)
					editing_flags |= E_EDITING_FLAG_IS_CALENDAR | E_EDITING_FLAG_HAS_CALENDAR_COMPONENTS;

				else if (resource->kind == E_WEBDAV_RESOURCE_KIND_SCHEDULE_INBOX ||
					 resource->kind == E_WEBDAV_RESOURCE_KIND_SCHEDULE_OUTBOX)
					editing_flags |= E_EDITING_FLAG_IS_COLLECTION | E_EDITING_FLAG_HAS_CALENDAR_COMPONENTS;

				if (resource->kind == E_WEBDAV_RESOURCE_KIND_COLLECTION)
					editing_flags |= E_EDITING_FLAG_IS_COLLECTION;

				if (!g_str_has_suffix (resource->href, "/")) {
					gchar *tmp;

					tmp = g_strconcat (resource->href, "/", NULL);

					g_free (resource->href);
					resource->href = tmp;
				}

				/* Because for example Google server returns the '@' sometimes encoded and sometimes not,
				   which breaks lookup based on href in the code. */
				if (strstr (resource->href, "%40")) {
					GString *tmp;

					tmp = e_str_replace_string (resource->href, "%40", "@");
					g_free (resource->href);
					resource->href = g_string_free (tmp, FALSE);
				}

				rd = g_slice_new0 (ResourceData);
				rd->editing_flags = editing_flags;
				rd->resource = resource;

				g_mutex_lock (&webdav_browser->priv->property_lock);
				webdav_browser->priv->resources = g_slist_prepend (webdav_browser->priv->resources, rd);
				g_mutex_unlock (&webdav_browser->priv->property_lock);

				link->data = NULL;
			}
		} else if (webdav_browser_manage_login_errors (webdav_browser, session, cancellable, local_error)) {
			done = FALSE;
			g_clear_error (&local_error);
		} else if (local_error) {
			g_propagate_error (error, local_error);
			success = FALSE;
		}

		g_slist_free_full (resources, e_webdav_resource_free);
	}

	return success;
}

typedef struct _SearchChildrenData {
	GWeakRef *webdav_browser_weakref;
	GtkTreeRowReference *loading_row;
	gchar *href;
} SearchChildrenData;

static void
search_children_data_free (gpointer ptr)
{
	SearchChildrenData *scd = ptr;

	if (scd) {
		if (scd->webdav_browser_weakref)
			e_weak_ref_free (scd->webdav_browser_weakref);
		if (scd->loading_row)
			gtk_tree_row_reference_free (scd->loading_row);
		g_free (scd->href);
		g_slice_free (SearchChildrenData, scd);
	}
}

static void
webdav_browser_finish_search_children (EWebDAVBrowser *webdav_browser,
				       gpointer user_data)
{
	SearchChildrenData *scd = user_data;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (scd);

	if (gtk_tree_row_reference_valid (scd->loading_row)) {
		GtkTreeIter sort_iter, iter;
		GtkTreePath *path;
		GtkTreeModel *model;

		model = gtk_tree_row_reference_get_model (scd->loading_row);
		path = gtk_tree_row_reference_get_path (scd->loading_row);
		if (path && gtk_tree_model_get_iter (model, &sort_iter, path)) {
			gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model), &iter, &sort_iter);

			model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model));

			gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
		}

		if (path)
			gtk_tree_path_free (path);

		if (scd->href) {
			GtkTreeRowReference *reference;

			reference = g_hash_table_lookup (webdav_browser->priv->href_to_reference, scd->href);
			if (reference) {
				model = gtk_tree_row_reference_get_model (reference);
				path = gtk_tree_row_reference_get_path (reference);
				if (path && gtk_tree_model_get_iter (model, &iter, path)) {
					gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_BOOL_CHILDREN_LOADED, TRUE, -1);
				}

				if (path)
					gtk_tree_path_free (path);
			}
		}
	}
}

static void
webdav_browser_search_children_thread (EAlertSinkThreadJobData *job_data,
				       gpointer user_data,
				       GCancellable *cancellable,
				       GError **error)
{
	SearchChildrenData *scd = user_data, *scd2;
	EWebDAVBrowser *webdav_browser;
	EWebDAVSession *session;

	g_return_if_fail (scd != NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return;

	webdav_browser = g_weak_ref_get (scd->webdav_browser_weakref);
	if (!webdav_browser)
		return;

	session = webdav_browser_ref_session (webdav_browser);
	if (!session) {
		g_object_unref (webdav_browser);
		return;
	}

	webdav_browser_gather_href_resources_sync (webdav_browser, session, scd->href, FALSE, TRUE, cancellable, error);

	scd2 = g_slice_new0 (SearchChildrenData);
	scd2->loading_row = scd->loading_row;
	scd2->href = scd->href;

	scd->loading_row = NULL;
	scd->href = NULL;

	webdav_browser_schedule_ui_update (webdav_browser,
		webdav_browser_finish_search_children, scd2, search_children_data_free);

	g_object_unref (webdav_browser);
	g_object_unref (session);
}

static gboolean
webdav_browser_is_any_parent_covered (GHashTable *covered_hrefs,
				      const gchar *href)
{
	gchar *path;

	g_return_val_if_fail (covered_hrefs != NULL, FALSE);
	g_return_val_if_fail (href != NULL, FALSE);

	if (!g_hash_table_size (covered_hrefs))
		return FALSE;

	path = g_strdup (href);
	if (path) {
		gint pos;

		for (pos = strlen (path) - 1; pos > 0; pos--) {
			if (path[pos] == '/' && path[pos + 1] != '\0') {
				path[pos + 1] = '\0';

				if (g_hash_table_contains (covered_hrefs, path)) {
					g_free (path);
					return TRUE;
				}
			}
		}

		g_free (path);
	}

	return FALSE;
}

static void
webdav_browser_search_user_home_thread (EAlertSinkThreadJobData *job_data,
					gpointer user_data,
					GCancellable *cancellable,
					GError **error)
{
	EWebDAVBrowser *webdav_browser;
	EWebDAVSession *session;
	EXmlDocument *xml;
	SearchHomeData shd;
	GHashTable *checked_tops;
	ESource *source;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return;

	webdav_browser = g_weak_ref_get (user_data);
	if (!webdav_browser)
		return;

	session = webdav_browser_ref_session (webdav_browser);
	if (!session) {
		g_object_unref (webdav_browser);
		return;
	}

	xml = e_xml_document_new (E_WEBDAV_NS_DAV, "propfind");
	g_return_if_fail (xml != NULL);

	e_xml_document_start_element (xml, E_WEBDAV_NS_DAV, "prop");
	e_xml_document_add_empty_element (xml, E_WEBDAV_NS_DAV, "current-user-principal");
	e_xml_document_add_empty_element (xml, E_WEBDAV_NS_DAV, "principal-URL");
	e_xml_document_add_empty_element (xml, E_WEBDAV_NS_CALDAV, "calendar-home-set");
	e_xml_document_add_empty_element (xml, E_WEBDAV_NS_CARDDAV, "addressbook-home-set");
	e_xml_document_end_element (xml); /* prop */

	shd.covered_todo_hrefs = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);
	shd.covered_home_hrefs = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);
	shd.todo_hrefs = NULL;
	shd.home_hrefs = NULL;

	source = e_soup_session_get_source (E_SOUP_SESSION (session));
	if (source && e_source_has_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND)) {
		ESourceWebdav *webdav_extension;
		GUri *guri;

		webdav_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
		guri = e_source_webdav_dup_uri (webdav_extension);

		if (guri) {
			gchar *path;

			path = g_uri_to_string_partial (guri, G_URI_HIDE_USERINFO | G_URI_HIDE_PASSWORD);
			if (path) {
				shd.home_hrefs = g_slist_prepend (shd.home_hrefs, g_strdup (path));
				g_hash_table_insert (shd.covered_home_hrefs, path, NULL);
			}

			path = g_strdup (g_uri_get_path (guri));
			if (path) {
				gint len, pos;
				gint levels_back = 0;

				/* There is no guarantee that the parent folder is a WebDAV collection,
				   but let's try it, just in case. */
				len = strlen (path);
				for (pos = len - 1; pos > 0; pos--) {
					if (path[pos] == '/' && path[pos + 1] != '\0') {
						levels_back++;

						/* Do not go back too far, most servers has URIs like "/dav/user/collection/" */
						if (levels_back > 2)
							break;

						path[pos + 1] = '\0';

						e_util_change_uri_component (&guri, SOUP_URI_PATH, path);
						shd.todo_hrefs = g_slist_prepend (shd.todo_hrefs, g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD));
					}
				}

				g_free (path);
			}
		}

		if (guri && (!g_uri_get_path (guri) || !strstr (g_uri_get_path (guri), "/.well-known/"))) {
			e_util_change_uri_component (&guri, SOUP_URI_PATH, "/.well-known/caldav");
			shd.todo_hrefs = g_slist_prepend (shd.todo_hrefs, g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD));

			e_util_change_uri_component (&guri, SOUP_URI_PATH, "/.well-known/carddav");
			shd.todo_hrefs = g_slist_prepend (shd.todo_hrefs, g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD));
		}

		if (guri) {
			e_util_change_uri_component (&guri, SOUP_URI_PATH, "");
			shd.todo_hrefs = g_slist_prepend (shd.todo_hrefs, g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD));
			g_uri_unref (guri);
		}
	}

	/* Try the URL provided in the ESource first */
	shd.todo_hrefs = g_slist_prepend (shd.todo_hrefs, NULL);

	checked_tops = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal, g_free, NULL);

	while (shd.todo_hrefs &&
	       !g_cancellable_set_error_if_cancelled (cancellable, error)) {
		gchar *top_href;
		gboolean done;
		GError *local_error = NULL;

		top_href = shd.todo_hrefs->data;
		shd.todo_hrefs = g_slist_remove (shd.todo_hrefs, top_href);

		done = top_href && g_hash_table_contains (checked_tops, top_href);

		if (top_href)
			g_hash_table_insert (checked_tops, g_strdup (top_href), NULL);

		while (!done) {
			done = TRUE;

			if (e_webdav_session_propfind_sync (session, top_href, E_WEBDAV_DEPTH_THIS, xml,
				webdav_browser_search_home_hrefs_cb, &shd, cancellable, &local_error)) {
			} else if (webdav_browser_manage_login_errors (webdav_browser, session, cancellable, local_error)) {
				done = FALSE;
			}

			/* Ignore all but login errors here, because some of the URIs are just guesses */
			g_clear_error (&local_error);
		}

		g_free (top_href);
	}

	g_hash_table_destroy (checked_tops);

	if (!shd.home_hrefs) {
		ESourceWebdav *webdav_extension;
		GUri *guri;

		webdav_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
		guri = e_source_webdav_dup_uri (webdav_extension);

		if (guri) {
			gchar *path;

			path = g_uri_to_string_partial (guri, G_URI_HIDE_USERINFO | G_URI_HIDE_PASSWORD);
			if (path) {
				gint len, pos;
				gint levels_back = 0;

				shd.home_hrefs = g_slist_prepend (shd.home_hrefs, g_strdup (path));

				/* There is no guarantee that the parent folder is a WebDAV collection,
				   but let's try it, just in case. */
				len = strlen (path);
				for (pos = len - 1; pos > 0; pos--) {
					if (path[pos] == '/' && path[pos + 1] != '\0') {
						gchar *href;

						levels_back++;

						/* Do not go back too far, most servers has URIs like "/dav/user/collection/" */
						if (levels_back > 2)
							break;

						path[pos + 1] = '\0';

						e_util_change_uri_component (&guri, SOUP_URI_PATH, path);
						href = g_uri_to_string_partial (guri, G_URI_HIDE_USERINFO | G_URI_HIDE_PASSWORD);

						if (!g_hash_table_contains (shd.covered_home_hrefs, href))
							shd.home_hrefs = g_slist_prepend (shd.home_hrefs, href);
						else
							g_free (href);
					}
				}

				g_free (path);
			}

			g_uri_unref (guri);
		}
	}

	g_hash_table_remove_all (shd.covered_home_hrefs);
	shd.home_hrefs = g_slist_sort (shd.home_hrefs, (GCompareFunc) g_strcmp0);

	while (!g_cancellable_is_cancelled (cancellable) && shd.home_hrefs) {
		gchar *home_href;

		home_href = shd.home_hrefs->data;
		shd.home_hrefs = g_slist_remove (shd.home_hrefs, home_href);

		if (webdav_browser_is_any_parent_covered (shd.covered_home_hrefs, home_href)) {
			g_free (home_href);
		} else {
			/* Ignore errors here as well */
			webdav_browser_gather_href_resources_sync (webdav_browser, session, home_href, TRUE, TRUE, cancellable, NULL);
			g_hash_table_insert (shd.covered_home_hrefs, home_href, NULL);
		}
	}

	webdav_browser_schedule_ui_update (webdav_browser, NULL, NULL, NULL);

	g_hash_table_destroy (shd.covered_todo_hrefs);
	g_hash_table_destroy (shd.covered_home_hrefs);
	g_slist_free_full (shd.todo_hrefs, g_free);
	g_slist_free_full (shd.home_hrefs, g_free);
	g_object_unref (webdav_browser);
	g_object_unref (session);
	g_clear_object (&xml);
}

static void
webdav_browser_selection_changed_cb (GtkTreeSelection *selection,
				     gpointer user_data)
{
	EWebDAVBrowser *webdav_browser = user_data;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	guint editing_flags = E_EDITING_FLAG_NONE;
	gboolean has_parent = FALSE;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		GtkTreeIter parent;

		gtk_tree_model_get (model, &iter,
			COLUMN_UINT_EDITING_FLAGS, &editing_flags,
			-1);

		has_parent = gtk_tree_model_iter_parent (model, &parent, &iter);
	}

	#define has_set(x) ((editing_flags & (x)) == (x))

	gtk_widget_set_sensitive (webdav_browser->priv->create_book_button,
		has_set (E_EDITING_FLAG_EXMKCOL | E_EDITING_FLAG_CAN_BOOK));

	gtk_widget_set_sensitive (webdav_browser->priv->create_calendar_button,
		has_set (E_EDITING_FLAG_MKCALENDAR | E_EDITING_FLAG_CAN_CALENDAR));

	gtk_widget_set_sensitive (webdav_browser->priv->create_collection_button,
		has_set (E_EDITING_FLAG_MKCOL));

	gtk_widget_set_sensitive (webdav_browser->priv->edit_button,
		(editing_flags & (E_EDITING_FLAG_IS_BOOK | E_EDITING_FLAG_IS_CALENDAR | E_EDITING_FLAG_IS_COLLECTION)) != 0);

	gtk_widget_set_sensitive (webdav_browser->priv->delete_button,
		has_set (E_EDITING_FLAG_CAN_DELETE) && has_parent);

	/* gtk_widget_set_sensitive (webdav_browser->priv->permissions_button,
		has_set (E_EDITING_FLAG_CAN_ACL)); */

	#undef has_set
}

static void
webdav_browser_change_busy_state (EWebDAVBrowser *webdav_browser,
				  gboolean is_busy)
{
	gtk_widget_set_sensitive (webdav_browser->priv->tree_view, !is_busy);

	if (is_busy) {
		gtk_widget_set_sensitive (webdav_browser->priv->create_book_button, FALSE);
		gtk_widget_set_sensitive (webdav_browser->priv->create_calendar_button, FALSE);
		gtk_widget_set_sensitive (webdav_browser->priv->create_collection_button, FALSE);
		gtk_widget_set_sensitive (webdav_browser->priv->edit_button, FALSE);
		gtk_widget_set_sensitive (webdav_browser->priv->delete_button, FALSE);
		/* gtk_widget_set_sensitive (webdav_browser->priv->permissions_button, FALSE); */
		gtk_widget_set_sensitive (webdav_browser->priv->refresh_button, FALSE);

		e_alert_bar_clear (webdav_browser->priv->alert_bar);
	} else {
		GtkTreeView *tree_view;

		tree_view = GTK_TREE_VIEW (webdav_browser->priv->tree_view);

		webdav_browser_selection_changed_cb (gtk_tree_view_get_selection (tree_view), webdav_browser);

		gtk_widget_set_sensitive (webdav_browser->priv->refresh_button, webdav_browser->priv->session != NULL);
	}
}

static void
webdav_browser_row_expanded_cb (GtkTreeView *tree_view,
				GtkTreeIter *iter,
				GtkTreePath *path,
				gpointer user_data)
{
	EWebDAVBrowser *webdav_browser = user_data;
	EActivity *activity;
	GtkTreeModel *model;
	GtkTreeIter loading_child;
	GtkTreePath *loading_path;
	SearchChildrenData *scd;
	gboolean is_loaded = TRUE;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
	g_return_if_fail (iter);

	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_get (model, iter, COLUMN_BOOL_CHILDREN_LOADED, &is_loaded, -1);

	if (is_loaded)
		return;

	g_return_if_fail (gtk_tree_model_iter_nth_child (model, &loading_child, iter, 0));
	g_return_if_fail (webdav_browser->priv->session);

	scd = g_slice_new0 (SearchChildrenData);
	scd->webdav_browser_weakref = e_weak_ref_new (webdav_browser);

	loading_path = gtk_tree_model_get_path (model, &loading_child);
	scd->loading_row = gtk_tree_row_reference_new (model, loading_path);
	gtk_tree_path_free (loading_path);

	gtk_tree_model_get (model, iter, COLUMN_STRING_HREF, &scd->href, -1);

	e_webdav_browser_abort (webdav_browser);

	g_clear_object (&webdav_browser->priv->cancellable);

	webdav_browser_change_busy_state (webdav_browser, TRUE);

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (webdav_browser),
		_("Searching collection children…"),
		"system:generic-error",
		_("Failed to search for collection children"),
		webdav_browser_search_children_thread, scd,
		search_children_data_free);

	if (activity) {
		webdav_browser->priv->cancellable = e_activity_get_cancellable (activity);

		if (webdav_browser->priv->cancellable)
			g_object_ref (webdav_browser->priv->cancellable);

		e_activity_bar_set_activity (webdav_browser->priv->activity_bar, activity);

		g_object_unref (activity);
	} else {
		webdav_browser_change_busy_state (webdav_browser, FALSE);
		webdav_browser_schedule_ui_update (webdav_browser, NULL, NULL, NULL);
	}
}

static void
webdav_browser_search_user_home (EWebDAVBrowser *webdav_browser)
{
	EActivity *activity;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (webdav_browser->priv->session);

	e_webdav_browser_abort (webdav_browser);

	g_clear_object (&webdav_browser->priv->cancellable);

	webdav_browser_change_busy_state (webdav_browser, TRUE);

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (webdav_browser),
		_("Searching for user home, please wait…"),
		"system:generic-error",
		_("Failed to search for user home"),
		webdav_browser_search_user_home_thread,
		e_weak_ref_new (webdav_browser),
		(GDestroyNotify) e_weak_ref_free);

	if (activity) {
		webdav_browser->priv->cancellable = e_activity_get_cancellable (activity);

		if (webdav_browser->priv->cancellable)
			g_object_ref (webdav_browser->priv->cancellable);

		e_activity_bar_set_activity (webdav_browser->priv->activity_bar, activity);

		g_object_unref (activity);
	} else {
		webdav_browser_change_busy_state (webdav_browser, FALSE);
		webdav_browser_schedule_ui_update (webdav_browser, NULL, NULL, NULL);
	}
}

static gchar *
webdav_browser_dup_selected_href (EWebDAVBrowser *webdav_browser)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	gchar *href = NULL;

	g_return_val_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser), NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (webdav_browser->priv->tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COLUMN_STRING_HREF, &href, -1);

	return href;
}

static gboolean
webdav_browser_get_selected_loaded (EWebDAVBrowser *webdav_browser)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	gboolean is_loaded = FALSE;

	g_return_val_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (webdav_browser->priv->tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get (model, &iter, COLUMN_BOOL_CHILDREN_LOADED, &is_loaded, -1);

	return is_loaded;
}

static gboolean
webdav_browser_any_parent_is_book_or_calendar (EWebDAVBrowser *webdav_browser)
{
	GtkTreeModel *model;
	GtkTreeIter iter, parent;
	GtkTreeSelection *selection;
	gboolean done;

	g_return_val_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (webdav_browser->priv->tree_view));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return FALSE;

	do {
		guint editing_flags = E_EDITING_FLAG_NONE;

		gtk_tree_model_get (model, &iter, COLUMN_UINT_EDITING_FLAGS, &editing_flags, -1);

		if ((editing_flags & (E_EDITING_FLAG_IS_BOOK | E_EDITING_FLAG_IS_CALENDAR)) != 0)
			return TRUE;

		done = !gtk_tree_model_iter_parent (model, &parent, &iter);
		if (!done)
			iter = parent;
	} while (!done);

	return FALSE;
}

static void
webdav_browser_drop_loading_node_for_href (EWebDAVBrowser *webdav_browser,
					   gpointer user_data)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter, child;
	const gchar *href = user_data;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (href != NULL);

	reference = g_hash_table_lookup (webdav_browser->priv->href_to_reference, href);
	if (!reference && !g_str_has_suffix (href, "/")) {
		gchar *tmp;

		tmp = g_strconcat (href, "/", NULL);
		reference = g_hash_table_lookup (webdav_browser->priv->href_to_reference, tmp);
		g_free (tmp);
	}

	if (!reference)
		return;

	model = gtk_tree_row_reference_get_model (reference);
	path = gtk_tree_row_reference_get_path (reference);

	if (!path)
		return;

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_BOOL_CHILDREN_LOADED, TRUE, -1);

		/* Remove "Loading…" row */
		if (gtk_tree_model_iter_nth_child (model, &child, &iter, 0)) {
			do {
				guint flags = E_EDITING_FLAG_NONE;

				gtk_tree_model_get (model, &child, COLUMN_UINT_EDITING_FLAGS, &flags, -1);

				if (flags == E_EDITING_FLAG_IS_LOADING_ROW) {
					gtk_tree_store_remove (GTK_TREE_STORE (model), &child);
					break;
				}
			} while (gtk_tree_model_iter_next (model, &child));
		}
	}

	gtk_tree_path_free (path);
}

typedef struct _SaveChangesData {
	GWeakRef *webdav_browser_weakref;
	gchar *href;
	gboolean is_edit; /* TRUE for save changes, FALSE to create new under href */
	gboolean load_first;
	gchar *name;
	GdkRGBA rgba;
	gint order;
	guint32 supports; /* bit-or of EWebDAVResourceSupports */
	gchar *description;
	gboolean success;
} SaveChangesData;

static void
save_changes_data_free (gpointer ptr)
{
	SaveChangesData *scd = ptr;

	if (scd) {
		if (scd->success) {
			EWebDAVBrowser *webdav_browser;

			webdav_browser = g_weak_ref_get (scd->webdav_browser_weakref);

			if (webdav_browser) {
				webdav_browser->priv->refresh_collection = TRUE;
				g_object_unref (webdav_browser);
			}
		}

		e_weak_ref_free (scd->webdav_browser_weakref);
		g_free (scd->href);
		g_free (scd->name);
		g_free (scd->description);
		g_slice_free (SaveChangesData, scd);
	}
}

static void
webdav_browser_save_changes_thread (EAlertSinkThreadJobData *job_data,
				    gpointer user_data,
				    GCancellable *cancellable,
				    GError **error)
{
	EWebDAVBrowser *webdav_browser;
	EWebDAVSession *session;
	SaveChangesData *scd = user_data;
	gchar *new_href = NULL;
	gboolean success = FALSE;

	g_return_if_fail (scd != NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return;

	webdav_browser = g_weak_ref_get (scd->webdav_browser_weakref);
	if (!webdav_browser)
		return;

	session = webdav_browser_ref_session (webdav_browser);
	if (!session) {
		g_object_unref (webdav_browser);
		return;
	}

	if (scd->load_first)
		webdav_browser_gather_href_resources_sync (webdav_browser, session, scd->href, FALSE, TRUE, cancellable, NULL);

	if (scd->is_edit) {
		GSList *changes = NULL;

		changes = g_slist_append (changes, e_webdav_property_change_new_set (E_WEBDAV_NS_DAV, "displayname", scd->name));

		if ((scd->supports & E_WEBDAV_RESOURCE_SUPPORTS_CONTACTS) != 0) {
			if (scd->description && *scd->description)
				changes = g_slist_append (changes, e_webdav_property_change_new_set (E_WEBDAV_NS_CARDDAV, "addressbook-description", scd->description));
			else
				changes = g_slist_append (changes, e_webdav_property_change_new_remove (E_WEBDAV_NS_CARDDAV, "addressbook-description"));
		} else if ((scd->supports & (E_WEBDAV_RESOURCE_SUPPORTS_EVENTS | E_WEBDAV_RESOURCE_SUPPORTS_MEMOS | E_WEBDAV_RESOURCE_SUPPORTS_TASKS)) != 0) {
			if (scd->rgba.alpha <= 1.0 - 1e-9) {
				changes = g_slist_append (changes, e_webdav_property_change_new_remove (E_WEBDAV_NS_ICAL, "calendar-color"));
			} else {
				gchar *color;

				color = g_strdup_printf ("#%02x%02x%02x",
					(gint) CLAMP (scd->rgba.red * 0xFF, 0, 0xFF),
					(gint) CLAMP (scd->rgba.green * 0xFF, 0, 0xFF),
					(gint) CLAMP (scd->rgba.blue * 0xFF, 0, 0xFF));

				changes = g_slist_append (changes, e_webdav_property_change_new_set (E_WEBDAV_NS_ICAL, "calendar-color", color));

				g_free (color);
			}

			if (scd->order >= 0) {
				gchar order_str[64];

				g_snprintf (order_str, sizeof (order_str), "%u", (guint) scd->order);

				changes = g_slist_append (changes, e_webdav_property_change_new_set (E_WEBDAV_NS_ICAL, "calendar-order", order_str));
			} else {
				changes = g_slist_append (changes, e_webdav_property_change_new_remove (E_WEBDAV_NS_ICAL, "calendar-order"));
			}

			if (scd->description && *scd->description)
				changes = g_slist_append (changes, e_webdav_property_change_new_set (E_WEBDAV_NS_CALDAV, "calendar-description", scd->description));
			else
				changes = g_slist_append (changes, e_webdav_property_change_new_remove (E_WEBDAV_NS_CALDAV, "calendar-description"));
		}

		success = e_webdav_session_update_properties_sync (session, scd->href, changes, cancellable, error);

		g_slist_free_full (changes, e_webdav_property_change_free);
	} else {
		GUri *guri;
		GString *path;
		gchar *encoded;

		guri = g_uri_parse (scd->href, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
		path = g_string_new (g_uri_get_path (guri));

		if (path->len && path->str[path->len - 1] != '/')
			g_string_append_c (path, '/');

		encoded = g_uri_escape_string (scd->name, NULL, FALSE);
		g_string_append (path, encoded);
		g_free (encoded);

		e_util_change_uri_component (&guri, SOUP_URI_PATH, path->str);

		new_href = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);

		if ((scd->supports & E_WEBDAV_RESOURCE_SUPPORTS_CONTACTS) != 0) {
			success = e_webdav_session_mkcol_addressbook_sync (session, new_href,
				scd->name, scd->description, cancellable, error);
		} else if ((scd->supports & (E_WEBDAV_RESOURCE_SUPPORTS_EVENTS | E_WEBDAV_RESOURCE_SUPPORTS_MEMOS | E_WEBDAV_RESOURCE_SUPPORTS_TASKS)) != 0) {
			gchar *color;

			color = g_strdup_printf ("#%02x%02x%02x",
				(gint) CLAMP (scd->rgba.red * 0xFF, 0, 0xFF),
				(gint) CLAMP (scd->rgba.green * 0xFF, 0, 0xFF),
				(gint) CLAMP (scd->rgba.blue * 0xFF, 0, 0xFF));

			success = e_webdav_session_mkcalendar_sync (session, new_href,
				scd->name, scd->description, color,
				scd->supports, cancellable, error);

			g_free (color);

			if (success && scd->order >= 0) {
				GSList *changes = NULL;
				GError *local_error = NULL;
				gchar order_str[64];

				g_snprintf (order_str, sizeof (order_str), "%u", (guint) scd->order);

				changes = g_slist_append (changes, e_webdav_property_change_new_set (E_WEBDAV_NS_ICAL, "calendar-order", order_str));

				/* Treat the failure to save calendar-order as non-fatal. */
				if (!e_webdav_session_update_properties_sync (session, new_href, changes, cancellable, &local_error)) {
					if (g_strcmp0 (g_getenv ("WEBDAV_DEBUG"), "1") == 0) {
						e_util_debug_print ("WEBDAV", "Failed to set calendar-order: %s", local_error ? local_error->message : "Unknown error");
					}

					g_clear_error (&local_error);
				}

				g_slist_free_full (changes, e_webdav_property_change_free);
			}
		} else {
			success = e_webdav_session_mkcol_sync (session, new_href, cancellable, error);
		}

		g_string_free (path, TRUE);
		g_uri_unref (guri);
	}

	if (success) {
		const gchar *href = new_href ? new_href : scd->href;

		if (scd->load_first) {
			GSList *link;

			for (link = webdav_browser->priv->resources; link; link = g_slist_next (link)) {
				ResourceData *rd = link->data;

				if (rd && rd->resource && rd->resource->href &&
				    g_strcmp0 (rd->resource->href, href) == 0) {
					webdav_browser->priv->resources = g_slist_remove (webdav_browser->priv->resources, rd);
					resource_data_free (rd);
					break;
				}
			}
		}

		webdav_browser_gather_href_resources_sync (webdav_browser, session, href, FALSE, FALSE, cancellable, error);

		if (!scd->is_edit)
			webdav_browser_schedule_ui_update (webdav_browser,
				webdav_browser_drop_loading_node_for_href, g_strdup (href), g_free);
		else
			webdav_browser_schedule_ui_update (webdav_browser, NULL, NULL, NULL);
	} else {
		webdav_browser_schedule_ui_update (webdav_browser, NULL, NULL, NULL);
	}

	scd->success = success;

	g_object_unref (webdav_browser);
	g_object_unref (session);
	g_free (new_href);
}

static void
webdav_browser_save_clicked (EWebDAVBrowser *webdav_browser,
			     gboolean is_book,
			     gboolean is_calendar,
			     gboolean is_edit)
{
	EActivity *activity;
	SaveChangesData *scd;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	guint32 supports = 0;
	gchar *text, *href;
	const gchar *description;
	const gchar *error_message;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));

	text = g_strdup (gtk_entry_get_text (GTK_ENTRY (webdav_browser->priv->create_edit_name_entry)));
	if (text)
		text = g_strstrip (text);

	if (!text || !*text) {
		gtk_widget_hide (webdav_browser->priv->create_edit_hint_popover);

		gtk_label_set_text (GTK_LABEL (webdav_browser->priv->create_edit_hint_label), _("Name cannot be empty"));
		gtk_popover_set_relative_to (GTK_POPOVER (webdav_browser->priv->create_edit_hint_popover),
			webdav_browser->priv->create_edit_name_entry);

		gtk_widget_set_sensitive (webdav_browser->priv->create_edit_hint_popover, TRUE);
		gtk_widget_show (webdav_browser->priv->create_edit_hint_popover);

		g_free (text);

		return;
	} else if (is_calendar &&
		   !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_event_check)) &&
		   !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_memo_check)) &&
		   !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_task_check))) {
		gtk_widget_hide (webdav_browser->priv->create_edit_hint_popover);

		gtk_label_set_text (GTK_LABEL (webdav_browser->priv->create_edit_hint_label), _("At least one component type should be set"));
		gtk_popover_set_relative_to (GTK_POPOVER (webdav_browser->priv->create_edit_hint_popover),
			webdav_browser->priv->create_edit_task_check);

		gtk_widget_set_sensitive (webdav_browser->priv->create_edit_hint_popover, TRUE);
		gtk_widget_show (webdav_browser->priv->create_edit_hint_popover);

		g_free (text);

		return;
	}

	gtk_widget_hide (webdav_browser->priv->create_edit_popover);

	href = webdav_browser_dup_selected_href (webdav_browser);
	if (!href || !*href) {
		g_free (href);
		g_free (text);

		webdav_browser_add_alert (webdav_browser, _("Failed to get selected collection HREF"), NULL);

		return;
	}

	if (is_book) {
		supports = E_WEBDAV_RESOURCE_SUPPORTS_CONTACTS;
	} else if (is_calendar) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_event_check)))
			supports |= E_WEBDAV_RESOURCE_SUPPORTS_EVENTS;
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_memo_check)))
			supports |= E_WEBDAV_RESOURCE_SUPPORTS_MEMOS;
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_task_check)))
			supports |= E_WEBDAV_RESOURCE_SUPPORTS_TASKS;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (webdav_browser->priv->create_edit_description_textview));
	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);

	scd = g_slice_new0 (SaveChangesData);
	scd->webdav_browser_weakref = e_weak_ref_new (webdav_browser);
	scd->href = href;
	scd->is_edit = is_edit;
	scd->load_first = !webdav_browser_get_selected_loaded (webdav_browser);
	scd->name = text;
	e_color_combo_get_current_color (E_COLOR_COMBO (webdav_browser->priv->create_edit_color_combo), &scd->rgba);
	scd->order = gtk_spin_button_get_value (GTK_SPIN_BUTTON (webdav_browser->priv->create_edit_order_spin));
	scd->supports = supports;
	scd->description = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	if (is_edit) {
		description = _("Saving changes…");
		error_message = _("Failed to save changes");
	} else if (is_book) {
		description = _("Creating new book…");
		error_message = _("Failed to create new book");
	} else if (is_calendar) {
		description = _("Creating new calendar…");
		error_message = _("Failed to create new calendar");
	} else {
		description = _("Creating new collection…");
		error_message = _("Failed to create new collection");
	}

	e_webdav_browser_abort (webdav_browser);

	g_clear_object (&webdav_browser->priv->cancellable);

	webdav_browser_change_busy_state (webdav_browser, TRUE);

	activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (webdav_browser),
		description, "system:generic-error", error_message,
		webdav_browser_save_changes_thread, scd,
		save_changes_data_free);

	if (activity) {
		webdav_browser->priv->cancellable = e_activity_get_cancellable (activity);

		if (webdav_browser->priv->cancellable)
			g_object_ref (webdav_browser->priv->cancellable);

		e_activity_bar_set_activity (webdav_browser->priv->activity_bar, activity);

		g_object_unref (activity);
	} else {
		webdav_browser_change_busy_state (webdav_browser, FALSE);
		webdav_browser_schedule_ui_update (webdav_browser, NULL, NULL, NULL);
	}
}

static void
webdav_browser_create_book_save_clicked_cb (GtkWidget *button,
					    EWebDAVBrowser *webdav_browser)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (GTK_IS_POPOVER (webdav_browser->priv->create_edit_popover));

	webdav_browser_save_clicked (webdav_browser, TRUE, FALSE, FALSE);
}

static void
webdav_browser_create_calendar_save_clicked_cb (GtkWidget *button,
						EWebDAVBrowser *webdav_browser)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (GTK_IS_POPOVER (webdav_browser->priv->create_edit_popover));

	webdav_browser_save_clicked (webdav_browser, FALSE, TRUE, FALSE);
}

static void
webdav_browser_create_collection_save_clicked_cb (GtkWidget *button,
						  EWebDAVBrowser *webdav_browser)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (GTK_IS_POPOVER (webdav_browser->priv->create_edit_popover));

	gtk_widget_hide (webdav_browser->priv->create_edit_popover);

	webdav_browser_save_clicked (webdav_browser, FALSE, FALSE, FALSE);
}

static void
webdav_browser_edit_book_save_clicked_cb (GtkWidget *button,
					  EWebDAVBrowser *webdav_browser)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (GTK_IS_POPOVER (webdav_browser->priv->create_edit_popover));

	webdav_browser_save_clicked (webdav_browser, TRUE, FALSE, TRUE);
}

static void
webdav_browser_edit_calendar_save_clicked_cb (GtkWidget *button,
					      EWebDAVBrowser *webdav_browser)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (GTK_IS_POPOVER (webdav_browser->priv->create_edit_popover));

	webdav_browser_save_clicked (webdav_browser, FALSE, TRUE, TRUE);
}

static void
webdav_browser_edit_collection_save_clicked_cb (GtkWidget *button,
						EWebDAVBrowser *webdav_browser)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (GTK_IS_POPOVER (webdav_browser->priv->create_edit_popover));

	gtk_widget_hide (webdav_browser->priv->create_edit_popover);

	webdav_browser_save_clicked (webdav_browser, FALSE, FALSE, TRUE);
}

static void
webdav_browser_prepare_popover (EWebDAVBrowser *webdav_browser,
				guint32 editing_flags)
{
	GdkRGBA rgba;
	gboolean for_book = (editing_flags & E_EDITING_FLAG_IS_BOOK) != 0;
	gboolean for_calendar = (editing_flags & E_EDITING_FLAG_IS_CALENDAR) != 0;
	gboolean has_calendar_components = (editing_flags & E_EDITING_FLAG_HAS_CALENDAR_COMPONENTS) != 0;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));

	gtk_widget_hide (webdav_browser->priv->create_edit_popover);

	gtk_widget_set_visible (webdav_browser->priv->create_edit_color_label, for_calendar);
	gtk_widget_set_visible (webdav_browser->priv->create_edit_color_combo, for_calendar);
	gtk_widget_set_visible (webdav_browser->priv->create_edit_order_label, for_calendar);
	gtk_widget_set_visible (webdav_browser->priv->create_edit_order_spin, for_calendar);
	gtk_widget_set_visible (webdav_browser->priv->create_edit_support_label, has_calendar_components);
	gtk_widget_set_visible (webdav_browser->priv->create_edit_event_check, has_calendar_components);
	gtk_widget_set_visible (webdav_browser->priv->create_edit_memo_check, has_calendar_components);
	gtk_widget_set_visible (webdav_browser->priv->create_edit_task_check, has_calendar_components);
	gtk_widget_set_visible (webdav_browser->priv->create_edit_description_label, for_book || for_calendar);
	gtk_widget_set_visible (webdav_browser->priv->create_edit_description_scrolled_window, for_book || for_calendar);

	gtk_widget_set_sensitive (webdav_browser->priv->create_edit_support_label, TRUE);
	gtk_widget_set_sensitive (webdav_browser->priv->create_edit_event_check, TRUE);
	gtk_widget_set_sensitive (webdav_browser->priv->create_edit_memo_check, TRUE);
	gtk_widget_set_sensitive (webdav_browser->priv->create_edit_task_check, TRUE);

	gtk_widget_hide (webdav_browser->priv->create_edit_hint_popover);

	rgba.red = 0.0;
	rgba.green = 0.0;
	rgba.blue = 0.0;
	rgba.alpha = 0.001;

	gtk_entry_set_text (GTK_ENTRY (webdav_browser->priv->create_edit_name_entry), "");
	e_color_combo_set_current_color (E_COLOR_COMBO (webdav_browser->priv->create_edit_color_combo), &rgba);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (webdav_browser->priv->create_edit_order_spin), -1);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_event_check), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_memo_check), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_task_check), FALSE);
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (webdav_browser->priv->create_edit_description_textview)), "", -1);
}

static void
webdav_browser_create_clicked_cb (GtkWidget *button,
				  EWebDAVBrowser *webdav_browser)
{
	GCallback callback;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (GTK_IS_POPOVER (webdav_browser->priv->create_edit_popover));

	if (button != webdav_browser->priv->create_collection_button &&
	    webdav_browser_any_parent_is_book_or_calendar (webdav_browser)) {
		const gchar *msg;

		if (button == webdav_browser->priv->create_book_button)
			msg = _("It is not allowed to create book under another book or calendar");
		else /* if (button == webdav_browser->priv->create_calendar_button) */
			msg = _("It is not allowed to create calendar under another book or calendar");

		gtk_widget_hide (webdav_browser->priv->create_edit_hint_popover);

		gtk_label_set_text (GTK_LABEL (webdav_browser->priv->create_edit_hint_label), msg);
		gtk_popover_set_relative_to (GTK_POPOVER (webdav_browser->priv->create_edit_hint_popover), button);

		gtk_widget_set_sensitive (webdav_browser->priv->create_edit_hint_popover, TRUE);
		gtk_widget_show (webdav_browser->priv->create_edit_hint_popover);

		return;
	}

	webdav_browser_prepare_popover (webdav_browser,
		(button == webdav_browser->priv->create_book_button ? E_EDITING_FLAG_IS_BOOK : 0) |
		(button == webdav_browser->priv->create_calendar_button ? E_EDITING_FLAG_IS_CALENDAR | E_EDITING_FLAG_HAS_CALENDAR_COMPONENTS: 0));

	gtk_popover_set_relative_to (GTK_POPOVER (webdav_browser->priv->create_edit_popover), button);

	g_signal_handlers_disconnect_by_data (webdav_browser->priv->create_edit_save_button, webdav_browser);

	if (button == webdav_browser->priv->create_book_button)
		callback = G_CALLBACK (webdav_browser_create_book_save_clicked_cb);
	else if (button == webdav_browser->priv->create_calendar_button)
		callback = G_CALLBACK (webdav_browser_create_calendar_save_clicked_cb);
	else
		callback = G_CALLBACK (webdav_browser_create_collection_save_clicked_cb);

	g_signal_connect (webdav_browser->priv->create_edit_save_button, "clicked", callback, webdav_browser);

	gtk_widget_set_sensitive (webdav_browser->priv->create_edit_popover, TRUE);
	gtk_widget_show (webdav_browser->priv->create_edit_popover);

	gtk_widget_grab_focus (webdav_browser->priv->create_edit_name_entry);
}

static void
webdav_browser_edit_clicked_cb (GtkWidget *button,
				EWebDAVBrowser *webdav_browser)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	gchar *href;
	gchar *display_name = NULL, *description = NULL;
	guint editing_flags = E_EDITING_FLAG_NONE, supports = E_WEBDAV_RESOURCE_SUPPORTS_NONE;
	gint order = -1;
	GdkRGBA *rgba = NULL;
	gboolean color_is_set = FALSE;
	GCallback callback;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (GTK_IS_POPOVER (webdav_browser->priv->create_edit_popover));

	href = webdav_browser_dup_selected_href (webdav_browser);
	g_return_if_fail (href != NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (webdav_browser->priv->tree_view));
	g_return_if_fail (gtk_tree_selection_get_selected (selection, &model, &iter));

	gtk_tree_model_get (model, &iter,
		COLUMN_STRING_DISPLAY_NAME, &display_name,
		COLUMN_STRING_DESCRIPTION, &description,
		COLUMN_RGBA_COLOR, &rgba,
		COLUMN_BOOL_COLOR_VISIBLE, &color_is_set,
		COLUMN_INT_ORDER, &order,
		COLUMN_UINT_EDITING_FLAGS, &editing_flags,
		COLUMN_UINT_SUPPORTS, &supports,
		-1);

	webdav_browser_prepare_popover (webdav_browser, editing_flags);

	if ((editing_flags & (E_EDITING_FLAG_IS_CALENDAR | E_EDITING_FLAG_HAS_CALENDAR_COMPONENTS)) != 0) {
		if (color_is_set && rgba && ((editing_flags & E_EDITING_FLAG_IS_CALENDAR) != 0))
			e_color_combo_set_current_color (E_COLOR_COMBO (webdav_browser->priv->create_edit_color_combo), rgba);

		gtk_spin_button_set_value (GTK_SPIN_BUTTON (webdav_browser->priv->create_edit_order_spin), order);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_event_check),
			(supports & E_WEBDAV_RESOURCE_SUPPORTS_EVENTS) != 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_memo_check),
			(supports & E_WEBDAV_RESOURCE_SUPPORTS_MEMOS) != 0);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (webdav_browser->priv->create_edit_task_check),
			(supports & E_WEBDAV_RESOURCE_SUPPORTS_TASKS) != 0);

		gtk_widget_set_sensitive (webdav_browser->priv->create_edit_support_label, FALSE);
		gtk_widget_set_sensitive (webdav_browser->priv->create_edit_event_check, FALSE);
		gtk_widget_set_sensitive (webdav_browser->priv->create_edit_memo_check, FALSE);
		gtk_widget_set_sensitive (webdav_browser->priv->create_edit_task_check, FALSE);
	}

	gtk_entry_set_text (GTK_ENTRY (webdav_browser->priv->create_edit_name_entry), display_name);

	if (description) {
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (webdav_browser->priv->create_edit_description_textview)),
			description, -1);
	}

	gtk_popover_set_relative_to (GTK_POPOVER (webdav_browser->priv->create_edit_popover), button);

	g_signal_handlers_disconnect_by_data (webdav_browser->priv->create_edit_save_button, webdav_browser);

	if ((editing_flags & E_EDITING_FLAG_IS_BOOK) != 0)
		callback = G_CALLBACK (webdav_browser_edit_book_save_clicked_cb);
	else if ((editing_flags & E_EDITING_FLAG_IS_CALENDAR) != 0)
		callback = G_CALLBACK (webdav_browser_edit_calendar_save_clicked_cb);
	else
		callback = G_CALLBACK (webdav_browser_edit_collection_save_clicked_cb);

	g_signal_connect (webdav_browser->priv->create_edit_save_button, "clicked", callback, webdav_browser);

	gtk_widget_set_sensitive (webdav_browser->priv->create_edit_popover, TRUE);
	gtk_widget_show (webdav_browser->priv->create_edit_popover);

	gtk_widget_grab_focus (webdav_browser->priv->create_edit_name_entry);

	g_free (href);
	g_free (description);
	g_free (display_name);

	if (rgba)
		gdk_rgba_free (rgba);
}

typedef struct _DeleteData {
	GWeakRef *webdav_browser_weakref;
	gchar *href;
	gboolean success;
} DeleteData;

static void
delete_data_free (gpointer ptr)
{
	DeleteData *dd = ptr;

	if (dd) {
		if (dd->success) {
			EWebDAVBrowser *webdav_browser;

			webdav_browser = g_weak_ref_get (dd->webdav_browser_weakref);

			if (webdav_browser) {
				webdav_browser->priv->refresh_collection = TRUE;
				g_object_unref (webdav_browser);
			}
		}

		e_weak_ref_free (dd->webdav_browser_weakref);
		g_free (dd->href);
		g_slice_free (DeleteData, dd);
	}
}

static void
webdav_browser_delete_done (EWebDAVBrowser *webdav_browser,
			    gpointer user_data)
{
	GtkTreeRowReference *reference;
	const gchar *href = user_data;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (href != NULL);

	reference = g_hash_table_lookup (webdav_browser->priv->href_to_reference, href);
	if (reference) {
		GtkTreeModel *model;
		GtkTreePath *path;
		GtkTreeIter iter;

		model = gtk_tree_row_reference_get_model (reference);
		path = gtk_tree_row_reference_get_path (reference);

		if (gtk_tree_model_get_iter (model, &iter, path))
			gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

		gtk_tree_path_free (path);
	}
}

static void
webdav_browser_delete_thread (EAlertSinkThreadJobData *job_data,
			      gpointer user_data,
			      GCancellable *cancellable,
			      GError **error)
{
	EWebDAVBrowser *webdav_browser;
	EWebDAVSession *session;
	DeleteData *dd = user_data;

	g_return_if_fail (dd != NULL);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return;

	webdav_browser = g_weak_ref_get (dd->webdav_browser_weakref);
	if (!webdav_browser)
		return;

	session = webdav_browser_ref_session (webdav_browser);
	if (!session) {
		g_object_unref (webdav_browser);
		return;
	}

	if (e_webdav_session_delete_sync (session, dd->href, E_WEBDAV_DEPTH_THIS_AND_CHILDREN, NULL, cancellable, error)) {
		dd->success = TRUE;

		webdav_browser_schedule_ui_update (webdav_browser,
			webdav_browser_delete_done, g_strdup (dd->href), g_free);
	} else {
		webdav_browser_schedule_ui_update (webdav_browser, NULL, NULL, NULL);
	}

	g_object_unref (webdav_browser);
	g_object_unref (session);
}

static void
webdav_browser_delete_clicked_cb (GtkWidget *button,
				  EWebDAVBrowser *webdav_browser)
{
	GtkWidget *parent;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	gchar *href;
	gchar *display_name = NULL;
	const gchar *question_tag, *description, *error_message;
	guint editing_flags = E_EDITING_FLAG_NONE;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));

	href = webdav_browser_dup_selected_href (webdav_browser);
	g_return_if_fail (href != NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (webdav_browser->priv->tree_view));
	g_return_if_fail (gtk_tree_selection_get_selected (selection, &model, &iter));

	gtk_tree_model_get (model, &iter,
		COLUMN_STRING_DISPLAY_NAME, &display_name,
		COLUMN_UINT_EDITING_FLAGS, &editing_flags,
		-1);

	if ((editing_flags & E_EDITING_FLAG_IS_BOOK) != 0) {
		question_tag = "addressbook:ask-delete-remote-addressbook";
		description = _("Deleting book…");
		error_message = _("Failed to delete book");
	} else if ((editing_flags & E_EDITING_FLAG_IS_CALENDAR) != 0) {
		question_tag = "calendar:prompt-delete-remote-calendar";
		description = _("Deleting calendar…");
		error_message = _("Failed to delete calendar");
	} else {
		question_tag = "system:prompt-delete-remote-collection";
		description = _("Deleting collection…");
		error_message = _("Failed to delete collection");
	}

	parent = gtk_widget_get_toplevel (button);
	if (!GTK_IS_WINDOW (parent))
		parent = NULL;

	if (e_alert_run_dialog_for_args (parent ? GTK_WINDOW (parent) : NULL, question_tag, display_name, NULL) == GTK_RESPONSE_YES) {
		EActivity *activity;
		DeleteData *dd;

		dd = g_slice_new0 (DeleteData);
		dd->webdav_browser_weakref = e_weak_ref_new (webdav_browser);
		dd->href = g_strdup (href);

		e_webdav_browser_abort (webdav_browser);

		g_clear_object (&webdav_browser->priv->cancellable);

		webdav_browser_change_busy_state (webdav_browser, TRUE);

		activity = e_alert_sink_submit_thread_job (E_ALERT_SINK (webdav_browser),
			description, "system:generic-error", error_message,
			webdav_browser_delete_thread, dd,
			delete_data_free);

		if (activity) {
			webdav_browser->priv->cancellable = e_activity_get_cancellable (activity);

			if (webdav_browser->priv->cancellable)
				g_object_ref (webdav_browser->priv->cancellable);

			e_activity_bar_set_activity (webdav_browser->priv->activity_bar, activity);

			g_object_unref (activity);
		} else {
			webdav_browser_change_busy_state (webdav_browser, FALSE);
			webdav_browser_schedule_ui_update (webdav_browser, NULL, NULL, NULL);
		}
	}

	g_free (href);
	g_free (display_name);
}

static void
webdav_browser_refresh (EWebDAVBrowser *webdav_browser)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));

	gtk_widget_set_sensitive (webdav_browser->priv->refresh_button, webdav_browser->priv->session != NULL);

	gtk_tree_store_clear (GTK_TREE_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (
		gtk_tree_view_get_model (GTK_TREE_VIEW (webdav_browser->priv->tree_view))))));

	g_hash_table_remove_all (webdav_browser->priv->href_to_reference);

	g_mutex_lock (&webdav_browser->priv->property_lock);
	g_slist_free_full (webdav_browser->priv->resources, resource_data_free);
	webdav_browser->priv->resources = NULL;
	g_mutex_unlock (&webdav_browser->priv->property_lock);

	if (webdav_browser->priv->session) {
		ESource *source;
		ESourceWebdav *webdav_extension;
		GUri *guri;

		source = e_soup_session_get_source (E_SOUP_SESSION (webdav_browser->priv->session));
		g_return_if_fail (E_IS_SOURCE (source));
		g_return_if_fail (e_source_has_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND));

		webdav_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
		guri = e_source_webdav_dup_uri (webdav_extension);

		g_return_if_fail (guri != NULL);

		gtk_label_set_text (webdav_browser->priv->url_label, g_uri_get_host (guri));

		g_uri_unref (guri);

		webdav_browser_search_user_home (webdav_browser);
	} else {
		gtk_label_set_text (webdav_browser->priv->url_label, "");
	}
}

static gint
webdav_browser_compare_iters_cb (GtkTreeModel *model,
				 GtkTreeIter *aa,
				 GtkTreeIter *bb,
				 gpointer user_data)
{
	gchar *aa_display_name = NULL, *bb_display_name = NULL;
	gint res;

	if (!aa || !bb)
		return aa == bb ? 0 : bb ? -1 : 1;

	gtk_tree_model_get (model, aa, COLUMN_STRING_DISPLAY_NAME, &aa_display_name, -1);
	gtk_tree_model_get (model, bb, COLUMN_STRING_DISPLAY_NAME, &bb_display_name, -1);

	if (!aa_display_name || !bb_display_name)
		res = g_strcmp0 (aa_display_name, bb_display_name);
	else
		res = g_utf8_collate (aa_display_name, bb_display_name);

	g_free (aa_display_name);
	g_free (bb_display_name);

	return res;
}

static GtkWidget *
webdav_browser_tree_view_new (EWebDAVBrowser *webdav_browser)
{
	GtkTreeStore *tree_store;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeModel *sort_model;
	GtkCellRenderer *cell_renderer;

	g_return_val_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser), NULL);

	tree_store = gtk_tree_store_new (N_COLUMNS,
		G_TYPE_STRING,	/* COLUMN_STRING_DISPLAY_NAME */
		G_TYPE_STRING,	/* COLUMN_STRING_TYPE */
		G_TYPE_STRING,	/* COLUMN_STRING_HREF */
		G_TYPE_STRING,	/* COLUMN_STRING_DESCRIPTION */
		G_TYPE_STRING,	/* COLUMN_STRING_ICON_NAME */
		G_TYPE_BOOLEAN,	/* COLUMN_BOOL_ICON_VISIBLE */
		GDK_TYPE_RGBA,	/* COLUMN_RGBA_COLOR */
		G_TYPE_BOOLEAN,	/* COLUMN_BOOL_COLOR_VISIBLE */
		G_TYPE_BOOLEAN,	/* COLUMN_BOOL_CHILDREN_LOADED */
		G_TYPE_UINT,	/* COLUMN_UINT_EDITING_FLAGS */
		G_TYPE_UINT,	/* COLUMN_UINT_SUPPORTS */
		G_TYPE_STRING,	/* COLUMN_STRING_TOOLTIP */
		G_TYPE_INT	/* COLUMN_INT_ORDER */
	);

	sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (tree_store));
	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (sort_model),
		webdav_browser_compare_iters_cb, NULL, NULL);

	tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (sort_model));

	g_object_unref (sort_model);
	g_object_unref (tree_store);

	gtk_tree_view_set_reorderable (tree_view, FALSE);
	gtk_tree_view_set_tooltip_column (tree_view, COLUMN_STRING_TOOLTIP);

	/* Column: Name */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Name"));

	cell_renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (cell_renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, cell_renderer, "icon-name", COLUMN_STRING_ICON_NAME);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "visible", COLUMN_BOOL_ICON_VISIBLE);

	cell_renderer = e_cell_renderer_color_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, cell_renderer, "rgba", COLUMN_RGBA_COLOR);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "visible", COLUMN_BOOL_COLOR_VISIBLE);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", COLUMN_STRING_DISPLAY_NAME);

	gtk_tree_view_append_column (tree_view, column);
	gtk_tree_view_set_expander_column (tree_view, column);

	/* Column: Type */

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Type"));

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);

	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", COLUMN_STRING_TYPE);

	gtk_tree_view_append_column (tree_view, column);

	return GTK_WIDGET (tree_view);
}

static void
webdav_browser_create_popover (EWebDAVBrowser *webdav_browser)
{
	GtkWidget *widget, *label;
	GtkGrid *grid;
	GdkRGBA rgba;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (webdav_browser->priv->create_edit_popover == NULL);

	widget = gtk_grid_new ();
	grid = GTK_GRID (widget);
	gtk_grid_set_column_spacing (grid, 6);
	gtk_grid_set_row_spacing (grid, 6);

	widget = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 0, 0, 1, 1);
	label = widget;

	widget = gtk_entry_new ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_grid_attach (grid, widget, 1, 0, 1, 1);
	webdav_browser->priv->create_edit_name_entry = widget;

	widget = gtk_label_new_with_mnemonic (_("_Color:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);
	webdav_browser->priv->create_edit_color_label = widget;
	label = widget;

	rgba.red = 0.0;
	rgba.green = 0.0;
	rgba.blue = 0.0;
	rgba.alpha = 0.001;

	widget = e_color_combo_new_defaults (&rgba, C_("ECompEditor", "None"));
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);
	webdav_browser->priv->create_edit_color_combo = widget;

	/* Translators: It's 'order' as 'sorting order' */
	widget = gtk_label_new_with_mnemonic (_("_Order:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 0, 2, 1, 1);
	webdav_browser->priv->create_edit_order_label = widget;
	label = widget;

	widget = gtk_spin_button_new_with_range (-1, G_MAXINT, 1);
	g_object_set (G_OBJECT (widget),
		"numeric", TRUE,
		"digits", 0,
		"tooltip-text", _("Use -1 to not set the sort order"),
		NULL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_grid_attach (grid, widget, 1, 2, 1, 1);
	webdav_browser->priv->create_edit_order_spin = widget;

	widget = gtk_label_new (_("For Components:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_grid_attach (grid, widget, 0, 3, 1, 1);
	webdav_browser->priv->create_edit_support_label = widget;

	widget = gtk_check_button_new_with_mnemonic (_("_Events"));
	gtk_grid_attach (grid, widget, 1, 3, 1, 1);
	webdav_browser->priv->create_edit_event_check = widget;

	widget = gtk_check_button_new_with_mnemonic (_("_Memos"));
	gtk_grid_attach (grid, widget, 1, 4, 1, 1);
	webdav_browser->priv->create_edit_memo_check = widget;

	widget = gtk_check_button_new_with_mnemonic (_("_Tasks"));
	gtk_grid_attach (grid, widget, 1, 5, 1, 1);
	webdav_browser->priv->create_edit_task_check = widget;

	widget = gtk_label_new_with_mnemonic (_("_Description:"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_widget_set_valign (widget, GTK_ALIGN_START);
	gtk_grid_attach (grid, widget, 0, 6, 1, 1);
	webdav_browser->priv->create_edit_description_label = widget;
	label = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_grid_attach (grid, widget, 1, 6, 1, 1);
	webdav_browser->priv->create_edit_description_scrolled_window = widget;

	widget = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (widget), GTK_WRAP_WORD);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	e_spell_text_view_attach (GTK_TEXT_VIEW (widget));
	gtk_container_add (GTK_CONTAINER (webdav_browser->priv->create_edit_description_scrolled_window), widget);
	webdav_browser->priv->create_edit_description_textview = widget;

	widget = gtk_button_new_with_mnemonic (_("_Save"));
	gtk_widget_set_halign (widget, GTK_ALIGN_END);
	gtk_grid_attach (grid, widget, 0, 7, 2, 1);
	webdav_browser->priv->create_edit_save_button = widget;

	gtk_widget_show_all (GTK_WIDGET (grid));

	widget = gtk_popover_new (GTK_WIDGET (webdav_browser));
	gtk_popover_set_position (GTK_POPOVER (widget), GTK_POS_BOTTOM);
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (grid));
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	webdav_browser->priv->create_edit_popover = widget;

	label = gtk_label_new ("");
	gtk_widget_show (label);
	webdav_browser->priv->create_edit_hint_label = label;

	widget = gtk_popover_new (webdav_browser->priv->create_edit_popover);
	gtk_popover_set_position (GTK_POPOVER (widget), GTK_POS_BOTTOM);
	gtk_popover_set_modal (GTK_POPOVER (widget), FALSE);
	gtk_container_add (GTK_CONTAINER (widget), label);
	gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
	webdav_browser->priv->create_edit_hint_popover = widget;
}

static void
webdav_browser_submit_alert (EAlertSink *alert_sink,
			     EAlert *alert)
{
	EWebDAVBrowser *webdav_browser;

	g_return_if_fail (E_IS_WEBDAV_BROWSER (alert_sink));

	webdav_browser = E_WEBDAV_BROWSER (alert_sink);

	e_alert_bar_submit_alert (webdav_browser->priv->alert_bar, alert);
}

static void
webdav_browser_set_credentials_prompter (EWebDAVBrowser *webdav_browser,
					 ECredentialsPrompter *credentials_prompter)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	g_return_if_fail (E_IS_CREDENTIALS_PROMPTER (credentials_prompter));
	g_return_if_fail (webdav_browser->priv->credentials_prompter == NULL);

	webdav_browser->priv->credentials_prompter = g_object_ref (credentials_prompter);
}

static void
webdav_browser_set_property (GObject *object,
			     guint property_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CREDENTIALS_PROMPTER:
			webdav_browser_set_credentials_prompter (
				E_WEBDAV_BROWSER (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE:
			e_webdav_browser_set_source (
				E_WEBDAV_BROWSER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
webdav_browser_get_property (GObject *object,
			     guint property_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CREDENTIALS_PROMPTER:
			g_value_set_object (
				value,
				e_webdav_browser_get_credentials_prompter (
				E_WEBDAV_BROWSER (object)));
			return;

		case PROP_SOURCE:
			g_value_take_object (
				value,
				e_webdav_browser_ref_source (
				E_WEBDAV_BROWSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
webdav_browser_dispose (GObject *object)
{
	EWebDAVBrowser *webdav_browser = E_WEBDAV_BROWSER (object);

	g_mutex_lock (&webdav_browser->priv->property_lock);

	if (webdav_browser->priv->update_ui_id) {
		g_source_remove (webdav_browser->priv->update_ui_id);
		webdav_browser->priv->update_ui_id = 0;
	}

	if (webdav_browser->priv->cancellable) {
		g_cancellable_cancel (webdav_browser->priv->cancellable);
		g_clear_object (&webdav_browser->priv->cancellable);
	}

	if (webdav_browser->priv->refresh_collection)
		webdav_browser_refresh_collection (webdav_browser);

	g_clear_object (&webdav_browser->priv->session);
	g_clear_object (&webdav_browser->priv->credentials_prompter);

	g_mutex_unlock (&webdav_browser->priv->property_lock);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_webdav_browser_parent_class)->dispose (object);
}

static void
webdav_browser_finalize (GObject *object)
{
	EWebDAVBrowser *webdav_browser = E_WEBDAV_BROWSER (object);

	g_slist_free_full (webdav_browser->priv->resources, resource_data_free);
	g_hash_table_destroy (webdav_browser->priv->href_to_reference);

	g_mutex_clear (&webdav_browser->priv->property_lock);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_webdav_browser_parent_class)->finalize (object);
}

static void
webdav_browser_constructed (GObject *object)
{
	EWebDAVBrowser *webdav_browser = E_WEBDAV_BROWSER (object);
	GtkTreeSelection *selection;
	GtkWidget *container;
	GtkWidget *widget;
	GtkGrid *grid;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_webdav_browser_parent_class)->constructed (object);

	grid = GTK_GRID (webdav_browser);
	gtk_grid_set_column_spacing (grid, 6);
	gtk_grid_set_row_spacing (grid, 6);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_grid_attach (grid, widget, 0, 0, 2, 1);

	container = widget;

	widget = gtk_label_new (_("WebDAV server:"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	widget = gtk_label_new ("");
	webdav_browser->priv->url_label = GTK_LABEL (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	widget = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (widget), GTK_SHADOW_IN);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_grid_attach (grid, widget, 0, 1, 1, 1);

	container = widget;

	widget = webdav_browser_tree_view_new (webdav_browser);
	gtk_container_add (GTK_CONTAINER (container), widget);
	webdav_browser->priv->tree_view = widget;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	g_signal_connect (selection, "changed",
		G_CALLBACK (webdav_browser_selection_changed_cb), webdav_browser);

	g_signal_connect (widget, "row-expanded",
		G_CALLBACK (webdav_browser_row_expanded_cb), webdav_browser);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (widget), GTK_BUTTONBOX_START);
	gtk_box_set_spacing (GTK_BOX (widget), 6);
	gtk_grid_attach (grid, widget, 1, 1, 1, 1);

	container = widget;

	widget = gtk_button_new_with_mnemonic (_("Create _Book"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	webdav_browser->priv->create_book_button = widget;

	g_signal_connect (webdav_browser->priv->create_book_button, "clicked",
		G_CALLBACK (webdav_browser_create_clicked_cb), webdav_browser);

	widget = gtk_button_new_with_mnemonic (_("Create _Calendar"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	webdav_browser->priv->create_calendar_button = widget;

	g_signal_connect (webdav_browser->priv->create_calendar_button, "clicked",
		G_CALLBACK (webdav_browser_create_clicked_cb), webdav_browser);

	widget = gtk_button_new_with_mnemonic (_("Create Collectio_n"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	webdav_browser->priv->create_collection_button = widget;

	g_signal_connect (webdav_browser->priv->create_collection_button, "clicked",
		G_CALLBACK (webdav_browser_create_clicked_cb), webdav_browser);

	widget = gtk_button_new_with_mnemonic (_("_Edit"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	webdav_browser->priv->edit_button = widget;

	g_signal_connect (webdav_browser->priv->edit_button, "clicked",
		G_CALLBACK (webdav_browser_edit_clicked_cb), webdav_browser);

	widget = gtk_button_new_with_mnemonic (_("_Delete"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	webdav_browser->priv->delete_button = widget;

	g_signal_connect (webdav_browser->priv->delete_button, "clicked",
		G_CALLBACK (webdav_browser_delete_clicked_cb), webdav_browser);

	/* widget = gtk_button_new_with_mnemonic (_("_Permissions"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	webdav_browser->priv->permissions_button = widget; */

	widget = gtk_button_new_with_mnemonic (_("_Refresh"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	webdav_browser->priv->refresh_button = widget;

	g_signal_connect_swapped (webdav_browser->priv->refresh_button, "clicked",
		G_CALLBACK (webdav_browser_refresh), webdav_browser);

	gtk_widget_show_all (GTK_WIDGET (grid));

	/* Add the bars after show_all call, because they manage
	   their visibility on their own */
	widget = e_alert_bar_new ();
	gtk_widget_set_margin_bottom (widget, 6);
	gtk_grid_attach (grid, widget, 0, 2, 2, 1);
	webdav_browser->priv->alert_bar = E_ALERT_BAR (widget);

	widget = e_activity_bar_new ();
	gtk_widget_set_margin_bottom (widget, 6);
	gtk_grid_attach (grid, widget, 0, 3, 2, 1);
	webdav_browser->priv->activity_bar = E_ACTIVITY_BAR (widget);

	webdav_browser_create_popover (webdav_browser);
}

static void
e_webdav_browser_class_init (EWebDAVBrowserClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = webdav_browser_set_property;
	object_class->get_property = webdav_browser_get_property;
	object_class->dispose = webdav_browser_dispose;
	object_class->finalize = webdav_browser_finalize;
	object_class->constructed = webdav_browser_constructed;

	/**
	 * EWebDAVBrowser:credentials-prompter:
	 *
	 * The #ECredentialsPrompter used to ask for credentials when needed.
	 *
	 * Since: 3.26
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CREDENTIALS_PROMPTER,
		g_param_spec_object (
			"credentials-prompter",
			"Credentials Prompter",
			"an ECredentialsPrompter",
			E_TYPE_CREDENTIALS_PROMPTER,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EWebDAVBrowser:source:
	 *
	 * The #ESource currently used for the GUI. It can be %NULL.
	 *
	 * Since: 3.26
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			"Source",
			"an ESource",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
webdav_browser_alert_sink_init (EAlertSinkInterface *iface)
{
	iface->submit_alert = webdav_browser_submit_alert;
}

static void
e_webdav_browser_init (EWebDAVBrowser *webdav_browser)
{
	webdav_browser->priv = e_webdav_browser_get_instance_private (webdav_browser);

	g_mutex_init (&webdav_browser->priv->property_lock);

	webdav_browser->priv->href_to_reference = g_hash_table_new_full (camel_strcase_hash, camel_strcase_equal,
		g_free, (GDestroyNotify) gtk_tree_row_reference_free);
}

/**
 * e_webdav_browser_new:
 * @credentials_prompter: an #ECredentialsPrompter
 *
 * Creates a new #EWebDAVBrowser instance.
 *
 * Returns: (transfer full): an #EWebDAVBrowser as a #GtkWidget
 *
 * Since: 3.26
 **/
GtkWidget *
e_webdav_browser_new (ECredentialsPrompter *credentials_prompter)
{
	return g_object_new (E_TYPE_WEBDAV_BROWSER,
		"credentials-prompter", credentials_prompter,
		NULL);
}

/**
 * e_webdav_browser_get_credentials_prompter:
 * @webdav_browser: an #EWebDAVBrowser
 *
 * Returns: (transfer none): an #ECredentialsPrompter used to call
 *    of e_webdav_browser_new()
 *
 * Since: 3.26
 **/
ECredentialsPrompter *
e_webdav_browser_get_credentials_prompter (EWebDAVBrowser *webdav_browser)
{
	g_return_val_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser), NULL);

	return webdav_browser->priv->credentials_prompter;
}

/**
 * e_webdav_browser_set_source:
 * @webdav_browser: an #EWebDAVBrowser
 * @source: (nullable): an #ESource
 *
 * Sets the @source to be the one used for the @webdav_browser.
 * It can be %NULL, to have none set.
 *
 * Since: 3.26
 **/
void
e_webdav_browser_set_source (EWebDAVBrowser *webdav_browser,
			     ESource *source)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));
	if (source)
		g_return_if_fail (E_IS_SOURCE (source));

	e_alert_bar_clear (webdav_browser->priv->alert_bar);

	g_mutex_lock (&webdav_browser->priv->property_lock);

	if (!source && !webdav_browser->priv->session) {
		g_mutex_unlock (&webdav_browser->priv->property_lock);
		return;
	}

	if (webdav_browser->priv->refresh_collection)
		webdav_browser_refresh_collection (webdav_browser);

	g_clear_object (&webdav_browser->priv->session);

	if (source) {
		webdav_browser->priv->session = e_webdav_session_new (source);

		if (webdav_browser->priv->session)
			e_soup_session_setup_logging (E_SOUP_SESSION (webdav_browser->priv->session), g_getenv ("WEBDAV_DEBUG"));
	}

	g_mutex_unlock (&webdav_browser->priv->property_lock);

	webdav_browser_refresh (webdav_browser);

	g_object_notify (G_OBJECT (webdav_browser), "source");
}

/**
 * e_webdav_browser_ref_source:
 * @webdav_browser: an #EWebDAVBrowser
 *
 * Returns: (transfer full) (nullable): an #ESource, currently used by @webdav_browser;
 *    if not %NULL, then free with g_object_unref(), when no longer needed.
 *
 * Since: 3.26
 **/
ESource *
e_webdav_browser_ref_source (EWebDAVBrowser *webdav_browser)
{
	ESource *source = NULL;

	g_return_val_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser), NULL);

	g_mutex_lock (&webdav_browser->priv->property_lock);

	if (webdav_browser->priv->session) {
		source = e_soup_session_get_source (E_SOUP_SESSION (webdav_browser->priv->session));
		if (source)
			g_object_ref (source);
	}

	g_mutex_unlock (&webdav_browser->priv->property_lock);

	return source;
}

/**
 * e_webdav_browser_abort:
 * @webdav_browser: an #EWebDAVBrowser
 *
 * Aborts any ongoing operation. It does nothing, if no
 * operation is running.
 *
 * Since: 3.26
 **/
void
e_webdav_browser_abort (EWebDAVBrowser *webdav_browser)
{
	g_return_if_fail (E_IS_WEBDAV_BROWSER (webdav_browser));

	if (webdav_browser->priv->cancellable)
		g_cancellable_cancel (webdav_browser->priv->cancellable);
}
