#ifndef _E_UTIL_H_
#define _E_UTIL_H_

#include <sys/types.h>
#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gal/util/e-marshal.h>

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


#define E_MAKE_X_TYPE(l,str,t,ci,i,parent,poa_init,offset) \
GtkType l##_get_type(void)\
{\
	static GtkType type = 0;\
	if (!type){\
		GtkTypeInfo info = {\
			str,\
			sizeof (t),\
			sizeof (t##Class),\
			(GtkClassInitFunc) ci,\
			(GtkObjectInitFunc) i,\
			NULL, /* reserved 1 */\
			NULL, /* reserved 2 */\
			(GtkClassInitFunc) NULL\
		};\
                type = bonobo_x_type_unique (\
			parent, poa_init, NULL,\
			offset, &info);\
	}\
	return type;\
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
int       g_str_compare                                                    (const void       *x,
									    const void       *y);
int       g_int_compare                                                    (const void       *x,
									    const void       *y);
char     *e_strdup_strip                                                   (const char       *string);

void      e_free_object_list                                               (GList            *list);
void      e_free_object_slist                                              (GSList           *list);
void      e_free_string_list                                               (GList            *list);
void      e_free_string_slist                                              (GSList           *list);

char     *e_read_file                                                      (const char       *filename);
int       e_write_file                                                     (const char       *filename,
									    const char       *data,
									    int               flags);
int       e_mkdir_hier                                                     (const char       *path,
									    mode_t            mode);

gchar   **e_strsplit                                                	   (const gchar      *string,
								    	    const gchar      *delimiter,
								    	    gint              max_tokens);
gchar    *e_strstrcase                                                     (const gchar      *haystack,
									    const gchar      *needle);
void      e_filename_make_safe                                             (gchar            *string);
gchar    *e_format_number                                                  (gint              number);
gchar    *e_format_number_float                                            (gfloat            number);
gboolean  e_create_directory                                               (gchar            *directory);


typedef int (*ESortCompareFunc) (const void *first,
				 const void *second,
				 gpointer    closure);
void      e_sort                                                           (void             *base,
									    size_t            nmemb,
									    size_t            size,
									    ESortCompareFunc  compare,
									    gpointer          closure);
void      e_bsearch                                                        (const void       *key,
									    const void       *base,
									    size_t            nmemb,
									    size_t            size,
									    ESortCompareFunc  compare,
									    gpointer          closure,
									    size_t           *start,
									    size_t           *end);
size_t    e_strftime_fix_am_pm                                             (char             *s,
									    size_t            max,
									    const char       *fmt,
									    const struct tm  *tm);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_UTIL_H_ */
