#ifndef _E_UTIL_H_
#define _E_UTIL_H_

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
}\


typedef enum {
  E_FOCUS_NONE,
  E_FOCUS_CURRENT,
  E_FOCUS_START,
  E_FOCUS_END
} EFocus;

int g_str_compare(const void *x, const void *y);
int g_int_compare(const void *x, const void *y);

#endif /* _E_UTIL_H_ */
