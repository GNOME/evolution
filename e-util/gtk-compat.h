#ifndef __GTK_COMPAT_H__
#define __GTK_COMPAT_H__

#include <gtk/gtk.h>

/* Provide a GTK+ compatibility layer. */

#if !GTK_CHECK_VERSION (2,91,0)  /* approximately; who cares at this point */

#define gtk_widget_get_preferred_size(widget, minimum_size, natural_size) \
	(gtk_widget_size_request ((widget), ((minimum_size))))

/* XXX Yes, the GtkScrollable interface is implemented by more than just
 *     GtkLayout, but it turns out GtkLayout is the only thing Evolution
 *     uses the GtkScrollable API for on the gtk3 branch. */
#define GtkScrollable				GtkLayout
#define GTK_SCROLLABLE				GTK_LAYOUT
#define gtk_scrollable_get_hadjustment		gtk_layout_get_hadjustment
#define gtk_scrollable_set_hadjustment		gtk_layout_set_hadjustment
#define gtk_scrollable_get_vadjustment		gtk_layout_get_vadjustment
#define gtk_scrollable_set_vadjustment		gtk_layout_set_vadjustment

#endif

#if !GTK_CHECK_VERSION (2,23,0)
#define gtk_combo_box_text_new			gtk_combo_box_new_text
#define gtk_combo_box_text_append_text		gtk_combo_box_append_text
#define gtk_combo_box_text_prepend_text		gtk_combo_box_prepend_text
#define gtk_combo_box_text_get_active_text	gtk_combo_box_get_active_text
#define GTK_COMBO_BOX_TEXT			GTK_COMBO_BOX
#define GTK_IS_COMBO_BOX_TEXT			GTK_IS_COMBO_BOX
#define GtkComboBoxText				GtkComboBox

/* The below can be used only once in sources */
#define ENSURE_GTK_COMBO_BOX_TEXT_TYPE						\
	GType gtk_combo_box_text_get_type (void);				\
	typedef struct _GtkComboBoxText GtkComboBoxText;			\
	typedef struct _GtkComboBoxTextClass GtkComboBoxTextClass;		\
										\
	struct _GtkComboBoxText {						\
		GtkComboBox parent;						\
	};									\
										\
	struct _GtkComboBoxTextClass {						\
		GtkComboBoxClass parent_class;					\
	};									\
										\
										\
	G_DEFINE_TYPE (GtkComboBoxText, gtk_combo_box_text, GTK_TYPE_COMBO_BOX)	\
										\
	static void gtk_combo_box_text_init (GtkComboBoxText *cbt) {}		\
	static void gtk_combo_box_text_class_init (GtkComboBoxTextClass *kl) {}

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

#define ENSURE_GTK_COMBO_BOX_ENTRY_TYPE						\
	GType gtk_combo_box_entry_get_type (void);				\
	typedef struct _GtkComboBoxEntry GtkComboBoxEntry;			\
	typedef struct _GtkComboBoxEntryClass GtkComboBoxEntryClass;		\
										\
	struct _GtkComboBoxEntry {						\
		GtkComboBoxText parent;						\
	};									\
										\
	struct _GtkComboBoxEntryClass {						\
		GtkComboBoxTextClass parent_class;				\
	};									\
										\
	G_DEFINE_TYPE (GtkComboBoxEntry, gtk_combo_box_entry, GTK_TYPE_COMBO_BOX_TEXT)\
										\
	static GObject *							\
	gtk_combo_box_entry_constructor (GType type, guint n_construct_properties, GObjectConstructParam *construct_properties) \
	{									\
		GObjectConstructParam *params = g_new0 (GObjectConstructParam, n_construct_properties + 1);\
		GValue val = {0};						\
		GObject *res;							\
		gint ii;							\
										\
		for (ii = 0; ii < n_construct_properties; ii++) {		\
			params[ii] = construct_properties[ii];			\
		}								\
										\
		g_value_init (&val, G_TYPE_BOOLEAN);				\
		g_value_set_boolean (&val, TRUE);				\
										\
		params[n_construct_properties].pspec = g_object_class_find_property (G_OBJECT_CLASS (gtk_combo_box_entry_parent_class), "has-entry");\
		params[n_construct_properties].value = &val;			\
										\
		res = G_OBJECT_CLASS (gtk_combo_box_entry_parent_class)->constructor (type, n_construct_properties + 1, params);\
										\
		g_free (params);						\
		return res;							\
	}									\
	static void gtk_combo_box_entry_init (GtkComboBoxEntry *cbt) {}		\
	static void gtk_combo_box_entry_class_init (GtkComboBoxEntryClass *kl)	\
	{									\
		GObjectClass *object_class = G_OBJECT_CLASS (kl);		\
		object_class->constructor = gtk_combo_box_entry_constructor;	\
	}
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
