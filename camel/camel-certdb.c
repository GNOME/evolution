/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2003 Ximian, Inc. (www.ximian.com)
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "camel-certdb.h"
#include "camel-private.h"

#include <camel/camel-file-utils.h>

#include <libedataserver/e-memory.h>


#define CAMEL_CERTDB_GET_CLASS(db)  ((CamelCertDBClass *) CAMEL_OBJECT_GET_CLASS (db))

#define CAMEL_CERTDB_VERSION  0x100

static void camel_certdb_class_init (CamelCertDBClass *klass);
static void camel_certdb_init       (CamelCertDB *certdb);
static void camel_certdb_finalize   (CamelObject *obj);

static int certdb_header_load (CamelCertDB *certdb, FILE *istream);
static int certdb_header_save (CamelCertDB *certdb, FILE *ostream);
static CamelCert *certdb_cert_load (CamelCertDB *certdb, FILE *istream);
static int certdb_cert_save (CamelCertDB *certdb, CamelCert *cert, FILE *ostream);
static CamelCert *certdb_cert_new (CamelCertDB *certdb);
static void certdb_cert_free (CamelCertDB *certdb, CamelCert *cert);

static const char *cert_get_string (CamelCertDB *certdb, CamelCert *cert, int string);
static void cert_set_string (CamelCertDB *certdb, CamelCert *cert, int string, const char *value);


static CamelObjectClass *parent_class = NULL;


CamelType
camel_certdb_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_object_get_type (),
					    "CamelCertDB",
					    sizeof (CamelCertDB),
					    sizeof (CamelCertDBClass),
					    (CamelObjectClassInitFunc) camel_certdb_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_certdb_init,
					    (CamelObjectFinalizeFunc) camel_certdb_finalize);
	}
	
	return type;
}


static void
camel_certdb_class_init (CamelCertDBClass *klass)
{
	parent_class = camel_type_get_global_classfuncs (camel_object_get_type ());
	
	klass->header_load = certdb_header_load;
	klass->header_save = certdb_header_save;
	
	klass->cert_new  = certdb_cert_new;
	klass->cert_load = certdb_cert_load;
	klass->cert_save = certdb_cert_save;
	klass->cert_free = certdb_cert_free;
	klass->cert_get_string = cert_get_string;
	klass->cert_set_string = cert_set_string;
}

static void
camel_certdb_init (CamelCertDB *certdb)
{
	certdb->priv = g_malloc (sizeof (struct _CamelCertDBPrivate));
	
	certdb->filename = NULL;
	certdb->version = CAMEL_CERTDB_VERSION;
	certdb->saved_certs = 0;
	
	certdb->cert_size = sizeof (CamelCert);
	
	certdb->cert_chunks = NULL;
	
	certdb->certs = g_ptr_array_new ();
	certdb->cert_hash = g_hash_table_new (g_str_hash, g_str_equal);
	
	certdb->priv->db_lock = g_mutex_new ();
	certdb->priv->io_lock = g_mutex_new ();
	certdb->priv->alloc_lock = g_mutex_new ();
	certdb->priv->ref_lock = g_mutex_new ();
}

static void
camel_certdb_finalize (CamelObject *obj)
{
	CamelCertDB *certdb = (CamelCertDB *) obj;
	struct _CamelCertDBPrivate *p;
	
	p = certdb->priv;
	
	if (certdb->flags & CAMEL_CERTDB_DIRTY)
		camel_certdb_save (certdb);
	
	camel_certdb_clear (certdb);
	g_ptr_array_free (certdb->certs, TRUE);
	g_hash_table_destroy (certdb->cert_hash);
	
	g_free (certdb->filename);
	
	if (certdb->cert_chunks)
		e_memchunk_destroy (certdb->cert_chunks);
	
	g_mutex_free (p->db_lock);
	g_mutex_free (p->io_lock);
	g_mutex_free (p->alloc_lock);
	g_mutex_free (p->ref_lock);
	
	g_free (p);
}


CamelCertDB *
camel_certdb_new (void)
{
	return (CamelCertDB *) camel_object_new (camel_certdb_get_type ());
}


static CamelCertDB *default_certdb = NULL;
static pthread_mutex_t default_certdb_lock = PTHREAD_MUTEX_INITIALIZER;


void
camel_certdb_set_default (CamelCertDB *certdb)
{
	pthread_mutex_lock (&default_certdb_lock);
	
	if (default_certdb)
		camel_object_unref (default_certdb);
	
	if (certdb)
		camel_object_ref (certdb);
	
	default_certdb = certdb;
	
	pthread_mutex_unlock (&default_certdb_lock);
}


CamelCertDB *
camel_certdb_get_default (void)
{
	CamelCertDB *certdb;
	
	pthread_mutex_lock (&default_certdb_lock);
	
	if (default_certdb)
		camel_object_ref (default_certdb);
	
	certdb = default_certdb;
	
	pthread_mutex_unlock (&default_certdb_lock);
	
	return certdb;
}


void
camel_certdb_set_filename (CamelCertDB *certdb, const char *filename)
{
	g_return_if_fail (CAMEL_IS_CERTDB (certdb));
	g_return_if_fail (filename != NULL);
	
	CAMEL_CERTDB_LOCK (certdb, db_lock);
	
	g_free (certdb->filename);
	certdb->filename = g_strdup (filename);
	
	CAMEL_CERTDB_UNLOCK (certdb, db_lock);
}


static int
certdb_header_load (CamelCertDB *certdb, FILE *istream)
{
	if (camel_file_util_decode_uint32 (istream, &certdb->version) == -1)
		return -1;
	if (camel_file_util_decode_uint32 (istream, &certdb->saved_certs) == -1)
		return -1;
	
	return 0;
}

static CamelCert *
certdb_cert_load (CamelCertDB *certdb, FILE *istream)
{
	CamelCert *cert;
	
	cert = camel_certdb_cert_new (certdb);

	if (camel_file_util_decode_string (istream, &cert->issuer) == -1)
		goto error;
	if (camel_file_util_decode_string (istream, &cert->subject) == -1)
		goto error;
	if (camel_file_util_decode_string (istream, &cert->hostname) == -1)
		goto error;
	if (camel_file_util_decode_string (istream, &cert->fingerprint) == -1)
		goto error;
	if (camel_file_util_decode_uint32 (istream, &cert->trust) == -1)
		goto error;
	
	return cert;
	
 error:
	
	camel_certdb_cert_unref (certdb, cert);
	
	return NULL;
}

int
camel_certdb_load (CamelCertDB *certdb)
{
	CamelCert *cert;
	FILE *in;
	int i;
	
	g_return_val_if_fail (CAMEL_IS_CERTDB (certdb), -1);
	g_return_val_if_fail (certdb->filename, -1);

	in = fopen (certdb->filename, "r");
	if (in == NULL)
		return -1;
	
	CAMEL_CERTDB_LOCK (certdb, io_lock);
	if (CAMEL_CERTDB_GET_CLASS (certdb)->header_load (certdb, in) == -1)
		goto error;
	
	for (i = 0; i < certdb->saved_certs; i++) {
		cert = CAMEL_CERTDB_GET_CLASS (certdb)->cert_load (certdb, in);
		
		if (cert == NULL)
			goto error;
		
		camel_certdb_add (certdb, cert);
	}
	
	CAMEL_CERTDB_UNLOCK (certdb, io_lock);
	
	if (fclose (in) != 0)
		return -1;
	
	certdb->flags &= ~CAMEL_CERTDB_DIRTY;
	
	return 0;
	
 error:
	
	g_warning ("Cannot load certificate database: %s", strerror (ferror (in)));
	
	CAMEL_CERTDB_UNLOCK (certdb, io_lock);
	
	fclose (in);
	
	return -1;
}

static int
certdb_header_save (CamelCertDB *certdb, FILE *ostream)
{
	if (camel_file_util_encode_uint32 (ostream, certdb->version) == -1)
		return -1;
	if (camel_file_util_encode_uint32 (ostream, certdb->saved_certs) == -1)
		return -1;
	
	return 0;
}

static int
certdb_cert_save (CamelCertDB *certdb, CamelCert *cert, FILE *ostream)
{
	if (camel_file_util_encode_string (ostream, cert->issuer) == -1)
		return -1;
	if (camel_file_util_encode_string (ostream, cert->subject) == -1)
		return -1;
	if (camel_file_util_encode_string (ostream, cert->hostname) == -1)
		return -1;
	if (camel_file_util_encode_string (ostream, cert->fingerprint) == -1)
		return -1;
	if (camel_file_util_encode_uint32 (ostream, cert->trust) == -1)
		return -1;
	
	return 0;
}

int
camel_certdb_save (CamelCertDB *certdb)
{
	CamelCert *cert;
	char *filename;
	int fd, i;
	FILE *out;
	
	g_return_val_if_fail (CAMEL_IS_CERTDB (certdb), -1);
	g_return_val_if_fail (certdb->filename, -1);
	
	filename = alloca (strlen (certdb->filename) + 4);
	sprintf (filename, "%s~", certdb->filename);
	
	fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd == -1)
		return -1;
	
	out = fdopen (fd, "w");
	if (out == NULL) {
		i = errno;
		close (fd);
		unlink (filename);
		errno = i;
		return -1;
	}
	
	CAMEL_CERTDB_LOCK (certdb, io_lock);
	
	certdb->saved_certs = certdb->certs->len;
	if (CAMEL_CERTDB_GET_CLASS (certdb)->header_save (certdb, out) == -1)
		goto error;
	
	for (i = 0; i < certdb->saved_certs; i++) {
		cert = (CamelCert *) certdb->certs->pdata[i];
		
		if (CAMEL_CERTDB_GET_CLASS (certdb)->cert_save (certdb, cert, out) == -1)
			goto error;
	}
	
	CAMEL_CERTDB_UNLOCK (certdb, io_lock);
	
	if (fflush (out) != 0 || fsync (fileno (out)) == -1) {
		i = errno;
		fclose (out);
		unlink (filename);
		errno = i;
		return -1;
	}
	
	if (fclose (out) != 0) {
		i = errno;
		unlink (filename);
		errno = i;
		return -1;
	}
	
	if (rename (filename, certdb->filename) == -1) {
		i = errno;
		unlink (filename);
		errno = i;
		return -1;
	}
	
	certdb->flags &= ~CAMEL_CERTDB_DIRTY;
	
	return 0;
	
 error:
	
	g_warning ("Cannot save certificate database: %s", strerror (ferror (out)));
	
	CAMEL_CERTDB_UNLOCK (certdb, io_lock);
	
	i = errno;
	fclose (out);
	unlink (filename);
	errno = i;
	
	return -1;
}

void
camel_certdb_touch (CamelCertDB *certdb)
{
	g_return_if_fail (CAMEL_IS_CERTDB (certdb));
	
	certdb->flags |= CAMEL_CERTDB_DIRTY;
}

CamelCert *
camel_certdb_get_cert (CamelCertDB *certdb, const char *fingerprint)
{
	CamelCert *cert;
	
	g_return_val_if_fail (CAMEL_IS_CERTDB (certdb), NULL);
	
	CAMEL_CERTDB_LOCK (certdb, db_lock);
	
	cert = g_hash_table_lookup (certdb->cert_hash, fingerprint);
	if (cert)
		camel_certdb_cert_ref (certdb, cert);
	
	CAMEL_CERTDB_UNLOCK (certdb, db_lock);
	
	return cert;
}

void
camel_certdb_add (CamelCertDB *certdb, CamelCert *cert)
{
	g_return_if_fail (CAMEL_IS_CERTDB (certdb));
	
	CAMEL_CERTDB_LOCK (certdb, db_lock);
	
	if (g_hash_table_lookup (certdb->cert_hash, cert->fingerprint)) {
		CAMEL_CERTDB_UNLOCK (certdb, db_lock);
		return;
	}
	
	camel_certdb_cert_ref (certdb, cert);
	g_ptr_array_add (certdb->certs, cert);
	g_hash_table_insert (certdb->cert_hash, cert->fingerprint, cert);
	
	certdb->flags |= CAMEL_CERTDB_DIRTY;
	
	CAMEL_CERTDB_UNLOCK (certdb, db_lock);
}

void
camel_certdb_remove (CamelCertDB *certdb, CamelCert *cert)
{
	g_return_if_fail (CAMEL_IS_CERTDB (certdb));
	
	CAMEL_CERTDB_LOCK (certdb, db_lock);
	
	if (g_hash_table_lookup (certdb->cert_hash, cert->fingerprint)) {
		g_hash_table_remove (certdb->cert_hash, cert->fingerprint);
		g_ptr_array_remove (certdb->certs, cert);
		camel_certdb_cert_unref (certdb, cert);
		
		certdb->flags |= CAMEL_CERTDB_DIRTY;
	}
	
	CAMEL_CERTDB_UNLOCK (certdb, db_lock);
}

static CamelCert *
certdb_cert_new (CamelCertDB *certdb)
{
	CamelCert *cert;
	
	if (certdb->cert_chunks)
		cert = e_memchunk_alloc0 (certdb->cert_chunks);
	else
		cert = g_malloc0 (certdb->cert_size);
	
	cert->refcount = 1;
	
	return cert;
}

CamelCert *
camel_certdb_cert_new (CamelCertDB *certdb)
{
	CamelCert *cert;
	
	g_return_val_if_fail (CAMEL_IS_CERTDB (certdb), NULL);
	
	CAMEL_CERTDB_LOCK (certdb, alloc_lock);
	
	cert = CAMEL_CERTDB_GET_CLASS (certdb)->cert_new (certdb);
	
	CAMEL_CERTDB_UNLOCK (certdb, alloc_lock);
	
	return cert;
}

void
camel_certdb_cert_ref (CamelCertDB *certdb, CamelCert *cert)
{
	g_return_if_fail (CAMEL_IS_CERTDB (certdb));
	g_return_if_fail (cert != NULL);
	
	CAMEL_CERTDB_LOCK (certdb, ref_lock);
	cert->refcount++;
	CAMEL_CERTDB_UNLOCK (certdb, ref_lock);
}

static void
certdb_cert_free (CamelCertDB *certdb, CamelCert *cert)
{
	g_free (cert->issuer);
	g_free (cert->subject);
	g_free (cert->hostname);
	g_free (cert->fingerprint);
	if (cert->rawcert)
		g_byte_array_free(cert->rawcert, TRUE);
}

void
camel_certdb_cert_unref (CamelCertDB *certdb, CamelCert *cert)
{
	g_return_if_fail (CAMEL_IS_CERTDB (certdb));
	g_return_if_fail (cert != NULL);
	
	CAMEL_CERTDB_LOCK (certdb, ref_lock);
	
	if (cert->refcount <= 1) {
		CAMEL_CERTDB_GET_CLASS (certdb)->cert_free (certdb, cert);
		if (certdb->cert_chunks)
			e_memchunk_free (certdb->cert_chunks, cert);
		else
			g_free (cert);
	} else {
		cert->refcount--;
	}
	
	CAMEL_CERTDB_UNLOCK (certdb, ref_lock);
}


static gboolean
cert_remove (gpointer key, gpointer value, gpointer user_data)
{
	return TRUE;
}

void
camel_certdb_clear (CamelCertDB *certdb)
{
	CamelCert *cert;
	int i;
	
	g_return_if_fail (CAMEL_IS_CERTDB (certdb));
	
	CAMEL_CERTDB_LOCK (certdb, db_lock);
	
	g_hash_table_foreach_remove (certdb->cert_hash, cert_remove, NULL);
	for (i = 0; i < certdb->certs->len; i++) {
		cert = (CamelCert *) certdb->certs->pdata[i];
		camel_certdb_cert_unref (certdb, cert);
	}
	
	certdb->saved_certs = 0;
	g_ptr_array_set_size (certdb->certs, 0);
	certdb->flags |= CAMEL_CERTDB_DIRTY;
	
	CAMEL_CERTDB_UNLOCK (certdb, db_lock);
}


static const char *
cert_get_string (CamelCertDB *certdb, CamelCert *cert, int string)
{
	switch (string) {
	case CAMEL_CERT_STRING_ISSUER:
		return cert->issuer;
	case CAMEL_CERT_STRING_SUBJECT:
		return cert->subject;
	case CAMEL_CERT_STRING_HOSTNAME:
		return cert->hostname;
	case CAMEL_CERT_STRING_FINGERPRINT:
		return cert->fingerprint;
	default:
		return NULL;
	}
}


const char *
camel_cert_get_string (CamelCertDB *certdb, CamelCert *cert, int string)
{
	g_return_val_if_fail (CAMEL_IS_CERTDB (certdb), NULL);
	g_return_val_if_fail (cert != NULL, NULL);
	
	/* FIXME: do locking? */
	
	return CAMEL_CERTDB_GET_CLASS (certdb)->cert_get_string (certdb, cert, string);
}

static void
cert_set_string (CamelCertDB *certdb, CamelCert *cert, int string, const char *value)
{
	switch (string) {
	case CAMEL_CERT_STRING_ISSUER:
		g_free (cert->issuer);
		cert->issuer = g_strdup (value);
		break;
	case CAMEL_CERT_STRING_SUBJECT:
		g_free (cert->subject);
		cert->subject = g_strdup (value);
		break;
	case CAMEL_CERT_STRING_HOSTNAME:
		g_free (cert->hostname);
		cert->hostname = g_strdup (value);
		break;
	case CAMEL_CERT_STRING_FINGERPRINT:
		g_free (cert->fingerprint);
		cert->fingerprint = g_strdup (value);
		break;
	default:
		break;
	}
}


void
camel_cert_set_string (CamelCertDB *certdb, CamelCert *cert, int string, const char *value)
{
	g_return_if_fail (CAMEL_IS_CERTDB (certdb));
	g_return_if_fail (cert != NULL);
	
	/* FIXME: do locking? */
	
	CAMEL_CERTDB_GET_CLASS (certdb)->cert_set_string (certdb, cert, string, value);
}


CamelCertTrust
camel_cert_get_trust (CamelCertDB *certdb, CamelCert *cert)
{
	g_return_val_if_fail (CAMEL_IS_CERTDB (certdb), CAMEL_CERT_TRUST_UNKNOWN);
	g_return_val_if_fail (cert != NULL, CAMEL_CERT_TRUST_UNKNOWN);
	
	return cert->trust;
}


void
camel_cert_set_trust (CamelCertDB *certdb, CamelCert *cert, CamelCertTrust trust)
{
	g_return_if_fail (CAMEL_IS_CERTDB (certdb));
	g_return_if_fail (cert != NULL);
	
	cert->trust = trust;
}
