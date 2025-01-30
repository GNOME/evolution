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
 */

#include "evolution-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>
#include <em-format/e-mail-part-utils.h>

#include <libebackend/libebackend.h>
#include <e-util/e-util.h>

#include "e-mail-parser-prefer-plain.h"

/* ------------------------------------------------------------------------ */

#define E_TYPE_NULL_REQUEST e_null_request_get_type ()
G_DECLARE_FINAL_TYPE (ENullRequest, e_null_request, E, NULL_REQUEST, GObject)

struct _ENullRequest {
	GObject parent;
};

static void e_null_request_content_request_init (EContentRequestInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ENullRequest, e_null_request, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_CONTENT_REQUEST, e_null_request_content_request_init))

static gboolean
e_null_request_can_process_uri (EContentRequest *request,
				const gchar *uri)
{
	return TRUE;
}

static gboolean
e_null_request_process_sync (EContentRequest *request,
			     const gchar *uri,
			     GObject *requester,
			     GInputStream **out_stream,
			     gint64 *out_stream_length,
			     gchar **out_mime_type,
			     GCancellable *cancellable,
			     GError **error)
{
	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "not supported");

	return FALSE;
}

static void
e_null_request_content_request_init (EContentRequestInterface *iface)
{
	iface->can_process_uri = e_null_request_can_process_uri;
	iface->process_sync = e_null_request_process_sync;
}

static void
e_null_request_class_init (ENullRequestClass *klass)
{
}

static void
e_null_request_init (ENullRequest *request)
{
}

static EContentRequest *
e_null_request_new (void)
{
	return g_object_new (E_TYPE_NULL_REQUEST, NULL);
}

/* ------------------------------------------------------------------------ */

typedef struct _EMailParserPreferPlain EMailParserPreferPlain;
typedef struct _EMailParserPreferPlainClass EMailParserPreferPlainClass;

struct _EMailParserPreferPlain {
	EMailParserExtension parent;

	GSettings *settings;
	gint mode;
	gboolean show_suppressed;
};

struct _EMailParserPreferPlainClass {
	EMailParserExtensionClass parent_class;
};

GType e_mail_parser_prefer_plain_get_type (void);

enum {
	PREFER_HTML,
	PREFER_PLAIN,
	PREFER_SOURCE,
	ONLY_PLAIN
};

G_DEFINE_DYNAMIC_TYPE (EMailParserPreferPlain, e_mail_parser_prefer_plain, E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"multipart/alternative",
	"text/html",
	NULL
};

static struct {
	const gchar *key;
	const gchar *label;
	const gchar *description;
} epp_options[] = {
	{ "normal",
	  N_("Show HTML if present"),
	  N_("Let Evolution choose the best part to show.") },

	{ "prefer_plain",
	  N_("Show plain text if present"),
	  N_("Show plain text part, if present, otherwise "
	     "let Evolution choose the best part to show.") },

	{ "prefer_source",
	  N_("Show plain text if present, or HTML source"),
	  N_("Show plain text part, if present, otherwise "
	     "the HTML part source.") },

	{ "only_plain",
	  N_("Only ever show plain text"),
	  N_("Always show plain text part and make attachments "
	     "from other parts, if requested.") },
};

enum {
	PROP_0,
	PROP_MODE,
	PROP_SHOW_SUPPRESSED
};

static void
mark_parts_not_printable (GQueue *parts)
{
	GList *link;

	for (link = g_queue_peek_head_link (parts); link; link = g_list_next (link)) {
		EMailPart *part = link->data;

		if (part)
			e_mail_part_set_is_printable (part, FALSE);
	}
}

static void
make_part_attachment (EMailParser *parser,
                      CamelMimePart *part,
                      GString *part_id,
                      gboolean force_html,
                      GCancellable *cancellable,
                      GQueue *out_mail_parts)
{
	CamelContentType *ct;

	ct = camel_mime_part_get_content_type (part);

	if (camel_content_type_is (ct, "text", "html")) {
		GQueue work_queue = G_QUEUE_INIT;
		EMailPart *mail_part;
		gboolean was_attachment;
		gint len;

		was_attachment = e_mail_part_is_attachment (part);

		/* always show HTML as attachments and not inline */
		camel_mime_part_set_disposition (part, "attachment");

		if (camel_mime_part_get_filename (part) == NULL) {
			gchar *filename;

			filename = g_strdup_printf ("%s.html", _("attachment"));
			camel_mime_part_set_filename (part, filename);
			g_free (filename);
		}

		len = part_id->len;
		g_string_append (part_id, ".text_html");
		mail_part = e_mail_part_new (part, part_id->str);
		e_mail_part_set_mime_type (mail_part, "text/html");
		g_string_truncate (part_id, len);

		g_queue_push_tail (&work_queue, mail_part);

		e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

		if (!was_attachment && !force_html)
			mark_parts_not_printable (&work_queue);

		e_queue_transfer (&work_queue, out_mail_parts);

	} else if (force_html && CAMEL_IS_MIME_MESSAGE (part)) {
		/* Note, the message was asked to be formatted as
		 * text/html; but the message may already be text/html. */
		CamelMimePart *new_part;
		CamelDataWrapper *content;

		content = camel_medium_get_content (CAMEL_MEDIUM (part));
		g_return_if_fail (content != NULL);

		new_part = camel_mime_part_new ();
		camel_medium_set_content (CAMEL_MEDIUM (new_part), content);

		e_mail_parser_parse_part (
			parser, new_part, part_id,
			cancellable, out_mail_parts);

		g_object_unref (new_part);
	} else {
		e_mail_parser_parse_part (
			parser, part, part_id, cancellable, out_mail_parts);
	}
}

static void
hide_parts (GQueue *work_queue)
{
	GList *head, *link;

	head = g_queue_peek_head_link (work_queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *mail_part = link->data;

		if (!e_mail_part_get_is_attachment (mail_part))
			mail_part->is_hidden = TRUE;
	}
}

static gchar *
mail_parser_prefer_plain_dup_part_text (CamelMimePart *mime_part,
				        GCancellable *cancellable)
{
	CamelDataWrapper *data_wrapper;
	CamelMedium *medium;
	CamelStream *stream;
	GByteArray *array;
	gchar *content;

	if (!mime_part)
		return NULL;

	array = g_byte_array_new ();
	medium = CAMEL_MEDIUM (mime_part);

	/* Stream takes ownership of the byte array. */
	stream = camel_stream_mem_new_with_byte_array (array);
	data_wrapper = camel_medium_get_content (medium);
	camel_data_wrapper_decode_to_stream_sync (
		data_wrapper, stream, NULL, NULL);

	content = g_strndup ((gchar *) array->data, array->len);

	g_object_unref (stream);

	return content;
}

typedef struct _AsyncContext {
	gchar *text_input;
	gchar *text_output;
	GCancellable *cancellable;
	EFlag *flag;
	WebKitWebView *web_view;
} AsyncContext;

static void
mail_parser_prefer_plain_convert_jsc_call_done_cb (GObject *source,
						   GAsyncResult *result,
						   gpointer user_data)
{
	JSCValue *value;
	AsyncContext *async_context = user_data;
	GError *error = NULL;

	g_return_if_fail (async_context != NULL);

	value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (source), result, &error);

	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		    (!g_error_matches (error, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED) ||
		     /* WebKit can return empty error message, thus ignore those. */
		     (error->message && *(error->message))))
			g_warning ("%s: JSC call failed: %s:%d: %s", G_STRFUNC, g_quark_to_string (error->domain), error->code, error->message);
		g_clear_error (&error);
	}

	if (value) {
		JSCException *exception;

		exception = jsc_context_get_exception (jsc_value_get_context (value));

		if (exception) {
			g_warning ("%s: JSC call failed: %s", G_STRFUNC, jsc_exception_get_message (exception));
			jsc_context_clear_exception (jsc_value_get_context (value));
		} else if (jsc_value_is_string (value)) {
			async_context->text_output = jsc_value_to_string (value);
		}

		g_clear_object (&value);
	}

	g_clear_object (&async_context->web_view);

	e_flag_set (async_context->flag);
}

static gboolean
mail_parser_prefer_plain_convert_text (gpointer user_data)
{
	AsyncContext *async_context = user_data;
	EContentRequest *content_request;
	EWebView *web_view;
	GSettings *settings;
	gchar *script;

	g_return_val_if_fail (async_context != NULL, FALSE);

	web_view = E_WEB_VIEW (g_object_ref_sink (e_web_view_new ()));
	async_context->web_view = WEBKIT_WEB_VIEW (web_view);

	/* Register schemes used by the EMailDisplay, but do not process them.
	   It avoids a runtime warning from the EWebView when it cannot find
	   the scheme handler. */
	content_request = e_null_request_new ();
	e_web_view_register_content_request_for_scheme (web_view, "evo-http", content_request);
	e_web_view_register_content_request_for_scheme (web_view, "evo-https", content_request);
	e_web_view_register_content_request_for_scheme (web_view, "mail", content_request);
	e_web_view_register_content_request_for_scheme (web_view, "cid", content_request);
	g_object_unref (content_request);

	e_web_view_load_uri (web_view, "evo://disable-remote-content");

	settings= e_util_ref_settings ("org.gnome.evolution.mail");

	script = e_web_view_jsc_printf_script (
		"var elem;\n"
		"elem = document.createElement('X-EVO-CONVERT');\n"
		"elem.innerHTML = %s;\n"
		"EvoConvert.ToPlainText(elem, -1, %d);",
		async_context->text_input,
		g_settings_get_enum (settings, "html-link-to-text"));

	g_object_unref (settings);

	webkit_web_view_evaluate_javascript (async_context->web_view, script, -1, NULL, NULL,
		async_context->cancellable, mail_parser_prefer_plain_convert_jsc_call_done_cb,
		async_context);

	g_free (script);

	return FALSE;
}

static gchar *
mail_parser_prefer_plain_convert_content_sync (CamelMimePart *mime_part,
					       GCancellable *cancellable)
{
	AsyncContext async_context;
	gchar *res = NULL;

	memset (&async_context, 0, sizeof (AsyncContext));

	async_context.text_input = mail_parser_prefer_plain_dup_part_text (mime_part, cancellable);

	if (!async_context.text_input || g_cancellable_is_cancelled (cancellable)) {
		g_free (async_context.text_input);
		return NULL;
	}

	async_context.flag = e_flag_new ();
	async_context.cancellable = cancellable;

	/* Run it in the main/GUI thread */
	g_timeout_add (1, mail_parser_prefer_plain_convert_text, &async_context);

	e_flag_wait (async_context.flag);
	e_flag_free (async_context.flag);

	if (async_context.text_output) {
		res = async_context.text_output;
		async_context.text_output = NULL;
	}

	g_free (async_context.text_input);
	g_free (async_context.text_output);

	return res;
}

static gboolean
empe_prefer_plain_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable,
                         GQueue *out_mail_parts)
{
	EMailParserPreferPlain *emp_pp;
	CamelMultipart *mp;
	gint i, nparts, partidlen;
	CamelContentType *ct;
	gboolean has_calendar = FALSE;
	gboolean has_html = FALSE;
	gboolean prefer_html;
	GQueue plain_text_parts = G_QUEUE_INIT;
	GQueue work_queue = G_QUEUE_INIT;
	GQueue attachments_queue = G_QUEUE_INIT;

	emp_pp = (EMailParserPreferPlain *) extension;
	prefer_html = (emp_pp->mode == PREFER_HTML);

	ct = camel_mime_part_get_content_type (part);

	/* We can actually parse HTML, but just discard it
	 * when "Only ever show plain text" mode is set. */
	if (camel_content_type_is (ct, "text", "html")) {

		/* Prevent recursion, fall back to next (real text/html) parser */
		if (strstr (part_id->str, ".alternative-prefer-plain.") != NULL)
			return FALSE;

		if (emp_pp->mode == PREFER_SOURCE && !e_mail_part_is_attachment (part)) {
			EMailPart *mail_part;

			partidlen = part_id->len;

			g_string_truncate (part_id, partidlen);
			g_string_append_printf (part_id, ".alternative-prefer-plain.%d", -1);

			mail_part = e_mail_part_new (part, part_id->str);
			e_mail_part_set_mime_type (mail_part, "application/vnd.evolution.plaintext");

			g_string_truncate (part_id, partidlen);

			g_queue_push_tail (out_mail_parts, mail_part);

			return TRUE;
		}

		/* Not enforcing text/plain, so use real parser. */
		if (emp_pp->mode != ONLY_PLAIN)
			return FALSE;

		/* The convert schedules a timeout GSource on the main thread and waits for an EFlag,
		   which causes a deadlock of the main thread. This can happen when opening a message
		   in the composer. Skip the plugin in this case. */
		if (e_util_is_main_thread (NULL))
			return FALSE;

		if (!e_mail_part_is_attachment (part)) {
			gchar *content;

			partidlen = part_id->len;

			g_string_truncate (part_id, partidlen);
			g_string_append_printf (part_id, ".alternative-prefer-plain.%d.converted", -1);

			content = mail_parser_prefer_plain_convert_content_sync (part, cancellable);

			if (content) {
				EMailPart *mail_part;
				CamelMimePart *text_part;

				text_part = camel_mime_part_new ();
				camel_mime_part_set_content (text_part, content, strlen (content), "application/vnd.evolution.plaintext");
				mail_part = e_mail_part_new (text_part, part_id->str);
				e_mail_part_set_mime_type (mail_part, "application/vnd.evolution.plaintext");
				g_free (content);

				g_queue_push_tail (out_mail_parts, mail_part);
			}

			g_string_truncate (part_id, partidlen);
		}

		if (emp_pp->show_suppressed || e_mail_part_is_attachment (part)) {
			/* Enforcing text/plain but got only HTML part, so add it
			 * as attachment to not show empty message preview, which
			 * is confusing. */
			make_part_attachment (
				parser, part, part_id, TRUE,
				cancellable, out_mail_parts);
		}

		return TRUE;
	}

	partidlen = part_id->len;

	mp = (CamelMultipart *) camel_medium_get_content (CAMEL_MEDIUM (part));

	if (!CAMEL_IS_MULTIPART (mp))
		return e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.source",
			cancellable, out_mail_parts);

	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		CamelMimePart *sp;

		sp = camel_multipart_get_part (mp, i);
		ct = camel_mime_part_get_content_type (sp);

		g_string_truncate (part_id, partidlen);
		g_string_append_printf (part_id, ".alternative-prefer-plain.%d", i);

		if (camel_content_type_is (ct, "text", "html")) {
			if (prefer_html) {
				e_mail_parser_parse_part (
					parser, sp, part_id,
					cancellable, &work_queue);
			} else if (emp_pp->show_suppressed) {
				make_part_attachment (
					parser, sp, part_id, FALSE,
					cancellable, &work_queue);
			}

			has_html = TRUE;

		} else if (camel_content_type_is (ct, "text", "plain")) {
			e_mail_parser_parse_part (
				parser, sp, part_id,
				cancellable, &plain_text_parts);

		/* Always show calendar part! */
		} else if (camel_content_type_is (ct, "text", "calendar") ||
			   camel_content_type_is (ct, "text", "x-calendar")) {

			/* Hide everything else, displaying
			 * native calendar part only. */
			hide_parts (&work_queue);

			e_mail_parser_parse_part (
				parser, sp, part_id, cancellable, &work_queue);

			has_calendar = TRUE;

		/* Multiparts can represent a text/html message
		 * with other things like embedded images, etc. */
		} else if (camel_content_type_is (ct, "multipart", "*")) {
			EMailPart *html_mail_part = NULL;
			GQueue inner_queue = G_QUEUE_INIT;
			GList *head, *link;

			e_mail_parser_parse_part (
				parser, sp, part_id, cancellable, &inner_queue);

			head = g_queue_peek_head_link (&inner_queue);

			/* Check whether the multipart contains a text/html part */
			for (link = head; link != NULL; link = g_list_next (link)) {
				EMailPart *mail_part = link->data;

				if (e_mail_part_id_has_substr (mail_part, ".text_html") ||
				    /* The HTML part as an HTML source code */
				    (emp_pp->mode == PREFER_SOURCE && e_mail_part_id_has_suffix (mail_part, ".alternative-prefer-plain.-1")) ||
				    /* The HTML part converted into text/plain */
				    (emp_pp->mode == ONLY_PLAIN && e_mail_part_id_has_suffix (mail_part, ".alternative-prefer-plain.-1.converted"))) {
					html_mail_part = mail_part;
					break;
				}
			}

			if (html_mail_part && !prefer_html) {
				if (emp_pp->show_suppressed) {
					GQueue suppressed_queue = G_QUEUE_INIT;
					CamelMimePart *inner_part;

					html_mail_part->is_hidden = TRUE;
					inner_part = e_mail_part_ref_mime_part (html_mail_part);

					if (inner_part) {
						e_mail_parser_wrap_as_attachment (parser, inner_part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &suppressed_queue);

						mark_parts_not_printable (&suppressed_queue);

						g_clear_object (&inner_part);
					}

					e_queue_transfer (&suppressed_queue, &inner_queue);
				} else {
					hide_parts (&inner_queue);
				}
			}

			e_queue_transfer (&inner_queue, &work_queue);

			has_html |= html_mail_part != NULL;

		/* Parse other than 'X' (those are custom types) as an attachment */
		} else if (ct && ct->subtype && ct->subtype[0] && ct->subtype[0] != 'x' && ct->subtype[0] != 'X') {
			e_mail_parser_parse_part (
				parser, sp, part_id,
				cancellable, &attachments_queue);
			e_mail_parser_wrap_as_attachment (parser, sp, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &attachments_queue);
		}
	}

	/* Don't hide the plain text if there's nothing else to display */
	if (has_calendar || (has_html && prefer_html))
		hide_parts (&plain_text_parts);

	if (!g_queue_is_empty (&plain_text_parts) && !g_queue_is_empty (&work_queue) && has_html) {
		/* a text/html part is hidden, but not marked as attachment,
		 * thus do that now, when there exists a text/plain part */
		GList *qiter;

		for (qiter = g_queue_peek_head_link (&work_queue); qiter; qiter = g_list_next (qiter)) {
			EMailPart *mpart = qiter->data;
			const gchar *mime_type;

			mime_type = e_mail_part_get_mime_type (mpart);

			if (mpart && mpart->is_hidden && g_strcmp0 (mime_type, "text/html") == 0) {
				e_mail_part_set_is_attachment (mpart, TRUE);
				e_mail_part_set_is_printable (mpart, FALSE);
			}
		}
	}

	/* plain_text parts should be always first */
	e_queue_transfer (&plain_text_parts, out_mail_parts);
	e_queue_transfer (&work_queue, out_mail_parts);
	e_queue_transfer (&attachments_queue, out_mail_parts);

	g_string_truncate (part_id, partidlen);

	return TRUE;
}

static void
e_mail_parser_prefer_plain_get_property (GObject *object,
                                         guint property_id,
                                         GValue *value,
                                         GParamSpec *pspec)
{
	EMailParserPreferPlain *parser;

	parser = (EMailParserPreferPlain *) object;

	switch (property_id) {
		case PROP_MODE:
			g_value_set_int (value, parser->mode);
			return;
		case PROP_SHOW_SUPPRESSED:
			g_value_set_boolean (value, parser->show_suppressed);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_parser_prefer_plain_set_property (GObject *object,
                                         guint property_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
	EMailParserPreferPlain *parser;

	parser = (EMailParserPreferPlain *) object;

	switch (property_id) {
		case PROP_MODE:
			parser->mode = g_value_get_int (value);
			return;
		case PROP_SHOW_SUPPRESSED:
			parser->show_suppressed = g_value_get_boolean (value);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_mail_parser_prefer_plain_dispose (GObject *object)
{
	EMailParserPreferPlain *parser;

	parser = (EMailParserPreferPlain *) object;

	g_clear_object (&parser->settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_parser_prefer_plain_parent_class)->
		dispose (object);
}

static void
e_mail_parser_prefer_plain_class_init (EMailParserPreferPlainClass *class)
{
	GObjectClass *object_class;
	EMailParserExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = e_mail_parser_prefer_plain_get_property;
	object_class->set_property = e_mail_parser_prefer_plain_set_property;
	object_class->dispose = e_mail_parser_prefer_plain_dispose;

	extension_class = E_MAIL_PARSER_EXTENSION_CLASS (class);
	extension_class->mime_types = parser_mime_types;
	extension_class->parse = empe_prefer_plain_parse;

	g_object_class_install_property (
		object_class,
		PROP_MODE,
		g_param_spec_int (
			"mode",
			"Mode",
			NULL,
			PREFER_HTML,
			ONLY_PLAIN,
			PREFER_HTML,
			G_PARAM_READABLE | G_PARAM_WRITABLE));

	g_object_class_install_property (
		object_class,
		PROP_SHOW_SUPPRESSED,
		g_param_spec_boolean (
			"show-suppressed",
			"Show Suppressed",
			NULL,
			FALSE,
			G_PARAM_READABLE | G_PARAM_WRITABLE));
}

void
e_mail_parser_prefer_plain_class_finalize (EMailParserPreferPlainClass *class)
{
}

static gboolean
parser_mode_get_mapping (GValue *value,
                         GVariant *variant,
                         gpointer user_data)
{
	gint i;

	const gchar *key = g_variant_get_string (variant, NULL);
	if (key) {
		for (i = 0; i < G_N_ELEMENTS (epp_options); i++) {
			if (!strcmp (epp_options[i].key, key)) {
				g_value_set_int (value, i);
				return TRUE;
			}
		}
	} else {
		g_value_set_int (value, 0);
	}

	return TRUE;
}

static GVariant *
parser_mode_set_mapping (const GValue *value,
                         const GVariantType *expected_type,
                         gpointer user_data)
{
	return g_variant_new_string (epp_options[g_value_get_int (value)].key);
}

static void
e_mail_parser_prefer_plain_init (EMailParserPreferPlain *parser)
{
	gchar *key;
	gint i;

	parser->settings = e_util_ref_settings ("org.gnome.evolution.plugin.prefer-plain");
	g_settings_bind_with_mapping (
		parser->settings, "mode",
		parser, "mode", G_SETTINGS_BIND_DEFAULT,
		parser_mode_get_mapping,
		parser_mode_set_mapping,
		NULL, NULL);
	g_settings_bind (
		parser->settings, "show-suppressed",
		parser, "show-suppressed", G_SETTINGS_BIND_DEFAULT);

	/* Initialize the settings */
	key = g_settings_get_string (parser->settings, "mode");
	if (key) {
		for (i = 0; i < G_N_ELEMENTS (epp_options); i++) {
			if (!strcmp (epp_options[i].key, key)) {
				parser->mode = i;
				break;
			}
		}
		g_free (key);
	} else {
		parser->mode = 0;
	}

	parser->show_suppressed = g_settings_get_boolean (parser->settings, "show-suppressed");
}

void
e_mail_parser_prefer_plain_type_register (GTypeModule *type_module)
{
	e_mail_parser_prefer_plain_register_type (type_module);
}
