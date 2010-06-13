privsolib_LTLIBRARIES = libart_lgpl_2.la

if OS_WIN32
else
libm = -lm
endif

libart_lgpl_2_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir) \
	$(GNOME_PLATFORM_CFLAGS)

libart_lgpl_2_la_SOURCES = \
	art_affine.c \
	art_affine.h \
	art_alphagamma.c \
	art_alphagamma.h \
	art_bpath.c \
	art_bpath.h \
	art_filterlevel.h \
	art_gray_svp.c \
	art_gray_svp.h \
	art_misc.c \
	art_misc.h \
	art_pathcode.h \
	art_pixbuf.c \
	art_pixbuf.h \
	art_point.h \
	art_rect.c \
	art_rect.h \
	art_rect_svp.c \
	art_rect_svp.h \
	art_rect_uta.c \
	art_rect_uta.h \
	art_render.c \
	art_render.h \
	art_render_gradient.c \
	art_render_gradient.h \
	art_render_mask.c \
	art_render_mask.h \
	art_render_svp.c \
	art_render_svp.h \
	art_rgb.c \
	art_rgb.h \
	art_rgb_a_affine.c \
	art_rgb_a_affine.h \
	art_rgb_affine.c \
	art_rgb_affine.h \
	art_rgb_affine_private.c \
	art_rgb_affine_private.h \
	art_rgb_bitmap_affine.c \
	art_rgb_bitmap_affine.h \
	art_rgb_pixbuf_affine.c \
	art_rgb_pixbuf_affine.h \
	art_rgb_rgba_affine.c \
	art_rgb_rgba_affine.h \
	art_rgb_svp.c \
	art_rgb_svp.h \
	art_rgba.c \
	art_rgba.h \
	art_svp.c \
	art_svp.h \
	art_svp_intersect.c \
	art_svp_intersect.h \
	art_svp_ops.c \
	art_svp_ops.h \
	art_svp_point.c \
	art_svp_point.h \
	art_svp_render_aa.c \
	art_svp_render_aa.h \
	art_svp_vpath.c \
	art_svp_vpath.h \
	art_svp_vpath_stroke.c \
	art_svp_vpath_stroke.h \
	art_svp_wind.c \
	art_svp_wind.h \
	art_uta.c \
	art_uta.h \
	art_uta_ops.c \
	art_uta_ops.h \
	art_uta_rect.c \
	art_uta_rect.h \
	art_uta_svp.c \
	art_uta_svp.h \
	art_uta_vpath.c \
	art_uta_vpath.h \
	art_vpath.c \
	art_vpath.h \
	art_vpath_bpath.c \
	art_vpath_bpath.h \
	art_vpath_dash.c \
	art_vpath_dash.h \
	art_vpath_svp.c \
	art_vpath_svp.h \
	libart.h

libart_lgpl_2_la_LDFLAGS = $(NO_UNDEFINED)

libart_lgpl_2_la_LIBADD = \
	$(GNOME_PLATFORM_LIBS) \
	$(libm)

-include $(top_srcdir)/git.mk
