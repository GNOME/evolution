/*
 * evolution-cal-config-webcal.c
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

#include <e-util/e-util.h>

typedef ESourceConfigBackend ECalConfigWebcal;
typedef ESourceConfigBackendClass ECalConfigWebcalClass;

typedef struct _Context Context;

struct _Context {
	GtkWidget *url_entry;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_cal_config_webcal_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	ECalConfigWebcal,
	e_cal_config_webcal,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static void
cal_config_webcal_context_free (Context *context)
{
	g_clear_object (&context->url_entry);

	g_slice_free (Context, context);
}

static gboolean
cal_config_webcal_uri_to_text (GBinding *binding,
                               const GValue *source_value,
                               GValue *target_value,
                               gpointer user_data)
{
	SoupURI *soup_uri;
	gchar *text;

	soup_uri = g_value_get_boxed (source_value);
	soup_uri_set_user (soup_uri, NULL);

	if (soup_uri_get_host (soup_uri)) {
		text = soup_uri_to_string (soup_uri, FALSE);
	} else {
		GObject *target;

		text = NULL;
		target = g_binding_get_target (binding);
		g_object_get (target, g_binding_get_target_property (binding), &text, NULL);

		if (!text || !*text) {
			g_free (text);
			text = soup_uri_to_string (soup_uri, FALSE);
		}
	}

	g_value_take_string (target_value, text);

	return TRUE;
}

static gboolean
cal_config_webcal_text_to_uri (GBinding *binding,
                               const GValue *source_value,
                               GValue *target_value,
                               gpointer user_data)
{
	ESource *source;
	SoupURI *soup_uri;
	ESourceAuthentication *extension;
	const gchar *extension_name;
	const gchar *text;
	const gchar *user;

	text = g_value_get_string (source_value);
	soup_uri = soup_uri_new (text);

	if (!soup_uri)
		soup_uri = soup_uri_new ("http://");

	source = E_SOURCE (user_data);
	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (source, extension_name);
	user = e_source_authentication_get_user (extension);

	soup_uri_set_user (soup_uri, user);

	g_value_take_boxed (target_value, soup_uri);

	return TRUE;
}

static void
cal_config_webcal_insert_widgets (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	ESourceConfig *config;
	ESourceExtension *extension;
	GtkWidget *widget;
	Context *context;
	const gchar *extension_name;
	const gchar *uid;

	context = g_slice_new0 (Context);
	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) cal_config_webcal_context_free);

	if (e_source_config_get_collection_source (config)) {
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
			extension, "soup-uri",
			widget, "label",
			G_BINDING_SYNC_CREATE,
			cal_config_webcal_uri_to_text,
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

	e_source_config_add_secure_connection_for_webdav (
		config, scratch_source);

	e_source_config_add_user_entry (config, scratch_source);

	e_source_config_add_refresh_interval (config, scratch_source);

	extension_name = E_SOURCE_EXTENSION_WEBDAV_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	if (context->url_entry) {
		e_binding_bind_property_full (
			extension, "soup-uri",
			context->url_entry, "text",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE,
			cal_config_webcal_uri_to_text,
			cal_config_webcal_text_to_uri,
			g_object_ref (scratch_source),
			(GDestroyNotify) g_object_unref);
	}
}

static gboolean
cal_config_webcal_check_complete (ESourceConfigBackend *backend,
                                  ESource *scratch_source)
{
	SoupURI *soup_uri;
	GtkEntry *entry;
	Context *context;
	const gchar *uri_string;
	const gchar *uid;
	gboolean complete;

	uid = e_source_get_uid (scratch_source);
	context = g_object_get_data (G_OBJECT (backend), uid);
	g_return_val_if_fail (context != NULL, FALSE);

	if (!context->url_entry)
		return TRUE;

	entry = GTK_ENTRY (context->url_entry);
	uri_string = gtk_entry_get_text (entry);

	soup_uri = soup_uri_new (uri_string);

	if (soup_uri) {
		/* XXX webcal:// is a non-standard scheme, but we accept it.
		 *     Just convert it to http:// for the URI validity test. */
		if (g_strcmp0 (soup_uri_get_scheme (soup_uri), "webcal") == 0)
			soup_uri_set_scheme (soup_uri, SOUP_URI_SCHEME_HTTP);

		complete = soup_uri_get_host (soup_uri) && SOUP_URI_VALID_FOR_HTTP (soup_uri);
	} else {
		complete = FALSE;
	}

	if (soup_uri != NULL)
		soup_uri_free (soup_uri);

	e_util_set_entry_issue_hint (context->url_entry, complete ? NULL : _("URL is not a valid http:// nor https:// URL"));

	return complete;
}

static void
e_cal_config_webcal_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_SOURCE_CONFIG;

	class->parent_uid = "webcal-stub";
	class->backend_name = "webcal";
	class->insert_widgets = cal_config_webcal_insert_widgets;
	class->check_complete = cal_config_webcal_check_complete;
}

static void
e_cal_config_webcal_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_cal_config_webcal_init (ESourceConfigBackend *backend)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_cal_config_webcal_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
