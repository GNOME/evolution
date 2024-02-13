/*
 * e-mail-part-headers.c
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

#include <glib/gi18n-lib.h>

#include "e-mail-part-list.h"
#include "e-mail-part-headers.h"

struct _EMailPartHeadersPrivate {
	GMutex property_lock;
	gchar **default_headers;
	GtkTreeModel *print_model;
};

enum {
	PROP_0,
	PROP_DEFAULT_HEADERS
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailPartHeaders, e_mail_part_headers, E_TYPE_MAIL_PART)

static const gchar *basic_headers[] = {
	N_("From"),
	N_("Reply-To"),
	N_("To"),
	N_("Cc"),
	N_("Bcc"),
	N_("Subject"),
	N_("Date"),
	N_("Newsgroups"),
	N_("Face"),
	NULL
};

static GtkTreeModel *
mail_part_headers_build_print_model (EMailPartHeaders *part)
{
	GtkListStore *list_store;
	CamelMimePart *mime_part;
	const CamelNameValueArray *headers;
	gint default_position = 0;
	guint ii, length = 0;

	list_store = gtk_list_store_new (
		E_MAIL_PART_HEADERS_PRINT_MODEL_NUM_COLUMNS,
		G_TYPE_BOOLEAN,  /* INCLUDE */
		G_TYPE_STRING,   /* HEADER_NAME */
		G_TYPE_STRING);  /* HEADER_VALUE */

	mime_part = e_mail_part_ref_mime_part (E_MAIL_PART (part));
	headers = camel_medium_get_headers (CAMEL_MEDIUM (mime_part));
	length = camel_name_value_array_get_length (headers);

	for (ii = 0; ii < length; ii++) {
		GtkTreeIter iter;
		gboolean include = FALSE;
		gint position = -1;
		const gchar *header_name = NULL;
		const gchar *header_value = NULL;

		if (!camel_name_value_array_get (headers, ii, &header_name, &header_value) ||
		    !header_name || !header_value)
			continue;

		/* EMailFormatterPrintHeaders excludes "Subject" from
		 * its header table (because it puts it in an <h1> tag
		 * at the top of the page), so we'll exclude it too. */
		if (g_ascii_strncasecmp (header_name, "Subject", 7) == 0)
			continue;

		/* Also skip the 'Face' header, which includes only
		   base64 encoded data anyway. */
		if (g_ascii_strcasecmp (header_value, "Face") == 0)
			continue;

		/* Arrange default headers first and select them to be
		 * included in the final printout.  All other headers
		 * are excluded by default in the final printout. */
		if (e_mail_part_headers_is_default (part, header_name)) {
			position = default_position++;
			include = TRUE;
		}

		gtk_list_store_insert (list_store, &iter, position);

		gtk_list_store_set (
			list_store, &iter,
			E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_INCLUDE,
			include,
			E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_NAME,
			header_name,
			E_MAIL_PART_HEADERS_PRINT_MODEL_COLUMN_HEADER_VALUE,
			header_value,
			-1);
	}

	g_object_unref (mime_part);

	/* Stash the print model internally. */

	g_mutex_lock (&part->priv->property_lock);

	g_clear_object (&part->priv->print_model);
	part->priv->print_model = GTK_TREE_MODEL (g_object_ref (list_store));

	g_mutex_unlock (&part->priv->property_lock);

	return GTK_TREE_MODEL (list_store);
}

static void
mail_part_headers_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_HEADERS:
			e_mail_part_headers_set_default_headers (
				E_MAIL_PART_HEADERS (object),
				g_value_get_boxed (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_headers_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_HEADERS:
			g_value_take_boxed (
				value,
				e_mail_part_headers_dup_default_headers (
				E_MAIL_PART_HEADERS (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_part_headers_dispose (GObject *object)
{
	EMailPartHeaders *self = E_MAIL_PART_HEADERS (object);

	g_clear_object (&self->priv->print_model);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_part_headers_parent_class)->dispose (object);
}

static void
mail_part_headers_finalize (GObject *object)
{
	EMailPartHeaders *self = E_MAIL_PART_HEADERS (object);

	g_mutex_clear (&self->priv->property_lock);

	g_strfreev (self->priv->default_headers);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_part_headers_parent_class)->finalize (object);
}

static void
mail_part_headers_constructed (GObject *object)
{
	EMailPart *part;

	part = E_MAIL_PART (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_part_headers_parent_class)->constructed (object);

	e_mail_part_set_mime_type (part, E_MAIL_PART_HEADERS_MIME_TYPE);
}

static void
e_mail_part_headers_class_init (EMailPartHeadersClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_part_headers_set_property;
	object_class->get_property = mail_part_headers_get_property;
	object_class->dispose = mail_part_headers_dispose;
	object_class->finalize = mail_part_headers_finalize;
	object_class->constructed = mail_part_headers_constructed;

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_HEADERS,
		g_param_spec_boxed (
			"default-headers",
			"Default Headers",
			"Headers to display by default",
			G_TYPE_STRV,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_part_headers_init (EMailPartHeaders *part)
{
	part->priv = e_mail_part_headers_get_instance_private (part);

	g_mutex_init (&part->priv->property_lock);
}

EMailPart *
e_mail_part_headers_new (CamelMimePart *mime_part,
                         const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_HEADERS,
		"id", id, "mime-part", mime_part, NULL);
}

gchar **
e_mail_part_headers_dup_default_headers (EMailPartHeaders *part)
{
	gchar **default_headers;

	g_return_val_if_fail (E_IS_MAIL_PART_HEADERS (part), NULL);

	g_mutex_lock (&part->priv->property_lock);

	default_headers = g_strdupv (part->priv->default_headers);

	g_mutex_unlock (&part->priv->property_lock);

	return default_headers;
}

void
e_mail_part_headers_set_default_headers (EMailPartHeaders *part,
                                         const gchar * const *default_headers)
{
	g_return_if_fail (E_IS_MAIL_PART_HEADERS (part));

	if (default_headers == NULL)
		default_headers = basic_headers;

	g_mutex_lock (&part->priv->property_lock);

	g_strfreev (part->priv->default_headers);
	part->priv->default_headers = g_strdupv ((gchar **) default_headers);

	g_mutex_unlock (&part->priv->property_lock);

	g_object_notify (G_OBJECT (part), "default-headers");
}

gboolean
e_mail_part_headers_is_default (EMailPartHeaders *part,
                                const gchar *header_name)
{
	gboolean is_default = FALSE;
	guint ii, length = 0;

	g_return_val_if_fail (E_IS_MAIL_PART_HEADERS (part), FALSE);
	g_return_val_if_fail (header_name != NULL, FALSE);

	g_mutex_lock (&part->priv->property_lock);

	if (part->priv->default_headers != NULL)
		length = g_strv_length (part->priv->default_headers);

	for (ii = 0; ii < length; ii++) {
		const gchar *candidate;

		/* g_strv_length() stops on the first NULL pointer,
		 * so we don't have to worry about this being NULL. */
		candidate = part->priv->default_headers[ii];

		if (g_ascii_strcasecmp (header_name, candidate) == 0) {
			is_default = TRUE;
			break;
		}
	}

	g_mutex_unlock (&part->priv->property_lock);

	return is_default;
}

GtkTreeModel *
e_mail_part_headers_ref_print_model (EMailPartHeaders *part)
{
	GtkTreeModel *print_model = NULL;

	g_return_val_if_fail (E_IS_MAIL_PART_HEADERS (part), NULL);

	g_mutex_lock (&part->priv->property_lock);

	if (part->priv->print_model != NULL)
		print_model = g_object_ref (part->priv->print_model);

	g_mutex_unlock (&part->priv->property_lock);

	if (print_model == NULL) {
		/* The print model is built once on demand. */
		print_model = mail_part_headers_build_print_model (part);
	}

	return print_model;
}

