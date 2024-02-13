/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
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
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#include "evolution-config.h"

#include <gtk/gtk.h>

#include "e-util-private.h"
#include "e-misc-utils.h"

#include "e-spinner.h"

#define MAIN_IMAGE_FILENAME	"working.png"
#define FRAME_SIZE		22
#define FRAME_TIMEOUT_MS	100

struct _ESpinnerPrivate
{
	GSList *pixbufs;
	GSList *current_frame; /* link of 'pixbufs' */
	gboolean active;
	guint timeout_id;
};

enum {
	PROP_0,
	PROP_ACTIVE
};

G_DEFINE_TYPE_WITH_PRIVATE (ESpinner, e_spinner, GTK_TYPE_IMAGE)

static gboolean
e_spinner_update_frame_cb (gpointer user_data)
{
	ESpinner *spinner = user_data;

	g_return_val_if_fail (E_IS_SPINNER (spinner), FALSE);

	if (spinner->priv->current_frame)
		spinner->priv->current_frame = spinner->priv->current_frame->next;
	if (!spinner->priv->current_frame)
		spinner->priv->current_frame = spinner->priv->pixbufs;

	if (!spinner->priv->current_frame) {
		g_warn_if_reached ();
		return FALSE;
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (spinner), spinner->priv->current_frame->data);

	return TRUE;
}

static void
e_spinner_disable_spin (ESpinner *spinner)
{
	if (spinner->priv->timeout_id) {
		g_source_remove (spinner->priv->timeout_id);
		spinner->priv->timeout_id = 0;
	}
}

static void
e_spinner_enable_spin (ESpinner *spinner)
{
	GtkSettings *settings;
	gboolean enable_animations = TRUE;

	settings = gtk_widget_get_settings (GTK_WIDGET (spinner));
	g_object_get (settings, "gtk-enable-animations", &enable_animations, NULL);

	e_spinner_disable_spin (spinner);

	if (spinner->priv->pixbufs && enable_animations)
		spinner->priv->timeout_id = g_timeout_add_full (
			G_PRIORITY_LOW, FRAME_TIMEOUT_MS, e_spinner_update_frame_cb, spinner, NULL);
}

static void
e_spinner_set_property (GObject *object,
			guint property_id,
			const GValue *value,
			GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE:
			e_spinner_set_active (
				E_SPINNER (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_spinner_get_property (GObject *object,
			guint property_id,
			GValue *value,
			GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ACTIVE:
			g_value_set_boolean (
				value,
				e_spinner_get_active (E_SPINNER (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_spinner_constructed (GObject *object)
{
	ESpinner *spinner;
	GdkPixbuf *main_pixbuf;
	gint xx, yy, width, height;
	GError *error = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_spinner_parent_class)->constructed (object);

	spinner = E_SPINNER (object);

#ifdef G_OS_WIN32
	{
		gchar *filename = g_strconcat (EVOLUTION_IMAGESDIR, G_DIR_SEPARATOR_S, MAIN_IMAGE_FILENAME, NULL);
		main_pixbuf = e_misc_util_ref_pixbuf (filename, &error);
		g_free (filename);
	}
#else
	main_pixbuf = e_misc_util_ref_pixbuf (EVOLUTION_IMAGESDIR G_DIR_SEPARATOR_S MAIN_IMAGE_FILENAME, &error);
#endif
	if (!main_pixbuf) {
		g_warning ("%s: Failed to load image: %s", error ? error->message : "Unknown error", G_STRFUNC);
		g_clear_error (&error);
		return;
	}

	width = gdk_pixbuf_get_width (main_pixbuf);
	height = gdk_pixbuf_get_height (main_pixbuf);

	for (yy = 0; yy < height; yy += FRAME_SIZE) {
		for (xx = 0; xx < width; xx+= FRAME_SIZE) {
			GdkPixbuf *frame;

			frame = gdk_pixbuf_new_subpixbuf (main_pixbuf, xx, yy, FRAME_SIZE, FRAME_SIZE);
			if (frame)
				spinner->priv->pixbufs = g_slist_prepend (spinner->priv->pixbufs, frame);
		}
	}

	g_object_unref (main_pixbuf);

	spinner->priv->pixbufs = g_slist_reverse (spinner->priv->pixbufs);

	spinner->priv->current_frame = spinner->priv->pixbufs;
	if (spinner->priv->pixbufs)
		gtk_image_set_from_pixbuf (GTK_IMAGE (spinner), spinner->priv->pixbufs->data);
}

static void
e_spinner_dispose (GObject *object)
{
	/* This resets the timeout_id too */
	e_spinner_set_active (E_SPINNER (object), FALSE);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_spinner_parent_class)->dispose (object);
}

static void
e_spinner_finalize (GObject *object)
{
	ESpinner *spinner = E_SPINNER (object);

	g_slist_free_full (spinner->priv->pixbufs, g_object_unref);
	spinner->priv->pixbufs = NULL;
	spinner->priv->current_frame = NULL;

	g_warn_if_fail (spinner->priv->timeout_id == 0);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_spinner_parent_class)->finalize (object);
}

static void
e_spinner_realize (GtkWidget *widget)
{
	ESpinner *spinner = E_SPINNER (widget);

	/* Chain up to the parent class first, then enable the spinner
	 * after the widget is realized
	 */
	GTK_WIDGET_CLASS (e_spinner_parent_class)->realize (widget);

	if (spinner->priv->active)
		e_spinner_enable_spin (spinner);
}

static void
e_spinner_unrealize (GtkWidget *widget)
{
	ESpinner *spinner = E_SPINNER (widget);

	/* Disable the spinner before chaining up to the parent class
	 * to unrealize the widget
	 */
	e_spinner_disable_spin (spinner);

	GTK_WIDGET_CLASS (e_spinner_parent_class)->unrealize (widget);
}

static void
e_spinner_class_init (ESpinnerClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_spinner_set_property;
	object_class->get_property = e_spinner_get_property;
	object_class->dispose = e_spinner_dispose;
	object_class->finalize = e_spinner_finalize;
	object_class->constructed = e_spinner_constructed;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->realize = e_spinner_realize;
	widget_class->unrealize = e_spinner_unrealize;

	/**
	 * ESpinner:active:
	 *
	 * Whether the animation is active.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_ACTIVE,
		g_param_spec_boolean (
			"active",
			"Active",
			"Whether the animation is active",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));
}

static void
e_spinner_init (ESpinner *spinner)
{
	spinner->priv = e_spinner_get_instance_private (spinner);
}

GtkWidget *
e_spinner_new (void)
{
	return g_object_new (E_TYPE_SPINNER, NULL);
}

gboolean
e_spinner_get_active (ESpinner *spinner)
{
	g_return_val_if_fail (E_IS_SPINNER (spinner), FALSE);

	return spinner->priv->active;
}

void
e_spinner_set_active (ESpinner *spinner,
		      gboolean active)
{
	g_return_if_fail (E_IS_SPINNER (spinner));

	if ((spinner->priv->active ? 1 : 0) == (active ? 1 : 0))
		return;

	spinner->priv->active = active;

	if (gtk_widget_get_realized (GTK_WIDGET (spinner))) {
		if (active)
			e_spinner_enable_spin (spinner);
		else
			e_spinner_disable_spin (spinner);
	}

	g_object_notify (G_OBJECT (spinner), "active");
}

void
e_spinner_start (ESpinner *spinner)
{
	e_spinner_set_active (spinner, TRUE);
}

void
e_spinner_stop (ESpinner *spinner)
{
	e_spinner_set_active (spinner, FALSE);
}
