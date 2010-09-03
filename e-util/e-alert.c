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

#define d(x)

struct _e_alert {
	guint32 flags;
	const gchar *id;
	gint type;
	gint default_response;
	const gchar *title;
	const gchar *primary;
	const gchar *secondary;
	const gchar *help_uri;
	gboolean scroll;
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
	{ GTK_DIALOG_MODAL, "error", 3, GTK_RESPONSE_OK,
	  N_("Evolution Error"), "{0}", "{1}", NULL, FALSE,
	  &default_ok_button },
	{ GTK_DIALOG_MODAL, "error-primary", 3, GTK_RESPONSE_OK,
	  N_("Evolution Error"), "{0}", NULL, NULL, FALSE,
	  &default_ok_button },
	{ GTK_DIALOG_MODAL, "warning", 1, GTK_RESPONSE_OK,
	  N_("Evolution Warning"), "{0}", "{1}", NULL, FALSE,
	  &default_ok_button },
	{ GTK_DIALOG_MODAL, "warning-primary", 1, GTK_RESPONSE_OK,
	  N_("Evolution Warning"), "{0}", NULL, NULL, FALSE,
	  &default_ok_button },
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

static struct {
	const gchar *name;
	const gchar *icon;
} type_map[] = {
	{ "info", GTK_STOCK_DIALOG_INFO },
	{ "warning", GTK_STOCK_DIALOG_WARNING },
	{ "question", GTK_STOCK_DIALOG_QUESTION },
	{ "error", GTK_STOCK_DIALOG_ERROR },
};

static gint
map_type (const gchar *name)
{
	gint i;

	if (name) {
		for (i = 0; i < G_N_ELEMENTS (type_map); i++)
			if (!strcmp (name, type_map[i].name))
				return i;
	}

	return 3;
}

G_DEFINE_TYPE (
	EAlert,
	e_alert,
	G_TYPE_OBJECT)

#define ALERT_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), E_TYPE_ALERT, EAlertPrivate))

enum
{
	PROP_0,
	PROP_TAG,
	PROP_ARGS
};

struct _EAlertPrivate
{
	gchar *tag;
	GPtrArray *args;
	struct _e_alert *definition;
};

/*
  XML format:

 <error id="error-id" type="info|warning|question|error"?
      response="default_response"? modal="true"? >
  <title>Window Title</title>?
  <primary>Primary error text.</primary>?
  <secondary>Secondary error text.</secondary>?
  <help uri="help uri"/> ?
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
			e->scroll = FALSE;

			xmlFree (tmp);
			lastbutton = (struct _e_alert_button *)&e->buttons;

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"modal");
			if (tmp) {
				if (!strcmp(tmp, "true"))
					e->flags |= GTK_DIALOG_MODAL;
				xmlFree (tmp);
			}

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"type");
			e->type = map_type (tmp);
			if (tmp)
				xmlFree (tmp);

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"default");
			if (tmp) {
				e->default_response = map_response (tmp);
				xmlFree (tmp);
			}

			tmp = (gchar *)xmlGetProp(error, (const guchar *)"scroll");
			if (tmp) {
				if (!strcmp(tmp, "yes"))
					e->scroll = TRUE;
				xmlFree (tmp);
			}

			for (scan = error->children;scan;scan=scan->next) {
				if (!strcmp((gchar *)scan->name, "primary")) {
					if ((tmp = (gchar *)xmlNodeGetContent (scan))) {
						e->primary = g_strdup (dgettext (table->translation_domain, tmp));
						xmlFree (tmp);
					}
				} else if (!strcmp((gchar *)scan->name, "secondary")) {
					if ((tmp = (gchar *)xmlNodeGetContent (scan))) {
						e->secondary = g_strdup (dgettext (table->translation_domain, tmp));
						xmlFree (tmp);
					}
				} else if (!strcmp((gchar *)scan->name, "title")) {
					if ((tmp = (gchar *)xmlNodeGetContent (scan))) {
						e->title = g_strdup (dgettext (table->translation_domain, tmp));
						xmlFree (tmp);
					}
				} else if (!strcmp((gchar *)scan->name, "help")) {
					tmp = (gchar *)xmlGetProp(scan, (const guchar *)"uri");
					if (tmp) {
						e->help_uri = g_strdup (tmp);
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

static void
e_alert_get_property (GObject *object, guint property_id,
		      GValue *value, GParamSpec *pspec)
{
	EAlert *alert = (EAlert*) object;

	switch (property_id)
	{
		case PROP_TAG:
			g_value_set_string (value, alert->priv->tag);
			break;
		case PROP_ARGS:
			g_value_set_boxed (value, alert->priv->args);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
e_alert_set_property (GObject *object, guint property_id,
		      const GValue *value, GParamSpec *pspec)
{
	EAlert *alert = (EAlert*) object;

	switch (property_id)
	{
		case PROP_TAG:
			alert->priv->tag = g_value_dup_string (value);
			break;
		case PROP_ARGS:
			alert->priv->args = g_value_dup_boxed (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
e_alert_dispose (GObject *object)
{
	EAlert *alert = (EAlert*) object;

	if (alert->priv->tag) {
		g_free (alert->priv->tag);
		alert->priv->tag = NULL;
	}

	if (alert->priv->args) {
		/* arg strings will be freed automatically since we set a free func when
		 * creating the ptr array */
		g_boxed_free (G_TYPE_PTR_ARRAY, alert->priv->args);
		alert->priv->args = NULL;
	}

	G_OBJECT_CLASS (e_alert_parent_class)->dispose (object);
}

static void
e_alert_constructed (GObject *obj)
{
	EAlert *alert = E_ALERT (obj);

	struct _e_alert_table *table;
	gchar *domain, *id;

	g_return_if_fail (alert_table);
	g_return_if_fail (alert->priv->tag);

	domain = g_alloca (strlen (alert->priv->tag)+1);
	strcpy (domain, alert->priv->tag);
	id = strchr (domain, ':');
	if (id)
		*id++ = 0;

	table = g_hash_table_lookup (alert_table, domain);
	g_return_if_fail (table);

	alert->priv->definition = g_hash_table_lookup (table->alerts, id);

	g_warn_if_fail (alert->priv->definition);

}

static void
e_alert_class_init (EAlertClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (EAlertPrivate));

	object_class->get_property = e_alert_get_property;
	object_class->set_property = e_alert_set_property;
	object_class->dispose = e_alert_dispose;
	object_class->constructed = e_alert_constructed;

	g_object_class_install_property (object_class,
					 PROP_TAG,
					 g_param_spec_string ("tag",
							      "alert tag",
							      "A tag describing the alert",
							      "",
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_ARGS,
					 g_param_spec_boxed ("args",
							     "Arguments",
							     "Arguments for formatting the alert",
							     G_TYPE_PTR_ARRAY,
							     G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY |
							     G_PARAM_STATIC_STRINGS));

	e_alert_load_tables ();
}

static void
e_alert_init (EAlert *self)
{
	self->priv = ALERT_PRIVATE (self);
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
	va_list ap;

	va_start (ap, tag);
	e = e_alert_new_valist (tag, ap);
	va_end (ap);

	return e;
}

EAlert *
e_alert_new_valist (const gchar *tag, va_list ap)
{
	EAlert *alert;
	GPtrArray *args;
	gchar *tmp;

	args = g_ptr_array_new_with_free_func (g_free);

	tmp = va_arg (ap, gchar *);
	while (tmp) {
		g_ptr_array_add (args, g_strdup (tmp));
		tmp = va_arg (ap, gchar *);
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

/* unfortunately, gmarkup_escape doesn't expose its gstring based api :( */
static void
e_alert_append_text_escaped (GString *out, const gchar *text)
{
	gchar *markup;

	markup = g_markup_escape_text (text, -1);
	g_string_append (out, markup);
	g_free (markup);
}

static void
e_alert_format_string (GString *out,
                       const gchar *fmt,
                       GPtrArray *args,
                       gboolean escape_args)
{
	const gchar *end, *newstart;
	gint id;

	while (fmt
	       && (newstart = strchr (fmt, '{'))
	       && (end = strchr (newstart+1, '}'))) {
		g_string_append_len (out, fmt, newstart-fmt);
		id = atoi (newstart+1);
		if (id < args->len) {
			if (escape_args)
				e_alert_append_text_escaped (out, args->pdata[id]);
			else
				g_string_append (out, args->pdata[id]);
		} else
			g_warning("Error references argument %d not supplied by caller", id);
		fmt = end+1;
	}

	g_string_append (out, fmt);
}

guint32
e_alert_get_flags (EAlert *alert)
{
	g_return_val_if_fail (alert && alert->priv && alert->priv->definition, 0);
	return alert->priv->definition->flags;
}

const gchar *
e_alert_peek_stock_image (EAlert *alert)
{
	g_return_val_if_fail (alert && alert->priv && alert->priv->definition, NULL);
	return type_map[alert->priv->definition->type].icon;
}

gint
e_alert_get_default_response (EAlert *alert)
{
	g_return_val_if_fail (alert && alert->priv && alert->priv->definition, 0);
	return alert->priv->definition->default_response;
}

gchar *
e_alert_get_title (EAlert *alert,
                   gboolean escaped)
{
	GString *formatted;

	g_return_val_if_fail (alert && alert->priv && alert->priv->definition, NULL);

	formatted = g_string_new ("");

	if (alert->priv->definition->title != NULL)
		e_alert_format_string (
			formatted, alert->priv->definition->title,
			alert->priv->args, escaped);

	return g_string_free (formatted, FALSE);
}

gchar *
e_alert_get_primary_text (EAlert *alert,
                          gboolean escaped)
{
	GString *formatted;

	g_return_val_if_fail (alert && alert->priv, NULL);

	formatted = g_string_new ("");

	if (alert->priv->definition != NULL)
		if (alert->priv->definition->primary != NULL) {
			e_alert_format_string (
				formatted, alert->priv->definition->primary,
				alert->priv->args, escaped);
		} else {
			gchar *title;

			title = e_alert_get_title (alert, escaped);
			g_string_append (formatted, title);
			g_free (title);
		}
	else {
		g_string_append_printf (
			formatted,
			_("Internal error, unknown error '%s' requested"),
			alert->priv->tag);
	}

	return g_string_free (formatted, FALSE);
}

gchar *
e_alert_get_secondary_text (EAlert *alert,
                            gboolean escaped)
{
	GString *formatted;

	g_return_val_if_fail (alert && alert->priv && alert->priv->definition, NULL);

	formatted = g_string_new ("");

	if (alert->priv->definition->secondary != NULL)
		e_alert_format_string (
			formatted, alert->priv->definition->secondary,
			alert->priv->args, escaped);

	return g_string_free (formatted, FALSE);
}

const gchar *
e_alert_peek_help_uri (EAlert *alert)
{
	g_return_val_if_fail (alert && alert->priv && alert->priv->definition, NULL);
	return alert->priv->definition->help_uri;
}

gboolean
e_alert_get_scroll (EAlert *alert)
{
	g_return_val_if_fail (alert && alert->priv && alert->priv->definition, FALSE);
	return alert->priv->definition->scroll;
}

struct _e_alert_button *
e_alert_peek_buttons (EAlert *alert)
{
	g_return_val_if_fail (alert && alert->priv && alert->priv->definition, NULL);
	return alert->priv->definition->buttons;
}
