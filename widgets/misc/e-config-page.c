/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-config-page.c
 *
 * Copyright (C) 2002  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-config-page.h"

#include <gal/util/e-util.h>


enum {
	APPLY,
	CHANGED,
	LAST_SIGNAL
};

#define PARENT_TYPE gtk_event_box_get_type ()
static GtkEventBoxClass *parent_class = NULL;
static unsigned int signals[LAST_SIGNAL] = { 0 };

struct _EConfigPagePrivate {
	gboolean changed;
};


/* GObject methods.  */

static void
impl_finalize (GObject *object)
{
	EConfigPage *page;
	EConfigPagePrivate *priv;

	page = E_CONFIG_PAGE (object);
	priv = page->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


static void
class_init (EConfigPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_ref(PARENT_TYPE);

	signals[APPLY] = gtk_signal_new ("apply",
					 GTK_RUN_LAST,
					 GTK_CLASS_TYPE (object_class),
					 G_STRUCT_OFFSET (EConfigPageClass, apply),
					 gtk_marshal_NONE__NONE,
					 GTK_TYPE_NONE, 0);

	signals[CHANGED] = gtk_signal_new ("changed",
					   GTK_RUN_FIRST,
					   GTK_CLASS_TYPE (object_class),
					   G_STRUCT_OFFSET (EConfigPageClass, changed),
					   gtk_marshal_NONE__NONE,
					   GTK_TYPE_NONE, 0);
}

static void
init (EConfigPage *config_page)
{
	EConfigPagePrivate *priv;

	priv = g_new (EConfigPagePrivate, 1);
	priv->changed = FALSE;

	config_page->priv = priv;
}


GtkWidget *
e_config_page_new (void)
{
	GtkWidget *new;

	new = gtk_type_new (e_config_page_get_type ());

	return new;
}


void
e_config_page_apply (EConfigPage *config_page)
{
	EConfigPagePrivate *priv;

	g_return_if_fail (E_IS_CONFIG_PAGE (config_page));

	priv = config_page->priv;

	gtk_signal_emit (GTK_OBJECT (config_page), signals[APPLY]);

	priv->changed = FALSE;
}

gboolean
e_config_page_is_applied (EConfigPage *config_page)
{
	g_return_val_if_fail (E_IS_CONFIG_PAGE (config_page), FALSE);

	return ! config_page->priv->changed;
}

void
e_config_page_changed (EConfigPage *config_page)
{
	EConfigPagePrivate *priv;

	g_return_if_fail (E_IS_CONFIG_PAGE (config_page));

	priv = config_page->priv;

	if (priv->changed)
		return;

	priv->changed = TRUE;
	gtk_signal_emit (GTK_OBJECT (config_page), signals[CHANGED]);
}


E_MAKE_TYPE (e_config_page, "EConfigPage", EConfigPage, class_init, init, PARENT_TYPE)
