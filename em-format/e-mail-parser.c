/*
 * e-mail-parser.c
 *
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
 */

#include "e-mail-parser.h"
#include "e-mail-parser-extension.h"
#include "e-mail-format-extensions.h"
#include "e-mail-part-attachment.h"
#include "e-mail-part-utils.h"

#include <camel/camel.h>
#include <libebackend/libebackend.h>

#include <e-util/e-util.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>

#include <widgets/misc/e-attachment.h>

#include <string.h>

static gpointer parent_class = 0;

struct _EMailParserPrivate {
	GMutex *mutex;

	gint last_error;

	CamelSession *session;
};

#define E_MAIL_PARSER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_PARSER, EMailParserPrivate))

#define d(x)

enum {
	PROP_0,
	PROP_SESSION
};

static GSList *
mail_parser_run (EMailParser *parser,
                 CamelMimeMessage *message,
                 GCancellable *cancellable)
{
	GSList *parts;
	EMailExtensionRegistry *reg;
	GQueue *parsers;
	GList *iter;
	GString *part_id;

	reg = e_mail_parser_get_extension_registry (parser);

	parsers = e_mail_extension_registry_get_for_mime_type (
			reg, "application/vnd.evolution.message");

	if (!parsers)
		parsers = e_mail_extension_registry_get_for_mime_type (
			reg, "message/*");

	/* parsers == NULL means, that the internal Evolution parser extensions
	 * were not loaded. Something is terribly wrong. */
	g_return_val_if_fail (parsers != NULL, NULL);

	part_id = g_string_new (".message");
	parts = NULL;

	if (!parsers) {
		parts = e_mail_parser_wrap_as_attachment (
				parser, CAMEL_MIME_PART (message),
				NULL, part_id, cancellable);
	} else {
		for (iter = parsers->head; iter; iter = iter->next) {

			EMailParserExtension *extension;

			if (g_cancellable_is_cancelled (cancellable))
				break;

			extension = iter->data;
			if (!extension)
				continue;

			parts = e_mail_parser_extension_parse (
					extension, parser, CAMEL_MIME_PART (message),
					part_id, cancellable);

			if (parts != NULL)
				break;
		}

		parts = g_slist_prepend (
				parts,
				e_mail_part_new (
					CAMEL_MIME_PART (message),
					".message"));
	}

	g_string_free (part_id, TRUE);

	return parts;
}

static void
mail_parser_set_session (EMailParser *parser,
                         CamelSession *session)
{
	g_return_if_fail (E_IS_MAIL_PARSER (parser));
	g_return_if_fail (CAMEL_IS_SESSION (session));

	g_object_ref (session);

	if (parser->priv->session)
		g_object_unref (parser->priv->session);

	parser->priv->session = session;
}

static void
e_mail_parser_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	EMailParser *parser = E_MAIL_PARSER (object);

	switch (property_id) {
		case PROP_SESSION:
			mail_parser_set_session (
				parser,
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_parser_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	EMailParser *parser = E_MAIL_PARSER (object);

	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_parser_get_session (parser));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_parser_finalize (GObject *object)
{
	EMailParserPrivate *priv;

	priv = E_MAIL_PARSER (object)->priv;

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}
static void
e_mail_parser_init (EMailParser *parser)
{
	parser->priv = E_MAIL_PARSER_GET_PRIVATE (parser);

	parser->priv->mutex = g_mutex_new ();
}

static void
e_mail_parser_base_init (EMailParserClass *class)
{
	class->extension_registry = g_object_new (
		E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY, NULL);

	e_mail_parser_internal_extensions_load (
		E_MAIL_EXTENSION_REGISTRY (class->extension_registry));

	e_extensible_load_extensions (E_EXTENSIBLE (class->extension_registry));
}

static void
e_mail_parser_base_finalize (EMailParserClass *class)
{
	g_object_unref (class->extension_registry);
}

static void
e_mail_parser_class_init (EMailParserClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailParserPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = e_mail_parser_finalize;
	object_class->set_property = e_mail_parser_set_property;
	object_class->get_property = e_mail_parser_get_property;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			"Camel Session",
			NULL,
			CAMEL_TYPE_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

GType
e_mail_parser_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailParserClass),
			(GBaseInitFunc) e_mail_parser_base_init,
			(GBaseFinalizeFunc) e_mail_parser_base_finalize,
			(GClassInitFunc) e_mail_parser_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailParser),
			0,     /* n_preallocs */
			(GInstanceInitFunc) e_mail_parser_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EMailParser",
			&type_info, 0);
	}

	return type;
}

EMailParser *
e_mail_parser_new (CamelSession *session)
{
	return g_object_new (
		E_TYPE_MAIL_PARSER,
		"session", session, NULL);
}

/**
 * e_mail_parser_parse_sync:
 * @parser: an #EMailParser
 * @folder: (allow none) a #CamelFolder containing the @message or %NULL
 * @message_uid: (allow none) UID of the @message within the @folder or %NULL
 * @message: a #CamelMimeMessage
 * @cancellable: (allow-none) a #GCancellable
 *
 * Parses the @message synchronously. Returns a list of #EMailPart<!-//>s which
 * represents structure of the message and additional properties of each part.
 *
 * Note that this function can block for a while, so it's not a good idea to call
 * it from main thread.
 *
 * Return Value: An #EMailPartsList
 */
EMailPartList *
e_mail_parser_parse_sync (EMailParser *parser,
                          CamelFolder *folder,
                          const gchar *message_uid,
                          CamelMimeMessage *message,
                          GCancellable *cancellable)
{
	EMailPartList *parts_list;

	g_return_val_if_fail (E_IS_MAIL_PARSER (parser), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	parts_list = e_mail_part_list_new ();

	if (folder)
		parts_list->folder = g_object_ref (folder);

	if (message_uid)
		parts_list->message_uid = g_strdup (message_uid);

	parts_list->message = g_object_ref (message);

	parts_list->list = mail_parser_run (parser, message, cancellable);

	if (camel_debug_start ("emformat:parser")) {
		GSList *iter;

		printf (
			"%s finished with EMailPartList:\n",
			G_OBJECT_TYPE_NAME (parser));

		for (iter = parts_list->list; iter; iter = iter->next) {
			EMailPart *part = iter->data;
			if (!part) continue;
			printf (
				"	id: %s | cid: %s | mime_type: %s | is_hidden: %d | is_attachment: %d\n",
				part->id, part->cid, part->mime_type,
				part->is_hidden ? 1 : 0,
				part->is_attachment ? 1 : 0);
		}

		camel_debug_end ();
	}

	return parts_list;
}

static void
mail_parser_prepare_async (GSimpleAsyncResult *res,
                           GObject *object,
                           GCancellable *cancellable)
{
	CamelMimeMessage *message;
	GSList *list;

	message = g_object_get_data (G_OBJECT (res), "message");

	list = mail_parser_run (E_MAIL_PARSER (object), message, cancellable);

	g_simple_async_result_set_op_res_gpointer (res, list, NULL);
}

/**
 * e_mail_parser_parse:
 * @parser: an #EMailParser
 * @message: a #CamelMimeMessage
 * @callback: a #GAsyncReadyCallback
 * @cancellable: (allow-none) a #GCancellable
 * @user_data: (allow-none) user data passed to the callback
 *
 * Asynchronous version of #e_mail_parser_parse_sync().
 */
void
e_mail_parser_parse (EMailParser *parser,
                     CamelFolder *folder,
                     const gchar *message_uid,
                     CamelMimeMessage *message,
                     GAsyncReadyCallback callback,
                     GCancellable *cancellable,
                     gpointer user_data)
{
	GSimpleAsyncResult *result;

	g_return_if_fail (E_IS_MAIL_PARSER (parser));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	result = g_simple_async_result_new (
		G_OBJECT (parser), callback,
		user_data, e_mail_parser_parse);

	g_object_set_data (G_OBJECT (result), "message", g_object_ref (message));

	if (folder)
		g_object_set_data (G_OBJECT (result), "folder", g_object_ref (folder));
	if (message_uid)
		g_object_set_data (G_OBJECT (result), "message_uid", g_strdup (message_uid));

	g_simple_async_result_run_in_thread (
		result, mail_parser_prepare_async, G_PRIORITY_DEFAULT, cancellable);
}

EMailPartList *
e_mail_parser_parse_finish (EMailParser *parser,
                            GAsyncResult *result,
                            GError **error)
{
	EMailPartList *parts_list;

	parts_list = e_mail_part_list_new ();

	/* The data were ref'ed or copied in e_mail_parser_parse_async */
	parts_list->message = g_object_get_data (G_OBJECT (result), "message");
	parts_list->folder = g_object_get_data (G_OBJECT (result), "folder");
	parts_list->message_uid = g_object_get_data (G_OBJECT (result), "message_uid");

	parts_list->list = g_simple_async_result_get_op_res_gpointer (
					G_SIMPLE_ASYNC_RESULT (result));

	if (camel_debug_start ("emformat:parser")) {
		GSList *iter;

		printf (
			"%s finished with EMailPartList:\n",
				G_OBJECT_TYPE_NAME (parser));

		for (iter = parts_list->list; iter; iter = iter->next) {
			EMailPart *part = iter->data;
			if (!part) continue;
			printf (
				"	id: %s | cid: %s | mime_type: %s | is_hidden: %d | is_attachment: %d\n",
				part->id, part->cid, part->mime_type,
				part->is_hidden ? 1 : 0,
				part->is_attachment ? 1 : 0);
		}

		camel_debug_end ();
	}

	return parts_list;
}

GSList *
e_mail_parser_parse_part (EMailParser *parser,
                          CamelMimePart *part,
                          GString *part_id,
                          GCancellable *cancellable)
{
	CamelContentType *ct;
	gchar *mime_type;
	GSList *list;

	ct = camel_mime_part_get_content_type (part);
	if (!ct) {
		mime_type = (gchar *) "application/vnd.evolution.error";
	} else {
		gchar *tmp;
		tmp = camel_content_type_simple (ct);
		mime_type = g_ascii_strdown (tmp, -1);
		g_free (tmp);
	}

	list = e_mail_parser_parse_part_as (
			parser, part, part_id, mime_type, cancellable);

	if (ct) {
		g_free (mime_type);
	}

	return list;
}

GSList *
e_mail_parser_parse_part_as (EMailParser *parser,
                             CamelMimePart *part,
                             GString *part_id,
                             const gchar *mime_type,
                             GCancellable *cancellable)
{
	GQueue *parsers;
	GList *iter;
	EMailExtensionRegistry *reg;
	EMailParserClass *parser_class;
	GSList *part_list;
	gchar *as_mime_type;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	if (mime_type)
		as_mime_type = g_ascii_strdown (mime_type, -1);
	else
		as_mime_type = NULL;

	parser_class = E_MAIL_PARSER_GET_CLASS (parser);
	reg = E_MAIL_EXTENSION_REGISTRY (parser_class->extension_registry);

	parsers = e_mail_extension_registry_get_for_mime_type (reg, as_mime_type);
	if (!parsers) {
		parsers = e_mail_extension_registry_get_fallback (reg, as_mime_type);
	}

	if (as_mime_type)
		g_free (as_mime_type);

	if (!parsers) {
		return e_mail_parser_wrap_as_attachment (
				parser, part, NULL, part_id, cancellable);
	}

	for (iter = parsers->head; iter; iter = iter->next) {
		EMailParserExtension *extension;

		extension = iter->data;
		if (!extension)
			continue;

		part_list = e_mail_parser_extension_parse (
				extension, parser, part, part_id, cancellable);

		if (part_list)
			break;
	}

	return part_list;
}

GSList *
e_mail_parser_error (EMailParser *parser,
                     GCancellable *cancellable,
                     const gchar *format,
                     ...)
{
	EMailPart *mail_part;
	CamelMimePart *part;
	gchar *errmsg;
	gchar *uri;
	va_list ap;

	g_return_val_if_fail (E_IS_MAIL_PARSER (parser), NULL);
	g_return_val_if_fail (format != NULL, NULL);

	va_start (ap, format);
	errmsg = g_strdup_vprintf (format, ap);

	part = camel_mime_part_new ();
	camel_mime_part_set_content (
		part,
		errmsg, strlen (errmsg),
		"application/vnd.evolution.error");
	g_free (errmsg);
	va_end (ap);

	g_mutex_lock (parser->priv->mutex);
	parser->priv->last_error++;
	uri = g_strdup_printf (".error.%d", parser->priv->last_error);
	g_mutex_unlock (parser->priv->mutex);

	mail_part = e_mail_part_new (part, uri);
	mail_part->mime_type = g_strdup ("application/vnd.evolution.error");
	mail_part->is_error = TRUE;

	g_free (uri);
	g_object_unref (part);

	return g_slist_append (NULL, mail_part);
}

static void
attachment_loaded (EAttachment *attachment,
                   GAsyncResult *res,
                   gpointer user_data)
{
	EShell *shell;
	GtkWindow *window;

	shell = e_shell_get_default ();
	window = e_shell_get_active_window (shell);

	e_attachment_load_handle_error (attachment, res, window);

	g_object_unref (attachment);
}

/* Idle callback */
static gboolean
load_attachment_idle (EAttachment *attachment)
{
	e_attachment_load_async (
		attachment,
		(GAsyncReadyCallback) attachment_loaded, NULL);

	return FALSE;
}

GSList *
e_mail_parser_wrap_as_attachment (EMailParser *parser,
                                  CamelMimePart *part,
                                  GSList *parts,
                                  GString *part_id,
                                  GCancellable *cancellable)
{
	EMailPartAttachment *empa;
	const gchar *snoop_mime_type, *cid;
	GQueue *extensions;
	CamelContentType *ct;
	gchar *mime_type;
	CamelDataWrapper *dw;
	GByteArray *ba;
	gsize size;
	gint part_id_len;

	ct = camel_mime_part_get_content_type (part);
	extensions = NULL;
	snoop_mime_type = NULL;
	if (ct) {
		EMailExtensionRegistry *reg;
		mime_type = camel_content_type_simple (ct);

		reg = e_mail_parser_get_extension_registry (parser);
		extensions = e_mail_extension_registry_get_for_mime_type (
				reg, mime_type);

		if (camel_content_type_is (ct, "text", "*") ||
		    camel_content_type_is (ct, "message", "*"))
			snoop_mime_type = mime_type;
		else
			g_free (mime_type);
	}

	if (!snoop_mime_type)
		snoop_mime_type = e_mail_part_snoop_type (part);

	if (!extensions) {
		EMailExtensionRegistry *reg;

		reg = e_mail_parser_get_extension_registry (parser);
		extensions = e_mail_extension_registry_get_for_mime_type (
				reg, snoop_mime_type);

		if (!extensions) {
			extensions = e_mail_extension_registry_get_fallback (
				reg, snoop_mime_type);
		}
	}

	part_id_len = part_id->len;
	g_string_append (part_id, ".attachment");

	empa = (EMailPartAttachment *) e_mail_part_subclass_new (
			part, part_id->str, sizeof (EMailPartAttachment),
			(GFreeFunc) e_mail_part_attachment_free);
	empa->parent.mime_type = g_strdup ("application/vnd.evolution.attachment");
	empa->parent.is_attachment = TRUE;
	empa->shown = extensions && (!g_queue_is_empty (extensions) &&
			e_mail_part_is_inline (part, extensions));
	empa->snoop_mime_type = snoop_mime_type;
	empa->attachment = e_attachment_new ();
	empa->attachment_view_part_id = parts ? g_strdup (E_MAIL_PART (parts->data)->id) : NULL;

	cid = camel_mime_part_get_content_id (part);
	if (cid)
		empa->parent.cid = g_strdup_printf ("cid:%s", cid);

	e_attachment_set_mime_part (empa->attachment, part);
	e_attachment_set_shown (empa->attachment, empa->shown);
	e_attachment_set_can_show (
		empa->attachment,
		extensions && !g_queue_is_empty (extensions));

	/* Try to guess size of the attachments */
	dw = camel_medium_get_content (CAMEL_MEDIUM (part));
	ba = camel_data_wrapper_get_byte_array (dw);
	if (ba) {
		size = ba->len;

		if (camel_mime_part_get_encoding (part) == CAMEL_TRANSFER_ENCODING_BASE64)
			size = size / 1.37;
	} else {
		size = 0;
	}

	/* e_attachment_load_async must be called from main thread */
	g_idle_add (
		(GSourceFunc) load_attachment_idle,
		g_object_ref (empa->attachment));

	if (size != 0) {
		GFileInfo *fileinfo;

		fileinfo = e_attachment_get_file_info (empa->attachment);

		if (!fileinfo) {
			fileinfo = g_file_info_new ();
			g_file_info_set_content_type (
				fileinfo, empa->snoop_mime_type);
		} else {
			g_object_ref (fileinfo);
		}

		g_file_info_set_size (fileinfo, size);
		e_attachment_set_file_info (empa->attachment, fileinfo);

		g_object_unref (fileinfo);
	}

	if (parts && parts->data) {
		E_MAIL_PART (parts->data)->is_hidden = TRUE;
	}

	g_string_truncate (part_id, part_id_len);

	return g_slist_prepend (parts, empa);
}

CamelSession *
e_mail_parser_get_session (EMailParser *parser)
{
	g_return_val_if_fail (E_IS_MAIL_PARSER (parser), NULL);

	return parser->priv->session;
}

EMailExtensionRegistry *
e_mail_parser_get_extension_registry (EMailParser *parser)
{
	EMailParserClass *parser_class;

	g_return_val_if_fail (E_IS_MAIL_PARSER (parser), NULL);

	parser_class = E_MAIL_PARSER_GET_CLASS (parser);
	return E_MAIL_EXTENSION_REGISTRY (parser_class->extension_registry);
}
