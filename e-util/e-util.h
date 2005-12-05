/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-util.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_UTIL_H_
#define _E_UTIL_H_

#include <sys/types.h>
#include <glib-object.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <e-util/e-util-marshal.h>

#define E_MAKE_TYPE(l,str,t,ci,i,parent) \
GType l##_get_type(void)\
{\
	static GType type = 0;				\
	if (!type){					\
		static GTypeInfo const object_info = {	\
			sizeof (t##Class),		\
							\
			(GBaseInitFunc) NULL,		\
			(GBaseFinalizeFunc) NULL,	\
							\
			(GClassInitFunc) ci,		\
			(GClassFinalizeFunc) NULL,	\
			NULL,	/* class_data */	\
							\
			sizeof (t),			\
			0,	/* n_preallocs */	\
			(GInstanceInitFunc) i,		\
		};					\
		type = g_type_register_static (parent, str, &object_info, 0);	\
	}						\
	return type;					\
}


#define E_MAKE_X_TYPE(l,str,t,ci,i,parent,poa_init,offset)	\
GtkType l##_get_type(void)					\
{								\
	static GtkType type = 0;				\
	if (!type){						\
		GTypeInfo info = {				\
			sizeof (t##Class),			\
								\
			(GBaseInitFunc) NULL,			\
			(GBaseFinalizeFunc) NULL,		\
								\
			(GClassInitFunc) ci,			\
			(GClassFinalizeFunc) NULL,		\
								\
                        NULL, 	/* class_data */		\
								\
			sizeof (t),				\
			0, 	/* n_preallocs */		\
			(GInstanceInitFunc) i,			\
		};						\
                type = bonobo_x_type_unique (			\
			parent, poa_init, NULL,			\
			offset, &info, str);			\
	}							\
	return type;						\
}

#define GET_STRING_ARRAY_FROM_ELLIPSIS(labels, first_string) \
        { \
		va_list args; \
		int i; \
		char *s; \
 \
		va_start (args, (first_string)); \
 \
		i = 0; \
		for (s = (first_string); s; s = va_arg (args, char *)) \
			i++; \
		va_end (args); \
 \
		(labels) = g_new (char *, i + 1); \
 \
		va_start (args, (first_string)); \
		i = 0; \
		for (s = (first_string); s; s = va_arg (args, char *)) \
		        (labels)[i++] = s; \
 \
		va_end (args); \
		(labels)[i] = NULL; \
	}


#define GET_DUPLICATED_STRING_ARRAY_FROM_ELLIPSIS(labels, first_string) \
        { \
                int i; \
                GET_STRING_ARRAY_FROM_ELLIPSIS ((labels), (first_string)); \
                for (i = 0; labels[i]; i++) \
			labels[i] = g_strdup (labels[i]); \
        }


#if 0
#  define E_OBJECT_CLASS_ADD_SIGNALS(oc,sigs,last) \
	gtk_object_class_add_signals (oc, sigs, last)
#  define E_OBJECT_CLASS_TYPE(oc) (oc)->type
#else
#  define E_OBJECT_CLASS_ADD_SIGNALS(oc,sigs,last)
#  define E_OBJECT_CLASS_TYPE(oc) G_TYPE_FROM_CLASS (oc)
#endif


typedef enum {
	E_FOCUS_NONE,
	E_FOCUS_CURRENT,
	E_FOCUS_START,
	E_FOCUS_END
} EFocus;
int       e_str_compare                                                    (const void        *x,
									    const void        *y);
int       e_str_case_compare                                               (const void        *x,
									    const void        *y);
int       e_collate_compare                                                (const void        *x,
									    const void        *y);
int       e_int_compare                                                    (const void        *x,
									    const void        *y);
char     *e_strdup_strip                                                   (const char        *string);
void      e_free_object_list                                               (GList             *list);
void      e_free_object_slist                                              (GSList            *list);
void      e_free_string_list                                               (GList             *list);
void      e_free_string_slist                                              (GSList            *list);
char     *e_read_file                                                      (const char        *filename);
int       e_write_file                                                     (const char        *filename,
									    const char        *data,
									    int                flags);
int       e_write_file_mkstemp                                             (char              *filename,
									    const char        *data);
int       e_mkdir_hier                                                     (const char        *path,
									    mode_t             mode);

gchar   **e_strsplit                                                	   (const gchar      *string,
								    	    const gchar      *delimiter,
								    	    gint              max_tokens);
gchar    *e_strstrcase                                                     (const gchar       *haystack,
									    const gchar       *needle);
/* This only makes a filename safe for usage as a filename.  It still may have shell meta-characters in it. */
void      e_filename_make_safe                                             (gchar             *string);
gchar    *e_format_number                                                  (gint               number);
gchar    *e_format_number_float                                            (gfloat             number);
gboolean  e_create_directory                                               (gchar             *directory);
gchar   **e_strdupv                                                        (const gchar      **str_array);


typedef int (*ESortCompareFunc) (const void *first,
				 const void *second,
				 gpointer    closure);
void     e_sort                (void             *base,
				size_t            nmemb,
				size_t            size,
				ESortCompareFunc  compare,
				gpointer          closure);
void     e_bsearch             (const void       *key,
				const void       *base,
				size_t            nmemb,
				size_t            size,
				ESortCompareFunc  compare,
				gpointer          closure,
				size_t           *start,
				size_t           *end);
size_t   e_strftime_fix_am_pm  (char             *s,
				size_t            max,
				const char       *fmt,
				const struct tm  *tm);

size_t   e_strftime		(char              *s,
				 size_t             max,
				 const char        *fmt,
				 const struct tm   *tm);

size_t   e_utf8_strftime_fix_am_pm  (char             *s,
				     size_t            max,
				     const char       *fmt,
				     const struct tm  *tm);

size_t   e_utf8_strftime	(char              *s,
				 size_t             max,
				 const char        *fmt,
				 const struct tm   *tm);

/* String to/from double conversion functions */
gdouble   e_flexible_strtod     (const gchar       *nptr,
				 gchar            **endptr);

/* 29 bytes should enough for all possible values that
 * g_ascii_dtostr can produce with the %.17g format.
 * Then add 10 for good measure */
#define E_ASCII_DTOSTR_BUF_SIZE (DBL_DIG + 12 + 10)
gchar    *e_ascii_dtostr                                                   (gchar             *buffer,
									    gint               buf_len,
									    const gchar       *format,
									    gdouble            d);

/* Alternating char * and int arguments with a NULL char * to end.
   Less than 0 for the int means copy the whole string. */
gchar    *e_strdup_append_strings                                          (gchar             *first_string,
									    ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_UTIL_H_ */
