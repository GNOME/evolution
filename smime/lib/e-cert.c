/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-cert.c
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Chris Toshok (toshok@ximian.com)
 */

#include "e-cert.h"

struct _ECertPrivate {
	CERTCertificate *cert;
	char *org_name;
	char *cn;
};

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class;

static void
e_cert_dispose (GObject *object)
{
	ECert *ec = E_CERT (object);

	if (!ec->priv)
		return;

	if (ec->priv->org_name)
		PORT_Free (ec->priv->org_name);
	if (ec->priv->cn)
		PORT_Free (ec->priv->org_name);

	g_free (ec->priv);
	ec->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->dispose)
		G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
e_cert_class_init (ECertClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_ref (PARENT_TYPE);

	object_class->dispose = e_cert_dispose;
}

static void
e_cert_init (ECert *ec)
{
	ec->priv = g_new0 (ECertPrivate, 1);
}

GType
e_cert_get_type (void)
{
	static GType cert_type = 0;

	if (!cert_type) {
		static const GTypeInfo cert_info =  {
			sizeof (ECertClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_cert_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (ECert),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_cert_init,
		};

		cert_type = g_type_register_static (PARENT_TYPE, "ECert", &cert_info, 0);
	}

	return cert_type;
}



static void
e_cert_populate (ECert *cert)
{
	CERTCertificate *c = cert->priv->cert;
	cert->priv->org_name = CERT_GetOrgName (&c->subject);
	cert->priv->cn = CERT_GetCommonName (&c->subject);
}

ECert*
e_cert_new (CERTCertificate *cert)
{
	ECert *ecert = E_CERT (g_object_new (E_TYPE_CERT, NULL));

	ecert->priv->cert = cert;

	e_cert_populate (ecert);

	return ecert;
}




const char*
e_cert_get_nickname (ECert *cert)
{
	return cert->priv->cert->nickname;
}

const char*
e_cert_get_email    (ECert *cert)
{
}

const char*
e_cert_get_org      (ECert *cert)
{
	return cert->priv->org_name;
}

const char*
e_cert_get_cn       (ECert *cert)
{
	return cert->priv->cn;
}

gboolean
e_cert_is_ca_cert (ECert *cert)
{
	return CERT_IsCACert (cert->priv->cert, NULL);
}
