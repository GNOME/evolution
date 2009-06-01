/*
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
 * Authors:
 *		Srinivasa Ragavan <sragavan@gnome.org>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include "e-logger.h"
#include "e-mktemp.h"

/* 5 Minutes */
#define TIMEOUT_INTERVAL	300

#define E_LOGGER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_LOGGER, ELoggerPrivate))

struct _ELoggerPrivate {
	gchar *component;
	gchar *logfile;
	FILE *fp;

	guint timer;
};

enum {
	PROP_0,
	PROP_COMPONENT
};

static gpointer parent_class;

static gboolean
flush_logfile (ELogger *logger)
{
	if (logger->priv->fp)
		fflush (logger->priv->fp);
	logger->priv->timer = 0;

	return FALSE;
}

static void
logger_set_component (ELogger *logger,
                      const gchar *component)
{
	gchar *temp;

	g_return_if_fail (logger->priv->component == NULL);

	temp = g_strdup_printf ("%s.log.XXXXXX", component);

	logger->priv->component = g_strdup (component);
	logger->priv->logfile = e_mktemp (temp);
	logger->priv->fp = g_fopen (logger->priv->logfile, "w");
	logger->priv->timer = 0;

	if (!logger->priv->fp)
		g_warning ("%s: Failed to open log file '%s' for writing.", G_STRFUNC, logger->priv->logfile ? logger->priv->logfile : "[null]");

	g_free (temp);
}

static void
logger_set_property (GObject *object,
                     guint property_id,
                     const GValue *value,
                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COMPONENT:
			logger_set_component (
				E_LOGGER (object),
				g_value_get_string (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
logger_get_property (GObject *object,
                     guint property_id,
                     GValue *value,
                     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COMPONENT:
			g_value_set_string (
				value, e_logger_get_component (
				E_LOGGER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
logger_finalize (GObject *object)
{
	ELogger *logger = E_LOGGER (object);

	if (logger->priv->timer)
		g_source_remove (logger->priv->timer);
	flush_logfile (logger);
	if (logger->priv->fp)
		fclose (logger->priv->fp);

	g_free (logger->priv->component);
	g_free (logger->priv->logfile);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
logger_class_init (ELoggerClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ELoggerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = logger_set_property;
	object_class->get_property = logger_get_property;
	object_class->finalize = logger_finalize;

	g_object_class_install_property (
		object_class,
		PROP_COMPONENT,
		g_param_spec_string (
			"component",
			_("Component"),
			_("Name of the component being logged"),
			"anonymous",
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
logger_init (ELogger *logger)
{
	logger->priv = E_LOGGER_GET_PRIVATE (logger);
}

GType
e_logger_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ELoggerClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) logger_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ELogger),
			0,     /* n_preallocs */
			(GInstanceInitFunc) logger_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "ELogger", &type_info, 0);
	}

	return type;
}

ELogger *
e_logger_create (const gchar *component)
{
	g_return_val_if_fail (component != NULL, NULL);

	return g_object_new (E_TYPE_LOGGER, "component", component, NULL);
}

const gchar *
e_logger_get_component (ELogger *logger)
{
	g_return_val_if_fail (E_IS_LOGGER (logger), NULL);

	return logger->priv->component;
}

static void
set_dirty (ELogger *logger)
{
	if (logger->priv->timer)
		return;

	logger->priv->timer = g_timeout_add_seconds (
		TIMEOUT_INTERVAL, (GSourceFunc) flush_logfile, logger);
}

void
e_logger_log (ELogger *logger,
              gint level,
              gchar *primary,
              gchar *secondary)
{
	time_t t = time (NULL);

	g_return_if_fail (E_LOGGER (logger));
	g_return_if_fail (primary != NULL);
	g_return_if_fail (secondary != NULL);

	if (!logger->priv->fp)
		return;

	fprintf (logger->priv->fp, "%d:%ld:%s\n", level, t, primary);
	fprintf (logger->priv->fp, "%d:%ld:%s\n", level, t, secondary);
	set_dirty (logger);
}

void
e_logger_get_logs (ELogger *logger,
                   ELogFunction func,
                   gpointer data)
{
	FILE *fp;
	gchar buf[250];

	g_return_if_fail (E_LOGGER (logger));
	g_return_if_fail (func != NULL);

	/* Flush everything before we get the logs */
	if (logger->priv->fp)
		fflush (logger->priv->fp);
	fp = g_fopen (logger->priv->logfile, "r");

	if (!fp) {
		g_warning ("%s: Cannot open log file '%s' for reading! No flush yet?\n", G_STRFUNC, logger->priv->logfile ? logger->priv->logfile : "[null]");
		return;
	}

	while (!feof (fp)) {
		gchar *tmp;
		gsize len;

		tmp = fgets (buf, sizeof (buf), fp);
		if (!tmp)
			break;

		len = strlen (tmp);
		if (len > 0 && tmp [len - 1] != '\n' && !feof (fp)) {
			/* there are more characters on a row than 249, so read them all */
			GString *str = g_string_sized_new (1024);

			g_string_append (str, tmp);

			while (!feof (fp) && len > 0 && tmp [len - 1] != '\n') {
				tmp = fgets (buf, sizeof (buf), fp);
				if (!tmp)
					break;

				len = strlen (tmp);
				g_string_append (str, tmp);
			}

			func (str->str, data);

			g_string_free (str, TRUE);
		} else
			func (tmp, data);
	}

	fclose (fp);
}
