/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Jon Trowbridge <trow@ximian.com>
 * SPDX-FileContributor: Jeffrey Stedfast <fejj@ximian.com>
 */

#ifndef EM_FILTER_SOURCE_ELEMENT_H
#define EM_FILTER_SOURCE_ELEMENT_H

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

/* Standard GObject macros */
#define EM_TYPE_FILTER_SOURCE_ELEMENT \
	(em_filter_source_element_get_type ())
#define EM_FILTER_SOURCE_ELEMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FILTER_SOURCE_ELEMENT, EMFilterSourceElement))
#define EM_FILTER_SOURCE_ELEMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FILTER_SOURCE_ELEMENT, EMFilterSourceElementClass))
#define EM_IS_FILTER_SOURCE_ELEMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FILTER_SOURCE_ELEMENT))
#define EM_IS_FILTER_SOURCE_ELEMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FILTER_SOURCE_ELEMENT))
#define EM_FILTER_SOURCE_ELEMENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FILTER_SOURCE_ELEMENT, EMFilterSourceElementClass))

G_BEGIN_DECLS

typedef struct _EMFilterSourceElement EMFilterSourceElement;
typedef struct _EMFilterSourceElementClass EMFilterSourceElementClass;
typedef struct _EMFilterSourceElementPrivate EMFilterSourceElementPrivate;

struct _EMFilterSourceElement {
	EFilterElement parent;
	EMFilterSourceElementPrivate *priv;
};

struct _EMFilterSourceElementClass {
	EFilterElementClass parent_class;
};

GType		em_filter_source_element_get_type
						(void) G_GNUC_CONST;
EFilterElement *em_filter_source_element_new	(EMailSession *session);
EMailSession *	em_filter_source_element_get_session
						(EMFilterSourceElement *element);

G_END_DECLS

#endif /* EM_FILTER_SOURCE_ELEMENT_H */
