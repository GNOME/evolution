/*
 * e-contact-marker.c
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
 * Copyright (C) 2008 Pierre-Luc Beaudoin <pierre-luc@pierlux.com>
 * Copyright (C) 2011 Jiri Techet <techet@gmail.com>
 * Copyright (C) 2011 Dan Vratil <dvratil@redhat.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WITH_CONTACT_MAPS

#include "e-contact-marker.h"

#include <champlain/champlain.h>
#include <gtk/gtk.h>
#include <clutter/clutter.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <cairo.h>
#include <math.h>
#include <string.h>

#define E_CONTACT_MARKER_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CONTACT_MARKER, EContactMarkerPrivate))

#define DEFAULT_FONT_NAME "Serif 9"

#define RADIUS 10
#define PADDING (RADIUS / 2)
#define HALF_PI (M_PI / 2.0)

struct _EContactMarkerPrivate {
	gchar *contact_uid;

	ClutterActor *image;
	ClutterActor *text_actor;

	ClutterActor *shadow;
	ClutterActor *background;

	guint total_width;
	guint total_height;

	ClutterGroup *content_group;

	guint redraw_id;
};

enum {
	DOUBLE_CLICKED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];

static ClutterColor DEFAULT_COLOR = { 0x33, 0x33, 0x33, 0xff };

G_DEFINE_TYPE (
	EContactMarker,
	e_contact_marker,
	CHAMPLAIN_TYPE_LABEL);

static gboolean
contact_marker_clicked_cb (ClutterActor *actor,
                           ClutterEvent *event,
                           gpointer user_data)
{
	gint click_count = clutter_event_get_click_count (event);

	if (click_count == 2)
		g_signal_emit (actor, signals[DOUBLE_CLICKED], 0);

	return TRUE;
}

static ClutterActor *
texture_new_from_pixbuf (GdkPixbuf *pixbuf,
                         GError **error)
{
	ClutterActor *texture = NULL;
	const guchar *data;
	gboolean has_alpha, success;
	gint width, height, rowstride;
	ClutterTextureFlags flags = 0;

	data = gdk_pixbuf_get_pixels (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	texture = clutter_texture_new ();
	success = clutter_texture_set_from_rgb_data (
		CLUTTER_TEXTURE (texture),
		data, has_alpha, width, height, rowstride,
		(has_alpha ? 4: 3), flags, NULL);

	if (!success) {
		clutter_actor_destroy (CLUTTER_ACTOR (texture));
		texture = NULL;
	}

	return texture;
}

static ClutterActor *
contact_photo_to_texture (EContactPhoto *photo)
{
	ClutterActor *texture = NULL;
	GdkPixbuf *pixbuf = NULL;

	if  (photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();

		gdk_pixbuf_loader_write (
			loader, photo->data.inlined.data,
			photo->data.inlined.length, NULL);
		gdk_pixbuf_loader_close (loader, NULL);
		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		if (pixbuf != NULL)
			g_object_ref (pixbuf);
		g_object_unref (loader);

	} else if (photo->type == E_CONTACT_PHOTO_TYPE_URI) {
		pixbuf = gdk_pixbuf_new_from_file (photo->data.uri, NULL);
	}

	if (pixbuf != NULL) {
		texture = texture_new_from_pixbuf (pixbuf, NULL);
		g_object_unref (pixbuf);
	}

	return texture;
}

static void
draw_box (cairo_t *cr,
          gint width,
          gint height,
          gint point)
{
	cairo_move_to (cr, RADIUS, 0);
	cairo_line_to (cr, width - RADIUS, 0);
	cairo_arc (cr, width - RADIUS, RADIUS, RADIUS - 1, 3 * HALF_PI, 0);
	cairo_line_to (cr, width, height - RADIUS);
	cairo_arc (cr, width - RADIUS, height - RADIUS, RADIUS - 1, 0, HALF_PI);
	cairo_line_to (cr, point, height);
	cairo_line_to (cr, 0, height + point);
	cairo_arc (cr, RADIUS, RADIUS, RADIUS - 1, M_PI, 3 * HALF_PI);
	cairo_close_path (cr);
}

static void
draw_shadow (EContactMarker *marker,
             gint width,
             gint height,
             gint point)
{
	ClutterActor *shadow = NULL;
	cairo_t *cr;
	gdouble slope;
	gdouble scaling;
	gint x;
	cairo_matrix_t matrix;

	slope = -0.3;
	scaling = 0.65;
	x = -40 * slope;

	shadow = clutter_cairo_texture_new (width + x, (height + point));
	cr = clutter_cairo_texture_create (CLUTTER_CAIRO_TEXTURE (shadow));

	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	cairo_matrix_init (&matrix, 1, 0, slope, scaling, x, 0);
	cairo_set_matrix (cr, &matrix);

	draw_box (cr, width, height, point);

	cairo_set_source_rgba (cr, 0, 0, 0, 0.15);
	cairo_fill (cr);

	cairo_destroy (cr);

	clutter_actor_set_position (shadow, 0, height / 2.0);

	clutter_container_add_actor (
		CLUTTER_CONTAINER (marker->priv->content_group), shadow);

	if (marker->priv->shadow != NULL) {
		clutter_container_remove_actor (
			CLUTTER_CONTAINER (marker->priv->content_group),
			marker->priv->shadow);
	}

	marker->priv->shadow = shadow;
}

static void
draw_background (EContactMarker *marker,
                 gint width,
                 gint height,
                 gint point)
{
	EContactMarkerPrivate *priv = marker->priv;
	ClutterActor *bg = NULL;
	const ClutterColor *color;
	ClutterColor darker_color;
	cairo_t *cr;

	bg = clutter_cairo_texture_new (width, height + point);
	cr = clutter_cairo_texture_create (CLUTTER_CAIRO_TEXTURE (bg));

	cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	/* If selected, add the selection color to the marker's color */
	if (champlain_marker_get_selected (CHAMPLAIN_MARKER (marker)))
		color = champlain_marker_get_selection_color ();
	else
		color = &DEFAULT_COLOR;

	draw_box (cr, width, height, point);

	clutter_color_darken (color, &darker_color);

	cairo_set_source_rgba (
		cr,
		color->red / 255.0,
		color->green / 255.0,
		color->blue / 255.0,
		color->alpha / 255.0);
	cairo_fill_preserve (cr);

	cairo_set_line_width (cr, 1.0);
	cairo_set_source_rgba (
		cr,
		darker_color.red / 255.0,
		darker_color.green / 255.0,
		darker_color.blue / 255.0,
		darker_color.alpha / 255.0);
	cairo_stroke (cr);
	cairo_destroy (cr);

	clutter_container_add_actor (CLUTTER_CONTAINER (priv->content_group), bg);

	if (priv->background != NULL) {
		clutter_container_remove_actor (
			CLUTTER_CONTAINER (priv->content_group),
			priv->background);
	}

	priv->background = bg;
}

static void
draw_marker (EContactMarker *marker)
{
	ChamplainLabel *label = CHAMPLAIN_LABEL (marker);
	guint height = 0, point = 0;
	guint total_width = 0, total_height = 0;
	ClutterText *text;

	if (marker->priv->image != NULL) {
		clutter_actor_set_position (
			marker->priv->image, 2 * PADDING, 2 * PADDING);
		if (clutter_actor_get_parent (marker->priv->image) == NULL)
			clutter_container_add_actor (
				CLUTTER_CONTAINER (marker->priv->content_group),
				marker->priv->image);
	}

	if (marker->priv->text_actor == NULL) {
		marker->priv->text_actor = clutter_text_new_with_text (
			"Serif 8",
			champlain_label_get_text (label));
		champlain_label_set_font_name (label, "Serif 8");
	}

	text = CLUTTER_TEXT (marker->priv->text_actor);
	clutter_text_set_text (
		text,
		champlain_label_get_text (label));
	clutter_text_set_font_name (
		text,
		champlain_label_get_font_name (label));
	clutter_text_set_line_alignment (text, PANGO_ALIGN_CENTER);
	clutter_text_set_line_wrap (text, TRUE);
	clutter_text_set_line_wrap_mode (text, PANGO_WRAP_WORD);
	clutter_text_set_ellipsize (
		text,
		champlain_label_get_ellipsize (label));
	clutter_text_set_attributes (
		text,
		champlain_label_get_attributes (label));
	clutter_text_set_use_markup (
		text,
		champlain_label_get_use_markup (label));

	if (marker->priv->image != NULL) {
		gfloat image_height;
		gfloat image_width;
		gfloat text_height;

		image_height = clutter_actor_get_height (marker->priv->image);
		image_width = clutter_actor_get_width (marker->priv->image);

		clutter_actor_set_width (
			marker->priv->text_actor, image_width);
		text_height = clutter_actor_get_height (
			marker->priv->text_actor);

		total_height =
			text_height + 2 * PADDING +
			image_height + 2 * PADDING;
		total_width = image_width + 4 * PADDING;

		clutter_actor_set_position (
			marker->priv->text_actor,
			PADDING, image_height + 2 * PADDING + 3);
	} else {
		gfloat text_height;
		gfloat text_width;

		text_height = clutter_actor_get_height (
			marker->priv->text_actor);
		text_width = clutter_actor_get_width (
			marker->priv->text_actor);

		total_height = text_height + 2 * PADDING;
		total_width = text_width + 4 * PADDING;

		clutter_actor_set_position (
			marker->priv->text_actor,
			2 * PADDING, PADDING);
	}

	height += 2 * PADDING;
	if (height > total_height)
		total_height = height;

	clutter_text_set_color (
		CLUTTER_TEXT (marker->priv->text_actor),
		(champlain_marker_get_selected (CHAMPLAIN_MARKER (marker)) ?
			champlain_marker_get_selection_text_color () :
			champlain_label_get_text_color (CHAMPLAIN_LABEL (marker))));
	if (clutter_actor_get_parent (marker->priv->text_actor) == NULL)
		clutter_container_add_actor (
			CLUTTER_CONTAINER (marker->priv->content_group),
			marker->priv->text_actor);

	if (marker->priv->text_actor == NULL && marker->priv->image == NULL) {
		total_width = 6 * PADDING;
		total_height = 6 * PADDING;
	}

	point = (total_height + 2 * PADDING) / 4.0;
	marker->priv->total_width = total_width;
	marker->priv->total_height = total_height;

	draw_shadow (marker, total_width, total_height, point);
	draw_background (marker, total_width, total_height, point);

	if (marker->priv->background != NULL) {
		if (marker->priv->text_actor != NULL)
			clutter_actor_raise (
				marker->priv->text_actor,
				marker->priv->background);
		if (marker->priv->image != NULL)
			clutter_actor_raise (
				marker->priv->image,
				marker->priv->background);
	}

	clutter_actor_set_anchor_point (
		CLUTTER_ACTOR (marker), 0, total_height + point);
}

static gboolean
redraw_on_idle (gpointer gobject)
{
	EContactMarker *marker = E_CONTACT_MARKER (gobject);

	draw_marker (marker);
	marker->priv->redraw_id = 0;
	return FALSE;
}

static void
queue_redraw (EContactMarker *marker)
{
	EContactMarkerPrivate *priv = marker->priv;

	if (!priv->redraw_id) {
		priv->redraw_id = g_idle_add_full (
			G_PRIORITY_DEFAULT,
			(GSourceFunc) redraw_on_idle,
			g_object_ref (marker),
			(GDestroyNotify) g_object_unref);
	}
}

static void
notify_selected (GObject *gobject,
                 G_GNUC_UNUSED GParamSpec *pspec,
                 G_GNUC_UNUSED gpointer user_data)
{
	queue_redraw (E_CONTACT_MARKER (gobject));
}

static void
contact_marker_dispose (GObject *object)
{
	EContactMarkerPrivate *priv;

	priv = E_CONTACT_MARKER_GET_PRIVATE (object);

	priv->background = NULL;
	priv->shadow = NULL;
	priv->text_actor = NULL;

	if (priv->redraw_id > 0) {
		g_source_remove (priv->redraw_id);
		priv->redraw_id = 0;
	}

	if (priv->content_group != NULL) {
		clutter_actor_unparent (CLUTTER_ACTOR (priv->content_group));
		priv->content_group = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_contact_marker_parent_class)->dispose (object);
}

static void
contact_marker_finalize (GObject *object)
{
	EContactMarkerPrivate *priv;

	priv = E_CONTACT_MARKER_GET_PRIVATE (object);

	g_free (priv->contact_uid);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_contact_marker_parent_class)->finalize (object);
}

static void
contact_marker_map (ClutterActor *actor)
{
	EContactMarker *marker;

	marker = E_CONTACT_MARKER (actor);

	/* Chain up to parent's map() method. */
	CLUTTER_ACTOR_CLASS (e_contact_marker_parent_class)->map (actor);

	clutter_actor_map (CLUTTER_ACTOR (marker->priv->content_group));
}

static void
contact_marker_unmap (ClutterActor *actor)
{
	EContactMarker *marker;

	marker = E_CONTACT_MARKER (actor);

	/* Chain up to parent's unmap() method. */
	CLUTTER_ACTOR_CLASS (e_contact_marker_parent_class)->unmap (actor);

	clutter_actor_unmap (CLUTTER_ACTOR (marker->priv->content_group));
}

static void
contact_marker_paint (ClutterActor *actor)
{
	EContactMarker *marker;

	marker = E_CONTACT_MARKER (actor);

	clutter_actor_paint (CLUTTER_ACTOR (marker->priv->content_group));
}

static void
contact_marker_pick (ClutterActor *actor,
                     const ClutterColor *color)
{
	EContactMarker *marker;
	gfloat width, height;

	if (!clutter_actor_should_pick_paint (actor))
		return;

	marker = E_CONTACT_MARKER (actor);

	width = marker->priv->total_width;
	height = marker->priv->total_height;

	cogl_path_new ();

	cogl_set_source_color4ub (
		color->red,
		color->green,
		color->blue,
		color->alpha);

	cogl_path_move_to (RADIUS, 0);
	cogl_path_line_to (width - RADIUS, 0);
	cogl_path_arc (width - RADIUS, RADIUS, RADIUS, RADIUS, -90, 0);
	cogl_path_line_to (width, height - RADIUS);
	cogl_path_arc (width - RADIUS, height - RADIUS, RADIUS, RADIUS, 0, 90);
	cogl_path_line_to (RADIUS, height);
	cogl_path_arc (RADIUS, height - RADIUS, RADIUS, RADIUS, 90, 180);
	cogl_path_line_to (0, RADIUS);
	cogl_path_arc (RADIUS, RADIUS, RADIUS, RADIUS, 180, 270);
	cogl_path_close ();
	cogl_path_fill ();
}

static void
contact_marker_allocate (ClutterActor *actor,
                         const ClutterActorBox *box,
                         ClutterAllocationFlags flags)
{
	EContactMarker *marker;
	ClutterActorBox child_box;

	marker = E_CONTACT_MARKER (actor);

	/* Chain up to parent's allocate() method. */
	CLUTTER_ACTOR_CLASS (e_contact_marker_parent_class)->
		allocate (actor, box, flags);

	child_box.x1 = 0;
	child_box.x2 = box->x2 - box->x1;
	child_box.y1 = 0;
	child_box.y2 = box->y2 - box->y1;

	clutter_actor_allocate (
		CLUTTER_ACTOR (marker->priv->content_group),
		&child_box, flags);
}

static void
e_contact_marker_class_init (EContactMarkerClass *class)
{
	GObjectClass *object_class;
	ClutterActorClass *actor_class;

	g_type_class_add_private (class, sizeof (EContactMarkerPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = contact_marker_dispose;
	object_class->finalize = contact_marker_finalize;

	actor_class = CLUTTER_ACTOR_CLASS (class);
	actor_class->map = contact_marker_map;
	actor_class->unmap = contact_marker_unmap;
	actor_class->paint = contact_marker_paint;
	actor_class->pick = contact_marker_pick;
	actor_class->allocate = contact_marker_allocate;

	signals[DOUBLE_CLICKED] = g_signal_new (
		"double-clicked",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EContactMarkerClass, double_clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_contact_marker_init (EContactMarker *marker)
{
	marker->priv = E_CONTACT_MARKER_GET_PRIVATE (marker);

	marker->priv->content_group = CLUTTER_GROUP (clutter_group_new ());

	clutter_actor_set_parent (
		CLUTTER_ACTOR (marker->priv->content_group),
		CLUTTER_ACTOR (marker));
	clutter_actor_queue_relayout (CLUTTER_ACTOR (marker));

	g_signal_connect (
		marker, "notify::selected",
		G_CALLBACK (notify_selected), NULL);
	g_signal_connect (
		marker, "button-release-event",
		G_CALLBACK (contact_marker_clicked_cb), NULL);
}

EContactMarker *
e_contact_marker_new (const gchar *name,
                      const gchar *contact_uid,
                      EContactPhoto *photo)
{
	EContactMarker *marker;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (contact_uid != NULL, NULL);

	marker = g_object_new (E_TYPE_CONTACT_MARKER, NULL);

	champlain_label_set_text (CHAMPLAIN_LABEL (marker), name);
	marker->priv->contact_uid = g_strdup (contact_uid);
	if (photo != NULL)
		marker->priv->image = contact_photo_to_texture (photo);

	queue_redraw (marker);

	return marker;
}

const gchar *
e_contact_marker_get_contact_uid (EContactMarker *marker)
{
	g_return_val_if_fail (E_IS_CONTACT_MARKER (marker), NULL);

	return marker->priv->contact_uid;
}

#endif /* WITH_CONTACT_MAPS */
