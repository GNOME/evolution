#ifndef __GTK_COMPAT_H__
#define __GTK_COMPAT_H__

#include <gtk/gtk.h>

/* Provide a GTK+ compatibility layer. */

#if !GTK_CHECK_VERSION (2,23,0)
#define gtk_combo_box_text_new			gtk_combo_box_new_text
#define gtk_combo_box_text_append_text		gtk_combo_box_append_text
#define gtk_combo_box_text_prepend_text		gtk_combo_box_prepend_text
#define gtk_combo_box_text_get_active_text	gtk_combo_box_get_active_text
#define GTK_COMBO_BOX_TEXT			GTK_COMBO_BOX
#define GtkComboBoxText				GtkComboBox

static inline gint
gdk_window_get_width (GdkWindow *window)
{
	gint width, height;

	gdk_drawable_get_size (GDK_DRAWABLE (window), &width, &height);

	return width;
}

static inline gint
gdk_window_get_height (GdkWindow *window)
{
	gint width, height;

	gdk_drawable_get_size (GDK_DRAWABLE (window), &width, &height);

	return height;
}
#endif

#if GTK_CHECK_VERSION (2,23,0)
#define GTK_COMBO_BOX_ENTRY			GTK_COMBO_BOX
#else
#define gtk_combo_box_set_entry_text_column \
	gtk_combo_box_entry_set_text_column
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
