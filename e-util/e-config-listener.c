/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Configuration component listener
 *
 * Author:
 *   Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2002, Ximian, Inc.
 */

#include <string.h>
#include <gconf/gconf-client.h>
#include "e-config-listener.h"

#define PARENT_TYPE G_TYPE_OBJECT

typedef struct {
	EConfigListener *cl;
	guint lid;
	char *key;
	GConfValueType type;
	union {
		gboolean v_bool;
		float v_float;
		long v_long;
		char *v_str;
	} value;
	gboolean used_default;
} KeyData;

struct _EConfigListenerPrivate {
	GConfClient *db;
	GHashTable *keys;
};

static void e_config_listener_class_init (EConfigListenerClass *klass);
static void e_config_listener_init       (EConfigListener *cl, EConfigListenerClass *klass);
static void e_config_listener_finalize   (GObject *object);

static GObjectClass *parent_class = NULL;

enum {
	KEY_CHANGED,
	LAST_SIGNAL
};

static guint config_listener_signals[LAST_SIGNAL];

static void
e_config_listener_class_init (EConfigListenerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = e_config_listener_finalize;
	klass->key_changed = NULL;

	config_listener_signals[KEY_CHANGED] =
		g_signal_new ("key_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EConfigListenerClass, key_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	/* make sure GConf is initialized */
	if (!gconf_is_initialized ())
		gconf_init (0, NULL, NULL);
}

static void
e_config_listener_init (EConfigListener *cl, EConfigListenerClass *klass)
{
	/* allocate internal structure */
	cl->priv = g_new0 (EConfigListenerPrivate, 1);

	cl->priv->keys = g_hash_table_new (g_str_hash, g_str_equal);
	cl->priv->db = gconf_client_get_default ();
}

static void
free_key_hash (gpointer key, gpointer value, gpointer user_data)
{
	KeyData *kd = (KeyData *) value;

	g_return_if_fail (kd != NULL);

	gconf_client_notify_remove (kd->cl->priv->db, kd->lid);

	g_free (kd->key);
	switch (kd->type) {
	case GCONF_VALUE_STRING :
		g_free (kd->value.v_str);
		break;
	default :
		break;
	}

	g_free (kd);
}

static void
e_config_listener_finalize (GObject *object)
{
	EConfigListener *cl = (EConfigListener *) object;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));

	g_hash_table_foreach (cl->priv->keys, (GHFunc) free_key_hash, NULL);
	g_hash_table_destroy (cl->priv->keys);
	cl->priv->keys = NULL;

	if (cl->priv->db != NULL) {
		g_object_unref (G_OBJECT (cl->priv->db));
		cl->priv->db = NULL;
	}

	g_free (cl->priv);
	cl->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

GType
e_config_listener_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
                        sizeof (EConfigListenerClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) e_config_listener_class_init,
                        NULL,
                        NULL,
                        sizeof (EConfigListener),
                        0,
                        (GInstanceInitFunc) e_config_listener_init
                };
                type = g_type_register_static (PARENT_TYPE, "EConfigListener", &info, 0);
	}

	return type;
}

/**
 * e_config_listener_new
 *
 * Create a new configuration listener, which is an object which
 * allows to listen for changes in the configuration database. It keeps
 * an updated copy of all requested configuration entries, so that
 * access is much quicker and instantaneous.
 *
 * Returns: the newly created listener.
 */
EConfigListener *
e_config_listener_new (void)
{
	EConfigListener *cl;

	cl = g_object_new (E_CONFIG_LISTENER_TYPE, NULL);
	return cl;
}

static void
property_change_cb (GConfEngine *engine,
		    guint cnxn_id,
		    GConfEntry *entry,
		    gpointer user_data)
{
	KeyData *kd = (KeyData *) user_data;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (kd != NULL);

	/* free previous value */
	if (kd->type == GCONF_VALUE_STRING)
		g_free (kd->value.v_str);

	/* set new value */
	if (entry->value->type == GCONF_VALUE_BOOL) {
		kd->type = GCONF_VALUE_BOOL;
		kd->value.v_bool = gconf_value_get_bool (entry->value);
	} else if (entry->value->type == GCONF_VALUE_FLOAT) {
		kd->type = GCONF_VALUE_FLOAT;
		kd->value.v_float = gconf_value_get_float (entry->value);
	} else if (entry->value->type == GCONF_VALUE_INT) {
		kd->type = GCONF_VALUE_INT;
		kd->value.v_long = gconf_value_get_int (entry->value);
	} else if (entry->value->type == GCONF_VALUE_STRING) {
		kd->type = GCONF_VALUE_STRING;
		kd->value.v_str = g_strdup (gconf_value_get_string (entry->value));
	} else
		return;

	g_signal_emit (G_OBJECT (kd->cl), config_listener_signals[KEY_CHANGED], 0, kd->key);
}

static KeyData *
add_key (EConfigListener *cl, const char *key, GConfValueType type,
	 gpointer value, gboolean used_default)
{
	KeyData *kd;

	/* add the key to our hash table */
	kd = g_new0 (KeyData, 1);
	kd->cl = cl;
	kd->key = g_strdup (key);
	kd->type = type;
	switch (type) {
	case GCONF_VALUE_BOOL :
		memcpy (&kd->value.v_bool, value, sizeof (gboolean));
		break;
	case GCONF_VALUE_FLOAT :
		memcpy (&kd->value.v_float, value, sizeof (float));
		break;
	case GCONF_VALUE_INT :
		memcpy (&kd->value.v_long, value, sizeof (long));
		break;
	case GCONF_VALUE_STRING :
		kd->value.v_str = g_strdup ((const char *) value);
		break;
	default :
		break;
	}

	kd->used_default = used_default;

	/* add the listener for changes */
	gconf_client_add_dir (cl->priv->db, key, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
	kd->lid = gconf_client_notify_add (cl->priv->db, key,
					   (GConfClientNotifyFunc) property_change_cb,
					   kd, NULL, NULL);

	g_hash_table_insert (cl->priv->keys, kd->key, kd);

	return kd;
}

gboolean
e_config_listener_get_boolean (EConfigListener *cl, const char *key)
{
	return e_config_listener_get_boolean_with_default (cl, key, FALSE, NULL);
}

gboolean
e_config_listener_get_boolean_with_default (EConfigListener *cl,
					    const char *key,
					    gboolean def,
					    gboolean *used_default)
{
	GConfValue *conf_value;
	gboolean value;
	KeyData *kd;
	gpointer orig_key, orig_value;

	g_return_val_if_fail (E_IS_CONFIG_LISTENER (cl), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	/* search for the key in our hash table */
	if (!g_hash_table_lookup_extended (cl->priv->keys, key, &orig_key, &orig_value)) {
		/* not found, so retrieve it from the configuration database */
		conf_value = gconf_client_get (cl->priv->db, key, NULL);
		if (conf_value) {
			value = gconf_value_get_bool (conf_value);
			kd = add_key (cl, key, GCONF_VALUE_BOOL, &value, FALSE);
			gconf_value_free (conf_value);

			if (used_default != NULL)
				*used_default = FALSE;
		} else {
			value = def;
			kd = add_key (cl, key, GCONF_VALUE_BOOL, &def, TRUE);

			if (used_default != NULL)
				*used_default = TRUE;
		}
	} else {
		kd = (KeyData *) orig_value;
		g_assert (kd != NULL);

		if (kd->type == GCONF_VALUE_BOOL) {
			value = kd->value.v_bool;
			if (used_default != NULL)
				*used_default = kd->used_default;
		} else
			return FALSE;
	}

	return value;
}

float
e_config_listener_get_float (EConfigListener *cl, const char *key)
{
	return e_config_listener_get_float_with_default (cl, key, 0.0, NULL);
}

float
e_config_listener_get_float_with_default (EConfigListener *cl,
					  const char *key,
					  float def,
					  gboolean *used_default)
{
	GConfValue *conf_value;
	float value;
	KeyData *kd;
	gpointer orig_key, orig_value;

	g_return_val_if_fail (E_IS_CONFIG_LISTENER (cl), -1);
	g_return_val_if_fail (key != NULL, -1);

	/* search for the key in our hash table */
	if (!g_hash_table_lookup_extended (cl->priv->keys, key, &orig_key, &orig_value)) {
		/* not found, so retrieve it from the configuration database */
		conf_value = gconf_client_get (cl->priv->db, key, NULL);
		if (conf_value) {
			value = gconf_value_get_float (conf_value);
			kd = add_key (cl, key, GCONF_VALUE_FLOAT, &value, FALSE);
			gconf_value_free (conf_value);

			if (used_default != NULL)
				*used_default = FALSE;
		} else {
			value = def;
			kd = add_key (cl, key, GCONF_VALUE_FLOAT, &def, TRUE);

			if (used_default != NULL)
				*used_default = TRUE;
		}
	} else {
		kd = (KeyData *) orig_value;
		g_assert (kd != NULL);

		if (kd->type == GCONF_VALUE_FLOAT) {
			value = kd->value.v_float;
			if (used_default != NULL)
				*used_default = kd->used_default;
		} else
			return -1;
	}

	return value;
}

long
e_config_listener_get_long (EConfigListener *cl, const char *key)
{
	return e_config_listener_get_long_with_default (cl, key, 0, NULL);
}

long
e_config_listener_get_long_with_default (EConfigListener *cl,
					 const char *key,
					 long def,
					 gboolean *used_default)
{
	GConfValue *conf_value;
	long value;
	KeyData *kd;
	gpointer orig_key, orig_value;

	g_return_val_if_fail (E_IS_CONFIG_LISTENER (cl), -1);
	g_return_val_if_fail (key != NULL, -1);

	/* search for the key in our hash table */
	if (!g_hash_table_lookup_extended (cl->priv->keys, key, &orig_key, &orig_value)) {
		/* not found, so retrieve it from the configuration database */
		conf_value = gconf_client_get (cl->priv->db, key, NULL);
		if (conf_value) {
			value = gconf_value_get_int (conf_value);
			kd = add_key (cl, key, GCONF_VALUE_INT, &value, FALSE);
			gconf_value_free (conf_value);

			if (used_default != NULL)
				*used_default = FALSE;
		} else {
			value = def;
			kd = add_key (cl, key, GCONF_VALUE_INT, &def, TRUE);

			if (used_default != NULL)
				*used_default = TRUE;
		}
	} else {
		kd = (KeyData *) orig_value;
		g_assert (kd != NULL);

		if (kd->type == GCONF_VALUE_INT) {
			value = kd->value.v_long;
			if (used_default != NULL)
				*used_default = kd->used_default;
		} else
			return -1;
	}

	return value;
}

char *
e_config_listener_get_string (EConfigListener *cl, const char *key)
{
	return e_config_listener_get_string_with_default (cl, key, NULL, NULL);
}

char *
e_config_listener_get_string_with_default (EConfigListener *cl,
					   const char *key,
					   const char *def,
					   gboolean *used_default)
{
	GConfValue *conf_value;
	char *str;
	KeyData *kd;
	gpointer orig_key, orig_value;

	g_return_val_if_fail (E_IS_CONFIG_LISTENER (cl), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	/* search for the key in our hash table */
	if (!g_hash_table_lookup_extended (cl->priv->keys, key, &orig_key, &orig_value)) {
		/* not found, so retrieve it from the configuration database */
		conf_value = gconf_client_get (cl->priv->db, key, NULL);
		if (conf_value) {
			str = g_strdup (gconf_value_get_string (conf_value));
			kd = add_key (cl, key, GCONF_VALUE_STRING, (gpointer) str, FALSE);
			gconf_value_free (conf_value);

			if (used_default != NULL)
				*used_default = FALSE;
		} else {
			str = g_strdup (def);
			kd = add_key (cl, key, GCONF_VALUE_STRING, (gpointer) str, TRUE);

			if (used_default != NULL)
				*used_default = TRUE;
		}
	} else {
		kd = (KeyData *) orig_value;
		g_assert (kd != NULL);

		if (kd->type == GCONF_VALUE_STRING) {
			str = g_strdup (kd->value.v_str);
			if (used_default != NULL)
				*used_default = kd->used_default;
		} else
			return NULL;
	}

	return str;
}

void
e_config_listener_set_boolean (EConfigListener *cl, const char *key, gboolean value)
{
	KeyData *kd;
	GError *err = NULL;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (key != NULL);

	/* check that the value is not the same */
	if (value == e_config_listener_get_boolean_with_default (cl, key, 0, NULL))
		return;

	gconf_client_set_bool (cl->priv->db, key, value, &err);
	if (err) {
		g_warning ("e_config_listener_set_bool: %s", err->message);
		g_error_free (err);
	} else {
		/* update the internal copy */
		kd = g_hash_table_lookup (cl->priv->keys, key);
		if (kd)
			kd->value.v_bool = value;
	}
}

void
e_config_listener_set_float (EConfigListener *cl, const char *key, float value)
{
	KeyData *kd;
	GError *err = NULL;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (key != NULL);

	/* check that the value is not the same */
	if (value == e_config_listener_get_float_with_default (cl, key, 0, NULL))
		return;

	gconf_client_set_float (cl->priv->db, key, value, &err);
	if (err) {
		g_warning ("e_config_listener_set_float: %s", err->message);
		g_error_free (err);
	} else {
		/* update the internal copy */
		kd = g_hash_table_lookup (cl->priv->keys, key);
		if (kd)
			kd->value.v_float = value;
	}
}

void
e_config_listener_set_long (EConfigListener *cl, const char *key, long value)
{
	KeyData *kd;
	GError *err = NULL;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (key != NULL);

	/* check that the value is not the same */
	if (value == e_config_listener_get_long_with_default (cl, key, 0, NULL))
		return;

	gconf_client_set_int (cl->priv->db, key, value, &err);
	if (err) {
		g_warning ("e_config_listener_set_long: %s", err->message);
		g_error_free (err);
	} else {
		/* update the internal copy */
		kd = g_hash_table_lookup (cl->priv->keys, key);
		if (kd)
			kd->value.v_long = value;
	}
}

void
e_config_listener_set_string (EConfigListener *cl, const char *key, const char *value)
{
	char *s1, *s2;
	KeyData *kd;
	GError *err = NULL;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (key != NULL);

	/* check that the value is not the same */
	s1 = (char *) value;
	s2 = e_config_listener_get_string_with_default (cl, key, NULL, NULL);
	if (!strcmp (s1 ? s1 : "", s2 ? s2 : "")) {
		g_free (s2);
		return;
	}

	g_free (s2);

	gconf_client_set_string (cl->priv->db, key, value, &err);
	if (err) {
		g_warning ("e_config_listener_set_bool: %s", err->message);
		g_error_free (err);
	} else {
		/* update the internal copy */
		kd = g_hash_table_lookup (cl->priv->keys, key);
		if (kd) {
			g_free (kd->value.v_str);
			kd->value.v_str = g_strdup (value);
		}
	}
}

void
e_config_listener_remove_value (EConfigListener *cl, const char *key)
{
	gpointer orig_key, orig_value;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (key != NULL);

	if (g_hash_table_lookup_extended (cl->priv->keys, key, &orig_key, &orig_value)) {
		KeyData *kd = orig_value;

		g_hash_table_remove (cl->priv->keys, key);
		g_free (kd->key);
		if (kd->type == GCONF_VALUE_STRING)
			g_free (kd->value.v_str);
		gconf_client_notify_remove (cl->priv->db, kd->lid);

		g_free (kd);
	}

	gconf_client_unset (cl->priv->db, key, NULL);
}

void
e_config_listener_remove_dir (EConfigListener *cl, const char *dir)
{
	GSList *slist, *iter;
	const gchar *key;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (dir != NULL);

	slist = gconf_client_all_entries (cl->priv->db, dir, NULL);
        for (iter = slist; iter != NULL; iter = iter->next) {
                GConfEntry *entry = iter->data;
                                                                                                
                key = gconf_entry_get_key (entry);
                gconf_client_unset (cl->priv->db, key, NULL);
                gconf_entry_free (entry);
        }

        g_slist_free (slist);
}
