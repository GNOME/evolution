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

/* The following is the mozilla license blurb, as the bodies some of
   these functions were derived from the mozilla source. */

/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */

#include "e-cert.h"
#include "e-cert-trust.h"
#include "pk11func.h"
#include "certdb.h"

struct _ECertPrivate {
	CERTCertificate *cert;

	/* pointers we cache since the nss implementation allocs the
	   string */
	char *org_name;
	char *cn;

	gboolean delete;
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
		PORT_Free (ec->priv->cn);

	if (ec->priv->delete) {
		printf ("attempting to delete cert marked for deletion\n");
		if (e_cert_get_cert_type (ec) == E_CERT_USER) {
			PK11_DeleteTokenCertAndKey(ec->priv->cert, NULL);
		} else if (!PK11_IsReadOnly(ec->priv->cert->slot)) {
			/* If the list of built-ins does contain a non-removable
			   copy of this certificate, our call will not remove
			   the certificate permanently, but rather remove all trust. */
			SEC_DeletePermCertificate(ec->priv->cert);
		}
	}

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

ECert*
e_cert_new_from_der (char *data, guint32 len)
{
	CERTCertificate *cert = CERT_DecodeCertFromPackage (data, len);

	if (!cert)
		return NULL;

	if (cert->dbhandle == NULL)
		cert->dbhandle = CERT_GetDefaultCertDB();

	return e_cert_new (cert);
}




CERTCertificate*
e_cert_get_internal_cert (ECert *cert)
{
	/* XXX should this refcnt it? */
	return cert->priv->cert;
}

gboolean
e_cert_get_raw_der (ECert *cert, char **data, guint32 *len)
{
	/* XXX do we really need to check if cert->priv->cert is NULL
	   here?  it should always be non-null if we have the
	   ECert.. */
	if (cert->priv->cert) {
		*data = (char*)cert->priv->cert->derCert.data;
		*len = (guint32)cert->priv->cert->derCert.len;
		return TRUE;
	}

	*len = 0;
	return FALSE;

}

const char*
e_cert_get_nickname (ECert *cert)
{
	return cert->priv->cert->nickname;
}

const char*
e_cert_get_email    (ECert *cert)
{
	return cert->priv->cert->emailAddr;
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

const char*
e_cert_get_issuer_name (ECert *cert)
{
	return cert->priv->cert->issuerName;
}

const char*
e_cert_get_subject_name (ECert *cert)
{
	return cert->priv->cert->subjectName;
}

gboolean
e_cert_mark_for_deletion (ECert *cert)
{
	//	nsNSSShutDownPreventionLock locker;

#if 0
	// make sure user is logged in to the token
	nsCOMPtr<nsIInterfaceRequestor> ctx = new PipUIContext();
#endif

	if (PK11_NeedLogin(cert->priv->cert->slot)
	    && !PK11_NeedUserInit(cert->priv->cert->slot)
	    && !PK11_IsInternal(cert->priv->cert->slot)) {
		if (SECSuccess != PK11_Authenticate(cert->priv->cert->slot, PR_TRUE, NULL)) {
			return FALSE;
		}
	}

	cert->priv->delete = TRUE;

	return TRUE;
}

ECertType
e_cert_get_cert_type (ECert *ecert)
{
	const char *nick = e_cert_get_nickname (ecert);
	const char *email = e_cert_get_email (ecert);
	CERTCertificate *cert = ecert->priv->cert;

	if (nick) {
		if (e_cert_trust_has_any_user (cert->trust))
			return E_CERT_USER;
		if (e_cert_trust_has_any_ca (cert->trust)
		    || CERT_IsCACert(cert,NULL))
			return E_CERT_CA;
		if (e_cert_trust_has_peer (cert->trust, PR_TRUE, PR_FALSE, PR_FALSE))
			return E_CERT_SITE;
	}
	if (email && e_cert_trust_has_peer (cert->trust, PR_FALSE, PR_TRUE, PR_FALSE))
		return E_CERT_CONTACT;

	return E_CERT_UNKNOWN;
}
