/*
 * Copyright 2000, Helix Code, Inc.
 */

#ifndef __PAS_BACKEND_LDAP_H__
#define __PAS_BACKEND_LDAP_H__

#include <libgnome/gnome-defs.h>
#include <pas-backend.h>

typedef struct _PASBackendLDAPPrivate PASBackendLDAPPrivate;

typedef struct {
	PASBackend             parent_object;
	PASBackendLDAPPrivate *priv;
} PASBackendLDAP;

typedef struct {
	PASBackendClass parent_class;
} PASBackendLDAPClass;

PASBackend *pas_backend_ldap_new      (void);
GtkType     pas_backend_ldap_get_type (void);

#define PAS_BACKEND_LDAP_TYPE        (pas_backend_ldap_get_type ())
#define PAS_BACKEND_LDAP(o)          (GTK_CHECK_CAST ((o), PAS_BACKEND_LDAP_TYPE, PASBackendLDAP))
#define PAS_BACKEND_LDAP_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), PAS_BACKEND_TYPE, PASBackendLDAPClass))
#define PAS_IS_BACKEND_LDAP(o)       (GTK_CHECK_TYPE ((o), PAS_BACKEND_LDAP_TYPE))
#define PAS_IS_BACKEND_LDAP_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), PAS_BACKEND_LDAP_TYPE))

#endif /* ! __PAS_BACKEND_LDAP_H__ */

