/*
 * evolution-mdn.c
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

#include <string.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <mail/em-utils.h>
#include <mail/e-mail-reader.h>
#include <mail/mail-send-recv.h>
#include <mail/message-list.h>
#include <mail/em-composer-utils.h>

#define MDN_USER_FLAG "receipt-handled"

#define E_TYPE_MDN (e_mdn_get_type ())
#define E_MDN(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_MDN, EMdn))
#define E_IS_MDN(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_MDN))

typedef struct _EMdn EMdn;
typedef struct _EMdnClass EMdnClass;

struct _EMdn {
	EExtension parent;
	gpointer alert;  /* weak pointer */
};

struct _EMdnClass {
	EExtensionClass parent_class;
};

typedef struct _MdnContext MdnContext;

struct _MdnContext {
	ESource *source;
	EMailReader *reader;
	CamelFolder *folder;
	CamelMessageInfo *info;
	CamelMimeMessage *message;
	gchar *notify_to;
	gchar *identity_address;
};

typedef enum {
	MDN_ACTION_MODE_MANUAL,
	MDN_ACTION_MODE_AUTOMATIC
} MdnActionMode;

typedef enum {
	MDN_SENDING_MODE_MANUAL,
	MDN_SENDING_MODE_AUTOMATIC
} MdnSendingMode;

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Forward Declarations */
GType e_mdn_get_type (void);

G_DEFINE_DYNAMIC_TYPE (EMdn, e_mdn, E_TYPE_EXTENSION)

static void
mdn_context_free (MdnContext *context)
{
	g_clear_object (&context->info);
	g_object_unref (context->source);
	g_object_unref (context->reader);
	g_object_unref (context->folder);
	g_object_unref (context->message);

	g_free (context->notify_to);
	g_free (context->identity_address);

	g_slice_free (MdnContext, context);
}

static void
mdn_remove_alert (EMdn *mdn)
{
	g_return_if_fail (E_IS_MDN (mdn));

	if (mdn->alert != NULL)
		e_alert_response (mdn->alert, GTK_RESPONSE_OK);
}

static void
mdn_submit_alert (EMdn *mdn,
                  EMailReader *reader,
                  EAlert *alert)
{
	EPreviewPane *preview_pane;

	g_return_if_fail (E_IS_MDN (mdn));

	mdn_remove_alert (mdn);

	g_return_if_fail (mdn->alert == NULL);

	/* Make sure alerts are shown in the preview pane and not
	 * wherever e_mail_reader_get_alert_sink() might show it. */
	preview_pane = e_mail_reader_get_preview_pane (reader);
	e_alert_sink_submit_alert (E_ALERT_SINK (preview_pane), alert);

	mdn->alert = alert;
	g_object_add_weak_pointer (G_OBJECT (mdn->alert), &mdn->alert);
}

static gchar *
mdn_get_notify_to (CamelMimeMessage *message)
{
	CamelMedium *medium;
	const gchar *address;
	const gchar *header_name;

	medium = CAMEL_MEDIUM (message);
	header_name = "Disposition-Notification-To";
	address = camel_medium_get_header (medium, header_name);

	/* TODO Should probably decode/format the address,
	 *      since it could be in RFC 2047 format. */
	if (address != NULL)
		while (camel_mime_is_lwsp (*address))
			address++;

	return g_strdup (address);
}

static gchar *
mdn_get_disposition (MdnActionMode action_mode,
                     MdnSendingMode sending_mode)
{
	GString *string;

	string = g_string_sized_new (64);

	switch (action_mode) {
		case MDN_ACTION_MODE_MANUAL:
			g_string_append (string, "manual-action");
			break;
		case MDN_ACTION_MODE_AUTOMATIC:
			g_string_append (string, "automatic-action");
			break;
		default:
			g_warn_if_reached ();
	}

	g_string_append_c (string, '/');

	switch (sending_mode) {
		case MDN_SENDING_MODE_MANUAL:
			g_string_append (string, "MDN-sent-manually");
			break;
		case MDN_SENDING_MODE_AUTOMATIC:
			g_string_append (string, "MDN-sent-automatically");
			break;
		default:
			g_warn_if_reached ();
	}

	g_string_append (string, ";displayed");

	return g_string_free (string, FALSE);
}

static void
mdn_receipt_done (EMailSession *session,
                  GAsyncResult *result,
                  gpointer user_data)
{
	GError *error = NULL;

	e_mail_session_append_to_local_folder_finish (
		session, result, NULL, &error);

	if (error == NULL) {
		mail_send (session);
	} else {
		/* FIXME Poor error handling. */
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static void
mdn_notify_sender (ESource *identity_source,
                   EMailReader *reader,
                   CamelFolder *folder,
                   CamelMimeMessage *message,
                   CamelMessageInfo *info,
                   const gchar *notify_to,
		   const gchar *identity_address,
                   MdnActionMode action_mode,
                   MdnSendingMode sending_mode)
{
	/* See RFC 3798 for a description of message receipts. */

	CamelMimeMessage *receipt;
	CamelMultipart *body;
	CamelMimePart *part;
	CamelMedium *medium;
	CamelDataWrapper *receipt_text, *receipt_data;
	CamelContentType *type;
	CamelInternetAddress *address;
	CamelStream *stream;
	CamelMessageInfo *receipt_info;
	EMailBackend *backend;
	EMailSession *session;
	ESourceExtension *extension;
	gchar *message_id;
	gchar *message_date;
	const gchar *message_subject;
	const gchar *extension_name;
	const gchar *transport_uid;
	const gchar *sent_folder_uri = NULL;
	const gchar *hostname;
	gchar *self_address;
	gchar *receipt_subject;
	gchar *disposition;
	gchar *recipient;
	gchar *content;
	gchar *ua;

	g_return_if_fail (identity_source != NULL);

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	/* Tag the message immediately even though we haven't actually sent
	 * the read receipt yet.  Not a big deal if we fail to send it, and
	 * we don't want to keep badgering the user about it. */
	camel_message_info_set_user_flag (info, MDN_USER_FLAG, TRUE);

	medium = CAMEL_MEDIUM (message);
	message_id = camel_header_unfold (camel_medium_get_header (medium, "Message-ID"));
	message_date = camel_header_unfold (camel_medium_get_header (medium, "Date"));
	message_subject = camel_mime_message_get_subject (message);

	if (message_id == NULL)
		message_id = g_strdup ("");

	if (message_date == NULL)
		message_date = g_strdup ("");

	/* Collect information for the receipt. */

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	extension = e_source_get_extension (identity_source, extension_name);

	if (identity_address && *identity_address)
		self_address = g_strdup (identity_address);
	else
		self_address = e_source_mail_identity_dup_address (E_SOURCE_MAIL_IDENTITY (extension));

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	extension = e_source_get_extension (identity_source, extension_name);

	if (e_source_mail_submission_get_use_sent_folder (E_SOURCE_MAIL_SUBMISSION (extension)))
		sent_folder_uri = e_source_mail_submission_get_sent_folder (E_SOURCE_MAIL_SUBMISSION (extension));

	transport_uid = e_source_mail_submission_get_transport_uid (
		E_SOURCE_MAIL_SUBMISSION (extension));

	hostname = self_address ? strchr (self_address, '@') : NULL;
	if (hostname)
		hostname++;
	else
		hostname = "localhost";

	/* Create toplevel container. */
	body = camel_multipart_new ();
	camel_data_wrapper_set_mime_type (
		CAMEL_DATA_WRAPPER (body),
		"multipart/report;"
		"report-type=\"disposition-notification\"");
	camel_multipart_set_boundary (body, NULL);

	/* Create textual receipt. */

	receipt_text = camel_data_wrapper_new ();

	type = camel_content_type_new ("text", "plain");
	camel_content_type_set_param (type, "format", "flowed");
	camel_content_type_set_param (type, "charset", "UTF-8");
	camel_data_wrapper_set_mime_type_field (receipt_text, type);
	camel_content_type_unref (type);

	content = g_strdup_printf (
		/* Translators: First %s is an email address, second %s
		 * is the subject of the email, third %s is the date. */
		_("Your message to %s about “%s” on %s has been displayed. This is no guarantee that the message has been read or understood."),
		self_address, message_subject, message_date);
	stream = camel_stream_mem_new ();
	camel_stream_write_string (stream, content, NULL, NULL);
	camel_data_wrapper_construct_from_stream_sync (
		receipt_text, stream, NULL, NULL);
	g_object_unref (stream);
	g_free (content);

	part = camel_mime_part_new ();
	camel_medium_set_content (CAMEL_MEDIUM (part), receipt_text);
	camel_mime_part_set_encoding (
		part, CAMEL_TRANSFER_ENCODING_QUOTEDPRINTABLE);
	camel_multipart_add_part (body, part);
	g_object_unref (part);

	g_object_unref (receipt_text);

	/* Create the machine-readable receipt. */

	receipt_data = camel_data_wrapper_new ();

	ua = g_strdup_printf (
		"%s; %s", hostname, "Evolution "
		VERSION VERSION_SUBSTRING " " VERSION_COMMENT);
	recipient = g_strdup_printf ("rfc822; %s", self_address);
	disposition = mdn_get_disposition (action_mode, sending_mode);

	type = camel_content_type_new ("message", "disposition-notification");
	camel_data_wrapper_set_mime_type_field (receipt_data, type);
	camel_content_type_unref (type);

	content = g_strdup_printf (
		"Reporting-UA: %s\n"
		"Final-Recipient: %s\n"
		"Original-Message-ID: %s\n"
		"Disposition: %s\n",
		ua, recipient, message_id, disposition);
	stream = camel_stream_mem_new ();
	camel_stream_write_string (stream, content, NULL, NULL);
	camel_data_wrapper_construct_from_stream_sync (
		receipt_data, stream, NULL, NULL);
	g_object_unref (stream);
	g_free (content);

	g_free (ua);
	g_free (recipient);
	g_free (disposition);

	part = camel_mime_part_new ();
	camel_medium_set_content (CAMEL_MEDIUM (part), receipt_data);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_7BIT);
	camel_multipart_add_part (body, part);
	g_object_unref (part);

	g_object_unref (receipt_data);

	/* Finish creating the message. */

	receipt = camel_mime_message_new ();
	camel_medium_set_content (
		CAMEL_MEDIUM (receipt), CAMEL_DATA_WRAPPER (body));
	g_object_unref (body);

	receipt_subject = g_strdup_printf (
		/* Translators: %s is the subject of the email message This text is used as
		   a subject of a delivery notification email (basically a notification, that
		   the user did read (or better displayed) the message). */
		_("Read: %s"), message_subject);
	camel_mime_message_set_subject (receipt, receipt_subject);
	g_free (receipt_subject);

	address = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (address), self_address);
	camel_mime_message_set_from (receipt, address);
	g_object_unref (address);

	address = camel_internet_address_new ();
	camel_address_decode (CAMEL_ADDRESS (address), notify_to);
	camel_mime_message_set_recipients (
		receipt, CAMEL_RECIPIENT_TYPE_TO, address);
	g_object_unref (address);

	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"Return-Path", "<>");
	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"X-Evolution-Identity",
		e_source_get_uid (identity_source));
	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"X-Evolution-Transport",
		transport_uid);
	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"X-Evolution-Fcc",
		sent_folder_uri);

	/* RFC 3834, Section 5 describes this header. */
	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"Auto-Submitted", "auto-replied");

	/* Send the receipt. */
	receipt_info = camel_message_info_new (NULL);
	camel_message_info_set_flags (
		receipt_info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);

	/* FIXME Pass a GCancellable. */
	e_mail_session_append_to_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX,
		receipt, receipt_info, G_PRIORITY_DEFAULT,
		NULL, (GAsyncReadyCallback) mdn_receipt_done,
		g_object_ref (session));

	g_clear_object (&receipt_info);

	g_free (self_address);
	g_free (message_date);
	g_free (message_id);
}

static void
mdn_notify_action_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	MdnContext *context = user_data;

	mdn_notify_sender (
		context->source,
		context->reader,
		context->folder,
		context->message,
		context->info,
		context->notify_to,
		context->identity_address,
		MDN_ACTION_MODE_MANUAL,
		MDN_SENDING_MODE_MANUAL);

	/* Make sure the newly-added user flag gets saved. */
	g_clear_object (&context->info);
}

static void
mdn_mail_reader_changed_cb (EMailReader *reader,
                            EMdn *mdn)
{
	MessageList *message_list;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));

	if (!message_list || message_list_selected_count (message_list) != 1)
		mdn_remove_alert (mdn);
}

static void
mdn_message_loaded_cb (EMailReader *reader,
                       const gchar *message_uid,
                       CamelMimeMessage *message,
                       EMdn *mdn)
{
	EAlert *alert;
	ESource *source;
	ESourceMDN *extension;
	ESourceRegistry *registry;
	EMailBackend *backend;
	EMailSession *session;
	CamelFolder *folder;
	CamelMessageInfo *info;
	EMdnResponsePolicy response_policy;
	const gchar *extension_name;
	gchar *notify_to = NULL, *identity_address = NULL;

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);
	registry = e_mail_session_get_registry (session);

	folder = e_mail_reader_ref_folder (reader);

	mdn_remove_alert (mdn);

	info = camel_folder_get_message_info (folder, message_uid);
	if (info == NULL)
		goto exit;

	if (camel_message_info_get_user_flag (info, MDN_USER_FLAG))
		goto exit;

	notify_to = mdn_get_notify_to (message);
	if (notify_to == NULL)
		goto exit;

	/* Do not show the notice in special folders. */

	if (em_utils_folder_is_drafts (registry, folder))
		goto exit;

	if (em_utils_folder_is_templates (registry, folder))
		goto exit;

	if (em_utils_folder_is_sent (registry, folder))
		goto exit;

	if (em_utils_folder_is_outbox (registry, folder))
		goto exit;

	/* This returns a new ESource reference. */
	source = em_utils_guess_mail_identity_with_recipients (registry, message, folder, message_uid, NULL, &identity_address);
	if (source == NULL)
		goto exit;

	extension_name = E_SOURCE_EXTENSION_MDN;
	extension = e_source_get_extension (source, extension_name);
	response_policy = e_source_mdn_get_response_policy (extension);

	if (response_policy == E_MDN_RESPONSE_POLICY_ASK) {
		MdnContext *context;
		EUIAction *action;
		gchar *tooltip;

		context = g_slice_new0 (MdnContext);
		context->source = g_object_ref (source);
		context->reader = g_object_ref (reader);
		context->folder = g_object_ref (folder);
		context->message = g_object_ref (message);
		context->info = g_object_ref (info);
		context->notify_to = notify_to;
		context->identity_address = identity_address;

		notify_to = NULL;
		identity_address = NULL;

		tooltip = g_strdup_printf (
			_("Send a read receipt to “%s”"),
			context->notify_to);

		action = e_ui_action_new ("mdn-map", "notify-sender", NULL);
		e_ui_action_set_label (action, _("_Notify Sender"));
		e_ui_action_set_tooltip (action, tooltip);

		g_signal_connect_data (
			action, "activate",
			G_CALLBACK (mdn_notify_action_cb),
			context,
			(GClosureNotify) mdn_context_free,
			(GConnectFlags) 0);

		alert = e_alert_new ("mdn:notify-sender", NULL);
		e_alert_add_action (alert, action, GTK_RESPONSE_APPLY, FALSE);
		mdn_submit_alert (mdn, reader, alert);
		g_object_unref (alert);

		g_object_unref (action);
		g_free (tooltip);
	}

	g_object_unref (source);

exit:
	g_clear_object (&info);
	g_clear_object (&folder);
	g_free (identity_address);
	g_free (notify_to);
}

static void
mdn_message_seen_cb (EMailReader *reader,
                     const gchar *message_uid,
                     CamelMimeMessage *message)
{
	ESource *source;
	ESourceMDN *extension;
	ESourceRegistry *registry;
	EMailBackend *backend;
	EMailSession *session;
	CamelFolder *folder;
	CamelMessageInfo *info;
	EMdnResponsePolicy response_policy;
	const gchar *extension_name;
	gchar *notify_to = NULL, *identity_address = NULL;

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);
	registry = e_mail_session_get_registry (session);

	folder = e_mail_reader_ref_folder (reader);

	info = camel_folder_get_message_info (folder, message_uid);
	if (info == NULL)
		goto exit;

	if (camel_message_info_get_user_flag (info, MDN_USER_FLAG))
		goto exit;

	notify_to = mdn_get_notify_to (message);
	if (notify_to == NULL)
		goto exit;

	/* This returns a new ESource reference. */
	source = em_utils_guess_mail_identity_with_recipients (
		registry, message, folder, message_uid, NULL, &identity_address);
	if (source == NULL)
		goto exit;

	extension_name = E_SOURCE_EXTENSION_MDN;
	extension = e_source_get_extension (source, extension_name);
	response_policy = e_source_mdn_get_response_policy (extension);

	if (response_policy == E_MDN_RESPONSE_POLICY_ALWAYS)
		mdn_notify_sender (
			source, reader, folder,
			message, info, notify_to,
			identity_address,
			MDN_ACTION_MODE_AUTOMATIC,
			MDN_SENDING_MODE_AUTOMATIC);

	g_object_unref (source);

exit:
	g_clear_object (&info);
	g_clear_object (&folder);
	g_free (identity_address);
	g_free (notify_to);
}

static void
mdn_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);
	g_return_if_fail (E_IS_MAIL_READER (extensible));

	g_signal_connect (
		extensible, "changed",
		G_CALLBACK (mdn_mail_reader_changed_cb), extension);

	g_signal_connect (
		extensible, "message-loaded",
		G_CALLBACK (mdn_message_loaded_cb), extension);

	g_signal_connect (
		extensible, "message-seen",
		G_CALLBACK (mdn_message_seen_cb), NULL);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mdn_parent_class)->constructed (object);
}

static void
mdn_dispose (GObject *object)
{
	mdn_remove_alert (E_MDN (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mdn_parent_class)->dispose (object);
}

static void
e_mdn_class_init (EMdnClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mdn_constructed;
	object_class->dispose = mdn_dispose;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_READER;
}

static void
e_mdn_class_finalize (EMdnClass *class)
{
}

static void
e_mdn_init (EMdn *extension)
{
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_mdn_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}

