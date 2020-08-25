/*
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

#ifndef _E_CERT_DB_H_
#define _E_CERT_DB_H_

#include <glib-object.h>
#include <gio/gio.h>
#include "e-cert.h"
#include <cert.h>

#define E_TYPE_CERT_DB            (e_cert_db_get_type ())
#define E_CERT_DB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CERT_DB, ECertDB))
#define E_CERT_DB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CERT_DB, ECertDBClass))
#define E_IS_CERT_DB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CERT_DB))
#define E_IS_CERT_DB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CERT_DB))
#define E_CERT_DB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_CERT_DB, ECertDBClass))

#define E_CERTDB_ERROR e_certdb_error_quark()
GQuark e_certdb_error_quark (void) G_GNUC_CONST;

typedef struct _ECertDB ECertDB;
typedef struct _ECertDBClass ECertDBClass;
typedef struct _ECertDBPrivate ECertDBPrivate;

struct _ECertDB {
	GObject parent;

	ECertDBPrivate *priv;
};

struct _ECertDBClass {
	GObjectClass parent_class;

	/* signals */
	gboolean (*pk11_passwd) (ECertDB *db, PK11SlotInfo *slot, gboolean retry, gchar **passwd);
	gboolean (*pk11_change_passwd) (ECertDB *db, gchar **orig_passwd, gchar **passwd);
	gboolean (*confirm_ca_cert_import) (ECertDB *db, ECert *cert, gboolean *trust_ssl, gboolean *trust_email, gboolean *trust_objsign);

	/* Padding for future expansion */
	void (*_ecert_reserved0) (void);
	void (*_ecert_reserved1) (void);
	void (*_ecert_reserved2) (void);
	void (*_ecert_reserved3) (void);
	void (*_ecert_reserved4) (void);
};

GType                e_cert_db_get_type     (void);

/* single instance */
ECertDB *             e_cert_db_peek         (void);

void                 e_cert_db_shutdown     (void);

/* deleting certificates */
gboolean             e_cert_db_delete_cert (ECertDB *certdb,
					    ECert   *cert);

/* importing certificates */
gboolean             e_cert_db_import_certs (ECertDB *certdb,
					     gchar *data, guint32 length,
					     ECertType cert_type,
					     GSList **imported_certs,
					     GError **error);

gboolean             e_cert_db_import_email_cert (ECertDB *certdb,
						  gchar *data, guint32 length,
						  GSList **imported_certs,
						  GError **error);

gboolean             e_cert_db_import_server_cert (ECertDB *certdb,
						   gchar *data, guint32 length,
						   GSList **imported_certs,
						   GError **error);

gboolean             e_cert_db_import_certs_from_file (ECertDB *cert_db,
						       const gchar *file_path,
						       ECertType cert_type,
						       GSList **imported_certs,
						       GError **error);

gboolean             e_cert_db_import_pkcs12_file (ECertDB *cert_db,
						   const gchar *file_path,
						   GError **error);

gboolean             e_cert_db_export_pkcs12_file (ECert *cert,
                                                   GFile *file,
                                                   const gchar *password,
                                                   gboolean save_chain,
                                                   GError **error);

gboolean             e_cert_db_login_to_slot      (ECertDB *cert_db,
						   PK11SlotInfo *slot);

gboolean	     e_cert_db_change_cert_trust  (CERTCertificate *cert,
						   CERTCertTrust *trust);

const gchar *	     e_cert_db_nss_error_to_string (gint errorcode);

#endif /* _E_CERT_DB_H_ */
