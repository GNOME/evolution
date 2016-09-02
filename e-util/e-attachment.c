/*
 * e-attachment.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-attachment.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#ifdef HAVE_AUTOAR
#include <gnome-autoar/gnome-autoar.h>
#endif

#include <libedataserver/libedataserver.h>

#include "e-icon-factory.h"
#include "e-mktemp.h"
#include "e-misc-utils.h"

#define E_ATTACHMENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ATTACHMENT, EAttachmentPrivate))

/* Fallback Icon */
#define DEFAULT_ICON_NAME	"mail-attachment"

/* Emblems */
#define EMBLEM_CANCELLED	"process-stop"
#define EMBLEM_LOADING		"emblem-downloads"
#define EMBLEM_SAVING		"document-save"
#define EMBLEM_ENCRYPT_WEAK	"security-low"
#define EMBLEM_ENCRYPT_STRONG	"security-high"
#define EMBLEM_ENCRYPT_UNKNOWN	"security-medium"
#define EMBLEM_SIGN_BAD		"stock_signature-bad"
#define EMBLEM_SIGN_GOOD	"stock_signature-ok"
#define EMBLEM_SIGN_UNKNOWN	"stock_signature"

/* Attributes needed for EAttachmentStore columns. */
#define ATTACHMENT_QUERY "standard::*,preview::*,thumbnail::*"

struct _EAttachmentPrivate {
	GMutex property_lock;

	GFile *file;
	GIcon *icon;
	GFileInfo *file_info;
	GCancellable *cancellable;
	CamelMimePart *mime_part;
	guint emblem_timeout_id;
	gchar *disposition;
	gint percent;
	gint64 last_percent_notify; /* to avoid excessive notifications */

	guint can_show : 1;
	guint loading : 1;
	guint saving : 1;
	guint initially_shown : 1;

	guint save_self      : 1;
	guint save_extracted : 1;

	CamelCipherValidityEncrypt encrypted;
	CamelCipherValiditySign signed_;

	/* These are IDs for idle callbacks,
	 * protected by the idle_lock mutex. */
	GMutex idle_lock;
	guint update_icon_column_idle_id;
	guint update_progress_columns_idle_id;
	guint update_file_info_columns_idle_id;
};

enum {
	PROP_0,
	PROP_CAN_SHOW,
	PROP_DISPOSITION,
	PROP_ENCRYPTED,
	PROP_FILE,
	PROP_FILE_INFO,
	PROP_ICON,
	PROP_LOADING,
	PROP_MIME_PART,
	PROP_PERCENT,
	PROP_SAVE_SELF,
	PROP_SAVE_EXTRACTED,
	PROP_SAVING,
	PROP_INITIALLY_SHOWN,
	PROP_SIGNED
};

enum {
	LOAD_FAILED,
	UPDATE_FILE_INFO,
	UPDATE_ICON,
	UPDATE_PROGRESS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	EAttachment,
	e_attachment,
	G_TYPE_OBJECT)

static gboolean
create_system_thumbnail (EAttachment *attachment,
                         GIcon **icon)
{
	GFile *file;
	GFile *icon_file;
	gchar *file_path = NULL;
	gchar *thumbnail = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);
	g_return_val_if_fail (icon != NULL, FALSE);

	file = e_attachment_ref_file (attachment);
	if (file != NULL)
		file_path = g_file_get_path (file);

	if (file_path != NULL) {
		thumbnail = e_icon_factory_create_thumbnail (file_path);
		g_free (file_path);
	}

	if (thumbnail == NULL)
		goto exit;

	icon_file = g_file_new_for_path (thumbnail);

	if (*icon != NULL)
		g_object_unref (*icon);

	*icon = g_file_icon_new (icon_file);

	g_object_unref (icon_file);

	if (file != NULL) {
		GFileInfo *file_info;
		const gchar *attribute;

		file_info = e_attachment_ref_file_info (attachment);
		attribute = G_FILE_ATTRIBUTE_THUMBNAIL_PATH;

		if (file_info != NULL) {
			g_file_info_set_attribute_byte_string (
				file_info, attribute, thumbnail);
			g_object_unref (file_info);
		}
	}

	g_free (thumbnail);

	success = TRUE;

exit:
	g_clear_object (&file);

	return success;
}

static gchar *
attachment_get_default_charset (void)
{
	GSettings *settings;
	gchar *charset;

	/* XXX This doesn't really belong here. */

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	charset = g_settings_get_string (settings, "composer-charset");
	if (charset == NULL || *charset == '\0') {
		g_free (charset);
		/* FIXME This was "/apps/evolution/mail/format/charset",
		 *       not sure it relates to "charset" */
		charset = g_settings_get_string (settings, "charset");
		if (charset == NULL || *charset == '\0') {
			g_free (charset);
			charset = NULL;
		}
	}
	g_object_unref (settings);

	if (charset == NULL)
		charset = g_strdup (camel_iconv_locale_charset ());

	if (charset == NULL)
		charset = g_strdup ("us-ascii");

	return charset;
}

static GFile*
attachment_get_temporary (GError **error)
{
	gchar *template;
	gchar *path;
	GFile *temp_directory;

	errno = 0;

	/* Save the file to a temporary directory.
	 * We use a directory so the files can retain their basenames.
	 * XXX This could trigger a blocking temp directory cleanup. */
	template = g_strdup_printf (PACKAGE "-%s-XXXXXX", g_get_user_name ());
	path = e_mkdtemp (template);
	g_free (template);

	/* XXX Let's hope errno got set properly. */
	if (path == NULL) {
		g_set_error (
			error, G_FILE_ERROR,
			g_file_error_from_errno (errno),
			"%s", g_strerror (errno));
		return NULL;
	}

	temp_directory = g_file_new_for_path (path);
	g_free (path);

	return temp_directory;
}


static gboolean
attachment_update_file_info_columns_idle_cb (gpointer weak_ref)
{
	EAttachment *attachment;
	GFileInfo *file_info;
	const gchar *content_type;
	const gchar *display_name;
	gchar *content_desc;
	gchar *display_size;
	gchar *description;
	gchar *caption;
	goffset size;

	attachment = g_weak_ref_get (weak_ref);
	if (attachment == NULL)
		goto exit;

	g_mutex_lock (&attachment->priv->idle_lock);
	attachment->priv->update_file_info_columns_idle_id = 0;
	g_mutex_unlock (&attachment->priv->idle_lock);

	file_info = e_attachment_ref_file_info (attachment);
	if (file_info == NULL)
		goto exit;

	content_type = g_file_info_get_content_type (file_info);
	display_name = g_file_info_get_display_name (file_info);
	size = g_file_info_get_size (file_info);

	content_desc = g_content_type_get_description (content_type);
	display_size = g_format_size (size);

	description = e_attachment_dup_description (attachment);
	if (description == NULL || *description == '\0') {
		g_free (description);
		description = g_strdup (display_name);
	}

	if (size > 0)
		caption = g_strdup_printf ("%s\n(%s)", description, display_size);
	else
		caption = g_strdup (description);

	g_signal_emit (attachment, signals[UPDATE_FILE_INFO], 0, caption, content_desc, description, (gint64) size);

	g_free (content_desc);
	g_free (display_size);
	g_free (description);
	g_free (caption);

	g_clear_object (&file_info);

exit:
	g_clear_object (&attachment);

	return FALSE;
}

static void
attachment_update_file_info_columns (EAttachment *attachment)
{
	g_mutex_lock (&attachment->priv->idle_lock);

	if (attachment->priv->update_file_info_columns_idle_id == 0) {
		guint idle_id;

		idle_id = g_idle_add_full (
			G_PRIORITY_HIGH_IDLE,
			attachment_update_file_info_columns_idle_cb,
			e_weak_ref_new (attachment),
			(GDestroyNotify) e_weak_ref_free);
		attachment->priv->update_file_info_columns_idle_id = idle_id;
	}

	g_mutex_unlock (&attachment->priv->idle_lock);
}

static gboolean
attachment_update_icon_column_idle_cb (gpointer weak_ref)
{
	EAttachment *attachment;
	GFileInfo *file_info;
	GCancellable *cancellable;
	GIcon *icon = NULL;
	const gchar *emblem_name = NULL;
	const gchar *thumbnail_path = NULL;

	attachment = g_weak_ref_get (weak_ref);
	if (attachment == NULL)
		goto exit;

	g_mutex_lock (&attachment->priv->idle_lock);
	attachment->priv->update_icon_column_idle_id = 0;
	g_mutex_unlock (&attachment->priv->idle_lock);

	cancellable = attachment->priv->cancellable;
	file_info = e_attachment_ref_file_info (attachment);

	if (file_info != NULL) {
		icon = g_file_info_get_icon (file_info);
		/* add the reference here, thus the create_system_thumbnail() can unref the *icon. */
		if (icon)
			g_object_ref (icon);
		thumbnail_path = g_file_info_get_attribute_byte_string (
			file_info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
	}

	if (e_attachment_is_mail_note (attachment)) {
		g_clear_object (&icon);
		icon = g_themed_icon_new ("evolution-memos");

	/* Prefer the thumbnail if we have one. */
	} else if (thumbnail_path != NULL && *thumbnail_path != '\0') {
		GFile *file;

		file = g_file_new_for_path (thumbnail_path);
		icon = g_file_icon_new (file);
		g_object_unref (file);

	/* Try the system thumbnailer. */
	} else if (create_system_thumbnail (attachment, &icon)) {
		/* Nothing to do, just use the icon. */

	/* Else use the standard icon for the content type. */
	} else if (icon != NULL) {
		/* Nothing to do, just use the already reffed icon. */

	/* Last ditch fallback.  (GFileInfo not yet loaded?) */
	} else
		icon = g_themed_icon_new (DEFAULT_ICON_NAME);

	/* Pick an emblem, limit one.  Choices listed by priority. */

	if (g_cancellable_is_cancelled (cancellable))
		emblem_name = EMBLEM_CANCELLED;

	else if (e_attachment_get_loading (attachment))
		emblem_name = EMBLEM_LOADING;

	else if (e_attachment_get_saving (attachment))
		emblem_name = EMBLEM_SAVING;

	else if (e_attachment_get_encrypted (attachment))
		switch (e_attachment_get_encrypted (attachment)) {
			case CAMEL_CIPHER_VALIDITY_ENCRYPT_WEAK:
				emblem_name = EMBLEM_ENCRYPT_WEAK;
				break;

			case CAMEL_CIPHER_VALIDITY_ENCRYPT_ENCRYPTED:
				emblem_name = EMBLEM_ENCRYPT_UNKNOWN;
				break;

			case CAMEL_CIPHER_VALIDITY_ENCRYPT_STRONG:
				emblem_name = EMBLEM_ENCRYPT_STRONG;
				break;

			default:
				g_warn_if_reached ();
				break;
		}

	else if (e_attachment_get_signed (attachment))
		switch (e_attachment_get_signed (attachment)) {
			case CAMEL_CIPHER_VALIDITY_SIGN_GOOD:
				emblem_name = EMBLEM_SIGN_GOOD;
				break;

			case CAMEL_CIPHER_VALIDITY_SIGN_BAD:
				emblem_name = EMBLEM_SIGN_BAD;
				break;

			case CAMEL_CIPHER_VALIDITY_SIGN_UNKNOWN:
			case CAMEL_CIPHER_VALIDITY_SIGN_NEED_PUBLIC_KEY:
				emblem_name = EMBLEM_SIGN_UNKNOWN;
				break;

			default:
				g_warn_if_reached ();
				break;
		}

	if (emblem_name != NULL) {
		GIcon *emblemed_icon;
		GEmblem *emblem;

		emblemed_icon = g_themed_icon_new (emblem_name);
		emblem = g_emblem_new (emblemed_icon);
		g_object_unref (emblemed_icon);

		emblemed_icon = g_emblemed_icon_new (icon, emblem);
		g_object_unref (emblem);
		g_object_unref (icon);

		icon = emblemed_icon;
	}

	g_signal_emit (attachment, signals[UPDATE_ICON], 0, icon);

	/* Cache the icon to reuse for things like drag-n-drop. */
	if (attachment->priv->icon != NULL)
		g_object_unref (attachment->priv->icon);
	attachment->priv->icon = icon;
	g_object_notify (G_OBJECT (attachment), "icon");

	g_clear_object (&file_info);

exit:
	g_clear_object (&attachment);

	return FALSE;
}

static void
attachment_update_icon_column (EAttachment *attachment)
{
	g_mutex_lock (&attachment->priv->idle_lock);

	if (attachment->priv->update_icon_column_idle_id == 0) {
		guint idle_id;

		idle_id = g_idle_add_full (
			G_PRIORITY_HIGH_IDLE,
			attachment_update_icon_column_idle_cb,
			e_weak_ref_new (attachment),
			(GDestroyNotify) e_weak_ref_free);
		attachment->priv->update_icon_column_idle_id = idle_id;
	}

	g_mutex_unlock (&attachment->priv->idle_lock);
}

static gboolean
attachment_update_progress_columns_idle_cb (gpointer weak_ref)
{
	EAttachment *attachment;
	gboolean loading;
	gboolean saving;
	gint percent;

	attachment = g_weak_ref_get (weak_ref);
	if (attachment == NULL)
		goto exit;

	g_mutex_lock (&attachment->priv->idle_lock);
	attachment->priv->update_progress_columns_idle_id = 0;
	g_mutex_unlock (&attachment->priv->idle_lock);

	/* Don't show progress bars until we have progress to report. */
	percent = e_attachment_get_percent (attachment);
	loading = e_attachment_get_loading (attachment) && (percent > 0);
	saving = e_attachment_get_saving (attachment) && (percent > 0);

	g_signal_emit (attachment, signals[UPDATE_PROGRESS], 0, loading, saving, percent);

exit:
	g_clear_object (&attachment);

	return FALSE;
}

static void
attachment_update_progress_columns (EAttachment *attachment)
{
	g_mutex_lock (&attachment->priv->idle_lock);

	if (attachment->priv->update_progress_columns_idle_id == 0) {
		guint idle_id;

		idle_id = g_idle_add_full (
			G_PRIORITY_HIGH_IDLE,
			attachment_update_progress_columns_idle_cb,
			e_weak_ref_new (attachment),
			(GDestroyNotify) e_weak_ref_free);
		attachment->priv->update_progress_columns_idle_id = idle_id;
	}

	g_mutex_unlock (&attachment->priv->idle_lock);
}

static void
attachment_set_loading (EAttachment *attachment,
                        gboolean loading)
{
	attachment->priv->percent = 0;
	attachment->priv->loading = loading;
	attachment->priv->last_percent_notify = 0;

	g_object_freeze_notify (G_OBJECT (attachment));
	g_object_notify (G_OBJECT (attachment), "percent");
	g_object_notify (G_OBJECT (attachment), "loading");
	g_object_thaw_notify (G_OBJECT (attachment));
}

static void
attachment_set_saving (EAttachment *attachment,
                       gboolean saving)
{
	attachment->priv->percent = 0;
	attachment->priv->saving = saving;
	attachment->priv->last_percent_notify = 0;
}

static void
attachment_progress_cb (goffset current_num_bytes,
                        goffset total_num_bytes,
                        EAttachment *attachment)
{
	gint new_percent;

	/* Avoid dividing by zero. */
	if (total_num_bytes == 0)
		return;

	/* do not notify too often, 5 times per second is sufficient */
	if (g_get_monotonic_time () - attachment->priv->last_percent_notify < 200000)
		return;

	attachment->priv->last_percent_notify = g_get_monotonic_time ();

	new_percent = (current_num_bytes * 100) / total_num_bytes;

	if (new_percent != attachment->priv->percent)
		attachment->priv->percent = new_percent;
}

static gboolean
attachment_cancelled_timeout_cb (gpointer user_data)
{
	EAttachment *attachment;

	attachment = E_ATTACHMENT (user_data);
	attachment->priv->emblem_timeout_id = 0;
	g_cancellable_reset (attachment->priv->cancellable);

	attachment_update_icon_column (attachment);

	return FALSE;
}

static void
attachment_cancelled_cb (EAttachment *attachment)
{
	/* Reset the GCancellable after one second.  This causes a
	 * cancel emblem to be briefly shown on the attachment icon
	 * as visual feedback that an operation was cancelled. */

	if (attachment->priv->emblem_timeout_id > 0)
		g_source_remove (attachment->priv->emblem_timeout_id);

	attachment->priv->emblem_timeout_id = e_named_timeout_add_seconds (
		1, attachment_cancelled_timeout_cb, attachment);

	attachment_update_icon_column (attachment);
}

static void
attachment_set_property (GObject *object,
                         guint property_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_SHOW:
			e_attachment_set_can_show (
				E_ATTACHMENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_DISPOSITION:
			e_attachment_set_disposition (
				E_ATTACHMENT (object),
				g_value_get_string (value));
			return;

		case PROP_ENCRYPTED:
			e_attachment_set_encrypted (
				E_ATTACHMENT (object),
				g_value_get_int (value));
			return;

		case PROP_FILE:
			e_attachment_set_file (
				E_ATTACHMENT (object),
				g_value_get_object (value));
			return;

		case PROP_INITIALLY_SHOWN:
			e_attachment_set_initially_shown (
				E_ATTACHMENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_MIME_PART:
			e_attachment_set_mime_part (
				E_ATTACHMENT (object),
				g_value_get_object (value));
			return;

		case PROP_SIGNED:
			e_attachment_set_signed (
				E_ATTACHMENT (object),
				g_value_get_int (value));
			return;

		case PROP_SAVE_SELF:
			e_attachment_set_save_self (
				E_ATTACHMENT (object),
				g_value_get_boolean (value));
			return;

		case PROP_SAVE_EXTRACTED:
			e_attachment_set_save_extracted (
				E_ATTACHMENT (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_get_property (GObject *object,
                         guint property_id,
                         GValue *value,
                         GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CAN_SHOW:
			g_value_set_boolean (
				value,
				e_attachment_get_can_show (
				E_ATTACHMENT (object)));
			return;

		case PROP_DISPOSITION:
			g_value_set_string (
				value,
				e_attachment_get_disposition (
				E_ATTACHMENT (object)));
			return;

		case PROP_ENCRYPTED:
			g_value_set_int (
				value,
				e_attachment_get_encrypted (
				E_ATTACHMENT (object)));
			return;

		case PROP_FILE:
			g_value_take_object (
				value,
				e_attachment_ref_file (
				E_ATTACHMENT (object)));
			return;

		case PROP_FILE_INFO:
			g_value_take_object (
				value,
				e_attachment_ref_file_info (
				E_ATTACHMENT (object)));
			return;

		case PROP_ICON:
			g_value_take_object (
				value,
				e_attachment_ref_icon (
				E_ATTACHMENT (object)));
			return;

		case PROP_INITIALLY_SHOWN:
			g_value_set_boolean (
				value,
				e_attachment_get_initially_shown (
				E_ATTACHMENT (object)));
			return;

		case PROP_LOADING:
			g_value_set_boolean (
				value,
				e_attachment_get_loading (
				E_ATTACHMENT (object)));
			return;

		case PROP_MIME_PART:
			g_value_take_object (
				value,
				e_attachment_ref_mime_part (
				E_ATTACHMENT (object)));
			return;

		case PROP_PERCENT:
			g_value_set_int (
				value,
				e_attachment_get_percent (
				E_ATTACHMENT (object)));
			return;

		case PROP_SAVE_SELF:
			g_value_set_boolean (
				value,
				e_attachment_get_save_self (
				E_ATTACHMENT (object)));
			return;

		case PROP_SAVE_EXTRACTED:
			g_value_set_boolean (
				value,
				e_attachment_get_save_extracted (
				E_ATTACHMENT (object)));
			return;

		case PROP_SAVING:
			g_value_set_boolean (
				value,
				e_attachment_get_saving (
				E_ATTACHMENT (object)));
			return;

		case PROP_SIGNED:
			g_value_set_int (
				value,
				e_attachment_get_signed (
				E_ATTACHMENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
attachment_dispose (GObject *object)
{
	EAttachmentPrivate *priv;

	priv = E_ATTACHMENT_GET_PRIVATE (object);

	g_clear_object (&priv->file);
	g_clear_object (&priv->icon);
	g_clear_object (&priv->file_info);
	g_clear_object (&priv->cancellable);
	g_clear_object (&priv->mime_part);

	if (priv->emblem_timeout_id > 0) {
		g_source_remove (priv->emblem_timeout_id);
		priv->emblem_timeout_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_attachment_parent_class)->dispose (object);
}

static void
attachment_finalize (GObject *object)
{
	EAttachmentPrivate *priv;

	priv = E_ATTACHMENT_GET_PRIVATE (object);

	if (priv->update_icon_column_idle_id > 0)
		g_source_remove (priv->update_icon_column_idle_id);

	if (priv->update_progress_columns_idle_id > 0)
		g_source_remove (priv->update_progress_columns_idle_id);

	if (priv->update_file_info_columns_idle_id > 0)
		g_source_remove (priv->update_file_info_columns_idle_id);

	g_mutex_clear (&priv->property_lock);
	g_mutex_clear (&priv->idle_lock);

	g_free (priv->disposition);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_attachment_parent_class)->finalize (object);
}

static void
e_attachment_class_init (EAttachmentClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EAttachmentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = attachment_set_property;
	object_class->get_property = attachment_get_property;
	object_class->dispose = attachment_dispose;
	object_class->finalize = attachment_finalize;

	g_object_class_install_property (
		object_class,
		PROP_CAN_SHOW,
		g_param_spec_boolean (
			"can-show",
			"Can Show",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_DISPOSITION,
		g_param_spec_string (
			"disposition",
			"Disposition",
			NULL,
			"attachment",
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/* FIXME Define a GEnumClass for this. */
	g_object_class_install_property (
		object_class,
		PROP_ENCRYPTED,
		g_param_spec_int (
			"encrypted",
			"Encrypted",
			NULL,
			CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE,
			CAMEL_CIPHER_VALIDITY_ENCRYPT_STRONG,
			CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_FILE,
		g_param_spec_object (
			"file",
			"File",
			NULL,
			G_TYPE_FILE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_FILE_INFO,
		g_param_spec_object (
			"file-info",
			"File Info",
			NULL,
			G_TYPE_FILE_INFO,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_ICON,
		g_param_spec_object (
			"icon",
			"Icon",
			NULL,
			G_TYPE_ICON,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_LOADING,
		g_param_spec_boolean (
			"loading",
			"Loading",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_MIME_PART,
		g_param_spec_object (
			"mime-part",
			"MIME Part",
			NULL,
			CAMEL_TYPE_MIME_PART,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_PERCENT,
		g_param_spec_int (
			"percent",
			"Percent",
			NULL,
			0,
			100,
			0,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SAVE_SELF,
		g_param_spec_boolean (
			"save-self",
			"Save self",
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SAVE_EXTRACTED,
		g_param_spec_boolean (
			"save-extracted",
			"Save extracted",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SAVING,
		g_param_spec_boolean (
			"saving",
			"Saving",
			NULL,
			FALSE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_INITIALLY_SHOWN,
		g_param_spec_boolean (
			"initially-shown",
			"Initially Shown",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/* FIXME Define a GEnumClass for this. */
	g_object_class_install_property (
		object_class,
		PROP_SIGNED,
		g_param_spec_int (
			"signed",
			"Signed",
			NULL,
			CAMEL_CIPHER_VALIDITY_SIGN_NONE,
			CAMEL_CIPHER_VALIDITY_SIGN_NEED_PUBLIC_KEY,
			CAMEL_CIPHER_VALIDITY_SIGN_NONE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[UPDATE_FILE_INFO] = g_signal_new (
		"update-file-info",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAttachmentClass, update_file_info),
		NULL, NULL, NULL,
		G_TYPE_NONE, 4,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_INT64);

	signals[UPDATE_ICON] = g_signal_new (
		"update-icon",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAttachmentClass, update_icon),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		G_TYPE_ICON);

	signals[UPDATE_PROGRESS] = g_signal_new (
		"update-progress",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAttachmentClass, update_progress),
		NULL, NULL, NULL,
		G_TYPE_NONE, 3,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_INT);

	signals[LOAD_FAILED] = g_signal_new (
		"load-failed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAttachmentClass, load_failed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 0,
		G_TYPE_NONE);
}

static void
e_attachment_init (EAttachment *attachment)
{
	attachment->priv = E_ATTACHMENT_GET_PRIVATE (attachment);
	attachment->priv->cancellable = g_cancellable_new ();
	attachment->priv->encrypted = CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE;
	attachment->priv->signed_ = CAMEL_CIPHER_VALIDITY_SIGN_NONE;

	g_mutex_init (&attachment->priv->property_lock);
	g_mutex_init (&attachment->priv->idle_lock);

	e_signal_connect_notify (
		attachment, "notify::encrypted",
		G_CALLBACK (attachment_update_icon_column), NULL);

	g_signal_connect (
		attachment, "notify::file-info",
		G_CALLBACK (attachment_update_file_info_columns), NULL);

	g_signal_connect (
		attachment, "notify::file-info",
		G_CALLBACK (attachment_update_icon_column), NULL);

	e_signal_connect_notify (
		attachment, "notify::loading",
		G_CALLBACK (attachment_update_icon_column), NULL);

	e_signal_connect_notify (
		attachment, "notify::loading",
		G_CALLBACK (attachment_update_progress_columns), NULL);

	e_signal_connect_notify (
		attachment, "notify::percent",
		G_CALLBACK (attachment_update_progress_columns), NULL);

	e_signal_connect_notify (
		attachment, "notify::saving",
		G_CALLBACK (attachment_update_icon_column), NULL);

	e_signal_connect_notify (
		attachment, "notify::saving",
		G_CALLBACK (attachment_update_progress_columns), NULL);

	e_signal_connect_notify (
		attachment, "notify::signed",
		G_CALLBACK (attachment_update_icon_column), NULL);

	g_signal_connect_swapped (
		attachment->priv->cancellable, "cancelled",
		G_CALLBACK (attachment_cancelled_cb), attachment);
}

EAttachment *
e_attachment_new (void)
{
	return g_object_new (E_TYPE_ATTACHMENT, NULL);
}

EAttachment *
e_attachment_new_for_path (const gchar *path)
{
	EAttachment *attachment;
	GFile *file;

	g_return_val_if_fail (path != NULL, NULL);

	file = g_file_new_for_path (path);
	attachment = g_object_new (E_TYPE_ATTACHMENT, "file", file, NULL);
	g_object_unref (file);

	return attachment;
}

EAttachment *
e_attachment_new_for_uri (const gchar *uri)
{
	EAttachment *attachment;
	GFile *file;

	g_return_val_if_fail (uri != NULL, NULL);

	file = g_file_new_for_uri (uri);
	attachment = g_object_new (E_TYPE_ATTACHMENT, "file", file, NULL);
	g_object_unref (file);

	return attachment;
}

EAttachment *
e_attachment_new_for_message (CamelMimeMessage *message)
{
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	EAttachment *attachment;
	GString *description;
	const gchar *subject;

	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	mime_part = camel_mime_part_new ();
	camel_mime_part_set_disposition (mime_part, "inline");
	subject = camel_mime_message_get_subject (message);

	/* To Translators: This text is set as a description of an attached
	 * message when, for example, attaching it to a composer. When the
	 * message to be attached has also filled Subject, then this text is
	 * of form "Attached message - Subject", otherwise it's left as is. */
	description = g_string_new (_("Attached message"));
	if (subject != NULL)
		g_string_append_printf (description, " - %s", subject);
	camel_mime_part_set_description (mime_part, description->str);
	g_string_free (description, TRUE);

	wrapper = CAMEL_DATA_WRAPPER (message);
	camel_medium_set_content (CAMEL_MEDIUM (mime_part), wrapper);
	camel_mime_part_set_content_type (mime_part, "message/rfc822");

	attachment = e_attachment_new ();
	e_attachment_set_mime_part (attachment, mime_part);
	g_object_unref (mime_part);

	return attachment;
}

void
e_attachment_add_to_multipart (EAttachment *attachment,
                               CamelMultipart *multipart,
                               const gchar *default_charset)
{
	CamelContentType *content_type;
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;

	/* XXX EMsgComposer might be a better place for this function. */

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (CAMEL_IS_MULTIPART (multipart));

	/* Still loading?  Too bad. */
	mime_part = e_attachment_ref_mime_part (attachment);
	if (mime_part == NULL)
		return;

	content_type = camel_mime_part_get_content_type (mime_part);
	wrapper = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

	if (CAMEL_IS_MULTIPART (wrapper))
		goto exit;

	/* For text content, determine the best encoding and character set. */
	if (camel_content_type_is (content_type, "text", "*")) {
		CamelTransferEncoding encoding;
		CamelStream *filtered_stream;
		CamelMimeFilter *filter;
		CamelStream *stream;
		const gchar *charset;

		charset = camel_content_type_param (content_type, "charset");

		/* Determine the best encoding by writing the MIME
		 * part to a NULL stream with a "bestenc" filter. */
		stream = camel_stream_null_new ();
		filtered_stream = camel_stream_filter_new (stream);
		filter = camel_mime_filter_bestenc_new (
			CAMEL_BESTENC_GET_ENCODING);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filtered_stream),
			CAMEL_MIME_FILTER (filter));
		camel_data_wrapper_decode_to_stream_sync (
			wrapper, filtered_stream, NULL, NULL);
		g_object_unref (filtered_stream);
		g_object_unref (stream);

		/* Retrieve the best encoding from the filter. */
		encoding = camel_mime_filter_bestenc_get_best_encoding (
			CAMEL_MIME_FILTER_BESTENC (filter),
			CAMEL_BESTENC_8BIT);
		camel_mime_part_set_encoding (mime_part, encoding);
		g_object_unref (filter);

		if (encoding == CAMEL_TRANSFER_ENCODING_7BIT) {
			/* The text fits within us-ascii, so this is safe.
			 * FIXME Check that this isn't iso-2022-jp? */
			default_charset = "us-ascii";

		} else if (charset == NULL && default_charset == NULL) {
			default_charset = attachment_get_default_charset ();
			/* FIXME Check that this fits within the
			 *       default_charset and if not, find one
			 *       that does and/or allow the user to
			 *       specify. */
		}

		if (charset == NULL) {
			gchar *type;

			camel_content_type_set_param (
				content_type, "charset", default_charset);
			type = camel_content_type_format (content_type);
			camel_mime_part_set_content_type (mime_part, type);
			g_free (type);
		}

	/* Otherwise, unless it's a message/rfc822, Base64 encode it. */
	} else if (!CAMEL_IS_MIME_MESSAGE (wrapper))
		camel_mime_part_set_encoding (
			mime_part, CAMEL_TRANSFER_ENCODING_BASE64);

exit:
	camel_multipart_add_part (multipart, mime_part);

	g_clear_object (&mime_part);
}

void
e_attachment_cancel (EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_cancellable_cancel (attachment->priv->cancellable);
}

gboolean
e_attachment_is_mail_note (EAttachment *attachment)
{
	CamelContentType *ct;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	if (!attachment->priv->mime_part)
		return FALSE;

	ct = camel_mime_part_get_content_type (attachment->priv->mime_part);
	if (!ct || !camel_content_type_is (ct, "message", "rfc822"))
		return FALSE;

	return camel_medium_get_header (CAMEL_MEDIUM (attachment->priv->mime_part), "X-Evolution-Note") != NULL;
}

gboolean
e_attachment_get_can_show (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	return attachment->priv->can_show;
}

void
e_attachment_set_can_show (EAttachment *attachment,
                           gboolean can_show)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->can_show = can_show;

	g_object_notify (G_OBJECT (attachment), "can-show");
}

const gchar *
e_attachment_get_disposition (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	return attachment->priv->disposition;
}

gchar *
e_attachment_dup_disposition (EAttachment *attachment)
{
	const gchar *protected;
	gchar *duplicate;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	g_mutex_lock (&attachment->priv->property_lock);

	protected = e_attachment_get_disposition (attachment);
	duplicate = g_strdup (protected);

	g_mutex_unlock (&attachment->priv->property_lock);

	return duplicate;
}

void
e_attachment_set_disposition (EAttachment *attachment,
                              const gchar *disposition)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	g_mutex_lock (&attachment->priv->property_lock);

	g_free (attachment->priv->disposition);
	attachment->priv->disposition = g_strdup (disposition);

	g_mutex_unlock (&attachment->priv->property_lock);

	g_object_notify (G_OBJECT (attachment), "disposition");
}

GFile *
e_attachment_ref_file (EAttachment *attachment)
{
	GFile *file = NULL;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	g_mutex_lock (&attachment->priv->property_lock);

	if (attachment->priv->file != NULL)
		file = g_object_ref (attachment->priv->file);

	g_mutex_unlock (&attachment->priv->property_lock);

	return file;
}

void
e_attachment_set_file (EAttachment *attachment,
                       GFile *file)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (file != NULL) {
		g_return_if_fail (G_IS_FILE (file));
		g_object_ref (file);
	}

	g_mutex_lock (&attachment->priv->property_lock);

	g_clear_object (&attachment->priv->file);
	attachment->priv->file = file;

	g_mutex_unlock (&attachment->priv->property_lock);

	g_object_notify (G_OBJECT (attachment), "file");
}

GFileInfo *
e_attachment_ref_file_info (EAttachment *attachment)
{
	GFileInfo *file_info = NULL;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	g_mutex_lock (&attachment->priv->property_lock);

	if (attachment->priv->file_info != NULL)
		file_info = g_object_ref (attachment->priv->file_info);

	g_mutex_unlock (&attachment->priv->property_lock);

	return file_info;
}

void
e_attachment_set_file_info (EAttachment *attachment,
                            GFileInfo *file_info)
{
	GIcon *icon;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (file_info != NULL) {
		g_return_if_fail (G_IS_FILE_INFO (file_info));
		g_object_ref (file_info);
	}

	g_mutex_lock (&attachment->priv->property_lock);

	g_clear_object (&attachment->priv->file_info);
	attachment->priv->file_info = file_info;

	/* If the GFileInfo contains a GThemedIcon, append a
	 * fallback icon name to ensure we display something. */
	icon = g_file_info_get_icon (file_info);
	if (G_IS_THEMED_ICON (icon))
		g_themed_icon_append_name (
			G_THEMED_ICON (icon), DEFAULT_ICON_NAME);

	g_mutex_unlock (&attachment->priv->property_lock);

	g_object_notify (G_OBJECT (attachment), "file-info");
}

/**
 * e_attachment_dup_mime_type:
 * @attachment: an #EAttachment
 *
 * Returns the MIME type of @attachment according to its #GFileInfo.
 * If the @attachment has no #GFileInfo then the function returns %NULL.
 * Free the returned MIME type string with g_free().
 *
 * Returns: a newly-allocated MIME type string, or %NULL
 **/
gchar *
e_attachment_dup_mime_type (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *content_type = NULL;
	gchar *mime_type = NULL;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	file_info = e_attachment_ref_file_info (attachment);
	if (file_info != NULL)
		content_type = g_file_info_get_content_type (file_info);

	if (content_type != NULL)
		mime_type = g_content_type_get_mime_type (content_type);

	if (mime_type != NULL)
		camel_strdown (mime_type);

	g_clear_object (&file_info);

	return mime_type;
}

GIcon *
e_attachment_ref_icon (EAttachment *attachment)
{
	GIcon *icon = NULL;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	g_mutex_lock (&attachment->priv->property_lock);

	if (attachment->priv->icon != NULL)
		icon = g_object_ref (attachment->priv->icon);

	g_mutex_unlock (&attachment->priv->property_lock);

	return icon;
}

gboolean
e_attachment_get_loading (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	return attachment->priv->loading;
}

CamelMimePart *
e_attachment_ref_mime_part (EAttachment *attachment)
{
	CamelMimePart *mime_part = NULL;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	g_mutex_lock (&attachment->priv->property_lock);

	if (attachment->priv->mime_part != NULL)
		mime_part = g_object_ref (attachment->priv->mime_part);

	g_mutex_unlock (&attachment->priv->property_lock);

	return mime_part;
}

void
e_attachment_set_mime_part (EAttachment *attachment,
                            CamelMimePart *mime_part)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (mime_part != NULL) {
		g_return_if_fail (CAMEL_IS_MIME_PART (mime_part));
		g_object_ref (mime_part);
	}

	g_mutex_lock (&attachment->priv->property_lock);

	g_clear_object (&attachment->priv->mime_part);
	attachment->priv->mime_part = mime_part;

	g_mutex_unlock (&attachment->priv->property_lock);

	g_object_notify (G_OBJECT (attachment), "mime-part");
}

gint
e_attachment_get_percent (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), 0);

	return attachment->priv->percent;
}

gboolean
e_attachment_get_saving (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	return attachment->priv->saving;
}

gboolean
e_attachment_get_initially_shown (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	return attachment->priv->initially_shown;
}

void
e_attachment_set_initially_shown (EAttachment *attachment,
				  gboolean initially_shown)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->initially_shown = initially_shown;

	g_object_notify (G_OBJECT (attachment), "initially-shown");
}

gboolean
e_attachment_get_save_self (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), TRUE);

	return attachment->priv->save_self;
}

void
e_attachment_set_save_self (EAttachment *attachment,
                            gboolean save_self)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->save_self = save_self;
}

gboolean
e_attachment_get_save_extracted (EAttachment *attachment)
{
	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	return attachment->priv->save_extracted;
}

void
e_attachment_set_save_extracted (EAttachment *attachment,
                                 gboolean save_extracted)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->save_extracted = save_extracted;
}

CamelCipherValidityEncrypt
e_attachment_get_encrypted (EAttachment *attachment)
{
	g_return_val_if_fail (
		E_IS_ATTACHMENT (attachment),
		CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE);

	return attachment->priv->encrypted;
}

void
e_attachment_set_encrypted (EAttachment *attachment,
                            CamelCipherValidityEncrypt encrypted)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->encrypted = encrypted;

	g_object_notify (G_OBJECT (attachment), "encrypted");
}

CamelCipherValiditySign
e_attachment_get_signed (EAttachment *attachment)
{
	g_return_val_if_fail (
		E_IS_ATTACHMENT (attachment),
		CAMEL_CIPHER_VALIDITY_SIGN_NONE);

	return attachment->priv->signed_;
}

void
e_attachment_set_signed (EAttachment *attachment,
                         CamelCipherValiditySign signed_)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment->priv->signed_ = signed_;

	g_object_notify (G_OBJECT (attachment), "signed");
}

gchar *
e_attachment_dup_description (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *attribute;
	const gchar *protected;
	gchar *duplicate;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	file_info = e_attachment_ref_file_info (attachment);
	if (file_info == NULL)
		return NULL;

	attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
	protected = g_file_info_get_attribute_string (file_info, attribute);
	duplicate = g_strdup (protected);

	g_object_unref (file_info);

	return duplicate;
}

gchar *
e_attachment_dup_thumbnail_path (EAttachment *attachment)
{
	GFileInfo *file_info;
	const gchar *attribute;
	const gchar *protected;
	gchar *duplicate;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	file_info = e_attachment_ref_file_info (attachment);
	if (file_info == NULL)
		return NULL;

	attribute = G_FILE_ATTRIBUTE_THUMBNAIL_PATH;
	protected = g_file_info_get_attribute_string (file_info, attribute);
	duplicate = g_strdup (protected);

	g_object_unref (file_info);

	return duplicate;
}

gboolean
e_attachment_is_rfc822 (EAttachment *attachment)
{
	gchar *mime_type;
	gboolean is_rfc822;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	mime_type = e_attachment_dup_mime_type (attachment);
	is_rfc822 =
		(mime_type != NULL) &&
		(g_ascii_strcasecmp (mime_type, "message/rfc822") == 0);
	g_free (mime_type);

	return is_rfc822;
}

GList *
e_attachment_list_apps (EAttachment *attachment)
{
	GList *app_info_list;
	GList *guessed_infos;
	GFileInfo *file_info;
	const gchar *content_type;
	const gchar *display_name;
	gboolean type_is_unknown;
	gchar *allocated;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);

	file_info = e_attachment_ref_file_info (attachment);
	if (file_info == NULL)
		return NULL;

	content_type = g_file_info_get_content_type (file_info);
	display_name = g_file_info_get_display_name (file_info);
	g_return_val_if_fail (content_type != NULL, NULL);

	app_info_list = g_app_info_get_all_for_type (content_type);
	type_is_unknown = g_content_type_is_unknown (content_type);

	if (app_info_list != NULL && !type_is_unknown)
		goto exit;

	if (display_name == NULL)
		goto exit;

	allocated = g_content_type_guess (display_name, NULL, 0, NULL);
	guessed_infos = g_app_info_get_all_for_type (allocated);
	app_info_list = g_list_concat (guessed_infos, app_info_list);
	g_free (allocated);

exit:
	g_clear_object (&file_info);

	return app_info_list;
}

void
e_attachment_update_store_columns (EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	attachment_update_file_info_columns (attachment);
	attachment_update_icon_column (attachment);
	attachment_update_progress_columns (attachment);
}

/************************* e_attachment_load_async() *************************/

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	EAttachment *attachment;
	CamelMimePart *mime_part;
	GSimpleAsyncResult *simple;

	GInputStream *input_stream;
	GOutputStream *output_stream;
	GFileInfo *file_info;
	goffset total_num_bytes;
	gssize bytes_read;
	gchar buffer[4096];
};

/* Forward Declaration */
static void
attachment_load_stream_read_cb (GInputStream *input_stream,
                                GAsyncResult *result,
                                LoadContext *load_context);
static void
attachment_load_query_info_cb (GFile *file,
                               GAsyncResult *result,
                               LoadContext *load_context);

static LoadContext *
attachment_load_context_new (EAttachment *attachment,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	LoadContext *load_context;
	GSimpleAsyncResult *simple;

	simple = g_simple_async_result_new (
		G_OBJECT (attachment), callback,
		user_data, e_attachment_load_async);

	load_context = g_slice_new0 (LoadContext);
	load_context->attachment = g_object_ref (attachment);
	load_context->simple = simple;

	attachment_set_loading (load_context->attachment, TRUE);

	return load_context;
}

static void
attachment_load_context_free (LoadContext *load_context)
{
	g_object_unref (load_context->attachment);

	if (load_context->mime_part != NULL)
		g_object_unref (load_context->mime_part);

	if (load_context->simple)
		g_object_unref (load_context->simple);

	if (load_context->input_stream != NULL)
		g_object_unref (load_context->input_stream);

	if (load_context->output_stream != NULL)
		g_object_unref (load_context->output_stream);

	if (load_context->file_info != NULL)
		g_object_unref (load_context->file_info);

	g_slice_free (LoadContext, load_context);
}

static gboolean
attachment_load_check_for_error (LoadContext *load_context,
                                 GError *error)
{
	GSimpleAsyncResult *simple;

	if (error == NULL)
		return FALSE;

	simple = load_context->simple;
	g_simple_async_result_take_error (simple, error);
	g_simple_async_result_complete (simple);

	attachment_load_context_free (load_context);

	return TRUE;
}

static void
attachment_load_finish (LoadContext *load_context)
{
	GFileInfo *file_info;
	EAttachment *attachment;
	GMemoryOutputStream *output_stream;
	GSimpleAsyncResult *simple;
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	CamelStream *stream;
	const gchar *attribute;
	const gchar *content_type;
	const gchar *display_name;
	const gchar *description;
	const gchar *disposition;
	gchar *mime_type;
	gpointer data;
	gsize size;

	simple = load_context->simple;

	file_info = load_context->file_info;
	attachment = load_context->attachment;
	output_stream = G_MEMORY_OUTPUT_STREAM (load_context->output_stream);

	if (e_attachment_is_rfc822 (attachment))
		wrapper = (CamelDataWrapper *) camel_mime_message_new ();
	else
		wrapper = camel_data_wrapper_new ();

	content_type = g_file_info_get_content_type (file_info);
	mime_type = g_content_type_get_mime_type (content_type);

	data = g_memory_output_stream_get_data (output_stream);
	size = g_memory_output_stream_get_data_size (output_stream);

	stream = camel_stream_mem_new_with_buffer (data, size);
	camel_data_wrapper_construct_from_stream_sync (
		wrapper, stream, NULL, NULL);
	camel_data_wrapper_set_mime_type (wrapper, mime_type);
	camel_stream_close (stream, NULL, NULL);
	g_object_unref (stream);

	mime_part = camel_mime_part_new ();
	camel_medium_set_content (CAMEL_MEDIUM (mime_part), wrapper);

	g_object_unref (wrapper);
	g_free (mime_type);

	display_name = g_file_info_get_display_name (file_info);
	if (display_name != NULL)
		camel_mime_part_set_filename (mime_part, display_name);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
	description = g_file_info_get_attribute_string (file_info, attribute);
	if (description != NULL)
		camel_mime_part_set_description (mime_part, description);

	disposition = e_attachment_get_disposition (attachment);
	if (disposition != NULL)
		camel_mime_part_set_disposition (mime_part, disposition);

	/* Correctly report the size of zero length special files. */
	if (g_file_info_get_size (file_info) == 0)
		g_file_info_set_size (file_info, size);

	load_context->mime_part = mime_part;

	g_simple_async_result_set_op_res_gpointer (
		simple, load_context,
		(GDestroyNotify) attachment_load_context_free);

	g_simple_async_result_complete (simple);

	/* Make sure it's freed on operation end. */
	g_clear_object (&load_context->simple);
}

static void
attachment_load_write_cb (GOutputStream *output_stream,
                          GAsyncResult *result,
                          LoadContext *load_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GInputStream *input_stream;
	gssize bytes_written;
	GError *error = NULL;

	bytes_written = g_output_stream_write_finish (
		output_stream, result, &error);

	if (attachment_load_check_for_error (load_context, error))
		return;

	attachment = load_context->attachment;
	cancellable = attachment->priv->cancellable;
	input_stream = load_context->input_stream;

	attachment_progress_cb (
		g_seekable_tell (G_SEEKABLE (output_stream)),
		load_context->total_num_bytes, attachment);

	if (bytes_written < load_context->bytes_read) {
		memmove (
			load_context->buffer,
			load_context->buffer + bytes_written,
			load_context->bytes_read - bytes_written);
		load_context->bytes_read -= bytes_written;

		g_output_stream_write_async (
			output_stream,
			load_context->buffer,
			load_context->bytes_read,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_load_write_cb,
			load_context);
	} else
		g_input_stream_read_async (
			input_stream,
			load_context->buffer,
			sizeof (load_context->buffer),
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_load_stream_read_cb,
			load_context);
}

static void
attachment_load_stream_read_cb (GInputStream *input_stream,
                                GAsyncResult *result,
                                LoadContext *load_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GOutputStream *output_stream;
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (
		input_stream, result, &error);

	if (attachment_load_check_for_error (load_context, error))
		return;

	if (bytes_read == 0) {
		attachment_load_finish (load_context);
		return;
	}

	attachment = load_context->attachment;
	cancellable = attachment->priv->cancellable;
	output_stream = load_context->output_stream;
	load_context->bytes_read = bytes_read;

	g_output_stream_write_async (
		output_stream,
		load_context->buffer,
		load_context->bytes_read,
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) attachment_load_write_cb,
		load_context);
}

static void
attachment_load_file_read_cb (GFile *file,
                              GAsyncResult *result,
                              LoadContext *load_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GFileInputStream *input_stream;
	GOutputStream *output_stream;
	GError *error = NULL;

	/* Input stream might be NULL, so don't use cast macro. */
	input_stream = g_file_read_finish (file, result, &error);
	load_context->input_stream = (GInputStream *) input_stream;

	if (attachment_load_check_for_error (load_context, error))
		return;

	/* Load the contents into a GMemoryOutputStream. */
	output_stream = g_memory_output_stream_new (
		NULL, 0, g_realloc, g_free);

	attachment = load_context->attachment;
	cancellable = attachment->priv->cancellable;
	load_context->output_stream = output_stream;

	g_input_stream_read_async (
		load_context->input_stream,
		load_context->buffer,
		sizeof (load_context->buffer),
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) attachment_load_stream_read_cb,
		load_context);
}

#ifdef HAVE_AUTOAR
static void
attachment_load_created_decide_dest_cb (AutoarCompressor *compressor,
                                        GFile *destination,
                                        EAttachment *attachment)
{
	e_attachment_set_file (attachment, destination);
}

static void
attachment_load_created_cancelled_cb (AutoarCompressor *compressor,
                                      LoadContext *load_context)
{
	attachment_load_check_for_error (load_context,
		g_error_new_literal (
			G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Operation was cancelled")));
	g_object_unref (compressor);
}

static void
attachment_load_created_completed_cb (AutoarCompressor *compressor,
                                      LoadContext *load_context)
{
	EAttachment *attachment;
	GFile *file;

	g_object_unref (compressor);

	/* We have set the file to the created temporary archive, so we can
	 * query info again and use the regular procedure to load the
	 * attachment. */
	attachment = load_context->attachment;
	file = e_attachment_ref_file (attachment);
	g_file_query_info_async (
		file, ATTACHMENT_QUERY,
		G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
		attachment->priv->cancellable, (GAsyncReadyCallback)
		attachment_load_query_info_cb, load_context);

	g_clear_object (&file);
}

static void
attachment_load_created_error_cb (AutoarCompressor *compressor,
                                  GError *error,
                                  LoadContext *load_context)
{
	attachment_load_check_for_error (load_context, g_error_copy (error));
	g_object_unref (compressor);
}
#endif

static void
attachment_load_query_info_cb (GFile *file,
                               GAsyncResult *result,
                               LoadContext *load_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GFileInfo *file_info;
	GError *error = NULL;

	attachment = load_context->attachment;
	cancellable = attachment->priv->cancellable;

	file_info = g_file_query_info_finish (file, result, &error);
	if (attachment_load_check_for_error (load_context, error))
		return;

	e_attachment_set_file_info (attachment, file_info);
	load_context->file_info = file_info;

	load_context->total_num_bytes = g_file_info_get_size (file_info);

#ifdef HAVE_AUTOAR
	if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
		AutoarCompressor *compressor;
		GFile *temporary;
		GSettings *settings;
		GList *files = NULL;
		char *format_string;
		char *filter_string;
		gint format;
		gint filter;

		temporary = attachment_get_temporary (&error);
		if (attachment_load_check_for_error (load_context, error))
			return;

		settings = e_util_ref_settings ("org.gnome.evolution.shell");

		format_string = g_settings_get_string (settings, "autoar-format");
		filter_string = g_settings_get_string (settings, "autoar-filter");

		if (!e_enum_from_string (AUTOAR_TYPE_FORMAT, format_string, &format)) {
			format = AUTOAR_FORMAT_ZIP;
		}
		if (!e_enum_from_string (AUTOAR_TYPE_FILTER, filter_string, &filter)) {
			filter = AUTOAR_FILTER_NONE;
		}

		files = g_list_prepend (files, file);

		compressor = autoar_compressor_new (
			files, temporary, format, filter, FALSE);
		g_signal_connect (compressor, "decide-dest",
			G_CALLBACK (attachment_load_created_decide_dest_cb), attachment);
		g_signal_connect (compressor, "cancelled",
			G_CALLBACK (attachment_load_created_cancelled_cb), load_context);
		g_signal_connect (compressor, "completed",
			G_CALLBACK (attachment_load_created_completed_cb), load_context);
		g_signal_connect (compressor, "error",
			G_CALLBACK (attachment_load_created_error_cb), load_context);
		autoar_compressor_start_async (compressor, cancellable);

		g_object_unref (settings);
		g_free (format_string);
		g_free (filter_string);
		g_list_free (files);
		g_object_unref (temporary);
	} else {
#endif
		g_file_read_async (
			file, G_PRIORITY_DEFAULT,
			cancellable, (GAsyncReadyCallback)
			attachment_load_file_read_cb, load_context);
#ifdef HAVE_AUTOAR
	}
#endif
}

#define ATTACHMENT_LOAD_CONTEXT "attachment-load-context-data"

static void
attachment_load_from_mime_part_thread (GSimpleAsyncResult *simple,
                                       GObject *object,
                                       GCancellable *cancellable)
{
	LoadContext *load_context;
	GFileInfo *file_info;
	EAttachment *attachment;
	CamelContentType *content_type;
	CamelMimePart *mime_part;
	const gchar *attribute;
	const gchar *string;
	gchar *allocated, *decoded_string = NULL;
	CamelStream *null;
	CamelDataWrapper *dw;

	load_context = g_object_get_data (
		G_OBJECT (simple), ATTACHMENT_LOAD_CONTEXT);
	g_return_if_fail (load_context != NULL);
	g_object_set_data (G_OBJECT (simple), ATTACHMENT_LOAD_CONTEXT, NULL);

	attachment = load_context->attachment;
	mime_part = e_attachment_ref_mime_part (attachment);

	file_info = g_file_info_new ();
	load_context->file_info = file_info;

	content_type = camel_mime_part_get_content_type (mime_part);
	allocated = camel_content_type_simple (content_type);
	if (allocated != NULL) {
		GIcon *icon;
		gchar *cp;

		/* GIO expects lowercase MIME types. */
		for (cp = allocated; *cp != '\0'; cp++)
			*cp = g_ascii_tolower (*cp);

		/* Swap the MIME type for a content type. */
		cp = g_content_type_from_mime_type (allocated);
		g_free (allocated);
		allocated = cp;

		/* Use the MIME part's filename if we have to. */
		if (g_content_type_is_unknown (allocated)) {
			string = camel_mime_part_get_filename (mime_part);
			if (string != NULL) {
				g_free (allocated);
				allocated = g_content_type_guess (
					string, NULL, 0, NULL);
			}
		}

		g_file_info_set_content_type (file_info, allocated);

		icon = g_content_type_get_icon (allocated);
		if (icon != NULL) {
			g_file_info_set_icon (file_info, icon);
			g_object_unref (icon);
		}
	}
	g_free (allocated);

	/* Strip any path components from the filename. */
	string = camel_mime_part_get_filename (mime_part);
	if (string == NULL) {
		/* Translators: Default attachment filename. */
		string = _("attachment.dat");

		if (camel_content_type_is (content_type, "message", "rfc822")) {
			CamelMimeMessage *msg = NULL;
			const gchar *subject = NULL;

			if (CAMEL_IS_MIME_MESSAGE (mime_part)) {
				msg = CAMEL_MIME_MESSAGE (mime_part);
			} else {
				CamelDataWrapper *content;

				content = camel_medium_get_content (
					CAMEL_MEDIUM (mime_part));
				if (CAMEL_IS_MIME_MESSAGE (content))
					msg = CAMEL_MIME_MESSAGE (content);
			}

			if (msg != NULL)
				subject = camel_mime_message_get_subject (msg);

			if (subject != NULL && *subject != '\0')
				string = subject;
		}
	} else {
		decoded_string = camel_header_decode_string (string, "UTF-8");
		if (decoded_string != NULL &&
		    *decoded_string != '\0' &&
		    !g_str_equal (decoded_string, string)) {
			string = decoded_string;
		} else {
			g_free (decoded_string);
			decoded_string = NULL;
		}
	}
	allocated = g_path_get_basename (string);
	g_file_info_set_display_name (file_info, allocated);
	g_free (decoded_string);
	g_free (allocated);

	attribute = G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION;
	string = camel_mime_part_get_description (mime_part);
	if (string != NULL)
		g_file_info_set_attribute_string (
			file_info, attribute, string);

	dw = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	null = camel_stream_null_new ();
	/* this actually downloads the part and makes it available later */
	camel_data_wrapper_decode_to_stream_sync (
		dw, null, attachment->priv->cancellable, NULL);
	g_file_info_set_size (file_info, CAMEL_STREAM_NULL (null)->written);
	g_object_unref (null);

	load_context->mime_part = g_object_ref (mime_part);

	/* Make sure it's freed on operation end. */
	g_clear_object (&load_context->simple);

	g_simple_async_result_set_op_res_gpointer (
		simple, load_context,
		(GDestroyNotify) attachment_load_context_free);

	g_clear_object (&mime_part);
}

void
e_attachment_load_async (EAttachment *attachment,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	LoadContext *load_context;
	GCancellable *cancellable;
	CamelMimePart *mime_part;
	GFile *file;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (e_attachment_get_loading (attachment)) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_BUSY,
			_("A load operation is already in progress"));
		return;
	}

	if (e_attachment_get_saving (attachment)) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_BUSY,
			_("A save operation is already in progress"));
		return;
	}

	file = e_attachment_ref_file (attachment);
	mime_part = e_attachment_ref_mime_part (attachment);
	g_return_if_fail (file != NULL || mime_part != NULL);

	load_context = attachment_load_context_new (
		attachment, callback, user_data);

	cancellable = attachment->priv->cancellable;
	g_cancellable_reset (cancellable);

	if (file != NULL) {
		g_file_query_info_async (
			file, ATTACHMENT_QUERY,
			G_FILE_QUERY_INFO_NONE,G_PRIORITY_DEFAULT,
			cancellable, (GAsyncReadyCallback)
			attachment_load_query_info_cb, load_context);

	} else if (mime_part != NULL) {
		g_object_set_data (
			G_OBJECT (load_context->simple),
			ATTACHMENT_LOAD_CONTEXT, load_context);

		g_simple_async_result_run_in_thread (
			load_context->simple,
			attachment_load_from_mime_part_thread,
			G_PRIORITY_DEFAULT,
			cancellable);
	}

	g_clear_object (&file);
	g_clear_object (&mime_part);
}

gboolean
e_attachment_load_finish (EAttachment *attachment,
                          GAsyncResult *result,
                          GError **error)
{
	GSimpleAsyncResult *simple;
	const LoadContext *load_context;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	if (g_simple_async_result_propagate_error (simple, error)) {
		attachment_set_loading (attachment, FALSE);
		return FALSE;
	}

	load_context = g_simple_async_result_get_op_res_gpointer (simple);

	if (load_context != NULL && load_context->mime_part != NULL) {
		const gchar *string;

		string = camel_mime_part_get_disposition (
			load_context->mime_part);
		e_attachment_set_disposition (attachment, string);

		e_attachment_set_file_info (
			attachment, load_context->file_info);
		e_attachment_set_mime_part (
			attachment, load_context->mime_part);
	}

	attachment_set_loading (attachment, FALSE);

	return (load_context != NULL);
}

void
e_attachment_load_handle_error (EAttachment *attachment,
                                GAsyncResult *result,
                                GtkWindow *parent)
{
	GtkWidget *dialog;
	GFileInfo *file_info;
	const gchar *display_name;
	const gchar *primary_text;
	GError *error = NULL;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	g_return_if_fail (!parent || GTK_IS_WINDOW (parent));

	if (e_attachment_load_finish (attachment, result, &error))
		return;

	g_signal_emit (attachment, signals[LOAD_FAILED], 0, NULL);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		return;
	}

	file_info = e_attachment_ref_file_info (attachment);

	if (file_info != NULL)
		display_name = g_file_info_get_display_name (file_info);
	else
		display_name = NULL;

	if (display_name != NULL)
		primary_text = g_strdup_printf (
			_("Could not load '%s'"), display_name);
	else
		primary_text = g_strdup_printf (
			_("Could not load the attachment"));

	g_clear_object (&file_info);

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>", primary_text);

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);
}

gboolean
e_attachment_load (EAttachment *attachment,
                   GError **error)
{
	EAsyncClosure *closure;
	GAsyncResult *result;
	gboolean success;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	closure = e_async_closure_new ();

	e_attachment_load_async (
		attachment, e_async_closure_callback, closure);

	result = e_async_closure_wait (closure);

	success = e_attachment_load_finish (attachment, result, error);

	e_async_closure_free (closure);

	return success;
}

/************************* e_attachment_open_async() *************************/

typedef struct _OpenContext OpenContext;

struct _OpenContext {
	EAttachment *attachment;
	GSimpleAsyncResult *simple;

	GAppInfo *app_info;
};

static OpenContext *
attachment_open_context_new (EAttachment *attachment,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	OpenContext *open_context;
	GSimpleAsyncResult *simple;

	simple = g_simple_async_result_new (
		G_OBJECT (attachment), callback,
		user_data, e_attachment_open_async);

	open_context = g_slice_new0 (OpenContext);
	open_context->attachment = g_object_ref (attachment);
	open_context->simple = simple;

	return open_context;
}

static void
attachment_open_context_free (OpenContext *open_context)
{
	g_object_unref (open_context->attachment);
	g_object_unref (open_context->simple);

	if (open_context->app_info != NULL)
		g_object_unref (open_context->app_info);

	g_slice_free (OpenContext, open_context);
}

static gboolean
attachment_open_check_for_error (OpenContext *open_context,
                                 GError *error)
{
	GSimpleAsyncResult *simple;

	if (error == NULL)
		return FALSE;

	simple = open_context->simple;
	g_simple_async_result_take_error (simple, error);
	g_simple_async_result_complete (simple);

	attachment_open_context_free (open_context);

	return TRUE;
}

static void
attachment_open_file (GFile *file,
                      OpenContext *open_context)
{
	GdkAppLaunchContext *context;
	GSimpleAsyncResult *simple;
	GdkDisplay *display;
	gboolean success;
	GError *error = NULL;

	simple = open_context->simple;

	display = gdk_display_get_default ();
	context = gdk_display_get_app_launch_context (display);

	if (open_context->app_info != NULL) {
		GList *file_list;

		file_list = g_list_prepend (NULL, file);
		success = g_app_info_launch (
			open_context->app_info, file_list,
			G_APP_LAUNCH_CONTEXT (context), &error);
		g_list_free (file_list);
	} else {
		gchar *uri;

		uri = g_file_get_uri (file);
		success = g_app_info_launch_default_for_uri (
			uri, G_APP_LAUNCH_CONTEXT (context), &error);
		g_free (uri);
	}

	g_object_unref (context);

	g_simple_async_result_set_op_res_gboolean (simple, success);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);

	g_simple_async_result_complete (simple);
	attachment_open_context_free (open_context);
}

static void
attachment_open_save_finished_cb (EAttachment *attachment,
                                  GAsyncResult *result,
                                  OpenContext *open_context)
{
	GFile *file;
	gchar *path;
	GError *error = NULL;

	file = e_attachment_save_finish (attachment, result, &error);

	if (attachment_open_check_for_error (open_context, error))
		return;

	/* Make the temporary file read-only.
	 *
	 * This step is non-critical, so if an error occurs just
	 * emit a warning and move on.
	 *
	 * XXX I haven't figured out how to do this through GIO.
	 *     Attempting to set the "access::can-write" attribute via
	 *     g_file_set_attribute() returned G_IO_ERROR_NOT_SUPPORTED
	 *     and the only other possibility I see is "unix::mode",
	 *     which is obviously not portable.
	 */
	path = g_file_get_path (file);
#ifndef G_OS_WIN32
	if (g_chmod (path, S_IRUSR | S_IRGRP | S_IROTH) < 0)
		g_warning ("%s", g_strerror (errno));
#endif
	g_free (path);

	attachment_open_file (file, open_context);
	g_object_unref (file);
}

static void
attachment_open_save_temporary (OpenContext *open_context)
{
	GFile *temp_directory;
	GError *error = NULL;

	temp_directory = attachment_get_temporary (&error);

	/* We already know if there's an error, but this does the cleanup. */
	if (attachment_open_check_for_error (open_context, error))
		return;

	e_attachment_save_async (
		open_context->attachment,
		temp_directory, (GAsyncReadyCallback)
		attachment_open_save_finished_cb, open_context);

	g_object_unref (temp_directory);
}

void
e_attachment_open_async (EAttachment *attachment,
                         GAppInfo *app_info,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	OpenContext *open_context;
	CamelMimePart *mime_part;
	GFile *file;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	file = e_attachment_ref_file (attachment);
	mime_part = e_attachment_ref_mime_part (attachment);
	g_return_if_fail (file != NULL || mime_part != NULL);

	open_context = attachment_open_context_new (
		attachment, callback, user_data);

	if (G_IS_APP_INFO (app_info))
		open_context->app_info = g_object_ref (app_info);

	/* If the attachment already references a GFile, we can launch
	 * the application directly.  Otherwise we have to save the MIME
	 * part to a temporary file and launch the application from that. */
	if (file != NULL) {
		attachment_open_file (file, open_context);

	} else if (mime_part != NULL)
		attachment_open_save_temporary (open_context);

	g_clear_object (&file);
	g_clear_object (&mime_part);
}

gboolean
e_attachment_open_finish (EAttachment *attachment,
                          GAsyncResult *result,
                          GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean success;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	success = !g_simple_async_result_propagate_error (simple, error) &&
		   g_simple_async_result_get_op_res_gboolean (simple);

	return success;
}

void
e_attachment_open_handle_error (EAttachment *attachment,
                                GAsyncResult *result,
                                GtkWindow *parent)
{
	GtkWidget *dialog;
	GFileInfo *file_info;
	const gchar *display_name;
	const gchar *primary_text;
	GError *error = NULL;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	if (e_attachment_open_finish (attachment, result, &error))
		return;

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	file_info = e_attachment_ref_file_info (attachment);

	if (file_info != NULL)
		display_name = g_file_info_get_display_name (file_info);
	else
		display_name = NULL;

	if (display_name != NULL)
		primary_text = g_strdup_printf (
			_("Could not open '%s'"), display_name);
	else
		primary_text = g_strdup_printf (
			_("Could not open the attachment"));

	g_clear_object (&file_info);

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>", primary_text);

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);
}

gboolean
e_attachment_open (EAttachment *attachment,
                   GAppInfo *app_info,
                   GError **error)
{
	EAsyncClosure *closure;
	GAsyncResult *result;
	gboolean success;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);

	closure = e_async_closure_new ();

	e_attachment_open_async (
		attachment, app_info,
		e_async_closure_callback, closure);

	result = e_async_closure_wait (closure);

	success = e_attachment_open_finish (attachment, result, error);

	e_async_closure_free (closure);

	return success;
}

/************************* e_attachment_save_async() *************************/

typedef struct _SaveContext SaveContext;

struct _SaveContext {
	EAttachment *attachment;
	GSimpleAsyncResult *simple;

	GFile *directory;
	GFile *destination;
	GInputStream *input_stream;
	GOutputStream *output_stream;
	goffset total_num_bytes;
	gssize bytes_read;
	gchar buffer[4096];
	gint count;

	GByteArray *input_buffer;
	gchar *suggested_destname;
	GFile *temporary_file;

	guint total_tasks : 2;
	guint completed_tasks : 2;
	guint prepared_tasks : 2;

	GMutex completed_tasks_mutex;
	GMutex prepared_tasks_mutex;
};

/* Forward Declaration */
static void
attachment_save_read_cb (GInputStream *input_stream,
                         GAsyncResult *result,
                         SaveContext *save_context);

static SaveContext *
attachment_save_context_new (EAttachment *attachment,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	SaveContext *save_context;
	GSimpleAsyncResult *simple;

	simple = g_simple_async_result_new (
		G_OBJECT (attachment), callback,
		user_data, e_attachment_save_async);

	save_context = g_slice_new0 (SaveContext);
	save_context->attachment = g_object_ref (attachment);
	save_context->simple = simple;

	g_mutex_init (&(save_context->completed_tasks_mutex));
	g_mutex_init (&(save_context->prepared_tasks_mutex));

	attachment_set_saving (save_context->attachment, TRUE);

	return save_context;
}

static void
attachment_save_context_free (SaveContext *save_context)
{
	g_object_unref (save_context->attachment);
	g_object_unref (save_context->simple);

	if (save_context->directory != NULL)
		g_object_unref (save_context->directory);

	if (save_context->destination != NULL)
		g_object_unref (save_context->destination);

	if (save_context->input_stream != NULL)
		g_object_unref (save_context->input_stream);

	if (save_context->output_stream != NULL)
		g_object_unref (save_context->output_stream);

	if (save_context->input_buffer != NULL)
		g_byte_array_unref (save_context->input_buffer);

	if (save_context->suggested_destname != NULL)
		g_free (save_context->suggested_destname);

	if (save_context->temporary_file != NULL)
		g_clear_object (&save_context->temporary_file);

	g_mutex_clear (&(save_context->completed_tasks_mutex));
	g_mutex_clear (&(save_context->prepared_tasks_mutex));

	g_slice_free (SaveContext, save_context);
}

static gboolean
attachment_save_check_for_error (SaveContext *save_context,
                                 GError *error)
{
	GSimpleAsyncResult *simple;

	if (error == NULL)
		return FALSE;

	simple = save_context->simple;
	g_simple_async_result_take_error (simple, error);

	g_mutex_lock (&(save_context->completed_tasks_mutex));
	if (++save_context->completed_tasks >= save_context->total_tasks) {
		g_simple_async_result_complete (simple);
		g_mutex_unlock (&(save_context->completed_tasks_mutex));
		attachment_save_context_free (save_context);
	} else {
		g_mutex_unlock (&(save_context->completed_tasks_mutex));
	}

	return TRUE;
}

static void
attachment_save_complete (SaveContext *save_context) {
	g_mutex_lock (&(save_context->completed_tasks_mutex));
	if (++save_context->completed_tasks >= save_context->total_tasks) {
		GSimpleAsyncResult *simple;
		GFile *result;

		/* Steal the destination. */
		result = save_context->destination;
		save_context->destination = NULL;

		if (result == NULL) {
			result = save_context->directory;
			save_context->directory = NULL;
		}

		simple = save_context->simple;
		g_simple_async_result_set_op_res_gpointer (
			simple, result, (GDestroyNotify) g_object_unref);
		g_simple_async_result_complete (simple);
		g_mutex_unlock (&(save_context->completed_tasks_mutex));
		attachment_save_context_free (save_context);
	} else {
		g_mutex_unlock (&(save_context->completed_tasks_mutex));
	}
}

static gchar *
get_new_name_with_count (const gchar *initial_name,
                         gint count)
{
	GString *string;
	const gchar *ext;
	gsize length;

	if (count == 0) {
		return g_strdup (initial_name);
	}

	string = g_string_sized_new (strlen (initial_name));
	ext = g_utf8_strchr (initial_name, -1, '.');

	if (ext != NULL)
		length = ext - initial_name;
	else
		length = strlen (initial_name);

	g_string_append_len (string, initial_name, length);
	g_string_append_printf (string, " (%d)", count);
	g_string_append (string, (ext != NULL) ? ext : "");

	return g_string_free (string, FALSE);
}

static GFile *
attachment_save_new_candidate (SaveContext *save_context)
{
	GFile *candidate;
	GFileInfo *file_info;
	EAttachment *attachment;
	const gchar *display_name = NULL;
	gchar *basename;

	attachment = save_context->attachment;
	file_info = e_attachment_ref_file_info (attachment);

	if (file_info != NULL)
		display_name = g_file_info_get_display_name (file_info);
	if (display_name == NULL)
		/* Translators: Default attachment filename. */
		display_name = _("attachment.dat");

	basename = get_new_name_with_count (display_name, save_context->count);

	save_context->count++;

	candidate = g_file_get_child (save_context->directory, basename);

	g_free (basename);

	g_clear_object (&file_info);

	return candidate;
}

static void
attachment_save_write_cb (GOutputStream *output_stream,
                          GAsyncResult *result,
                          SaveContext *save_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GInputStream *input_stream;
	gssize bytes_written;
	GError *error = NULL;

	bytes_written = g_output_stream_write_finish (
		output_stream, result, &error);

	if (attachment_save_check_for_error (save_context, error))
		return;

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;
	input_stream = save_context->input_stream;

	if (bytes_written < save_context->bytes_read) {
		memmove (
			save_context->buffer,
			save_context->buffer + bytes_written,
			save_context->bytes_read - bytes_written);
		save_context->bytes_read -= bytes_written;

		g_output_stream_write_async (
			output_stream,
			save_context->buffer,
			save_context->bytes_read,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_write_cb,
			save_context);
	} else
		g_input_stream_read_async (
			input_stream,
			save_context->buffer,
			sizeof (save_context->buffer),
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_read_cb,
			save_context);
}

static void
attachment_save_read_cb (GInputStream *input_stream,
                         GAsyncResult *result,
                         SaveContext *save_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GOutputStream *output_stream;
	gssize bytes_read;
	GError *error = NULL;

	bytes_read = g_input_stream_read_finish (
		input_stream, result, &error);

	if (attachment_save_check_for_error (save_context, error))
		return;

	if (bytes_read == 0) {
		attachment_save_complete (save_context);
		return;
	}

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;
	output_stream = save_context->output_stream;
	save_context->bytes_read = bytes_read;

	attachment_progress_cb (
		g_seekable_tell (G_SEEKABLE (input_stream)),
		save_context->total_num_bytes, attachment);

	g_output_stream_write_async (
		output_stream,
		save_context->buffer,
		save_context->bytes_read,
		G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) attachment_save_write_cb,
		save_context);
}

#ifdef HAVE_AUTOAR
static GFile*
attachment_save_extracted_decide_destination_cb (AutoarExtractor *extractor,
                                                 GFile *destination,
                                                 GList *files,
                                                 SaveContext *save_context)
{
	gchar *basename;
	GFile *destination_directory;
	GFile *new_destination;
	gint count = 0;

	basename = g_file_get_basename (destination);
	destination_directory = g_file_get_parent (destination);

	new_destination = g_object_ref (destination);

	while (g_file_query_exists (new_destination, NULL)) {
		gchar *new_basename;

		new_basename = get_new_name_with_count (basename, ++count);

		g_object_unref (new_destination);

		new_destination = g_file_get_child (
			destination_directory, new_basename);

		g_free (new_basename);
	}

	g_object_unref (destination_directory);
	g_free (basename);

	return new_destination;
}

static void
attachment_save_extracted_progress_cb (AutoarExtractor *extractor,
                                       guint64 completed_size,
                                       guint completed_files,
                                       SaveContext *save_context)
{
	attachment_progress_cb (
		autoar_extractor_get_total_size (extractor),
		completed_size, save_context->attachment);
}

static void
attachment_save_extracted_cancelled_cb (AutoarExtractor *extractor,
                                        SaveContext *save_context)
{
	attachment_save_check_for_error (save_context,
		g_error_new_literal (
			G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Operation was cancelled")));
	g_object_unref (extractor);
}

static void
attachment_save_extracted_completed_cb (AutoarExtractor *extractor,
                                        SaveContext *save_context)
{
	attachment_save_complete (save_context);
	g_object_unref (extractor);
}

static void
attachment_save_extracted_error_cb (AutoarExtractor *extractor,
                                    GError *error,
                                    SaveContext *save_context)
{
	attachment_save_check_for_error (save_context, g_error_copy (error));
	g_object_unref (extractor);
}

static void
attachament_save_write_archive_cb (GOutputStream *output_stream,
                                   GAsyncResult *result,
                                   SaveContext *save_context)
{
	AutoarExtractor *extractor;
	GError *error = NULL;
	gsize bytes_written;

	g_output_stream_write_all_finish (
		output_stream, result, &bytes_written, &error);

	g_object_unref (output_stream);

	if (attachment_save_check_for_error (save_context, error)) {
		return;
	}

	extractor = autoar_extractor_new (
		save_context->temporary_file, save_context->directory);

	autoar_extractor_set_delete_after_extraction (extractor, TRUE);

	g_signal_connect (extractor, "decide-destination",
		G_CALLBACK (attachment_save_extracted_decide_destination_cb),
		save_context);
	g_signal_connect (extractor, "progress",
		G_CALLBACK (attachment_save_extracted_progress_cb),
		save_context);
	g_signal_connect (extractor, "cancelled",
		G_CALLBACK (attachment_save_extracted_cancelled_cb),
		save_context);
	g_signal_connect (extractor, "error",
		G_CALLBACK (attachment_save_extracted_error_cb),
		save_context);
	g_signal_connect (extractor, "completed",
		G_CALLBACK (attachment_save_extracted_completed_cb),
		save_context);

	autoar_extractor_start_async (
		extractor, save_context->attachment->priv->cancellable);

	/* We do not g_object_unref (extractor); here because
	 * autoar_extractor_run_start_async () does not increase the
	 * reference count of extractor. We unref the object in
	 * callbacks instead. */
}

static void
attachment_save_create_archive_cb (GFile *file,
                                   GAsyncResult *result,
                                   SaveContext *save_context)
{
	GFileOutputStream *output_stream;
	GError *error = NULL;

	output_stream = g_file_create_finish (file, result, &error);

	if (attachment_save_check_for_error (save_context, error)) {
		return;
	}

	g_output_stream_write_all_async (
		G_OUTPUT_STREAM (output_stream),
		save_context->input_buffer->data,
		save_context->input_buffer->len,
		G_PRIORITY_DEFAULT,
		save_context->attachment->priv->cancellable,
		(GAsyncReadyCallback) attachament_save_write_archive_cb,
		save_context);
}

#endif

static void
attachment_save_got_output_stream (SaveContext *save_context)
{
	GCancellable *cancellable;
	GInputStream *input_stream;
	CamelDataWrapper *wrapper;
	CamelMimePart *mime_part;
	CamelStream *stream;
	EAttachment *attachment;
	GByteArray *buffer;

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;
	mime_part = e_attachment_ref_mime_part (attachment);

	/* Decode the MIME part to an in-memory buffer.  We have to do
	 * this because CamelStream is synchronous-only, and using threads
	 * is dangerous because CamelDataWrapper is not reentrant. */
	buffer = g_byte_array_new ();
	stream = camel_stream_mem_new ();
	camel_stream_mem_set_byte_array (CAMEL_STREAM_MEM (stream), buffer);
	wrapper = camel_medium_get_content (CAMEL_MEDIUM (mime_part));
	camel_data_wrapper_decode_to_stream_sync (wrapper, stream, NULL, NULL);
	g_object_unref (stream);

	save_context->input_buffer = buffer;

	if (attachment->priv->save_self) {
		/* Load the buffer into a GMemoryInputStream.
		 * But watch out for zero length MIME parts. */
		input_stream = g_memory_input_stream_new ();
		if (buffer->len > 0)
			g_memory_input_stream_add_data (
				G_MEMORY_INPUT_STREAM (input_stream),
				buffer->data, (gssize) buffer->len, NULL);
		save_context->input_stream = input_stream;
		save_context->total_num_bytes = (goffset) buffer->len;

		g_input_stream_read_async (
			input_stream,
			save_context->buffer,
			sizeof (save_context->buffer),
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_read_cb,
			save_context);
	}

#ifdef HAVE_AUTOAR
	if (attachment->priv->save_extracted) {
		GFile *temporary_directory;
		GError *error = NULL;

		temporary_directory = attachment_get_temporary (&error);
		if (attachment_save_check_for_error (save_context, error))
			return;

		save_context->temporary_file = g_file_get_child (
			temporary_directory, save_context->suggested_destname);

		g_file_create_async (
			save_context->temporary_file,
			G_FILE_CREATE_NONE,
			G_PRIORITY_DEFAULT,
			cancellable,
			(GAsyncReadyCallback) attachment_save_create_archive_cb,
			save_context);

		g_object_unref (temporary_directory);
	}
#endif

	g_clear_object (&mime_part);
}

static void
attachment_save_create_cb (GFile *destination,
                           GAsyncResult *result,
                           SaveContext *save_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GFileOutputStream *output_stream;
	GError *error = NULL;

	/* Output stream might be NULL, so don't use cast macro. */
	output_stream = g_file_create_finish (destination, result, &error);
	save_context->output_stream = (GOutputStream *) output_stream;

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
		destination = attachment_save_new_candidate (save_context);

		g_file_create_async (
			destination, G_FILE_CREATE_NONE,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_create_cb,
			save_context);

		g_object_unref (destination);
		g_error_free (error);
		return;
	}

	if (attachment_save_check_for_error (save_context, error))
		return;

	save_context->destination = g_object_ref (destination);

	g_mutex_lock (&(save_context->prepared_tasks_mutex));
	if (++save_context->prepared_tasks >= save_context->total_tasks)
		attachment_save_got_output_stream (save_context);
	g_mutex_unlock (&(save_context->prepared_tasks_mutex));
}

static void
attachment_save_replace_cb (GFile *destination,
                            GAsyncResult *result,
                            SaveContext *save_context)
{
	GFileOutputStream *output_stream;
	GError *error = NULL;

	/* Output stream might be NULL, so don't use cast macro. */
	output_stream = g_file_replace_finish (destination, result, &error);
	save_context->output_stream = (GOutputStream *) output_stream;

	if (attachment_save_check_for_error (save_context, error))
		return;

	save_context->destination = g_object_ref (destination);

	g_mutex_lock (&(save_context->prepared_tasks_mutex));
	if (++save_context->prepared_tasks >= save_context->total_tasks)
		attachment_save_got_output_stream (save_context);
	g_mutex_unlock (&(save_context->prepared_tasks_mutex));
}

static void
attachment_save_query_info_cb (GFile *destination,
                               GAsyncResult *result,
                               SaveContext *save_context)
{
	EAttachment *attachment;
	GCancellable *cancellable;
	GFileInfo *file_info;
	GFileType file_type;
	GError *error = NULL;

	attachment = save_context->attachment;
	cancellable = attachment->priv->cancellable;

	file_info = g_file_query_info_finish (destination, result, &error);

	/* G_IO_ERROR_NOT_FOUND just means we're creating a new file. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
		g_error_free (error);
		goto replace;
	}

	if (attachment_save_check_for_error (save_context, error))
		return;

	file_type = g_file_info_get_file_type (file_info);
	g_object_unref (file_info);

	if (file_type == G_FILE_TYPE_DIRECTORY) {
		save_context->directory = g_object_ref (destination);

		if (attachment->priv->save_self) {
			destination = attachment_save_new_candidate (save_context);

			g_file_create_async (
				destination, G_FILE_CREATE_NONE,
				G_PRIORITY_DEFAULT, cancellable,
				(GAsyncReadyCallback) attachment_save_create_cb,
				save_context);

			g_object_unref (destination);
		}

#ifdef HAVE_AUTOAR
		if (attachment->priv->save_extracted) {
			EAttachment *attachment;
			GFileInfo *info;
			gchar *suggested;

			attachment = save_context->attachment;
			suggested = NULL;
			info = e_attachment_ref_file_info (attachment);
			if (info != NULL)
				suggested = g_strdup (
					g_file_info_get_display_name (info));
			if (suggested == NULL)
				suggested = g_strdup (_("attachment.dat"));

			save_context->suggested_destname = suggested;

			g_mutex_lock (&(save_context->prepared_tasks_mutex));
			if (++save_context->prepared_tasks >= save_context->total_tasks)
				attachment_save_got_output_stream (save_context);
			g_mutex_unlock (&(save_context->prepared_tasks_mutex));
		}
#endif
		return;
	}

replace:
	if (attachment->priv->save_self) {
		g_file_replace_async (
			destination, NULL, FALSE,
			G_FILE_CREATE_REPLACE_DESTINATION,
			G_PRIORITY_DEFAULT, cancellable,
			(GAsyncReadyCallback) attachment_save_replace_cb,
			save_context);
	}

#ifdef HAVE_AUTOAR
	if (attachment->priv->save_extracted) {
		/* We can safely use save_context->directory here because
		 * attachment_save_replace_cb never calls
		 * attachment_save_new_candidate, the only function using
		 * the value of save_context->directory. */

		save_context->suggested_destname =
			g_file_get_basename (destination);
		save_context->directory = g_file_get_parent (destination);
		if (save_context->directory == NULL)
			save_context->directory = g_object_ref (destination);

		g_mutex_lock (&(save_context->prepared_tasks_mutex));
		if (++save_context->prepared_tasks >= save_context->total_tasks)
			attachment_save_got_output_stream (save_context);
		g_mutex_unlock (&(save_context->prepared_tasks_mutex));
	}
#endif
}

void
e_attachment_save_async (EAttachment *attachment,
                         GFile *destination,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	SaveContext *save_context;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (G_IS_FILE (destination));

	if (e_attachment_get_loading (attachment)) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_BUSY,
			_("A load operation is already in progress"));
		return;
	}

	if (e_attachment_get_saving (attachment)) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_BUSY,
			_("A save operation is already in progress"));
		return;
	}

	/* Just peek, don't reference. */
	if (attachment->priv->mime_part == NULL) {
		g_simple_async_report_error_in_idle (
			G_OBJECT (attachment), callback, user_data,
			G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			_("Attachment contents not loaded"));
		return;
	}

	save_context = attachment_save_context_new (
		attachment, callback, user_data);

	/* No task is not allowed. */
	if (!attachment->priv->save_self && !attachment->priv->save_extracted)
		attachment->priv->save_self = TRUE;

	if (attachment->priv->save_self)
		save_context->total_tasks++;
#ifdef HAVE_AUTOAR
	if (attachment->priv->save_extracted)
		save_context->total_tasks++;
#endif

	cancellable = attachment->priv->cancellable;
	g_cancellable_reset (cancellable);

	/* First we need to know if destination is a directory. */
	g_file_query_info_async (
		destination, G_FILE_ATTRIBUTE_STANDARD_TYPE,
		G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
		cancellable, (GAsyncReadyCallback)
		attachment_save_query_info_cb, save_context);
}

GFile *
e_attachment_save_finish (EAttachment *attachment,
                          GAsyncResult *result,
                          GError **error)
{
	GSimpleAsyncResult *simple;
	GFile *destination;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), NULL);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (result), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	if (g_simple_async_result_propagate_error (simple, error)) {
		attachment_set_saving (attachment, FALSE);
		return NULL;
	}

	destination = g_simple_async_result_get_op_res_gpointer (simple);
	if (destination != NULL)
		g_object_ref (destination);

	attachment_set_saving (attachment, FALSE);

	return destination;
}

void
e_attachment_save_handle_error (EAttachment *attachment,
                                GAsyncResult *result,
                                GtkWindow *parent)
{
	GFile *file;
	GFileInfo *file_info;
	GtkWidget *dialog;
	const gchar *display_name;
	const gchar *primary_text;
	GError *error = NULL;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	file = e_attachment_save_finish (attachment, result, &error);

	if (file != NULL) {
		g_object_unref (file);
		return;
	}

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	file_info = e_attachment_ref_file_info (attachment);

	if (file_info != NULL)
		display_name = g_file_info_get_display_name (file_info);
	else
		display_name = NULL;

	if (display_name != NULL)
		primary_text = g_strdup_printf (
			_("Could not save '%s'"), display_name);
	else
		primary_text = g_strdup_printf (
			_("Could not save the attachment"));

	g_clear_object (&file_info);

	dialog = gtk_message_dialog_new_with_markup (
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
		"<big><b>%s</b></big>", primary_text);

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_error_free (error);
}

gboolean
e_attachment_save (EAttachment *attachment,
                   GFile *in_destination,
                   GFile **out_destination,
                   GError **error)
{
	EAsyncClosure *closure;
	GAsyncResult *result;

	g_return_val_if_fail (E_IS_ATTACHMENT (attachment), FALSE);
	g_return_val_if_fail (out_destination != NULL, FALSE);

	closure = e_async_closure_new ();

	e_attachment_save_async (
		attachment, in_destination,
		e_async_closure_callback, closure);

	result = e_async_closure_wait (closure);

	*out_destination =
		e_attachment_save_finish (attachment, result, error);

	e_async_closure_free (closure);

	return *out_destination != NULL;
}
