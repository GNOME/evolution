/*
 * evolution-book-config-webdav.c
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

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

typedef ESourceConfigBackend EBookConfigWebdav;
typedef ESourceConfigBackendClass EBookConfigWebdavClass;

typedef struct _Context Context;

struct _Context {
	GtkWidget *url_entry;
	GtkWidget *avoid_ifmatch;
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_book_config_webdav_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EBookConfigWebdav,
	e_book_config_webdav,
	E_TYPE_SOURCE_CONFIG_BACKEND)

static void
book_config_webdav_context_free (Context *context)
{
	g_object_unref (context->url_entry);
	g_object_unref (context->avoid_ifmatch);

	g_slice_free (Context, context);
}

static gboolean
book_config_webdav_uri_to_text (GBinding *binding,
                                const GValue *source_value,
                                GValue *target_value,
                                gpointer user_data)
{
	SoupURI *soup_uri;
	gchar *text;

	soup_uri = g_value_get_boxed (source_value);
	soup_uri_set_user (soup_uri, NULL);

	text = soup_uri_to_string (soup_uri, FALSE);
	g_value_take_string (target_value, text);

	return TRUE;
}

static gboolean
book_config_webdav_text_to_uri (GBinding *binding,
                                const GValue *source_value,
                                GValue *target_value,
                                gpointer user_data)
{
	ESource *source;
	SoupURI *soup_uri;
	GObject *source_binding;
	ESourceAuthentication *extension;
	const gchar *extension_name;
	const gchar *text;
	const gchar *user;

	text = g_value_get_string (source_value);
	soup_uri = soup_uri_new (text);

	if (soup_uri == NULL)
		return FALSE;

	source_binding = g_binding_get_source (binding);
	source = e_source_extension_ref_source (
		E_SOURCE_EXTENSION (source_binding));

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (source, extension_name);
	user = e_source_authentication_get_user (extension);

	soup_uri_set_user (soup_uri, user);

	g_value_take_boxed (target_value, soup_uri);

	g_object_unref (source);

	return TRUE;
}

static void
book_config_webdav_insert_widgets (ESourceConfigBackend *backend,
                                   ESource *scratch_source)
{
	ESourceConfig *config;
	ESourceExtension *extension;
	GtkWidget *widget;
	Context *context;
	const gchar *extension_name;
	const gchar *uid;

	context = g_slice_new (Context);
	uid = e_source_get_uid (scratch_source);
	config = e_source_config_backend_get_config (backend);

	g_object_set_data_full (
		G_OBJECT (backend), uid, context,
		(GDestroyNotify) book_config_webdav_context_free);

	e_book_source_config_add_offline_toggle (
		E_BOOK_SOURCE_CONFIG (config), scratch_source);

	widget = gtk_entry_new ();
	e_source_config_insert_widget (
		config, scratch_source, _("URL:"), widget);
	context->url_entry = g_object_ref (widget);
	gtk_widget_show (widget);

	e_source_config_add_secure_connection_for_webdav (
		config, scratch_source);

	e_source_config_add_user_entry (config, scratch_source);

	widget = gtk_check_button_new_with_label (
		_("Avoid IfMatch (needed on Apache < 2.2.8)"));
	e_source_config_insert_widget (
		config, scratch_source, NULL, widget);
	context->avoid_ifmatch = g_object_ref (widget);
	gtk_widget_show (widget);

	extension_name = E_SOURCE_EXTENSION_WEBDAV_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	e_binding_bind_property (
		extension, "avoid-ifmatch",
		context->avoid_ifmatch, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property_full (
		extension, "soup-uri",
		context->url_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		book_config_webdav_uri_to_text,
		book_config_webdav_text_to_uri,
		NULL, (GDestroyNotify) NULL);
}

static gboolean
book_config_webdav_check_complete (ESourceConfigBackend *backend,
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

	entry = GTK_ENTRY (context->url_entry);
	uri_string = gtk_entry_get_text (entry);

	soup_uri = soup_uri_new (uri_string);
	complete = SOUP_URI_VALID_FOR_HTTP (soup_uri);

	if (soup_uri != NULL)
		soup_uri_free (soup_uri);

	return complete;
}

static void
e_book_config_webdav_class_init (ESourceConfigBackendClass *class)
{
	EExtensionClass *extension_class;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_BOOK_SOURCE_CONFIG;

	class->parent_uid = "webdav-stub";
	class->backend_name = "webdav";
	class->insert_widgets = book_config_webdav_insert_widgets;
	class->check_complete = book_config_webdav_check_complete;
}

static void
e_book_config_webdav_class_finalize (ESourceConfigBackendClass *class)
{
}

static void
e_book_config_webdav_init (ESourceConfigBackend *backend)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_book_config_webdav_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
