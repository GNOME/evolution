/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-corba-icon-utils.c
 *
 * Copyright (C) 2002  Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

#include "e-shell-corba-icon-utils.h"



/**
 * e_store_corba_icon_from_pixbuf:
 * @pixbuf: 
 * @icon_return: 
 * 
 * Store a CORBA Evolution::Icon in *@icon_return.  @icon_return is not
 * supposed to point to allocated memory, so all of its pointers are just
 * overwritten.
 **/
void
e_store_corba_icon_from_pixbuf (GdkPixbuf *pixbuf,
				GNOME_Evolution_Icon *icon_return)
{
	const char *sp;
	CORBA_octet *dp;
	int width, height, total_width, rowstride;
	int i, j;
	gboolean has_alpha;

	if (pixbuf == NULL) {
		icon_return->width = 0;
		icon_return->height = 0;
		icon_return->hasAlpha = FALSE;
		icon_return->rgbaData._length = 0;
		icon_return->rgbaData._maximum = 0;
		icon_return->rgbaData._buffer = NULL;
		return;
	}

	width     = gdk_pixbuf_get_width (pixbuf);
	height    = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);

	if (has_alpha)
		total_width = 4 * width;
	else
		total_width = 3 * width;

	icon_return->width = width;
	icon_return->height = height;
	icon_return->hasAlpha = has_alpha;

	icon_return->rgbaData._length = icon_return->height * total_width;
	icon_return->rgbaData._maximum = icon_return->rgbaData._length;
	icon_return->rgbaData._buffer = CORBA_sequence_CORBA_octet_allocbuf (icon_return->rgbaData._maximum);

	sp = gdk_pixbuf_get_pixels (pixbuf);
	dp = icon_return->rgbaData._buffer;
	for (i = 0; i < height; i ++) {
		for (j = 0; j < total_width; j++)
			*(dp ++) = sp[j];
		sp += rowstride;
	}

	CORBA_sequence_set_release (& icon_return->rgbaData, TRUE);
}

/**
 * e_new_corba_icon_from_pixbuf:
 * @pixbuf: 
 * 
 * Create a CORBA Evolution::Icon from the specified @pixbuf.
 * 
 * Return value: The new Evolution::Icon.
 **/
GNOME_Evolution_Icon *
e_new_corba_icon_from_pixbuf (GdkPixbuf *pixbuf)
{
	GNOME_Evolution_Icon *icon;

	icon = GNOME_Evolution_Icon__alloc ();
	e_store_corba_icon_from_pixbuf (pixbuf, icon);

	return icon;
}

/**
 * e_new_corba_animated_icon_from_pixbuf_array:
 * @pixbuf_array: 
 * 
 * Generate a CORBA Evolution::AnimatedIcon from a NULL-terminated
 * @pixbuf_array.
 * 
 * Return value: The new Evolution::AnimatedIcon.
 **/
GNOME_Evolution_AnimatedIcon *
e_new_corba_animated_icon_from_pixbuf_array (GdkPixbuf **pixbuf_array)
{
	GNOME_Evolution_AnimatedIcon *animated_icon;
	GdkPixbuf **p;
	int num_frames;
	int i;

	g_return_val_if_fail (pixbuf_array != NULL, NULL);

	num_frames = 0;
	for (p = pixbuf_array; *p != NULL; p++)
		num_frames++; 

	if (num_frames == 0)
		return NULL;

	animated_icon = GNOME_Evolution_AnimatedIcon__alloc ();

	animated_icon->_length = num_frames;
	animated_icon->_maximum = num_frames;
	animated_icon->_buffer = CORBA_sequence_GNOME_Evolution_Icon_allocbuf (animated_icon->_maximum);

	for (i = 0; i < num_frames; i++)
		e_store_corba_icon_from_pixbuf (pixbuf_array[i], & animated_icon->_buffer[i]);

	CORBA_sequence_set_release (animated_icon, TRUE);

	return animated_icon;
}


/**
 * e_new_gdk_pixbuf_from_corba_icon:
 * @icon: A CORBA Evolution::Icon.
 * @scaled_width: Width of the GdkPixbuf to obtain.
 * @scaled_height: Width of the GdkPixbuf to obtain.
 *
 * If @scaled_width or @scaled_height is -1, do not scale.
 * 
 * Create a GdkPixbuf for the specified CORBA @icon.
 * 
 * Return value: The newly created GdkPixbuf.
 **/
GdkPixbuf *
e_new_gdk_pixbuf_from_corba_icon  (const GNOME_Evolution_Icon *icon,
				   int scaled_width,
				   int scaled_height)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;
	unsigned char *p;
	int src_offset;
	int i, j;
	int rowstride;
	int total_width;

	g_return_val_if_fail (icon != NULL, NULL);

	if (scaled_width == -1)
		scaled_width = icon->width;

	if (scaled_height == -1)
		scaled_height = icon->height;

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, icon->hasAlpha, 8, icon->width, icon->height);

	if (icon->hasAlpha)
		total_width = 4 * icon->width;
	else
		total_width = 3 * icon->width;

	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	src_offset = 0;
	p = gdk_pixbuf_get_pixels (pixbuf);

	for (i = 0; i < icon->height; i++) {
		for (j = 0; j < total_width; j++)
			p[j] = icon->rgbaData._buffer[src_offset ++];
		p += rowstride;
	}

	if (icon->width == scaled_width && icon->height == scaled_height)
		return pixbuf;
		
	scaled_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, icon->hasAlpha, 8, scaled_width, scaled_height);
	gdk_pixbuf_scale (pixbuf, scaled_pixbuf,
			  0, 0, scaled_width, scaled_height,
			  0, 0, (double) scaled_width / icon->width, (double) scaled_height / icon->height,
			  GDK_INTERP_HYPER);

	gdk_pixbuf_unref (pixbuf);

	return scaled_pixbuf;
}

