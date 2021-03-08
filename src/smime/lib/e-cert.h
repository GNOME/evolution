/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_CERT_H_
#define _E_CERT_H_

#include <glib-object.h>
#include <cert.h>

#define E_TYPE_CERT            (e_cert_get_type ())
#define E_CERT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CERT, ECert))
#define E_CERT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CERT, ECertClass))
#define E_IS_CERT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CERT))
#define E_IS_CERT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CERT))
#define E_CERT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_CERT, ECertClass))

typedef struct _ECert ECert;
typedef struct _ECertClass ECertClass;
typedef struct _ECertPrivate ECertPrivate;

typedef enum {
	E_CERT_CA,
	E_CERT_CONTACT,
	E_CERT_SITE,
	E_CERT_USER,
	E_CERT_UNKNOWN
} ECertType;

struct _ECert {
	GObject parent;

	ECertPrivate *priv;
};

struct _ECertClass {
	GObjectClass parent_class;

	/* Padding for future expansion */
	void (*_ecert_reserved0) (void);
	void (*_ecert_reserved1) (void);
	void (*_ecert_reserved2) (void);
	void (*_ecert_reserved3) (void);
	void (*_ecert_reserved4) (void);
};

GType                e_cert_get_type     (void);

ECert *               e_cert_new          (CERTCertificate *cert);
ECert *               e_cert_new_from_der (gchar *data, guint32 len);

CERTCertificate *     e_cert_get_internal_cert (ECert *cert);

gboolean             e_cert_get_raw_der       (ECert *cert, gchar **data, guint32 *len);
const gchar *          e_cert_get_nickname      (ECert *cert);
const gchar *          e_cert_get_email         (ECert *cert);
const gchar *          e_cert_get_org           (ECert *cert);
const gchar *          e_cert_get_org_unit      (ECert *cert);
const gchar *          e_cert_get_cn            (ECert *cert);
const gchar *          e_cert_get_subject_name  (ECert *cert);

const gchar *          e_cert_get_issuer_name     (ECert *cert);
const gchar *          e_cert_get_issuer_cn       (ECert *cert);
const gchar *          e_cert_get_issuer_org      (ECert *cert);
const gchar *          e_cert_get_issuer_org_unit (ECert *cert);

const gchar *          e_cert_get_issued_on       (ECert *cert);
const gchar *          e_cert_get_expires_on      (ECert *cert);
const gchar *	     e_cert_get_usage (ECert *cert);

const gchar *          e_cert_get_serial_number    (ECert *cert);
const gchar *          e_cert_get_sha256_fingerprint
						   (ECert *cert);
const gchar *          e_cert_get_sha1_fingerprint (ECert *cert);
const gchar *          e_cert_get_md5_fingerprint  (ECert *cert);

ECert *              e_cert_get_ca_cert     (ECert *ecert);

gboolean             e_cert_mark_for_deletion (ECert *cert);

ECertType            e_cert_get_cert_type (ECert *cert);

#endif /* _E_CERT_H_ */
