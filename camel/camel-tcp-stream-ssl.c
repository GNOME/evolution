/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_NSS
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <nspr.h>
#include <prio.h>
#include <prerror.h>
#include <prerr.h>
#include "nss.h"    /* Don't use <> here or it will include the system nss.h instead */
#include <ssl.h>

#include "camel-tcp-stream-ssl.h"
#include "camel-session.h"

static CamelTcpStreamClass *parent_class = NULL;

/* Returns the class for a CamelTcpStreamSSL */
#define CTSS_CLASS(so) CAMEL_TCP_STREAM_SSL_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);

static int stream_connect    (CamelTcpStream *stream, struct hostent *host, int port);
static int stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data);
static int stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data);
static gpointer stream_get_socket (CamelTcpStream *stream);

struct _CamelTcpStreamSSLPrivate {
	PRFileDesc *sockfd;
	
	CamelService *service;
	char *expected_host;
};

static void
camel_tcp_stream_ssl_class_init (CamelTcpStreamSSLClass *camel_tcp_stream_ssl_class)
{
	CamelTcpStreamClass *camel_tcp_stream_class =
		CAMEL_TCP_STREAM_CLASS (camel_tcp_stream_ssl_class);
	CamelStreamClass *camel_stream_class =
		CAMEL_STREAM_CLASS (camel_tcp_stream_ssl_class);
	
	parent_class = CAMEL_TCP_STREAM_CLASS (camel_type_get_global_classfuncs (camel_tcp_stream_get_type ()));
	
	/* virtual method overload */
	camel_stream_class->read = stream_read;
	camel_stream_class->write = stream_write;
	camel_stream_class->flush = stream_flush;
	camel_stream_class->close = stream_close;
	
	camel_tcp_stream_class->connect = stream_connect;
	camel_tcp_stream_class->getsockopt = stream_getsockopt;
	camel_tcp_stream_class->setsockopt = stream_setsockopt;
	camel_tcp_stream_class->get_socket = stream_get_socket;
}

static void
camel_tcp_stream_ssl_init (gpointer object, gpointer klass)
{
	CamelTcpStreamSSL *stream = CAMEL_TCP_STREAM_SSL (object);
	
	stream->priv = g_new0 (struct _CamelTcpStreamSSLPrivate, 1);
}

static void
camel_tcp_stream_ssl_finalize (CamelObject *object)
{
	CamelTcpStreamSSL *stream = CAMEL_TCP_STREAM_SSL (object);
	
	if (stream->priv->sockfd != NULL)
		PR_Close (stream->priv->sockfd);
	
	g_free (stream->priv->expected_host);
	
	g_free (stream->priv);
}


CamelType
camel_tcp_stream_ssl_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_tcp_stream_get_type (),
					    "CamelTcpStreamSSL",
					    sizeof (CamelTcpStreamSSL),
					    sizeof (CamelTcpStreamSSLClass),
					    (CamelObjectClassInitFunc) camel_tcp_stream_ssl_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_tcp_stream_ssl_init,
					    (CamelObjectFinalizeFunc) camel_tcp_stream_ssl_finalize);
	}
	
	return type;
}

/**
 * camel_tcp_stream_ssl_new:
 * @service: camel service
 * @expected_host: host that the stream is expected to connect with.
 *
 * Since the SSL certificate authenticator may need to prompt the
 * user, a CamelService is needed. @expected_host is needed as a
 * protection against an MITM attack.
 *
 * Return value: a tcp stream
 **/
CamelStream *
camel_tcp_stream_ssl_new (CamelService *service, const char *expected_host)
{
	CamelTcpStreamSSL *stream;
	
	stream = CAMEL_TCP_STREAM_SSL (camel_object_new (camel_tcp_stream_ssl_get_type ()));
	
	stream->priv->service = service;
	stream->priv->expected_host = g_strdup (expected_host);
	
	return CAMEL_STREAM (stream);
}

static void
set_errno (int code)
{
	/* FIXME: this should handle more. */
	switch (code) {
	case PR_IO_TIMEOUT_ERROR:
		errno = EAGAIN;
		break;
	case PR_IO_ERROR:
		errno = EIO;
		break;
	default:
		/* what to set by default?? */
		errno = EINTR;
	}
}

static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelTcpStreamSSL *tcp_stream_ssl = CAMEL_TCP_STREAM_SSL (stream);
	ssize_t nread;
	
	do {
		nread = PR_Read (tcp_stream_ssl->priv->sockfd, buffer, n);
	} while (nread == -1 && PR_GetError () == PR_PENDING_INTERRUPT_ERROR);
	
	if (nread == -1)
		set_errno (PR_GetError ());
	
	return nread;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelTcpStreamSSL *tcp_stream_ssl = CAMEL_TCP_STREAM_SSL (stream);
	ssize_t written = 0;
	
	do {
		written = PR_Write (tcp_stream_ssl->priv->sockfd, buffer, n);
	} while (written == -1 && PR_GetError () == PR_PENDING_INTERRUPT_ERROR);
	
	if (written == -1)
		set_errno (PR_GetError ());
	
	return written;
}

static int
stream_flush (CamelStream *stream)
{
	return PR_Sync (((CamelTcpStreamSSL *)stream)->priv->sockfd);
}

static int
stream_close (CamelStream *stream)
{
	if (PR_Close (((CamelTcpStreamSSL *)stream)->priv->sockfd) == PR_FAILURE)
		return -1;
	
	((CamelTcpStreamSSL *)stream)->priv->sockfd = NULL;
	
	return 0;
}

#if 0
/* Since this is default implementation, let NSS handle it. */
static SECStatus
ssl_get_client_auth (void *data, PRFileDesc *sockfd,
		     struct CERTDistNamesStr *caNames,
		     struct CERTCertificateStr **pRetCert,
		     struct SECKEYPrivateKeyStr **pRetKey) 
{
	SECStatus status = SECFailure;
	SECKEYPrivateKey *privkey;
	CERTCertificate *cert;
	void *proto_win;
	
	proto_win = SSL_RevealPinArg (sockfd);
	
	if ((char *)data) {
		cert = PK11_FindCertFromNickname ((char *)data, proto_win);
		if (cert) {
			privKey = PK11_FindKeyByAnyCert (cert, proto_win);
			if (privkey) {
				status = SECSuccess;
			} else {
				CERT_DestroyCertificate (cert);
			}
		}
	} else {
		/* no nickname given, automatically find the right cert */
		CERTCertNicknames *names;
		int i;
		
		names = CERT_GetCertNicknames (CERT_GetDefaultCertDB (), 
					       SEC_CERT_NICKNAMES_USER,
					       proto_win);
		
		if (names != NULL) {
			for (i = 0; i < names->numnicknames; i++) {
				
				cert = PK11_FindCertFromNickname (names->nicknames[i], 
								  proto_win);
				if (!cert)
					continue;
				
				/* Only check unexpired certs */
				if (CERT_CheckCertValidTimes (cert, PR_Now (), PR_FALSE) != secCertTimeValid) {
					CERT_DestroyCertificate (cert);
					continue;
				}
				
				status = NSS_CmpCertChainWCANames (cert, caNames);
				if (status == SECSuccess) {
					privkey = PK11_FindKeyByAnyCert (cert, proto_win);
					if (privkey)
						break;
					
					status = SECFailure;
					break;
				}
				
				CERT_FreeNicknames (names);
			}
		}
	}
	
	if (status == SECSuccess) {
		*pRetCert = cert;
		*pRetKey  = privkey;
	}
	
	return status;
}
#endif

#if 0
/* Since this is the default NSS implementation, no need for us to use this. */
static SECStatus
ssl_auth_cert (void *data, PRFileDesc *sockfd, PRBool checksig, PRBool is_server)
{
	CERTCertificate *cert;
	SECStatus status;
	void *pinarg;
	char *host;
	
	cert = SSL_PeerCertificate (sockfd);
	pinarg = SSL_RevealPinArg (sockfd);
	status = CERT_VerifyCertNow ((CERTCertDBHandle *)data, cert,
				     checksig, certUsageSSLClient, pinarg);
	
	if (status != SECSuccess)
		return SECFailure;
	
	/* Certificate is OK.  Since this is the client side of an SSL
	 * connection, we need to verify that the name field in the cert
	 * matches the desired hostname.  This is our defense against
	 * man-in-the-middle attacks.
	 */
	
	/* SSL_RevealURL returns a hostname, not a URL. */
	host = SSL_RevealURL (sockfd);
	
	if (host && *host) {
		status = CERT_VerifyCertName (cert, host);
	} else {
		PR_SetError (SSL_ERROR_BAD_CERT_DOMAIN, 0);
		status = SECFailure;
	}
	
	if (host)
		PR_Free (hostName);
	
	return secStatus;
}
#endif

static SECStatus
ssl_bad_cert (void *data, PRFileDesc *sockfd)
{
	CERTCertificate *cert;
	CamelService *service;
	char *prompt, *cert_str;
	gboolean accept;
	
	g_return_val_if_fail (data != NULL, SECFailure);
	g_return_val_if_fail (CAMEL_IS_SERVICE (data), SECFailure);
	
	service = CAMEL_SERVICE (data);
	
	cert = SSL_PeerCertificate (sockfd);
	
	cert_str = g_strdup_printf (_("EMail: %s\n"
				      "Common Name: %s\n"
				      "Organization Unit: %s\n"
				      "Organization: %s\n"
				      "Locality: %s\n"
				      "State: %s\n"
				      "Country: %s"),
				    cert->emailAddr ? cert->emailAddr : "",
				    CERT_GetCommonName (&cert->issuer) ? CERT_GetCommonName (&cert->issuer) : "",
				    CERT_GetOrgUnitName (&cert->issuer) ? CERT_GetOrgUnitName (&cert->issuer) : "",
				    CERT_GetOrgName (&cert->issuer) ? CERT_GetOrgName (&cert->issuer) : "",
				    CERT_GetLocalityName (&cert->issuer) ? CERT_GetLocalityName (&cert->issuer) : "",
				    CERT_GetStateName (&cert->issuer) ? CERT_GetStateName (&cert->issuer) : "",
				    CERT_GetCountryName (&cert->issuer) ? CERT_GetCountryName (&cert->issuer) : "");
	
	/* construct our user prompt */
	prompt = g_strdup_printf (_("Bad certificate from %s:\n\n%s\n\nDo you wish to accept anyway?"),
				  service->url->host, cert_str);
	g_free (cert_str);
	
	/* query the user to find out if we want to accept this certificate */
	accept = camel_session_alert_user (service->session, CAMEL_SESSION_ALERT_WARNING, prompt, TRUE);
	g_free (prompt);
	
	if (accept)
		return SECSuccess;
	
	return SECFailure;
}

static int
stream_connect (CamelTcpStream *stream, struct hostent *host, int port)
{
	CamelTcpStreamSSL *ssl = CAMEL_TCP_STREAM_SSL (stream);
	PRIntervalTime timeout = PR_INTERVAL_MIN;
	PRNetAddr netaddr;
	PRFileDesc *fd, *ssl_fd;
	
	g_return_val_if_fail (host != NULL, -1);
	
	memset ((void *) &netaddr, 0, sizeof (PRNetAddr));
	memcpy (&netaddr.inet.ip, host->h_addr, sizeof (netaddr.inet.ip));
	
	if (PR_InitializeNetAddr (PR_IpAddrNull, port, &netaddr) == PR_FAILURE)
		return -1;
	
	fd = PR_OpenTCPSocket (host->h_addrtype);
	ssl_fd = SSL_ImportFD (NULL, fd);
	
	SSL_SetURL (ssl_fd, ssl->priv->expected_host);
	
	if (ssl_fd == NULL || PR_Connect (ssl_fd, &netaddr, timeout) == PR_FAILURE) {
		if (ssl_fd != NULL)
			PR_Close (ssl_fd);
		
		return -1;
	}
	
	/*SSL_GetClientAuthDataHook (sslSocket, ssl_get_client_auth, (void *)certNickname);*/
	/*SSL_AuthCertificateHook (ssl_fd, ssl_auth_cert, (void *) CERT_GetDefaultCertDB ());*/
	SSL_BadCertHook (ssl_fd, ssl_bad_cert, ssl->priv->service);
	
	ssl->priv->sockfd = ssl_fd;
	
	return 0;
}


static int
stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data)
{
	PRSocketOptionData sodata;
	
	memset ((void *) &sodata, 0, sizeof (sodata));
	memcpy ((void *) &sodata, (void *) data, sizeof (CamelSockOptData));
	
	if (PR_GetSocketOption (((CamelTcpStreamSSL *)stream)->priv->sockfd, &sodata) == PR_FAILURE)
		return -1;
	
	memcpy ((void *) data, (void *) &sodata, sizeof (CamelSockOptData));
	
	return 0;
}

static int
stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data)
{
	PRSocketOptionData sodata;
	
	memset ((void *) &sodata, 0, sizeof (sodata));
	memcpy ((void *) &sodata, (void *) data, sizeof (CamelSockOptData));
	
	if (PR_SetSocketOption (((CamelTcpStreamSSL *)stream)->priv->sockfd, &sodata) == PR_FAILURE)
		return -1;
	
	return 0;
}

static gpointer
stream_get_socket (CamelTcpStream *stream)
{
	return (gpointer) CAMEL_TCP_STREAM_SSL (stream)->priv->sockfd;
}

#endif /* HAVE_NSS */
