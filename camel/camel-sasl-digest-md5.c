/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001-2003 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <libedataserver/md5-utils.h>

#include <libedataserver/e-iconv.h>

#include "camel-charset-map.h"
#include "camel-mime-utils.h"
#include "camel-sasl-digest-md5.h"
#include "camel-i18n.h"
#include "camel-net-utils.h"

#define d(x)

#define PARANOID(x) x

/* Implements rfc2831 */

CamelServiceAuthType camel_sasl_digest_md5_authtype = {
	N_("DIGEST-MD5"),

	N_("This option will connect to the server using a "
	   "secure DIGEST-MD5 password, if the server supports it."),

	"DIGEST-MD5",
	TRUE
};

static CamelSaslClass *parent_class = NULL;

/* Returns the class for a CamelSaslDigestMd5 */
#define CSCM_CLASS(so) CAMEL_SASL_DIGEST_MD5_CLASS (CAMEL_OBJECT_GET_CLASS (so))

static GByteArray *digest_md5_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex);

enum {
	STATE_AUTH,
	STATE_FINAL
};

typedef struct {
	char *name;
	guint type;
} DataType;

enum {
	DIGEST_REALM,
	DIGEST_NONCE,
	DIGEST_QOP,
	DIGEST_STALE,
	DIGEST_MAXBUF,
	DIGEST_CHARSET,
	DIGEST_ALGORITHM,
	DIGEST_CIPHER,
	DIGEST_UNKNOWN
};

static DataType digest_args[] = {
	{ "realm",     DIGEST_REALM     },
	{ "nonce",     DIGEST_NONCE     },
	{ "qop",       DIGEST_QOP       },
	{ "stale",     DIGEST_STALE     },
	{ "maxbuf",    DIGEST_MAXBUF    },
	{ "charset",   DIGEST_CHARSET   },
	{ "algorithm", DIGEST_ALGORITHM },
	{ "cipher",    DIGEST_CIPHER    },
	{ NULL,        DIGEST_UNKNOWN   }
};

#define QOP_AUTH           (1<<0)
#define QOP_AUTH_INT       (1<<1)
#define QOP_AUTH_CONF      (1<<2)
#define QOP_INVALID        (1<<3)

static DataType qop_types[] = {
	{ "auth",      QOP_AUTH      },
	{ "auth-int",  QOP_AUTH_INT  },
	{ "auth-conf", QOP_AUTH_CONF },
	{ NULL,        QOP_INVALID   }
};

#define CIPHER_DES         (1<<0)
#define CIPHER_3DES        (1<<1)
#define CIPHER_RC4         (1<<2)
#define CIPHER_RC4_40      (1<<3)
#define CIPHER_RC4_56      (1<<4)
#define CIPHER_INVALID     (1<<5)

static DataType cipher_types[] = {
	{ "des",    CIPHER_DES     },
	{ "3des",   CIPHER_3DES    },
	{ "rc4",    CIPHER_RC4     },
	{ "rc4-40", CIPHER_RC4_40  },
	{ "rc4-56", CIPHER_RC4_56  },
	{ NULL,     CIPHER_INVALID }
};

struct _param {
	char *name;
	char *value;
};

struct _DigestChallenge {
	GPtrArray *realms;
	char *nonce;
	guint qop;
	gboolean stale;
	gint32 maxbuf;
	char *charset;
	char *algorithm;
	guint cipher;
	GList *params;
};

struct _DigestURI {
	char *type;
	char *host;
	char *name;
};

struct _DigestResponse {
	char *username;
	char *realm;
	char *nonce;
	char *cnonce;
	char nc[9];
	guint qop;
	struct _DigestURI *uri;
	char resp[33];
	guint32 maxbuf;
	char *charset;
	guint cipher;
	char *authzid;
	char *param;
};

struct _CamelSaslDigestMd5Private {
	struct _DigestChallenge *challenge;
	struct _DigestResponse *response;
	int state;
};

static void
camel_sasl_digest_md5_class_init (CamelSaslDigestMd5Class *camel_sasl_digest_md5_class)
{
	CamelSaslClass *camel_sasl_class = CAMEL_SASL_CLASS (camel_sasl_digest_md5_class);
	
	parent_class = CAMEL_SASL_CLASS (camel_type_get_global_classfuncs (camel_sasl_get_type ()));
	
	/* virtual method overload */
	camel_sasl_class->challenge = digest_md5_challenge;
}

static void
camel_sasl_digest_md5_init (gpointer object, gpointer klass)
{
	CamelSaslDigestMd5 *sasl_digest = CAMEL_SASL_DIGEST_MD5 (object);
	
	sasl_digest->priv = g_new0 (struct _CamelSaslDigestMd5Private, 1);
}

static void
camel_sasl_digest_md5_finalize (CamelObject *object)
{
	CamelSaslDigestMd5 *sasl = CAMEL_SASL_DIGEST_MD5 (object);
	struct _DigestChallenge *c = sasl->priv->challenge;
	struct _DigestResponse *r = sasl->priv->response;
	GList *p;
	int i;
	
	for (i = 0; i < c->realms->len; i++)
		g_free (c->realms->pdata[i]);
	g_ptr_array_free (c->realms, TRUE);
	g_free (c->nonce);
	g_free (c->charset);
	g_free (c->algorithm);
	for (p = c->params; p; p = p->next) {
		struct _param *param = p->data;
		
		g_free (param->name);
		g_free (param->value);
		g_free (param);
	}
	g_list_free (c->params);
	g_free (c);
	
	g_free (r->username);
	g_free (r->realm);
	g_free (r->nonce);
	g_free (r->cnonce);
	if (r->uri) {
		g_free (r->uri->type);
		g_free (r->uri->host);
		g_free (r->uri->name);
	}
	g_free (r->charset);
	g_free (r->authzid);
	g_free (r->param);
	g_free (r);
	
	g_free (sasl->priv);
}


CamelType
camel_sasl_digest_md5_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_sasl_get_type (),
					    "CamelSaslDigestMd5",
					    sizeof (CamelSaslDigestMd5),
					    sizeof (CamelSaslDigestMd5Class),
					    (CamelObjectClassInitFunc) camel_sasl_digest_md5_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_sasl_digest_md5_init,
					    (CamelObjectFinalizeFunc) camel_sasl_digest_md5_finalize);
	}
	
	return type;
}

static void
decode_lwsp (const char **in)
{
	const char *inptr = *in;
	
	while (isspace (*inptr))
		inptr++;
	
	*in = inptr;
}

static char *
decode_quoted_string (const char **in)
{
	const char *inptr = *in;
	char *out = NULL, *outptr;
	int outlen;
	int c;
	
	decode_lwsp (&inptr);
	if (*inptr == '"') {
		const char *intmp;
		int skip = 0;
		
		/* first, calc length */
		inptr++;
		intmp = inptr;
		while ((c = *intmp++) && c != '"') {
			if (c == '\\' && *intmp) {
				intmp++;
				skip++;
			}
		}
		
		outlen = intmp - inptr - skip;
		out = outptr = g_malloc (outlen + 1);
		
		while ((c = *inptr++) && c != '"') {
			if (c == '\\' && *inptr) {
				c = *inptr++;
			}
			*outptr++ = c;
		}
		*outptr = '\0';
	}
	
	*in = inptr;
	
	return out;
}

static char *
decode_token (const char **in)
{
	const char *inptr = *in;
	const char *start;
	
	decode_lwsp (&inptr);
	start = inptr;
	
	while (*inptr && *inptr != '=' && *inptr != ',')
		inptr++;
	
	if (inptr > start) {
		*in = inptr;
		return g_strndup (start, inptr - start);
	} else {
		return NULL;
	}
}

static char *
decode_value (const char **in)
{
	const char *inptr = *in;
	
	decode_lwsp (&inptr);
	if (*inptr == '"') {
		d(printf ("decoding quoted string token\n"));
		return decode_quoted_string (in);
	} else {
		d(printf ("decoding string token\n"));
		return decode_token (in);
	}
}

static GList *
parse_param_list (const char *tokens)
{
	GList *params = NULL;
	struct _param *param;
	const char *ptr;
	
	for (ptr = tokens; ptr && *ptr; ) {
		param = g_new0 (struct _param, 1);
		param->name = decode_token (&ptr);
		if (*ptr == '=') {
			ptr++;
			param->value = decode_value (&ptr);
		}
		
		params = g_list_prepend (params, param);
		
		if (*ptr == ',')
			ptr++;
	}
	
	return params;
}

static guint
decode_data_type (DataType *dtype, const char *name)
{
	int i;
	
	for (i = 0; dtype[i].name; i++) {
		if (!g_ascii_strcasecmp (dtype[i].name, name))
			break;
	}
	
	return dtype[i].type;
}

#define get_digest_arg(name) decode_data_type (digest_args, name)
#define decode_qop(name)     decode_data_type (qop_types, name)
#define decode_cipher(name)  decode_data_type (cipher_types, name)

static const char *
type_to_string (DataType *dtype, guint type)
{
	int i;
	
	for (i = 0; dtype[i].name; i++) {
		if (dtype[i].type == type)
			break;
	}
	
	return dtype[i].name;
}

#define qop_to_string(type)    type_to_string (qop_types, type)
#define cipher_to_string(type) type_to_string (cipher_types, type)

static void
digest_abort (gboolean *have_type, gboolean *abort)
{
	if (*have_type)
		*abort = TRUE;
	*have_type = TRUE;
}

static struct _DigestChallenge *
parse_server_challenge (const char *tokens, gboolean *abort)
{
	struct _DigestChallenge *challenge = NULL;
	GList *params, *p;
	const char *ptr;
#ifdef PARANOID
	gboolean got_algorithm = FALSE;
	gboolean got_stale = FALSE;
	gboolean got_maxbuf = FALSE;
	gboolean got_charset = FALSE;
#endif /* PARANOID */
	
	params = parse_param_list (tokens);
	if (!params) {
		*abort = TRUE;
		return NULL;
	}
	
	*abort = FALSE;
	
	challenge = g_new0 (struct _DigestChallenge, 1);
	challenge->realms = g_ptr_array_new ();
	challenge->maxbuf = 65536;
	
	for (p = params; p; p = p->next) {
		struct _param *param = p->data;
		int type;
		
		type = get_digest_arg (param->name);
		switch (type) {
		case DIGEST_REALM:
			for (ptr = param->value; ptr && *ptr; ) {
				char *token;
				
				token = decode_token (&ptr);
				if (token)
					g_ptr_array_add (challenge->realms, token);
				
				if (*ptr == ',')
					ptr++;
			}
			g_free (param->value);
			g_free (param->name);
			g_free (param);
			break;
		case DIGEST_NONCE:
			g_free (challenge->nonce);
			challenge->nonce = param->value;
			g_free (param->name);
			g_free (param);
			break;
		case DIGEST_QOP:
			for (ptr = param->value; ptr && *ptr; ) {
				char *token;
				
				token = decode_token (&ptr);
				if (token)
					challenge->qop |= decode_qop (token);
				
				if (*ptr == ',')
					ptr++;
			}
			
			if (challenge->qop & QOP_INVALID)
				challenge->qop = QOP_INVALID;
			g_free (param->value);
			g_free (param->name);
			g_free (param);
			break;
		case DIGEST_STALE:
			PARANOID (digest_abort (&got_stale, abort));
			if (!g_ascii_strcasecmp (param->value, "true"))
				challenge->stale = TRUE;
			else
				challenge->stale = FALSE;
			g_free (param->value);
			g_free (param->name);
			g_free (param);
			break;
		case DIGEST_MAXBUF:
			PARANOID (digest_abort (&got_maxbuf, abort));
			challenge->maxbuf = atoi (param->value);
			g_free (param->value);
			g_free (param->name);
			g_free (param);
			break;
		case DIGEST_CHARSET:
			PARANOID (digest_abort (&got_charset, abort));
			g_free (challenge->charset);
			if (param->value && *param->value)
				challenge->charset = param->value;
			else
				challenge->charset = NULL;
			g_free (param->name);
			g_free (param);
			break;
		case DIGEST_ALGORITHM:
			PARANOID (digest_abort (&got_algorithm, abort));
			g_free (challenge->algorithm);
			challenge->algorithm = param->value;
			g_free (param->name);
			g_free (param);
			break;
		case DIGEST_CIPHER:
			for (ptr = param->value; ptr && *ptr; ) {
				char *token;
				
				token = decode_token (&ptr);
				if (token)
					challenge->cipher |= decode_cipher (token);
				
				if (*ptr == ',')
					ptr++;
			}
			if (challenge->cipher & CIPHER_INVALID)
				challenge->cipher = CIPHER_INVALID;
			g_free (param->value);
			g_free (param->name);
			g_free (param);
			break;
		default:
			challenge->params = g_list_prepend (challenge->params, param);
			break;
		}
	}
	
	g_list_free (params);
	
	return challenge;
}

static void
digest_hex (guchar *digest, guchar hex[33])
{
	guchar *s, *p;
	
	/* lowercase hexify that bad-boy... */
	for (s = digest, p = hex; p < hex + 32; s++, p += 2)
		sprintf (p, "%.2x", *s);
}

static char *
digest_uri_to_string (struct _DigestURI *uri)
{
	if (uri->name)
		return g_strdup_printf ("%s/%s/%s", uri->type, uri->host, uri->name);
	else
		return g_strdup_printf ("%s/%s", uri->type, uri->host);
}

static void
compute_response (struct _DigestResponse *resp, const char *passwd, gboolean client, guchar out[33])
{
	guchar hex_a1[33], hex_a2[33];
	guchar digest[16];
	MD5Context ctx;
	char *buf;
	
	/* compute A1 */
	md5_init (&ctx);
	md5_update (&ctx, resp->username, strlen (resp->username));
	md5_update (&ctx, ":", 1);
	md5_update (&ctx, resp->realm, strlen (resp->realm));
	md5_update (&ctx, ":", 1);
	md5_update (&ctx, passwd, strlen (passwd));
	md5_final (&ctx, digest);
	
	md5_init (&ctx);
	md5_update (&ctx, digest, 16);
	md5_update (&ctx, ":", 1);
	md5_update (&ctx, resp->nonce, strlen (resp->nonce));
	md5_update (&ctx, ":", 1);
	md5_update (&ctx, resp->cnonce, strlen (resp->cnonce));
	if (resp->authzid) {
		md5_update (&ctx, ":", 1);
		md5_update (&ctx, resp->authzid, strlen (resp->authzid));
	}
	
	/* hexify A1 */
	md5_final (&ctx, digest);
	digest_hex (digest, hex_a1);
	
	/* compute A2 */
	md5_init (&ctx);
	if (client) {
		/* we are calculating the client response */
		md5_update (&ctx, "AUTHENTICATE:", strlen ("AUTHENTICATE:"));
	} else {
		/* we are calculating the server rspauth */
		md5_update (&ctx, ":", 1);
	}
	
	buf = digest_uri_to_string (resp->uri);
	md5_update (&ctx, buf, strlen (buf));
	g_free (buf);
	
	if (resp->qop == QOP_AUTH_INT || resp->qop == QOP_AUTH_CONF)
		md5_update (&ctx, ":00000000000000000000000000000000", 33);
	
	/* now hexify A2 */
	md5_final (&ctx, digest);
	digest_hex (digest, hex_a2);
	
	/* compute KD */
	md5_init (&ctx);
	md5_update (&ctx, hex_a1, 32);
	md5_update (&ctx, ":", 1);
	md5_update (&ctx, resp->nonce, strlen (resp->nonce));
	md5_update (&ctx, ":", 1);
	md5_update (&ctx, resp->nc, 8);
	md5_update (&ctx, ":", 1);
	md5_update (&ctx, resp->cnonce, strlen (resp->cnonce));
	md5_update (&ctx, ":", 1);
	md5_update (&ctx, qop_to_string (resp->qop), strlen (qop_to_string (resp->qop)));
	md5_update (&ctx, ":", 1);
	md5_update (&ctx, hex_a2, 32);
	md5_final (&ctx, digest);
	
	digest_hex (digest, out);
}

static struct _DigestResponse *
generate_response (struct _DigestChallenge *challenge, const char *host,
		   const char *protocol, const char *user, const char *passwd)
{
	struct _DigestResponse *resp;
	struct _DigestURI *uri;
	char *bgen, digest[16];
	
	resp = g_new0 (struct _DigestResponse, 1);
	resp->username = g_strdup (user);
	/* FIXME: we should use the preferred realm */
	if (challenge->realms && challenge->realms->len > 0)
		resp->realm = g_strdup (challenge->realms->pdata[0]);
	else
		resp->realm = g_strdup ("");
	
	resp->nonce = g_strdup (challenge->nonce);
	
	/* generate the cnonce */
	bgen = g_strdup_printf ("%p:%lu:%lu", resp,
				(unsigned long) getpid (),
				(unsigned long) time (0));
	md5_get_digest (bgen, strlen (bgen), digest);
	g_free (bgen);
	/* take our recommended 64 bits of entropy */
	resp->cnonce = camel_base64_encode_simple (digest, 8);
	
	/* we don't support re-auth so the nonce count is always 1 */
	strcpy (resp->nc, "00000001");
	
	/* choose the QOP */
	/* FIXME: choose - probably choose "auth" ??? */
	resp->qop = QOP_AUTH;
	
	/* create the URI */
	uri = g_new0 (struct _DigestURI, 1);
	uri->type = g_strdup (protocol);
	uri->host = g_strdup (host);
	uri->name = NULL;
	resp->uri = uri;
	
	/* charsets... yay */
	if (challenge->charset) {
		/* I believe that this is only ever allowed to be
		 * UTF-8. We strdup the charset specified by the
		 * challenge anyway, just in case it's not UTF-8.
		 */
		resp->charset = g_strdup (challenge->charset);
	}
	
	resp->cipher = CIPHER_INVALID;
	if (resp->qop == QOP_AUTH_CONF) {
		/* FIXME: choose a cipher? */
		resp->cipher = CIPHER_INVALID;
	}
	
	/* we don't really care about this... */
	resp->authzid = NULL;
	
	compute_response (resp, passwd, TRUE, resp->resp);
	
	return resp;
}

static GByteArray *
digest_response (struct _DigestResponse *resp)
{
	GByteArray *buffer;
	const char *str;
	char *buf;
	
	buffer = g_byte_array_new ();
	g_byte_array_append (buffer, "username=\"", 10);
	if (resp->charset) {
		/* Encode the username using the requested charset */
		char *username, *outbuf;
		const char *charset;
		size_t len, outlen;
		const char *inbuf;
		iconv_t cd;
		
		charset = e_iconv_locale_charset ();
		if (!charset)
			charset = "iso-8859-1";
		
		cd = e_iconv_open (resp->charset, charset);
		
		len = strlen (resp->username);
		outlen = 2 * len; /* plenty of space */
		
		outbuf = username = g_malloc0 (outlen + 1);
		inbuf = resp->username;
		if (cd == (iconv_t) -1 || e_iconv (cd, &inbuf, &len, &outbuf, &outlen) == (size_t) -1) {
			/* We can't convert to UTF-8 - pretend we never got a charset param? */
			g_free (resp->charset);
			resp->charset = NULL;
			
			/* Set the username to the non-UTF-8 version */
			g_free (username);
			username = g_strdup (resp->username);
		}
		
		if (cd != (iconv_t) -1)
			e_iconv_close (cd);
		
		g_byte_array_append (buffer, username, strlen (username));
		g_free (username);
	} else {
		g_byte_array_append (buffer, resp->username, strlen (resp->username));
	}
	
	g_byte_array_append (buffer, "\",realm=\"", 9);
	g_byte_array_append (buffer, resp->realm, strlen (resp->realm));
	
	g_byte_array_append (buffer, "\",nonce=\"", 9);
	g_byte_array_append (buffer, resp->nonce, strlen (resp->nonce));
	
	g_byte_array_append (buffer, "\",cnonce=\"", 10);
	g_byte_array_append (buffer, resp->cnonce, strlen (resp->cnonce));
	
	g_byte_array_append (buffer, "\",nc=", 5);
	g_byte_array_append (buffer, resp->nc, 8);
	
	g_byte_array_append (buffer, ",qop=", 5);
	str = qop_to_string (resp->qop);
	g_byte_array_append (buffer, str, strlen (str));
	
	g_byte_array_append (buffer, ",digest-uri=\"", 13);
	buf = digest_uri_to_string (resp->uri);
	g_byte_array_append (buffer, buf, strlen (buf));
	g_free (buf);
	
	g_byte_array_append (buffer, "\",response=", 11);
	g_byte_array_append (buffer, resp->resp, 32);
	
	if (resp->maxbuf > 0) {
		g_byte_array_append (buffer, ",maxbuf=", 8);
		buf = g_strdup_printf ("%d", resp->maxbuf);
		g_byte_array_append (buffer, buf, strlen (buf));
		g_free (buf);
	}
	
	if (resp->charset) {
		g_byte_array_append (buffer, ",charset=", 9);
		g_byte_array_append (buffer, resp->charset, strlen (resp->charset));
	}
	
	if (resp->cipher != CIPHER_INVALID) {
		str = cipher_to_string (resp->cipher);
		if (str) {
			g_byte_array_append (buffer, ",cipher=\"", 9);
			g_byte_array_append (buffer, str, strlen (str));
			g_byte_array_append (buffer, "\"", 1);
		}
	}
	
	if (resp->authzid) {
		g_byte_array_append (buffer, ",authzid=\"", 10);
		g_byte_array_append (buffer, resp->authzid, strlen (resp->authzid));
		g_byte_array_append (buffer, "\"", 1);
	}
	
	return buffer;
}

static GByteArray *
digest_md5_challenge (CamelSasl *sasl, GByteArray *token, CamelException *ex)
{
	CamelSaslDigestMd5 *sasl_digest = CAMEL_SASL_DIGEST_MD5 (sasl);
	struct _CamelSaslDigestMd5Private *priv = sasl_digest->priv;
	struct _param *rspauth;
	GByteArray *ret = NULL;
	gboolean abort = FALSE;
	const char *ptr;
	guchar out[33];
	char *tokens;
	struct addrinfo *ai, hints;
	
	/* Need to wait for the server */
	if (!token)
		return NULL;
	
	g_return_val_if_fail (sasl->service->url->passwd != NULL, NULL);
	
	switch (priv->state) {
	case STATE_AUTH:
		if (token->len > 2048) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Server challenge too long (>2048 octets)\n"));
			return NULL;
		}
		
		tokens = g_strndup (token->data, token->len);
		priv->challenge = parse_server_challenge (tokens, &abort);
		g_free (tokens);
		if (!priv->challenge || abort) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Server challenge invalid\n"));
			return NULL;
		}
		
		if (priv->challenge->qop == QOP_INVALID) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Server challenge contained invalid "
						"\"Quality of Protection\" token\n"));
			return NULL;
		}

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_CANONNAME;
		ai = camel_getaddrinfo(sasl->service->url->host?sasl->service->url->host:"localhost", NULL, &hints, NULL);
		if (ai && ai->ai_canonname)
			ptr = ai->ai_canonname;
		else
			ptr = "localhost.localdomain";

		priv->response = generate_response (priv->challenge, ptr, sasl->service_name,
						    sasl->service->url->user,
						    sasl->service->url->passwd);
		if (ai)
			camel_freeaddrinfo(ai);
		ret = digest_response (priv->response);
		
		break;
	case STATE_FINAL:
		if (token->len)
			tokens = g_strndup (token->data, token->len);
		else
			tokens = NULL;
		
		if (!tokens || !*tokens) {
			g_free (tokens);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Server response did not contain authorization data\n"));
			return NULL;
		}
		
		rspauth = g_new0 (struct _param, 1);
		
		ptr = tokens;
		rspauth->name = decode_token (&ptr);
		if (*ptr == '=') {
			ptr++;
			rspauth->value = decode_value (&ptr);
		}
		g_free (tokens);
		
		if (!rspauth->value) {
			g_free (rspauth->name);
			g_free (rspauth);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Server response contained incomplete authorization data\n"));
			return NULL;
		}
		
		compute_response (priv->response, sasl->service->url->passwd, FALSE, out);
		if (memcmp (out, rspauth->value, 32) != 0) {
			g_free (rspauth->name);
			g_free (rspauth->value);
			g_free (rspauth);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_CANT_AUTHENTICATE,
					      _("Server response does not match\n"));
			sasl->authenticated = TRUE;
			
			return NULL;
		}
		
		g_free (rspauth->name);
		g_free (rspauth->value);
		g_free (rspauth);
		
		ret = g_byte_array_new ();
		
		sasl->authenticated = TRUE;
	default:
		break;
	}
	
	priv->state++;
	
	return ret;
}
