/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-composer-header.h"

#define E_COMPOSER_HEADER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COMPOSER_HEADER, EComposerHeaderPrivate))

enum {
	PROP_0,
	PROP_BUTTON,
	PROP_LABEL,
	PROP_SENSITIVE,
	PROP_VISIBLE
};

enum {
	CHANGED,
	CLICKED,
	LAST_SIGNAL
};

struct _EComposerHeaderPrivate {
	gchar *label;
	gboolean button;
};

static gpointer parent_class;
static guint signal_ids[LAST_SIGNAL];

static void
composer_header_button_clicked_cb (GtkButton *button,
                                   EComposerHeader *header)
{
	gtk_widget_grab_focus (header->input_widget);
	g_signal_emit (header, signal_ids[CLICKED], 0);
}

static GObject *
composer_header_constructor (GType type,
                             guint n_construct_properties,
                             GObjectConstructParam *construct_properties)
{
	GObject *object;
	GtkWidget *widget;
	EComposerHeader *header;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	header = E_COMPOSER_HEADER (object);

	if (header->priv->button) {
		widget = gtk_button_new_with_mnemonic (header->priv->label);
		GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
		g_signal_connect (
			widget, "clicked",
			G_CALLBACK (composer_header_button_clicked_cb),
			header);
	} else {
		widget = gtk_label_new_with_mnemonic (header->priv->label);
		gtk_label_set_mnemonic_widget (
			GTK_LABEL (widget), header->input_widget);
	}
	header->title_widget = g_object_ref_sink (widget);

	g_free (header->priv->label);
	header->priv->label = NULL;

	return object;
}

static void
composer_header_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	EComposerHeaderPrivate *priv;

	priv = E_COMPOSER_HEADER_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_BUTTON:	/* construct only */
			priv->button = g_value_get_boolean (value);
			return;

		case PROP_LABEL:	/* construct only */
			priv->label = g_value_dup_string (value);
			return;

		case PROP_SENSITIVE:
			e_composer_header_set_sensitive (
				E_COMPOSER_HEADER (object),
				g_value_get_boolean (value));
			return;

		case PROP_VISIBLE:
			e_composer_header_set_visible (
				E_COMPOSER_HEADER (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_header_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	EComposerHeaderPrivate *priv;

	priv = E_COMPOSER_HEADER_GET_PRIVATE (object);

	switch (property_id) {
		case PROP_BUTTON:	/* construct only */
			g_value_set_boolean (value, priv->button);
			return;

		case PROP_LABEL:	/* construct only */
			g_value_take_string (
				value, e_composer_header_get_label (
				E_COMPOSER_HEADER (object)));
			return;

		case PROP_SENSITIVE:
			g_value_set_boolean (
				value, e_composer_header_get_sensitive (
				E_COMPOSER_HEADER (object)));
			return;

		case PROP_VISIBLE:
			g_value_set_boolean (
				value, e_composer_header_get_visible (
				E_COMPOSER_HEADER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
composer_header_dispose (GObject *object)
{
	EComposerHeader *header = E_COMPOSER_HEADER (object);

	if (header->title_widget != NULL) {
		g_object_unref (header->title_widget);
		header->title_widget = NULL;
	}

	if (header->input_widget != NULL) {
		g_object_unref (header->input_widget);
		header->input_widget = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
composer_header_class_init (EComposerHeaderClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EComposerHeaderPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = composer_header_constructor;
	object_class->set_property = composer_header_set_property;
	object_class->get_property = composer_header_get_property;
	object_class->dispose = composer_header_dispose;

	g_object_class_install_property (
		object_class,
		PROP_BUTTON,
		g_param_spec_boolean (
			"button",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_LABEL,
		g_param_spec_string (
			"label",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SENSITIVE,
		g_param_spec_boolean (
			"sensitive",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_VISIBLE,
		g_param_spec_boolean (
			"visible",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	signal_ids[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signal_ids[CLICKED] = g_signal_new (
		"clicked",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
composer_header_init (EComposerHeader *header)
{
	header->priv = E_COMPOSER_HEADER_GET_PRIVATE (header);
}

GType
e_composer_header_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EComposerHeaderClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) composer_header_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EComposerHeader),
			0,     /* n_preallocs */
			(GInstanceInitFunc) composer_header_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_OBJECT, "EComposerHeader",
			&type_info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

gchar *
e_composer_header_get_label (EComposerHeader *header)
{
	gchar *label;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER (header), NULL);

	/* GtkButton and GtkLabel both have a "label" property. */
	g_object_get (header->title_widget, "label", &label, NULL);

	return label;
}

gboolean
e_composer_header_get_sensitive (EComposerHeader *header)
{
	gboolean sensitive;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER (header), FALSE);

	sensitive = GTK_WIDGET_SENSITIVE (header->title_widget);
	if (GTK_WIDGET_SENSITIVE (header->input_widget) != sensitive)
		g_warning ("%s: Sensitivity is out of sync", G_STRFUNC);

	return sensitive;
}

void
e_composer_header_set_sensitive (EComposerHeader *header,
                                 gboolean sensitive)
{
	g_return_if_fail (E_IS_COMPOSER_HEADER (header));

	gtk_widget_set_sensitive (header->title_widget, sensitive);
	gtk_widget_set_sensitive (header->input_widget, sensitive);

	g_object_notify (G_OBJECT (header), "sensitive");
}

gboolean
e_composer_header_get_visible (EComposerHeader *header)
{
	gboolean visible;

	g_return_val_if_fail (E_IS_COMPOSER_HEADER (header), FALSE);

	visible = GTK_WIDGET_VISIBLE (header->title_widget);
	if (GTK_WIDGET_VISIBLE (header->input_widget) != visible)
		g_warning ("%s: Visibility is out of sync", G_STRFUNC);

	return visible;
}

void
e_composer_header_set_visible (EComposerHeader *header,
                               gboolean visible)
{
	g_return_if_fail (E_IS_COMPOSER_HEADER (header));

	if (visible) {
		gtk_widget_show (header->title_widget);
		gtk_widget_show (header->input_widget);
	} else {
		gtk_widget_hide (header->title_widget);
		gtk_widget_hide (header->input_widget);
	}

	g_object_notify (G_OBJECT (header), "visible");
}

void
e_composer_header_set_title_tooltip (EComposerHeader *header,
                                     const gchar *tooltip)
{
	g_return_if_fail (E_IS_COMPOSER_HEADER (header));

	gtk_widget_set_tooltip_text (header->title_widget, tooltip);
}

void
e_composer_header_set_input_tooltip (EComposerHeader *header,
                                     const gchar *tooltip)
{
	g_return_if_fail (E_IS_COMPOSER_HEADER (header));

	gtk_widget_set_tooltip_text (header->input_widget, tooltip);
}
