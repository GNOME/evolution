#ifndef __GTK_COMPAT_H__
#define __GTK_COMPAT_H__

#include <gtk/gtk.h>

/* Provide a compatibility layer for accessor functions introduced
 * in GTK+ 2.21.1 which we need to build with sealed GDK.  That way it
 * is still possible to build with GTK+ 2.20. */

#if (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION < 21) \
	|| (GTK_MINOR_VERSION == 21 && GTK_MICRO_VERSION < 1)

#define gdk_drag_context_get_actions(context)		(context)->actions
#define gdk_drag_context_get_suggested_action(context)	(context)->suggested_action
#define gdk_drag_context_get_selected_action(context)	(context)->action
#define gdk_drag_context_list_targets(context)		(context)->targets
#define gdk_visual_get_depth(visual)			(visual)->depth

#define gtk_accessible_get_widget(accessible) \
	(GTK_ACCESSIBLE (accessible)->widget)
#endif

#if GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION == 21 && GTK_MICRO_VERSION == 1
#define gdk_drag_context_get_selected_action(context) \
	gdk_drag_context_get_action(context)
#endif

#if GTK_CHECK_VERSION (2,90,5)

/* Recreate GdkRegion until we drop GTK2 compatibility. */
#define GdkOverlapType cairo_region_overlap_t
#define GDK_OVERLAP_RECTANGLE_IN   CAIRO_REGION_OVERLAP_IN
#define GDK_OVERLAP_RECTANGLE_OUT  CAIRO_REGION_OVERLAP_OUT
#define GDK_OVERLAP_RECTANGLE_PART CAIRO_REGION_OVERLAP_PART

#define GdkRegion cairo_region_t

#define gdk_region_new() \
	(cairo_region_create ())

#define gdk_region_destroy(region) \
	(cairo_region_destroy (region))

#define gdk_region_intersect(source1, source2) \
	(cairo_region_intersect ((source1), (source2)))

#define gdk_region_rect_in(region, rectangle) \
	(cairo_region_contains_rectangle ((region), (rectangle)))

#define gdk_region_rectangle(rectangle) \
	(((rectangle)->width <= 0 || (rectangle)->height <= 0) ? \
	cairo_region_create () : cairo_region_create_rectangle (rectangle))

#define gdk_region_get_rectangles(region, rectangles, n_rectangles) \
	G_STMT_START { \
		GdkRectangle *__rects; \
		gint __i, __nrects; \
		\
		__nrects = cairo_region_num_rectangles (region); \
		__rects = g_new (GdkRectangle, __nrects); \
		\
		for (__i = 0; __i < __nrects; __i++) \
			cairo_region_get_rectangle ((region), __i, &__rects[__i]); \
		\
		*(n_rectangles) = __nrects; \
		*(rectangles) = __rects; \
	} G_STMT_END

#define gdk_region_union_with_rect(region, rect) \
	G_STMT_START { \
		if ((rect)->width > 0 && (rect)->height > 0) \
			cairo_region_union_rectangle ((region), (rect)); \
	} G_STMT_END

#endif

#endif /* __GTK_COMPAT_H__ */
