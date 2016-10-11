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
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifndef _E_PKCS12_H_
#define _E_PKCS12_H_

#include <glib-object.h>

#define E_TYPE_PKCS12            (e_pkcs12_get_type ())
#define E_PKCS12(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_PKCS12, EPKCS12))
#define E_PKCS12_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_PKCS12, EPKCS12Class))
#define E_IS_PKCS12(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_PKCS12))
#define E_IS_PKCS12_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_PKCS12))
#define E_PKCS12_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), E_TYPE_PKCS12, EPKCS12Class))

typedef struct _EPKCS12 EPKCS12;
typedef struct _EPKCS12Class EPKCS12Class;
typedef struct _EPKCS12Private EPKCS12Private;

struct _EPKCS12 {
	GObject parent;

	EPKCS12Private *priv;
};

struct _EPKCS12Class {
	GObjectClass parent_class;

	/* Padding for future expansion */
	void (*_epkcs12_reserved0) (void);
	void (*_epkcs12_reserved1) (void);
	void (*_epkcs12_reserved2) (void);
	void (*_epkcs12_reserved3) (void);
	void (*_epkcs12_reserved4) (void);
};

#define E_PKCS12_ERROR e_pkcs12_error_quark ()
GQuark e_pkcs12_error_quark (void) G_GNUC_CONST;

typedef enum
{
	E_PKCS12_ERROR_CANCELED,
	E_PKCS12_ERROR_NSS_FAILED,
	E_PKCS12_ERROR_FAILED
} EPKCS12Errors;

GType                e_pkcs12_get_type     (void);

EPKCS12 *             e_pkcs12_new (void);

#if 0
/* XXX we're not going to support additional slots in the initial ssl
 * stuff, so we just always default to the internal token (and thus
 * don't need this function yet. */
gboolean             e_pkcs12_set_token    (void);
#endif

gboolean             e_pkcs12_import_from_file (EPKCS12 *pkcs12, const gchar *path, GError **error);
gboolean             e_pkcs12_export_to_file   (GList *certs,
                                                GFile *file,
                                                const gchar *pwd,
                                                gboolean save_chain,
                                                GError **error);

#endif /* _E_CERT_H_ */
