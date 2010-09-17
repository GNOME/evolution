#ifndef __GTK_COMPAT_H__
#define __GTK_COMPAT_H__

#include <gtk/gtk.h>

/* Provide a GTK+ compatibility layer. */

#if !GTK_CHECK_VERSION (2,21,8)

#define GDK_KEY_3270_BackTab	GDK_3270_BackTab
#define GDK_KEY_3270_Enter	GDK_3270_Enter
#define GDK_KEY_BackSpace	GDK_BackSpace
#define GDK_KEY_Caps_Lock	GDK_Caps_Lock
#define GDK_KEY_Clear		GDK_Clear
#define GDK_KEY_Delete		GDK_Delete
#define GDK_KEY_Down		GDK_Down
#define GDK_KEY_End		GDK_End
#define GDK_KEY_Escape		GDK_Escape
#define GDK_KEY_Home		GDK_Home
#define GDK_KEY_ISO_Enter	GDK_ISO_Enter
#define GDK_KEY_ISO_Left_Tab	GDK_ISO_Left_Tab
#define GDK_KEY_ISO_Lock	GDK_ISO_Lock
#define GDK_KEY_Insert		GDK_Insert
#define GDK_KEY_Left		GDK_Left
#define GDK_KEY_Page_Down	GDK_Page_Down
#define GDK_KEY_Page_Up		GDK_Page_Up
#define GDK_KEY_Return		GDK_Return
#define GDK_KEY_Right		GDK_Right
#define GDK_KEY_Scroll_Lock	GDK_Scroll_Lock
#define GDK_KEY_Shift_Lock	GDK_Shift_Lock
#define GDK_KEY_Sys_Req		GDK_Sys_Req
#define GDK_KEY_Tab		GDK_Tab
#define GDK_KEY_Up		GDK_Up
#define GDK_KEY_VoidSymbol	GDK_VoidSymbol
#define GDK_KEY_backslash	GDK_backslash
#define GDK_KEY_bracketleft	GDK_bracketleft
#define GDK_KEY_bracketright	GDK_bracketright
#define GDK_KEY_comma		GDK_comma
#define GDK_KEY_equal		GDK_equal
#define GDK_KEY_exclam		GDK_exclam
#define GDK_KEY_minus		GDK_minus
#define GDK_KEY_period		GDK_period
#define GDK_KEY_plus		GDK_plus
#define GDK_KEY_space		GDK_space
#define GDK_KEY_underscore	GDK_underscore

#define GDK_KEY_KP_0		GDK_KP_0
#define GDK_KEY_KP_1		GDK_KP_1
#define GDK_KEY_KP_2		GDK_KP_2
#define GDK_KEY_KP_3		GDK_KP_3
#define GDK_KEY_KP_4		GDK_KP_4
#define GDK_KEY_KP_5		GDK_KP_5
#define GDK_KEY_KP_6		GDK_KP_6
#define GDK_KEY_KP_7		GDK_KP_7
#define GDK_KEY_KP_8		GDK_KP_8
#define GDK_KEY_KP_9		GDK_KP_9
#define GDK_KEY_KP_Add		GDK_KP_Add
#define GDK_KEY_KP_Decimal	GDK_KP_Decimal
#define GDK_KEY_KP_Delete	GDK_KP_Delete
#define GDK_KEY_KP_Divide	GDK_KP_Divide
#define GDK_KEY_KP_Down		GDK_KP_Down
#define GDK_KEY_KP_End		GDK_KP_End
#define GDK_KEY_KP_Enter	GDK_KP_Enter
#define GDK_KEY_KP_Equal	GDK_KP_Equal
#define GDK_KEY_KP_Home		GDK_KP_Home
#define GDK_KEY_KP_Insert	GDK_KP_Insert
#define GDK_KEY_KP_Left		GDK_KP_Left
#define GDK_KEY_KP_Multiply	GDK_KP_Multiply
#define GDK_KEY_KP_Page_Down	GDK_KP_Page_Down
#define GDK_KEY_KP_Page_Up	GDK_KP_Page_Up
#define GDK_KEY_KP_Right	GDK_KP_Right
#define GDK_KEY_KP_Space	GDK_KP_Space
#define GDK_KEY_KP_Subtract	GDK_KP_Subtract
#define GDK_KEY_KP_Tab		GDK_KP_Tab
#define GDK_KEY_KP_Up		GDK_KP_Up

#define GDK_KEY_0		GDK_0
#define GDK_KEY_1		GDK_1
#define GDK_KEY_2		GDK_2
#define GDK_KEY_3		GDK_3
#define GDK_KEY_4		GDK_4
#define GDK_KEY_5		GDK_5
#define GDK_KEY_6		GDK_6
#define GDK_KEY_7		GDK_7
#define GDK_KEY_8		GDK_8
#define GDK_KEY_9		GDK_9
#define GDK_KEY_a		GDK_a
#define GDK_KEY_b		GDK_b
#define GDK_KEY_c		GDK_c
#define GDK_KEY_d		GDK_d
#define GDK_KEY_e		GDK_e
#define GDK_KEY_f		GDK_f
#define GDK_KEY_g		GDK_g
#define GDK_KEY_h		GDK_h
#define GDK_KEY_i		GDK_i
#define GDK_KEY_j		GDK_j
#define GDK_KEY_k		GDK_k
#define GDK_KEY_l		GDK_l
#define GDK_KEY_m		GDK_m
#define GDK_KEY_n		GDK_n
#define GDK_KEY_o		GDK_o
#define GDK_KEY_p		GDK_p
#define GDK_KEY_q		GDK_q
#define GDK_KEY_r		GDK_r
#define GDK_KEY_s		GDK_s
#define GDK_KEY_t		GDK_t
#define GDK_KEY_u		GDK_u
#define GDK_KEY_v		GDK_v
#define GDK_KEY_w		GDK_w
#define GDK_KEY_x		GDK_x
#define GDK_KEY_y		GDK_y
#define GDK_KEY_z		GDK_z
#define GDK_KEY_A		GDK_A
#define GDK_KEY_B		GDK_B
#define GDK_KEY_C		GDK_C
#define GDK_KEY_D		GDK_D
#define GDK_KEY_E		GDK_E
#define GDK_KEY_F		GDK_F
#define GDK_KEY_G		GDK_G
#define GDK_KEY_H		GDK_H
#define GDK_KEY_I		GDK_I
#define GDK_KEY_J		GDK_J
#define GDK_KEY_K		GDK_K
#define GDK_KEY_L		GDK_L
#define GDK_KEY_M		GDK_M
#define GDK_KEY_N		GDK_N
#define GDK_KEY_O		GDK_O
#define GDK_KEY_P		GDK_P
#define GDK_KEY_Q		GDK_Q
#define GDK_KEY_R		GDK_R
#define GDK_KEY_S		GDK_S
#define GDK_KEY_T		GDK_T
#define GDK_KEY_U		GDK_U
#define GDK_KEY_V		GDK_V
#define GDK_KEY_W		GDK_W
#define GDK_KEY_X		GDK_X
#define GDK_KEY_Y		GDK_Y
#define GDK_KEY_Z		GDK_Z

#define GDK_KEY_F10		GDK_F10
#define GDK_KEY_F14		GDK_F14
#define GDK_KEY_F16		GDK_F16
#define GDK_KEY_F18		GDK_F18
#define GDK_KEY_F20		GDK_F20

#define GDK_KEY_Alt_L		GDK_Alt_L
#define GDK_KEY_Alt_R		GDK_Alt_R

#define GDK_KEY_Control_L	GDK_Control_L
#define GDK_KEY_Control_R	GDK_Control_R

#define GDK_KEY_Hyper_L		GDK_Hyper_L
#define GDK_KEY_Hyper_R		GDK_Hyper_R

#define GDK_KEY_Meta_L		GDK_Meta_L
#define GDK_KEY_Meta_R		GDK_Meta_R

#define GDK_KEY_Shift_L		GDK_Shift_L
#define GDK_KEY_Shift_R		GDK_Shift_R

#define GDK_KEY_Super_L		GDK_Super_L
#define GDK_KEY_Super_R		GDK_Super_R

#endif

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
	gdk_drag_context_get_action (context)
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
