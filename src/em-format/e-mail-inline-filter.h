/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Michael Zucchi <notzed@ximian.com>
 */

#ifndef E_MAIL_INLINE_FILTER_H
#define E_MAIL_INLINE_FILTER_H

#include <camel/camel.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_INLINE_FILTER \
	(e_mail_inline_filter_get_type ())
#define E_MAIL_INLINE_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_INLINE_FILTER, EMailInlineFilter))
#define E_MAIL_INLINE_FILTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_INLINE_FILTER, EMailInlineFilterClass))
#define E_IS_MAIL_INLINE_FILTER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_INLINE_FILTER))
#define E_IS_MAIL_INLINE_FILTER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_INLINE_FILTER))
#define E_MAIL_INLINE_FILTER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_INLINE_FILTER, EMailInlineFilterClass))

G_BEGIN_DECLS

typedef struct _EMailInlineFilter EMailInlineFilter;
typedef struct _EMailInlineFilterClass EMailInlineFilterClass;

struct _EMailInlineFilter {
	CamelMimeFilter filter;

	gint state;

	CamelTransferEncoding base_encoding;
	CamelContentType *base_type;

	GByteArray *data;
	gchar *filename;
	GSList *parts;

	gboolean found_any;
};

struct _EMailInlineFilterClass {
	CamelMimeFilterClass filter_class;
};

GType		e_mail_inline_filter_get_type	(void);
EMailInlineFilter *
		e_mail_inline_filter_new	(CamelTransferEncoding base_encoding,
						 CamelContentType *type,
						 const gchar *filename);
CamelMultipart *e_mail_inline_filter_get_multipart
						(EMailInlineFilter *emif);
gboolean	e_mail_inline_filter_found_any	(EMailInlineFilter *emif);

G_END_DECLS

#endif /* E_MAIL_INLINE_FILTER_H */
