/*
 * Copyright 2000, Ximian, Inc.
 */

#ifndef __PAS_BACKEND_LDAP_H__
#define __PAS_BACKEND_LDAP_H__

#include "pas-backend.h"

#define PAS_TYPE_BACKEND_LDAP        (pas_backend_ldap_get_type ())
#define PAS_BACKEND_LDAP(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), PAS_TYPE_BACKEND_LDAP, PASBackendLDAP))
#define PAS_BACKEND_LDAP_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), PAS_BACKEND_TYPE, PASBackendLDAPClass))
#define PAS_IS_BACKEND_LDAP(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), PAS_TYPE_BACKEND_LDAP))
#define PAS_IS_BACKEND_LDAP_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), PAS_TYPE_BACKEND_LDAP))
#define PAS_BACKEND_LDAP_GET_CLASS(k) (G_TYPE_INSTANCE_GET_CLASS ((obj), PAS_TYPE_BACKEND_LDAP, PASBackendLDAPClass))

typedef struct _PASBackendLDAPPrivate PASBackendLDAPPrivate;

typedef struct {
	PASBackend             parent_object;
	PASBackendLDAPPrivate *priv;
} PASBackendLDAP;

typedef struct {
	PASBackendClass parent_class;
} PASBackendLDAPClass;

PASBackend *pas_backend_ldap_new      (void);
GType       pas_backend_ldap_get_type (void);

#endif /* ! __PAS_BACKEND_LDAP_H__ */

