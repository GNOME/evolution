/*
 * e-caldav-chooser.c
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

#include "e-caldav-chooser.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libsoup/soup.h>

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <e-util/e-util.h>

#define E_CALDAV_CHOOSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CALDAV_CHOOSER, ECaldavChooserPrivate))

#define XC(string) ((xmlChar *) string)

/* Standard Namespaces */
#define NS_WEBDAV  "DAV:"
#define NS_CALDAV  "urn:ietf:params:xml:ns:caldav"

/* Application-Specific Namespaces */
#define NS_CALSRV  "http://calendarserver.org/ns/"
#define NS_ICAL    "http://apple.com/ns/ical/"

typedef struct _Context Context;

struct _ECaldavChooserPrivate {
	ESourceRegistry *registry;
	ESource *source;
	ECalClientSourceType source_type;
	SoupSession *session;
	GList *user_address_set;
	gchar *password;
};

struct _Context {
	SoupSession *session;
	ESourceRegistry *registry;
	ESource *source;

	GCancellable *cancellable;
	gulong cancelled_handler_id;

	GList *user_address_set;
};

enum {
	PROP_0,
	PROP_REGISTRY,
	PROP_SOURCE,
	PROP_SOURCE_TYPE
};

/* Mainly for readability. */
enum {
	DEPTH_0,
	DEPTH_1
};

typedef enum {
	SUPPORTS_VEVENT = 1 << 0,
	SUPPORTS_VTODO = 1 << 1,
	SUPPORTS_VJOURNAL = 1 << 2,
	SUPPORTS_ALL = 0x7
} SupportedComponentSet;

enum {
	COLUMN_DISPLAY_NAME,		/* G_TYPE_STRING */
	COLUMN_PATH_ENCODED,		/* G_TYPE_STRING */
	COLUMN_PATH_DECODED,		/* G_TYPE_STRING */
	COLUMN_COLOR,			/* GDK_TYPE_COLOR */
	COLUMN_HAS_COLOR,		/* G_TYPE_BOOLEAN */
	NUM_COLUMNS
};

/* Forward Declarations */
static void	e_caldav_chooser_authenticator_init
				(ESourceAuthenticatorInterface *iface);
static void	caldav_chooser_get_collection_details
				(SoupSession *session,
				 SoupMessage *message,
				 const gchar *path_or_uri,
				 GSimpleAsyncResult *simple);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	ECaldavChooser,
	e_caldav_chooser,
	GTK_TYPE_TREE_VIEW,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_SOURCE_AUTHENTICATOR,
		e_caldav_chooser_authenticator_init))

static gconstpointer
compat_libxml_output_buffer_get_content (xmlOutputBufferPtr buf,
                                         gsize *out_len)
{
#ifdef LIBXML2_NEW_BUFFER
	*out_len = xmlOutputBufferGetSize (buf);
	return xmlOutputBufferGetContent (buf);
#else
	*out_len = buf->buffer->use;
	return buf->buffer->content;
#endif
}

static void
context_cancel_message (GCancellable *cancellable,
                        Context *context)
{
	soup_session_abort (context->session);
}

static Context *
context_new (ECaldavChooser *chooser,
             GCancellable *cancellable)
{
	Context *context;

	context = g_slice_new0 (Context);
	context->session = g_object_ref (chooser->priv->session);
	context->registry = g_object_ref (chooser->priv->registry);
	context->source = g_object_ref (chooser->priv->source);

	if (cancellable != NULL) {
		context->cancellable = g_object_ref (cancellable);
		context->cancelled_handler_id = g_cancellable_connect (
			context->cancellable,
			G_CALLBACK (context_cancel_message),
			context, (GDestroyNotify) NULL);
	}

	return context;
}

static void
context_free (Context *context)
{
	if (context->session != NULL)
		g_object_unref (context->session);

	if (context->registry != NULL)
		g_object_unref (context->registry);

	if (context->source != NULL)
		g_object_unref (context->source);

	if (context->cancellable != NULL) {
		g_cancellable_disconnect (
			context->cancellable,
			context->cancelled_handler_id);
		g_object_unref (context->cancellable);
	}

	g_list_free_full (
		context->user_address_set,
		(GDestroyNotify) g_free);

	g_slice_free (Context, context);
}

static ETrustPromptResponse
trust_prompt_sync (const ENamedParameters *parameters,
                   GCancellable *cancellable,
                   GError **error)
{
	EUserPrompter *prompter;
	gint response;

	g_return_val_if_fail (parameters != NULL, E_TRUST_PROMPT_RESPONSE_UNKNOWN);

	prompter = e_user_prompter_new ();
	g_return_val_if_fail (prompter != NULL, E_TRUST_PROMPT_RESPONSE_UNKNOWN);

	response = e_user_prompter_extension_prompt_sync (prompter, "ETrustPrompt::trust-prompt", parameters, NULL, cancellable, error);

	g_object_unref (prompter);

	if (response == 0)
		return E_TRUST_PROMPT_RESPONSE_REJECT;
	if (response == 1)
		return E_TRUST_PROMPT_RESPONSE_ACCEPT;
	if (response == 2)
		return E_TRUST_PROMPT_RESPONSE_ACCEPT_TEMPORARILY;
	if (response == -1)
		return E_TRUST_PROMPT_RESPONSE_REJECT_TEMPORARILY;

	return E_TRUST_PROMPT_RESPONSE_UNKNOWN;
}

static void
caldav_chooser_redirect (SoupMessage *message,
                         SoupSession *session)
{
	SoupURI *soup_uri;
	const gchar *location;

	if (!SOUP_STATUS_IS_REDIRECTION (message->status_code))
		return;

	location = soup_message_headers_get_list (
		message->response_headers, "Location");

	if (location == NULL)
		return;

	soup_uri = soup_uri_new_with_base (
		soup_message_get_uri (message), location);

	if (soup_uri == NULL) {
		soup_message_set_status_full (
			message, SOUP_STATUS_MALFORMED,
			"Invalid Redirect URL");
		return;
	}

	soup_message_set_uri (message, soup_uri);
	soup_session_requeue_message (session, message);

	soup_uri_free (soup_uri);
}

static G_GNUC_NULL_TERMINATED SoupMessage *
caldav_chooser_new_propfind (SoupSession *session,
                             SoupURI *soup_uri,
                             gint depth,
                             ...)
{
	GHashTable *namespaces;
	SoupMessage *message;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr node;
	xmlNsPtr ns;
	xmlOutputBufferPtr output;
	gconstpointer content;
	gsize length;
	gpointer key;
	va_list va;

	/* Construct the XML content. */

	doc = xmlNewDoc (XC ("1.0"));
	node = xmlNewDocNode (doc, NULL, XC ("propfind"), NULL);

	/* Build a hash table of namespace URIs to xmlNs structs. */
	namespaces = g_hash_table_new (NULL, NULL);

	ns = xmlNewNs (node, XC (NS_CALDAV), XC ("C"));
	g_hash_table_insert (namespaces, (gpointer) NS_CALDAV, ns);

	ns = xmlNewNs (node, XC (NS_CALSRV), XC ("CS"));
	g_hash_table_insert (namespaces, (gpointer) NS_CALSRV, ns);

	ns = xmlNewNs (node, XC (NS_ICAL), XC ("IC"));
	g_hash_table_insert (namespaces, (gpointer) NS_ICAL, ns);

	/* Add WebDAV last since we use it below. */
	ns = xmlNewNs (node, XC (NS_WEBDAV), XC ("D"));
	g_hash_table_insert (namespaces, (gpointer) NS_WEBDAV, ns);

	xmlSetNs (node, ns);
	xmlDocSetRootElement (doc, node);

	node = xmlNewTextChild (node, ns, XC ("prop"), NULL);

	va_start (va, depth);
	while ((key = va_arg (va, gpointer)) != NULL) {
		xmlChar *name;

		ns = g_hash_table_lookup (namespaces, key);
		name = va_arg (va, xmlChar *);

		if (ns != NULL && name != NULL)
			xmlNewTextChild (node, ns, name, NULL);
		else
			g_warn_if_reached ();
	}
	va_end (va);

	g_hash_table_destroy (namespaces);

	/* Construct the SoupMessage. */

	message = soup_message_new_from_uri (SOUP_METHOD_PROPFIND, soup_uri);

	soup_message_set_flags (message, SOUP_MESSAGE_NO_REDIRECT);

	soup_message_headers_append (
		message->request_headers,
		"User-Agent", "Evolution/" VERSION);

	soup_message_headers_append (
		message->request_headers,
		"Depth", (depth == 0) ? "0" : "1");

	output = xmlAllocOutputBuffer (NULL);

	root = xmlDocGetRootElement (doc);
	xmlNodeDumpOutput (output, doc, root, 0, 1, NULL);
	xmlOutputBufferFlush (output);

	content = compat_libxml_output_buffer_get_content (output, &length);

	soup_message_set_request (
		message, "application/xml", SOUP_MEMORY_COPY,
		content, length);

	xmlOutputBufferClose (output);

	soup_message_add_header_handler (
		message, "got-body", "Location",
		G_CALLBACK (caldav_chooser_redirect), session);

	return message;
}

static void
caldav_chooser_authenticate_cb (SoupSession *session,
                                SoupMessage *message,
                                SoupAuth *auth,
                                gboolean retrying,
                                ECaldavChooser *chooser)
{
	ESource *source;
	ESourceAuthentication *extension;
	const gchar *extension_name;
	const gchar *username;
	const gchar *password;

	source = e_caldav_chooser_get_source (chooser);
	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (source, extension_name);

	username = e_source_authentication_get_user (extension);
	password = chooser->priv->password;

	/* If our password was rejected, let the operation fail. */
	if (retrying)
		return;

	/* If we don't have a username, let the operation fail. */
	if (username == NULL || *username == '\0')
		return;

	/* If we don't have a password, let the operation fail. */
	if (password == NULL || *password == '\0')
		return;

	soup_auth_authenticate (auth, username, password);
}

static void
caldav_chooser_configure_session (ECaldavChooser *chooser,
                                  SoupSession *session)
{
	if (g_getenv ("CALDAV_DEBUG") != NULL) {
		SoupLogger *logger;

		logger = soup_logger_new (
			SOUP_LOGGER_LOG_BODY, 100 * 1024 * 1024);
		soup_session_add_feature (
			session, SOUP_SESSION_FEATURE (logger));
		g_object_unref (logger);
	}

	g_object_set (
		session,
		SOUP_SESSION_TIMEOUT, 90,
		SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE,
		SOUP_SESSION_SSL_STRICT, TRUE,
		NULL);

	g_signal_connect (
		session, "authenticate",
		G_CALLBACK (caldav_chooser_authenticate_cb), chooser);
}

static gboolean
caldav_chooser_check_successful (SoupMessage *message,
                                 GError **error)
{
	GIOErrorEnum error_code;

	/* Loosely copied from the GVFS DAV backend. */

	if (SOUP_STATUS_IS_SUCCESSFUL (message->status_code))
		return TRUE;

	switch (message->status_code) {
		case SOUP_STATUS_CANCELLED:
			error_code = G_IO_ERROR_CANCELLED;
			break;
		case SOUP_STATUS_NOT_FOUND:
			error_code = G_IO_ERROR_NOT_FOUND;
			break;
		case SOUP_STATUS_UNAUTHORIZED:
		case SOUP_STATUS_PAYMENT_REQUIRED:
		case SOUP_STATUS_FORBIDDEN:
			error_code = G_IO_ERROR_PERMISSION_DENIED;
			break;
		case SOUP_STATUS_REQUEST_TIMEOUT:
			error_code = G_IO_ERROR_TIMED_OUT;
			break;
		case SOUP_STATUS_CANT_RESOLVE:
			error_code = G_IO_ERROR_HOST_NOT_FOUND;
			break;
		case SOUP_STATUS_NOT_IMPLEMENTED:
			error_code = G_IO_ERROR_NOT_SUPPORTED;
			break;
		case SOUP_STATUS_INSUFFICIENT_STORAGE:
			error_code = G_IO_ERROR_NO_SPACE;
			break;
		default:
			error_code = G_IO_ERROR_FAILED;
			break;
	}

	g_set_error (
		error, G_IO_ERROR, error_code,
		_("HTTP Error: %s"), message->reason_phrase);

	return FALSE;
}

static xmlDocPtr
caldav_chooser_parse_xml (SoupMessage *message,
                          const gchar *expected_name,
                          GError **error)
{
	xmlDocPtr doc;
	xmlNodePtr root;

	if (!caldav_chooser_check_successful (message, error))
		return NULL;

	doc = xmlReadMemory (
		message->response_body->data,
		message->response_body->length,
		"response.xml", NULL,
		XML_PARSE_NONET |
		XML_PARSE_NOWARNING |
		XML_PARSE_NOCDATA |
		XML_PARSE_COMPACT);

	if (doc == NULL) {
		g_set_error_literal (
			error, G_IO_ERROR, G_IO_ERROR_FAILED,
			_("Could not parse response"));
		return NULL;
	}

	root = xmlDocGetRootElement (doc);

	if (root == NULL || root->children == NULL) {
		g_set_error_literal (
			error, G_IO_ERROR, G_IO_ERROR_FAILED,
			_("Empty response"));
		xmlFreeDoc (doc);
		return NULL;
	}

	if (g_strcmp0 ((gchar *) root->name, expected_name) != 0) {
		g_set_error_literal (
			error, G_IO_ERROR, G_IO_ERROR_FAILED,
			_("Unexpected reply from server"));
		xmlFreeDoc (doc);
		return NULL;
	}

	return doc;
}

static xmlXPathObjectPtr
caldav_chooser_get_xpath (xmlXPathContextPtr xp_ctx,
                          const gchar *path_format,
                          ...)
{
	xmlXPathObjectPtr xp_obj;
	va_list va;
	gchar *path;

	va_start (va, path_format);
	path = g_strdup_vprintf (path_format, va);
	va_end (va);

	xp_obj = xmlXPathEvalExpression (XC (path), xp_ctx);

	g_free (path);

	if (xp_obj == NULL)
		return NULL;

	if (xp_obj->type != XPATH_NODESET) {
		xmlXPathFreeObject (xp_obj);
		return NULL;
	}

	if (xmlXPathNodeSetGetLength (xp_obj->nodesetval) == 0) {
		xmlXPathFreeObject (xp_obj);
		return NULL;
	}

	return xp_obj;
}

static gchar *
caldav_chooser_get_xpath_string (xmlXPathContextPtr xp_ctx,
                                 const gchar *path_format,
                                 ...)
{
	xmlXPathObjectPtr xp_obj;
	va_list va;
	gchar *path;
	gchar *expression;
	gchar *string = NULL;

	va_start (va, path_format);
	path = g_strdup_vprintf (path_format, va);
	va_end (va);

	expression = g_strdup_printf ("string(%s)", path);
	xp_obj = xmlXPathEvalExpression (XC (expression), xp_ctx);
	g_free (expression);

	g_free (path);

	if (xp_obj == NULL)
		return NULL;

	if (xp_obj->type == XPATH_STRING)
		string = g_strdup ((gchar *) xp_obj->stringval);

	/* If the string is empty, return NULL. */
	if (string != NULL && *string == '\0') {
		g_free (string);
		string = NULL;
	}

	xmlXPathFreeObject (xp_obj);

	return string;
}

static void
caldav_chooser_process_user_address_set (xmlXPathContextPtr xp_ctx,
                                         Context *context)
{
	xmlXPathObjectPtr xp_obj;
	gint ii, length;

	/* XXX Is response[1] safe to assume? */
	xp_obj = caldav_chooser_get_xpath (
		xp_ctx,
		"/D:multistatus"
		"/D:response"
		"/D:propstat"
		"/D:prop"
		"/C:calendar-user-address-set");

	if (xp_obj == NULL)
		return;

	length = xmlXPathNodeSetGetLength (xp_obj->nodesetval);

	for (ii = 0; ii < length; ii++) {
		GList *duplicate;
		const gchar *address;
		gchar *href;

		href = caldav_chooser_get_xpath_string (
			xp_ctx,
			"/D:multistatus"
			"/D:response"
			"/D:propstat"
			"/D:prop"
			"/C:calendar-user-address-set"
			"/D:href[%d]", ii + 1);

		if (href == NULL)
			continue;

		if (!g_str_has_prefix (href, "mailto:")) {
			g_free (href);
			continue;
		}

		/* strlen("mailto:") == 7 */
		address = href + 7;

		/* Avoid duplicates. */
		duplicate = g_list_find_custom (
			context->user_address_set,
			address, (GCompareFunc) strdup);

		if (duplicate != NULL) {
			g_free (href);
			continue;
		}

		context->user_address_set = g_list_append (
			context->user_address_set, g_strdup (address));

		g_free (href);
	}

	xmlXPathFreeObject (xp_obj);
}

static SupportedComponentSet
caldav_chooser_get_supported_component_set (xmlXPathContextPtr xp_ctx,
                                            gint index)
{
	xmlXPathObjectPtr xp_obj;
	SupportedComponentSet set = 0;
	gint ii, length;

	xp_obj = caldav_chooser_get_xpath (
		xp_ctx,
		"/D:multistatus"
		"/D:response[%d]"
		"/D:propstat"
		"/D:prop"
		"/C:supported-calendar-component-set"
		"/C:comp", index);

	/* If the property is not present, assume all component
	 * types are supported.  (RFC 4791, Section 5.2.3) */
	if (xp_obj == NULL)
		return SUPPORTS_ALL;

	length = xmlXPathNodeSetGetLength (xp_obj->nodesetval);

	for (ii = 0; ii < length; ii++) {
		gchar *name;

		name = caldav_chooser_get_xpath_string (
			xp_ctx,
			"/D:multistatus"
			"/D:response[%d]"
			"/D:propstat"
			"/D:prop"
			"/C:supported-calendar-component-set"
			"/C:comp[%d]"
			"/@name", index, ii + 1);

		if (name == NULL)
			continue;

		if (g_ascii_strcasecmp (name, "VEVENT") == 0)
			set |= SUPPORTS_VEVENT;
		else if (g_ascii_strcasecmp (name, "VTODO") == 0)
			set |= SUPPORTS_VTODO;
		else if (g_ascii_strcasecmp (name, "VJOURNAL") == 0)
			set |= SUPPORTS_VJOURNAL;

		g_free (name);
	}

	xmlXPathFreeObject (xp_obj);

	return set;
}

static void
caldav_chooser_process_response (SoupSession *session,
                                 SoupMessage *message,
                                 GSimpleAsyncResult *simple,
                                 xmlXPathContextPtr xp_ctx,
                                 gint index)
{
	GObject *object;
	xmlXPathObjectPtr xp_obj;
	SupportedComponentSet comp_set;
	ECaldavChooser *chooser;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	GdkColor color;
	gchar *color_spec;
	gchar *display_name;
	gchar *href_decoded;
	gchar *href_encoded;
	gchar *status_line;
	guint status;
	gboolean has_color;
	gboolean success;

	/* This returns a new reference, for reasons passing understanding. */
	object = g_async_result_get_source_object (G_ASYNC_RESULT (simple));

	chooser = E_CALDAV_CHOOSER (object);
	tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));

	g_object_unref (object);

	status_line = caldav_chooser_get_xpath_string (
		xp_ctx,
		"/D:multistatus"
		"/D:response[%d]"
		"/D:propstat"
		"/D:status",
		index);

	if (status_line == NULL)
		return;

	success = soup_headers_parse_status_line (
		status_line, NULL, &status, NULL);

	g_free (status_line);

	if (!success || status != SOUP_STATUS_OK)
		return;

	href_encoded = caldav_chooser_get_xpath_string (
		xp_ctx,
		"/D:multistatus"
		"/D:response[%d]"
		"/D:href",
		index);

	if (href_encoded == NULL)
		return;

	href_decoded = soup_uri_decode (href_encoded);

	/* Get the display name or fall back to the href. */

	display_name = caldav_chooser_get_xpath_string (
		xp_ctx,
		"/D:multistatus"
		"/D:response[%d]"
		"/D:propstat"
		"/D:prop"
		"/D:displayname",
		index);

	if (display_name == NULL) {
		gchar *href_copy, *cp;

		href_copy = g_strdup (href_decoded);

		/* Use the last non-empty path segment. */
		while ((cp = strrchr (href_copy, '/')) != NULL) {
			if (*(cp + 1) == '\0')
				*cp = '\0';
			else {
				display_name = g_strdup (cp + 1);
				break;
			}
		}

		g_free (href_copy);
	}

	/* Make sure the resource is a calendar. */

	xp_obj = caldav_chooser_get_xpath (
		xp_ctx,
		"/D:multistatus"
		"/D:response[%d]"
		"/D:propstat"
		"/D:prop"
		"/D:resourcetype"
		"/C:calendar",
		index);

	if (xp_obj == NULL)
		goto exit;

	xmlXPathFreeObject (xp_obj);

	/* Get the color specification string. */

	color_spec = caldav_chooser_get_xpath_string (
		xp_ctx,
		"/D:multistatus"
		"/D:response[%d]"
		"/D:propstat"
		"/D:prop"
		"/IC:calendar-color",
		index);

	if (color_spec != NULL)
		has_color = gdk_color_parse (color_spec, &color);
	else
		has_color = FALSE;

	g_free (color_spec);

	/* Which calendar component types are supported? */

	comp_set = caldav_chooser_get_supported_component_set (xp_ctx, index);

	switch (e_caldav_chooser_get_source_type (chooser)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			if ((comp_set & SUPPORTS_VEVENT) == 0)
				goto exit;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			if ((comp_set & SUPPORTS_VJOURNAL) == 0)
				goto exit;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			if ((comp_set & SUPPORTS_VTODO) == 0)
				goto exit;
			break;
		default:
			goto exit;
	}

	/* Append a new tree model row. */

	gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (tree_model), &iter,
		COLUMN_DISPLAY_NAME, display_name,
		COLUMN_PATH_ENCODED, href_encoded,
		COLUMN_PATH_DECODED, href_decoded,
		COLUMN_COLOR, has_color ? &color : NULL,
		COLUMN_HAS_COLOR, has_color,
		-1);

exit:
	g_free (display_name);
	g_free (href_decoded);
	g_free (href_encoded);
}

static void
caldav_chooser_collection_details_cb (SoupSession *session,
                                      SoupMessage *message,
                                      GSimpleAsyncResult *simple)
{
	xmlDocPtr doc;
	xmlXPathContextPtr xp_ctx;
	xmlXPathObjectPtr xp_obj;
	GError *error = NULL;

	doc = caldav_chooser_parse_xml (message, "multistatus", &error);

	if (error != NULL) {
		g_warn_if_fail (doc == NULL);
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
		goto exit;
	}

	xp_ctx = xmlXPathNewContext (doc);
	xmlXPathRegisterNs (xp_ctx, XC ("D"), XC (NS_WEBDAV));
	xmlXPathRegisterNs (xp_ctx, XC ("C"), XC (NS_CALDAV));
	xmlXPathRegisterNs (xp_ctx, XC ("CS"), XC (NS_CALSRV));
	xmlXPathRegisterNs (xp_ctx, XC ("IC"), XC (NS_ICAL));

	xp_obj = caldav_chooser_get_xpath (
		xp_ctx,
		"/D:multistatus"
		"/D:response");

	if (xp_obj != NULL) {
		gint length, ii;

		length = xmlXPathNodeSetGetLength (xp_obj->nodesetval);

		for (ii = 0; ii < length; ii++)
			caldav_chooser_process_response (
				session, message, simple, xp_ctx, ii + 1);

		xmlXPathFreeObject (xp_obj);
	}

	xmlXPathFreeContext (xp_ctx);
	xmlFreeDoc (doc);

exit:
	/* If we were cancelled then we're in a GCancellable::cancelled
	 * signal handler right now and GCancellable has its mutex locked,
	 * which means calling g_cancellable_disconnect() now will deadlock
	 * when it too tries to acquire the mutex.  So defer the GAsyncResult
	 * completion to an idle callback to avoid this deadlock. */
	g_simple_async_result_complete_in_idle (simple);
	g_object_unref (simple);
}

static void
caldav_chooser_get_collection_details (SoupSession *session,
                                       SoupMessage *message,
                                       const gchar *path_or_uri,
                                       GSimpleAsyncResult *simple)
{
	SoupURI *soup_uri;

	soup_uri = soup_uri_new (path_or_uri);
	if (!soup_uri ||
	    !soup_uri_get_scheme (soup_uri) ||
	    !soup_uri_get_host (soup_uri) ||
	    !soup_uri_get_path (soup_uri) ||
	    !*soup_uri_get_scheme (soup_uri) ||
	    !*soup_uri_get_host (soup_uri) ||
	    !*soup_uri_get_path (soup_uri)) {
		/* it's a path only, not full uri */
		if (soup_uri)
			soup_uri_free (soup_uri);
		soup_uri = soup_uri_copy (soup_message_get_uri (message));
		soup_uri_set_path (soup_uri, path_or_uri);
	}

	message = caldav_chooser_new_propfind (
		session, soup_uri, DEPTH_1,
		NS_WEBDAV, XC ("displayname"),
		NS_WEBDAV, XC ("resourcetype"),
		NS_CALDAV, XC ("calendar-description"),
		NS_CALDAV, XC ("supported-calendar-component-set"),
		NS_CALDAV, XC ("calendar-user-address-set"),
		NS_CALSRV, XC ("getctag"),
		NS_ICAL,   XC ("calendar-color"),
		NULL);

	/* This takes ownership of the message. */
	soup_session_queue_message (
		session, message, (SoupSessionCallback)
		caldav_chooser_collection_details_cb, simple);

	soup_uri_free (soup_uri);
}

static void
caldav_chooser_calendar_home_set_cb (SoupSession *session,
                                     SoupMessage *message,
                                     GSimpleAsyncResult *simple)
{
	Context *context;
	SoupURI *soup_uri;
	xmlDocPtr doc;
	xmlXPathContextPtr xp_ctx;
	xmlXPathObjectPtr xp_obj;
	gchar *calendar_home_set;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (message->status_code == SOUP_STATUS_SSL_FAILED) {
		ETrustPromptResponse response;
		ENamedParameters *parameters;
		ESourceWebdav *extension;

		extension = e_source_get_extension (context->source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
		parameters = e_named_parameters_new ();

		response = e_source_webdav_prepare_ssl_trust_prompt (extension, message, context->registry, parameters);
		if (response == E_TRUST_PROMPT_RESPONSE_UNKNOWN) {
			response = trust_prompt_sync (parameters, context->cancellable, NULL);
			if (response != E_TRUST_PROMPT_RESPONSE_UNKNOWN)
				e_source_webdav_store_ssl_trust_prompt (extension, message, response);
		}

		e_named_parameters_free (parameters);

		if (response == E_TRUST_PROMPT_RESPONSE_ACCEPT ||
		    response == E_TRUST_PROMPT_RESPONSE_ACCEPT_TEMPORARILY) {
			g_object_set (context->session, SOUP_SESSION_SSL_STRICT, FALSE, NULL);

			soup_session_queue_message (
				context->session, g_object_ref (message), (SoupSessionCallback)
				caldav_chooser_calendar_home_set_cb, simple);
			return;
		}
	}

	doc = caldav_chooser_parse_xml (message, "multistatus", &error);

	/* If we were cancelled then we're in a GCancellable::cancelled
	 * signal handler right now and GCancellable has its mutex locked,
	 * which means calling g_cancellable_disconnect() now will deadlock
	 * when it too tries to acquire the mutex.  So defer the GAsyncResult
	 * completion to an idle callback to avoid this deadlock. */
	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		g_error_free (error);
		return;
	}

	g_return_if_fail (doc != NULL);

	xp_ctx = xmlXPathNewContext (doc);
	xmlXPathRegisterNs (xp_ctx, XC ("D"), XC (NS_WEBDAV));
	xmlXPathRegisterNs (xp_ctx, XC ("C"), XC (NS_CALDAV));

	/* Record any "C:calendar-user-address-set" properties. */
	caldav_chooser_process_user_address_set (xp_ctx, context);

	/* Try to find the calendar home URL using the
	 * following properties in order of preference:
	 *
	 *   "C:calendar-home-set"
	 *   "D:current-user-principal"
	 *   "D:principal-URL"
	 *
	 * If the second or third URL preference is used, rerun
	 * the PROPFIND method on that URL at Depth=1 in hopes
	 * of getting a proper "C:calendar-home-set" property.
	 */

	/* FIXME There can be multiple "D:href" elements for a
	 *       "C:calendar-home-set".  We're only processing
	 *       the first one.  Need to iterate over them. */

	calendar_home_set = caldav_chooser_get_xpath_string (
		xp_ctx,
		"/D:multistatus"
		"/D:response"
		"/D:propstat"
		"/D:prop"
		"/C:calendar-home-set"
		"/D:href");

	if (calendar_home_set != NULL)
		goto get_collection_details;

	g_free (calendar_home_set);

	calendar_home_set = caldav_chooser_get_xpath_string (
		xp_ctx,
		"/D:multistatus"
		"/D:response"
		"/D:propstat"
		"/D:prop"
		"/D:current-user-principal"
		"/D:href");

	if (calendar_home_set != NULL)
		goto retry_propfind;

	g_free (calendar_home_set);

	calendar_home_set = caldav_chooser_get_xpath_string (
		xp_ctx,
		"/D:multistatus"
		"/D:response"
		"/D:propstat"
		"/D:prop"
		"/D:principal-URL"
		"/D:href");

	if (calendar_home_set != NULL)
		goto retry_propfind;

	g_free (calendar_home_set);
	calendar_home_set = NULL;

	/* None of the aforementioned properties are present.  If the
	 * user-supplied CalDAV URL is a calendar resource, use that. */

	xp_obj = caldav_chooser_get_xpath (
		xp_ctx,
		"/D:multistatus"
		"/D:response"
		"/D:propstat"
		"/D:prop"
		"/D:resourcetype"
		"/C:calendar");

	if (xp_obj != NULL) {
		soup_uri = soup_message_get_uri (message);

		if (soup_uri->path != NULL && *soup_uri->path != '\0') {
			gchar *slash;

			soup_uri = soup_uri_copy (soup_uri);

			slash = strrchr (soup_uri->path, '/');
			while (slash != NULL && slash != soup_uri->path) {

				if (slash[1] != '\0') {
					slash[1] = '\0';
					calendar_home_set =
						g_strdup (soup_uri->path);
					break;
				}

				slash[0] = '\0';
				slash = strrchr (soup_uri->path, '/');
			}

			soup_uri_free (soup_uri);
		}

		xmlXPathFreeObject (xp_obj);
	}

	if (calendar_home_set == NULL || *calendar_home_set == '\0') {
		g_free (calendar_home_set);
		g_simple_async_result_set_error (
			simple, G_IO_ERROR, G_IO_ERROR_FAILED,
			_("Could not locate user's calendars"));
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
		return;
	}

get_collection_details:

	xmlXPathFreeContext (xp_ctx);
	xmlFreeDoc (doc);

	caldav_chooser_get_collection_details (
		session, message, calendar_home_set, simple);

	g_free (calendar_home_set);

	return;

retry_propfind:

	xmlXPathFreeContext (xp_ctx);
	xmlFreeDoc (doc);

	soup_uri = soup_uri_copy (soup_message_get_uri (message));
	soup_uri_set_path (soup_uri, calendar_home_set);

	/* Note that we omit "D:resourcetype", "D:current-user-principal"
	 * and "D:principal-URL" in order to short-circuit the recursion. */
	message = caldav_chooser_new_propfind (
		session, soup_uri, DEPTH_1,
		NS_CALDAV, XC ("calendar-home-set"),
		NS_CALDAV, XC ("calendar-user-address-set"),
		NULL);

	/* This takes ownership of the message. */
	soup_session_queue_message (
		session, message, (SoupSessionCallback)
		caldav_chooser_calendar_home_set_cb, simple);

	soup_uri_free (soup_uri);

	g_free (calendar_home_set);
}

static void
caldav_chooser_set_registry (ECaldavChooser *chooser,
                             ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (chooser->priv->registry == NULL);

	chooser->priv->registry = g_object_ref (registry);
}

static void
caldav_chooser_set_source (ECaldavChooser *chooser,
                           ESource *source)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (chooser->priv->source == NULL);

	chooser->priv->source = g_object_ref (source);
}

static void
caldav_chooser_set_source_type (ECaldavChooser *chooser,
                                ECalClientSourceType source_type)
{
	chooser->priv->source_type = source_type;
}

static void
caldav_chooser_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			caldav_chooser_set_registry (
				E_CALDAV_CHOOSER (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE:
			caldav_chooser_set_source (
				E_CALDAV_CHOOSER (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE_TYPE:
			caldav_chooser_set_source_type (
				E_CALDAV_CHOOSER (object),
				g_value_get_enum (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
caldav_chooser_get_property (GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value, e_caldav_chooser_get_registry (
				E_CALDAV_CHOOSER (object)));
			return;

		case PROP_SOURCE:
			g_value_set_object (
				value, e_caldav_chooser_get_source (
				E_CALDAV_CHOOSER (object)));
			return;

		case PROP_SOURCE_TYPE:
			g_value_set_enum (
				value, e_caldav_chooser_get_source_type (
				E_CALDAV_CHOOSER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
caldav_chooser_dispose (GObject *object)
{
	ECaldavChooserPrivate *priv;

	priv = E_CALDAV_CHOOSER_GET_PRIVATE (object);

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->source != NULL) {
		g_object_unref (priv->source);
		priv->source = NULL;
	}

	if (priv->session != NULL) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_caldav_chooser_parent_class)->dispose (object);
}

static void
caldav_chooser_finalize (GObject *object)
{
	ECaldavChooserPrivate *priv;

	priv = E_CALDAV_CHOOSER_GET_PRIVATE (object);

	g_list_free_full (
		priv->user_address_set,
		(GDestroyNotify) g_free);

	g_free (priv->password);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_caldav_chooser_parent_class)->finalize (object);
}

static void
caldav_chooser_constructed (GObject *object)
{
	ECaldavChooser *chooser;
	GtkTreeView *tree_view;
	GtkListStore *list_store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	SoupSession *session;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_caldav_chooser_parent_class)->constructed (object);

	chooser = E_CALDAV_CHOOSER (object);
	session = soup_session_new ();
	caldav_chooser_configure_session (chooser, session);
	chooser->priv->session = session;

	tree_view = GTK_TREE_VIEW (object);

	list_store = gtk_list_store_new (
		NUM_COLUMNS,
		G_TYPE_STRING,		/* COLUMN_DISPLAY_NAME */
		G_TYPE_STRING,		/* COLUMN_PATH_ENCODED */
		G_TYPE_STRING,		/* COLUMN_PATH_DECODED */
		GDK_TYPE_COLOR,		/* COLUMN_COLOR */
		G_TYPE_BOOLEAN);	/* COLUMN_HAS_COLOR */

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Name"));
	gtk_tree_view_insert_column (tree_view, column, -1);

	renderer = e_cell_renderer_color_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (
		column, renderer,
		"color", COLUMN_COLOR,
		"visible", COLUMN_HAS_COLOR,
		NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (
		column, renderer,
		"text", COLUMN_DISPLAY_NAME,
		NULL);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_title (column, _("Path"));
	gtk_tree_view_insert_column (tree_view, column, -1);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (
		column, renderer,
		"text", COLUMN_PATH_DECODED,
		NULL);
}

/* Helper for caldav_chooser_try_password_sync() */
static void
caldav_chooser_try_password_cancelled_cb (GCancellable *cancellable,
                                          SoupSession *session)
{
	soup_session_abort (session);
}

static ESourceAuthenticationResult
caldav_chooser_try_password_sync (ESourceAuthenticator *auth,
                                  const GString *password,
                                  GCancellable *cancellable,
                                  GError **error)
{
	ECaldavChooser *chooser;
	ESourceAuthenticationResult result;
	SoupMessage *message;
	SoupSession *session;
	SoupURI *soup_uri;
	ESource *source;
	ESourceWebdav *extension;
	const gchar *extension_name;
	gulong cancel_id = 0;
	GError *local_error = NULL;

	chooser = E_CALDAV_CHOOSER (auth);

	/* Cache the password for later use in our
	 * SoupSession::authenticate signal handler. */
	g_free (chooser->priv->password);
	chooser->priv->password = g_strdup (password->str);

	/* Create our own SoupSession so we
	 * can try the password synchronously. */
	session = soup_session_new ();
	caldav_chooser_configure_session (chooser, session);

	source = e_caldav_chooser_get_source (chooser);
	extension_name = E_SOURCE_EXTENSION_WEBDAV_BACKEND;
	extension = e_source_get_extension (source, extension_name);

	soup_uri = e_source_webdav_dup_soup_uri (extension);
	g_return_val_if_fail (soup_uri != NULL, E_SOURCE_AUTHENTICATION_ERROR);

	/* Try some simple PROPFIND query.  We don't care about the query
	 * result, only whether the CalDAV server will accept our password. */
	message = caldav_chooser_new_propfind (
		session, soup_uri, DEPTH_0,
		NS_WEBDAV, XC ("resourcetype"),
		NULL);

	if (G_IS_CANCELLABLE (cancellable))
		cancel_id = g_cancellable_connect (
			cancellable,
			G_CALLBACK (caldav_chooser_try_password_cancelled_cb),
			g_object_ref (session),
			(GDestroyNotify) g_object_unref);

	g_object_set (session, SOUP_SESSION_SSL_STRICT, TRUE, NULL);
	g_object_set (chooser->priv->session, SOUP_SESSION_SSL_STRICT, TRUE, NULL);

	if (soup_session_send_message (session, message) == SOUP_STATUS_SSL_FAILED) {
		ETrustPromptResponse response;
		ENamedParameters *parameters;

		parameters = e_named_parameters_new ();

		response = e_source_webdav_prepare_ssl_trust_prompt (extension, message, chooser->priv->registry, parameters);
		if (response == E_TRUST_PROMPT_RESPONSE_UNKNOWN) {
			response = trust_prompt_sync (parameters, cancellable, NULL);
			if (response != E_TRUST_PROMPT_RESPONSE_UNKNOWN)
				e_source_webdav_store_ssl_trust_prompt (extension, message, response);
		}

		e_named_parameters_free (parameters);

		if (response == E_TRUST_PROMPT_RESPONSE_ACCEPT ||
		    response == E_TRUST_PROMPT_RESPONSE_ACCEPT_TEMPORARILY) {
			g_object_set (session, SOUP_SESSION_SSL_STRICT, FALSE, NULL);
			g_object_set (chooser->priv->session, SOUP_SESSION_SSL_STRICT, FALSE, NULL);
			soup_session_send_message (session, message);
		}
	}

	if (cancel_id > 0)
		g_cancellable_disconnect (cancellable, cancel_id);

	if (caldav_chooser_check_successful (message, &local_error)) {
		result = E_SOURCE_AUTHENTICATION_ACCEPTED;

	} else if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED)) {
		result = E_SOURCE_AUTHENTICATION_REJECTED;
		g_clear_error (&local_error);

	} else {
		result = E_SOURCE_AUTHENTICATION_ERROR;
	}

	if (local_error != NULL)
		g_propagate_error (error, local_error);

	g_object_unref (message);
	g_object_unref (session);

	soup_uri_free (soup_uri);

	return result;
}

static void
e_caldav_chooser_class_init (ECaldavChooserClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ECaldavChooserPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = caldav_chooser_set_property;
	object_class->get_property = caldav_chooser_get_property;
	object_class->dispose = caldav_chooser_dispose;
	object_class->finalize = caldav_chooser_finalize;
	object_class->constructed = caldav_chooser_constructed;

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			"Source",
			"CalDAV data source",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_TYPE,
		g_param_spec_enum (
			"source-type",
			"Source Type",
			"The iCalendar object type",
			E_TYPE_CAL_CLIENT_SOURCE_TYPE,
			E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_caldav_chooser_class_finalize (ECaldavChooserClass *class)
{
}

static void
e_caldav_chooser_authenticator_init (ESourceAuthenticatorInterface *iface)
{
	iface->try_password_sync = caldav_chooser_try_password_sync;
}

static void
e_caldav_chooser_init (ECaldavChooser *chooser)
{
	chooser->priv = E_CALDAV_CHOOSER_GET_PRIVATE (chooser);
}

void
e_caldav_chooser_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_caldav_chooser_register_type (type_module);
}

GtkWidget *
e_caldav_chooser_new (ESourceRegistry *registry,
                      ESource *source,
                      ECalClientSourceType source_type)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return g_object_new (
		E_TYPE_CALDAV_CHOOSER,
		"registry", registry, "source", source,
		"source-type", source_type, NULL);
}

ESourceRegistry *
e_caldav_chooser_get_registry (ECaldavChooser *chooser)
{
	g_return_val_if_fail (E_IS_CALDAV_CHOOSER (chooser), NULL);

	return chooser->priv->registry;
}

ESource *
e_caldav_chooser_get_source (ECaldavChooser *chooser)
{
	g_return_val_if_fail (E_IS_CALDAV_CHOOSER (chooser), NULL);

	return chooser->priv->source;
}

ECalClientSourceType
e_caldav_chooser_get_source_type (ECaldavChooser *chooser)
{
	g_return_val_if_fail (E_IS_CALDAV_CHOOSER (chooser), 0);

	return chooser->priv->source_type;
}

void
e_caldav_chooser_populate (ECaldavChooser *chooser,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	Context *context;
	ESource *source;
	SoupURI *soup_uri;
	SoupMessage *message;
	ESourceWebdav *extension;
	GtkTreeModel *tree_model;
	GSimpleAsyncResult *simple;
	const gchar *extension_name;

	g_return_if_fail (E_IS_CALDAV_CHOOSER (chooser));

	tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser));
	gtk_list_store_clear (GTK_LIST_STORE (tree_model));
	soup_session_abort (chooser->priv->session);

	source = e_caldav_chooser_get_source (chooser);
	extension_name = E_SOURCE_EXTENSION_WEBDAV_BACKEND;
	extension = e_source_get_extension (source, extension_name);

	soup_uri = e_source_webdav_dup_soup_uri (extension);
	g_return_if_fail (soup_uri != NULL);

	context = context_new (chooser, cancellable);

	simple = g_simple_async_result_new (
		G_OBJECT (chooser), callback,
		user_data, e_caldav_chooser_populate);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) context_free);

	message = caldav_chooser_new_propfind (
		context->session, soup_uri, DEPTH_0,
		NS_WEBDAV, XC ("resourcetype"),
		NS_CALDAV, XC ("calendar-home-set"),
		NS_CALDAV, XC ("calendar-user-address-set"),
		NS_WEBDAV, XC ("current-user-principal"),
		NS_WEBDAV, XC ("principal-URL"),
		NULL);

	/* This takes ownership of the message. */
	soup_session_queue_message (
		context->session, message, (SoupSessionCallback)
		caldav_chooser_calendar_home_set_cb, simple);

	soup_uri_free (soup_uri);
}

gboolean
e_caldav_chooser_populate_finish (ECaldavChooser *chooser,
                                  GAsyncResult *result,
                                  GError **error)
{
	GSimpleAsyncResult *simple;
	Context *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (chooser),
		e_caldav_chooser_populate), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	/* Transfer user addresses to the private struct. */

	g_list_free_full (
		chooser->priv->user_address_set,
		(GDestroyNotify) g_free);

	chooser->priv->user_address_set = context->user_address_set;
	context->user_address_set = NULL;

	return TRUE;
}

gboolean
e_caldav_chooser_apply_selected (ECaldavChooser *chooser)
{
	ESourceWebdav *webdav_extension;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	ESource *source;
	GdkColor *color;
	gboolean has_color;
	gchar *display_name;
	gchar *path_encoded;

	g_return_val_if_fail (E_IS_CALDAV_CHOOSER (chooser), FALSE);

	source = e_caldav_chooser_get_source (chooser);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser));

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get (
		model, &iter,
		COLUMN_DISPLAY_NAME, &display_name,
		COLUMN_PATH_ENCODED, &path_encoded,
		COLUMN_HAS_COLOR, &has_color,
		COLUMN_COLOR, &color,
		-1);

	/* Sanity check. */
	g_warn_if_fail (
		(has_color && color != NULL) ||
		(!has_color && color == NULL));

	webdav_extension = e_source_get_extension (
		source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);

	e_source_set_display_name (source, display_name);

	e_source_webdav_set_display_name (webdav_extension, display_name);
	e_source_webdav_set_resource_path (webdav_extension, path_encoded);

	/* XXX For now just pick the first user address in the list.
	 *     Might be better to compare the list against our own mail
	 *     accounts and give preference to matches (especially if an
	 *     address matches the default mail account), but I'm not sure
	 *     if multiple user addresses are common enough to justify the
	 *     extra effort. */
	if (chooser->priv->user_address_set != NULL)
		e_source_webdav_set_email_address (
			webdav_extension,
			chooser->priv->user_address_set->data);

	if (has_color) {
		ESourceSelectable *selectable_extension;
		const gchar *extension_name;
		gchar *color_spec;

		switch (e_caldav_chooser_get_source_type (chooser)) {
			case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
				extension_name = E_SOURCE_EXTENSION_CALENDAR;
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
				extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
				break;
			case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
				extension_name = E_SOURCE_EXTENSION_TASK_LIST;
				break;
			default:
				g_return_val_if_reached (TRUE);
		}

		selectable_extension =
			e_source_get_extension (source, extension_name);

		color_spec = gdk_color_to_string (color);
		e_source_selectable_set_color (
			selectable_extension, color_spec);
		g_free (color_spec);

		gdk_color_free (color);
	}

	g_free (display_name);
	g_free (path_encoded);

	return TRUE;
}

