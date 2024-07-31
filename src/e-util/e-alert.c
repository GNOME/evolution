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
 *   Michael Zucchi <notzed@ximian.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <sys/types.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <libedataserver/libedataserver.h>

#include "e-alert-sink.h"
#include "e-ui-action.h"

#include "e-alert.h"

#define d(x)

typedef struct _EAlertButton EAlertButton;

struct _e_alert {
	const gchar *id;
	GtkMessageType message_type;
	gint default_response;
	const gchar *primary_text;
	const gchar *secondary_text;
	EAlertButton *buttons;
};

struct _e_alert_table {
	const gchar *domain;
	const gchar *translation_domain;
	GHashTable *alerts;
};

struct _EAlertButton {
	EAlertButton *next;
	const gchar *stock_id;
	const gchar *label;
	gint response_id;
	gboolean destructive;
};

static GHashTable *alert_table;

/* ********************************************************************** */

static EAlertButton default_ok_button = {
	NULL, NULL, NULL, GTK_RESPONSE_OK
};

static struct _e_alert default_alerts[] = {
	{ "error", GTK_MESSAGE_ERROR, GTK_RESPONSE_OK,
	  "{0}", "{1}", &default_ok_button },
	{ "warning", GTK_MESSAGE_WARNING, GTK_RESPONSE_OK,
	  "{0}", "{1}", &default_ok_button }
};

/* ********************************************************************** */

struct _EAlertPrivate {
	gchar *tag;
	GPtrArray *args;
	gchar *primary_text;
	gchar *secondary_text;
	struct _e_alert *definition;
	GtkMessageType message_type;
	gint default_response;
	guint timeout_id;

	/* It may occur to one that we could use a EUIActionGroup here,
	 * but we need to preserve the button order and EUIActionGroup
	 * uses a hash table, which does not preserve order. */
	GQueue actions;

	GQueue widgets;
};

enum {
	PROP_0,
	PROP_ARGS,
	PROP_TAG,
	PROP_MESSAGE_TYPE,
	PROP_PRIMARY_TEXT,
	PROP_SECONDARY_TEXT
};

enum {
	RESPONSE,
	LAST_SIGNAL
};

static gulong signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_PRIVATE (EAlert, e_alert, G_TYPE_OBJECT)

static gint
map_response (const gchar *name)
{
	GEnumClass *class;
	GEnumValue *value;

	class = g_type_class_ref (GTK_TYPE_RESPONSE_TYPE);
	value = g_enum_get_value_by_name (class, name);
	g_type_class_unref (class);

	return (value != NULL) ? value->value : 0;
}

static GtkMessageType
map_type (const gchar *nick)
{
	GEnumClass *class;
	GEnumValue *value;

	class = g_type_class_ref (GTK_TYPE_MESSAGE_TYPE);
	value = g_enum_get_value_by_nick (class, nick);
	g_type_class_unref (class);

	return (value != NULL) ? value->value : GTK_MESSAGE_ERROR;
}

/*
 * XML format:
 *
 * <error id="error-id" type="info|warning|question|error"?
 *      response="default_response"? >
 *  <primary> Primary error text.</primary>?
 *  <secondary> Secondary error text.</secondary>?
 *  <button stock="stock-button-id"? label="button label"?
 *      response="response_id"? /> *
 * </error>
 */

static void
e_alert_load (const gchar *path)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root, error, scan;
	struct _e_alert *e;
	EAlertButton *lastbutton;
	struct _e_alert_table *table;
	gchar *tmp;

	d (printf ("loading error file %s\n", path));

	doc = e_xml_parse_file (path);
	if (doc == NULL) {
		g_warning ("Error file '%s' not found", path);
		return;
	}

	root = xmlDocGetRootElement (doc);
	if (root == NULL
	    || strcmp ((gchar *) root->name, "error-list") != 0
	    || (tmp = (gchar *) xmlGetProp (root, (const guchar *)"domain")) == NULL) {
		g_warning ("Error file '%s' invalid format", path);
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

		tmp2 = (gchar *) xmlGetProp (
			root, (const guchar *) "translation-domain");
		if (tmp2) {
			table->translation_domain = g_strdup (tmp2);
			xmlFree (tmp2);

			tmp2 = (gchar *) xmlGetProp (
				root, (const guchar *) "translation-localedir");
			if (tmp2) {
				bindtextdomain (table->translation_domain, tmp2);
				xmlFree (tmp2);
			}
		}
	} else
		g_warning (
			"Error file '%s', domain '%s' "
			"already used, merging", path, tmp);
	xmlFree (tmp);

	for (error = root->children; error; error = error->next) {
		if (!strcmp ((gchar *) error->name, "error")) {
			tmp = (gchar *) xmlGetProp (error, (const guchar *)"id");
			if (tmp == NULL)
				continue;

			e = g_malloc0 (sizeof (*e));
			e->id = g_strdup (tmp);

			xmlFree (tmp);
			lastbutton = (EAlertButton *) &e->buttons;

			tmp = (gchar *) xmlGetProp (error, (const guchar *)"type");
			e->message_type = map_type (tmp);
			if (tmp)
				xmlFree (tmp);

			tmp = (gchar *) xmlGetProp (error, (const guchar *)"default");
			if (tmp) {
				e->default_response = map_response (tmp);
				xmlFree (tmp);
			}

			for (scan = error->children; scan; scan = scan->next) {
				if (!strcmp ((gchar *) scan->name, "primary")) {
					if ((tmp = (gchar *) xmlNodeGetContent (scan))) {
						e->primary_text = g_strdup (
							dgettext (table->
							translation_domain, tmp));
						xmlFree (tmp);
					}
				} else if (!strcmp ((gchar *) scan->name, "secondary")) {
					if ((tmp = (gchar *) xmlNodeGetContent (scan))) {
						e->secondary_text = g_strdup (
							dgettext (table->
							translation_domain, tmp));
						xmlFree (tmp);
					}
				} else if (!strcmp ((gchar *) scan->name, "button")) {
					EAlertButton *button;
					gchar *label = NULL;
					gchar *stock_id = NULL;

					button = g_new0 (EAlertButton, 1);
					tmp = (gchar *) xmlGetProp (scan, (const guchar *)"stock");
					if (tmp) {
						stock_id = g_strdup (tmp);
						button->stock_id = stock_id;
						xmlFree (tmp);
					}
					tmp = (gchar *) xmlGetProp (
						scan, (xmlChar *) "label");
					if (tmp) {
						label = g_strdup (
							dgettext (table->
							translation_domain,
							tmp));
						button->label = label;
						xmlFree (tmp);
					}
					tmp = (gchar *) xmlGetProp (
						scan, (xmlChar *) "response");
					if (tmp) {
						button->response_id =
							map_response (tmp);
						xmlFree (tmp);
					}
					tmp = (gchar *) xmlGetProp (scan, (xmlChar *) "destructive");
					if (g_strcmp0 (tmp, "1") == 0 || g_strcmp0 (tmp, "true") == 0)
						button->destructive = TRUE;
					if (tmp)
						xmlFree (tmp);

					if (stock_id == NULL && label == NULL) {
						g_warning (
							"Error file '%s': "
							"missing button "
							"details in error "
							"'%s'", path, e->id);
						g_free (stock_id);
						g_free (label);
						g_free (button);
					} else {
						lastbutton->next = button;
						lastbutton = button;
					}
				}
			}

			g_hash_table_insert (table->alerts, (gpointer) e->id, e);
		}
	}

	xmlFreeDoc (doc);
}

static void
e_alert_load_directory (const gchar *dirname)
{
	GDir *dir;
	const gchar *d;

	dir = g_dir_open (dirname, 0, NULL);
	if (dir == NULL) {
		return;
	}

	while ((d = g_dir_read_name (dir))) {
		gchar *path;

		if (d[0] == '.')
			continue;

		path = g_build_filename (dirname, d, NULL);
		e_alert_load (path);
		g_free (path);
	}

	g_dir_close (dir);
}

static void
e_alert_load_tables (void)
{
	GPtrArray *variants;
	gchar *base;
	struct _e_alert_table *table;
	guint ii;

	if (alert_table != NULL)
		return;

	alert_table = g_hash_table_new (g_str_hash, g_str_equal);

	/* setup system alert types */
	table = g_malloc0 (sizeof (*table));
	table->domain = "builtin";
	table->alerts = g_hash_table_new (g_str_hash, g_str_equal);
	for (ii = 0; ii < G_N_ELEMENTS (default_alerts); ii++)
		g_hash_table_insert (
			table->alerts, (gpointer)
			default_alerts[ii].id, &default_alerts[ii]);
	g_hash_table_insert (alert_table, (gpointer) table->domain, table);

	/* look for installed alert tables */
	base = g_build_filename (EVOLUTION_PRIVDATADIR, "errors", NULL);
	variants = e_util_get_directory_variants (base, EVOLUTION_PREFIX, TRUE);
	if (variants) {
		for (ii = 0; ii < variants->len; ii++) {
			const gchar *dirname = g_ptr_array_index (variants, ii);

			if (dirname && *dirname)
				e_alert_load_directory (dirname);
		}
		g_ptr_array_unref (variants);
	} else {
		e_alert_load_directory (base);
	}
	g_free (base);
}

static void
alert_action_activate (EAlert *alert,
		       GVariant *parameter,
                       EUIAction *action)
{
	GObject *object;
	gpointer data;

	object = G_OBJECT (action);
	data = g_object_get_data (object, "e-alert-response-id");
	e_alert_response (alert, GPOINTER_TO_INT (data));
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
	       && (end = strchr (newstart + 1, '}'))) {
		g_string_append_len (string, format, newstart - format);
		id = atoi (newstart + 1);
		if (id < args->len) {
			g_string_append (string, args->pdata[id]);
		} else
			g_warning (
				"Error references argument %d "
				"not supplied by caller", id);
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
}

static gboolean
alert_timeout_cb (gpointer user_data)
{
	EAlert *alert = user_data;

	if (g_source_is_destroyed (g_main_current_source ()))
		return FALSE;

	g_return_val_if_fail (E_IS_ALERT (alert), FALSE);

	if (g_source_get_id (g_main_current_source ()) == alert->priv->timeout_id)
		alert->priv->timeout_id = 0;

	e_alert_response (alert, alert->priv->default_response);

	return FALSE;
}

static void
alert_set_property (GObject *object,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
	EAlert *alert = (EAlert *) object;

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
	EAlert *alert = (EAlert *) object;

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
alert_dispose (GObject *object)
{
	EAlert *alert = E_ALERT (object);

	if (alert->priv->timeout_id > 0) {
		g_source_remove (alert->priv->timeout_id);
		alert->priv->timeout_id = 0;
	}

	while (!g_queue_is_empty (&alert->priv->actions)) {
		EUIAction *action;

		action = g_queue_pop_head (&alert->priv->actions);
		g_signal_handlers_disconnect_by_func (
			action, G_CALLBACK (alert_action_activate), object);
		g_object_unref (action);
	}

	while (!g_queue_is_empty (&alert->priv->widgets)) {
		GtkWidget *widget;

		widget = g_queue_pop_head (&alert->priv->widgets);
		g_object_unref (widget);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_alert_parent_class)->dispose (object);
}

static void
alert_finalize (GObject *object)
{
	EAlert *self = E_ALERT (object);

	g_free (self->priv->tag);
	g_free (self->priv->primary_text);
	g_free (self->priv->secondary_text);

	g_ptr_array_free (self->priv->args, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_alert_parent_class)->finalize (object);
}

static void
alert_constructed (GObject *object)
{
	EAlert *alert;
	EAlertButton *button;
	struct _e_alert *definition;
	gint ii = 0;

	alert = E_ALERT (object);
	definition = alert->priv->definition;
	g_return_if_fail (definition != NULL);

	e_alert_set_message_type (alert, definition->message_type);
	e_alert_set_default_response (alert, definition->default_response);

	/* Build actions out of the button definitions. */
	button = definition->buttons;
	while (button != NULL) {
		EUIAction *action;
		gchar *action_name;

		action_name = g_strdup_printf ("alert-response-%d", ii++);

		if (button->stock_id != NULL) {
			action = e_ui_action_new ("alert-map", action_name, NULL);
			e_ui_action_set_icon_name (action, button->stock_id);
			e_alert_add_action (alert, action, button->response_id, button->destructive);
			g_object_unref (action);

		} else if (button->label != NULL) {
			action = e_ui_action_new ("alert-map", action_name, NULL);
			e_ui_action_set_label (action, button->label);
			e_alert_add_action (alert, action, button->response_id, button->destructive);
			g_object_unref (action);
		}

		g_free (action_name);

		button = button->next;
	}

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_alert_parent_class)->constructed (object);
}

static void
e_alert_class_init (EAlertClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->set_property = alert_set_property;
	object_class->get_property = alert_get_property;
	object_class->dispose = alert_dispose;
	object_class->finalize = alert_finalize;
	object_class->constructed = alert_constructed;

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

	signals[RESPONSE] = g_signal_new (
		"response",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EAlertClass, response),
		NULL, NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);

	e_alert_load_tables ();
}

static void
e_alert_init (EAlert *alert)
{
	alert->priv = e_alert_get_instance_private (alert);

	g_queue_init (&alert->priv->actions);
	g_queue_init (&alert->priv->widgets);
}

/**
 * e_alert_new:
 * @tag: alert identifier
 * @...: %NULL-terminated argument list
 *
 * Creates a new EAlert.  The @tag argument is used to determine
 * which alert to use, it is in the format domain:alert-id.  The NULL
 * terminated list of arguments is used to fill out the alert definition.
 *
 * Returns: a new #EAlert
 **/
EAlert *
e_alert_new (const gchar *tag,
             ...)
{
	EAlert *e;
	va_list va;

	va_start (va, tag);
	e = e_alert_new_valist (tag, va);
	va_end (va);

	return e;
}

EAlert *
e_alert_new_valist (const gchar *tag,
                    va_list va)
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
e_alert_new_array (const gchar *tag,
                   GPtrArray *args)
{
	return g_object_new (E_TYPE_ALERT, "tag", tag, "args", args, NULL);
}

const gchar *
e_alert_get_tag (EAlert *alert)
{
	g_return_val_if_fail (E_IS_ALERT (alert), NULL);

	return alert->priv->tag;
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

	if (alert->priv->message_type == message_type)
		return;

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

	if (g_strcmp0 (alert->priv->primary_text, primary_text) == 0)
		return;

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

	if (g_strcmp0 (alert->priv->secondary_text, secondary_text) == 0)
		return;

	g_free (alert->priv->secondary_text);
	alert->priv->secondary_text = g_strdup (secondary_text);

	g_object_notify (G_OBJECT (alert), "secondary-text");
}

const gchar *
e_alert_get_icon_name (EAlert *alert)
{
	const gchar *icon_name;

	g_return_val_if_fail (E_IS_ALERT (alert), NULL);

	switch (e_alert_get_message_type (alert)) {
		case GTK_MESSAGE_INFO:
			icon_name = "dialog-information";
			break;
		case GTK_MESSAGE_WARNING:
			icon_name = "dialog-warning";
			break;
		case GTK_MESSAGE_QUESTION:
			icon_name = "dialog-question";
			break;
		case GTK_MESSAGE_ERROR:
			icon_name = "dialog-error";
			break;
		default:
			icon_name = "image-missing";
			g_warn_if_reached ();
			break;
	}

	return icon_name;
}

void
e_alert_add_action (EAlert *alert,
                    EUIAction *action,
                    gint response_id,
		    gboolean is_destructive)
{
	g_return_if_fail (E_IS_ALERT (alert));
	g_return_if_fail (E_IS_UI_ACTION (action));

	g_object_set_data (
		G_OBJECT (action), "e-alert-response-id",
		GINT_TO_POINTER (response_id));
	g_object_set_data (
		G_OBJECT (action), "e-alert-is-destructive",
		GINT_TO_POINTER (is_destructive ? 1 : 0));

	g_signal_connect_swapped (
		action, "activate",
		G_CALLBACK (alert_action_activate), alert);

	g_queue_push_tail (&alert->priv->actions, g_object_ref (action));
}

GList * /* EUIAction * */
e_alert_peek_actions (EAlert *alert)
{
	g_return_val_if_fail (E_IS_ALERT (alert), NULL);

	return g_queue_peek_head_link (&alert->priv->actions);
}

/* The widget is consumed by this function */
void
e_alert_add_widget (EAlert *alert,
		    GtkWidget *widget)
{
	g_return_if_fail (E_IS_ALERT (alert));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	g_queue_push_tail (&alert->priv->widgets, g_object_ref_sink (widget));
}

GList *
e_alert_peek_widgets (EAlert *alert)
{
	g_return_val_if_fail (E_IS_ALERT (alert), NULL);

	return g_queue_peek_head_link (&alert->priv->widgets);
}

GtkWidget *
e_alert_create_image (EAlert *alert,
                      GtkIconSize size)
{
	const gchar *icon_name;

	g_return_val_if_fail (E_IS_ALERT (alert), NULL);

	icon_name = e_alert_get_icon_name (alert);

	return gtk_image_new_from_icon_name (icon_name, size);
}

void
e_alert_response (EAlert *alert,
                  gint response_id)
{
	g_return_if_fail (E_IS_ALERT (alert));

	g_signal_emit (alert, signals[RESPONSE], 0, response_id);
}

/**
 * e_alert_start_timer:
 * @alert: an #EAlert
 * @seconds: seconds until timeout occurs
 *
 * Starts an internal timer for @alert.  When the timer expires, @alert
 * will emit the default response.  There is only one timer per #EAlert,
 * so calling this function repeatedly on the same #EAlert will restart
 * its timer each time.  If @seconds is zero, the timer is cancelled and
 * no response will be emitted.
 **/
void
e_alert_start_timer (EAlert *alert,
                     guint seconds)
{
	g_return_if_fail (E_IS_ALERT (alert));

	if (alert->priv->timeout_id > 0) {
		g_source_remove (alert->priv->timeout_id);
		alert->priv->timeout_id = 0;
	}

	if (seconds > 0) {
		alert->priv->timeout_id =
			e_named_timeout_add_seconds (
			seconds, alert_timeout_cb, alert);
	}
}

void
e_alert_submit (EAlertSink *alert_sink,
                const gchar *tag,
                ...)
{
	va_list va;

	va_start (va, tag);
	e_alert_submit_valist (alert_sink, tag, va);
	va_end (va);
}

void
e_alert_submit_valist (EAlertSink *alert_sink,
                       const gchar *tag,
                       va_list va)
{
	EAlert *alert;

	g_return_if_fail (E_IS_ALERT_SINK (alert_sink));
	g_return_if_fail (tag != NULL);

	alert = e_alert_new_valist (tag, va);
	e_alert_sink_submit_alert (alert_sink, alert);
	g_object_unref (alert);
}

static void
alert_action_button_clicked_cb (GtkButton *button,
				gpointer user_data)
{
	EUIAction *action = user_data;

	g_action_activate (G_ACTION (action), NULL);
}

GtkWidget *
e_alert_create_button_for_action (EUIAction *for_action)
{
	GtkWidget *widget;
	GtkStyleContext *style_context;

	g_return_val_if_fail (E_IS_UI_ACTION (for_action), NULL);

	/* action's icon-name is stock_id */
	if (e_ui_action_get_icon_name (for_action)) {
		widget = gtk_button_new_from_stock (e_ui_action_get_icon_name (for_action));
		if (e_ui_action_get_label (for_action)) {
			gtk_button_set_use_underline (GTK_BUTTON (widget), TRUE);
			gtk_button_set_label (GTK_BUTTON (widget), e_ui_action_get_label (for_action));
		}
	} else {
		widget = gtk_button_new_with_mnemonic (e_ui_action_get_label (for_action));
	}

	if (e_ui_action_get_tooltip (for_action))
		gtk_widget_set_tooltip_text (widget, e_ui_action_get_tooltip (for_action));

	gtk_widget_set_visible (widget, TRUE);

	g_signal_connect_object (widget, "clicked",
		G_CALLBACK (alert_action_button_clicked_cb), for_action, 0);

	style_context = gtk_widget_get_style_context (widget);

	if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (for_action), "e-alert-is-destructive")) != 0)
		gtk_style_context_add_class (style_context, "destructive-action");
	else
		gtk_style_context_remove_class (style_context, "destructive-action");

	return widget;
}
