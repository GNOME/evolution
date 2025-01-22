/*
 * Copyright (C) 2020 Red Hat, Inc. (www.redhat.com)
 *
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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>
#include <libedataserverui/libedataserverui.h>

#include <e-util/e-util.h>

typedef ESourceConfigBackend ECalConfigWebDAVNotes;
typedef ESourceConfigBackendClass ECalConfigWebDAVNotesClass;

typedef struct _Context Context;

struct _Context {
	ESourceConfigBackend *backend;		/* not referenced */
	ESource *scratch_source;		/* not referenced */

	GtkWidget *url_entry;
	GtkWidget *find_button;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_cal_config_webdav_notes_get_type (void);

G_DEFINE_DYNAMIC_TYPE (ECalConfigWebDAVNotes, e_cal_config_webdav_notes, E_TYPE_SOURCE_CONFIG_BACKEND)

static Context *
cal_config_webdav_notes_context_new (ESourceConfigBackend *backend,
				     ESource *scratch_source)
{
	Context *context;

	context = g_slice_new0 (Context);
	context->backend = backend;
	context->scratch_source = scratch_source;

	return context;
}

static void
cal_config_webdav_notes_context_free (Context *context)
{
	g_clear_object (&context->url_entry);
	g_clear_object (&context->find_button);

	g_slice_free (Context, context);
}

static GtkWindow *
caldav_config_get_dialog_parent_cb (ECredentialsPrompter *prompter,
				    GtkWindow *dialog)
{
	return dialog;
}

static void
cal_config_webdav_notes_run_dialog (GtkButton *button,
				    Context *context)
{
	ESourceConfig *config;
	ESourceRegistry *registry;
	ESourceWebdav *webdav_extension;
	ECalClientSourceType source_type;
	ECredentialsPrompter *prompter;
	GUri *guri;
	gchar *base_url;
	GtkDialog *dialog;
	gpointer parent;
	gulong handler_id;
	guint supports_filter = 0;
	const gchar *title = NULL;

	config = e_source_config_backend_get_config (context->backend);
	registry = e_source_config_get_registry (config);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (config));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;
	source_type = e_cal_source_config_get_source_type (E_CAL_SOURCE_CONFIG (config));

	switch (source_type) {
	case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
		supports_filter = E_WEBDAV_RESOURCE_SUPPORTS_WEBDAV_NOTES;
		title = _("Choose Notes");
		break;
	default:
		g_return_if_reached ();
	}

	webdav_extension = e_source_get_extension (context->scratch_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	guri = e_source_webdav_dup_uri (webdav_extension);

	prompter = e_credentials_prompter_new (registry);
	e_credentials_prompter_set_auto_prompt (prompter, FALSE);
	base_url = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);

	dialog = e_webdav_discover_dialog_new (parent, title, prompter, context->scratch_source, base_url, supports_filter);

	if (parent != NULL)
		e_binding_bind_property (
			parent, "icon-name",
			dialog, "icon-name",
			G_BINDING_SYNC_CREATE);

	handler_id = g_signal_connect (prompter, "get-dialog-parent",
		G_CALLBACK (caldav_config_get_dialog_parent_cb), dialog);

	e_webdav_discover_dialog_refresh (dialog);

	if (gtk_dialog_run (dialog) == GTK_RESPONSE_ACCEPT) {
		gchar *href = NULL, *display_name = NULL, *color = NULL, *email;
		guint supports = 0, order = 0;
		GtkWidget *content;

		content = e_webdav_discover_dialog_get_content (dialog);

		if (e_webdav_discover_content_get_selected (content, 0, &href, &supports, &display_name, &color, &order)) {
			g_uri_unref (guri);
			guri = g_uri_parse (href, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

			if (guri) {
				ESourceSelectable *selectable_extension;
				const gchar *extension_name;

				switch (source_type) {
					case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
						extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
						break;
					default:
						g_return_if_reached ();
				}

				selectable_extension = e_source_get_extension (context->scratch_source, extension_name);

				e_source_set_display_name (context->scratch_source, display_name);

				e_source_webdav_set_display_name (webdav_extension, display_name);
				e_source_webdav_set_uri (webdav_extension, guri);
				e_source_webdav_set_order (webdav_extension, order);

				if (color && *color)
					e_source_selectable_set_color (selectable_extension, color);

				e_source_selectable_set_order (selectable_extension, order);
			}

			g_free (href);
			g_free (display_name);
			g_free (color);

			href = NULL;
			display_name = NULL;
			color = NULL;
		}

		email = e_webdav_discover_content_get_user_address (content);
		if (email && *email)
			e_source_webdav_set_email_address (webdav_extension, email);
		g_free (email);
	}

	g_signal_handler_disconnect (prompter, handler_id);

	gtk_widget_destroy (GTK_WIDGET (dialog));

	g_object_unref (prompter);
	if (guri)
		g_uri_unref (guri);
	g_free (base_url);
}

static gboolean
cal_config_webdav_notes_allow_creation (ESourceConfigBackend *backend)
{
	ESourceConfig *config;
	ECalClientSourceType source_type;

	config = e_source_config_backend_get_config (backend);
	source_type = e_cal_source_config_get_source_type (E_CAL_SOURCE_CONFIG (config));

	return source_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS;
}

static void
cal_config_webdav_notes_insert_widgets (ESourceConfigBackend *backend,
					ESource *scratch_source)
{
	ESourceConfig *config;
	ESource *collection_source;
	ESourceExtension *extension;
	ESourceWebDAVNotes *notes_extension;
	ECalClientSourceType source_type;
	GtkWidget *widget, *container;
	Context *context;
	const gchar *extension_name;
	const gchar *label;
	const gchar *uid;

	config = e_source_config_backend_get_config (backend);
	collection_source = e_source_config_get_collection_source (config);

	uid = e_source_get_uid (scratch_source);
	context = cal_config_webdav_notes_context_new (backend, scratch_source);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) cal_config_webdav_notes_context_free);

	if (collection_source) {
		widget = gtk_label_new ("");
		g_object_set (G_OBJECT (widget),
			"ellipsize", PANGO_ELLIPSIZE_MIDDLE,
			"selectable", TRUE,
			"xalign", 0.0f,
			NULL);
		e_source_config_insert_widget (config, scratch_source, _("URL:"), widget);
		gtk_widget_show (widget);

		extension = e_source_get_extension (scratch_source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

		e_binding_bind_property_full (
			extension, "uri",
			widget, "label",
			G_BINDING_SYNC_CREATE,
			e_binding_transform_uri_to_text,
			NULL,
			g_object_ref (scratch_source),
			(GDestroyNotify) g_object_unref);

		e_binding_bind_property (
			widget, "label",
			widget, "tooltip-text",
			G_BINDING_SYNC_CREATE);
	} else {
		widget = gtk_entry_new ();
		e_source_config_insert_widget (
			config, scratch_source, _("URL:"), widget);
		context->url_entry = g_object_ref (widget);
		gtk_widget_show (widget);
	}

	e_source_config_add_secure_connection_for_webdav (config, scratch_source);

	source_type = e_cal_source_config_get_source_type (E_CAL_SOURCE_CONFIG (config));

	if (!collection_source) {
		e_source_config_add_user_entry (config, scratch_source);

		g_warn_if_fail (source_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS);

		label = _("Find Notes");

		widget = gtk_button_new_with_label (label);
		e_source_config_insert_widget (config, scratch_source, NULL, widget);
		context->find_button = g_object_ref (widget);
		gtk_widget_show (widget);

		g_signal_connect (
			widget, "clicked",
			G_CALLBACK (cal_config_webdav_notes_run_dialog), context);
	}

	e_source_config_add_refresh_interval (config, scratch_source);
	e_source_config_add_refresh_on_metered_network (config, scratch_source);
	e_source_config_add_timeout_interval_for_webdav (config, scratch_source);

	notes_extension = e_source_get_extension (scratch_source, E_SOURCE_EXTENSION_WEBDAV_NOTES);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
	e_source_config_insert_widget (config, scratch_source, NULL, widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_label_new (_("File extension for new notes:"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = gtk_combo_box_text_new ();
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), ".md", ".md");
	gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (widget), ".txt", ".txt");
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		notes_extension, "default-ext",
		widget, "active-id",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	if (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)) == -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	extension_name = E_SOURCE_EXTENSION_WEBDAV_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	if (context->url_entry) {
		e_binding_bind_property_full (
			extension, "uri",
			context->url_entry, "text",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE,
			e_binding_transform_uri_to_text,
			e_binding_transform_text_to_uri,
			g_object_ref (scratch_source),
			(GDestroyNotify) g_object_unref);
	}
}

static gboolean
cal_config_webdav_notes_check_complete (ESourceConfigBackend *backend,
					ESource *scratch_source)
{
	Context *context;
	const gchar *uid;
	const gchar *uri_string;
	GUri *guri;
	gboolean complete;

	uid = e_source_get_uid (scratch_source);
	context = g_object_get_data (G_OBJECT (backend), uid);
	g_return_val_if_fail (context != NULL, FALSE);

	if (!context->url_entry)
		return TRUE;

	uri_string = gtk_entry_get_text (GTK_ENTRY (context->url_entry));
	guri = g_uri_parse (uri_string, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

	if (guri) {
		if (g_strcmp0 (g_uri_get_scheme (guri), "caldav") == 0)
			e_util_change_uri_component (&guri, SOUP_URI_SCHEME, "https");

		complete = (g_strcmp0 (g_uri_get_scheme (guri), "http") == 0 ||
			    g_strcmp0 (g_uri_get_scheme (guri), "https") == 0) &&
			    g_uri_get_host (guri) && g_uri_get_path (guri);
	} else {
		complete = FALSE;
	}

	if (guri)
		g_uri_unref (guri);

	gtk_widget_set_sensitive (context->find_button, complete);

	e_util_set_entry_issue_hint (context->url_entry, complete ? NULL : _("URL is not a valid http:// nor https:// URL"));

	return complete;
}

static void
e_cal_config_webdav_notes_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_SOURCE_CONFIG;

	class->parent_uid = "webdav-notes-stub";
	class->backend_name = "webdav-notes";
	class->allow_creation = cal_config_webdav_notes_allow_creation;
	class->insert_widgets = cal_config_webdav_notes_insert_widgets;
	class->check_complete = cal_config_webdav_notes_check_complete;
}

static void
e_cal_config_webdav_notes_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_cal_config_webdav_notes_init (ESourceConfigBackend *backend)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_cal_config_webdav_notes_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
