/*
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

#include "evolution-config.h"

#include "e-composer-post-header.h"

#include <string.h>
#include <glib/gi18n.h>
#include <camel/camel.h>

enum {
	PROP_0,
	PROP_MAIL_ACCOUNT
};

struct _EComposerPostHeaderPrivate {
	ESource *mail_account;
	gchar *base_url;  /* derived from account */
	gboolean custom;
};

G_DEFINE_TYPE_WITH_PRIVATE (EComposerPostHeader, e_composer_post_header, E_TYPE_COMPOSER_TEXT_HEADER)

static gchar *
composer_post_header_folder_name_to_string (EComposerPostHeader *header,
                                            const gchar *url)
{
	gchar *res = NULL;
	const gchar *base_url = header->priv->base_url;

	if (base_url != NULL) {
		gsize length = strlen (base_url);

		if (g_ascii_strncasecmp (url, base_url, length) == 0) {
			res = g_uri_unescape_string (url + length, NULL);
			if (!res)
				res = g_strdup (url + length);
		}
	}

	if (!res) {
		res = g_uri_unescape_string (url, NULL);
		if (!res)
			res = g_strdup (url);
	}

	return res;
}

static void
composer_post_header_set_base_url (EComposerPostHeader *header)
{
	ESource *source;
	const gchar *uid;

	source = header->priv->mail_account;
	if (source == NULL)
		return;

	uid = e_source_get_uid (source);
	g_free (header->priv->base_url);
	header->priv->base_url = g_strdup_printf ("folder://%s", uid);
}

static GList *
composer_post_header_split_csv (const gchar *csv)
{
	GList *list = NULL;
	gchar **strv;
	guint length, ii;

	strv = g_strsplit (csv, ",", 0);
	length = g_strv_length (strv);

	for (ii = 0; ii < length; ii++)
		if (*g_strstrip (strv[ii]) != '\0')
			list = g_list_prepend (list, g_strdup (strv[ii]));

	g_strfreev (strv);

	return g_list_reverse (list);
}

static void
composer_post_header_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MAIL_ACCOUNT:
			e_composer_post_header_set_mail_account (
				E_COMPOSER_POST_HEADER (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_post_header_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MAIL_ACCOUNT:
			g_value_set_object (
				value,
				e_composer_post_header_get_mail_account (
				E_COMPOSER_POST_HEADER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_post_header_dispose (GObject *object)
{
	EComposerPostHeader *self = E_COMPOSER_POST_HEADER (object);

	g_clear_object (&self->priv->mail_account);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_composer_post_header_parent_class)->dispose (object);
}

static void
composer_post_header_finalize (GObject *object)
{
	EComposerPostHeader *self = E_COMPOSER_POST_HEADER (object);

	g_free (self->priv->base_url);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_composer_post_header_parent_class)->finalize (object);
}

static void
composer_post_header_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_composer_post_header_parent_class)->constructed (object);

	e_composer_header_set_title_tooltip (
		E_COMPOSER_HEADER (object),
		_("Click here to select folders to post to"));
}

static void
composer_post_header_changed (EComposerHeader *header)
{
	EComposerPostHeader *self = E_COMPOSER_POST_HEADER (header);

	self->priv->custom = TRUE;
}

static void
composer_post_header_clicked (EComposerHeader *header)
{
	EComposerPostHeader *self = E_COMPOSER_POST_HEADER (header);

	self->priv->custom = FALSE;
}

static void
e_composer_post_header_class_init (EComposerPostHeaderClass *class)
{
	GObjectClass *object_class;
	EComposerHeaderClass *header_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = composer_post_header_set_property;
	object_class->get_property = composer_post_header_get_property;
	object_class->dispose = composer_post_header_dispose;
	object_class->finalize = composer_post_header_finalize;
	object_class->constructed = composer_post_header_constructed;

	header_class = E_COMPOSER_HEADER_CLASS (class);
	header_class->changed = composer_post_header_changed;
	header_class->clicked = composer_post_header_clicked;

	g_object_class_install_property (
		object_class,
		PROP_MAIL_ACCOUNT,
		g_param_spec_object (
			"mail-account",
			NULL,
			NULL,
			E_TYPE_SOURCE,
			G_PARAM_READWRITE));
}

static void
e_composer_post_header_init (EComposerPostHeader *header)
{
	header->priv = e_composer_post_header_get_instance_private (header);
}

EComposerHeader *
e_composer_post_header_new (ESourceRegistry *registry,
                            const gchar *label)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_COMPOSER_POST_HEADER,
		"label", label, "button", TRUE,
		"registry", registry, NULL);
}

GList *
e_composer_post_header_get_folders (EComposerPostHeader *header)
{
	GList *folders, *iter;
	gchar *base_url;

	g_return_val_if_fail (E_IS_COMPOSER_POST_HEADER (header), NULL);

	folders = composer_post_header_split_csv (
		e_composer_text_header_get_text (
		E_COMPOSER_TEXT_HEADER (header)));

	base_url = header->priv->base_url;
	if (base_url == NULL)
		return folders;

	for (iter = folders; iter != NULL; iter = iter->next) {
		/* Convert relative folder names to absolute. */
		/* XXX Should use CamelURL for this. */
		if (strstr (iter->data, ":/") == NULL) {
			gchar *abs_url;

			abs_url = g_strconcat (base_url, iter->data, NULL);
			g_free (iter->data);
			iter->data = abs_url;
		}
	}

	return folders;
}

void
e_composer_post_header_set_folders (EComposerPostHeader *header,
                                    GList *folders)
{
	GList *iter;
	gint ii = 0;
	gchar **strv;
	gchar *text;
	gboolean custom_save;

	g_return_if_fail (E_IS_COMPOSER_POST_HEADER (header));

	strv = g_new0 (gchar *, g_list_length (folders) + 1);

	for (iter = folders; iter != NULL; iter = iter->next)
		strv[ii++] = composer_post_header_folder_name_to_string (
			header, iter->data);

	text = g_strjoinv (", ", strv);
	custom_save = header->priv->custom;
	e_composer_text_header_set_text (
		E_COMPOSER_TEXT_HEADER (header), text);
	header->priv->custom = custom_save;
	g_free (text);

	g_strfreev (strv);
}

void
e_composer_post_header_set_folders_base (EComposerPostHeader *header,
                                         const gchar *base_url,
                                         const gchar *folders)
{
	GList *list, *iter;

	list = composer_post_header_split_csv (folders);
	for (iter = list; iter != NULL; iter = iter->next) {
		gchar *abs_url;

		/* FIXME This doesn't handle all folder names properly. */
		abs_url = g_strdup_printf (
			"%s/%s", base_url, (gchar *) iter->data);
		g_free (iter->data);
		iter->data = abs_url;
	}

	e_composer_post_header_set_folders (header, list);
	g_list_foreach (list, (GFunc) g_free, NULL);
	g_list_free (list);
}

ESource *
e_composer_post_header_get_mail_account (EComposerPostHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_POST_HEADER (header), NULL);

	return header->priv->mail_account;
}

void
e_composer_post_header_set_mail_account (EComposerPostHeader *header,
                                         ESource *mail_account)
{
	GList *folders = NULL;

	g_return_if_fail (E_IS_COMPOSER_POST_HEADER (header));

	if (header->priv->mail_account == mail_account)
		return;

	if (mail_account != NULL) {
		g_return_if_fail (E_IS_SOURCE (mail_account));
		g_object_ref (mail_account);
	}

	if (!header->priv->custom)
		folders = e_composer_post_header_get_folders (header);

	if (header->priv->mail_account != NULL)
		g_object_unref (header->priv->mail_account);

	header->priv->mail_account = mail_account;
	composer_post_header_set_base_url (header);

	/* Make folders relative to the new account. */
	if (!header->priv->custom) {
		e_composer_post_header_set_folders (header, folders);
		g_list_foreach (folders, (GFunc) g_free, NULL);
		g_list_free (folders);
	}

	g_object_notify (G_OBJECT (header), "mail-account");
}
