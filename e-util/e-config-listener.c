/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Configuration component listener
 *
 * Author:
 *   Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2002, Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtksignal.h>
#include <gtk/gtktypeutils.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-event-source.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-object.h>
#include "e-config-listener.h"

#define PARENT_TYPE GTK_TYPE_OBJECT

typedef struct {
	EConfigListener *cl;
	Bonobo_EventSource_ListenerId lid;
	char *key;
	GtkFundamentalType type;
	union {
		gboolean v_bool;
		float v_float;
		long v_long;
		char *v_str;
	} value;
	gboolean used_default;
} KeyData;

struct _EConfigListenerPrivate {
	Bonobo_ConfigDatabase db;
	GHashTable *keys;
};

static void e_config_listener_class_init (EConfigListenerClass *klass);
static void e_config_listener_init       (EConfigListener *cl);
static void e_config_listener_destroy    (GtkObject *object);

static GtkObjectClass *parent_class = NULL;

enum {
	KEY_CHANGED,
	LAST_SIGNAL
};

static guint config_listener_signals[LAST_SIGNAL];

static void
e_config_listener_class_init (EConfigListenerClass *klass)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = e_config_listener_destroy;
	klass->key_changed = NULL;

	config_listener_signals[KEY_CHANGED] =
		gtk_signal_new ("key_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EConfigListenerClass, key_changed),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1, GTK_TYPE_STRING);
	gtk_object_class_add_signals (object_class, config_listener_signals, LAST_SIGNAL);
}

static void
e_config_listener_init (EConfigListener *cl)
{
	CORBA_Environment ev;

	/* allocate internal structure */
	cl->priv = g_new0 (EConfigListenerPrivate, 1);

	cl->priv->keys = g_hash_table_new (g_str_hash, g_str_equal);

	/* activate the configuration database */
	CORBA_exception_init (&ev);
	cl->priv->db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);

	if (BONOBO_EX (&ev) || cl->priv->db == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		cl->priv->db = CORBA_OBJECT_NIL;
	}
}

static void
free_key_hash (gpointer key, gpointer value, gpointer user_data)
{
	KeyData *kd = (KeyData *) value;

	g_return_if_fail (kd != NULL);

	bonobo_event_source_client_remove_listener (kd->cl->priv->db, kd->lid, NULL);

	g_free (kd->key);
	switch (kd->type) {
	case GTK_TYPE_STRING :
		g_free (kd->value.v_str);
		break;
	default :
		break;
	}

	g_free (kd);
}

static void
e_config_listener_destroy (GtkObject *object)
{
	EConfigListener *cl = (EConfigListener *) object;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));

	g_hash_table_foreach (cl->priv->keys, (GHFunc) free_key_hash, NULL);
	g_hash_table_destroy (cl->priv->keys);
	cl->priv->keys = NULL;

	if (cl->priv->db != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (cl->priv->db, NULL);
		cl->priv->db = CORBA_OBJECT_NIL;
	}

	g_free (cl->priv);
	cl->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

GtkType
e_config_listener_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info = {
			"EConfigListener",
			sizeof (EConfigListener),
			sizeof (EConfigListenerClass),
			(GtkClassInitFunc) e_config_listener_class_init,
			(GtkObjectInitFunc) e_config_listener_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (PARENT_TYPE, &info);
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

	cl = gtk_type_new (E_CONFIG_LISTENER_TYPE);
	return cl;
}

static void
property_change_cb (BonoboListener *listener,
		    char *event_name,
		    CORBA_any *any,
		    CORBA_Environment *ev,
		    gpointer user_data)
{
	KeyData *kd = (KeyData *) user_data;

	g_return_if_fail (any != NULL);
	g_return_if_fail (kd != NULL);

	/* free previous value */
	if (kd->type == GTK_TYPE_STRING)
		g_free (kd->value.v_str);

	/* set new value */
	if (bonobo_arg_type_is_equal (any->_type, BONOBO_ARG_BOOLEAN, NULL)) {
		kd->type = GTK_TYPE_BOOL;
		kd->value.v_bool = BONOBO_ARG_GET_BOOLEAN (any);
	} else if (bonobo_arg_type_is_equal (any->_type, BONOBO_ARG_FLOAT, NULL)) {
		kd->type = GTK_TYPE_FLOAT;
		kd->value.v_float = BONOBO_ARG_GET_FLOAT (any);
	} else if (bonobo_arg_type_is_equal (any->_type, BONOBO_ARG_LONG, NULL)) {
		kd->type = GTK_TYPE_LONG;
		kd->value.v_long = BONOBO_ARG_GET_LONG (any);
	} else if (bonobo_arg_type_is_equal (any->_type, BONOBO_ARG_STRING, NULL)) {
		kd->type = GTK_TYPE_STRING;
		kd->value.v_str = g_strdup (BONOBO_ARG_GET_STRING (any));
	} else
		return;

	gtk_signal_emit (GTK_OBJECT (kd->cl), config_listener_signals[KEY_CHANGED], kd->key);
}

static KeyData *
add_key (EConfigListener *cl, const char *key, GtkFundamentalType type,
	 gpointer value, gboolean used_default)
{
	KeyData *kd;
	char *event_name;
	char *ch;
	CORBA_Environment ev;

	/* add the key to our hash table */
	kd = g_new0 (KeyData, 1);
	kd->cl = cl;
	kd->key = g_strdup (key);
	kd->type = type;
	switch (type) {
	case GTK_TYPE_BOOL :
		memcpy (&kd->value.v_bool, value, sizeof (gboolean));
		break;
	case GTK_TYPE_FLOAT :
		memcpy (&kd->value.v_float, value, sizeof (float));
		break;
	case GTK_TYPE_LONG :
		memcpy (&kd->value.v_long, value, sizeof (long));
		break;
	case GTK_TYPE_STRING :
		kd->value.v_str = (char *) value;
		break;
	default :
		break;
	}

	kd->used_default = used_default;

	/* add the listener for changes */
	event_name = g_strdup_printf ("=Bonobo/ConfigDatabase:change%s",
				      kd->key);
	ch = strrchr (event_name, '/');
	if (ch)
		*ch = ':';

	CORBA_exception_init (&ev);
	kd->lid = bonobo_event_source_client_add_listener (
		cl->priv->db,
		property_change_cb,
		event_name,
		&ev, kd);
	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		g_free (event_name);
		free_key_hash (kd->key, kd, NULL);
		return NULL;
	}

	g_hash_table_insert (cl->priv->keys, kd->key, kd);

	CORBA_exception_free (&ev);
	g_free (event_name);

	return kd;
}

gboolean
e_config_listener_get_boolean_with_default (EConfigListener *cl,
					    const char *key,
					    gboolean def,
					    gboolean *used_default)
{
	gboolean value;
	KeyData *kd;
	gboolean d;
	gpointer orig_key, orig_value;

	g_return_val_if_fail (E_IS_CONFIG_LISTENER (cl), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);

	/* search for the key in our hash table */
	if (!g_hash_table_lookup_extended (cl->priv->keys, key, &orig_key, &orig_value)) {
		/* not found, so retrieve it from the configuration database */
		value = bonobo_config_get_boolean_with_default (cl->priv->db, key, def, &d);
		kd = add_key (cl, key, GTK_TYPE_BOOL, &value, d);

		if (used_default != NULL)
			*used_default = d;
	} else {
		kd = (KeyData *) orig_value;
		g_assert (kd != NULL);

		if (kd->type == GTK_TYPE_BOOL) {
			value = kd->value.v_bool;
			if (used_default != NULL)
				*used_default = kd->used_default;
		} else
			return FALSE;
	}

	return value;
}

float
e_config_listener_get_float_with_default (EConfigListener *cl,
					  const char *key,
					  float def,
					  gboolean *used_default)
{
	float value;
	KeyData *kd;
	gboolean d;
	gpointer orig_key, orig_value;

	g_return_val_if_fail (E_IS_CONFIG_LISTENER (cl), -1);
	g_return_val_if_fail (key != NULL, -1);

	/* search for the key in our hash table */
	if (!g_hash_table_lookup_extended (cl->priv->keys, key, &orig_key, &orig_value)) {
		/* not found, so retrieve it from the configuration database */
		value = bonobo_config_get_float_with_default (cl->priv->db, key, def, &d);
		kd = add_key (cl, key, GTK_TYPE_FLOAT, &value, d);

		if (used_default != NULL)
			*used_default = d;
	} else {
		kd = (KeyData *) orig_value;
		g_assert (kd != NULL);

		if (kd->type == GTK_TYPE_FLOAT) {
			value = kd->value.v_float;
			if (used_default != NULL)
				*used_default = kd->used_default;
		} else
			return -1;
	}

	return value;
}

long
e_config_listener_get_long_with_default (EConfigListener *cl,
					 const char *key,
					 long def,
					 gboolean *used_default)
{
	long value;
	KeyData *kd;
	gboolean d;
	gpointer orig_key, orig_value;

	g_return_val_if_fail (E_IS_CONFIG_LISTENER (cl), -1);
	g_return_val_if_fail (key != NULL, -1);

	/* search for the key in our hash table */
	if (!g_hash_table_lookup_extended (cl->priv->keys, key, &orig_key, &orig_value)) {
		/* not found, so retrieve it from the configuration database */
		value = bonobo_config_get_long_with_default (cl->priv->db, key, def, &d);
		kd = add_key (cl, key, GTK_TYPE_LONG, &value, d);

		if (used_default != NULL)
			*used_default = d;
	} else {
		kd = (KeyData *) orig_value;
		g_assert (kd != NULL);

		if (kd->type == GTK_TYPE_LONG) {
			value = kd->value.v_long;
			if (used_default != NULL)
				*used_default = kd->used_default;
		} else
			return -1;
	}

	return value;
}

char *
e_config_listener_get_string_with_default (EConfigListener *cl,
					   const char *key,
					   const char *def,
					   gboolean *used_default)
{
	char *str;
	KeyData *kd;
	gboolean d;
	gpointer orig_key, orig_value;

	g_return_val_if_fail (E_IS_CONFIG_LISTENER (cl), NULL);
	g_return_val_if_fail (key != NULL, NULL);

	/* search for the key in our hash table */
	if (!g_hash_table_lookup_extended (cl->priv->keys, key, &orig_key, &orig_value)) {
		/* not found, so retrieve it from the configuration database */
		str = bonobo_config_get_string_with_default (cl->priv->db, key, (char *) def, &d);
		if (str) {
			kd = add_key (cl, key, GTK_TYPE_STRING, (gpointer) str, d);

			if (used_default != NULL)
				*used_default = d;
		} else
			return NULL;
	} else {
		kd = (KeyData *) orig_value;
		g_assert (kd != NULL);

		if (kd->type == GTK_TYPE_STRING) {
			str = kd->value.v_str;
			if (used_default != NULL)
				*used_default = kd->used_default;
		} else
			return NULL;
	}

	return g_strdup (str);
}

void
e_config_listener_set_boolean (EConfigListener *cl, const char *key, gboolean value)
{
	CORBA_Environment ev;
	KeyData *kd;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (key != NULL);

	/* check that the value is not the same */
	if (value == e_config_listener_get_boolean_with_default (cl, key, 0, NULL))
		return;

	CORBA_exception_init (&ev);

	bonobo_config_set_boolean (cl->priv->db, key, value, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot save config key %s -- %s", key, BONOBO_EX_ID (&ev));
	else {
		/* update the internal copy */
		kd = g_hash_table_lookup (cl->priv->keys, key);
		if (kd)
			kd->value.v_bool = value;
	}

	CORBA_exception_free (&ev);
}

void
e_config_listener_set_float (EConfigListener *cl, const char *key, float value)
{
	CORBA_Environment ev;
	KeyData *kd;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (key != NULL);

	/* check that the value is not the same */
	if (value == e_config_listener_get_float_with_default (cl, key, 0, NULL))
		return;

	CORBA_exception_init (&ev);

	bonobo_config_set_float (cl->priv->db, key, value, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot save config key %s -- %s", key, BONOBO_EX_ID (&ev));
	else {
		/* update the internal copy */
		kd = g_hash_table_lookup (cl->priv->keys, key);
		if (kd)
			kd->value.v_float = value;
	}

	CORBA_exception_free (&ev);
}

void
e_config_listener_set_long (EConfigListener *cl, const char *key, long value)
{
	CORBA_Environment ev;
	KeyData *kd;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (key != NULL);

	/* check that the value is not the same */
	if (value == e_config_listener_get_long_with_default (cl, key, 0, NULL))
		return;

	CORBA_exception_init (&ev);

	bonobo_config_set_long (cl->priv->db, key, value, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot save config key %s -- %s", key, BONOBO_EX_ID (&ev));
	else {
		/* update the internal copy */
		kd = g_hash_table_lookup (cl->priv->keys, key);
		if (kd)
			kd->value.v_long = value;
	}

	CORBA_exception_free (&ev);
}

void
e_config_listener_set_string (EConfigListener *cl, const char *key, const char *value)
{
	CORBA_Environment ev;
	char *s1, *s2;
	KeyData *kd;

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

	CORBA_exception_init (&ev);

	bonobo_config_set_string (cl->priv->db, key, value, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot save config key %s -- %s", key, BONOBO_EX_ID (&ev));
	else {
		/* update the internal copy */
		kd = g_hash_table_lookup (cl->priv->keys, key);
		if (kd) {
			g_free (kd->value.v_str);
			kd->value.v_str = g_strdup (value);
		}
	}

	CORBA_exception_free (&ev);
}

void
e_config_listener_remove_dir (EConfigListener *cl, const char *dir)
{
	CORBA_Environment ev;

	g_return_if_fail (E_IS_CONFIG_LISTENER (cl));
	g_return_if_fail (dir != NULL);

	CORBA_exception_init (&ev);
	Bonobo_ConfigDatabase_removeDir (cl->priv->db, dir, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Cannot remove config dir %s -- %s", dir, BONOBO_EX_ID (&ev));
	}

	CORBA_exception_free (&ev);
}

Bonobo_ConfigDatabase
e_config_listener_get_db (EConfigListener *cl)
{
	g_return_val_if_fail (E_IS_CONFIG_LISTENER (cl), CORBA_OBJECT_NIL);
	return cl->priv->db;
}
