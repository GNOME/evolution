/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Srinivasa Ragavan <sragavan@gnome.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gstdio.h>
#include "e-logger.h"
#include "e-mktemp.h"

/* 5 Minutes */
#define TIMEOUT_INTERVAL	300000 

static GObjectClass *parent;

struct _ELoggerPrivate {
	char *component;
	char *logfile;
	FILE *fp;

	guint timer;
};

static gboolean
flush_logfile (ELogger *el)
{
	fflush (el->priv->fp);
	el->priv->timer = 0;

	return FALSE;
}

static void
el_init(GObject *o)
{
	ELogger *l = (ELogger *) o;
	ELoggerPrivate *priv;
	
	priv = g_new (ELoggerPrivate, 1);
	priv->logfile = NULL;
	priv->fp = NULL;
	l->priv=priv;
}

static void
el_finalise(GObject *o)
{
	ELogger *el = (ELogger *) o;
	ELoggerPrivate *priv = el->priv;
	
	if (priv->timer)
		g_source_remove (priv->timer);
	flush_logfile (el);
	fclose (el->priv->fp);
	g_free (el->priv);
	((GObjectClass *)parent)->finalize(o);
}

static void
el_class_init(GObjectClass *klass)
{
	klass->finalize = el_finalise;

}

GType
e_logger_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(ELoggerClass),
			NULL, NULL,
			(GClassInitFunc)el_class_init,
			NULL, NULL,
			sizeof(ELogger), 0,
			(GInstanceInitFunc)el_init
		};
		parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "ELogger", &info, 0);
	}

	return type;
}


ELogger *e_logger_create(char *component)
{
	ELogger *el = g_object_new(e_logger_get_type(), 0);
	ELoggerPrivate *priv;
	char *tmp;

	tmp = g_strdup_printf("%s.log.XXXXXX", component);
	priv = el->priv;
	priv->component = g_strdup (component);
	priv->logfile = e_mktemp (tmp);
	g_free (tmp);
	priv->fp = g_fopen (priv->logfile, "w");

	priv->timer = 0;
	return el;
}



static void set_dirty (ELogger *el)
{
	if (el->priv->timer)
		return;

	el->priv->timer = g_timeout_add (TIMEOUT_INTERVAL, (GSourceFunc) flush_logfile, el);
}

void
e_logger_log (ELogger *el, int level, char *primary, char *secondary)
{
	time_t t = time (NULL);

	fprintf(el->priv->fp, "%d:%ld:%s\n", level, t, primary);
	fprintf(el->priv->fp, "%d:%ld:%s\n", level, t, secondary);
	set_dirty (el);
}

void 
e_logger_get_logs (ELogger *el, ELogFunction func, gpointer data)
{
	FILE *fp;
	char buf[250];
	gboolean error = FALSE;

	/* Flush everything before we get the logs */
	fflush (el->priv->fp);	
	fp = g_fopen (el->priv->logfile, "r");
	while (!error || feof(fp)) {
		char *tmp;
		tmp = fgets (buf, 250, fp);
		if (!tmp)
			break;
#if 0		
		if (strlen(tmp) == 249) {
			/* FIXME: There may be more */
		}
#endif 		
		func (tmp, data);
	}
	fclose(fp);
}

