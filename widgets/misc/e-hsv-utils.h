/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_HSV_UTILS_H_
#define _E_HSV_UTILS_H_

#include <libgnome/gnome-defs.h>
#include <gdk/gdk.h>

BEGIN_GNOME_DECLS

void  e_hsv_to_rgb  (gdouble   h,
		     gdouble   s,
		     gdouble   v,
		     gdouble  *r,
		     gdouble  *g,
		     gdouble  *b);

void  e_rgb_to_hsv  (gdouble   r,
		     gdouble   g,
		     gdouble   b,
		     gdouble  *h,
		     gdouble  *s,
		     gdouble  *v);

void  e_hsv_tweak   (GdkColor *colour,
		     gdouble   delta_h,
		     gdouble   delta_s,
		     gdouble   delta_v);

END_GNOME_DECLS

#endif /* _E_HSV_UTILS_H_ */
