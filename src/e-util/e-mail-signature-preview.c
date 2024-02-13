/*
 * e-mail-signature-preview.c
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

#include "e-mail-signature-preview.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <glib/gstdio.h>

#include "e-alert-sink.h"

#define SOURCE_IS_MAIL_SIGNATURE(source) \
	(e_source_has_extension ((source), E_SOURCE_EXTENSION_MAIL_SIGNATURE))

struct _EMailSignaturePreviewPrivate {
	ESourceRegistry *registry;
	GCancellable *cancellable;
	gchar *source_uid;
	gboolean webprocess_crashed;
};

enum {
	PROP_0,
	PROP_REGISTRY,
	PROP_SOURCE_UID
};

enum {
	REFRESH,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EMailSignaturePreview, e_mail_signature_preview, E_TYPE_WEB_VIEW)

static void
mail_signature_preview_load_cb (ESource *source,
                                GAsyncResult *result,
                                EMailSignaturePreview *preview)
{
	ESourceMailSignature *extension;
	const gchar *extension_name;
	const gchar *mime_type;
	gchar *contents = NULL;
	GError *error = NULL;

	e_source_mail_signature_load_finish (
		source, result, &contents, NULL, &error);

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (contents == NULL);
		g_object_unref (preview);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (contents == NULL);
		e_alert_submit (
			E_ALERT_SINK (preview),
			"widgets:no-load-signature",
			error->message, NULL);
		g_object_unref (preview);
		g_error_free (error);
		return;
	}

	g_return_if_fail (contents != NULL);

	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	extension = e_source_get_extension (source, extension_name);
	mime_type = e_source_mail_signature_get_mime_type (extension);

	if (g_strcmp0 (mime_type, "text/html") == 0) {
		e_web_view_load_string (E_WEB_VIEW (preview), contents);
	} else {
		gchar *string;

		string = g_markup_printf_escaped ("<pre>%s</pre>", contents);
		e_web_view_load_string (E_WEB_VIEW (preview), string);
		g_free (string);
	}

	g_free (contents);

	g_object_unref (preview);
}

static void
mail_signature_preview_web_process_terminated_cb (EMailSignaturePreview *preview,
						  WebKitWebProcessTerminationReason reason)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_PREVIEW (preview));

	if (preview->priv->webprocess_crashed)
		return;

	preview->priv->webprocess_crashed = TRUE;

	/* Should not use the EWebView, because it places the alerts inside itself,
	   but no better place here. Thus show the error only once, to avoid endless
	   repeating. */
	e_alert_submit (E_ALERT_SINK (preview), "mail:webkit-web-process-crashed-signature", NULL);
}

static void
mail_signature_preview_set_registry (EMailSignaturePreview *preview,
                                     ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (preview->priv->registry == NULL);

	preview->priv->registry = g_object_ref (registry);
}

static void
mail_signature_preview_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			mail_signature_preview_set_registry (
				E_MAIL_SIGNATURE_PREVIEW (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE_UID:
			e_mail_signature_preview_set_source_uid (
				E_MAIL_SIGNATURE_PREVIEW (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_preview_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_signature_preview_get_registry (
				E_MAIL_SIGNATURE_PREVIEW (object)));
			return;

		case PROP_SOURCE_UID:
			g_value_set_string (
				value,
				e_mail_signature_preview_get_source_uid (
				E_MAIL_SIGNATURE_PREVIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_preview_dispose (GObject *object)
{
	EMailSignaturePreview *self = E_MAIL_SIGNATURE_PREVIEW (object);

	g_clear_object (&self->priv->registry);

	if (self->priv->cancellable != NULL) {
		g_cancellable_cancel (self->priv->cancellable);
		g_clear_object (&self->priv->cancellable);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_signature_preview_parent_class)->dispose (object);
}

static void
mail_signature_preview_finalize (GObject *object)
{
	EMailSignaturePreview *self = E_MAIL_SIGNATURE_PREVIEW (object);

	g_free (self->priv->source_uid);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_signature_preview_parent_class)->finalize (object);
}

static void
mail_signature_preview_refresh (EMailSignaturePreview *preview)
{
	ESourceRegistry *registry;
	ESource *source;
	const gchar *extension_name;
	const gchar *source_uid;

	/* Cancel any unfinished refreshes. */
	if (preview->priv->cancellable != NULL) {
		g_cancellable_cancel (preview->priv->cancellable);
		g_object_unref (preview->priv->cancellable);
		preview->priv->cancellable = NULL;
	}

	source_uid = e_mail_signature_preview_get_source_uid (preview);

	if (source_uid == NULL)
		goto fail;

	registry = e_mail_signature_preview_get_registry (preview);
	source = e_source_registry_ref_source (registry, source_uid);

	if (source == NULL)
		goto fail;

	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		goto fail;
	}

	preview->priv->cancellable = g_cancellable_new ();

	e_source_mail_signature_load (
		source, G_PRIORITY_DEFAULT,
		preview->priv->cancellable, (GAsyncReadyCallback)
		mail_signature_preview_load_cb, g_object_ref (preview));

	g_object_unref (source);

	return;

fail:
	e_web_view_clear (E_WEB_VIEW (preview));
}

static void
e_mail_signature_preview_class_init (EMailSignaturePreviewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_signature_preview_set_property;
	object_class->get_property = mail_signature_preview_get_property;
	object_class->dispose = mail_signature_preview_dispose;
	object_class->finalize = mail_signature_preview_finalize;

	class->refresh = mail_signature_preview_refresh;

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_UID,
		g_param_spec_string (
			"source-uid",
			"Source UID",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signals[REFRESH] = g_signal_new (
		"refresh",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailSignaturePreviewClass, refresh),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_signature_preview_init (EMailSignaturePreview *preview)
{
	preview->priv = e_mail_signature_preview_get_instance_private (preview);
	preview->priv->webprocess_crashed = FALSE;

	g_signal_connect (
		preview, "web-process-terminated",
		G_CALLBACK (mail_signature_preview_web_process_terminated_cb), NULL);
}

GtkWidget *
e_mail_signature_preview_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_MAIL_SIGNATURE_PREVIEW,
		"registry", registry, NULL);
}

void
e_mail_signature_preview_refresh (EMailSignaturePreview *preview)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_PREVIEW (preview));

	g_signal_emit (preview, signals[REFRESH], 0);
}

ESourceRegistry *
e_mail_signature_preview_get_registry (EMailSignaturePreview *preview)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_PREVIEW (preview), NULL);

	return preview->priv->registry;
}

const gchar *
e_mail_signature_preview_get_source_uid (EMailSignaturePreview *preview)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_PREVIEW (preview), NULL);

	return preview->priv->source_uid;
}

void
e_mail_signature_preview_set_source_uid (EMailSignaturePreview *preview,
                                         const gchar *source_uid)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_PREVIEW (preview));

	/* Avoid repeatedly loading the same signature file. */
	if (g_strcmp0 (source_uid, preview->priv->source_uid) == 0)
		return;

	g_free (preview->priv->source_uid);
	preview->priv->source_uid = g_strdup (source_uid);

	g_object_notify (G_OBJECT (preview), "source-uid");

	e_mail_signature_preview_refresh (preview);
}
