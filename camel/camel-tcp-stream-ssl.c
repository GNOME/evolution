/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
 */

/* NOTE: This is the default implementation of CamelTcpStreamSSL,
 * used when the Mozilla NSS libraries are used. If you configured
 * OpenSSL support instead, then this file won't be compiled and
 * the CamelTcpStreamSSL implementation in camel-tcp-stream-openssl.c
 * will be used instead.
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
#include <cert.h>
#include <certdb.h>
#include <pk11func.h>

/* this is commented because otherwise we get an error about the
   redefinition of MD5Context...yay */
/*#include <e-util/md5-utils.h>*/

#include "camel-tcp-stream-ssl.h"
#include "camel-session.h"
#include "camel-certdb.h"

/* from md5-utils.h */
void md5_get_digest (const char *buffer, int buffer_size, unsigned char digest[16]);


static CamelTcpStreamClass *parent_class = NULL;

/* Returns the class for a CamelTcpStreamSSL */
#define CTSS_CLASS(so) CAMEL_TCP_STREAM_SSL_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static ssize_t stream_read (CamelStream *stream, char *buffer, size_t n);
static ssize_t stream_write (CamelStream *stream, const char *buffer, size_t n);
static int stream_flush  (CamelStream *stream);
static int stream_close  (CamelStream *stream);

static PRFileDesc *enable_ssl (CamelTcpStreamSSL *ssl, PRFileDesc *fd);

static int stream_connect    (CamelTcpStream *stream, struct hostent *host, int port);
static int stream_getsockopt (CamelTcpStream *stream, CamelSockOptData *data);
static int stream_setsockopt (CamelTcpStream *stream, const CamelSockOptData *data);
static CamelTcpAddress *stream_get_local_address (CamelTcpStream *stream);
static CamelTcpAddress *stream_get_remote_address (CamelTcpStream *stream);

struct _CamelTcpStreamSSLPrivate {
	PRFileDesc *sockfd;
	
	CamelService *service;
	char *expected_host;
	gboolean ssl_mode;
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
	camel_tcp_stream_class->get_local_address  = stream_get_local_address;
	camel_tcp_stream_class->get_remote_address = stream_get_remote_address;
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
 * Return value: a ssl stream (in ssl mode)
 **/
CamelStream *
camel_tcp_stream_ssl_new (CamelService *service, const char *expected_host)
{
	CamelTcpStreamSSL *stream;
	
	stream = CAMEL_TCP_STREAM_SSL (camel_object_new (camel_tcp_stream_ssl_get_type ()));
	
	stream->priv->service = service;
	stream->priv->expected_host = g_strdup (expected_host);
	stream->priv->ssl_mode = TRUE;
	
	return CAMEL_STREAM (stream);
}


/**
 * camel_tcp_stream_ssl_new_raw:
 * @service: camel service
 * @expected_host: host that the stream is expected to connect with.
 *
 * Since the SSL certificate authenticator may need to prompt the
 * user, a CamelService is needed. @expected_host is needed as a
 * protection against an MITM attack.
 *
 * Return value: a ssl-capable stream (in non ssl mode)
 **/
CamelStream *
camel_tcp_stream_ssl_new_raw (CamelService *service, const char *expected_host)
{
	CamelTcpStreamSSL *stream;
	
	stream = CAMEL_TCP_STREAM_SSL (camel_object_new (camel_tcp_stream_ssl_get_type ()));
	
	stream->priv->service = service;
	stream->priv->expected_host = g_strdup (expected_host);
	stream->priv->ssl_mode = FALSE;
	
	return CAMEL_STREAM (stream);
}


static void
set_errno (int code)
{
	/* FIXME: this should handle more. */
	switch (code) {
	case PR_INVALID_ARGUMENT_ERROR:
		errno = EINVAL;
		break;
	case PR_PENDING_INTERRUPT_ERROR:
		errno = EINTR;
		break;
	case PR_IO_PENDING_ERROR:
		errno = EAGAIN;
		break;
	case PR_WOULD_BLOCK_ERROR:
		errno = EWOULDBLOCK;
		break;
	case PR_IN_PROGRESS_ERROR:
		errno = EINPROGRESS;
		break;
	case PR_ALREADY_INITIATED_ERROR:
		errno = EALREADY;
		break;
	case PR_NETWORK_UNREACHABLE_ERROR:
		errno = EHOSTUNREACH;
		break;
	case PR_CONNECT_REFUSED_ERROR:
		errno = ECONNREFUSED;
		break;
	case PR_CONNECT_TIMEOUT_ERROR:
	case PR_IO_TIMEOUT_ERROR:
		errno = ETIMEDOUT;
		break;
	case PR_NOT_CONNECTED_ERROR:
		errno = ENOTCONN;
		break;
	case PR_CONNECT_RESET_ERROR:
		errno = ECONNRESET;
		break;
	case PR_IO_ERROR:
	default:
		errno = EIO;
		break;
	}
}


/**
 * camel_tcp_stream_ssl_enable_ssl:
 * @ssl: ssl stream
 *
 * Toggles an ssl-capable stream into ssl mode (if it isn't already).
 *
 * Returns 0 on success or -1 on fail.
 **/
int
camel_tcp_stream_ssl_enable_ssl (CamelTcpStreamSSL *ssl)
{
	PRFileDesc *fd;
	
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM_SSL (ssl), -1);
	
	if (ssl->priv->sockfd && !ssl->priv->ssl_mode) {
		fd = enable_ssl (ssl, NULL);
		if (fd == NULL) {
			int errnosave;
			
			set_errno (PR_GetError ());
			errnosave = errno;
			errno = errnosave;
			
			return -1;
		}
		
		SSL_ResetHandshake (fd, FALSE);
		
		ssl->priv->sockfd = fd;
	}
	
	ssl->priv->ssl_mode = TRUE;
	
	return 0;
}


static ssize_t
stream_read (CamelStream *stream, char *buffer, size_t n)
{
	CamelTcpStreamSSL *tcp_stream_ssl = CAMEL_TCP_STREAM_SSL (stream);
	ssize_t nread;
	
	do {
		if (camel_operation_cancel_check (NULL)) {
			errno = EINTR;
			return -1;
		}
		
		nread = PR_Read (tcp_stream_ssl->priv->sockfd, buffer, n);
		if (nread == -1)
			set_errno (PR_GetError ());
	} while (nread == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
	
	return nread;
}

static ssize_t
stream_write (CamelStream *stream, const char *buffer, size_t n)
{
	CamelTcpStreamSSL *tcp_stream_ssl = CAMEL_TCP_STREAM_SSL (stream);
	ssize_t w, written = 0;
	
	do {
		if (camel_operation_cancel_check (NULL)) {
			errno = EINTR;
			return -1;
		}
		
		do {
			w = PR_Write (tcp_stream_ssl->priv->sockfd, buffer + written, n - written);
			if (w == -1)
				set_errno (PR_GetError ());
		} while (w == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
		
		if (w > 0)
			written += w;
	} while (w != -1 && written < n);
	
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
	
	if ((char *) data) {
		cert = PK11_FindCertFromNickname ((char *) data, proto_win);
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
		PR_Free (host);
	
	return secStatus;
}
#endif

static SECStatus
ssl_bad_cert (void *data, PRFileDesc *sockfd)
{
	unsigned char md5sum[16], fingerprint[40], *f;
	gboolean accept, valid_cert;
	CamelCertDB *certdb = NULL;
	CamelCert *ccert = NULL;
	char *prompt, *cert_str;
	CamelTcpStreamSSL *ssl;
	CERTCertificate *cert;
	CamelService *service;
	int i;
	
	g_return_val_if_fail (data != NULL, SECFailure);
	g_return_val_if_fail (CAMEL_IS_TCP_STREAM_SSL (data), SECFailure);
	
	ssl = CAMEL_TCP_STREAM_SSL (data);
	service = ssl->priv->service;
	
	cert = SSL_PeerCertificate (sockfd);
	
	/* calculate the MD5 hash of the raw certificate */
	md5_get_digest (cert->derCert.data, cert->derCert.len, md5sum);
	/*HASH_HashBuf (HASH_AlgMD5, md5sum, cert->derCert.data, cert->derCert.len);*/
	for (i = 0, f = fingerprint; i < 16; i++, f += 3)
		sprintf (f, "%.2x%c", md5sum[i], i != 15 ? ':' : '\0');
	
	valid_cert = CERT_VerifyCertNow (CERT_GetDefaultCertDB (), cert, TRUE, certUsageSSLClient, NULL);
	/*issuer = CERT_FindCertByName (CERT_GetDefaultCertDB (), &cert->derIssuer);
	  valid_cert = issuer && CERT_VerifySignedData (&cert->signatureWrap, issuer, PR_Now (), NULL);*/
	
	/* first check our own certificate database to see if we accepted the cert (nss's certdb seems to not work) */
	certdb = camel_certdb_get_default ();
	if (certdb) {
		ccert = camel_certdb_get_cert (certdb, fingerprint);
		if (ccert) {
			if (ccert->trust != CAMEL_CERT_TRUST_UNKNOWN) {
				accept = ccert->trust != CAMEL_CERT_TRUST_NEVER;
				camel_certdb_cert_unref (certdb, ccert);
				camel_object_unref (certdb);
				
				return accept ? SECSuccess : SECFailure;
			}
		} else {
			/* create a new camel-cert */
			ccert = camel_certdb_cert_new (certdb);
			camel_cert_set_issuer (certdb, ccert, CERT_NameToAscii (&cert->issuer));
			camel_cert_set_subject (certdb, ccert, CERT_NameToAscii (&cert->subject));
			camel_cert_set_hostname (certdb, ccert, ssl->priv->expected_host);
			camel_cert_set_fingerprint (certdb, ccert, fingerprint);
			camel_cert_set_trust (certdb, ccert, CAMEL_CERT_TRUST_UNKNOWN);
			
			/* Add the certificate to our db */
			camel_certdb_add (certdb, ccert);
		}
	}
	
	cert_str = g_strdup_printf (_("Issuer:            %s\n"
				      "Subject:           %s\n"
				      "Fingerprint:       %s\n"
				      "Signature:         %s"),
				    CERT_NameToAscii (&cert->issuer),
				    CERT_NameToAscii (&cert->subject),
				    fingerprint, valid_cert ? _("GOOD") : _("BAD"));
	
	/* construct our user prompt */
	prompt = g_strdup_printf (_("SSL Certificate check for %s:\n\n%s\n\nDo you wish to accept?"),
				  service->url->host, cert_str);
	g_free (cert_str);
	
	/* query the user to find out if we want to accept this certificate */
	accept = camel_session_alert_user (service->session, CAMEL_SESSION_ALERT_WARNING, prompt, TRUE);
	g_free (prompt);
	
	if (accept) {
#if 0
		/* this is how mutt does it but last time I tried to
                   use CERT_AddTempCertToPerm() I got link errors and
                   I have also been told by the nss devs that that
                   function has been deprecated... */
		CERTCertTrust trust;
		
		CERT_DecodeTrustString (&trust, "P,,");
		
		CERT_AddTempCertToPerm (cert, NULL, &trust);
#else
		SECItem *certs[1];
		
		if (!cert->trust)
			cert->trust = PORT_ZAlloc (sizeof (CERTCertTrust));
		
		cert->trust->sslFlags = CERTDB_VALID_PEER | CERTDB_TRUSTED;
		
		certs[0] = &cert->derCert;
		
		CERT_ImportCerts (CERT_GetDefaultCertDB (), certUsageSSLServer, 1, certs,
				  NULL, TRUE, FALSE, cert->nickname);
#endif
		
		if (ccert) {
			camel_cert_set_trust (certdb, ccert, CAMEL_CERT_TRUST_FULLY);
			camel_certdb_touch (certdb);
		}
	}
	
	if (certdb) {
		camel_certdb_cert_unref (certdb, ccert);
		camel_object_unref (certdb);
	}
	
	return accept ? SECSuccess : SECFailure;
}

static PRFileDesc *
enable_ssl (CamelTcpStreamSSL *ssl, PRFileDesc *fd)
{
	PRFileDesc *ssl_fd;
	
	ssl_fd = SSL_ImportFD (NULL, fd ? fd : ssl->priv->sockfd);
	if (!ssl_fd)
		return NULL;
	
	SSL_OptionSet (ssl_fd, SSL_SECURITY, PR_TRUE);
	SSL_SetURL (ssl_fd, ssl->priv->expected_host);
	
	/*SSL_GetClientAuthDataHook (sslSocket, ssl_get_client_auth, (void *) certNickname);*/
	/*SSL_AuthCertificateHook (ssl_fd, ssl_auth_cert, (void *) CERT_GetDefaultCertDB ());*/
	SSL_BadCertHook (ssl_fd, ssl_bad_cert, ssl);
	
	ssl->priv->ssl_mode = TRUE;
	
	return ssl_fd;
}

#define CONNECT_TIMEOUT PR_TicksPerSecond () * 120

static int
stream_connect (CamelTcpStream *stream, struct hostent *host, int port)
{
	CamelTcpStreamSSL *ssl = CAMEL_TCP_STREAM_SSL (stream);
	PRIntervalTime timeout = CONNECT_TIMEOUT;
	PRNetAddr netaddr;
	PRFileDesc *fd;
	
	g_return_val_if_fail (host != NULL, -1);
	
	memset ((void *) &netaddr, 0, sizeof (PRNetAddr));
	memcpy (&netaddr.inet.ip, host->h_addr, sizeof (netaddr.inet.ip));
	
	if (PR_InitializeNetAddr (PR_IpAddrNull, port, &netaddr) == PR_FAILURE) {
		set_errno (PR_GetError ());
		return -1;
	}
	
	fd = PR_OpenTCPSocket (host->h_addrtype);
	if (fd == NULL) {
		set_errno (PR_GetError ());
		return -1;
	}
	
	if (ssl->priv->ssl_mode) {
		PRFileDesc *ssl_fd;
		
		ssl_fd = enable_ssl (ssl, fd);
		if (ssl_fd == NULL) {
			int errnosave;
			
			set_errno (PR_GetError ());
			errnosave = errno;
			PR_Close (fd);
			errno = errnosave;
			
			return -1;
		}
		
		fd = ssl_fd;
	}
	
	if (PR_Connect (fd, &netaddr, timeout) == PR_FAILURE) {
		int errnosave;
		
		set_errno (PR_GetError ());
		if (errno == EINPROGRESS) {
			gboolean connected = FALSE;
			PRPollDesc poll;
			
			do {
				poll.fd = fd;
				poll.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
				poll.out_flags = 0;
				
				timeout = CONNECT_TIMEOUT;
				
				if (PR_Poll (&poll, 1, timeout) == PR_FAILURE) {
					set_errno (PR_GetError ());
					goto exception;
				}
				
				if (PR_GetConnectStatus (&poll) == PR_FAILURE) {
					set_errno (PR_GetError ());
					if (errno != EINPROGRESS)
						goto exception;
				} else {
					connected = TRUE;
				}
			} while (!connected);
		} else {
		exception:
			errnosave = errno;
			PR_Close (fd);
			ssl->priv->sockfd = NULL;
			errno = errnosave;
			
			return -1;
		}
		
		errno = 0;
	}
	
	ssl->priv->sockfd = fd;
	
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

static CamelTcpAddress *
stream_get_local_address (CamelTcpStream *stream)
{
	PRFileDesc *sockfd = CAMEL_TCP_STREAM_SSL (stream)->priv->sockfd;
	PRNetAddr addr;

	PR_GetSockName (sockfd, &addr);
	if (addr.inet.family != PR_AF_INET)
		return NULL;

	return camel_tcp_address_new (CAMEL_TCP_ADDRESS_IPV4, addr.inet.port,
				      4, &addr.inet.ip);
}

static CamelTcpAddress *
stream_get_remote_address (CamelTcpStream *stream)
{
	PRFileDesc *sockfd = CAMEL_TCP_STREAM_SSL (stream)->priv->sockfd;
	PRNetAddr addr;

	PR_GetPeerName (sockfd, &addr);
	if (addr.inet.family != PR_AF_INET)
		return NULL;

	return camel_tcp_address_new (CAMEL_TCP_ADDRESS_IPV4, addr.inet.port,
				      4, &addr.inet.ip);
}

#endif /* HAVE_NSS */
