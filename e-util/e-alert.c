/*
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
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

#include <config.h>

#include <string.h>
#include <sys/types.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libedataserver/e-xml-utils.h>

#include "e-util.h"
#include "e-util-private.h"
#include "e-alert.h"
#include "e-alert-sink.h"

#define d(x)

#define E_ALERT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ALERT, EAlertPrivate))

struct _e_alert {
	const gchar *id;
	GtkMessageType message_type;
	gint default_response;
	const gchar *primary_text;
	const gchar *secondary_text;
	struct _e_alert_button *buttons;
};

struct _e_alert_table {
	const gchar *domain;
	const gchar *translation_domain;
	GHashTable *alerts;
};

static GHashTable *alert_table;

/* ********************************************************************** */

static struct _e_alert_button default_ok_button = {
	NULL, "gtk-ok", NULL, GTK_RESPONSE_OK
};

static struct _e_alert default_alerts[] = {
	{ "error", GTK_MESSAGE_ERROR, GTK_RESPONSE_OK,
	  "{0}", "{1}", &default_ok_button },
	{ "warning", GTK_MESSAGE_WARNING, GTK_RESPONSE_OK,
	  "{0}", "{1}", &default_ok_button }
};

/* ********************************************************************** */

static struct {
	const gchar *name;
	gint id;
} response_map[] = {
	{ "GTK_RESPONSE_REJECT", GTK_RESPONSE_REJECT },
	{ "GTK_RESPONSE_ACCEPT", GTK_RESPONSE_ACCEPT },
	{ "GTK_RESPONSE_OK", GTK_RESPONSE_OK },
	{ "GTK_RESPONSE_CANCEL", GTK_RESPONSE_CANCEL },
	{ "GTK_RESPONSE_CLOSE", GTK_RESPONSE_CLOSE },
	{ "GTK_RESPONSE_YES", GTK_RESPONSE_YES },
	{ "GTK_RESPONSE_NO", GTK_RESPONSE_NO },
	{ "GTK_RESPONSE_APPLY", GTK_RESPONSE_APPLY },
	{ "GTK_RESPONSE_HELP", GTK_RESPONSE_HELP },
};

static gint
map_response (const gchar *name)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (response_map); i++)
		if (!strcmp (name, response_map[i].name))
			return response_map[i].id;

	return 0;
}

static GtkMessageType
map_type (const gchar *nick)
{
	GEnumClass *class;
	GEnumValue *value;

	class = g_type_class_peek (GTK_TYPE_MESSAGE_TYPE);
	value = g_enum_get_value_by_nick (class, nick);

	return (value != NULL) ? value->value : GTK_MESSAGE_ERROR;
}

G_DEFINE_TYPE (
	EAlert,
	e_alert,
	G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_ARGS,
	PROP_TAG,
	PROP_MESSAGE_TYPE,
	PROP_PRIMARY_TEXT,
	PROP_SECONDARY_TEXT
};

struct _EAlertPrivate {
	gchar *tag;
	GPtrArray *args;
	gchar *primary_text;
	gchar *secondary_text;
	struct _e_alert *definition;
	GtkMessageType message_type;
	gint default_response;
};

/*
  XML format:

 <error id="error-id" type="info|warning|question|error"?
      response="default_response"? >
  <primary>Primary error text.</primary>?
  <secondary>Secondary error text.</secondary>?
  <button stock="stock-button-id"? label="button label"?
      response="response_id"? /> *
 </error>

 The tool e-error-tool is used to extract the translatable strings for
 translation.

*/
static void
e_alert_load (const gchar *path)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root, error, scan;
	struct _e_alert *e;
	struct _e_alert_button *lastbutton;
	struct _e_alert_table *table;
	gchar *tmp;

	d(printf("loading error file %s\n", path));

	doc = e_xml_parse_file (path);
	if (doc == NULL) {
		g_warning("Error file '%s' not found", path);
		return;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL
	    || strcmp((gchar *)root->name, "error-list") != 0
	    || (tmp = (gchar *)xmlGetProp(root, (const guchar *)"domain")) == NULL) {
		g_warning("Error file '%s' invalid format", path);
		xmlFreeDoc (doc);
		return;
	}

	table = g_hash_table_lookup (alert_table, tmp);
	if (table == NULL) {
		gchar *tmp2;

		table = g_malloc0 (sizeof (*table));
		table->domain = g_strdup (tmp);
		table->alerts = g_hash_table_new (g_str_hash, g_str_equal);
		g_hash_table_insert (alert_table, (gpointer) table->domain, table);

		tmp2 = (gchar *)xmlGetProp(root, (const guchar *)"translation-domain");
		if (tmp2) {
			table->translation_domain = g_strdup (tmp2);
			xmlFree (tmp2);

			tmp2 = (gchar *)xmlGetProp(root, (const guchar *)"translation-localedir");
			if (tmp2) {
				bindtextdomain (table->translation_domain, tmp2);
				xmlFree (tmp2);
			}
		}
	} else
		g_warning("Error file '%s', domain '%s' already used, merging", path, tmp);
	xmlFree (tmp);

	for (error = root->children;error;error = error->next) {
		if (!strcmp((gchar *)error->name, "error")) {
			tmp = (gchar *)xmlGetProp(error, (const guchar *)"id");
			if (tmp == NULL)
				continue;

			e = g_malloc0 (sizeof (*e));
			e->id = g_strdup (tmp);

			xmlFree (tmp);
			lastbutton = (struct _e_alert_button *)&e->buttons;

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"type");
			e->message_type = map_type (tmp);
			if (tmp)
				xmlFree (tmp);

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"default");
			if (tmp) {
				e->default_response = map_response (tmp);
				xmlFree (tmp);
			}

			for (scan = error->children;scan;scan=scan->next) {
				if (!strcmp((gchar *)scan->name, "primary")) {
					if ((tmp = (gchar *)xmlNodeGetContent (scan))) {
						e->primary_text = g_strdup (dgettext (table->translation_domain, tmp));
						xmlFree (tmp);
					}
				} else if (!strcmp((gchar *)scan->name, "secondary")) {
					if ((tmp = (gchar *)xmlNodeGetContent (scan))) {
						e->secondary_text = g_strdup (dgettext (table->translation_domain, tmp));
						xmlFree (tmp);
					}
				} else if (!strcmp((gchar *)scan->name, "button")) {
					struct _e_alert_button *b;
					gchar *label = NULL;
					gchar *stock = NULL;

					b = g_malloc0 (sizeof (*b));
					tmp = (gchar *)xmlGetProp(scan, (const guchar *)"stock");
					if (tmp) {
						stock = g_strdup (tmp);
						b->stock = stock;
						xmlFree (tmp);
					}
					tmp = (gchar *)xmlGetProp(scan, (const guchar *)"label");
					if (tmp) {
						label = g_strdup (dgettext (table->translation_domain, tmp));
						b->label = label;
						xmlFree (tmp);
					}
					tmp = (gchar *)xmlGetProp(scan, (const guchar *)"response");
					if (tmp) {
						b->response = map_response (tmp);
						xmlFree (tmp);
					}

					if (stock == NULL && label == NULL) {
						g_warning("Error file '%s': missing button details in error '%s'", path, e->id);
						g_free (stock);
						g_free (label);
						g_free (b);
					} else {
						lastbutton->next = b;
						lastbutton = b;
					}
				}
			}

			g_hash_table_insert (table->alerts, (gpointer) e->id, e);
		}
	}

	xmlFreeDoc (doc);
}

static void
e_alert_load_tables (void)
{
	GDir *dir;
	const gchar *d;
	gchar *base;
	struct _e_alert_table *table;
	gint i;

	if (alert_table != NULL)
		return;

	alert_table = g_hash_table_new (g_str_hash, g_str_equal);

	/* setup system alert types */
	table = g_malloc0 (sizeof (*table));
	table->domain = "builtin";
	table->alerts = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < G_N_ELEMENTS (default_alerts); i++)
		g_hash_table_insert (
			table->alerts, (gpointer)
			default_alerts[i].id, &default_alerts[i]);
	g_hash_table_insert (alert_table, (gpointer) table->domain, table);

	/* look for installed alert tables */
	base = g_build_filename (EVOLUTION_PRIVDATADIR, "errors", NULL);
	dir = g_dir_open (base, 0, NULL);
	if (dir == NULL) {
		g_free (base);
		return;
	}

	while ((d = g_dir_read_name (dir))) {
		gchar *path;

		if (d[0] == '.')
			continue;

		path = g_build_filename (base, d, NULL);
		e_alert_load (path);
		g_free (path);
	}

	g_dir_close (dir);
	g_free (base);
}

static gchar *
alert_format_string (const gchar *format,
                     GPtrArray *args)
{
	GString *string;
	const gchar *end, *newstart;
	gint id;

	string = g_string_sized_new (strlen (format));

	while (format
	       && (newstart = strchr (format, '{'))
	       && (end = strchr (newstart+1, '}'))) {
		g_string_append_len (string, format, newstart - format);
		id = atoi (newstart + 1);
		if (id < args->len) {
			g_string_append (string, args->pdata[id]);
		} else
			g_warning("Error references argument %d not supplied by caller", id);
		format = end + 1;
	}

	g_string_append (string, format);

	return g_string_free (string, FALSE);
}

static void
alert_set_tag (EAlert *alert,
               const gchar *tag)
{
	struct _e_alert *definition;
	struct _e_alert_table *table;
	gchar *domain, *id;

	alert->priv->tag = g_strdup (tag);

	g_return_if_fail (alert_table);

	domain = g_alloca (strlen (tag) + 1);
	strcpy (domain, tag);
	id = strchr (domain, ':');
	if (id)
		*id++ = 0;
	else {
		g_warning ("Alert tag '%s' is missing a domain", tag);
		return;
	}

	table = g_hash_table_lookup (alert_table, domain);
	g_return_if_fail (table);

	definition = g_hash_table_lookup (table->alerts, id);
	g_warn_if_fail (definition);

	alert->priv->definition = definition;
	e_alert_set_message_type (alert, definition->message_type);
	e_alert_set_default_response (alert, definition->default_response);
}

static void
alert_set_property (GObject *object,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
	EAlert *alert = (EAlert*) object;

	switch (property_id) {
		case PROP_TAG:
			alert_set_tag (
				E_ALERT (object),
				g_value_get_string (value));
			return;

		case PROP_ARGS:
			alert->priv->args = g_value_dup_boxed (value);
			return;

		case PROP_MESSAGE_TYPE:
			e_alert_set_message_type (
				E_ALERT (object),
				g_value_get_enum (value));
			return;

		case PROP_PRIMARY_TEXT:
			e_alert_set_primary_text (
				E_ALERT (object),
				g_value_get_string (value));
			return;

		case PROP_SECONDARY_TEXT:
			e_alert_set_secondary_text (
				E_ALERT (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
alert_get_property (GObject *object,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
	EAlert *alert = (EAlert*) object;

	switch (property_id) {
		case PROP_TAG:
			g_value_set_string (value, alert->priv->tag);
			return;

		case PROP_ARGS:
			g_value_set_boxed (value, alert->priv->args);
			return;

		case PROP_MESSAGE_TYPE:
			g_value_set_enum (
				value, e_alert_get_message_type (
				E_ALERT (object)));
			return;

		case PROP_PRIMARY_TEXT:
			g_value_set_string (
				value, e_alert_get_primary_text (
				E_ALERT (object)));
			return;

		case PROP_SECONDARY_TEXT:
			g_value_set_string (
				value, e_alert_get_secondary_text (
				E_ALERT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
alert_finalize (GObject *object)
{
	EAlertPrivate *priv;

	priv = E_ALERT_GET_PRIVATE (object);

	g_free (priv->tag);
	g_free (priv->primary_text);
	g_free (priv->secondary_text);

	g_ptr_array_free (priv->args, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_alert_parent_class)->finalize (object);
}


static void
e_alert_class_init (EAlertClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	g_type_class_add_private (class, sizeof (EAlertPrivate));

	object_class->set_property = alert_set_property;
	object_class->get_property = alert_get_property;
	object_class->finalize = alert_finalize;

	g_object_class_install_property (
		object_class,
		PROP_ARGS,
		g_param_spec_boxed (
			"args",
			"Arguments",
			"Arguments for formatting the alert",
			G_TYPE_PTR_ARRAY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_TAG,
		g_param_spec_string (
			"tag",
			"alert tag",
			"A tag describing the alert",
			"",
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_MESSAGE_TYPE,
		g_param_spec_enum (
			"message-type",
			NULL,
			NULL,
			GTK_TYPE_MESSAGE_TYPE,
			GTK_MESSAGE_ERROR,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_PRIMARY_TEXT,
		g_param_spec_string (
			"primary-text",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SECONDARY_TEXT,
		g_param_spec_string (
			"secondary-text",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	e_alert_load_tables ();
}

static void
e_alert_init (EAlert *self)
{
	self->priv = E_ALERT_GET_PRIVATE (self);
}

/**
 * e_alert_new:
 * @tag: alert identifier
 * @arg0: The first argument for the alert formatter.  The list must
 * be NULL terminated.
 *
 * Creates a new EAlert.  The @tag argument is used to determine
 * which alert to use, it is in the format domain:alert-id.  The NULL
 * terminated list of arguments, starting with @arg0 is used to fill
 * out the alert definition.
 *
 * Returns: a new #EAlert
 **/
EAlert *
e_alert_new (const gchar *tag, ...)
{
	EAlert *e;
	va_list va;

	va_start (va, tag);
	e = e_alert_new_valist (tag, va);
	va_end (va);

	return e;
}

EAlert *
e_alert_new_valist (const gchar *tag, va_list va)
{
	EAlert *alert;
	GPtrArray *args;
	gchar *tmp;

	args = g_ptr_array_new_with_free_func (g_free);

	tmp = va_arg (va, gchar *);
	while (tmp) {
		g_ptr_array_add (args, g_strdup (tmp));
		tmp = va_arg (va, gchar *);
	}

	alert = e_alert_new_array (tag, args);

	g_ptr_array_unref (args);

	return alert;
}

EAlert *
e_alert_new_array (const gchar *tag, GPtrArray *args)
{
	return g_object_new (E_TYPE_ALERT, "tag", tag, "args", args, NULL);
}

gint
e_alert_get_default_response (EAlert *alert)
{
	g_return_val_if_fail (E_IS_ALERT (alert), 0);

	return alert->priv->default_response;
}

void
e_alert_set_default_response (EAlert *alert,
                              gint response_id)
{
	g_return_if_fail (E_IS_ALERT (alert));

	alert->priv->default_response = response_id;
}

GtkMessageType
e_alert_get_message_type (EAlert *alert)
{
	g_return_val_if_fail (E_IS_ALERT (alert), GTK_MESSAGE_OTHER);

	return alert->priv->message_type;
}

void
e_alert_set_message_type (EAlert *alert,
                          GtkMessageType message_type)
{
	g_return_if_fail (E_IS_ALERT (alert));

	alert->priv->message_type = message_type;

	g_object_notify (G_OBJECT (alert), "message-type");
}

const gchar *
e_alert_get_primary_text (EAlert *alert)
{
	g_return_val_if_fail (E_IS_ALERT (alert), NULL);

	if (alert->priv->primary_text != NULL)
		goto exit;

	if (alert->priv->definition == NULL)
		goto exit;

	if (alert->priv->definition->primary_text == NULL)
		goto exit;

	if (alert->priv->args == NULL)
		goto exit;

	alert->priv->primary_text = alert_format_string (
		alert->priv->definition->primary_text,
		alert->priv->args);

exit:
	return alert->priv->primary_text;
}

void
e_alert_set_primary_text (EAlert *alert,
                            const gchar *primary_text)
{
	g_return_if_fail (E_IS_ALERT (alert));

	g_free (alert->priv->primary_text);
	alert->priv->primary_text = g_strdup (primary_text);

	g_object_notify (G_OBJECT (alert), "primary-text");
}

const gchar *
e_alert_get_secondary_text (EAlert *alert)
{
	g_return_val_if_fail (E_IS_ALERT (alert), NULL);

	if (alert->priv->secondary_text != NULL)
		goto exit;

	if (alert->priv->definition == NULL)
		goto exit;

	if (alert->priv->definition->secondary_text == NULL)
		goto exit;

	if (alert->priv->args == NULL)
		goto exit;

	alert->priv->secondary_text = alert_format_string (
		alert->priv->definition->secondary_text,
		alert->priv->args);

exit:
	return alert->priv->secondary_text;
}

void
e_alert_set_secondary_text (EAlert *alert,
                            const gchar *secondary_text)
{
	g_return_if_fail (E_IS_ALERT (alert));

	g_free (alert->priv->secondary_text);
	alert->priv->secondary_text = g_strdup (secondary_text);

	g_object_notify (G_OBJECT (alert), "secondary-text");
}

struct _e_alert_button *
e_alert_peek_buttons (EAlert *alert)
{
	g_return_val_if_fail (alert && alert->priv && alert->priv->definition, NULL);
	return alert->priv->definition->buttons;
}

GtkWidget *
e_alert_create_image (EAlert *alert,
                      GtkIconSize size)
{
	const gchar *stock_id;

	g_return_val_if_fail (E_IS_ALERT (alert), NULL);

	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
			stock_id = GTK_STOCK_DIALOG_INFO;
			break;
		case GTK_MESSAGE_WARNING:
			stock_id = GTK_STOCK_DIALOG_WARNING;
			break;
		case GTK_MESSAGE_QUESTION:
			stock_id = GTK_STOCK_DIALOG_QUESTION;
			break;
		case GTK_MESSAGE_ERROR:
			stock_id = GTK_STOCK_DIALOG_ERROR;
			break;
		default:
			stock_id = GTK_STOCK_MISSING_IMAGE;
			g_warn_if_reached ();
			break;
	}

	return gtk_image_new_from_stock (stock_id, size);
}

void
e_alert_submit (GtkWidget *widget,
                const gchar *tag,
                ...)
{
	va_list va;

	va_start (va, tag);
	e_alert_submit_valist (widget, tag, va);
	va_end (va);
}

void
e_alert_submit_valist (GtkWidget *widget,
                       const gchar *tag,
                       va_list va)
{
	EAlert *alert;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (tag != NULL);

	alert = e_alert_new_valist (tag, va);
	e_alert_sink_submit_alert (widget, alert);
	g_object_unref (alert);
}
