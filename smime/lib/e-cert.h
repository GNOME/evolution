/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright (C) 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
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

ECert*               e_cert_new          (CERTCertificate *cert);

const char*          e_cert_get_nickname (ECert *cert);
const char*          e_cert_get_email    (ECert *cert);
const char*          e_cert_get_org      (ECert *cert);
const char*          e_cert_get_cn       (ECert *cert);

gboolean             e_cert_is_ca_cert   (ECert *cert);
#endif /* _E_CERT_H_ */
