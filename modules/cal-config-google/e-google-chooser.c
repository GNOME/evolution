/*
 * e-google-chooser.c
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

#include "e-google-chooser.h"

#include <config.h>
#include <string.h>
#include <gdata/gdata.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#define E_GOOGLE_CHOOSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_GOOGLE_CHOOSER, EGoogleChooserPrivate))

#define CALDAV_EVENTS_PATH_FORMAT "/calendar/dav/%s/events"

typedef struct _Context Context;

struct _EGoogleChooserPrivate {
	ESource *source;
};

struct _Context {
	GCancellable *cancellable;
	GDataCalendarService *service;
	GDataClientLoginAuthorizer *authorizer;
	ESource *source;
};

enum {
	PROP_0,
	PROP_SOURCE
};

enum {
	COLUMN_COLOR,
	COLUMN_PATH,
	COLUMN_TITLE,
	COLUMN_WRITABLE,
	NUM_COLUMNS
};

G_DEFINE_DYNAMIC_TYPE (
	EGoogleChooser,
	e_google_chooser,
	GTK_TYPE_TREE_VIEW)

static void
context_free (Context *context)
{
	if (context->cancellable != NULL)
		g_object_unref (context->cancellable);

	if (context->service != NULL)
		g_object_unref (context->service);

	if (context->authorizer != NULL)
		g_object_unref (context->authorizer);

	if (context->source != NULL)
		g_object_unref (context->source);

	g_slice_free (Context, context);
}

static gchar *
google_chooser_extract_caldav_events_path (const gchar *uri)
{
	SoupURI *soup_uri;
	gchar *resource_name;
	gchar *path;
	gchar *cp;

	soup_uri = soup_uri_new (uri);
	g_return_val_if_fail (soup_uri != NULL, NULL);

	/* Isolate the resource name in the "feeds" URI. */

	cp = strstr (soup_uri->path, "/feeds/");
	g_return_val_if_fail (cp != NULL, NULL);

	/* strlen("/feeds/) == 7 */
	resource_name = g_strdup (cp + 7);
	cp = strchr (resource_name, '/');
	if (cp != NULL)
		*cp = '\0';

	/* Decode any encoded 'at' symbols ('%40' -> '@'). */
	if (strstr (resource_name, "%40") != NULL) {
		gchar **segments;

		segments = g_strsplit (resource_name, "%40", 0);
		g_free (resource_name);
		resource_name = g_strjoinv ("@", segments);
		g_strfreev (segments);
	}

	/* Use the decoded resource name in the CalDAV events path. */
	path = g_strdup_printf (CALDAV_EVENTS_PATH_FORMAT, resource_name);

	g_free (resource_name);

	soup_uri_free (soup_uri);

	return path;
}

static gchar *
google_chooser_decode_user (const gchar *user)
{
	gchar *decoded_user;

	if (user == NULL || *user == '\0')
		return NULL;

	/* Decode any encoded 'at' symbols ('%40' -> '@'). */
	if (strstr (user, "%40") != NULL) {
		gchar **segments;

		segments = g_strsplit (user, "%40", 0);
		decoded_user = g_strjoinv ("@", segments);
		g_strfreev (segments);

	/* If no domain is given, append "@gmail.com". */
	} else if (strstr (user, "@") == NULL) {
		decoded_user = g_strconcat (user, "@gmail.com", NULL);

	/* Otherwise the user name should be fine as is. */
	} else {
		decoded_user = g_strdup (user);
	}

	return decoded_user;
}

static void
google_chooser_set_source (EGoogleChooser *chooser,
                           ESource *source)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (chooser->priv->source == NULL);

	chooser->priv->source = g_object_ref (source);
}

static void
google_chooser_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE:
			google_chooser_set_source (
				E_GOOGLE_CHOOSER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
google_chooser_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE:
			g_value_set_object (
				value, e_google_chooser_get_source (
				E_GOOGLE_CHOOSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
google_chooser_dispose (GObject *object)
{
	EGoogleChooserPrivate *priv;

	priv = E_GOOGLE_CHOOSER_GET_PRIVATE (object);

	if (priv->source != NULL) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}

	/* Chain up parent's dispose() method. */
	G_OBJECT_CLASS (e_google_chooser_parent_class)->dispose (object);
}

static void
google_chooser_constructed (GObject *object)
{
	GtkTreeView *tree_view;
	GtkListStore *list_store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	tree_view = GTK_TREE_VIEW (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_google_chooser_parent_class)->constructed (object);

	list_store = gtk_list_store_new (
		NUM_COLUMNS,
		GDK_TYPE_COLOR,		/* COLUMN_COLOR */
		G_TYPE_STRING,		/* COLUMN_PATH */
		G_TYPE_STRING,		/* COLUMN_TITLE */
		G_TYPE_BOOLEAN);	/* COLUMN_WRITABLE */

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Name"));
	gtk_tree_view_insert_column (tree_view, column, -1);

	renderer = e_cell_renderer_color_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "color", COLUMN_COLOR);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "text", COLUMN_TITLE);
}

static void
e_google_chooser_class_init (EGoogleChooserClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EGoogleChooserPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = google_chooser_set_property;
	object_class->get_property = google_chooser_get_property;
	object_class->dispose = google_chooser_dispose;
	object_class->constructed = google_chooser_constructed;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			"Source",
			"Google data source",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_google_chooser_class_finalize (EGoogleChooserClass *class)
{
}

static void
e_google_chooser_init (EGoogleChooser *chooser)
{
	chooser->priv = E_GOOGLE_CHOOSER_GET_PRIVATE (chooser);
}

void
e_google_chooser_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_google_chooser_register_type (type_module);
}

GtkWidget *
e_google_chooser_new (ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return g_object_new (E_TYPE_GOOGLE_CHOOSER, "source", source, NULL);
}

ESource *
e_google_chooser_get_source (EGoogleChooser *chooser)
{
	g_return_val_if_fail (E_IS_GOOGLE_CHOOSER (chooser), NULL);

	return chooser->priv->source;
}

gchar *
e_google_chooser_get_decoded_user (EGoogleChooser *chooser)
{
	ESource *source;
	ESourceAuthentication *authentication_extension;
	const gchar *user;

	g_return_val_if_fail (E_IS_GOOGLE_CHOOSER (chooser), NULL);

	source = e_google_chooser_get_source (chooser);

	authentication_extension = e_source_get_extension (
		source, E_SOURCE_EXTENSION_AUTHENTICATION);

	user = e_source_authentication_get_user (authentication_extension);
	return google_chooser_decode_user (user);
}

static void
google_chooser_query_cb (GDataService *service,
                         GAsyncResult *result,
                         GSimpleAsyncResult *simple)
{
	GObject *object;
	GDataFeed *feed;
	GList *list, *link;
	GtkTreeView *tree_view;
	GtkListStore *list_store;
	GtkTreeModel *tree_model;
	GError *error = NULL;

	feed = gdata_service_query_finish (service, result, &error);

	if (error != NULL) {
		g_warn_if_fail (feed == NULL);
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
		return;
	}

	g_return_if_fail (GDATA_IS_FEED (feed));

	list = gdata_feed_get_entries (feed);

	/* This returns a new reference, for reasons passing understanding. */
	object = g_async_result_get_source_object (G_ASYNC_RESULT (simple));

	tree_view = GTK_TREE_VIEW (object);
	tree_model = gtk_tree_view_get_model (tree_view);
	list_store = GTK_LIST_STORE (tree_model);

	gtk_list_store_clear (list_store);

	for (link = list; link != NULL; link = g_list_next (link)) {
		GDataCalendarCalendar *calendar;
		GDataEntry *entry;
		GDataLink *alt;
		GDataColor color;
		GdkColor gdkcolor;
		GtkTreeIter iter;
		const gchar *uri;
		const gchar *title;
		const gchar *access;
		gboolean writable;
		gchar *path;

		entry = GDATA_ENTRY (link->data);
		calendar = GDATA_CALENDAR_CALENDAR (entry);

		/* Skip hidden entries. */
		if (gdata_calendar_calendar_is_hidden (calendar))
			continue;

		/* Look up the alternate link, skip if there is none. */
		alt = gdata_entry_look_up_link (entry, GDATA_LINK_ALTERNATE);
		if (alt == NULL)
			continue;

		uri = gdata_link_get_uri (alt);
		title = gdata_entry_get_title (entry);
		gdata_calendar_calendar_get_color (calendar, &color);
		access = gdata_calendar_calendar_get_access_level (calendar);

		if (uri == NULL || *uri == '\0')
			continue;

		if (title == NULL || *title == '\0')
			continue;

		path = google_chooser_extract_caldav_events_path (uri);

		gdkcolor.pixel = 0;
		gdkcolor.red = color.red * 256;
		gdkcolor.green = color.green * 256;
		gdkcolor.blue = color.blue * 256;

		if (access == NULL)
			writable = TRUE;
		else if (g_ascii_strcasecmp (access, "owner") == 0)
			writable = TRUE;
		else if (g_ascii_strcasecmp (access, "contributor") == 0)
			writable = TRUE;
		else
			writable = FALSE;

		gtk_list_store_append (list_store, &iter);

		gtk_list_store_set (
			list_store, &iter,
			COLUMN_COLOR, &gdkcolor,
			COLUMN_PATH, path,
			COLUMN_TITLE, title,
			COLUMN_WRITABLE, writable,
			-1);

		g_free (path);
	}

	g_object_unref (object);
	g_object_unref (feed);

	g_simple_async_result_complete (simple);
	g_object_unref (simple);
}

static void
google_chooser_authenticate_cb (GDataClientLoginAuthorizer *authorizer,
                                GAsyncResult *result,
                                GSimpleAsyncResult *simple)
{
	Context *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	gdata_client_login_authorizer_authenticate_finish (
		authorizer, result, &error);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
		return;
	}

	/* We're authenticated, now query for all calendars. */

	gdata_calendar_service_query_all_calendars_async (
		context->service, NULL, context->cancellable,
		NULL, NULL, NULL, (GAsyncReadyCallback)
		google_chooser_query_cb, simple);
}

void
e_google_chooser_populate (EGoogleChooser *chooser,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	GDataClientLoginAuthorizer *authorizer;
	GDataCalendarService *service;
	GSimpleAsyncResult *simple;
	Context *context;
	ESource *source;
	gpointer parent;
	gchar *password;
	gchar *prompt;
	gchar *user;

	g_return_if_fail (E_IS_GOOGLE_CHOOSER (chooser));

	source = e_google_chooser_get_source (chooser);

	authorizer = gdata_client_login_authorizer_new (
		PACKAGE_NAME, GDATA_TYPE_CALENDAR_SERVICE);

	service = gdata_calendar_service_new (GDATA_AUTHORIZER (authorizer));

	context = g_slice_new0 (Context);
	context->service = service;  /* takes ownership */
	context->source = g_object_ref (source);

	if (G_IS_CANCELLABLE (cancellable))
		context->cancellable = g_object_ref (cancellable);
	else
		context->cancellable = g_cancellable_new ();

	simple = g_simple_async_result_new (
		G_OBJECT (chooser), callback,
		user_data, e_google_chooser_populate);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) context_free);

	/* Prompt for a password. */

	user = e_google_chooser_get_decoded_user (chooser);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (chooser));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	prompt = g_strdup_printf (
		_("Enter Google password for user '%s'."), user);

	/* XXX The 'key' (2nd) argument doesn't matter since we're
	 *     passing E_PASSWORDS_REMEMBER_NEVER, it just needs to
	 *     be non-NULL.  This API is degenerating rapidly. */
	password = e_passwords_ask_password (
		"", "bogus key", prompt,
		E_PASSWORDS_REMEMBER_NEVER |
		E_PASSWORDS_DISABLE_REMEMBER |
		E_PASSWORDS_SECRET, NULL, parent);

	g_free (prompt);

	if (password == NULL) {
		g_cancellable_cancel (context->cancellable);
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_CANCELLED,
			"%s", _("User declined to provide a password"));
		g_simple_async_result_complete (simple);
		g_object_unref (authorizer);
		g_object_unref (simple);
		g_free (user);
		return;
	}

	/* Try authenticating. */

	gdata_client_login_authorizer_authenticate_async (
		authorizer, user, password,
		context->cancellable, (GAsyncReadyCallback)
		google_chooser_authenticate_cb, simple);

	g_free (password);
	g_free (user);

	g_object_unref (authorizer);
}

gboolean
e_google_chooser_populate_finish (EGoogleChooser *chooser,
                                  GAsyncResult *result,
                                  GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (chooser),
		e_google_chooser_populate), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

gboolean
e_google_chooser_apply_selected (EGoogleChooser *chooser)
{
	ESourceSelectable *selectable_extension;
	ESourceWebdav *webdav_extension;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESource *source;
	GdkColor *color;
	SoupURI *soup_uri;
	gchar *color_spec;
	gchar *title;
	gchar *path;

	g_return_val_if_fail (E_IS_GOOGLE_CHOOSER (chooser), FALSE);

	source = e_google_chooser_get_source (chooser);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get (
		model, &iter,
		COLUMN_COLOR, &color,
		COLUMN_PATH, &path,
		COLUMN_TITLE, &title,
		-1);

	selectable_extension = e_source_get_extension (
		source, E_SOURCE_EXTENSION_CALENDAR);

	webdav_extension = e_source_get_extension (
		source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	e_source_set_display_name (source, title);

	e_source_webdav_set_display_name (webdav_extension, title);

	/* XXX Might be easier to expose get/set_path functions? */
	soup_uri = e_source_webdav_dup_soup_uri (webdav_extension);
	soup_uri_set_path (soup_uri, path);
	e_source_webdav_set_soup_uri (webdav_extension, soup_uri);
	soup_uri_free (soup_uri);

	color_spec = gdk_color_to_string (color);
	e_source_selectable_set_color (selectable_extension, color_spec);
	g_free (color_spec);

	gdk_color_free (color);
	g_free (title);
	g_free (path);

	return TRUE;
}

void
e_google_chooser_construct_default_uri (SoupURI *soup_uri,
                                        const gchar *username)
{
	gchar *decoded_user, *path;

	decoded_user = google_chooser_decode_user (username);
	if (!decoded_user)
		return;

	path = g_strdup_printf (CALDAV_EVENTS_PATH_FORMAT, decoded_user);

	soup_uri_set_user (soup_uri, decoded_user);
	soup_uri_set_path (soup_uri, path);

	g_free (decoded_user);
	g_free (path);
}
