/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-composer-header.h"

#include <glib/gi18n-lib.h>

#define E_COMPOSER_HEADER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COMPOSER_HEADER, EComposerHeaderPrivate))

struct _EComposerHeaderPrivate {
	gchar *label;
	gboolean button;

	ESourceRegistry *registry;

	guint sensitive : 1;
	guint visible : 1;
};

#define E_COMPOSER_HEADER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_COMPOSER_HEADER, EComposerHeaderPrivate))

enum {
	PROP_0,
	PROP_BUTTON,
	PROP_LABEL,
	PROP_REGISTRY,
	PROP_SENSITIVE,
	PROP_VISIBLE
};

enum {
	CHANGED,
	CLICKED,
	LAST_SIGNAL
};

static guint signal_ids[LAST_SIGNAL];

G_DEFINE_ABSTRACT_TYPE (
	EComposerHeader,
	e_composer_header,
	G_TYPE_OBJECT)

static void
composer_header_button_clicked_cb (GtkButton *button,
                                   EComposerHeader *header)
{
	gtk_widget_grab_focus (header->input_widget);
	g_signal_emit (header, signal_ids[CLICKED], 0);
}

static void
composer_header_set_registry (EComposerHeader *header,
                              ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (header->priv->registry == NULL);

	header->priv->registry = g_object_ref (registry);
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

		case PROP_REGISTRY:
			composer_header_set_registry (
				E_COMPOSER_HEADER (object),
				g_value_get_object (value));
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
			g_value_set_string (value, priv->label);
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value, e_composer_header_get_registry (
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

	if (header->priv->registry != NULL) {
		g_object_unref (header->priv->registry);
		header->priv->registry = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_composer_header_parent_class)->dispose (object);
}

static void
composer_header_finalize (GObject *object)
{
	EComposerHeaderPrivate *priv;

	priv = E_COMPOSER_HEADER_GET_PRIVATE (object);

	g_free (priv->label);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_composer_header_parent_class)->finalize (object);
}

static void
composer_header_constructed (GObject *object)
{
	EComposerHeader *header;
	GtkWidget *widget;
	GtkWidget *label;

	header = E_COMPOSER_HEADER (object);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_composer_header_parent_class)->constructed (object);

	if (header->input_widget == NULL) {
		g_critical (
			"EComposerHeader's input_widget "
			"must be set before chaining up");
		return;
	}

	if (header->priv->button) {
		widget = gtk_button_new_with_mnemonic (header->priv->label);
		gtk_widget_set_can_focus (widget, FALSE);
		g_signal_connect (
			widget, "clicked",
			G_CALLBACK (composer_header_button_clicked_cb),
			header);
		label = gtk_bin_get_child (GTK_BIN (widget));
	} else {
		widget = gtk_label_new_with_mnemonic (header->priv->label);
		gtk_label_set_mnemonic_widget (
			GTK_LABEL (widget), header->input_widget);
		label = widget;
	}

	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);

	header->title_widget = g_object_ref_sink (widget);

	e_binding_bind_property (
		header, "visible",
		header->title_widget, "visible",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		header, "visible",
		header->input_widget, "visible",
		G_BINDING_SYNC_CREATE);
}

static void
e_composer_header_class_init (EComposerHeaderClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EComposerHeaderPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = composer_header_set_property;
	object_class->get_property = composer_header_get_property;
	object_class->dispose = composer_header_dispose;
	object_class->finalize = composer_header_finalize;
	object_class->constructed = composer_header_constructed;

	g_object_class_install_property (
		object_class,
		PROP_BUTTON,
		g_param_spec_boolean (
			"button",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_LABEL,
		g_param_spec_string (
			"label",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			NULL,
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SENSITIVE,
		g_param_spec_boolean (
			"sensitive",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_VISIBLE,
		g_param_spec_boolean (
			"visible",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	signal_ids[CHANGED] = g_signal_new (
		"changed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EComposerHeaderClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signal_ids[CLICKED] = g_signal_new (
		"clicked",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EComposerHeaderClass, clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_composer_header_init (EComposerHeader *header)
{
	header->priv = E_COMPOSER_HEADER_GET_PRIVATE (header);
}

const gchar *
e_composer_header_get_label (EComposerHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER (header), NULL);

	return header->priv->label;
}

ESourceRegistry *
e_composer_header_get_registry (EComposerHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER (header), NULL);

	return header->priv->registry;
}

gboolean
e_composer_header_get_sensitive (EComposerHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER (header), FALSE);

	return header->priv->sensitive;
}

void
e_composer_header_set_sensitive (EComposerHeader *header,
                                 gboolean sensitive)
{
	g_return_if_fail (E_IS_COMPOSER_HEADER (header));

	if (header->priv->sensitive == sensitive)
		return;

	header->priv->sensitive = sensitive;

	g_object_notify (G_OBJECT (header), "sensitive");
}

gboolean
e_composer_header_get_visible (EComposerHeader *header)
{
	g_return_val_if_fail (E_IS_COMPOSER_HEADER (header), FALSE);

	return header->priv->visible;
}

void
e_composer_header_set_visible (EComposerHeader *header,
                               gboolean visible)
{
	g_return_if_fail (E_IS_COMPOSER_HEADER (header));

	if (header->priv->visible == visible)
		return;

	header->priv->visible = visible;

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
