/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_FILTER_FILE_H
#define E_FILTER_FILE_H

#include <e-util/e-filter-element.h>

/* Standard GObject macros */
#define E_TYPE_FILTER_FILE \
	(e_filter_file_get_type ())
#define E_FILTER_FILE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_FILTER_FILE, EFilterFile))
#define E_FILTER_FILE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_FILTER_FILE, EFilterFileClass))
#define E_IS_FILTER_FILE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_FILTER_FILE))
#define E_IS_FILTER_FILE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_FILTER_FILE))
#define E_FILTER_FILE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_FILTER_FILE, EFilterFileClass))

G_BEGIN_DECLS

typedef struct _EFilterFile EFilterFile;
typedef struct _EFilterFileClass EFilterFileClass;
typedef struct _EFilterFilePrivate EFilterFilePrivate;

struct _EFilterFile {
	EFilterElement parent;
	EFilterFilePrivate *priv;

	gchar *type;
	gchar *path;
};

struct _EFilterFileClass {
	EFilterElementClass parent_class;
};

GType		e_filter_file_get_type		(void) G_GNUC_CONST;
EFilterFile *	e_filter_file_new		(void);
EFilterFile *	e_filter_file_new_type_name	(const gchar *type);
void		e_filter_file_set_path		(EFilterFile *file,
						 const gchar *path);

G_END_DECLS

#endif /* E_FILTER_FILE_H */
