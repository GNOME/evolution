/*
 * SPDX-FileCopyrightText: (C) 2021 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef EM_FILTER_MAIL_IDENTITY_ELEMENT_H
#define EM_FILTER_MAIL_IDENTITY_ELEMENT_H

#include <libedataserver/libedataserver.h>
#include <e-util/e-util.h>

/* Standard GObject macros */
#define EM_TYPE_FILTER_MAIL_IDENTITY_ELEMENT \
	(em_filter_mail_identity_element_get_type ())
#define EM_FILTER_MAIL_IDENTITY_ELEMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_FILTER_MAIL_IDENTITY_ELEMENT, EMFilterMailIdentityElement))
#define EM_FILTER_MAIL_IDENTITY_ELEMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_FILTER_MAIL_IDENTITY_ELEMENT, EMFilterMailIdentityElementClass))
#define EM_IS_FILTER_MAIL_IDENTITY_ELEMENT(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_FILTER_MAIL_IDENTITY_ELEMENT))
#define EM_IS_FILTER_MAIL_IDENTITY_ELEMENT_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_FILTER_MAIL_IDENTITY_ELEMENT))
#define EM_FILTER_MAIL_IDENTITY_ELEMENT_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_FILTER_MAIL_IDENTITY_ELEMENT, EMFilterMailIdentityElementClass))

G_BEGIN_DECLS

typedef struct _EMFilterMailIdentityElement EMFilterMailIdentityElement;
typedef struct _EMFilterMailIdentityElementClass EMFilterMailIdentityElementClass;
typedef struct _EMFilterMailIdentityElementPrivate EMFilterMailIdentityElementPrivate;

struct _EMFilterMailIdentityElement {
	EFilterElement parent;
	EMFilterMailIdentityElementPrivate *priv;
};

struct _EMFilterMailIdentityElementClass {
	EFilterElementClass parent_class;
};

GType		em_filter_mail_identity_element_get_type	(void) G_GNUC_CONST;
EFilterElement *em_filter_mail_identity_element_new		(ESourceRegistry *registry);
ESourceRegistry *
		em_filter_mail_identity_element_get_registry	(EMFilterMailIdentityElement *mail_identity);

G_END_DECLS

#endif /* EM_FILTER_MAIL_IDENTITY_ELEMENT_H */
