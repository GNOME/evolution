/*
 * evolution-mdn.c
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

#include <config.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include <libebackend/e-extension.h>

#include <e-util/e-alert-dialog.h>
#include <e-util/e-account-utils.h>

#include <mail/em-utils.h>
#include <mail/e-mail-local.h>
#include <mail/e-mail-reader.h>
#include <mail/mail-send-recv.h>
#include <mail/em-composer-utils.h>
#include <mail/e-mail-folder-utils.h>

#define MDN_USER_FLAG "receipt-handled"

typedef EExtension EMdn;
typedef EExtensionClass EMdnClass;

typedef struct _MdnContext MdnContext;

struct _MdnContext {
	EAccount *account;
	EMailReader *reader;
	CamelFolder *folder;
	CamelMessageInfo *info;
	CamelMimeMessage *message;
	gchar *notify_to;
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
	if (context->info != NULL)
		camel_folder_free_message_info (
			context->folder, context->info);

	g_object_unref (context->account);
	g_object_unref (context->reader);
	g_object_unref (context->folder);
	g_object_unref (context->message);

	g_free (context->notify_to);

	g_slice_free (MdnContext, context);
}

static void
mdn_submit_alert (EMailReader *reader,
                  EAlert *alert)
{
	EPreviewPane *preview_pane;

	/* Make sure alerts are shown in the preview pane and not
	 * wherever e_mail_reader_get_alert_sink() might show it. */
	preview_pane = e_mail_reader_get_preview_pane (reader);
	e_alert_sink_submit_alert (E_ALERT_SINK (preview_pane), alert);
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
mdn_receipt_done (CamelFolder *folder,
                  GAsyncResult *result,
                  EMailBackend *backend)
{
	/* FIXME Poor error handling. */
	if (e_mail_folder_append_message_finish (folder, result, NULL, NULL))
		mail_send (backend);

	g_object_unref (backend);
}

static void
mdn_notify_sender (EAccount *account,
                   EMailReader *reader,
                   CamelFolder *folder,
                   CamelMimeMessage *message,
                   CamelMessageInfo *info,
                   const gchar *notify_to,
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
	CamelFolder *out_folder;
	CamelMessageInfo *receipt_info;
	EMailBackend *backend;
	const gchar *message_id;
	const gchar *message_date;
	const gchar *message_subject;
	gchar *fake_msgid;
	gchar *hostname;
	gchar *self_address;
	gchar *receipt_subject;
	gchar *transport_uid;
	gchar *disposition;
	gchar *recipient;
	gchar *content;
	gchar *ua;

	backend = e_mail_reader_get_backend (reader);

	/* Tag the message immediately even though we haven't actually sent
	 * the read receipt yet.  Not a big deal if we fail to send it, and
	 * we don't want to keep badgering the user about it. */
	camel_message_info_set_user_flag (info, MDN_USER_FLAG, TRUE);

	receipt = camel_mime_message_new ();
	body = camel_multipart_new ();

	medium = CAMEL_MEDIUM (message);
	message_id = camel_medium_get_header (medium, "Message-ID");
	message_date = camel_medium_get_header (medium, "Date");
	message_subject = camel_mime_message_get_subject (message);

	if (message_id == NULL)
		message_id = "";

	if (message_date == NULL)
		message_date = "";

	/* Collect information for the receipt. */

	/* We use camel_header_msgid_generate() to get a canonical
	 * hostname, then skip the part leading to '@' */
	fake_msgid = camel_header_msgid_generate ();
	hostname = strchr (fake_msgid, '@');
	hostname++;

	self_address = account->id->address;

	/* Create toplevel container. */
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
		_("Your message to %s about \"%s\" on %s has been read."),
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
		VERSION SUB_VERSION " " VERSION_COMMENT);
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
	g_free (fake_msgid);
	g_free (disposition);

	part = camel_mime_part_new ();
	camel_medium_set_content (CAMEL_MEDIUM (part), receipt_data);
	camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_7BIT);
	camel_multipart_add_part (body, part);
	g_object_unref (part);

	g_object_unref (receipt_data);

	/* Finish creating the message. */

	camel_medium_set_content (
		CAMEL_MEDIUM (receipt), CAMEL_DATA_WRAPPER (body));
	g_object_unref (body);

	receipt_subject = g_strdup_printf (
		/* Translators: %s is the subject of the email message. */
		_("Delivery Notification for \"%s\""), message_subject);
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

	transport_uid = g_strconcat (
		account->uid, "-transport", NULL);

	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"Return-Path", "<>");
	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"X-Evolution-Account",
		account->uid);
	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"X-Evolution-Transport",
		transport_uid);
	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"X-Evolution-Fcc",
		account->sent_folder_uri);

	/* RFC 3834, Section 5 describes this header. */
	camel_medium_set_header (
		CAMEL_MEDIUM (receipt),
		"Auto-Submitted", "auto-replied");

	g_free (transport_uid);

	/* Send the receipt. */
	receipt_info = camel_message_info_new (NULL);
	out_folder = e_mail_local_get_folder (E_MAIL_LOCAL_FOLDER_OUTBOX);
	camel_message_info_set_flags (
		receipt_info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);

	/* FIXME Pass a GCancellable. */
	e_mail_folder_append_message (
		out_folder, receipt, receipt_info, G_PRIORITY_DEFAULT,
		NULL, (GAsyncReadyCallback) mdn_receipt_done,
		g_object_ref (backend));

	camel_message_info_free (receipt_info);
}

static void
mdn_notify_action_cb (GtkAction *action,
                      MdnContext *context)
{
	mdn_notify_sender (
		context->account,
		context->reader,
		context->folder,
		context->message,
		context->info,
		context->notify_to,
		MDN_ACTION_MODE_MANUAL,
		MDN_SENDING_MODE_MANUAL);

	/* Make sure the newly-added user flag gets saved. */
	camel_folder_free_message_info (context->folder, context->info);
	context->info = NULL;
}

static void
mdn_message_loaded_cb (EMailReader *reader,
                       const gchar *message_uid,
                       CamelMimeMessage *message,
                       EMdn *extension)
{
	EAlert *alert;
	EAccount *account;
	CamelFolder *folder;
	CamelMessageInfo *info;
	gchar *notify_to = NULL;

	folder = e_mail_reader_get_folder (reader);

	info = camel_folder_get_message_info (folder, message_uid);
	if (info == NULL)
		return;

	if (camel_message_info_user_flag (info, MDN_USER_FLAG)) {
		alert = e_alert_new ("mdn:sender-notified", NULL);
		mdn_submit_alert (reader, alert);
		g_object_unref (alert);
		goto exit;
	}

	notify_to = mdn_get_notify_to (message);
	if (notify_to == NULL)
		goto exit;

	account = em_utils_guess_account_with_recipients (message, folder);
	if (account == NULL)
		goto exit;

	if (account->receipt_policy == E_ACCOUNT_RECEIPT_ASK) {
		MdnContext *context;
		GtkAction *action;
		gchar *tooltip;

		context = g_slice_new0 (MdnContext);
		context->account = g_object_ref (account);
		context->reader = g_object_ref (reader);
		context->folder = g_object_ref (folder);
		context->message = g_object_ref (message);
		context->info = camel_message_info_ref (info);

		context->notify_to = notify_to;
		notify_to = NULL;

		tooltip = g_strdup_printf (
			_("Send a read receipt to '%s'"),
			context->notify_to);

		action = gtk_action_new (
			"notify-sender",  /* name doesn't matter */
			_("_Notify Sender"),
			tooltip, NULL);

		g_signal_connect_data (
			action, "activate",
			G_CALLBACK (mdn_notify_action_cb),
			context,
			(GClosureNotify) mdn_context_free,
			(GConnectFlags) 0);

		alert = e_alert_new ("mdn:notify-sender", NULL);
		e_alert_add_action (alert, action, GTK_RESPONSE_APPLY);
		mdn_submit_alert (reader, alert);
		g_object_unref (alert);

		g_object_unref (action);
		g_free (tooltip);
	}

exit:
	camel_folder_free_message_info (folder, info);
	g_free (notify_to);
}

static void
mdn_message_seen_cb (EMailReader *reader,
                     const gchar *message_uid,
                     CamelMimeMessage *message,
                     EMdn *extension)
{
	EAccount *account;
	CamelFolder *folder;
	CamelMessageInfo *info;
	gchar *notify_to = NULL;

	folder = e_mail_reader_get_folder (reader);

	info = camel_folder_get_message_info (folder, message_uid);
	if (info == NULL)
		return;

	if (camel_message_info_user_flag (info, MDN_USER_FLAG))
		goto exit;

	notify_to = mdn_get_notify_to (message);
	if (notify_to == NULL)
		goto exit;

	account = em_utils_guess_account_with_recipients (message, folder);
	if (account == NULL)
		goto exit;

	if (account->receipt_policy == E_ACCOUNT_RECEIPT_ALWAYS)
		mdn_notify_sender (
			account, reader, folder,
			message, info, notify_to,
			MDN_ACTION_MODE_AUTOMATIC,
			MDN_SENDING_MODE_AUTOMATIC);

exit:
	camel_folder_free_message_info (folder, info);
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
		extensible, "message-loaded",
		G_CALLBACK (mdn_message_loaded_cb), extension);

	g_signal_connect (
		extensible, "message-seen",
		G_CALLBACK (mdn_message_seen_cb), extension);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mdn_parent_class)->constructed (object);
}

static void
e_mdn_class_init (EMdnClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mdn_constructed;

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

