/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-content.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-cal-shell-content.h"

#include <glib/gi18n.h>

#include "e-util/gconf-bridge.h"

#include "calendar/gui/calendar-config.h"

#include "widgets/menus/gal-view-etable.h"

#define E_CAL_SHELL_CONTENT_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_CONTENT, ECalShellContentPrivate))

struct _ECalShellContentPrivate {
	gint dummy;
};

enum {
	PROP_0
};

static gpointer parent_class;

static void
cal_shell_content_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_content_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_content_dispose (GObject *object)
{
	ECalShellContentPrivate *priv;

	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_shell_content_finalize (GObject *object)
{
	ECalShellContentPrivate *priv;

	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
cal_shell_content_constructed (GObject *object)
{
	ECalShellContentPrivate *priv;

	priv = E_CAL_SHELL_CONTENT_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);

}

static void
cal_shell_content_class_init (ECalShellContentClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalShellContentPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_shell_content_set_property;
	object_class->get_property = cal_shell_content_get_property;
	object_class->dispose = cal_shell_content_dispose;
	object_class->finalize = cal_shell_content_finalize;
	object_class->constructed = cal_shell_content_constructed;
}

static void
cal_shell_content_init (ECalShellContent *cal_shell_content)
{
	cal_shell_content->priv =
		E_CAL_SHELL_CONTENT_GET_PRIVATE (cal_shell_content);

	/* Postpone widget construction until we have a shell view. */
}

GType
e_cal_shell_content_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ECalShellContentClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) cal_shell_content_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ECalShellContent),
			0,     /* n_preallocs */
			(GInstanceInitFunc) cal_shell_content_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_CONTENT, "ECalShellContent",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_cal_shell_content_new (EShellView *shell_view)
{
	g_return_val_if_fail (E_IS_SHELL_VIEW (shell_view), NULL);

	return g_object_new (
		E_TYPE_CAL_SHELL_CONTENT,
		"shell-view", shell_view, NULL);
}
