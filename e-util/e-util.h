#ifndef _E_UTIL_H_
#define _E_UTIL_H_

#include <sys/types.h>
#include <gtk/gtktypeutils.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_MAKE_TYPE(l,str,t,ci,i,parent) \
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
                type = gtk_type_unique (parent, &info);\
	}\
	return type;\
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
void      e_free_string_list                                               (GList            *list);
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
void      e_marshal_INT__INT_INT_POINTER                                   (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_INT__INT_POINTER_INT_POINTER                           (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_NONE__OBJECT_DOUBLE_DOUBLE_BOOL                        (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_DOUBLE__OBJECT_DOUBLE_DOUBLE_BOOL                      (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_BOOL__OBJECT_DOUBLE_DOUBLE_BOOL                        (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);

#define e_marshal_NONE__INT_INT_POINTER_POINTER_UINT_UINT e_marshal_NONE__INT_INT_POINTER_POINTER_INT_INT
void      e_marshal_NONE__INT_INT_POINTER_POINTER_INT_INT                  (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);

#define e_marshal_NONE__INT_POINTER_INT_POINTER_POINTER_UINT_UINT e_marshal_NONE__INT_POINTER_INT_POINTER_POINTER_INT_INT
void      e_marshal_NONE__INT_POINTER_INT_POINTER_POINTER_INT_INT          (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);

#define e_marshal_NONE__INT_INT_POINTER_UINT e_marshal_NONE__INT_INT_POINTER_INT
void      e_marshal_NONE__INT_INT_POINTER_INT                              (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);

#define e_marshal_NONE__INT_POINTER_INT_POINTER_UINT e_marshal_NONE__INT_POINTER_INT_POINTER_INT
void      e_marshal_NONE__INT_POINTER_INT_POINTER_INT                      (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);

#define e_marshal_BOOL__INT_INT_POINTER_INT_INT_UINT e_marshal_BOOL__INT_INT_POINTER_INT_INT_INT
void      e_marshal_BOOL__INT_INT_POINTER_INT_INT_INT                      (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);

#define e_marshal_BOOL__INT_POINTER_INT_POINTER_INT_INT_UINT e_marshal_BOOL__INT_POINTER_INT_POINTER_INT_INT_INT
void      e_marshal_BOOL__INT_POINTER_INT_POINTER_INT_INT_INT              (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);

#define e_marshal_NONE__INT_INT_POINTER_INT_INT_POINTER_UINT_UINT e_marshal_NONE__INT_INT_POINTER_INT_INT_POINTER_INT_INT
void      e_marshal_NONE__INT_INT_POINTER_INT_INT_POINTER_INT_INT          (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);

#define e_marshal_NONE__INT_POINTER_INT_POINTER_INT_INT_POINTER_UINT_UINT e_marshal_NONE__INT_POINTER_INT_POINTER_INT_INT_POINTER_INT_INT
void      e_marshal_NONE__INT_POINTER_INT_POINTER_INT_INT_POINTER_INT_INT  (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_NONE__POINTER_POINTER_INT                              (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_NONE__INT_POINTER_INT_POINTER                          (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_INT__POINTER_POINTER                                   (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_INT__POINTER_POINTER_POINTER                           (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_INT__POINTER_POINTER_POINTER_POINTER                   (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_NONE__POINTER_INT_INT_INT                              (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);
void      e_marshal_INT__OBJECT_POINTER                                    (GtkObject        *object,
									    GtkSignalFunc     func,
									    gpointer          func_data,
									    GtkArg           *args);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_UTIL_H_ */
