/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Johnny Jacob <jjohnny@novell.com>
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

#include <gmodule.h>
#include <libebackend/libebackend.h>

#include <camel/camel.h>
#include <camel/camel-search-private.h>

#include <e-util/e-util.h>

#include <mail/em-composer-utils.h>
#include <mail/em-utils.h>

#include "composer/e-msg-composer.h"
#include "composer/e-composer-actions.h"

#define CONF_KEY_ATTACH_REMINDER_CLUES "attachment-reminder-clues"

typedef struct {
	GSettings   *settings;
	GtkWidget   *treeview;
	GtkWidget   *clue_add;
	GtkWidget   *clue_edit;
	GtkWidget   *clue_remove;
	GtkListStore *store;
} UIData;

enum {
	CLUE_KEYWORD_COLUMN,
	CLUE_N_COLUMNS
};

enum {
	AR_IS_PLAIN,
	AR_IS_FORWARD,
	AR_IS_REPLY
};

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

/* Standard GObject macros */
#define E_TYPE_ATTACHMENT_REMINDER \
	(e_attachment_reminder_get_type ())
#define E_ATTACHMENT_REMINDER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ATTACHMENT_REMINDER, EAttachmentReminder))
#define E_ATTACHMENT_REMINDER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ATTACHMENT_REMINDER, EAttachmentReminderClass))
#define E_IS_ATTACHMENT_REMINDER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ATTACHMENT_REMINDER))

typedef struct _EAttachmentReminder EAttachmentReminder;
typedef struct _EAttachmentReminderClass EAttachmentReminderClass;

struct _EAttachmentReminder {
	EExtension parent;
	gboolean disabled;
};

struct _EAttachmentReminderClass {
	EExtensionClass parent_class;
};

GType e_attachment_reminder_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (EAttachmentReminder, e_attachment_reminder, E_TYPE_EXTENSION)

static gboolean ask_for_missing_attachment (EAttachmentReminder *ext, GtkWindow *widget);
static gboolean check_for_attachment_clues (EMsgComposer *composer, GByteArray *msg_text, guint32 ar_flags);
static gboolean check_for_attachment (EMsgComposer *composer);
static guint32 get_flags_from_composer (EMsgComposer *composer);

static guint32
get_flags_from_composer (EMsgComposer *composer)
{
	const gchar *header;

	g_return_val_if_fail (E_IS_MSG_COMPOSER (composer), AR_IS_PLAIN);

	header = e_msg_composer_get_header (composer, "X-Evolution-Source-Flags", 0);
	if (!header || !*header)
		return AR_IS_PLAIN;

	if (e_util_utf8_strstrcase (header, "FORWARDED")) {
		GSettings *settings;
		EMailForwardStyle style;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		style = g_settings_get_enum (settings, "forward-style-name");
		g_object_unref (settings);

		return style == E_MAIL_FORWARD_STYLE_INLINE ? AR_IS_FORWARD : AR_IS_PLAIN;
	}

	if (e_util_utf8_strstrcase (header, "ANSWERED") ||
	    e_util_utf8_strstrcase (header, "ANSWERED_ALL")) {
		GSettings *settings;
		EMailReplyStyle style;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		style = g_settings_get_enum (settings, "reply-style-name");
		g_object_unref (settings);

		return style == E_MAIL_REPLY_STYLE_OUTLOOK ? AR_IS_REPLY : AR_IS_PLAIN;
	}

	return AR_IS_PLAIN;
}

static gboolean
ask_for_missing_attachment (EAttachmentReminder *ext,
                            GtkWindow *window)
{
	GtkWidget *check;
	GtkWidget *dialog;
	GtkWidget *container;
	gint response;

	dialog = e_alert_dialog_new_for_args (
		window, "org.gnome.evolution.plugins.attachment_reminder:"
		"attachment-reminder", NULL);

	container = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));

	/*Check buttons*/
	check = gtk_check_button_new_with_mnemonic (
		_("_Do not show this message again."));
	gtk_box_pack_start (GTK_BOX (container), check, FALSE, FALSE, 0);
	gtk_widget_show (check);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
		ext->disabled = TRUE;

	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_OK)
		g_action_activate (G_ACTION (E_COMPOSER_ACTION_ATTACH (window)), NULL);

	return response == GTK_RESPONSE_YES;
}

static void
censor_quoted_lines (GByteArray *msg_text,
		     const gchar *until_marker)
{
	gchar *ptr;
	gboolean in_quotation = FALSE;
	gint marker_len;

	g_return_if_fail (msg_text != NULL);

	if (until_marker)
		marker_len = strlen (until_marker);
	else
		marker_len = 0;

	ptr = (gchar *) msg_text->data;

	if (marker_len &&
	    strncmp (ptr, until_marker, marker_len) == 0 &&
	    (ptr[marker_len] == '\r' || ptr[marker_len] == '\n')) {
		/* Simply cut everything below the marker and the marker itself */
		if (marker_len > 3) {
			ptr[0] = '\r';
			ptr[1] = '\n';
			ptr[2] = '\0';
		} else {
			*ptr = '\0';
		}

		return;
	}

	for (ptr = (gchar *) msg_text->data; ptr && *ptr; ptr++) {
		if (*ptr == '\n') {
			in_quotation = ptr[1] == '>';
			if (!in_quotation && marker_len &&
			    strncmp (ptr + 1, until_marker, marker_len) == 0 &&
			    (ptr[1 + marker_len] == '\r' || ptr[1 + marker_len] == '\n')) {
				/* Simply cut everything below the marker and the marker itself */
				if (marker_len > 3) {
					ptr[0] = '\r';
					ptr[1] = '\n';
					ptr[2] = '\0';
				} else {
					*ptr = '\0';
				}
				break;
			}
		} else if (*ptr != '\r' && in_quotation) {
			*ptr = ' ';
		}
	}
}

/* check for the clues */
static gboolean
check_for_attachment_clues (EMsgComposer *composer,
			    GByteArray *msg_text,
			    guint32 ar_flags)
{
	GSettings *settings;
	gchar **clue_list;
	gchar *marker = NULL;
	gboolean found = FALSE;

	if (ar_flags == AR_IS_FORWARD)
		marker = em_composer_utils_get_forward_marker (composer);
	else if (ar_flags == AR_IS_REPLY)
		marker = em_composer_utils_get_original_marker (composer);

	settings = e_util_ref_settings ("org.gnome.evolution.plugin.attachment-reminder");

	/* Get the list from GSettings */
	clue_list = g_settings_get_strv (settings, CONF_KEY_ATTACH_REMINDER_CLUES);

	g_object_unref (settings);

	if (clue_list && clue_list[0]) {
		gint ii, jj, to;

		g_byte_array_append (msg_text, (const guint8 *) "\r\n\0", 3);

		censor_quoted_lines (msg_text, marker);

		for (ii = 0; clue_list[ii] && !found; ii++) {
			GString *word;
			const gchar *clue = clue_list[ii];

			if (!*clue)
				continue;

			word = g_string_new ("\"");

			to = word->len;
			g_string_append (word, clue);

			for (jj = word->len - 1; jj >= to; jj--) {
				if (word->str[jj] == '\\' || word->str[jj] == '\"')
					g_string_insert_c (word, jj, '\\');
			}

			g_string_append_c (word, '\"');

			found = camel_search_header_match ((const gchar *) msg_text->data, word->str, CAMEL_SEARCH_MATCH_WORD, CAMEL_SEARCH_TYPE_ASIS, NULL);

			g_string_free (word, TRUE);
		}
	}

	g_strfreev (clue_list);
	g_free (marker);

	return found;
}

/* check for the any attachment */
static gboolean
check_for_attachment (EMsgComposer *composer)
{
	EAttachmentView *view;
	EAttachmentStore *store;

	view = e_msg_composer_get_attachment_view (composer);
	store = e_attachment_view_get_store (view);

	return (e_attachment_store_get_num_attachments (store) > 0);
}

static gboolean
attachment_reminder_presend_cb (EMsgComposer *composer,
                                gpointer user_data)
{
	EAttachmentReminder *ext = E_ATTACHMENT_REMINDER (user_data);
	GByteArray *raw_msg_barray;
	gboolean can_send = TRUE;

	if (ext->disabled)
		return TRUE;

	/* no need to check for content, when there are attachments */
	if (check_for_attachment (composer))
		return TRUE;

	raw_msg_barray =
		e_msg_composer_get_raw_message_text_without_signature (composer);
	if (!raw_msg_barray)
		return TRUE;

	if (check_for_attachment_clues (composer, raw_msg_barray, get_flags_from_composer (composer))) {
		can_send = ask_for_missing_attachment (ext, (GtkWindow *) composer);
	}

	g_byte_array_free (raw_msg_barray, TRUE);

	return can_send;
}

static void
attachment_reminder_constructed (GObject *object)
{
	EExtensible *extensible;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_attachment_reminder_parent_class)->constructed (object);

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	g_signal_connect (
		extensible, "presend",
		G_CALLBACK (attachment_reminder_presend_cb), object);
}

static void
e_attachment_reminder_class_init (EAttachmentReminderClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = attachment_reminder_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MSG_COMPOSER;
}

static void
e_attachment_reminder_class_finalize (EAttachmentReminderClass *class)
{
}

static void
e_attachment_reminder_init (EAttachmentReminder *extension)
{
	extension->disabled = FALSE;
}

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_attachment_reminder_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
