/*
 * e-signature-preview.c
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-signature-preview.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include "e-util/e-signature-utils.h"

#define E_SIGNATURE_PREVIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SIGNATURE_PREVIEW, ESignaturePreviewPrivate))

enum {
	PROP_0,
	PROP_ALLOW_SCRIPTS,
	PROP_SIGNATURE
};

enum {
	REFRESH,
	LAST_SIGNAL
};

struct _ESignaturePreviewPrivate {
	ESignature *signature;
	guint allow_scripts : 1;
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	ESignaturePreview,
	e_signature_preview,
	E_TYPE_WEB_VIEW)

static void
signature_preview_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALLOW_SCRIPTS:
			e_signature_preview_set_allow_scripts (
				E_SIGNATURE_PREVIEW (object),
				g_value_get_boolean (value));
			return;

		case PROP_SIGNATURE:
			e_signature_preview_set_signature (
				E_SIGNATURE_PREVIEW (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_preview_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALLOW_SCRIPTS:
			g_value_set_boolean (
				value, e_signature_preview_get_allow_scripts (
				E_SIGNATURE_PREVIEW (object)));
			return;

		case PROP_SIGNATURE:
			g_value_set_object (
				value, e_signature_preview_get_signature (
				E_SIGNATURE_PREVIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
signature_preview_dispose (GObject *object)
{
	ESignaturePreviewPrivate *priv;

	priv = E_SIGNATURE_PREVIEW_GET_PRIVATE (object);

	if (priv->signature != NULL) {
		g_object_unref (priv->signature);
		priv->signature = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_signature_preview_parent_class)->dispose (object);
}

static void
signature_preview_refresh (ESignaturePreview *preview)
{
	EWebView *web_view;
	ESignature *signature;
	const gchar *filename;
	gboolean is_script;
	gchar *content = NULL;

	/* XXX We should show error messages in the preview. */

	web_view = E_WEB_VIEW (preview);
	signature = e_signature_preview_get_signature (preview);

	if (signature == NULL)
		goto clear;

	filename = e_signature_get_filename (signature);
	is_script = e_signature_get_is_script (signature);

	if (is_script && !preview->priv->allow_scripts)
		goto clear;

	if (is_script)
		content = e_run_signature_script (filename);
	else
		content = e_read_signature_file (signature, FALSE, NULL);

	if (content == NULL || *content == '\0')
		goto clear;

	if (e_signature_get_is_html (signature))
		e_web_view_load_string (web_view, content);
	else {
		gchar *string;

		string = g_strdup_printf ("<PRE>%s</PRE>", content);
		e_web_view_load_string (web_view, string);
		g_free (string);
	}

	g_free (content);

	return;

clear:
	e_web_view_clear (web_view);

	g_free (content);
}

static void
e_signature_preview_class_init (ESignaturePreviewClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ESignaturePreviewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = signature_preview_set_property;
	object_class->get_property = signature_preview_get_property;
	object_class->dispose = signature_preview_dispose;

	class->refresh = signature_preview_refresh;

	g_object_class_install_property (
		object_class,
		PROP_ALLOW_SCRIPTS,
		g_param_spec_boolean (
			"allow-scripts",
			"Allow Scripts",
			NULL,
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	g_object_class_install_property (
		object_class,
		PROP_SIGNATURE,
		g_param_spec_object (
			"signature",
			"Signature",
			NULL,
			E_TYPE_SIGNATURE,
			G_PARAM_READWRITE));

	signals[REFRESH] = g_signal_new (
		"refresh",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (ESignaturePreviewClass, refresh),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_signature_preview_init (ESignaturePreview *preview)
{
	preview->priv = E_SIGNATURE_PREVIEW_GET_PRIVATE (preview);
}

GtkWidget *
e_signature_preview_new (void)
{
	return g_object_new (E_TYPE_SIGNATURE_PREVIEW, NULL);
}

void
e_signature_preview_refresh (ESignaturePreview *preview)
{
	g_return_if_fail (E_IS_SIGNATURE_PREVIEW (preview));

	g_signal_emit (preview, signals[REFRESH], 0);
}

gboolean
e_signature_preview_get_allow_scripts (ESignaturePreview *preview)
{
	g_return_val_if_fail (E_IS_SIGNATURE_PREVIEW (preview), FALSE);

	return preview->priv->allow_scripts;
}

void
e_signature_preview_set_allow_scripts (ESignaturePreview *preview,
                                       gboolean allow_scripts)
{
	g_return_if_fail (E_IS_SIGNATURE_PREVIEW (preview));

	preview->priv->allow_scripts = allow_scripts;
	g_object_notify (G_OBJECT (preview), "allow-scripts");
}

ESignature *
e_signature_preview_get_signature (ESignaturePreview *preview)
{
	g_return_val_if_fail (E_IS_SIGNATURE_PREVIEW (preview), NULL);

	return preview->priv->signature;
}

void
e_signature_preview_set_signature (ESignaturePreview *preview,
                                   ESignature *signature)
{
	g_return_if_fail (E_IS_SIGNATURE_PREVIEW (preview));

	if (signature != NULL) {
		g_return_if_fail (E_IS_SIGNATURE (signature));
		g_object_ref (signature);
	}

	if (preview->priv->signature != NULL)
		g_object_unref (preview->priv->signature);

	preview->priv->signature = signature;
	g_object_notify (G_OBJECT (preview), "signature");

	e_signature_preview_refresh (preview);
}
