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
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <locale.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <e-util/e-util.h>

#include <libemail-engine/libemail-engine.h>

#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-part-utils.h>
#include <em-format/e-mail-formatter-quote.h>

#include <shell/e-shell.h>

#include <composer/e-msg-composer.h>
#include <composer/e-composer-actions.h>
#include <composer/e-composer-post-header.h>

#include "e-mail-printer.h"
#include "e-mail-ui-session.h"
#include "e-mail-templates.h"
#include "e-mail-templates-store.h"
#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-folder-selector.h"
#include "em-folder-tree.h"
#include "em-event.h"
#include "mail-send-recv.h"

#ifdef G_OS_WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif
#ifdef localtime_r
#undef localtime_r
#endif

/* The gmtime() and localtime() in Microsoft's C library are MT-safe */
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#define localtime_r(tp,tmp) (localtime(tp)?(*(tmp)=*localtime(tp),(tmp)):0)
#endif

typedef struct _AsyncContext AsyncContext;
typedef struct _ForwardData ForwardData;

struct _AsyncContext {
	CamelMimeMessage *message;
	EMailSession *session;
	EMsgComposer *composer;
	ESource *transport_source;
	EActivity *activity;
	gchar *folder_uri;
	gchar *message_uid;
	gulong num_loading_handler_id;
	gulong cancelled_handler_id;
};

struct _ForwardData {
	EShell *shell;
	CamelFolder *folder;
	GPtrArray *uids;
	EMailForwardStyle style;
};

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->cancelled_handler_id) {
		GCancellable *cancellable;

		cancellable = e_activity_get_cancellable (async_context->activity);
		/* Cannot use g_cancellable_disconnect(), because when this is called
		   from inside the cancelled handler, then the GCancellable deadlocks. */
		g_signal_handler_disconnect (cancellable, async_context->cancelled_handler_id);
		async_context->cancelled_handler_id = 0;
	}

	if (async_context->num_loading_handler_id) {
		EAttachmentView *view;
		EAttachmentStore *store;

		view = e_msg_composer_get_attachment_view (async_context->composer);
		store = e_attachment_view_get_store (view);

		e_signal_disconnect_notify_handler (store, &async_context->num_loading_handler_id);
	}

	g_clear_object (&async_context->message);
	g_clear_object (&async_context->session);
	g_clear_object (&async_context->composer);
	g_clear_object (&async_context->transport_source);
	g_clear_object (&async_context->activity);

	g_free (async_context->folder_uri);
	g_free (async_context->message_uid);

	g_slice_free (AsyncContext, async_context);
}

static void
forward_data_free (ForwardData *data)
{
	if (data->shell != NULL)
		g_object_unref (data->shell);

	if (data->folder != NULL)
		g_object_unref (data->folder);

	if (data->uids != NULL)
		g_ptr_array_unref (data->uids);

	g_slice_free (ForwardData, data);
}

static gboolean
check_destination_accepts_html (EDestination *dest,
				/* const */ gchar **accepts_html)
{
	const gchar *email;
	guint ii;

	if (!dest)
		return FALSE;
	if (!accepts_html)
		return FALSE;

	email = e_destination_get_email (dest);
	if (!email || !*email)
		return FALSE;

	for (ii = 0; accepts_html[ii]; ii++) {
		if (camel_strstrcase (email, accepts_html[ii]))
			return TRUE;
	}

	return FALSE;
}

static gboolean
ask_confirm_for_unwanted_html_mail (EMsgComposer *composer,
                                    EDestination **recipients,
				    /*const */ gchar **accepts_html)
{
	gboolean res;
	GString *str;
	gint i;

	str = g_string_new ("");
	for (i = 0; recipients[i] != NULL; ++i) {
		if (!e_destination_get_html_mail_pref (recipients[i]) &&
		    !check_destination_accepts_html (recipients[i], accepts_html)) {
			const gchar *name;

			name = e_destination_get_textrep (recipients[i], FALSE);

			g_string_append_printf (str, "     %s\n", name);
		}
	}

	if (str->len)
		res = e_util_prompt_user (
			GTK_WINDOW (composer),
			"org.gnome.evolution.mail", "prompt-on-unwanted-html",
			"mail:ask-send-html", str->str, NULL);
	else
		res = TRUE;

	g_string_free (str, TRUE);

	return res;
}

static gboolean
ask_confirm_for_empty_subject (EMsgComposer *composer)
{
	return e_util_prompt_user (
		GTK_WINDOW (composer),
		"org.gnome.evolution.mail",
		"prompt-on-empty-subject",
		"mail:ask-send-no-subject", NULL);
}

static gboolean
ask_confirm_for_only_bcc (EMsgComposer *composer,
                          gboolean hidden_list_case)
{
	/* If the user is mailing a hidden contact list, it is possible for
	 * them to create a message with only Bcc recipients without really
	 * realizing it.  To try to avoid being totally confusing, I've changed
	 * this dialog to provide slightly different text in that case, to
	 * better explain what the hell is going on. */

	return e_util_prompt_user (
		GTK_WINDOW (composer),
		"org.gnome.evolution.mail",
		"prompt-on-only-bcc",
		hidden_list_case ?
		"mail:ask-send-only-bcc-contact" :
		"mail:ask-send-only-bcc", NULL);
}

static gboolean
is_group_definition (const gchar *str)
{
	const gchar *colon;

	if (!str || !*str)
		return FALSE;

	colon = strchr (str, ':');
	return colon > str && strchr (str, ';') > colon;
}

static gboolean
composer_presend_check_recipients (EMsgComposer *composer,
                                   EMailSession *session)
{
	EDestination **recipients;
	EDestination **recipients_bcc;
	CamelInternetAddress *cia;
	EComposerHeaderTable *table;
	EComposerHeader *post_to_header;
	GString *invalid_addrs = NULL;
	GSettings *settings;
	gchar *to_cc_domain = NULL;
	gboolean check_passed = FALSE;
	gint hidden = 0;
	gint shown = 0;
	gint num = 0;
	gint num_to_cc = 0;
	gint num_to_cc_same_domain = 0;
	gint num_bcc = 0;
	gint num_post = 0;
	gint ii;

	/* We should do all of the validity checks based on the composer,
	 * and not on the created message, as extra interaction may occur
	 * when we get the message (e.g. passphrase to sign a message). */

	table = e_msg_composer_get_header_table (composer);

	recipients = e_composer_header_table_get_destinations_to (table);
	if (recipients) {
		for (ii = 0; recipients[ii] != NULL; ii++) {
			const gchar *addr;

			addr = e_destination_get_address (recipients[ii]);
			if (addr == NULL || *addr == '\0')
				continue;

			addr = e_destination_get_email (recipients[ii]);
			if (addr && *addr) {
				const gchar *at;

				at = strchr (addr, '@');

				if (!to_cc_domain && at) {
					to_cc_domain = g_strdup (at);
					num_to_cc_same_domain = 1;
				} else if (to_cc_domain && at && g_ascii_strcasecmp (to_cc_domain, at) == 0) {
					num_to_cc_same_domain++;
				}
			}

			num_to_cc++;
		}

		e_destination_freev (recipients);
	}

	recipients = e_composer_header_table_get_destinations_cc (table);
	if (recipients) {
		for (ii = 0; recipients[ii] != NULL; ii++) {
			const gchar *addr;

			addr = e_destination_get_address (recipients[ii]);
			if (addr == NULL || *addr == '\0')
				continue;

			addr = e_destination_get_email (recipients[ii]);
			if (addr && *addr) {
				const gchar *at;

				at = strchr (addr, '@');

				if (!to_cc_domain && at) {
					to_cc_domain = g_strdup (at);
					num_to_cc_same_domain = 1;
				} else if (to_cc_domain && at && g_ascii_strcasecmp (to_cc_domain, at) == 0) {
					num_to_cc_same_domain++;
				}
			}

			num_to_cc++;
		}

		e_destination_freev (recipients);
	}

	recipients = e_composer_header_table_get_destinations (table);

	cia = camel_internet_address_new ();

	/* See which ones are visible, present, etc. */
	for (ii = 0; recipients != NULL && recipients[ii] != NULL; ii++) {
		const gchar *addr;
		gint len, j;

		addr = e_destination_get_address (recipients[ii]);
		if (addr == NULL || *addr == '\0')
			continue;

		camel_address_decode (CAMEL_ADDRESS (cia), addr);
		len = camel_address_length (CAMEL_ADDRESS (cia));

		if (len > 0) {
			if (!e_destination_is_evolution_list (recipients[ii])) {
				for (j = 0; j < len; j++) {
					const gchar *name = NULL, *eml = NULL;

					if (!camel_internet_address_get (cia, j, &name, &eml) ||
					    !eml ||
					    strchr (eml, '@') <= eml) {
						if (!invalid_addrs)
							invalid_addrs = g_string_new ("");
						else
							g_string_append (invalid_addrs, ", ");

						if (name)
							g_string_append (invalid_addrs, name);
						if (eml) {
							g_string_append (invalid_addrs, name ? " <" : "");
							g_string_append (invalid_addrs, eml);
							g_string_append (invalid_addrs, name ? ">" : "");
						}
					}
				}
			}

			camel_address_remove (CAMEL_ADDRESS (cia), -1);
			num++;
			if (e_destination_is_evolution_list (recipients[ii])
			    && !e_destination_list_show_addresses (recipients[ii])) {
				hidden++;
			} else {
				shown++;
			}
		} else if (is_group_definition (addr)) {
			/* like an address, it will not claim on only-bcc */
			shown++;
			num++;
		} else if (!invalid_addrs) {
			invalid_addrs = g_string_new (addr);
		} else {
			g_string_append (invalid_addrs, ", ");
			g_string_append (invalid_addrs, addr);
		}
	}

	recipients_bcc = e_composer_header_table_get_destinations_bcc (table);
	if (recipients_bcc) {
		for (ii = 0; recipients_bcc[ii] != NULL; ii++) {
			const gchar *addr;

			addr = e_destination_get_address (recipients_bcc[ii]);
			if (addr == NULL || *addr == '\0')
				continue;

			camel_address_decode (CAMEL_ADDRESS (cia), addr);
			if (camel_address_length (CAMEL_ADDRESS (cia)) > 0) {
				camel_address_remove (CAMEL_ADDRESS (cia), -1);
				num_bcc++;
			}
		}

		e_destination_freev (recipients_bcc);
	}

	g_object_unref (cia);

	post_to_header = e_composer_header_table_get_header (
		table, E_COMPOSER_HEADER_POST_TO);
	if (e_composer_header_get_visible (post_to_header)) {
		GList *postlist;

		postlist = e_composer_header_table_get_post_to (table);
		num_post = g_list_length (postlist);
		g_list_foreach (postlist, (GFunc) g_free, NULL);
		g_list_free (postlist);
	}

	/* I'm sensing a lack of love, er, I mean recipients. */
	if (num == 0 && num_post == 0) {
		EHTMLEditor *editor;

		editor = e_msg_composer_get_editor (composer);
		e_alert_submit (E_ALERT_SINK (editor), "mail:send-no-recipients", NULL);

		goto finished;
	}

	if (invalid_addrs) {
		if (!e_util_prompt_user (
			GTK_WINDOW (composer),
			"org.gnome.evolution.mail",
			"prompt-on-invalid-recip",
			strstr (invalid_addrs->str, ", ") ?
				"mail:ask-send-invalid-recip-multi" :
				"mail:ask-send-invalid-recip-one",
			invalid_addrs->str, NULL)) {
			g_string_free (invalid_addrs, TRUE);
			goto finished;
		}

		g_string_free (invalid_addrs, TRUE);
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (num_to_cc > 1 && num_to_cc != num_to_cc_same_domain &&
	    num_to_cc >= g_settings_get_int (settings, "composer-many-to-cc-recips-num")) {
		gchar *head;
		gchar *msg;

		g_clear_object (&settings);

		head = g_strdup_printf (ngettext (
			/* Translators: The %d is replaced with the actual count of recipients, which is always more than one. */
			"Are you sure you want to send a message with %d To and CC recipients?",
			"Are you sure you want to send a message with %d To and CC recipients?",
			num_to_cc), num_to_cc);

		msg = g_strdup_printf (ngettext (
			/* Translators: The %d is replaced with the actual count of recipients, which is always more than one. */
			"You are trying to send a message to %d recipients in To and CC fields."
			" This would result in all recipients seeing the email addresses of each"
			" other. In some cases this behaviour is undesired, especially if they"
			" do not know each other or if privacy is a concern. Consider adding"
			" recipients to the BCC field instead.",
			"You are trying to send a message to %d recipients in To and CC fields."
			" This would result in all recipients seeing the email addresses of each"
			" other. In some cases this behaviour is undesired, especially if they"
			" do not know each other or if privacy is a concern. Consider adding"
			" recipients to the BCC field instead.",
			num_to_cc), num_to_cc);

		if (!e_util_prompt_user (
			GTK_WINDOW (composer),
			"org.gnome.evolution.mail",
			"prompt-on-many-to-cc-recips",
			"mail:ask-many-to-cc-recips",
			head, msg, NULL)) {
			EUIAction *action;

			g_free (head);
			g_free (msg);

			action = E_COMPOSER_ACTION_VIEW_BCC (composer);
			e_ui_action_set_active (action, TRUE);

			goto finished;
		}

		g_free (head);
		g_free (msg);
	}
	g_clear_object (&settings);

	if (num > 0 && (num == num_bcc || shown == 0)) {
		/* this means that the only recipients are Bcc's */
		if (!ask_confirm_for_only_bcc (composer, shown == 0))
			goto finished;
	}

	check_passed = TRUE;

finished:
	g_free (to_cc_domain);

	if (recipients != NULL)
		e_destination_freev (recipients);

	return check_passed;
}

static gboolean
composer_presend_check_identity (EMsgComposer *composer,
                                 EMailSession *session)
{
	EComposerHeaderTable *table;
	ESource *source = NULL;
	gchar *uid;
	gboolean success = TRUE;

	table = e_msg_composer_get_header_table (composer);

	uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);
	if (uid)
		source = e_composer_header_table_ref_source (table, uid);
	g_free (uid);

	if (source) {
		EClientCache *client_cache;
		ESourceRegistry *registry;

		client_cache = e_composer_header_table_ref_client_cache (table);
		registry = e_client_cache_ref_registry (client_cache);

		success = e_source_registry_check_enabled (registry, source);
		if (!success) {
			e_alert_submit (
				E_ALERT_SINK (e_msg_composer_get_editor (composer)),
				"mail:send-no-account-enabled", NULL);
		}

		g_object_unref (client_cache);
		g_object_unref (registry);
	} else {
		success = FALSE;
		e_alert_submit (
			E_ALERT_SINK (e_msg_composer_get_editor (composer)),
			"mail:send-no-account", NULL);
	}


	g_clear_object (&source);

	return success;
}

static gboolean
composer_presend_check_plugins (EMsgComposer *composer,
                                EMailSession *session)
{
	EMEvent *eme;
	EMEventTargetComposer *target;
	gpointer data;

	/** @Event: composer.presendchecks
	 * @Title: Composer PreSend Checks
	 * @Target: EMEventTargetMessage
	 *
	 * composer.presendchecks is emitted during pre-checks for the
	 * message just before sending.  Since the e-plugin framework
	 * doesn't provide a way to return a value from the plugin,
	 * use 'presend_check_status' to set whether the check passed.
	 */
	eme = em_event_peek ();
	target = em_event_target_new_composer (eme, composer, 0);

	e_event_emit (
		(EEvent *) eme, "composer.presendchecks",
		(EEventTarget *) target);

	/* A non-NULL value for this key means the check failed. */
	data = g_object_get_data (G_OBJECT (composer), "presend_check_status");

	/* Clear the value in case we have to run these checks again. */
	g_object_set_data (G_OBJECT (composer), "presend_check_status", NULL);

	return (data == NULL);
}

static gboolean
composer_presend_check_subject (EMsgComposer *composer,
                                EMailSession *session)
{
	EComposerHeaderTable *table;
	const gchar *subject;
	gboolean check_passed = TRUE;

	table = e_msg_composer_get_header_table (composer);
	subject = e_composer_header_table_get_subject (table);

	if (subject == NULL || subject[0] == '\0') {
		if (!ask_confirm_for_empty_subject (composer))
			check_passed = FALSE;
	}

	return check_passed;
}

static gboolean
composer_presend_check_unwanted_html (EMsgComposer *composer,
                                      EMailSession *session)
{
	EDestination **recipients;
	EHTMLEditor *editor;
	EComposerHeaderTable *table;
	EContentEditorMode mode;
	GSettings *settings;
	gboolean check_passed = TRUE;
	gboolean html_mode;
	gboolean send_html;
	gboolean confirm_html;
	gint ii;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	editor = e_msg_composer_get_editor (composer);
	mode = e_html_editor_get_mode (editor);
	html_mode = mode == E_CONTENT_EDITOR_MODE_HTML || mode == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML;

	table = e_msg_composer_get_header_table (composer);
	recipients = e_composer_header_table_get_destinations (table);

	mode = g_settings_get_enum (settings, "composer-mode");
	send_html = mode == E_CONTENT_EDITOR_MODE_HTML || mode == E_CONTENT_EDITOR_MODE_MARKDOWN_HTML;
	confirm_html = g_settings_get_boolean (settings, "prompt-on-unwanted-html");

	/* Only show this warning if our default is to send html.  If it
	 * isn't, we've manually switched into html mode in the composer
	 * and (presumably) had a good reason for doing this. */
	if (html_mode && send_html && confirm_html && recipients != NULL) {
		gboolean html_problem = FALSE;
		gchar **accepts_html;

		accepts_html = g_settings_get_strv (settings, "composer-addresses-accept-html");

		for (ii = 0; recipients[ii] != NULL; ii++) {
			if (!e_destination_get_html_mail_pref (recipients[ii]) &&
			    !check_destination_accepts_html (recipients[ii], accepts_html)) {
				html_problem = TRUE;
				break;
			}
		}

		if (html_problem) {
			if (!ask_confirm_for_unwanted_html_mail (composer, recipients, accepts_html))
				check_passed = FALSE;
		}

		g_strfreev (accepts_html);
	}

	if (recipients != NULL)
		e_destination_freev (recipients);

	g_object_unref (settings);

	return check_passed;
}

static gboolean
composer_presend_check_attachments (EMsgComposer *composer,
				    EMailSession *session)
{
	EAttachmentView *view;
	EAttachmentStore *store;
	GList *attachments, *link;
	gboolean can_send = TRUE;
	EAttachment *first_changed = NULL;
	guint n_changed = 0;

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);
	attachments = e_attachment_store_get_attachments (store);

	for (link = attachments; link; link = g_list_next (link)) {
		EAttachment *attach = link->data;
		gboolean file_exists = FALSE;

		if (e_attachment_check_file_changed (attach, &file_exists, NULL) &&
		    file_exists) {
			e_attachment_set_may_reload (attach, TRUE);
			if (!first_changed)
				first_changed = attach;
			n_changed++;
		} else {
			e_attachment_set_may_reload (attach, FALSE);
		}
	}

	if (n_changed > 0) {
		GFileInfo *file_info = NULL;
		const gchar *display_name = NULL;
		gchar *title, *text;

		if (n_changed == 1) {
			file_info = e_attachment_ref_file_info (first_changed);
			display_name = g_file_info_get_display_name (file_info);
			if (display_name && !*display_name)
				display_name = NULL;
		}

		title = g_strdup_printf (ngettext (
			"Attachment changed",
			"%d attachments changed",
			n_changed), n_changed);

		if (n_changed == 1 && display_name) {
			text = g_strdup_printf (_("Attachment “%s” changed after being added to the message. Do you want to send the message anyway?"),
				display_name);
		} else {
			text = g_strdup_printf (ngettext ("One attachment changed after being added to the message. Do you want to send the message anyway?",
							  "%d attachments changed after being added to the message. Do you want to send the message anyway?",
				n_changed), n_changed);
		}

		can_send = e_util_prompt_user (
			GTK_WINDOW (composer),
			"org.gnome.evolution.mail",
			"prompt-on-changed-attachment",
			"mail:ask-composer-changed-attachment",
			title, text, NULL);

		g_clear_object (&file_info);
		g_free (title);
		g_free (text);
	}

	g_list_free_full (attachments, g_object_unref);

	return can_send;
}

static void
openpgp_changes_saved_cb (GObject *source_object,
			  GAsyncResult *result,
			  gpointer user_data)
{
	ESource *source = E_SOURCE (source_object);
	GError *local_error = NULL;

	if (!e_source_write_finish (source, result, &local_error))
		g_warning ("%s: Failed to save changes to '%s': %s", G_STRFUNC, e_source_get_uid (source), local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);
}

static gboolean
composer_presend_check_autocrypt_wanted (EMsgComposer *composer,
					 EMailSession *session)
{
	EComposerHeaderTable *table;
	const gchar *value;
	gchar *identity_uid;
	gboolean ask_send_public_key = TRUE;
	gboolean can_send = TRUE;

	table = e_msg_composer_get_header_table (composer);

	value = e_msg_composer_get_header (composer, "Autocrypt", 0);
	if (value && *value) {
		CamelHeaderParam *params, *param;
		gboolean removed = FALSE;

		params = em_utils_decode_autocrypt_header_value (value);
		if (params) {
			for (param = params; param; param = param->next) {
				if (!param->name || !param->value)
					continue;
				if (g_ascii_strcasecmp (param->name, "addr") == 0) {
					const gchar *from = e_composer_header_table_get_from_address (table);
					/* the user could change the From address in the "From Field Override",
					   the remove the header when the two do not match */
					if (!from || g_ascii_strcasecmp (from, param->value) != 0) {
						e_msg_composer_remove_header (composer, "Autocrypt");
						removed = TRUE;
					}
					break;
				}
			}

			camel_header_param_list_free (params);
		}

		if (removed)
			return TRUE;
	} else {
		return TRUE;
	}

	identity_uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);
	if (identity_uid) {
		ESource *source;

		source = e_composer_header_table_ref_source (table, identity_uid);

		if (source) {
			if (e_source_has_extension (source, E_SOURCE_EXTENSION_OPENPGP)) {
				ESourceOpenPGP *extension;

				extension = e_source_get_extension (source, E_SOURCE_EXTENSION_OPENPGP);

				ask_send_public_key = e_source_openpgp_get_ask_send_public_key (extension);
			}

			g_object_unref (source);
		}
	} else {
		ask_send_public_key = FALSE;
	}

	if (ask_send_public_key) {
		gint response;

		response = e_alert_run_dialog_for_args (GTK_WINDOW (composer), "mail:ask-composer-send-autocrypt", NULL);
		if (response == GTK_RESPONSE_YES) {
			can_send = TRUE;
		} else if (response == GTK_RESPONSE_NO) {
			e_msg_composer_remove_header (composer, "Autocrypt");
			can_send = TRUE;
		} else if (response == GTK_RESPONSE_ACCEPT ||
			   response == GTK_RESPONSE_REJECT) {
			ESource *source;

			source = e_composer_header_table_ref_source (table, identity_uid);
			if (source) {
				ESourceOpenPGP *extension;
				extension = e_source_get_extension (source, E_SOURCE_EXTENSION_OPENPGP);
				/* when disabling send of the public key, keep the ask set */
				e_source_openpgp_set_ask_send_public_key (extension, response == GTK_RESPONSE_REJECT);
				e_source_openpgp_set_send_public_key (extension, response == GTK_RESPONSE_ACCEPT);
				e_source_write (source, NULL, openpgp_changes_saved_cb, NULL);
				g_object_unref (source);
			} else {
				g_warn_if_reached ();
			}

			if (response != GTK_RESPONSE_ACCEPT)
				e_msg_composer_remove_header (composer, "Autocrypt");

			can_send = TRUE;
		} else {
			can_send = FALSE;
		}
	}

	g_free (identity_uid);

	return can_send;
}

static void
composer_send_completed (GObject *source_object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	EActivity *activity;
	const gchar *outbox_uid;
	gboolean service_unavailable;
	gboolean set_changed = FALSE;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	if (async_context->transport_source) {
		EShell *shell;

		shell = e_msg_composer_get_shell (async_context->composer);

		e_shell_set_auth_prompt_parent (shell, async_context->transport_source, NULL);
	}

	activity = async_context->activity;

	e_mail_session_send_to_finish (
		E_MAIL_SESSION (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		set_changed = TRUE;
		goto exit;
	}

	/* Check for error codes which may indicate we're offline
	 * or name resolution failed or connection attempt failed. */
	service_unavailable =
		g_error_matches (
			local_error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE) ||
		/* name resolution failed */
		g_error_matches (
			local_error, G_RESOLVER_ERROR,
			G_RESOLVER_ERROR_NOT_FOUND) ||
		g_error_matches (
			local_error, G_RESOLVER_ERROR,
			G_RESOLVER_ERROR_TEMPORARY_FAILURE) ||
		/* something internal to Camel failed */
		g_error_matches (
			local_error, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_URL_INVALID);
	if (service_unavailable) {
		/* Inform the user. */
		e_alert_run_dialog_for_args (
			GTK_WINDOW (async_context->composer),
			"mail-composer:saving-to-outbox", NULL);
		if (async_context->message)
			g_signal_emit_by_name (
				async_context->composer, "save-to-outbox",
				async_context->message, activity);
		else
			e_msg_composer_save_to_outbox (async_context->composer);
		goto exit;
	}

	/* Post-processing errors are shown in the shell window. */
	if (g_error_matches (
		local_error, E_MAIL_ERROR,
		E_MAIL_ERROR_POST_PROCESSING)) {
		EAlert *alert;
		EShell *shell;

		shell = e_msg_composer_get_shell (async_context->composer);

		alert = e_alert_new (
			"mail-composer:send-post-processing-error",
			local_error->message, NULL);
		e_shell_submit_alert (shell, alert);
		g_object_unref (alert);

	/* All other errors are shown in the composer window. */
	} else if (local_error != NULL) {
		gint response;

		/* Clear the activity bar before
		 * presenting the error dialog. */
		g_clear_object (&async_context->activity);
		async_context->activity = e_html_editor_new_activity (e_msg_composer_get_editor (async_context->composer));
		activity = async_context->activity;

		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (async_context->composer),
			"mail-composer:send-error",
			local_error->message, NULL);
		if (response == GTK_RESPONSE_OK)  /* Try Again */
			e_msg_composer_send (async_context->composer);
		if (response == GTK_RESPONSE_ACCEPT) { /* Save to Outbox */
			if (async_context->message)
				g_signal_emit_by_name (
					async_context->composer, "save-to-outbox",
					async_context->message, activity);
			else
				e_msg_composer_save_to_outbox (async_context->composer);
		}
		set_changed = TRUE;
		goto exit;
	}

	/* Remove the Outbox message after successful send */
	outbox_uid = e_msg_composer_get_header (async_context->composer, "X-Evolution-Outbox-UID", 0);
	if (outbox_uid && *outbox_uid) {
		CamelSession *session;
		CamelFolder *outbox;

		session = e_msg_composer_ref_session (async_context->composer);
		outbox = e_mail_session_get_local_folder (E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_OUTBOX);
		if (outbox) {
			CamelMessageInfo *info;

			info = camel_folder_get_message_info (outbox, outbox_uid);
			if (info) {
				camel_message_info_set_flags (info, CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);

				g_clear_object (&info);
			}
		}

		g_clear_object (&session);
	}

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	/* Wait for the EActivity's completion message to
	 * time out and then destroy the composer window. */
	g_object_weak_ref (
		G_OBJECT (activity), (GWeakNotify)
		gtk_widget_destroy, async_context->composer);

exit:
	g_clear_error (&local_error);

	if (set_changed) {
		EHTMLEditor *editor;
		EContentEditor *cnt_editor;

		editor = e_msg_composer_get_editor (async_context->composer);
		cnt_editor = e_html_editor_get_content_editor (editor);

		e_content_editor_set_changed (cnt_editor, TRUE);

		gtk_window_present (GTK_WINDOW (async_context->composer));
	}

	async_context_free (async_context);
}

static void
em_utils_composer_real_send (EMsgComposer *composer,
			     CamelMimeMessage *message,
			     EActivity *activity,
			     EMailSession *session)
{
	AsyncContext *async_context;
	CamelService *transport;
	GCancellable *cancellable;
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (settings, "composer-use-outbox")) {
		/* Using e_msg_composer_save_to_outbox() means building
		   the message again; better to use the built message here. */
		g_signal_emit_by_name (
			composer, "save-to-outbox",
			message, activity);

		g_object_unref (settings);
		return;
	}

	g_object_unref (settings);

	if (!camel_session_get_online (CAMEL_SESSION (session))) {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (composer),
			"mail-composer:saving-to-outbox", NULL);

		/* Using e_msg_composer_save_to_outbox() means building
		   the message again; better to use the built message here. */
		g_signal_emit_by_name (
			composer, "save-to-outbox",
			message, activity);

		return;
	}

	async_context = g_slice_new0 (AsyncContext);
	async_context->message = g_object_ref (message);
	async_context->composer = g_object_ref (composer);
	async_context->activity = g_object_ref (activity);

	transport = e_mail_session_ref_transport_for_message (session, message);

	if (transport) {
		EShell *shell;

		shell = e_msg_composer_get_shell (composer);

		async_context->transport_source = e_source_registry_ref_source (e_shell_get_registry (shell), camel_service_get_uid (transport));

		if (async_context->transport_source)
			e_shell_set_auth_prompt_parent (shell, async_context->transport_source, GTK_WINDOW (composer));

		g_object_unref (transport);
	}

	cancellable = e_activity_get_cancellable (activity);

	e_mail_session_send_to (
		session, message,
		G_PRIORITY_DEFAULT,
		cancellable, NULL, NULL,
		composer_send_completed,
		async_context);
}

static void
composer_num_loading_notify_cb (EAttachmentStore *store,
				GParamSpec *param,
				AsyncContext *async_context)
{
	if (e_attachment_store_get_num_loading (store) > 0)
		return;

	em_utils_composer_real_send (
		async_context->composer,
		async_context->message,
		async_context->activity,
		async_context->session);

	async_context_free (async_context);
}

static void
composer_wait_for_attachment_load_cancelled_cb (GCancellable *cancellable,
						AsyncContext *async_context)
{
	async_context_free (async_context);
}

static void
em_utils_composer_send_cb (EMsgComposer *composer,
                           CamelMimeMessage *message,
                           EActivity *activity,
                           EMailSession *session)
{
	AsyncContext *async_context;
	GCancellable *cancellable;
	EAttachmentView *view;
	EAttachmentStore *store;

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	if (e_attachment_store_get_num_loading (store) <= 0) {
		em_utils_composer_real_send (composer, message, activity, session);
		return;
	}

	async_context = g_slice_new0 (AsyncContext);
	async_context->session = g_object_ref (session);
	async_context->message = g_object_ref (message);
	async_context->composer = g_object_ref (composer);
	async_context->activity = g_object_ref (activity);

	cancellable = e_activity_get_cancellable (activity);
	/* This message is never removed from the camel operation, otherwise the GtkInfoBar
	   hides itself and the user sees no feedback. */
	camel_operation_push_message (cancellable, "%s", _("Waiting for attachments to load…"));

	async_context->num_loading_handler_id = e_signal_connect_notify (store, "notify::num-loading",
		G_CALLBACK (composer_num_loading_notify_cb), async_context);
	/* Cannot use g_cancellable_connect() here, see async_context_free() */
	async_context->cancelled_handler_id = g_signal_connect (cancellable, "cancelled",
		G_CALLBACK (composer_wait_for_attachment_load_cancelled_cb), async_context);
}

static void
composer_set_no_change (EMsgComposer *composer)
{
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;

	g_return_if_fail (composer != NULL);

	editor = e_msg_composer_get_editor (composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	e_content_editor_set_changed (cnt_editor, FALSE);
}

/* delete original messages from Outbox folder */
static void
manage_x_evolution_replace_outbox (EMsgComposer *composer,
                                   EMailSession *session,
                                   CamelMimeMessage *message,
                                   GCancellable *cancellable)
{
	const gchar *message_uid;
	const gchar *header;
	CamelFolder *outbox;

	g_return_if_fail (composer != NULL);
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	header = "X-Evolution-Replace-Outbox-UID";
	message_uid = camel_medium_get_header (CAMEL_MEDIUM (message), header);
	e_msg_composer_remove_header (composer, header);

	if (!message_uid)
		return;

	outbox = e_mail_session_get_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX);
	g_return_if_fail (outbox != NULL);

	camel_folder_set_message_flags (
		outbox, message_uid,
		CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN,
		CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);

	/* ignore errors here */
	camel_folder_synchronize_message_sync (
		outbox, message_uid, cancellable, NULL);
}

static void
composer_save_to_drafts_complete (GObject *source_object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	EActivity *activity;
	AsyncContext *async_context;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	editor = e_msg_composer_get_editor (async_context->composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	/* We don't really care if this failed.  If something other than
	 * cancellation happened, emit a runtime warning so the error is
	 * not completely lost. */
	e_mail_session_handle_draft_headers_finish (
		E_MAIL_SESSION (source_object), result, &local_error);

	activity = async_context->activity;

	if (e_activity_handle_cancellation (activity, local_error)) {
		e_content_editor_set_changed (cnt_editor, TRUE);
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_content_editor_set_changed (cnt_editor, TRUE);
		g_warning ("%s", local_error->message);
		g_error_free (local_error);
	} else
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	/* Encode the draft message we just saved into the EMsgComposer
	 * as X-Evolution-Draft headers.  The message will be marked for
	 * deletion if the user saves a newer draft message or sends the
	 * composed message. */
	e_msg_composer_set_draft_headers (
		async_context->composer,
		async_context->folder_uri,
		async_context->message_uid);

	e_content_editor_set_changed (cnt_editor, FALSE);

	async_context_free (async_context);
}

static void
composer_save_to_drafts_append_mail (AsyncContext *async_context,
                                     CamelFolder *drafts_folder);

static void
composer_save_to_drafts_cleanup (GObject *source_object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	CamelSession *session;
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	editor = e_msg_composer_get_editor (async_context->composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	e_mail_folder_append_message_finish (
		CAMEL_FOLDER (source_object), result,
		&async_context->message_uid, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_warn_if_fail (async_context->message_uid == NULL);
		e_content_editor_set_changed (cnt_editor, TRUE);
		async_context_free (async_context);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		g_warn_if_fail (async_context->message_uid == NULL);

		if (e_msg_composer_is_exiting (async_context->composer)) {
			gint response;

			/* If we can't retrieve the Drafts folder for the
			 * selected account, ask the user if he wants to
			 * save to the local Drafts folder instead. */
			response = e_alert_run_dialog_for_args (
				GTK_WINDOW (async_context->composer),
				"mail:ask-default-drafts", local_error->message, NULL);
			if (response != GTK_RESPONSE_YES) {
				e_content_editor_set_changed (cnt_editor, TRUE);
				async_context_free (async_context);
			} else {
				composer_save_to_drafts_append_mail (async_context, NULL);
			}

			g_error_free (local_error);
			return;
		}

		e_alert_submit (
			alert_sink,
			"mail-composer:save-to-drafts-error",
			local_error->message, NULL);
		e_content_editor_set_changed (cnt_editor, TRUE);
		async_context_free (async_context);
		g_error_free (local_error);
		return;
	}

	session = e_msg_composer_ref_session (async_context->composer);

	/* Mark the previously saved draft message for deletion.
	 * Note: This is just a nice-to-have; ignore failures. */
	e_mail_session_handle_draft_headers (
		E_MAIL_SESSION (session),
		async_context->message,
		G_PRIORITY_DEFAULT, cancellable,
		composer_save_to_drafts_complete,
		async_context);

	g_object_unref (session);
}

static void
composer_save_to_drafts_append_mail (AsyncContext *async_context,
                                     CamelFolder *drafts_folder)
{
	CamelFolder *local_drafts_folder;
	GCancellable *cancellable;
	CamelMessageInfo *info;

	local_drafts_folder =
		e_mail_session_get_local_folder (
		async_context->session, E_MAIL_LOCAL_FOLDER_DRAFTS);

	if (drafts_folder == NULL)
		drafts_folder = g_object_ref (local_drafts_folder);

	cancellable = e_activity_get_cancellable (async_context->activity);

	info = camel_message_info_new (NULL);

	camel_message_info_set_flags (info, CAMEL_MESSAGE_DRAFT | CAMEL_MESSAGE_SEEN |
		(camel_mime_message_has_attachment (async_context->message) ? CAMEL_MESSAGE_ATTACHMENTS : 0), ~0);

	camel_medium_remove_header (
		CAMEL_MEDIUM (async_context->message),
		"X-Evolution-Replace-Outbox-UID");

	e_mail_folder_append_message (
		drafts_folder, async_context->message,
		info, G_PRIORITY_DEFAULT, cancellable,
		composer_save_to_drafts_cleanup,
		async_context);

	g_clear_object (&info);

	g_object_unref (drafts_folder);
}

static void
composer_save_to_drafts_got_folder (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
	EActivity *activity;
	CamelFolder *drafts_folder;
	EHTMLEditor *editor;
	EContentEditor *cnt_editor;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;

	editor = e_msg_composer_get_editor (async_context->composer);
	cnt_editor = e_html_editor_get_content_editor (editor);

	drafts_folder = e_mail_session_uri_to_folder_finish (
		E_MAIL_SESSION (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((drafts_folder != NULL) && (local_error == NULL)) ||
		((drafts_folder == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		e_content_editor_set_changed (cnt_editor, TRUE);
		async_context_free (async_context);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		gint response;

		/* If we can't retrieve the Drafts folder for the
		 * selected account, ask the user if he wants to
		 * save to the local Drafts folder instead. */
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (async_context->composer),
			"mail:ask-default-drafts", local_error->message, NULL);

		g_error_free (local_error);

		if (response != GTK_RESPONSE_YES) {
			e_content_editor_set_changed (cnt_editor, TRUE);
			async_context_free (async_context);
			return;
		}
	}

	composer_save_to_drafts_append_mail (async_context, drafts_folder);
}

static void
em_utils_composer_save_to_drafts_cb (EMsgComposer *composer,
                                     CamelMimeMessage *message,
                                     EActivity *activity,
                                     EMailSession *session)
{
	AsyncContext *async_context;
	EComposerHeaderTable *table;
	ESource *source;
	const gchar *local_drafts_folder_uri;
	gchar *identity_uid;
	gchar *drafts_folder_uri = NULL;

	async_context = g_slice_new0 (AsyncContext);
	async_context->message = g_object_ref (message);
	async_context->session = g_object_ref (session);
	async_context->composer = g_object_ref (composer);
	async_context->activity = g_object_ref (activity);

	table = e_msg_composer_get_header_table (composer);

	identity_uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);
	source = e_composer_header_table_ref_source (table, identity_uid);

	/* Get the selected identity's preferred Drafts folder. */
	if (source != NULL) {
		ESourceMailComposition *extension;
		const gchar *extension_name;
		gchar *uri;

		extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
		extension = e_source_get_extension (source, extension_name);
		uri = e_source_mail_composition_dup_drafts_folder (extension);

		drafts_folder_uri = uri;

		g_object_unref (source);
	}

	local_drafts_folder_uri =
		e_mail_session_get_local_folder_uri (
		session, E_MAIL_LOCAL_FOLDER_DRAFTS);

	if (drafts_folder_uri == NULL) {
		async_context->folder_uri = g_strdup (local_drafts_folder_uri);
		composer_save_to_drafts_append_mail (async_context, NULL);
	} else {
		GCancellable *cancellable;

		cancellable = e_activity_get_cancellable (activity);
		async_context->folder_uri = g_strdup (drafts_folder_uri);

		e_mail_session_uri_to_folder (
			session, drafts_folder_uri, 0,
			G_PRIORITY_DEFAULT, cancellable,
			composer_save_to_drafts_got_folder,
			async_context);

		g_free (drafts_folder_uri);
	}

	g_free (identity_uid);
}

static void
emcu_manage_flush_outbox (EMailSession *session)
{
	GSettings *settings;

	g_return_if_fail (E_IS_MAIL_SESSION (session));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (settings, "composer-use-outbox")) {
		gint delay_flush = g_settings_get_int (settings, "composer-delay-outbox-flush");

		if (delay_flush == 0) {
			e_mail_session_flush_outbox (session);
		} else if (delay_flush > 0) {
			e_mail_session_schedule_outbox_flush (session, delay_flush);
		}
	}
	g_object_unref (settings);
}

static void
composer_save_to_outbox_completed (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	EMailSession *session;
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	AsyncContext *async_context;
	GError *local_error = NULL;

	session = E_MAIL_SESSION (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	e_mail_session_append_to_local_folder_finish (
		session, result, NULL, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);
		goto exit;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"mail-composer:append-to-outbox-error",
			local_error->message, NULL);
		g_error_free (local_error);
		goto exit;
	}

	/* special processing for Outbox folder */
	manage_x_evolution_replace_outbox (
		async_context->composer,
		session,
		async_context->message,
		cancellable);

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	/* Wait for the EActivity's completion message to
	 * time out and then destroy the composer window. */
	g_object_weak_ref (
		G_OBJECT (activity), (GWeakNotify)
		gtk_widget_destroy, async_context->composer);

	emcu_manage_flush_outbox (session);

exit:
	async_context_free (async_context);
}

static void
em_utils_composer_save_to_outbox_cb (EMsgComposer *composer,
                                     CamelMimeMessage *message,
                                     EActivity *activity,
                                     EMailSession *session)
{
	AsyncContext *async_context;
	CamelMessageInfo *info;
	GCancellable *cancellable;

	async_context = g_slice_new0 (AsyncContext);
	async_context->message = g_object_ref (message);
	async_context->composer = g_object_ref (composer);
	async_context->activity = g_object_ref (activity);

	cancellable = e_activity_get_cancellable (activity);

	info = camel_message_info_new (NULL);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, ~0);

	e_mail_session_append_to_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX,
		message, info, G_PRIORITY_DEFAULT, cancellable,
		composer_save_to_outbox_completed,
		async_context);

	g_clear_object (&info);
}

typedef struct _PrintAsyncContext {
	GMainLoop *main_loop;
	GError *error;
} PrintAsyncContext;

static void
em_composer_utils_print_done_cb (GObject *source_object,
				 GAsyncResult *result,
				 gpointer user_data)
{
	PrintAsyncContext *async_context = user_data;

	g_return_if_fail (E_IS_MAIL_PRINTER (source_object));
	g_return_if_fail (async_context != NULL);
	g_return_if_fail (async_context->main_loop != NULL);

	e_mail_printer_print_finish (E_MAIL_PRINTER (source_object), result, &(async_context->error));

	g_main_loop_quit (async_context->main_loop);
}

static void
em_utils_composer_print_cb (EMsgComposer *composer,
                            GtkPrintOperationAction action,
                            CamelMimeMessage *message,
                            EActivity *activity,
                            EMailSession *session)
{
	EMailParser *parser;
	EMailPartList *parts, *reserved_parts;
	EMailPrinter *printer;
	EMailBackend *mail_backend;
	const gchar *message_id;
	GCancellable *cancellable;
	CamelObjectBag *parts_registry;
	gchar *mail_uri;
	PrintAsyncContext async_context;

	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (e_msg_composer_get_shell (composer), "mail"));
	g_return_if_fail (mail_backend != NULL);

	cancellable = e_activity_get_cancellable (activity);
	parser = e_mail_parser_new (CAMEL_SESSION (session));

	message_id = camel_mime_message_get_message_id (message);
	parts = e_mail_parser_parse_sync (parser, NULL, message_id, message, cancellable);
	if (!parts) {
		g_clear_object (&parser);
		return;
	}

	parts_registry = e_mail_part_list_get_registry ();

	mail_uri = e_mail_part_build_uri (NULL, message_id, NULL, NULL);
	reserved_parts = camel_object_bag_reserve (parts_registry, mail_uri);
	g_clear_object (&reserved_parts);

	camel_object_bag_add (parts_registry, mail_uri, parts);

	printer = e_mail_printer_new (parts, e_mail_backend_get_remote_content (mail_backend));

	async_context.error = NULL;
	async_context.main_loop = g_main_loop_new (NULL, FALSE);

	/* Cannot use EAsyncClosure here, it blocks the main context, which is not good here. */
	e_mail_printer_print (printer, action, NULL, cancellable, em_composer_utils_print_done_cb, &async_context);

	g_main_loop_run (async_context.main_loop);

	camel_object_bag_remove (parts_registry, parts);
	g_main_loop_unref (async_context.main_loop);
	g_object_unref (printer);
	g_object_unref (parts);
	g_free (mail_uri);

	if (e_activity_handle_cancellation (activity, async_context.error)) {
		g_error_free (async_context.error);
	} else if (async_context.error != NULL) {
		e_alert_submit (
			e_activity_get_alert_sink (activity),
			"mail-composer:no-build-message",
			async_context.error->message, NULL);
		g_error_free (async_context.error);
	}
}

static gint
compare_sources_with_uids_order_cb (gconstpointer a,
                                    gconstpointer b,
                                    gpointer user_data)
{
	ESource *asource = (ESource *) a;
	ESource *bsource = (ESource *) b;
	GHashTable *uids_order = user_data;
	gint aindex, bindex;

	aindex = GPOINTER_TO_INT (g_hash_table_lookup (uids_order, e_source_get_uid (asource)));
	bindex = GPOINTER_TO_INT (g_hash_table_lookup (uids_order, e_source_get_uid (bsource)));

	if (aindex <= 0)
		aindex = g_hash_table_size (uids_order);
	if (bindex <= 0)
		bindex = g_hash_table_size (uids_order);

	return aindex - bindex;
}

static void
sort_sources_by_ui (GList **psources,
                    gpointer user_data)
{
	EShell *shell = user_data;
	EShellBackend *shell_backend;
	EMailSession *mail_session;
	EMailAccountStore *account_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GHashTable *uids_order;
	gint index = 0;

	g_return_if_fail (psources != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	/* nothing to sort */
	if (!*psources || !g_list_next (*psources))
		return;

	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	g_return_if_fail (shell_backend != NULL);

	mail_session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));
	g_return_if_fail (mail_session != NULL);

	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (mail_session));
	g_return_if_fail (account_store != NULL);

	model = GTK_TREE_MODEL (account_store);
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	uids_order = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	do {
		CamelService *service = NULL;

		gtk_tree_model_get (model, &iter, E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, &service, -1);

		if (service) {
			index++;
			g_hash_table_insert (uids_order, g_strdup (camel_service_get_uid (service)), GINT_TO_POINTER (index));
			g_object_unref (service);
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	*psources = g_list_sort_with_data (*psources, compare_sources_with_uids_order_cb, uids_order);

	g_hash_table_destroy (uids_order);
}

/* (trasfer full) */
ESource *
em_composer_utils_guess_identity_source (EShell *shell,
					 CamelMimeMessage *message,
					 CamelFolder *folder,
					 const gchar *message_uid,
					 gchar **out_identity_name,
					 gchar **out_identity_address)
{
	ESource *source;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	/* Check send account override for the passed-in folder */
	source = em_utils_check_send_account_override (shell, message, folder, out_identity_name, out_identity_address);

	/* If not set and it's a search folder, then check the original folder */
	if (!source && message_uid && CAMEL_IS_VEE_FOLDER (folder)) {
		CamelMessageInfo *mi = camel_folder_get_message_info (folder, message_uid);
		if (mi) {
			CamelFolder *location;

			location = camel_vee_folder_get_location (CAMEL_VEE_FOLDER (folder), (CamelVeeMessageInfo *) mi, NULL);
			if (location)
				source = em_utils_check_send_account_override (shell, message, location, out_identity_name, out_identity_address);
			g_clear_object (&mi);
		}
	}

	/* If no send account override, then guess */
	if (!source) {
		source = em_utils_guess_mail_identity_with_recipients_and_sort (e_shell_get_registry (shell),
			message, folder, message_uid, out_identity_name, out_identity_address, sort_sources_by_ui, shell);
	}

	return source;
}

/* Composing messages... */

static CamelMimeMessage *em_utils_get_composer_recipients_as_message (EMsgComposer *composer);

static void
set_up_new_composer (EMsgComposer *composer,
		     const gchar *subject,
		     CamelFolder *folder,
		     CamelMimeMessage *message,
		     const gchar *message_uid,
		     gboolean is_new_message)
{
	EClientCache *client_cache;
	ESourceRegistry *registry;
	EComposerHeaderTable *table;
	ESource *source = NULL;
	gchar *identity = NULL, *identity_name = NULL, *identity_address = NULL;

	table = e_msg_composer_get_header_table (composer);

	client_cache = e_composer_header_table_ref_client_cache (table);
	registry = e_client_cache_ref_registry (client_cache);

	if (folder != NULL) {
		gchar *folder_uri;
		GList *list;

		if (message) {
			g_object_ref (message);
		} else if (message_uid && !is_new_message) {
			/* Do this only if it's not a new message, in case the default account
			   has set "always CC/Bcc" address, which might match one of the configured
			   accounts, which in turn forces to use the default account due to this
			   read back from the composer. */
			message = em_utils_get_composer_recipients_as_message (composer);
		}

		if (message) {
			EShell *shell;

			shell = e_msg_composer_get_shell (composer);

			source = em_composer_utils_guess_identity_source (shell, message, folder, message_uid, &identity_name, &identity_address);
		}

		/* In case of search folder, try to guess the store from
		   the internal folders of it. If they are all from the same
		   store, then use that store. */
		if (!source && CAMEL_IS_VEE_FOLDER (folder)) {
			GHashTable *stores, *done_folders;
			GSList *todo;

			stores = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);
			done_folders = g_hash_table_new (g_direct_hash, g_direct_equal);

			todo = g_slist_prepend (NULL, g_object_ref (folder));

			while (todo) {
				CamelVeeFolder *vfolder = todo->data;

				todo = g_slist_remove (todo, vfolder);
				if (!g_hash_table_contains (done_folders, vfolder)) {
					GPtrArray *subfolders;
					guint ii;

					g_hash_table_insert (done_folders, vfolder, NULL);

					subfolders = camel_vee_folder_dup_folders (vfolder);
					for (ii = 0; subfolders && ii < subfolders->len; ii++) {
						CamelFolder *subfolder = g_ptr_array_index (subfolders, ii);

						if (!g_hash_table_contains (done_folders, subfolder)) {
							if (CAMEL_IS_VEE_FOLDER (subfolder)) {
								todo = g_slist_prepend (todo, g_object_ref (subfolder));
							} else {
								CamelStore *store = camel_folder_get_parent_store (subfolder);

								g_hash_table_insert (done_folders, subfolder, NULL);

								if (store) {
									g_hash_table_insert (stores, g_object_ref (store), NULL);

									if (g_hash_table_size (stores) > 1) {
										g_slist_free_full (todo, g_object_unref);
										todo = NULL;
										break;
									}
								}
							}
						}
					}

					g_clear_pointer (&subfolders, g_ptr_array_unref);
				}

				g_object_unref (vfolder);
			}

			if (g_hash_table_size (stores) == 1) {
				GHashTableIter iter;
				gpointer store;

				g_hash_table_iter_init (&iter, stores);
				if (g_hash_table_iter_next (&iter, &store, NULL) && store)
					source = em_utils_ref_mail_identity_for_store (registry, store);
			}

			g_slist_free_full (todo, g_object_unref);
			g_hash_table_destroy (done_folders);
			g_hash_table_destroy (stores);
		}

		g_clear_object (&message);

		if (!source) {
			CamelStore *store;

			store = camel_folder_get_parent_store (folder);
			source = em_utils_ref_mail_identity_for_store (registry, store);
		}

		folder_uri = e_mail_folder_uri_from_folder (folder);

		list = g_list_prepend (NULL, folder_uri);
		e_composer_header_table_set_post_to_list (table, list);
		g_list_free (list);

		g_free (folder_uri);
	}

	if (source != NULL) {
		identity = e_source_dup_uid (source);
		g_object_unref (source);
	}

	if (subject)
		e_composer_header_table_set_subject (table, subject);
	e_composer_header_table_set_identity_uid (table, identity, identity_name, identity_address);

	em_utils_apply_send_account_override_to_composer (composer, folder);

	g_free (identity);
	g_free (identity_name);
	g_free (identity_address);

	g_object_unref (client_cache);
	g_object_unref (registry);
}

/**
 * em_utils_compose_new_message:
 * @composer: an #EMsgComposer
 * @folder: (nullable): a #CamelFolder, or %NULL
 *
 * Sets up a new @composer window.
 *
 * See: em_utils_compose_new_message_with_selection()
 *
 * Since: 3.22
 **/
void
em_utils_compose_new_message (EMsgComposer *composer,
                              CamelFolder *folder)
{
	em_utils_compose_new_message_with_selection (composer, folder, NULL);
}

/**
 * em_utils_compose_new_message_with_selection:
 * @composer: an #EMsgComposer
 * @folder: (nullable): a #CamelFolder, or %NULL
 * @message_uid: (nullable): a UID of the selected message, or %NULL
 *
 * Sets up a new @composer window, similar to em_utils_compose_new_message(),
 * but also tries to identify From account more precisely, when the @folder
 * is a search folder.
 *
 * Since: 3.28
 **/
void
em_utils_compose_new_message_with_selection (EMsgComposer *composer,
					     CamelFolder *folder,
					     const gchar *message_uid)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (folder)
		g_return_if_fail (CAMEL_IS_FOLDER (folder));

	set_up_new_composer (composer, "", folder, NULL, message_uid, TRUE);
	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));
}

static CamelMimeMessage *
em_utils_get_composer_recipients_as_message (EMsgComposer *composer)
{
	CamelMimeMessage *message;
	EComposerHeaderTable *table;
	EComposerHeader *header;
	EDestination **destv;
	CamelInternetAddress *to_addr, *cc_addr, *bcc_addr, *dest_addr;
	const gchar *text_addr;
	gint ii;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), NULL);

	table = e_msg_composer_get_header_table (composer);
	header = e_composer_header_table_get_header (table, E_COMPOSER_HEADER_TO);

	if (!e_composer_header_get_visible (header))
		return NULL;

	message = camel_mime_message_new ();

	to_addr = camel_internet_address_new ();
	cc_addr = camel_internet_address_new ();
	bcc_addr = camel_internet_address_new ();

	/* To */
	dest_addr = to_addr;
	destv = e_composer_header_table_get_destinations_to (table);
	for (ii = 0; destv != NULL && destv[ii] != NULL; ii++) {
		text_addr = e_destination_get_address (destv[ii]);
		if (text_addr && *text_addr) {
			if (camel_address_decode (CAMEL_ADDRESS (dest_addr), text_addr) <= 0)
				camel_internet_address_add (dest_addr, "", text_addr);
		}
	}
	e_destination_freev (destv);

	/* CC */
	dest_addr = cc_addr;
	destv = e_composer_header_table_get_destinations_cc (table);
	for (ii = 0; destv != NULL && destv[ii] != NULL; ii++) {
		text_addr = e_destination_get_address (destv[ii]);
		if (text_addr && *text_addr) {
			if (camel_address_decode (CAMEL_ADDRESS (dest_addr), text_addr) <= 0)
				camel_internet_address_add (dest_addr, "", text_addr);
		}
	}
	e_destination_freev (destv);

	/* Bcc */
	dest_addr = bcc_addr;
	destv = e_composer_header_table_get_destinations_bcc (table);
	for (ii = 0; destv != NULL && destv[ii] != NULL; ii++) {
		text_addr = e_destination_get_address (destv[ii]);
		if (text_addr && *text_addr) {
			if (camel_address_decode (CAMEL_ADDRESS (dest_addr), text_addr) <= 0)
				camel_internet_address_add (dest_addr, "", text_addr);
		}
	}
	e_destination_freev (destv);

	if (camel_address_length (CAMEL_ADDRESS (to_addr)) > 0)
		camel_mime_message_set_recipients (message, CAMEL_RECIPIENT_TYPE_TO, to_addr);

	if (camel_address_length (CAMEL_ADDRESS (cc_addr)) > 0)
		camel_mime_message_set_recipients (message, CAMEL_RECIPIENT_TYPE_CC, cc_addr);

	if (camel_address_length (CAMEL_ADDRESS (bcc_addr)) > 0)
		camel_mime_message_set_recipients (message, CAMEL_RECIPIENT_TYPE_BCC, bcc_addr);

	g_object_unref (to_addr);
	g_object_unref (cc_addr);
	g_object_unref (bcc_addr);

	return message;
}

typedef struct _CreateComposerData {
	CamelFolder *folder;
	const gchar *message_uid; /* In the Camel string pool */
	gchar *mailto;
} CreateComposerData;

static void
create_composer_data_free (gpointer ptr)
{
	CreateComposerData *ccd = ptr;

	if (ccd) {
		g_clear_object (&ccd->folder);
		camel_pstring_free (ccd->message_uid);
		g_free (ccd->mailto);
		g_slice_free (CreateComposerData, ccd);
	}
}

static void
msg_composer_created_with_mailto_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (ccd != NULL);

	g_application_release (G_APPLICATION (e_shell_get_default ()));

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
		create_composer_data_free (ccd);

		return;
	}

	if (ccd->mailto)
		e_msg_composer_setup_from_url (composer, ccd->mailto);

	set_up_new_composer (composer, NULL, ccd->folder, NULL, ccd->message_uid, TRUE);

	composer_set_no_change (composer);

	gtk_window_present (GTK_WINDOW (composer));

	create_composer_data_free (ccd);
}

/**
 * em_utils_compose_new_message_with_mailto:
 * @shell: an #EShell
 * @mailto: a mailto URL
 * @folder: a #CamelFolder, or %NULL
 *
 * Opens a new composer window as a child window of @parent's toplevel
 * window. If @mailto is non-NULL, the composer fields will be filled in
 * according to the values in the mailto URL.
 *
 * See: em_utils_compose_new_message_with_mailto_and_selection()
 **/
void
em_utils_compose_new_message_with_mailto (EShell *shell,
                                          const gchar *mailto,
                                          CamelFolder *folder)
{
	em_utils_compose_new_message_with_mailto_and_selection (shell, mailto, folder, NULL);
}

/**
 * em_utils_compose_new_message_with_mailto_and_selection:
 * @shell: an #EShell
 * @mailto: a mailto URL
 * @folder: a #CamelFolder, or %NULL
 * @message_uid: (nullable): a UID of the selected message, or %NULL
 *
 * similarly to em_utils_compose_new_message_with_mailto(), opens a new composer
 * window as a child window of @parent's toplevel window. If @mailto is non-NULL,
 * the composer fields will be filled in according to the values in the mailto URL.
 * It also tries to identify From account more precisely, when the @folder
 * is a search folder.
 *
 * Since: 3.28
 **/
void
em_utils_compose_new_message_with_mailto_and_selection (EShell *shell,
							const gchar *mailto,
							CamelFolder *folder,
							const gchar *message_uid)
{
	CreateComposerData *ccd;

	g_return_if_fail (E_IS_SHELL (shell));

	if (folder)
		g_return_if_fail (CAMEL_IS_FOLDER (folder));

	ccd = g_slice_new0 (CreateComposerData);
	ccd->folder = folder ? g_object_ref (folder) : NULL;
	ccd->message_uid = camel_pstring_strdup (message_uid);
	ccd->mailto = g_strdup (mailto);

	/* In case the app was started with "mailto:" URI; the composer is created
	   asynchronously, where the async delay can cause shutdown of the app. */
	g_application_hold (G_APPLICATION (shell));

	e_msg_composer_new (shell, msg_composer_created_with_mailto_cb, ccd);
}

static ESource *
emcu_ref_identity_source_from_composer (EMsgComposer *composer)
{
	EComposerHeaderTable *table;
	ESource *source = NULL;
	gchar *identity_uid;

	if (!composer)
		return NULL;

	table = e_msg_composer_get_header_table (composer);
	identity_uid = e_composer_header_table_dup_identity_uid (table, NULL, NULL);
	if (identity_uid)
		source = e_composer_header_table_ref_source (table, identity_uid);
	g_free (identity_uid);

	return source;
}

static void
emcu_change_locale (const gchar *lc_messages,
		    const gchar *lc_time,
		    gchar **out_lc_messages,
		    gchar **out_lc_time)
{
	gboolean success;
	gchar *previous;

	if (lc_messages) {
		#if defined(LC_MESSAGES)
		previous = g_strdup (setlocale (LC_MESSAGES, NULL));
		success = setlocale (LC_MESSAGES, lc_messages) != NULL;
		#else
		previous = g_strdup (setlocale (LC_ALL, NULL));
		success = setlocale (LC_ALL, lc_messages) != NULL;
		#endif

		if (out_lc_messages)
			*out_lc_messages = success ? g_strdup (previous) : NULL;

		g_free (previous);
	}

	if (lc_time) {
		#if defined(LC_TIME)
		previous = g_strdup (setlocale (LC_TIME, NULL));
		success = setlocale (LC_TIME, lc_time) != NULL;
		#elif defined(LC_MESSAGES)
		previous = g_strdup (setlocale (LC_ALL, NULL));
		success = setlocale (LC_ALL, lc_time) != NULL;
		#else
		previous = NULL;
		success = FALSE;
		#endif

		if (out_lc_time)
			*out_lc_time = success ? g_strdup (previous) : NULL;

		g_free (previous);
	}
}

static void
emcu_prepare_attribution_locale (ESource *identity_source,
				 gchar **out_lc_messages,
				 gchar **out_lc_time)
{
	gchar *lang = NULL;

	g_return_if_fail (out_lc_messages != NULL);
	g_return_if_fail (out_lc_time != NULL);

	if (identity_source && e_source_has_extension (identity_source, E_SOURCE_EXTENSION_MAIL_COMPOSITION)) {
		ESourceMailComposition *extension;

		extension = e_source_get_extension (identity_source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
		lang = e_source_mail_composition_dup_language (extension);
	}

	if (!lang || !*lang) {
		GSettings *settings;

		g_free (lang);

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		lang = g_settings_get_string (settings, "composer-attribution-language");
		g_object_unref (settings);

		if (!lang || !*lang)
			g_clear_pointer (&lang, g_free);
	}

	if (!lang) {
		/* Set the locale always, even when using the user interface
		   language, because gettext() can return wrong text (in the previously
		   used language) */
		#if defined(LC_MESSAGES)
		lang = g_strdup (setlocale (LC_MESSAGES, NULL));
		#else
		lang = g_strdup (setlocale (LC_ALL, NULL));
		#endif
	}

	if (lang) {
		if (!g_str_equal (lang, "C") && !strchr (lang, '.')) {
			gchar *tmp;

			tmp = g_strconcat (lang, ".UTF-8", NULL);
			g_free (lang);
			lang = tmp;
		}

		emcu_change_locale (lang, lang, out_lc_messages, out_lc_time);

		g_free (lang);
	}
}

/* Takes ownership (frees) the 'restore_locale' */
static void
emcu_restore_locale_after_attribution (gchar *restore_lc_messages,
				       gchar *restore_lc_time)
{
	emcu_change_locale (restore_lc_messages, restore_lc_time, NULL, NULL);

	g_free (restore_lc_messages);
	g_free (restore_lc_time);
}

static gchar *
emcu_generate_forward_subject (EMsgComposer *composer,
			       CamelMimeMessage *message,
			       const gchar *orig_subject)
{
	GSettings *settings;
	gchar *restore_lc_messages = NULL, *restore_lc_time = NULL;
	gchar *subject;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (g_settings_get_boolean (settings, "composer-use-localized-fwd-re")) {
		ESource *identity_source;

		identity_source = emcu_ref_identity_source_from_composer (composer);

		emcu_prepare_attribution_locale (identity_source, &restore_lc_messages, &restore_lc_time);

		g_clear_object (&identity_source);
	}

	g_object_unref (settings);

	subject = mail_tool_generate_forward_subject (message, orig_subject);

	emcu_restore_locale_after_attribution (restore_lc_messages, restore_lc_time);

	return subject;
}

/* Editing messages... */

typedef enum {
	QUOTING_ATTRIBUTION,
	QUOTING_FORWARD,
	QUOTING_ORIGINAL
} QuotingTextEnum;

static struct {
	const gchar * conf_key;
	const gchar * message;
} conf_messages[] = {
	[QUOTING_ATTRIBUTION] =
		{ "composer-message-attribution",
		/* Note to translators: this is the attribution string used
		 * when quoting messages.  Each ${Variable} gets replaced
		 * with a value.  To see a full list of available variables,
		 * see mail/em-composer-utils.c:attribvars array. */
		  N_("On ${AbbrevWeekdayName}, ${Year}-${Month}-${Day} at "
		     "${24Hour}:${Minute} ${TimeZone}, ${Sender} wrote:")
		},

	[QUOTING_FORWARD] =
		{ "composer-message-forward",
		  N_("-------- Forwarded Message --------")
		},

	[QUOTING_ORIGINAL] =
		{ "composer-message-original",
		  N_("-----Original Message-----")
		}
};

static gchar *
quoting_text (QuotingTextEnum type,
	      EMsgComposer *composer,
	      gchar **out_restore_lc_messages,
	      gchar **out_restore_lc_time)
{
	GSettings *settings;
	gchar *restore_lc_messages = NULL, *restore_lc_time = NULL;
	gchar *text;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	text = g_settings_get_string (settings, conf_messages[type].conf_key);
	g_object_unref (settings);

	if (text && *text) {
		if (composer && out_restore_lc_messages && out_restore_lc_time) {
			ESource *identity_source;

			identity_source = emcu_ref_identity_source_from_composer (composer);

			emcu_prepare_attribution_locale (identity_source, &restore_lc_messages, &restore_lc_time);

			g_clear_object (&identity_source);
		}
		return text;
	}

	g_free (text);

	if (composer) {
		ESource *identity_source;

		identity_source = emcu_ref_identity_source_from_composer (composer);

		emcu_prepare_attribution_locale (identity_source, &restore_lc_messages, &restore_lc_time);

		g_clear_object (&identity_source);
	}

	text = g_strdup (_(conf_messages[type].message));

	if (out_restore_lc_messages && out_restore_lc_time) {
		*out_restore_lc_messages = restore_lc_messages;
		*out_restore_lc_time = restore_lc_time;
	} else {
		emcu_restore_locale_after_attribution (restore_lc_messages, restore_lc_time);
	}

	return text;
}

/**
 * em_composer_utils_get_forward_marker:
 * @composer: an #EMsgComposer from which to get the identity information
 *
 * Returns: (transfer full): a text marker which is used for inline forwarded messages.
 *   Free returned pointer with g_free(), when no longer needed.
 *
 * Since: 3.24
 **/
gchar *
em_composer_utils_get_forward_marker (EMsgComposer *composer)
{
	return quoting_text (QUOTING_FORWARD, composer, NULL, NULL);
}

/**
 * em_composer_utils_get_original_marker:
 * @composer: an #EMsgComposer from which to get the identity information
 *
 * Returns: (transfer full): a text marker which is used for inline message replies.
 *   Free returned pointer with g_free(), when no longer needed.
 *
 * Since: 3.24
 **/
gchar *
em_composer_utils_get_original_marker (EMsgComposer *composer)
{
	return quoting_text (QUOTING_ORIGINAL, composer, NULL, NULL);
}

static gboolean
emcu_message_references_existing_account (CamelMimeMessage *message,
					  EMsgComposer *composer)
{
	ESource *source;
	gchar *identity_uid;
	gboolean res = FALSE;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);
	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), FALSE);

	identity_uid = (gchar *) camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Identity");
	if (!identity_uid) {
		/* for backward compatibility */
		identity_uid = (gchar *) camel_medium_get_header (CAMEL_MEDIUM (message), "X-Evolution-Account");
	}

	if (!identity_uid)
		return FALSE;

	identity_uid = g_strstrip (g_strdup (identity_uid));
	source = e_composer_header_table_ref_source (e_msg_composer_get_header_table (composer), identity_uid);

	res = source != NULL;

	g_clear_object (&source);
	g_free (identity_uid);

	return res;
}

typedef struct _OutboxData {
	CamelSession *session;
	CamelMessageInfo *info;
} OutboxData;

static void
outbox_data_free (gpointer ptr)
{
	OutboxData *od = ptr;

	if (od) {
		if (od->info) {
			g_object_set_data (G_OBJECT (od->info), MAIL_USER_KEY_EDITING, NULL);

			if (od->session && !(camel_message_info_get_flags (od->info) & CAMEL_MESSAGE_DELETED)) {
				emcu_manage_flush_outbox (E_MAIL_SESSION (od->session));
			}
		}

		g_clear_object (&od->session);
		g_clear_object (&od->info);
		g_free (od);
	}
}

/**
 * em_utils_edit_message:
 * @composer: an #EMsgComposer
 * @folder: a #CamelFolder
 * @message: a #CamelMimeMessage
 * @message_uid: UID of @message, or %NULL
 * @keep_signature: whether to keep signature in the original message
 * @replace_original_message: whether can replace the message in the original Draft/Outbox folder
 *
 * Sets up the @composer with the headers/mime-parts/etc of the @message.
 *
 * Since: 3.22
 **/
void
em_utils_edit_message (EMsgComposer *composer,
                       CamelFolder *folder,
                       CamelMimeMessage *message,
                       const gchar *message_uid,
                       gboolean keep_signature,
		       gboolean replace_original_message)
{
	ESourceRegistry *registry;
	ESource *source;
	CamelFolder *real_folder = NULL, *original_folder = NULL;
	gboolean folder_is_sent;
	gboolean folder_is_drafts;
	gboolean folder_is_outbox;
	gboolean folder_is_templates;
	gchar *real_message_uid = NULL;
	gchar *override_identity_uid = NULL, *override_alias_name = NULL, *override_alias_address = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	if (folder) {
		g_return_if_fail (CAMEL_IS_FOLDER (folder));

		if (CAMEL_IS_VEE_FOLDER (folder) && message_uid) {
			em_utils_get_real_folder_and_message_uid (folder, message_uid, &real_folder, NULL, &real_message_uid);

			if (real_folder) {
				original_folder = folder;
				folder = real_folder;
			}

			if (real_message_uid)
				message_uid = real_message_uid;
		}
	}

	registry = e_shell_get_registry (e_msg_composer_get_shell (composer));

	if (folder) {
		folder_is_sent = em_utils_folder_is_sent (registry, folder);
		folder_is_drafts = em_utils_folder_is_drafts (registry, folder);
		folder_is_outbox = em_utils_folder_is_outbox (registry, folder);
		folder_is_templates = em_utils_folder_is_templates (registry, folder);
	} else {
		folder_is_sent = FALSE;
		folder_is_drafts = FALSE;
		folder_is_outbox = FALSE;
		folder_is_templates = FALSE;
	}
	if (folder) {
		if ((!folder_is_sent && !folder_is_drafts && !folder_is_outbox && !folder_is_templates) ||
		    (!folder_is_outbox && !folder_is_templates && !emcu_message_references_existing_account (message, composer))) {
			CamelStore *store;

			store = camel_folder_get_parent_store (folder);
			source = em_utils_ref_mail_identity_for_store (registry, store);

			if (source) {
				g_free (override_identity_uid);
				override_identity_uid = e_source_dup_uid (source);
				g_object_unref (source);
			}
		}

		source = NULL;

		if (original_folder)
			source = em_utils_check_send_account_override (e_msg_composer_get_shell (composer), message, original_folder, &override_alias_name, &override_alias_address);
		if (!source)
			source = em_utils_check_send_account_override (e_msg_composer_get_shell (composer), message, folder, &override_alias_name, &override_alias_address);
		if (source) {
			g_free (override_identity_uid);
			override_identity_uid = e_source_dup_uid (source);
			g_object_unref (source);
		}
	}

	/* Remember the source message headers when re-editing in Drafts or Outbox,
	   thus Reply, Forward and such mark the source message properly. Do this
	   before the setup_with_message(), because it modifies the message headers. */
	if (folder_is_drafts || folder_is_outbox) {
		CamelMedium *medium;
		const gchar *hdr_folder;
		const gchar *hdr_message;
		const gchar *hdr_flags;

		medium = CAMEL_MEDIUM (message);

		hdr_folder = camel_medium_get_header (medium, "X-Evolution-Source-Folder");
		hdr_message = camel_medium_get_header (medium, "X-Evolution-Source-Message");
		hdr_flags = camel_medium_get_header (medium, "X-Evolution-Source-Flags");

		if (hdr_folder && hdr_message && hdr_flags) {
			e_msg_composer_set_header (composer, "X-Evolution-Source-Folder", hdr_folder);
			e_msg_composer_set_header (composer, "X-Evolution-Source-Message", hdr_message);
			e_msg_composer_set_header (composer, "X-Evolution-Source-Flags", hdr_flags);
		}
	} else if (folder_is_templates) {
		gchar *folder_uri;

		folder_uri = e_mail_folder_uri_from_folder (folder);

		if (folder_uri && message_uid) {
			e_msg_composer_set_header (composer, "X-Evolution-Templates-Folder", folder_uri);
			e_msg_composer_set_header (composer, "X-Evolution-Templates-Message", message_uid);
		}

		g_free (folder_uri);
	}

	e_msg_composer_setup_with_message (composer, message, keep_signature, override_identity_uid, override_alias_name, override_alias_address, NULL);

	g_free (override_identity_uid);
	g_free (override_alias_name);
	g_free (override_alias_address);

	/* Override PostTo header only if the folder is a regular folder */
	if (folder && !folder_is_sent && !folder_is_drafts && !folder_is_outbox && !folder_is_templates) {
		EComposerHeaderTable *table;
		gchar *folder_uri;
		GList *list;

		table = e_msg_composer_get_header_table (composer);

		folder_uri = e_mail_folder_uri_from_folder (folder);

		list = g_list_prepend (NULL, folder_uri);
		e_composer_header_table_set_post_to_list (table, list);
		g_list_free (list);

		g_free (folder_uri);
	}

	e_msg_composer_remove_header (
		composer, "X-Evolution-Replace-Outbox-UID");

	if (message_uid != NULL && folder_is_drafts && folder && replace_original_message) {
		gchar *folder_uri;

		folder_uri = e_mail_folder_uri_from_folder (folder);

		e_msg_composer_set_draft_headers (
			composer, folder_uri, message_uid);

		g_free (folder_uri);

	} else if (message_uid != NULL && folder_is_outbox && replace_original_message) {
		CamelMessageInfo *info;

		e_msg_composer_set_header (
			composer, "X-Evolution-Replace-Outbox-UID",
			message_uid);

		info = camel_folder_get_message_info (folder, message_uid);
		if (info) {
			OutboxData *od;

			/* This makes the message not to send it while it's being edited */
			g_object_set_data (G_OBJECT (info), MAIL_USER_KEY_EDITING, GINT_TO_POINTER (1));

			od = g_new0 (OutboxData, 1);
			od->session = e_msg_composer_ref_session (composer);
			od->info = info; /* takes ownership of it */

			g_object_set_data_full (G_OBJECT (composer), MAIL_USER_KEY_EDITING, od, outbox_data_free);
		}
	}

	if (message_uid != NULL && folder_is_outbox) {
		/* To remove the message after send */
		e_msg_composer_set_header (
			composer, "X-Evolution-Outbox-UID",
			message_uid);
	}

	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));

	g_clear_object (&real_folder);
	g_free (real_message_uid);
}

static void
emu_update_composers_security (EMsgComposer *composer,
                               guint32 validity_found)
{
	EUIAction *action;
	GSettings *settings;
	gboolean sign_reply;

	g_return_if_fail (composer != NULL);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	sign_reply = (validity_found & E_MAIL_PART_VALIDITY_SIGNED) != 0 &&
		     g_settings_get_boolean (settings, "composer-sign-reply-if-signed");
	g_object_unref (settings);

	/* Pre-set only for encrypted messages, not for signed */
	if (sign_reply) {
		action = NULL;

		if (validity_found & E_MAIL_PART_VALIDITY_SMIME) {
			if (!e_ui_action_get_active (E_COMPOSER_ACTION_PGP_SIGN (composer)) &&
			    !e_ui_action_get_active (E_COMPOSER_ACTION_PGP_ENCRYPT (composer)))
				action = E_COMPOSER_ACTION_SMIME_SIGN (composer);
		} else {
			if (!e_ui_action_get_active (E_COMPOSER_ACTION_SMIME_SIGN (composer)) &&
			    !e_ui_action_get_active (E_COMPOSER_ACTION_SMIME_ENCRYPT (composer)))
				action = E_COMPOSER_ACTION_PGP_SIGN (composer);
		}

		if (action)
			e_ui_action_set_active (action, TRUE);
	}

	if (validity_found & E_MAIL_PART_VALIDITY_ENCRYPTED) {
		action = NULL;

		if (validity_found & E_MAIL_PART_VALIDITY_SMIME) {
			if (!e_ui_action_get_active (E_COMPOSER_ACTION_PGP_SIGN (composer)) &&
			    !e_ui_action_get_active (E_COMPOSER_ACTION_PGP_ENCRYPT (composer)))
				action = E_COMPOSER_ACTION_SMIME_ENCRYPT (composer);
		} else {
			if (!e_ui_action_get_active (E_COMPOSER_ACTION_SMIME_SIGN (composer)) &&
			    !e_ui_action_get_active (E_COMPOSER_ACTION_SMIME_ENCRYPT (composer)))
				action = E_COMPOSER_ACTION_PGP_ENCRYPT (composer);
		}

		if (action)
			e_ui_action_set_active (action, TRUE);
	}
}

void
em_utils_get_real_folder_uri_and_message_uid (CamelFolder *folder,
                                              const gchar *uid,
                                              gchar **folder_uri,
                                              gchar **message_uid)
{
	g_return_if_fail (folder != NULL);
	g_return_if_fail (uid != NULL);
	g_return_if_fail (folder_uri != NULL);
	g_return_if_fail (message_uid != NULL);

	em_utils_get_real_folder_and_message_uid (folder, uid, NULL, folder_uri, message_uid);
}

static void
emu_add_composer_references_from_message (EMsgComposer *composer,
					  CamelMimeMessage *message)
{
	const gchar *message_id_header;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	message_id_header = camel_mime_message_get_message_id (message);
	if (message_id_header && *message_id_header) {
		GString *references = g_string_new ("");
		gint ii = 0;
		const gchar *value;
		gchar *unfolded;

		while (value = e_msg_composer_get_header (composer, "References", ii), value) {
			ii++;

			if (references->len)
				g_string_append_c (references, ' ');
			g_string_append (references, value);
		}

		if (references->len)
			g_string_append_c (references, ' ');

		if (*message_id_header != '<')
			g_string_append_c (references, '<');

		g_string_append (references, message_id_header);

		if (*message_id_header != '<')
			g_string_append_c (references, '>');

		unfolded = camel_header_unfold (references->str);

		e_msg_composer_set_header (composer, "References", unfolded);

		g_string_free (references, TRUE);
		g_free (unfolded);
	}
}

static void
real_update_forwarded_flag (gpointer uid,
                            gpointer folder)
{
	if (uid && folder)
		camel_folder_set_message_flags (
			folder, uid, CAMEL_MESSAGE_FORWARDED,
			CAMEL_MESSAGE_FORWARDED);
}

static void
update_forwarded_flags_cb (EMsgComposer *composer,
			   CamelMimeMessage *message,
			   EActivity *activity,
                           ForwardData *data)
{
	if (data && data->uids && data->folder)
		g_ptr_array_foreach (
			data->uids, real_update_forwarded_flag, data->folder);
}

static void
emu_set_source_headers (EMsgComposer *composer,
			CamelFolder *folder,
			const gchar *message_uid,
			guint32 flags)
{
	gchar *source_folder_uri = NULL;
	gchar *source_message_uid = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (!folder || !message_uid)
		return;
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	em_utils_get_real_folder_uri_and_message_uid (folder, message_uid,
		&source_folder_uri, &source_message_uid);

	if (!source_message_uid)
		source_message_uid = g_strdup (message_uid);

	if (source_folder_uri && source_message_uid)
		e_msg_composer_set_source_headers (composer, source_folder_uri, source_message_uid, flags);

	g_free (source_folder_uri);
	g_free (source_message_uid);
}

static void
setup_forward_attached_callbacks (EMsgComposer *composer,
                                  CamelFolder *folder,
                                  GPtrArray *uids)
{
	ForwardData *data;

	if (!composer || !folder || !uids || !uids->len)
		return;

	if (uids->len == 1) {
		emu_set_source_headers (composer, folder, uids->pdata[0], CAMEL_MESSAGE_FORWARDED);
		return;
	}

	data = g_slice_new0 (ForwardData);
	data->folder = g_object_ref (folder);
	data->uids = g_ptr_array_ref (uids);

	g_signal_connect (
		composer, "send",
		G_CALLBACK (update_forwarded_flags_cb), data);
	g_signal_connect (
		composer, "save-to-drafts",
		G_CALLBACK (update_forwarded_flags_cb), data);

	g_object_set_data_full (
		G_OBJECT (composer), "forward-data", data,
		(GDestroyNotify) forward_data_free);
}

static void
forward_non_attached (EMsgComposer *composer,
                      CamelFolder *folder,
                      const gchar *uid,
                      CamelMimeMessage *message,
                      EMailForwardStyle style,
		      gboolean skip_insecure_parts)
{
	CamelSession *session;
	EComposerHeaderTable *table;
	EMailPartList *part_list = NULL;
	gchar *restore_lc_messages = NULL, *restore_lc_time = NULL;
	gchar *text, *forward, *subject;
	guint32 validity_found = 0;
	guint32 flags;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	session = e_msg_composer_ref_session (composer);

	flags = E_MAIL_FORMATTER_QUOTE_FLAG_HEADERS |
		E_MAIL_FORMATTER_QUOTE_FLAG_KEEP_SIG |
		(skip_insecure_parts ? E_MAIL_FORMATTER_QUOTE_FLAG_SKIP_INSECURE_PARTS : 0);
	if (style == E_MAIL_FORWARD_STYLE_QUOTED)
		flags |= E_MAIL_FORMATTER_QUOTE_FLAG_CITE;
	if (e_html_editor_get_mode (e_msg_composer_get_editor (composer)) != E_CONTENT_EDITOR_MODE_HTML)
		flags |= E_MAIL_FORMATTER_QUOTE_FLAG_NO_FORMATTING;

	/* Setup composer's From account before calling quoting_text() and
	   forward subject, because both rely on that account. */
	set_up_new_composer (composer, NULL, folder, message, uid, FALSE);

	forward = quoting_text (QUOTING_FORWARD, composer, &restore_lc_messages, &restore_lc_time);
	text = em_utils_message_to_html_ex (session, message, forward, flags, NULL, NULL, NULL, &validity_found, &part_list);
	emcu_restore_locale_after_attribution (restore_lc_messages, restore_lc_time);

	e_msg_composer_add_attachments_from_part_list (composer, part_list, FALSE);

	/* Read the Subject after EMFormatter, because it can update
	   it from an encrypted part */
	subject = emcu_generate_forward_subject (composer, message, NULL);
	table = e_msg_composer_get_header_table (composer);
	e_composer_header_table_set_subject (table, subject);
	g_free (subject);

	if (text != NULL) {
		e_msg_composer_set_body_text (composer, text, TRUE);

		emu_add_composer_references_from_message (composer, message);

		emu_set_source_headers (composer, folder, uid, CAMEL_MESSAGE_FORWARDED);

		emu_update_composers_security (composer, validity_found);
		e_msg_composer_check_inline_attachments (composer);
		composer_set_no_change (composer);
		gtk_widget_show (GTK_WIDGET (composer));

		g_free (text);
	}

	g_clear_object (&session);
	g_clear_object (&part_list);
	g_free (forward);
}

/**
 * em_utils_forward_message:
 * @composer: an #EMsgComposer
 * @message: a #CamelMimeMessage to forward
 * @style: the forward style to use
 * @folder: (nullable):  a #CamelFolder, or %NULL
 * @uid: (nullable): the UID of %message, or %NULL
 * @skip_insecure_parts: whether to not quote insecure parts
 *
 * Forwards @message in the given @style.
 *
 * If @style is #E_MAIL_FORWARD_STYLE_ATTACHED, the new message is
 * created as follows.  If there is more than a single message in @uids,
 * a multipart/digest will be constructed and attached to a new composer
 * window preset with the appropriate header defaults for forwarding the
 * first message in the list.  If only one message is to be forwarded,
 * it is forwarded as a simple message/rfc822 attachment.
 *
 * If @style is #E_MAIL_FORWARD_STYLE_INLINE, each message is forwarded
 * in its own composer window in 'inline' form.
 *
 * If @style is #E_MAIL_FORWARD_STYLE_QUOTED, each message is forwarded
 * in its own composer window in 'quoted' form (each line starting with
 * a "> ").
 **/
void
em_utils_forward_message (EMsgComposer *composer,
                          CamelMimeMessage *message,
                          EMailForwardStyle style,
                          CamelFolder *folder,
                          const gchar *uid,
			  gboolean skip_insecure_parts)
{
	CamelMimePart *part;
	GPtrArray *uids = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	e_msg_composer_set_is_reply_or_forward (composer, TRUE);

	switch (style) {
		case E_MAIL_FORWARD_STYLE_ATTACHED:
		default:
			part = mail_tool_make_message_attachment (message);

			if (folder && uid) {
				uids = g_ptr_array_new ();
				g_ptr_array_add (uids, (gpointer) uid);
			}

			em_utils_forward_attachment (composer, part, camel_mime_message_get_subject (message), uids ? folder : NULL, uids);

			g_object_unref (part);
			break;

		case E_MAIL_FORWARD_STYLE_INLINE:
		case E_MAIL_FORWARD_STYLE_QUOTED:
			forward_non_attached (composer, folder, uid, message, style, skip_insecure_parts);
			break;
	}

	if (uids)
		g_ptr_array_unref (uids);
}

void
em_utils_forward_attachment (EMsgComposer *composer,
                             CamelMimePart *part,
                             const gchar *orig_subject,
                             CamelFolder *folder,
                             GPtrArray *uids)
{
	GSettings *settings;
	CamelDataWrapper *content;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_PART (part));

	if (folder != NULL)
		g_return_if_fail (CAMEL_IS_FOLDER (folder));

	e_msg_composer_set_is_reply_or_forward (composer, TRUE);

	set_up_new_composer (composer, NULL, folder, NULL,
		uids && uids->len > 0 ? g_ptr_array_index (uids, 0) : NULL, FALSE);

	if (orig_subject) {
		gchar *subject;

		subject = emcu_generate_forward_subject (composer, NULL, orig_subject);
		e_composer_header_table_set_subject (e_msg_composer_get_header_table (composer), subject);
		g_free (subject);
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	content = camel_medium_get_content (CAMEL_MEDIUM (part));

	if (uids != NULL && uids->len > 1 && CAMEL_IS_MULTIPART (content) &&
	    g_settings_get_boolean (settings, "composer-attach-separate-messages")) {
		CamelMultipart *multipart;
		guint ii, nparts;

		multipart = CAMEL_MULTIPART (content);
		nparts = camel_multipart_get_number (multipart);

		for (ii = 0; ii < nparts; ii++) {
			CamelMimePart *mpart;
			gchar *mime_type;

			mpart = camel_multipart_get_part (multipart, ii);
			mime_type = camel_data_wrapper_get_mime_type (CAMEL_DATA_WRAPPER (mpart));

			if (mime_type && g_ascii_strcasecmp (mime_type, "message/rfc822") == 0) {
				CamelDataWrapper *mpart_content;

				mpart_content = camel_medium_get_content (CAMEL_MEDIUM (mpart));

				if (CAMEL_IS_MIME_MESSAGE (mpart_content))
					e_msg_composer_attach (composer, mpart);
			}

			g_free (mime_type);
		}
	} else {
		e_msg_composer_attach (composer, part);
	}

	g_clear_object (&settings);

	if (CAMEL_IS_MIME_MESSAGE (content)) {
		emu_add_composer_references_from_message (composer, CAMEL_MIME_MESSAGE (content));
	} else if (CAMEL_IS_MULTIPART (content)) {
		gchar *mime_type;

		mime_type = camel_data_wrapper_get_mime_type (content);
		if (mime_type && g_ascii_strcasecmp (mime_type, "multipart/digest") == 0) {
			/* This is the way evolution forwards multiple messages as attachment */
			CamelMultipart *multipart;
			guint ii, nparts;

			multipart = CAMEL_MULTIPART (content);
			nparts = camel_multipart_get_number (multipart);

			for (ii = 0; ii < nparts; ii++) {
				CamelMimePart *mpart;

				g_free (mime_type);

				mpart = camel_multipart_get_part (multipart, ii);
				mime_type = camel_data_wrapper_get_mime_type (CAMEL_DATA_WRAPPER (mpart));

				if (mime_type && g_ascii_strcasecmp (mime_type, "message/rfc822") == 0) {
					content = camel_medium_get_content (CAMEL_MEDIUM (mpart));

					if (CAMEL_IS_MIME_MESSAGE (content))
						emu_add_composer_references_from_message (composer, CAMEL_MIME_MESSAGE (content));
				}
			}
		}

		g_free (mime_type);
	}

	if (uids != NULL)
		setup_forward_attached_callbacks (composer, folder, uids);

	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));
}

/**
 * em_utils_redirect_message:
 * @composer: an #EMsgComposer
 * @message: message to redirect
 *
 * Sets up the @composer to redirect @message (Note: only headers will be
 * editable). Adds Resent-From/Resent-To/etc headers.
 *
 * Since: 3.22
 **/
void
em_utils_redirect_message (EMsgComposer *composer,
                           CamelMimeMessage *message)
{
	ESource *source;
	EShell *shell;
	CamelMedium *medium;
	gchar *identity_uid = NULL, *alias_name = NULL, *alias_address = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	shell = e_msg_composer_get_shell (composer);
	medium = CAMEL_MEDIUM (message);

	/* QMail will refuse to send a message if it finds one of
	 * it's Delivered-To headers in the message, so remove all
	 * Delivered-To headers. Fixes bug #23635. */
	while (camel_medium_get_header (medium, "Delivered-To"))
		camel_medium_remove_header (medium, "Delivered-To");

	while (camel_medium_get_header (medium, "Bcc"))
		camel_medium_remove_header (medium, "Bcc");

	while (camel_medium_get_header (medium, "Resent-Bcc"))
		camel_medium_remove_header (medium, "Resent-Bcc");

	source = em_composer_utils_guess_identity_source (shell, message, NULL, NULL, &alias_name, &alias_address);

	if (source != NULL) {
		identity_uid = e_source_dup_uid (source);
		g_object_unref (source);
	}

	e_msg_composer_setup_redirect (composer, message, identity_uid, alias_name, alias_address, NULL);

	g_free (identity_uid);
	g_free (alias_name);
	g_free (alias_address);

	gtk_widget_show (GTK_WIDGET (composer));

	composer_set_no_change (composer);
}

/* Replying to messages... */

EDestination **
em_utils_camel_address_to_destination (CamelInternetAddress *iaddr)
{
	EDestination *dest, **destv;
	gint n, i, j;

	if (iaddr == NULL)
		return NULL;

	if ((n = camel_address_length ((CamelAddress *) iaddr)) == 0)
		return NULL;

	destv = g_malloc (sizeof (EDestination *) * (n + 1));
	for (i = 0, j = 0; i < n; i++) {
		const gchar *name, *addr;

		if (camel_internet_address_get (iaddr, i, &name, &addr)) {
			dest = e_destination_new ();
			e_destination_set_name (dest, name);
			e_destination_set_email (dest, addr);

			destv[j++] = dest;
		}
	}

	if (j == 0) {
		g_free (destv);
		return NULL;
	}

	destv[j] = NULL;

	return destv;
}

static gchar *
emcu_construct_reply_subject (EMsgComposer *composer,
			      const gchar *source_subject)
{
	gchar *res;

	if (source_subject) {
		GSettings *settings;
		gboolean skip_len = -1;

		if (em_utils_is_re_in_subject (source_subject, &skip_len, NULL, NULL) && skip_len > 0)
			source_subject = source_subject + skip_len;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		if (g_settings_get_boolean (settings, "composer-use-localized-fwd-re")) {
			gchar *restore_lc_messages = NULL, *restore_lc_time = NULL;

			if (composer) {
				ESource *identity_source;

				identity_source = emcu_ref_identity_source_from_composer (composer);

				emcu_prepare_attribution_locale (identity_source, &restore_lc_messages, &restore_lc_time);

				g_clear_object (&identity_source);
			}

			/* Translators: This is a reply attribution in the message reply subject. The %s is replaced with the subject of the original message. Both 'Re'-s in the 'reply-attribution' translation context should translate into the same string, the same as the ':' separator. */
			res = g_strdup_printf (C_("reply-attribution", "Re: %s"), source_subject);

			emcu_restore_locale_after_attribution (restore_lc_messages, restore_lc_time);
		} else {
			/* Do not localize this string */
			res = g_strdup_printf ("Re: %s", source_subject);
		}
		g_clear_object (&settings);
	} else {
		res = g_strdup ("");
	}

	return res;
}

static void
reply_setup_composer_recipients (EMsgComposer *composer,
				 CamelInternetAddress *to,
				 CamelInternetAddress *cc,
				 CamelFolder *folder,
				 const gchar *message_uid,
				 CamelNNTPAddress *postto)
{
	EComposerHeaderTable *table;
	EDestination **tov, **ccv;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	if (to != NULL)
		g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (to));

	if (cc != NULL)
		g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (cc));

	/* Construct the tov/ccv */
	tov = em_utils_camel_address_to_destination (to);
	ccv = em_utils_camel_address_to_destination (cc);

	table = e_msg_composer_get_header_table (composer);
	e_composer_header_table_set_destinations_to (table, tov);

	/* Add destinations instead of setting, so we don't remove
	 * automatic CC addresses that have already been added. */
	e_composer_header_table_add_destinations_cc (table, ccv);

	e_destination_freev (tov);
	e_destination_freev (ccv);

	/* Add post-to, if necessary */
	if (postto && camel_address_length ((CamelAddress *) postto)) {
		CamelFolder *use_folder = folder, *temp_folder = NULL;
		gchar *folder_uri = NULL;
		gchar *post;

		if (use_folder && CAMEL_IS_VEE_FOLDER (use_folder) && message_uid) {
			em_utils_get_real_folder_and_message_uid (use_folder, message_uid, &temp_folder, NULL, NULL);

			if (temp_folder)
				use_folder = temp_folder;
		}

		if (use_folder)
			folder_uri = e_mail_folder_uri_from_folder (use_folder);

		post = camel_address_encode ((CamelAddress *) postto);
		e_composer_header_table_set_post_to_base (
			table, folder_uri ? folder_uri : "", post);
		g_free (post);
		g_free (folder_uri);
		g_clear_object (&temp_folder);
	}
}

static void
reply_setup_composer (EMsgComposer *composer,
		      CamelMimeMessage *message,
		      const gchar *identity_uid,
		      const gchar *identity_name,
		      const gchar *identity_address,
		      CamelInternetAddress *to,
		      CamelInternetAddress *cc,
		      CamelFolder *folder,
		      const gchar *message_uid,
		      CamelNNTPAddress *postto)
{
	gchar *message_id, *references;
	EComposerHeaderTable *table;
	CamelMedium *medium;
	gchar *subject;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	e_msg_composer_set_is_reply_or_forward (composer, TRUE);

	if (to != NULL)
		g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (to));

	if (cc != NULL)
		g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (cc));

	reply_setup_composer_recipients (composer, to, cc, folder, message_uid, postto);

	table = e_msg_composer_get_header_table (composer);
	e_composer_header_table_set_identity_uid (table, identity_uid, identity_name, identity_address);

	/* Set the subject of the new message. */
	subject = emcu_construct_reply_subject (composer, camel_mime_message_get_subject (message));
	e_composer_header_table_set_subject (table, subject);
	g_free (subject);

	/* Add In-Reply-To and References. */

	medium = CAMEL_MEDIUM (message);
	message_id = camel_header_unfold (camel_medium_get_header (medium, "Message-ID"));
	references = camel_header_unfold (camel_medium_get_header (medium, "References"));

	if (message_id != NULL) {
		gchar *reply_refs;

		e_msg_composer_add_header (
			composer, "In-Reply-To", message_id);

		if (references)
			reply_refs = g_strdup_printf (
				"%s %s", references, message_id);
		else
			reply_refs = g_strdup (message_id);

		e_msg_composer_add_header (
			composer, "References", reply_refs);
		g_free (reply_refs);

	} else if (references != NULL) {
		e_msg_composer_add_header (
			composer, "References", references);
	}

	g_free (message_id);
	g_free (references);
}

static gboolean
get_reply_list (CamelMimeMessage *message,
                CamelInternetAddress *to)
{
	const gchar *header, *p;
	gchar *addr;

	/* Examples:
	 *
	 * List-Post: <mailto:list@host.com>
	 * List-Post: <mailto:moderator@host.com?subject=list%20posting>
	 * List-Post: NO (posting not allowed on this list)
	 */
	if (!(header = camel_medium_get_header ((CamelMedium *) message, "List-Post")))
		return FALSE;

	while (*header == ' ' || *header == '\t')
		header++;

	/* check for NO */
	if (!g_ascii_strncasecmp (header, "NO", 2))
		return FALSE;

	/* Search for the first mailto angle-bracket enclosed URL.
	 * (See rfc2369, Section 2, paragraph 3 for details) */
	if (!(header = camel_strstrcase (header, "<mailto:")))
		return FALSE;

	header += 8;

	p = header;
	while (*p && !strchr ("?>", *p))
		p++;

	addr = g_strndup (header, p - header);
	camel_internet_address_add (to, NULL, addr);
	g_free (addr);

	return TRUE;
}

gboolean
em_utils_is_munged_list_message (CamelMimeMessage *message)
{
	CamelInternetAddress *reply_to, *list;
	gboolean result = FALSE;

	reply_to = camel_mime_message_get_reply_to (message);
	if (reply_to) {
		list = camel_internet_address_new ();

		if (get_reply_list (message, list) &&
		    camel_address_length (CAMEL_ADDRESS (list)) ==
		    camel_address_length (CAMEL_ADDRESS (reply_to))) {
			gint i;
			const gchar *r_name, *r_addr;
			const gchar *l_name, *l_addr;

			for (i = 0; i < camel_address_length (CAMEL_ADDRESS (list)); i++) {
				if (!camel_internet_address_get (reply_to, i, &r_name, &r_addr))
					break;
				if (!camel_internet_address_get (list, i, &l_name, &l_addr))
					break;
				if (strcmp (l_addr, r_addr))
					break;
			}
			if (i == camel_address_length (CAMEL_ADDRESS (list)))
				result = TRUE;
		}
		g_object_unref (list);
	}
	return result;
}

static CamelInternetAddress *
get_reply_to (CamelMimeMessage *message)
{
	CamelInternetAddress *reply_to;

	reply_to = camel_mime_message_get_reply_to (message);
	if (reply_to) {
		GSettings *settings;
		gboolean ignore_list_reply_to;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		ignore_list_reply_to = g_settings_get_boolean (
			settings, "composer-ignore-list-reply-to");
		g_object_unref (settings);

		if (ignore_list_reply_to && em_utils_is_munged_list_message (message))
			reply_to = NULL;
	}
	if (!reply_to)
		reply_to = camel_mime_message_get_from (message);

	return reply_to;
}

static void
get_reply_sender (CamelMimeMessage *message,
                  CamelInternetAddress *to,
                  CamelNNTPAddress *postto)
{
	CamelInternetAddress *reply_to = NULL;
	CamelMedium *medium;
	const gchar *posthdr = NULL;
	const gchar *mail_reply_to;

	medium = CAMEL_MEDIUM (message);

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Followup-To");

	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Newsgroups");

	if (postto != NULL && posthdr != NULL) {
		camel_address_decode (CAMEL_ADDRESS (postto), posthdr);
		return;
	}

	/* Prefer Mail-Reply-To for Reply-Sender, if exists */
	mail_reply_to = camel_medium_get_header (medium, "Mail-Reply-To");
	if (mail_reply_to && *mail_reply_to) {
		reply_to = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (reply_to), mail_reply_to);

		if (!camel_address_length (CAMEL_ADDRESS (reply_to))) {
			g_clear_object (&reply_to);
		}
	}

	if (!reply_to) {
		reply_to = get_reply_to (message);

		if (reply_to)
			g_object_ref (reply_to);
	}

	if (reply_to != NULL) {
		const gchar *name;
		const gchar *addr;
		gint ii = 0;

		while (camel_internet_address_get (reply_to, ii++, &name, &addr))
			camel_internet_address_add (to, name, addr);

		g_object_unref (reply_to);
	}
}

void
em_utils_get_reply_sender (CamelMimeMessage *message,
                           CamelInternetAddress *to,
                           CamelNNTPAddress *postto)
{
	get_reply_sender (message, to, postto);
}

static void
get_reply_from (CamelMimeMessage *message,
                CamelInternetAddress *to,
                CamelNNTPAddress *postto)
{
	CamelInternetAddress *from;
	CamelMedium *medium;
	const gchar *name, *addr;
	const gchar *posthdr = NULL;

	medium = CAMEL_MEDIUM (message);

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Followup-To");

	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Newsgroups");

	if (postto != NULL && posthdr != NULL) {
		camel_address_decode (CAMEL_ADDRESS (postto), posthdr);
		return;
	}

	from = camel_mime_message_get_from (message);

	if (from != NULL) {
		gint ii = 0;

		while (camel_internet_address_get (from, ii++, &name, &addr))
			camel_internet_address_add (to, name, addr);
	}
}

static void
get_reply_recipient (CamelMimeMessage *message,
                     CamelInternetAddress *to,
                     CamelNNTPAddress *postto,
                     CamelInternetAddress *address)
{
	CamelMedium *medium;
	const gchar *posthdr = NULL;

	medium = CAMEL_MEDIUM (message);

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Followup-To");

	if (postto != NULL && posthdr == NULL)
		 posthdr = camel_medium_get_header (medium, "Newsgroups");

	if (postto != NULL && posthdr != NULL) {
		camel_address_decode (CAMEL_ADDRESS (postto), posthdr);
		return;
	}

	if (address != NULL) {
		const gchar *name;
		const gchar *addr;
		gint ii = 0;

		while (camel_internet_address_get (address, ii++, &name, &addr))
			camel_internet_address_add (to, name, addr);
	}

}

static void
concat_unique_addrs (CamelInternetAddress *dest,
                     CamelInternetAddress *src,
                     GHashTable *rcpt_hash)
{
	const gchar *name, *addr;
	gint i;

	for (i = 0; camel_internet_address_get (src, i, &name, &addr); i++) {
		if (!g_hash_table_contains (rcpt_hash, addr)) {
			camel_internet_address_add (dest, name, addr);
			g_hash_table_insert (rcpt_hash, g_strdup (addr), NULL);
		}
	}
}

static void
add_source_to_recipient_hash (ESourceRegistry *registry,
			      GHashTable *rcpt_hash,
			      const gchar *address,
			      ESource *source,
			      gboolean source_is_default)
{
	ESource *cached_source;
	gboolean insert_source;

	g_return_if_fail (rcpt_hash != NULL);
	g_return_if_fail (E_IS_SOURCE (source));

	if (!address || !*address)
		return;

	cached_source = g_hash_table_lookup (rcpt_hash, address);

	insert_source = source_is_default || !cached_source;

	if (insert_source)
		g_hash_table_insert (rcpt_hash, g_strdup (address), g_object_ref (source));
}

static void
unref_nonull_object (gpointer ptr)
{
	if (ptr)
		g_object_unref (ptr);
}

static GHashTable *
generate_recipient_hash (ESourceRegistry *registry)
{
	GHashTable *rcpt_hash;
	ESource *default_source;
	GList *list, *link;
	const gchar *extension_name;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	rcpt_hash = g_hash_table_new_full (
		camel_strcase_hash,
		camel_strcase_equal,
		g_free, unref_nonull_object);

	default_source = e_source_registry_ref_default_mail_identity (registry);

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceMailIdentity *extension;
		GHashTable *aliases;
		const gchar *address;
		gboolean source_is_default;

		/* No default mail identity implies there are no mail
		 * identities at all and so we should never get here. */
		g_warn_if_fail (default_source != NULL);

		if (!e_source_registry_check_enabled (registry, source))
			continue;

		source_is_default = e_source_equal (source, default_source);

		extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
		extension = e_source_get_extension (source, extension_name);

		address = e_source_mail_identity_get_address (extension);

		add_source_to_recipient_hash (registry, rcpt_hash, address, source, source_is_default);

		aliases = e_source_mail_identity_get_aliases_as_hash_table (extension);
		if (aliases) {
			GHashTableIter iter;
			gpointer key;

			g_hash_table_iter_init (&iter, aliases);
			while (g_hash_table_iter_next (&iter, &key, NULL)) {
				address = key;

				add_source_to_recipient_hash (registry, rcpt_hash, address, source, source_is_default);
			}

			g_hash_table_destroy (aliases);
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (default_source != NULL)
		g_object_unref (default_source);

	return rcpt_hash;
}

void
em_utils_get_reply_all (ESourceRegistry *registry,
                        CamelMimeMessage *message,
                        CamelInternetAddress *to,
                        CamelInternetAddress *cc,
                        CamelNNTPAddress *postto)
{
	CamelInternetAddress *reply_to = NULL;
	CamelInternetAddress *to_addrs = NULL;
	CamelInternetAddress *cc_addrs = NULL;
	CamelMedium *medium;
	const gchar *name, *addr;
	const gchar *posthdr = NULL;
	const gchar *mail_followup_to;
	GHashTable *rcpt_hash;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (to));
	g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (cc));

	medium = CAMEL_MEDIUM (message);

	/* check whether there is a 'Newsgroups: ' header in there */
	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Followup-To");

	if (postto != NULL && posthdr == NULL)
		posthdr = camel_medium_get_header (medium, "Newsgroups");

	if (postto != NULL && posthdr != NULL)
		camel_address_decode (CAMEL_ADDRESS (postto), posthdr);

	rcpt_hash = generate_recipient_hash (registry);

	/* Prefer Mail-Followup-To for Reply-All, if exists */
	mail_followup_to = camel_medium_get_header (medium, "Mail-Followup-To");
	if (mail_followup_to && *mail_followup_to) {
		to_addrs = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (to_addrs), mail_followup_to);

		if (!camel_address_length (CAMEL_ADDRESS (to_addrs))) {
			g_clear_object (&to_addrs);
		}
	}

	if (to_addrs == NULL) {
		reply_to = get_reply_to (message);
		to_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
		cc_addrs = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);

		g_object_ref (to_addrs);
	}

	if (reply_to != NULL) {
		gint ii = 0;

		while (camel_internet_address_get (reply_to, ii++, &name, &addr)) {
			/* Ignore references to the Reply-To address
			 * in the To and Cc lists. */
			if (addr && !g_hash_table_contains (rcpt_hash, addr)) {
				/* In the case we are doing a Reply-To-All,
				 * we do not want to include the user's email
				 * address because replying to oneself is
				 * kinda silly. */
				camel_internet_address_add (to, name, addr);
				g_hash_table_insert (rcpt_hash, g_strdup (addr), NULL);
			}
		}
	}

	if (to_addrs)
		concat_unique_addrs (to, to_addrs, rcpt_hash);
	if (cc_addrs)
		concat_unique_addrs (cc, cc_addrs, rcpt_hash);

	/* Set as the 'To' the first 'Reply-To' address, if such exists, when no address
	   had been picked (like when all addresses are configured mail accounts). */
	if (reply_to &&
	    camel_address_length ((CamelAddress *) to) == 0 &&
	    camel_internet_address_get (reply_to, 0, &name, &addr)) {
		camel_internet_address_add (to, name, addr);
	}

	/* Promote the first Cc: address to To: if To: is empty. */
	if (camel_address_length ((CamelAddress *) to) == 0 &&
	    camel_address_length ((CamelAddress *) cc) > 0) {
		if (camel_internet_address_get (cc, 0, &name, &addr))
			camel_internet_address_add (to, name, addr);
		camel_address_remove ((CamelAddress *) cc, 0);
	}

	/* If To: is still empty, may we removed duplicates (i.e. ourself),
	 * so add the original To if it was set. */
	if (camel_address_length ((CamelAddress *) to) == 0
	    && (camel_internet_address_get (to_addrs, 0, &name, &addr)
		|| camel_internet_address_get (cc_addrs, 0, &name, &addr))) {
		camel_internet_address_add (to, name, addr);
	}

	g_hash_table_destroy (rcpt_hash);
	g_clear_object (&to_addrs);
}

enum {
	ATTRIB_UNKNOWN,
	ATTRIB_CUSTOM,
	ATTRIB_TIMEZONE,
	ATTRIB_STRFTIME,
	ATTRIB_TM_SEC,
	ATTRIB_TM_MIN,
	ATTRIB_TM_24HOUR,
	ATTRIB_TM_12HOUR,
	ATTRIB_TM_MDAY,
	ATTRIB_TM_MON,
	ATTRIB_TM_YEAR,
	ATTRIB_TM_2YEAR,
	ATTRIB_TM_WDAY, /* not actually used */
	ATTRIB_TM_YDAY
};

typedef void		(*AttribFormatter)	(GString *str,
						 const gchar *attr,
						 CamelMimeMessage *message);

static void
format_sender (GString *str,
               const gchar *attr,
               CamelMimeMessage *message)
{
	CamelInternetAddress *sender;
	const gchar *name, *addr = NULL;
	gchar *tmp = NULL;

	sender = camel_mime_message_get_from (message);
	if (sender != NULL && camel_address_length (CAMEL_ADDRESS (sender)) > 0) {
		name = NULL;

		if (camel_internet_address_get (sender, 0, &name, &addr)) {
			if (name && !*name) {
				name = NULL;
			} else if (name && *name == '\"') {
				gint len = strlen (name);

				if (len == 1) {
					name = NULL;
				} else if (len > 1 && name[len - 1] == '\"') {
					if (len == 2) {
						name = NULL;
					} else {
						tmp = g_strndup (name + 1, len - 2);
						name = tmp;
					}
				}
			}
		}
	} else {
		name = _("an unknown sender");
	}

	if (name && !strcmp (attr, "{SenderName}")) {
		g_string_append (str, name);
	} else if (addr && !strcmp (attr, "{SenderEMail}")) {
		g_string_append (str, addr);
	} else if (name && *name) {
		g_string_append (str, name);
	} else if (addr) {
		g_string_append (str, addr);
	}

	g_free (tmp);
}

static struct {
	const gchar *name;
	gint type;
	struct {
		const gchar *format;         /* strftime or printf format */
		AttribFormatter formatter;  /* custom formatter */
	} v;
} attribvars[] = {
	{ "{Sender}", ATTRIB_CUSTOM, { NULL, format_sender } },
	{ "{SenderName}", ATTRIB_CUSTOM, { NULL, format_sender } },
	{ "{SenderEMail}", ATTRIB_CUSTOM, { NULL, format_sender } },
	{ "{AbbrevWeekdayName}", ATTRIB_STRFTIME, { "%a", NULL } },
	{ "{WeekdayName}", ATTRIB_STRFTIME, { "%A", NULL } },
	{ "{AbbrevMonthName}", ATTRIB_STRFTIME, { "%b", NULL } },
	{ "{MonthName}", ATTRIB_STRFTIME, { "%B", NULL } },
	{ "{AmPmUpper}", ATTRIB_STRFTIME, { "%p", NULL } },
	{ "{AmPmLower}", ATTRIB_STRFTIME, { "%P", NULL } },
	{ "{Day}", ATTRIB_TM_MDAY, { "%02d", NULL } },  /* %d  01-31 */
	{ "{ Day}", ATTRIB_TM_MDAY, { "% 2d", NULL } },  /* %e   1-31 */
	{ "{24Hour}", ATTRIB_TM_24HOUR, { "%02d", NULL } },  /* %H  00-23 */
	{ "{12Hour}", ATTRIB_TM_12HOUR, { "%02d", NULL } },  /* %I  00-12 */
	{ "{DayOfYear}", ATTRIB_TM_YDAY, { "%d", NULL } },  /* %j  1-366 */
	{ "{Month}", ATTRIB_TM_MON, { "%02d", NULL } },  /* %m  01-12 */
	{ "{Minute}", ATTRIB_TM_MIN, { "%02d", NULL } },  /* %M  00-59 */
	{ "{Seconds}", ATTRIB_TM_SEC, { "%02d", NULL } },  /* %S  00-61 */
	{ "{2DigitYear}", ATTRIB_TM_2YEAR, { "%02d", NULL } },  /* %y */
	{ "{Year}", ATTRIB_TM_YEAR, { "%04d", NULL } },  /* %Y */
	{ "{TimeZone}", ATTRIB_TIMEZONE, { "%+05d", NULL } }
};

gchar *
em_composer_utils_get_reply_credits (ESource *identity_source,
				     CamelMimeMessage *message)
{
	register const gchar *inptr;
	const gchar *start;
	gint tzone, len, i;
	gchar buf[64];
	GString *str;
	struct tm tm;
	time_t date;
	gint type;
	gchar *format, *restore_lc_messages = NULL, *restore_lc_time = NULL;

	emcu_prepare_attribution_locale (identity_source, &restore_lc_messages, &restore_lc_time);

	format = quoting_text (QUOTING_ATTRIBUTION, NULL, NULL, NULL);
	str = g_string_new ("");

	date = camel_mime_message_get_date (message, &tzone);

	if (date == CAMEL_MESSAGE_DATE_CURRENT) {
		/* The message has no Date: header, look at Received: */
		date = camel_mime_message_get_date_received (message, &tzone);
	}
	if (date == CAMEL_MESSAGE_DATE_CURRENT) {
		/* That didn't work either, use current time */
		time (&date);
		tzone = 0;
	} else {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");

		if ((tzone == 0 && g_settings_get_boolean (settings, "composer-reply-credits-utc-to-localtime")) ||
		    g_settings_get_boolean (settings, "composer-reply-credits-to-localtime")) {
			struct tm local;
			gint offset = 0;

			e_localtime_with_offset (date, &local, &offset);

			tzone = offset / 3600;
			tzone = (tzone * 100) + ((offset / 60) % 60);
		}

		g_clear_object (&settings);
	}

	/* Convert to UTC */
	date += (tzone / 100) * 60 * 60;
	date += (tzone % 100) * 60;

	gmtime_r (&date, &tm);

	inptr = format;
	while (*inptr != '\0') {
		start = inptr;
		while (*inptr && strncmp (inptr, "${", 2) != 0)
			inptr++;

		g_string_append_len (str, start, inptr - start);

		if (*inptr == '\0')
			break;

		start = ++inptr;
		while (*inptr && *inptr != '}')
			inptr++;

		if (*inptr != '}') {
			/* broken translation */
			g_string_append_len (str, "${", 2);
			inptr = start + 1;
			continue;
		}

		inptr++;
		len = inptr - start;
		type = ATTRIB_UNKNOWN;
		for (i = 0; i < G_N_ELEMENTS (attribvars); i++) {
			if (!strncmp (attribvars[i].name, start, len)) {
				type = attribvars[i].type;
				break;
			}
		}

		switch (type) {
		case ATTRIB_CUSTOM:
			attribvars[i].v.formatter (
				str, attribvars[i].name, message);
			break;
		case ATTRIB_TIMEZONE:
			g_string_append_printf (
				str, attribvars[i].v.format, tzone);
			break;
		case ATTRIB_STRFTIME:
			e_utf8_strftime_match_lc_messages (
				buf, sizeof (buf), attribvars[i].v.format, &tm);
			g_string_append (str, buf);
			break;
		case ATTRIB_TM_SEC:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_sec);
			break;
		case ATTRIB_TM_MIN:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_min);
			break;
		case ATTRIB_TM_24HOUR:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_hour);
			break;
		case ATTRIB_TM_12HOUR:
			g_string_append_printf (
				str, attribvars[i].v.format,
				(tm.tm_hour + 1) % 13);
			break;
		case ATTRIB_TM_MDAY:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_mday);
			break;
		case ATTRIB_TM_MON:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_mon + 1);
			break;
		case ATTRIB_TM_YEAR:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_year + 1900);
			break;
		case ATTRIB_TM_2YEAR:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_year % 100);
			break;
		case ATTRIB_TM_WDAY:
			/* not actually used */
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_wday);
			break;
		case ATTRIB_TM_YDAY:
			g_string_append_printf (
				str, attribvars[i].v.format, tm.tm_yday + 1);
			break;
		default:
			/* Misspelled variable?  Drop the
			 * format argument and continue. */
			break;
		}
	}

	emcu_restore_locale_after_attribution (restore_lc_messages, restore_lc_time);

	g_free (format);

	return g_string_free (str, FALSE);
}

static void
composer_set_body (EMsgComposer *composer,
                   CamelMimeMessage *message,
                   EMailReplyStyle style,
		   gboolean skip_insecure_parts,
                   EMailPartList *parts_list,
		   EMailPartList **out_used_part_list)
{
	gchar *text, *credits, *original;
	ESource *identity_source;
	CamelMimePart *part;
	CamelSession *session;
	GSettings *settings;
	guint32 validity_found = 0, add_flags = 0;
	gchar *restore_lc_messages = NULL, *restore_lc_time = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (settings, "composer-reply-keep-signature"))
		add_flags = E_MAIL_FORMATTER_QUOTE_FLAG_KEEP_SIG;
	g_clear_object (&settings);

	if (skip_insecure_parts)
		add_flags |= E_MAIL_FORMATTER_QUOTE_FLAG_SKIP_INSECURE_PARTS;

	session = e_msg_composer_ref_session (composer);

	switch (style) {
	case E_MAIL_REPLY_STYLE_DO_NOT_QUOTE:
		/* do nothing */
		break;
	case E_MAIL_REPLY_STYLE_ATTACH:
		/* attach the original message as an attachment */
		part = mail_tool_make_message_attachment (message);
		e_msg_composer_attach (composer, part);
		g_object_unref (part);
		break;
	case E_MAIL_REPLY_STYLE_OUTLOOK:
		original = quoting_text (QUOTING_ORIGINAL, composer, &restore_lc_messages, &restore_lc_time);
		text = em_utils_message_to_html_ex (
			session, message, original, E_MAIL_FORMATTER_QUOTE_FLAG_HEADERS | add_flags,
			parts_list, NULL, NULL, &validity_found, out_used_part_list);
		emcu_restore_locale_after_attribution (restore_lc_messages, restore_lc_time);
		e_msg_composer_set_body_text (composer, text, TRUE);
		g_free (text);
		g_free (original);
		emu_update_composers_security (composer, validity_found);
		break;

	case E_MAIL_REPLY_STYLE_QUOTED:
	default:
		identity_source = emcu_ref_identity_source_from_composer (composer);

		/* do what any sane user would want when replying... */
		credits = em_composer_utils_get_reply_credits (identity_source, message);

		g_clear_object (&identity_source);

		text = em_utils_message_to_html_ex (
			session, message, credits, E_MAIL_FORMATTER_QUOTE_FLAG_CITE | add_flags,
			parts_list, NULL, NULL, &validity_found, out_used_part_list);
		g_free (credits);
		e_msg_composer_set_body_text (composer, text, TRUE);
		g_free (text);
		emu_update_composers_security (composer, validity_found);
		break;
	}

	g_object_unref (session);
}

static gboolean
emcu_folder_is_inbox (CamelFolder *folder)
{
	CamelSession *session;
	CamelStore *store;
	gboolean is_inbox = FALSE;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	store = camel_folder_get_parent_store (folder);
	if (!store)
		return FALSE;

	session = camel_service_ref_session (CAMEL_SERVICE (store));
	if (!session)
		return FALSE;

	if (E_IS_MAIL_SESSION (session)) {
		MailFolderCache *folder_cache;
		CamelFolderInfoFlags flags = 0;

		folder_cache = e_mail_session_get_folder_cache (E_MAIL_SESSION (session));
		if (folder_cache && mail_folder_cache_get_folder_info_flags (
			folder_cache, store, camel_folder_get_full_name (folder), &flags)) {
			is_inbox = (flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX;
		}
	}

	g_object_unref (session);

	return is_inbox;
}

typedef struct _AltReplyContext {
	EShell *shell;
	EAlertSink *alert_sink;
	CamelMimeMessage *source_message;
	CamelFolder *folder;
	gchar *message_uid;
	CamelMimeMessage *new_message; /* When processed with a template */
	EMailPartList *source;
	EMailReplyType type;
	EMailReplyStyle style;
	guint32 flags;
	gboolean template_preserve_subject;
	EMailPartValidityFlags validity_pgp_sum;
	EMailPartValidityFlags validity_smime_sum;
} AltReplyContext;

static void
alt_reply_context_free (gpointer ptr)
{
	AltReplyContext *context = ptr;

	if (context) {
		g_clear_object (&context->shell);
		g_clear_object (&context->alert_sink);
		g_clear_object (&context->source_message);
		g_clear_object (&context->folder);
		g_clear_object (&context->source);
		g_clear_object (&context->new_message);
		g_free (context->message_uid);
		g_slice_free (AltReplyContext, context);
	}
}

static guint32
get_composer_mark_read_on_reply_flag (void)
{
	GSettings *settings;
	guint32 res = 0;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	if (g_settings_get_boolean (settings, "composer-mark-read-on-reply"))
		res = CAMEL_MESSAGE_SEEN;

	g_object_unref (settings);

	return res;
}

static void
alt_reply_composer_created_cb (GObject *source_object,
			       GAsyncResult *result,
			       gpointer user_data)
{
	AltReplyContext *context = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (context != NULL);

	composer = e_msg_composer_new_finish (result, &error);

	if (composer) {
		if (context->new_message) {
			CamelInternetAddress *to = NULL, *cc = NULL;
			CamelNNTPAddress *postto = NULL;

			if (context->template_preserve_subject) {
				gchar *subject;

				subject = emcu_construct_reply_subject (composer, camel_mime_message_get_subject (context->source_message));
				camel_mime_message_set_subject (context->new_message, subject);
				g_free (subject);
			}

			em_utils_edit_message (composer, context->folder, context->new_message, context->message_uid, TRUE, FALSE);

			to = camel_internet_address_new ();
			cc = camel_internet_address_new ();

			if (context->folder)
				postto = camel_nntp_address_new ();

			em_utils_get_reply_recipients (e_shell_get_registry (context->shell), context->source_message, context->type, NULL, to, cc, postto);

			if (postto && !camel_address_length (CAMEL_ADDRESS (postto)))
				g_clear_object (&postto);

			reply_setup_composer_recipients (composer, to, cc, context->folder, context->message_uid, postto);
			e_msg_composer_check_autocrypt (composer, context->source_message);

			composer_set_no_change (composer);

			g_clear_object (&to);
			g_clear_object (&cc);
			g_clear_object (&postto);

			if (context->folder && context->message_uid) {
				emu_set_source_headers (composer, context->folder, context->message_uid,
					CAMEL_MESSAGE_ANSWERED | get_composer_mark_read_on_reply_flag ());
			}
		} else {
			em_utils_reply_to_message (composer, context->source_message,
				context->folder, context->message_uid, context->type, context->style,
				context->source, NULL, context->flags | E_MAIL_REPLY_FLAG_FORCE_SENDER_REPLY);
		}

		em_composer_utils_update_security (composer, context->validity_pgp_sum, context->validity_smime_sum);
	} else {
		e_alert_submit (context->alert_sink, "mail-composer:failed-create-composer",
			error ? error->message : _("Unknown error"), NULL);
	}

	alt_reply_context_free (context);
	g_clear_error (&error);
}

static void
alt_reply_template_applied_cb (GObject *source_object,
			       GAsyncResult *result,
			       gpointer user_data)
{
	AltReplyContext *context = user_data;
	GError *error = NULL;

	g_return_if_fail (context != NULL);

	context->new_message = e_mail_templates_apply_finish (source_object, result, &error);

	if (context->new_message) {
		e_msg_composer_new (context->shell, alt_reply_composer_created_cb, context);
	} else {
		e_alert_submit (context->alert_sink, "mail:no-retrieve-message",
			error ? error->message : _("Unknown error"), NULL);
		alt_reply_context_free (context);
	}

	g_clear_error (&error);
}

static void
emcu_three_state_toggled_cb (GtkToggleButton *widget,
			     gpointer user_data)
{
	glong *phandlerid = user_data;

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (widget));
	g_return_if_fail (phandlerid != NULL);

	g_signal_handler_block (widget, *phandlerid);

	if (gtk_toggle_button_get_inconsistent (widget) &&
	    gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_active (widget, FALSE);
		gtk_toggle_button_set_inconsistent (widget, FALSE);
	} else if (!gtk_toggle_button_get_active (widget)) {
		gtk_toggle_button_set_inconsistent (widget, TRUE);
		gtk_toggle_button_set_active (widget, FALSE);
	} else {
	}

	g_signal_handler_unblock (widget, *phandlerid);
}

static void
emcu_connect_three_state_changer (GtkToggleButton *toggle_button)
{
	glong *phandlerid;

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

	phandlerid = g_new0 (glong, 1);

	*phandlerid = g_signal_connect_data (toggle_button, "toggled",
		G_CALLBACK (emcu_three_state_toggled_cb),
		phandlerid, (GClosureNotify) g_free, 0);
}

static void
emcu_three_state_set_value (GtkToggleButton *toggle_button,
			    EThreeState value)
{
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button));

	if (value == E_THREE_STATE_OFF) {
		gtk_toggle_button_set_active (toggle_button, FALSE);
		gtk_toggle_button_set_inconsistent (toggle_button, FALSE);
	} else if (value == E_THREE_STATE_ON) {
		gtk_toggle_button_set_active (toggle_button, TRUE);
		gtk_toggle_button_set_inconsistent (toggle_button, FALSE);
	} else {
		gtk_toggle_button_set_active (toggle_button, FALSE);
		gtk_toggle_button_set_inconsistent (toggle_button, TRUE);
	}
}

static EThreeState
emcu_three_state_get_value (GtkToggleButton *toggle_button)
{
	g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (toggle_button), E_THREE_STATE_INCONSISTENT);

	if (gtk_toggle_button_get_inconsistent (toggle_button))
		return E_THREE_STATE_INCONSISTENT;
	else if (gtk_toggle_button_get_active (toggle_button))
		return E_THREE_STATE_ON;

	return E_THREE_STATE_OFF;
}

static GtkComboBox *
emcu_create_templates_combo (EShell *shell,
			     const gchar *folder_uri,
			     const gchar *message_uid)
{
	GtkComboBox *combo;
	GtkCellRenderer *renderer;
	EShellBackend *shell_backend;
	EMailSession *mail_session;
	EMailTemplatesStore *templates_store;
	GtkTreeStore *tree_store;
	GtkTreeIter found_iter;
	gboolean found_message = FALSE;

	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	g_return_val_if_fail (E_IS_MAIL_BACKEND (shell_backend), NULL);

	mail_session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));
	templates_store = e_mail_templates_store_ref_default (e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (mail_session)));

	tree_store = e_mail_templates_store_build_model (templates_store, folder_uri, message_uid, &found_message, &found_iter);

	combo = GTK_COMBO_BOX (gtk_combo_box_new_with_model (GTK_TREE_MODEL (tree_store)));

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer),
		"ellipsize", PANGO_ELLIPSIZE_END,
		"single-paragraph-mode", TRUE,
		NULL);

	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
		"text", E_MAIL_TEMPLATES_STORE_COLUMN_DISPLAY_NAME,
		NULL);

	g_clear_object (&templates_store);
	g_clear_object (&tree_store);

	if (found_message) {
		gtk_combo_box_set_active_iter (combo, &found_iter);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (combo), FALSE);
	}

	return combo;
}

static void
emcu_add_editor_mode_unknown (EActionComboBox *mode_combo)
{
	EUIAction *existing_action, *new_action;
	GPtrArray *existing_radio_group;

	existing_action = e_action_combo_box_get_action (mode_combo);
	existing_radio_group = e_ui_action_get_radio_group (existing_action);

	new_action = e_ui_action_new_stateful (e_ui_action_get_map_name (existing_action),
		"unknown", G_VARIANT_TYPE_INT32, g_variant_new_int32 (E_CONTENT_EDITOR_MODE_UNKNOWN));
	e_ui_action_set_label (new_action, _("Use global setting"));
	e_ui_action_set_radio_group (new_action, existing_radio_group);
	e_ui_action_set_action_group (new_action, e_ui_action_get_action_group (existing_action));

	g_object_unref (new_action);
}

/**
 * em_utils_reply_alternative:
 * @parent: (nullable): a parent #GtkWindow for the question dialog
 * @shell: an #EShell instance used to create #EMsgComposer
 * @alert_sink: an #EAlertSink to put any errors to
 * @message: a #CamelMimeMessage
 * @folder: (nullable): a #CamelFolder, or %NULL
 * @message_uid: (nullable): the UID of @message, or %NULL
 * @style: the reply style to use
 * @source: (nullable): source to inherit view settings from
 * @validity_pgp_sum: a bit-or of #EMailPartValidityFlags for PGP from original message part list
 * @validity_smime_sum: a bit-or of #EMailPartValidityFlags for S/MIME from original message part list
 * @skip_insecure_parts: whether to not quote insecure parts
 *
 * This is similar to em_utils_reply_to_message(), except it asks user to
 * change some settings before sending. It calls em_utils_reply_to_message()
 * at the end for non-templated replies.
 *
 * Since: 3.30
 **/
void
em_utils_reply_alternative (GtkWindow *parent,
			    EShell *shell,
			    EAlertSink *alert_sink,
			    CamelMimeMessage *message,
			    CamelFolder *folder,
			    const gchar *message_uid,
			    EMailReplyStyle default_style,
			    EMailPartList *source,
			    EMailPartValidityFlags validity_pgp_sum,
			    EMailPartValidityFlags validity_smime_sum,
			    gboolean skip_insecure_parts)
{
	GtkWidget *dialog, *widget, *style_label;
	GtkBox *hbox, *vbox;
	GtkGrid *grid;
	GtkRadioButton *recip_sender, *recip_list, *recip_all;
	GtkLabel *sender_label, *list_label, *all_label;
	GtkRadioButton *style_default, *style_attach, *style_inline, *style_quote, *style_no_quote;
	EActionComboBox *mode_combo;
	GtkToggleButton *bottom_posting;
	GtkToggleButton *top_signature;
	GtkCheckButton *apply_template;
	GtkComboBox *templates;
	GtkCheckButton *preserve_message_subject;
	PangoAttrList *attr_list;
	GSettings *settings;
	gchar *last_tmpl_folder_uri, *last_tmpl_message_uid, *address, *text;
	gboolean can_reply_list = FALSE;
	CamelInternetAddress *to, *cc;
	CamelNNTPAddress *postto = NULL;
	gint n_addresses;
	guint row = 0;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	dialog = gtk_dialog_new_with_buttons (_("Alternative Reply"), parent,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Reply"), GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	vbox = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_column_spacing (grid, 2);
	gtk_grid_set_row_spacing (grid, 2);
	gtk_box_pack_start (vbox, GTK_WIDGET (grid), TRUE, TRUE, 0);

	#define add_with_indent(x, _cols) \
		gtk_widget_set_margin_start (GTK_WIDGET (x), 12); \
		gtk_grid_attach (grid, GTK_WIDGET (x), 0, row, (_cols), 1); \
		row++;

	widget = gtk_label_new (_("Recipients:"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		NULL);
	gtk_grid_attach (grid, widget, 0, row, 1, 1);
	row++;

	attr_list = pango_attr_list_new ();
	pango_attr_list_insert (attr_list, pango_attr_style_new (PANGO_STYLE_ITALIC));

	#define add_with_label(wgt, lbl, _cols) G_STMT_START { \
		GtkWidget *divider_label = gtk_label_new (":"); \
		gtk_label_set_attributes (GTK_LABEL (lbl), attr_list); \
		hbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6)); \
		gtk_box_pack_start (hbox, GTK_WIDGET (wgt), FALSE, FALSE, 0); \
		gtk_box_pack_start (hbox, GTK_WIDGET (divider_label), FALSE, FALSE, 0); \
		gtk_box_pack_start (hbox, GTK_WIDGET (lbl), FALSE, FALSE, 0); \
		e_binding_bind_property ( \
			wgt, "sensitive", \
			divider_label, "visible", \
			G_BINDING_SYNC_CREATE); \
		e_binding_bind_property ( \
			wgt, "sensitive", \
			lbl, "visible", \
			G_BINDING_SYNC_CREATE); \
		add_with_indent (hbox, (_cols)); } G_STMT_END

	recip_sender = GTK_RADIO_BUTTON (gtk_radio_button_new_with_mnemonic (
		NULL, _("Reply to _Sender")));
	sender_label = GTK_LABEL (gtk_label_new (""));
	add_with_label (recip_sender, sender_label, 2);

	recip_list = GTK_RADIO_BUTTON (gtk_radio_button_new_with_mnemonic (
		gtk_radio_button_get_group (recip_sender), _("Reply to _List")));
	list_label = GTK_LABEL (gtk_label_new (""));
	add_with_label (recip_list, list_label, 2);

	recip_all = GTK_RADIO_BUTTON (gtk_radio_button_new_with_mnemonic (
		gtk_radio_button_get_group (recip_sender), _("Reply to _All")));
	all_label = GTK_LABEL (gtk_label_new (""));
	add_with_label (recip_all, all_label, 2);

	#undef add_with_label

	pango_attr_list_unref (attr_list);

	/* One line gap between sections */
	widget = gtk_label_new (" ");
	gtk_grid_attach (grid, widget, 0, row, 1, 1);
	row++;

	style_label = gtk_label_new (_("Reply style:"));
	g_object_set (G_OBJECT (style_label),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		NULL);
	gtk_grid_attach (grid, style_label, 0, row, 1, 1);
	row++;

	style_default = GTK_RADIO_BUTTON (gtk_radio_button_new_with_mnemonic (
		NULL, _("_Default")));
	add_with_indent (style_default, 1);

	style_attach = GTK_RADIO_BUTTON (gtk_radio_button_new_with_mnemonic (
		gtk_radio_button_get_group (style_default), _("Attach_ment")));
	add_with_indent (style_attach, 1);

	style_inline = GTK_RADIO_BUTTON (gtk_radio_button_new_with_mnemonic (
		gtk_radio_button_get_group (style_default), _("Inline (_Outlook style)")));
	add_with_indent (style_inline, 1);

	style_quote = GTK_RADIO_BUTTON (gtk_radio_button_new_with_mnemonic (
		gtk_radio_button_get_group (style_default), _("_Quote")));
	add_with_indent (style_quote, 1);

	style_no_quote = GTK_RADIO_BUTTON (gtk_radio_button_new_with_mnemonic (
		gtk_radio_button_get_group (style_default), _("Do _Not Quote")));
	add_with_indent (style_no_quote, 1);

	bottom_posting = GTK_TOGGLE_BUTTON (gtk_check_button_new_with_mnemonic (_("Start _typing at the bottom")));
	g_object_set (bottom_posting, "margin-start", 12, NULL);
	gtk_grid_attach (grid, GTK_WIDGET (bottom_posting), 1, row - 5, 1, 1);

	top_signature = GTK_TOGGLE_BUTTON (gtk_check_button_new_with_mnemonic (_("_Keep signature above the original message")));
	g_object_set (top_signature, "margin-start", 12, NULL);
	gtk_grid_attach (grid, GTK_WIDGET (top_signature), 1, row - 4, 1, 1);

	/* One line gap between sections */
	widget = gtk_label_new (" ");
	gtk_grid_attach (grid, widget, 0, row, 1, 1);
	row++;

	hbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6));
	gtk_grid_attach (grid, GTK_WIDGET (hbox), 0, row, 2, 1);
	row++;

	/* Translators: The text is followed by the format combo with values like 'Plain Text', 'HTML' and so on */
	widget = gtk_label_new_with_mnemonic (_("_Format message in"));
	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

	mode_combo = e_html_editor_util_new_mode_combobox ();
	gtk_label_set_mnemonic_widget (GTK_LABEL (widget), GTK_WIDGET (mode_combo));
	gtk_box_pack_start (hbox, GTK_WIDGET (mode_combo), FALSE, FALSE, 0);

	e_binding_bind_property (
		mode_combo, "sensitive",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE);

	emcu_add_editor_mode_unknown (mode_combo);
	e_action_combo_box_update_model (mode_combo);

	/* One line gap between sections */
	widget = gtk_label_new (" ");
	gtk_grid_attach (grid, widget, 0, row, 1, 1);
	row++;

	apply_template = GTK_CHECK_BUTTON (gtk_check_button_new_with_mnemonic (_("Apply t_emplate")));
	gtk_grid_attach (grid, GTK_WIDGET (apply_template), 0, row, 2, 1);
	row++;

	last_tmpl_folder_uri = g_settings_get_string (settings, "alt-reply-template-folder-uri");
	last_tmpl_message_uid = g_settings_get_string (settings, "alt-reply-template-message-uid");

	templates = emcu_create_templates_combo (shell, last_tmpl_folder_uri, last_tmpl_message_uid);
	add_with_indent (templates, 2);

	g_free (last_tmpl_folder_uri);
	g_free (last_tmpl_message_uid);

	preserve_message_subject = GTK_CHECK_BUTTON (gtk_check_button_new_with_mnemonic (_("Preserve original message S_ubject")));
	add_with_indent (preserve_message_subject, 2);

	#undef add_with_indent

	gtk_widget_show_all (GTK_WIDGET (vbox));

	#define populate_label_with_text(lbl, txt) \
		g_object_set (G_OBJECT (lbl), \
			"ellipsize", PANGO_ELLIPSIZE_END, \
			"max-width-chars", 50, \
			"label", txt ? txt : "", \
			"tooltip-text", txt ? txt : "", \
			NULL);

	/* Reply to sender */
	to = camel_internet_address_new ();
	if (folder)
		postto = camel_nntp_address_new ();
	get_reply_sender (message, to, postto);

	if (postto && camel_address_length (CAMEL_ADDRESS (postto)) > 0) {
		address = camel_address_format (CAMEL_ADDRESS (postto));
	} else {
		address = camel_address_format (CAMEL_ADDRESS (to));
	}

	populate_label_with_text (sender_label, address);

	g_clear_object (&postto);
	g_clear_object (&to);
	g_free (address);

	/* Reply to list */
	to = camel_internet_address_new ();

	if (!get_reply_list (message, to)) {
		gtk_widget_set_sensitive (GTK_WIDGET (recip_list), FALSE);
	} else {
		can_reply_list = TRUE;

		address = camel_address_format (CAMEL_ADDRESS (to));
		populate_label_with_text (list_label, address);
		g_free (address);
	}

	g_clear_object (&to);

	/* Reply to all */
	to = camel_internet_address_new ();
	cc = camel_internet_address_new ();

	if (folder)
		postto = camel_nntp_address_new ();

	em_utils_get_reply_all (e_shell_get_registry (shell), message, to, cc, postto);

	if (postto && camel_address_length (CAMEL_ADDRESS (postto)) > 0) {
		n_addresses = camel_address_length (CAMEL_ADDRESS (postto));
		address = camel_address_format (CAMEL_ADDRESS (postto));
	} else {
		camel_address_cat (CAMEL_ADDRESS (to), CAMEL_ADDRESS (cc));
		n_addresses = camel_address_length (CAMEL_ADDRESS (to));
		address = camel_address_format (CAMEL_ADDRESS (to));
	}

	text = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "one recipient", "%d recipients", n_addresses), n_addresses);
	populate_label_with_text (all_label, text);
	gtk_widget_set_tooltip_text (GTK_WIDGET (all_label), address);

	g_clear_object (&to);
	g_clear_object (&cc);
	g_clear_object (&postto);
	g_free (address);
	g_free (text);

	#undef populate_label_with_text

	/* Prefer reply-to-list */
	if (g_settings_get_boolean (settings, "composer-group-reply-to-list")) {
		if (can_reply_list)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (recip_list), TRUE);
		else if (n_addresses > 1)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (recip_all), TRUE);
		else
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (recip_sender), TRUE);

	/* Prefer reply-to-all */
	} else {
		if (n_addresses > 1)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (recip_all), TRUE);
		else if (can_reply_list)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (recip_list), TRUE);
		else
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (recip_sender), TRUE);
	}

	switch (g_settings_get_enum (settings, "alt-reply-style")) {
	case E_MAIL_REPLY_STYLE_UNKNOWN:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (style_default), TRUE);
		break;
	case E_MAIL_REPLY_STYLE_QUOTED:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (style_quote), TRUE);
		break;
	case E_MAIL_REPLY_STYLE_DO_NOT_QUOTE:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (style_no_quote), TRUE);
		break;
	case E_MAIL_REPLY_STYLE_ATTACH:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (style_attach), TRUE);
		break;
	case E_MAIL_REPLY_STYLE_OUTLOOK:
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (style_inline), TRUE);
		break;
	}

	e_action_combo_box_set_current_value (mode_combo, g_settings_get_enum (settings, "alt-reply-format-mode"));
	emcu_three_state_set_value (bottom_posting, g_settings_get_enum (settings, "alt-reply-start-bottom"));
	emcu_three_state_set_value (top_signature, g_settings_get_enum (settings, "alt-reply-top-signature"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (apply_template), g_settings_get_boolean (settings, "alt-reply-template-apply"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (preserve_message_subject), g_settings_get_boolean (settings, "alt-reply-template-preserve-subject"));

	if (!gtk_widget_get_sensitive (GTK_WIDGET (templates)))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (apply_template), FALSE);

	emcu_connect_three_state_changer (bottom_posting);
	emcu_connect_three_state_changer (top_signature);

	e_binding_bind_property (
		apply_template, "active",
		mode_combo, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	e_binding_bind_property (
		apply_template, "active",
		templates, "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		apply_template, "active",
		preserve_message_subject, "sensitive",
		G_BINDING_SYNC_CREATE);

	/* Enable the 'Reply Style' section only if not using Template */
	e_binding_bind_property (
		apply_template, "active",
		style_label, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	e_binding_bind_property (
		apply_template, "active",
		style_default, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	e_binding_bind_property (
		apply_template, "active",
		style_attach, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	e_binding_bind_property (
		apply_template, "active",
		style_inline, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	e_binding_bind_property (
		apply_template, "active",
		style_quote, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	e_binding_bind_property (
		apply_template, "active",
		style_no_quote, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	/* Similarly with other options, which don't work when using Templates */
	e_binding_bind_property (
		apply_template, "active",
		bottom_posting, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	e_binding_bind_property (
		apply_template, "active",
		top_signature, "sensitive",
		G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		GtkTreeIter iter;
		AltReplyContext *context;
		EContentEditorMode mode;
		EThreeState three_state;
		CamelFolder *template_folder = NULL;
		gchar *template_message_uid = NULL;

		context = g_slice_new0 (AltReplyContext);
		context->shell = g_object_ref (shell);
		context->alert_sink = g_object_ref (alert_sink);
		context->source_message = g_object_ref (message);
		context->folder = folder ? g_object_ref (folder) : NULL;
		context->source = source ? g_object_ref (source) : NULL;
		context->message_uid = g_strdup (message_uid);
		context->style = E_MAIL_REPLY_STYLE_UNKNOWN;
		context->flags = E_MAIL_REPLY_FLAG_FORCE_STYLE;
		context->validity_pgp_sum = validity_pgp_sum;
		context->validity_smime_sum = validity_smime_sum;

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (style_quote)))
			context->style = E_MAIL_REPLY_STYLE_QUOTED;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (style_no_quote)))
			context->style = E_MAIL_REPLY_STYLE_DO_NOT_QUOTE;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (style_attach)))
			context->style = E_MAIL_REPLY_STYLE_ATTACH;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (style_inline)))
			context->style = E_MAIL_REPLY_STYLE_OUTLOOK;
		else
			context->flags = context->flags & (~E_MAIL_REPLY_FLAG_FORCE_STYLE);

		mode = e_action_combo_box_get_current_value (mode_combo);
		g_settings_set_enum (settings, "alt-reply-format-mode", mode);

		switch (mode) {
		case E_CONTENT_EDITOR_MODE_UNKNOWN:
			break;
		case E_CONTENT_EDITOR_MODE_PLAIN_TEXT:
			context->flags |= E_MAIL_REPLY_FLAG_FORMAT_PLAIN;
			break;
		case E_CONTENT_EDITOR_MODE_HTML:
			context->flags |= E_MAIL_REPLY_FLAG_FORMAT_HTML;
			break;
		case E_CONTENT_EDITOR_MODE_MARKDOWN:
			context->flags |= E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN;
			break;
		case E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT:
			context->flags |= E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_PLAIN;
			break;
		case E_CONTENT_EDITOR_MODE_MARKDOWN_HTML:
			context->flags |= E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_HTML;
			break;
		}

		three_state = emcu_three_state_get_value (bottom_posting);
		g_settings_set_enum (settings, "alt-reply-start-bottom", three_state);

		if (three_state == E_THREE_STATE_ON)
			context->flags |= E_MAIL_REPLY_FLAG_BOTTOM_POSTING;
		else if (three_state == E_THREE_STATE_OFF)
			context->flags |= E_MAIL_REPLY_FLAG_TOP_POSTING;

		three_state = emcu_three_state_get_value (top_signature);
		g_settings_set_enum (settings, "alt-reply-top-signature", three_state);

		if (three_state == E_THREE_STATE_ON)
			context->flags |= E_MAIL_REPLY_FLAG_TOP_SIGNATURE;
		else if (three_state == E_THREE_STATE_OFF)
			context->flags |= E_MAIL_REPLY_FLAG_BOTTOM_SIGNATURE;

		if (skip_insecure_parts)
			context->flags |= E_MAIL_REPLY_FLAG_SKIP_INSECURE_PARTS;

		g_settings_set_enum (settings, "alt-reply-style", context->style);
		g_settings_set_boolean (settings, "alt-reply-template-apply", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (apply_template)));
		g_settings_set_boolean (settings, "alt-reply-template-preserve-subject", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (preserve_message_subject)));

		if (gtk_combo_box_get_active_iter (templates, &iter)) {
			gtk_tree_model_get (gtk_combo_box_get_model (templates), &iter,
				E_MAIL_TEMPLATES_STORE_COLUMN_FOLDER, &template_folder,
				E_MAIL_TEMPLATES_STORE_COLUMN_MESSAGE_UID, &template_message_uid,
				-1);
		}

		if (template_folder) {
			gchar *folder_uri;

			folder_uri = e_mail_folder_uri_from_folder (template_folder);
			g_settings_set_string (settings, "alt-reply-template-folder-uri", folder_uri ? folder_uri : "");
			g_free (folder_uri);
		}

		g_settings_set_string (settings, "alt-reply-template-message-uid", template_message_uid ? template_message_uid : "");

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (recip_sender)))
			context->type = E_MAIL_REPLY_TO_SENDER;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (recip_list)))
			context->type = E_MAIL_REPLY_TO_LIST;
		else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (recip_all)))
			context->type = E_MAIL_REPLY_TO_ALL;
		else
			g_warn_if_reached ();

		if (context->style == E_MAIL_REPLY_STYLE_UNKNOWN)
			context->style = default_style;

		context->template_preserve_subject = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (preserve_message_subject));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (apply_template))) {
			e_mail_templates_apply (context->source_message, context->folder, message_uid, template_folder, template_message_uid,
				NULL, alt_reply_template_applied_cb, context);

		} else {
			e_msg_composer_new (context->shell, alt_reply_composer_created_cb, context);
		}

		g_clear_object (&template_folder);
		g_free (template_message_uid);
	}

	gtk_widget_destroy (dialog);
	g_clear_object (&settings);
}

static void
em_utils_update_by_reply_flags (EContentEditor *cnt_editor,
				guint32 reply_flags) /* EMailReplyFlags */
{
	if ((reply_flags & (E_MAIL_REPLY_FLAG_TOP_POSTING | E_MAIL_REPLY_FLAG_BOTTOM_POSTING)) != 0) {
		e_content_editor_set_start_bottom (cnt_editor,
			(reply_flags & E_MAIL_REPLY_FLAG_TOP_POSTING) != 0 ?
			E_THREE_STATE_OFF : E_THREE_STATE_ON);
	}

	if ((reply_flags & (E_MAIL_REPLY_FLAG_TOP_SIGNATURE | E_MAIL_REPLY_FLAG_BOTTOM_SIGNATURE)) != 0) {
		e_content_editor_set_top_signature (cnt_editor,
			(reply_flags & E_MAIL_REPLY_FLAG_TOP_SIGNATURE) != 0 ?
			E_THREE_STATE_ON : E_THREE_STATE_OFF);
	}
}

void
em_utils_get_reply_recipients (ESourceRegistry *registry,
			       CamelMimeMessage *message,
			       EMailReplyType reply_type,
			       CamelInternetAddress *address,
			       CamelInternetAddress *inout_to,
			       CamelInternetAddress *inout_cc,
			       CamelNNTPAddress *inout_postto)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));
	g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (inout_to));
	g_return_if_fail (CAMEL_IS_INTERNET_ADDRESS (inout_cc));

	switch (reply_type) {
	case E_MAIL_REPLY_TO_FROM:
		get_reply_from (message, inout_to, inout_postto);
		break;
	case E_MAIL_REPLY_TO_RECIPIENT:
		get_reply_recipient (message, inout_to, inout_postto, address);
		break;
	case E_MAIL_REPLY_TO_SENDER:
		get_reply_sender (message, inout_to, inout_postto);
		break;
	case E_MAIL_REPLY_TO_LIST:
		if (get_reply_list (message, inout_to))
			break;
		/* falls through */
	case E_MAIL_REPLY_TO_ALL:
		em_utils_get_reply_all (registry, message, inout_to, inout_cc, inout_postto);
		break;
	default:
		g_warn_if_reached ();
		break;
	}
}

/**
 * em_utils_reply_to_message:
 * @composer: an #EMsgComposer
 * @message: a #CamelMimeMessage
 * @folder: a #CamelFolder, or %NULL
 * @message_uid: the UID of @message, or %NULL
 * @type: the type of reply to create
 * @style: the reply style to use
 * @source: source to inherit view settings from
 * @address: used for E_MAIL_REPLY_TO_RECIPIENT @type
 * @reply_flags: bit-or of #EMailReplyFlags
 *
 * Creates a new composer ready to reply to @message.
 *
 * @folder and @message_uid may be supplied in order to update the message
 * flags once it has been replied to.
 **/
void
em_utils_reply_to_message (EMsgComposer *composer,
                           CamelMimeMessage *message,
                           CamelFolder *folder,
                           const gchar *message_uid,
                           EMailReplyType type,
                           EMailReplyStyle style,
                           EMailPartList *parts_list,
			   CamelInternetAddress *address,
			   EMailReplyFlags reply_flags)
{
	ESourceRegistry *registry;
	CamelInternetAddress *to, *cc;
	CamelNNTPAddress *postto = NULL;
	EShell *shell;
	ESourceMailCompositionReplyStyle prefer_reply_style = E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_DEFAULT;
	ESource *source;
	EMailPartList *used_part_list = NULL;
	EContentEditor *cnt_editor;
	gchar *identity_uid = NULL, *identity_name = NULL, *identity_address = NULL;
	guint32 flags;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	cnt_editor = e_html_editor_get_content_editor (e_msg_composer_get_editor (composer));

	flags = reply_flags & (E_MAIL_REPLY_FLAG_FORMAT_PLAIN |
				E_MAIL_REPLY_FLAG_FORMAT_HTML |
				E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN |
				E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_PLAIN |
				E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_HTML);
	if (flags != 0) {
		EContentEditorMode mode = E_CONTENT_EDITOR_MODE_UNKNOWN;

		switch (flags) {
		case E_MAIL_REPLY_FLAG_FORMAT_PLAIN:
			mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;
			break;
		case E_MAIL_REPLY_FLAG_FORMAT_HTML:
			mode = E_CONTENT_EDITOR_MODE_HTML;
			break;
		case E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN:
			mode = E_CONTENT_EDITOR_MODE_MARKDOWN;
			break;
		case E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_PLAIN:
			mode = E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT;
			break;
		case E_MAIL_REPLY_FLAG_FORMAT_MARKDOWN_HTML:
			mode = E_CONTENT_EDITOR_MODE_MARKDOWN_HTML;
			break;
		}

		if (mode != E_CONTENT_EDITOR_MODE_UNKNOWN)
			e_html_editor_set_mode (e_msg_composer_get_editor (composer), mode);
	}

	em_utils_update_by_reply_flags (cnt_editor, reply_flags);

	to = camel_internet_address_new ();
	cc = camel_internet_address_new ();

	shell = e_msg_composer_get_shell (composer);
	registry = e_shell_get_registry (shell);

	if (type == E_MAIL_REPLY_TO_SENDER &&
	    !(reply_flags & E_MAIL_REPLY_FLAG_FORCE_SENDER_REPLY) &&
	    em_utils_sender_is_user (registry, message, TRUE)) {
		type = E_MAIL_REPLY_TO_ALL;
	}

	source = em_composer_utils_guess_identity_source (shell, message, folder, message_uid, &identity_name, &identity_address);

	if (source != NULL) {
		identity_uid = e_source_dup_uid (source);
		if (!(reply_flags & E_MAIL_REPLY_FLAG_FORCE_STYLE) &&
		    e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION)) {
			ESourceMailComposition *extension;

			extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
			prefer_reply_style = e_source_mail_composition_get_reply_style (extension);
		}

		g_object_unref (source);
	}

	flags = CAMEL_MESSAGE_ANSWERED | get_composer_mark_read_on_reply_flag ();

	if (!address && (type == E_MAIL_REPLY_TO_FROM || type == E_MAIL_REPLY_TO_SENDER) &&
	    folder && !emcu_folder_is_inbox (folder) && em_utils_folder_is_sent (registry, folder))
		type = E_MAIL_REPLY_TO_ALL;

	if (folder)
		postto = camel_nntp_address_new ();

	em_utils_get_reply_recipients (registry, message, type, address, to, cc, postto);

	if (postto && !camel_address_length (CAMEL_ADDRESS (postto)))
		g_clear_object (&postto);

	if (type == E_MAIL_REPLY_TO_LIST || type == E_MAIL_REPLY_TO_ALL)
		flags |= CAMEL_MESSAGE_ANSWERED_ALL;

	reply_setup_composer (composer, message, identity_uid, identity_name, identity_address, to, cc, folder, message_uid, postto);

	if (postto)
		g_object_unref (postto);
	g_object_unref (to);
	g_object_unref (cc);

	/* If there was no send-account override */
	if (!identity_uid) {
		EComposerHeaderTable *header_table;
		gchar *used_identity_uid;

		header_table = e_msg_composer_get_header_table (composer);
		used_identity_uid = e_composer_header_table_dup_identity_uid (header_table, NULL, NULL);

		if (used_identity_uid) {
			source = e_source_registry_ref_source (registry, used_identity_uid);
			if (source) {
				if (!(reply_flags & E_MAIL_REPLY_FLAG_FORCE_STYLE) &&
				    e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION)) {
					ESourceMailComposition *extension;

					extension = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_COMPOSITION);
					prefer_reply_style = e_source_mail_composition_get_reply_style (extension);
				}

				g_object_unref (source);
			}
		}

		g_free (used_identity_uid);
	}

	switch (prefer_reply_style) {
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_DEFAULT:
			/* Do nothing, keep the passed-in reply style. */
			break;
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_QUOTED:
			style = E_MAIL_REPLY_STYLE_QUOTED;
			break;
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_DO_NOT_QUOTE:
			style = E_MAIL_REPLY_STYLE_DO_NOT_QUOTE;
			break;
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_ATTACH:
			style = E_MAIL_REPLY_STYLE_ATTACH;
			break;
		case E_SOURCE_MAIL_COMPOSITION_REPLY_STYLE_OUTLOOK:
			style = E_MAIL_REPLY_STYLE_OUTLOOK;
			break;
	}

	composer_set_body (composer, message, style, (reply_flags & E_MAIL_REPLY_FLAG_SKIP_INSECURE_PARTS) != 0, parts_list, &used_part_list);

	e_msg_composer_add_attachments_from_part_list (composer, used_part_list, TRUE);
	g_clear_object (&used_part_list);

	if (folder)
		emu_set_source_headers (composer, folder, message_uid, flags);

	/* because some reply types can change recipients after the composer is populated */
	em_utils_apply_send_account_override_to_composer (composer, folder);

	/* This is required to be done (also) at the end */
	em_utils_update_by_reply_flags (cnt_editor, reply_flags);
	e_msg_composer_check_autocrypt (composer, message);

	composer_set_no_change (composer);

	gtk_widget_show (GTK_WIDGET (composer));

	g_free (identity_uid);
	g_free (identity_name);
	g_free (identity_address);
}

static void
post_header_clicked_cb (EComposerPostHeader *header,
                        EMailSession *session)
{
	GtkTreeSelection *selection;
	EMFolderSelector *selector;
	EMFolderTreeModel *model;
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	GList *list;
	const gchar *caption;

	/* FIXME Limit the folder tree to the NNTP account? */
	model = em_folder_tree_model_get_default ();

	dialog = em_folder_selector_new (
		/* FIXME GTK_WINDOW (composer) */ NULL, model);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Posting destination"));

	selector = EM_FOLDER_SELECTOR (dialog);
	em_folder_selector_set_can_create (selector, TRUE);

	caption = _("Choose folders to post the message to.");
	em_folder_selector_set_caption (selector, caption);

	folder_tree = em_folder_selector_get_folder_tree (selector);

	em_folder_tree_set_excluded (
		folder_tree,
		EMFT_EXCLUDE_NOSELECT |
		EMFT_EXCLUDE_VIRTUAL |
		EMFT_EXCLUDE_VTRASH);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	list = e_composer_post_header_get_folders (header);
	em_folder_tree_set_selected_list (folder_tree, list, FALSE);
	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK) {
		/* Prevent the header's "custom" flag from being reset,
		 * which is what the default method will do next. */
		g_signal_stop_emission_by_name (header, "clicked");
		goto exit;
	}

	list = em_folder_tree_get_selected_uris (folder_tree);
	e_composer_post_header_set_folders (header, list);
	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);

exit:
	gtk_widget_destroy (dialog);
}

/**
 * em_configure_new_composer:
 * @composer: a newly created #EMsgComposer
 *
 * Integrates a newly created #EMsgComposer into the mail backend.  The
 * composer can't link directly to the mail backend without introducing
 * circular library dependencies, so this function finishes configuring
 * things the #EMsgComposer instance can't do itself.
 **/
void
em_configure_new_composer (EMsgComposer *composer,
                           EMailSession *session)
{
	EComposerHeaderTable *table;
	EComposerHeaderType header_type;
	EComposerHeader *header;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));
	g_return_if_fail (E_IS_MAIL_SESSION (session));

	header_type = E_COMPOSER_HEADER_POST_TO;
	table = e_msg_composer_get_header_table (composer);
	header = e_composer_header_table_get_header (table, header_type);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_recipients), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_identity), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_plugins), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_subject), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_unwanted_html), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_attachments), session);

	g_signal_connect (
		composer, "presend",
		G_CALLBACK (composer_presend_check_autocrypt_wanted), session);

	g_signal_connect (
		composer, "send",
		G_CALLBACK (em_utils_composer_send_cb), session);

	g_signal_connect (
		composer, "save-to-drafts",
		G_CALLBACK (em_utils_composer_save_to_drafts_cb), session);

	g_signal_connect (
		composer, "save-to-outbox",
		G_CALLBACK (em_utils_composer_save_to_outbox_cb), session);

	g_signal_connect (
		composer, "print",
		G_CALLBACK (em_utils_composer_print_cb), session);

	/* Handle "Post To:" button clicks, which displays a folder tree
	 * widget.  The composer doesn't know about folder tree widgets,
	 * so it can't handle this itself.
	 *
	 * Note: This is a G_SIGNAL_RUN_LAST signal, which allows us to
	 *       stop the signal emission if the user cancels or closes
	 *       the folder selector dialog.  See the handler function. */
	g_signal_connect (
		header, "clicked",
		G_CALLBACK (post_header_clicked_cb), session);
}

/* free returned pointer with g_object_unref(), if not NULL */
ESource *
em_utils_check_send_account_override (EShell *shell,
                                      CamelMimeMessage *message,
                                      CamelFolder *folder,
				      gchar **out_alias_name,
				      gchar **out_alias_address)
{
	EMailBackend *mail_backend;
	EMailSendAccountOverride *account_override;
	CamelInternetAddress *to = NULL, *cc = NULL, *bcc = NULL;
	gchar *folder_uri = NULL, *account_uid, *alias_name = NULL, *alias_address = NULL;
	ESource *account_source = NULL;
	ESourceRegistry *source_registry;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	if (!message && !folder)
		return NULL;

	if (message) {
		to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
		cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
		bcc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);
	}

	mail_backend = E_MAIL_BACKEND (e_shell_get_backend_by_name (shell, "mail"));
	g_return_val_if_fail (mail_backend != NULL, NULL);

	if (folder)
		folder_uri = e_mail_folder_uri_from_folder (folder);

	source_registry = e_shell_get_registry (shell);
	account_override = e_mail_backend_get_send_account_override (mail_backend);
	account_uid = e_mail_send_account_override_get_account_uid (account_override, folder_uri, to, cc, bcc, &alias_name, &alias_address);

	while (account_uid) {
		account_source = e_source_registry_ref_source (source_registry, account_uid);
		if (account_source)
			break;

		/* stored send account override settings contain a reference
		 * to a dropped account, thus cleanup it now */
		e_mail_send_account_override_remove_for_account_uid (account_override, account_uid, alias_name, alias_address);

		g_free (account_uid);
		g_free (alias_name);
		g_free (alias_address);

		alias_name = NULL;
		alias_address = NULL;

		account_uid = e_mail_send_account_override_get_account_uid (account_override, folder_uri, to, cc, bcc, &alias_name, &alias_address);
	}

	if (out_alias_name)
		*out_alias_name = alias_name;
	else
		g_free (alias_name);

	if (out_alias_address)
		*out_alias_address = alias_address;
	else
		g_free (alias_address);

	g_free (folder_uri);
	g_free (account_uid);

	return account_source;
}

void
em_utils_apply_send_account_override_to_composer (EMsgComposer *composer,
                                                  CamelFolder *folder)
{
	CamelMimeMessage *message;
	EComposerHeaderTable *header_table;
	EShell *shell;
	ESource *source;
	gchar *alias_name = NULL, *alias_address = NULL;

	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	shell = e_msg_composer_get_shell (composer);
	message = em_utils_get_composer_recipients_as_message (composer);
	source = em_utils_check_send_account_override (shell, message, folder, &alias_name, &alias_address);
	g_clear_object (&message);

	if (!source)
		return;

	header_table = e_msg_composer_get_header_table (composer);
	e_composer_header_table_set_identity_uid (header_table, e_source_get_uid (source), alias_name, alias_address);

	g_object_unref (source);
	g_free (alias_name);
	g_free (alias_address);
}

void
em_utils_add_installed_languages (GtkComboBoxText *combo)
{
	const ESupportedLocales *supported_locales;
	GHashTable *locales;
	GList *langs = NULL, *link;
	gboolean has_en_us = FALSE;
	gint ii, n_langs = 0;

	g_return_if_fail (GTK_IS_COMBO_BOX_TEXT (combo));

	supported_locales = e_util_get_supported_locales ();
	locales = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (ii = 0; supported_locales[ii].code; ii++) {
		const gchar *locale = supported_locales[ii].locale;

		if (locale) {
			gchar *name;

			name = e_util_get_language_name (locale);

			if (name && *name) {
				g_hash_table_insert (locales, name, (gpointer) locale);
			} else {
				g_free (name);
				g_hash_table_insert (locales, g_strdup (locale), (gpointer) locale);
			}

			has_en_us = has_en_us || g_strcmp0 (locale, "en_US") == 0;
		}
	}

	if (!has_en_us) {
		const gchar *locale = "C";
		gchar *name = e_util_get_language_name ("en_US");

		if (name && *name) {
			g_hash_table_insert (locales, name, (gpointer) locale);
		} else {
			g_free (name);
			g_hash_table_insert (locales, g_strdup ("en_US"), (gpointer) locale);
		}
	}

	langs = g_hash_table_get_keys (locales);
	langs = g_list_sort (langs, (GCompareFunc) g_utf8_collate);

	for (link = langs; link; link = g_list_next (link)) {
		const gchar *lang_name = link->data;

		if (lang_name) {
			const gchar *locale = g_hash_table_lookup (locales, lang_name);

			gtk_combo_box_text_append (combo, locale, lang_name);
			n_langs++;
		}
	}

	g_hash_table_destroy (locales);
	g_list_free (langs);

	if (n_langs > 10)
		gtk_combo_box_set_wrap_width (GTK_COMBO_BOX (combo), 5);
}

void
em_composer_utils_update_security (EMsgComposer *composer,
				   EMailPartValidityFlags validity_pgp_sum,
				   EMailPartValidityFlags validity_smime_sum)
{
	g_return_if_fail (E_IS_MSG_COMPOSER (composer));

	if (validity_pgp_sum != 0 || validity_smime_sum != 0) {
		EUIAction *action;
		GSettings *settings;
		gboolean sign_reply;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		sign_reply = g_settings_get_boolean (settings, "composer-sign-reply-if-signed");
		g_object_unref (settings);

		if ((validity_pgp_sum & E_MAIL_PART_VALIDITY_PGP) != 0) {
			if (sign_reply && (validity_pgp_sum & E_MAIL_PART_VALIDITY_SIGNED) != 0) {
				action = E_COMPOSER_ACTION_PGP_SIGN (composer);
				e_ui_action_set_active (action, TRUE);
			}

			if ((validity_pgp_sum & E_MAIL_PART_VALIDITY_ENCRYPTED) != 0) {
				action = E_COMPOSER_ACTION_PGP_ENCRYPT (composer);
				e_ui_action_set_active (action, TRUE);
			}
		}

		if ((validity_smime_sum & E_MAIL_PART_VALIDITY_SMIME) != 0) {
			if (sign_reply && (validity_smime_sum & E_MAIL_PART_VALIDITY_SIGNED) != 0) {
				action = E_COMPOSER_ACTION_SMIME_SIGN (composer);
				e_ui_action_set_active (action, TRUE);
			}

			if ((validity_smime_sum & E_MAIL_PART_VALIDITY_ENCRYPTED) != 0) {
				action = E_COMPOSER_ACTION_SMIME_ENCRYPT (composer);
				e_ui_action_set_active (action, TRUE);
			}
		}
	}
}
