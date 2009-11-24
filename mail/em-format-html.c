/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#include <glib.h>
#include <gtk/gtk.h>
#ifdef G_OS_WIN32
/* Work around 'DATADIR' and 'interface' lossage in <windows.h> */
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#undef interface
#endif

#include <libedataserver/e-data-server-util.h>	/* for e_utf8_strftime, what about e_time_format_time? */
#include <libedataserver/e-time-utils.h>
#include "e-util/e-datetime-format.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util.h"

#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-embedded.h>
#include <gtkhtml/gtkhtml-stream.h>

#include <glib/gi18n.h>

#include <camel/camel-iconv.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter.h>
#include <camel/camel-mime-filter-tohtml.h>
#include <camel/camel-mime-filter-enriched.h>
#include <camel/camel-mime-filter-basic.h>
#include <camel/camel-gpg-context.h>
#include <camel/camel-cipher-context.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-url.h>
#include <camel/camel-stream-fs.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-http-stream.h>
#include <camel/camel-data-cache.h>
#include <camel/camel-file-utils.h>

#include <libedataserver/e-msgport.h>

#include "mail-component.h"
#include "mail-config.h"
#include "mail-mt.h"

#include "em-format-html.h"
#include "em-html-stream.h"
#include "em-utils.h"

#define d(x)

#define EFM_MESSAGE_START_ANAME "evolution#message#start"
#define EFH_MESSAGE_START "<A name=\"" EFM_MESSAGE_START_ANAME "\"></A>"

struct _EMFormatHTMLCache {
	CamelMultipart *textmp;

	gchar partid[1];
};

struct _EMFormatHTMLPrivate {
	CamelMimeMessage *last_part;	/* not reffed, DO NOT dereference */
	volatile gint format_id;		/* format thread id */
	guint format_timeout_id;
	struct _format_msg *format_timeout_msg;

	/* Table that re-maps text parts into a mutlipart/mixed, EMFormatHTMLCache * */
	GHashTable *text_inline_parts;

	EDList pending_jobs;
	GMutex *lock;
};

static void efh_url_requested(GtkHTML *html, const gchar *url, GtkHTMLStream *handle, EMFormatHTML *efh);
static gboolean efh_object_requested(GtkHTML *html, GtkHTMLEmbedded *eb, EMFormatHTML *efh);
static void efh_gtkhtml_destroy(GtkHTML *html, EMFormatHTML *efh);

static void efh_format_message(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info);

static void efh_format_clone(EMFormat *emf, CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, EMFormat *emfsource);
static void efh_format_error(EMFormat *emf, CamelStream *stream, const gchar *txt);
static void efh_format_source(EMFormat *, CamelStream *, CamelMimePart *);
static void efh_format_attachment(EMFormat *, CamelStream *, CamelMimePart *, const gchar *, const EMFormatHandler *);
static void efh_format_secure(EMFormat *emf, CamelStream *stream, CamelMimePart *part, CamelCipherValidity *valid);
static gboolean efh_busy(EMFormat *);

static void efh_builtin_init(EMFormatHTMLClass *efhc);

static void efh_write_image(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri);

static EMFormatClass *efh_parent;
static CamelDataCache *emfh_http_cache;

#define EMFH_HTTP_CACHE_PATH "http"

static void
efh_free_cache(struct _EMFormatHTMLCache *efhc)
{
	if (efhc->textmp)
		camel_object_unref(efhc->textmp);
	g_free(efhc);
}

static void
efh_init(GObject *o)
{
	EMFormatHTML *efh = (EMFormatHTML *)o;

	efh->priv = g_malloc0(sizeof(*efh->priv));

	e_dlist_init(&efh->pending_object_list);
	e_dlist_init(&efh->priv->pending_jobs);
	efh->priv->lock = g_mutex_new();
	efh->priv->format_id = -1;
	efh->priv->text_inline_parts = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) efh_free_cache);

	efh->html = (GtkHTML *)gtk_html_new();
	gtk_html_set_blocking(efh->html, FALSE);
	gtk_html_set_caret_first_focus_anchor (efh->html, EFM_MESSAGE_START_ANAME);
	g_object_ref_sink(efh->html);

	gtk_html_set_default_content_type(efh->html, "text/html; charset=utf-8");
	gtk_html_set_editable(efh->html, FALSE);

	g_signal_connect(efh->html, "destroy", G_CALLBACK(efh_gtkhtml_destroy), efh);
	g_signal_connect(efh->html, "url_requested", G_CALLBACK(efh_url_requested), efh);
	g_signal_connect(efh->html, "object_requested", G_CALLBACK(efh_object_requested), efh);

	efh->body_colour = 0xeeeeee;
	efh->header_colour = 0xeeeeee;
	efh->text_colour = 0;
	efh->frame_colour = 0x3f3f3f;
	efh->content_colour = 0xffffff;
	efh->text_html_flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_NL | CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES
		| CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
	efh->show_icon = TRUE;
	efh->state = EM_FORMAT_HTML_STATE_NONE;
}

static void
efh_gtkhtml_destroy(GtkHTML *html, EMFormatHTML *efh)
{
	if (efh->priv->format_timeout_id != 0) {
		g_source_remove(efh->priv->format_timeout_id);
		efh->priv->format_timeout_id = 0;
		mail_msg_unref(efh->priv->format_timeout_msg);
		efh->priv->format_timeout_msg = NULL;
	}

	/* This probably works ... */
	if (efh->priv->format_id != -1)
		mail_msg_cancel(efh->priv->format_id);

	if (efh->html) {
		g_object_unref(efh->html);
		efh->html = NULL;
	}
}

static struct _EMFormatHTMLCache *
efh_insert_cache(EMFormatHTML *efh, const gchar *partid)
{
	struct _EMFormatHTMLCache *efhc;

	efhc = g_malloc0(sizeof(*efh) + strlen(partid));
	strcpy(efhc->partid, partid);
	g_hash_table_insert(efh->priv->text_inline_parts, efhc->partid, efhc);

	return efhc;
}

static void
efh_finalise(GObject *o)
{
	EMFormatHTML *efh = (EMFormatHTML *)o;

	/* FIXME: check for leaked stuff */

	em_format_html_clear_pobject(efh);

	efh_gtkhtml_destroy(efh->html, efh);

	g_hash_table_destroy(efh->priv->text_inline_parts);

	g_free(efh->priv);

	((GObjectClass *)efh_parent)->finalize(o);
}

static void
efh_base_init(EMFormatHTMLClass *efhklass)
{
	efh_builtin_init(efhklass);
}

static void
efh_class_init(GObjectClass *klass)
{
	((EMFormatClass *)klass)->format_clone = efh_format_clone;
	((EMFormatClass *)klass)->format_error = efh_format_error;
	((EMFormatClass *)klass)->format_source = efh_format_source;
	((EMFormatClass *)klass)->format_attachment = efh_format_attachment;
	((EMFormatClass *)klass)->format_secure = efh_format_secure;
	((EMFormatClass *)klass)->busy = efh_busy;

	klass->finalize = efh_finalise;
}

GType
em_format_html_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMFormatHTMLClass),
			(GBaseInitFunc)efh_base_init, NULL,
			(GClassInitFunc)efh_class_init,
			NULL, NULL,
			sizeof(EMFormatHTML), 0,
			(GInstanceInitFunc)efh_init
		};
		const gchar *base_directory = e_get_user_data_dir ();
		gchar *path;

		/* Trigger creation of mail component. */
		mail_component_peek ();

		efh_parent = g_type_class_ref(em_format_get_type());
		type = g_type_register_static(em_format_get_type(), "EMFormatHTML", &info, 0);

		/* cache expiry - 2 hour access, 1 day max */
		path = alloca(strlen(base_directory)+16);
		sprintf(path, "%s/cache", base_directory);
		emfh_http_cache = camel_data_cache_new(path, 0, NULL);
		if (emfh_http_cache) {
			camel_data_cache_set_expire_age(emfh_http_cache, 24*60*60);
			camel_data_cache_set_expire_access(emfh_http_cache, 2*60*60);
		}
	}

	return type;
}

EMFormatHTML *em_format_html_new(void)
{
	EMFormatHTML *efh;

	efh = g_object_new(em_format_html_get_type(), NULL);

	return efh;
}

/* force loading of http images */
void em_format_html_load_http(EMFormatHTML *emfh)
{
	if (emfh->load_http == MAIL_CONFIG_HTTP_ALWAYS)
		return;

	/* This will remain set while we're still rendering the same message, then it wont be */
	emfh->load_http_now = TRUE;
	d(printf("redrawing with images forced on\n"));
	em_format_redraw((EMFormat *)emfh);
}

void
em_format_html_set_load_http(EMFormatHTML *emfh, gint style)
{
	if (emfh->load_http != style) {
		emfh->load_http = style;
		em_format_redraw((EMFormat *)emfh);
	}
}

void
em_format_html_set_mark_citations(EMFormatHTML *emfh, gint state, guint32 citation_colour)
{
	if (emfh->mark_citations ^ state || emfh->citation_colour != citation_colour) {
		emfh->mark_citations = state;
		emfh->citation_colour = citation_colour;

		if (state)
			emfh->text_html_flags |= CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;
		else
			emfh->text_html_flags &= ~CAMEL_MIME_FILTER_TOHTML_MARK_CITATION;

		em_format_redraw((EMFormat *)emfh);
	}
}

CamelMimePart *
em_format_html_file_part(EMFormatHTML *efh, const gchar *mime_type, const gchar *filename)
{
	CamelMimePart *part;
	CamelStream *stream;
	CamelDataWrapper *dw;
	gchar *basename;

	stream = camel_stream_fs_new_with_name(filename, O_RDONLY, 0);
	if (stream == NULL)
		return NULL;

	part = camel_mime_part_new();
	dw = camel_data_wrapper_new();
	camel_data_wrapper_construct_from_stream(dw, stream);
	camel_object_unref(stream);
	if (mime_type)
		camel_data_wrapper_set_mime_type(dw, mime_type);
	part = camel_mime_part_new();
	camel_medium_set_content_object((CamelMedium *)part, dw);
	camel_object_unref(dw);
	basename = g_path_get_basename (filename);
	camel_mime_part_set_filename(part, basename);
	g_free (basename);

	return part;
}

/* all this api is a pain in the bum ... */

EMFormatHTMLPObject *
em_format_html_add_pobject(EMFormatHTML *efh, gsize size, const gchar *classid, CamelMimePart *part, EMFormatHTMLPObjectFunc func)
{
	EMFormatHTMLPObject *pobj;

	if (size < sizeof(EMFormatHTMLPObject)) {
		g_warning ("size is less than the size of EMFormatHTMLPObject\n");
		size = sizeof(EMFormatHTMLPObject);
	}

	pobj = g_malloc0(size);
	if (classid)
		pobj->classid = g_strdup(classid);
	else
		pobj->classid = g_strdup_printf("e-object:///%s", ((EMFormat *)efh)->part_id->str);

	pobj->format = efh;
	pobj->func = func;
	pobj->part = part;

	e_dlist_addtail(&efh->pending_object_list, (EDListNode *)pobj);

	return pobj;
}

EMFormatHTMLPObject *
em_format_html_find_pobject(EMFormatHTML *emf, const gchar *classid)
{
	EMFormatHTMLPObject *pw;

	pw = (EMFormatHTMLPObject *)emf->pending_object_list.head;
	while (pw->next) {
		if (!strcmp(pw->classid, classid))
			return pw;
		pw = pw->next;
	}

	return NULL;
}

EMFormatHTMLPObject *
em_format_html_find_pobject_func(EMFormatHTML *emf, CamelMimePart *part, EMFormatHTMLPObjectFunc func)
{
	EMFormatHTMLPObject *pw;

	pw = (EMFormatHTMLPObject *)emf->pending_object_list.head;
	while (pw->next) {
		if (pw->func == func && pw->part == part)
			return pw;
		pw = pw->next;
	}

	return NULL;
}

void
em_format_html_remove_pobject(EMFormatHTML *emf, EMFormatHTMLPObject *pobject)
{
	e_dlist_remove((EDListNode *)pobject);
	if (pobject->free)
		pobject->free(pobject);
	g_free(pobject->classid);
	g_free(pobject);
}

void
em_format_html_clear_pobject(EMFormatHTML *emf)
{
	d(printf("clearing pending objects\n"));
	while (!e_dlist_empty(&emf->pending_object_list))
		em_format_html_remove_pobject(emf, (EMFormatHTMLPObject *)emf->pending_object_list.head);
}

struct _EMFormatHTMLJob *
em_format_html_job_new(EMFormatHTML *emfh, void (*callback)(struct _EMFormatHTMLJob *job, gint cancelled), gpointer data)
{
	struct _EMFormatHTMLJob *job = g_malloc0(sizeof(*job));

	job->format = emfh;
	job->puri_level = ((EMFormat *)emfh)->pending_uri_level;
	job->callback = callback;
	job->u.data = data;
	if (((EMFormat *)emfh)->base)
		job->base = camel_url_copy(((EMFormat *)emfh)->base);

	return job;
}

void
em_format_html_job_queue(EMFormatHTML *emfh, struct _EMFormatHTMLJob *job)
{
	g_mutex_lock(emfh->priv->lock);
	e_dlist_addtail(&emfh->priv->pending_jobs, (EDListNode *)job);
	g_mutex_unlock(emfh->priv->lock);
}

/* ********************************************************************** */

static void emfh_getpuri(struct _EMFormatHTMLJob *job, gint cancelled)
{
	d(printf(" running getpuri task\n"));
	if (!cancelled)
		job->u.puri->func((EMFormat *)job->format, job->stream, job->u.puri);
}

static void emfh_gethttp(struct _EMFormatHTMLJob *job, gint cancelled)
{
	CamelStream *cistream = NULL, *costream = NULL, *instream = NULL;
	CamelURL *url;
	CamelContentType *content_type;
	CamelHttpStream *tmp_stream;
	gssize n, total = 0, pc_complete = 0, nread = 0;
	gchar buffer[1500];
	const gchar *length;

	if (cancelled
	    || (url = camel_url_new(job->u.uri, NULL)) == NULL)
		goto badurl;

	d(printf(" running load uri task: %s\n", job->u.uri));

	if (emfh_http_cache)
		instream = cistream = camel_data_cache_get(emfh_http_cache, EMFH_HTTP_CACHE_PATH, job->u.uri, NULL);

	if (instream == NULL) {
		gchar *proxy;

		if (!(job->format->load_http_now
		      || job->format->load_http == MAIL_CONFIG_HTTP_ALWAYS
		      || (job->format->load_http == MAIL_CONFIG_HTTP_SOMETIMES
			  && em_utils_in_addressbook((CamelInternetAddress *)camel_mime_message_get_from(job->format->format.message), FALSE)))) {
			/* TODO: Ideally we would put the http requests into another queue and only send them out
			   if the user selects 'load images', when they do.  The problem is how to maintain this
			   state with multiple renderings, and how to adjust the thread dispatch/setup routine to handle it */
			camel_url_free(url);
			goto done;
		}

		instream = camel_http_stream_new(CAMEL_HTTP_METHOD_GET, ((EMFormat *)job->format)->session, url);
		camel_http_stream_set_user_agent((CamelHttpStream *)instream, "CamelHttpStream/1.0 Evolution/" VERSION);
		proxy = em_utils_get_proxy_uri (job->u.uri);
		if (proxy) {
			camel_http_stream_set_proxy ((CamelHttpStream *)instream, proxy);
			g_free (proxy);
		}
		camel_operation_start(NULL, _("Retrieving `%s'"), job->u.uri);
		tmp_stream = (CamelHttpStream *)instream;
		content_type = camel_http_stream_get_content_type(tmp_stream);
		length = camel_header_raw_find(&tmp_stream->headers, "Content-Length", NULL);
		d(printf("  Content-Length: %s\n", length));
		if (length != NULL)
			total = atoi(length);
		camel_content_type_unref(content_type);
	} else
		camel_operation_start_transient(NULL, _("Retrieving `%s'"), job->u.uri);

	camel_url_free(url);

	if (instream == NULL)
		goto done;

	if (emfh_http_cache != NULL && cistream == NULL)
		costream = camel_data_cache_add(emfh_http_cache, EMFH_HTTP_CACHE_PATH, job->u.uri, NULL);

	do {
		if (camel_operation_cancel_check (NULL)) {
			n = -1;
			break;
		}
		/* FIXME: progress reporting in percentage, can we get the length always?  do we care? */
		n = camel_stream_read(instream, buffer, sizeof (buffer));
		if (n > 0) {
			nread += n;
			/* If we didn't get a valid Content-Length header, do not try to calculate percentage */
			if (total != 0) {
				pc_complete = ((nread * 100) / total);
				camel_operation_progress(NULL, pc_complete);
			}
			d(printf("  read %d bytes\n", n));
			if (costream && camel_stream_write (costream, buffer, n) == -1) {
				n = -1;
				break;
			}

			camel_stream_write(job->stream, buffer, n);
		}
	} while (n>0);

	/* indicates success */
	if (n == 0)
		camel_stream_close(job->stream);

	if (costream) {
		/* do not store broken files in a cache */
		if (n != 0)
			camel_data_cache_remove(emfh_http_cache, EMFH_HTTP_CACHE_PATH, job->u.uri, NULL);
		camel_object_unref(costream);
	}

	camel_object_unref(instream);
done:
	camel_operation_end(NULL);
badurl:
	g_free(job->u.uri);
}

/* ********************************************************************** */

static void
efh_url_requested(GtkHTML *html, const gchar *url, GtkHTMLStream *handle, EMFormatHTML *efh)
{
	EMFormatPURI *puri;
	struct _EMFormatHTMLJob *job = NULL;

	d(printf("url requested, html = %p, url '%s'\n", html, url));

	puri = em_format_find_visible_puri((EMFormat *)efh, url);
	if (puri) {
		CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)puri->part);
		CamelContentType *ct = dw?dw->mime_type:NULL;

		/* GtkHTML only handles text and images.
		   application/octet-stream parts are the only ones
		   which are snooped for other content.  So only try
		   to pass these to it - any other types are badly
		   formed or intentionally malicious emails.  They
		   will still show as attachments anyway */

		if (ct && (camel_content_type_is(ct, "text", "*")
			   || camel_content_type_is(ct, "image", "*")
			   || camel_content_type_is(ct, "application", "octet-stream"))) {
			puri->use_count++;

			d(printf(" adding puri job\n"));
			job = em_format_html_job_new(efh, emfh_getpuri, puri);
		} else {
			d(printf(" part is unknown type '%s', not using\n", ct?camel_content_type_format(ct):"<unset>"));
			gtk_html_stream_close(handle, GTK_HTML_STREAM_ERROR);
		}
	} else if (g_ascii_strncasecmp(url, "http:", 5) == 0 || g_ascii_strncasecmp(url, "https:", 6) == 0) {
		d(printf(" adding job, get %s\n", url));
		job = em_format_html_job_new(efh, emfh_gethttp, g_strdup(url));
	} else if  (g_ascii_strncasecmp(url, "/", 1) == 0) {
		gchar *data = NULL;
		gsize length = 0;
		gboolean status;

		status = g_file_get_contents (url, &data, &length, NULL);
		if (status)
			gtk_html_stream_write (handle, data, length);

		gtk_html_stream_close(handle, status? GTK_HTML_STREAM_OK : GTK_HTML_STREAM_ERROR);
		g_free (data);
	} else {
		d(printf("HTML Includes reference to unknown uri '%s'\n", url));
		gtk_html_stream_close(handle, GTK_HTML_STREAM_ERROR);
	}

	if (job) {
		job->stream = em_html_stream_new(html, handle);
		em_format_html_job_queue(efh, job);
	}
}

static gboolean
efh_object_requested(GtkHTML *html, GtkHTMLEmbedded *eb, EMFormatHTML *efh)
{
	EMFormatHTMLPObject *pobject;
	gint res = FALSE;

	if (eb->classid == NULL)
		return FALSE;

	pobject = em_format_html_find_pobject(efh, eb->classid);
	if (pobject) {
		/* This stops recursion of the part */
		e_dlist_remove((EDListNode *)pobject);
		res = pobject->func(efh, eb, pobject);
		e_dlist_addhead(&efh->pending_object_list, (EDListNode *)pobject);
	} else {
		d(printf("HTML Includes reference to unknown object '%s'\n", eb->classid));
	}

	return res;
}

/* ********************************************************************** */
#include "em-inline-filter.h"
#include <camel/camel-stream-null.h>

/* FIXME: This is duplicated in em-format-html-display, should be exported or in security module */
static const struct {
	const gchar *icon, *shortdesc;
} smime_sign_table[5] = {
	{ "stock_signature-bad", N_("Unsigned") },
	{ "stock_signature-ok", N_("Valid signature") },
	{ "stock_signature-bad", N_("Invalid signature") },
	{ "stock_signature", N_("Valid signature, but cannot verify sender") },
	{ "stock_signature-bad", N_("Signature exists, but need public key") },
};

static const struct {
	const gchar *icon, *shortdesc;
} smime_encrypt_table[4] = {
	{ "stock_lock-broken", N_("Unencrypted") },
	{ "stock_lock", N_("Encrypted, weak"),},
	{ "stock_lock-ok", N_("Encrypted") },
	{ "stock_lock-ok", N_("Encrypted, strong") },
};

static const gchar *smime_sign_colour[4] = {
	"", " bgcolor=\"#88bb88\"", " bgcolor=\"#bb8888\"", " bgcolor=\"#e8d122\""
};

/* TODO: this could probably be virtual on em-format-html
   then we only need one version of each type handler */
static void
efh_format_secure(EMFormat *emf, CamelStream *stream, CamelMimePart *part, CamelCipherValidity *valid)
{
	efh_parent->format_secure(emf, stream, part, valid);

	/* To explain, if the validity is the same, then we are the
	   base validity and now have a combined sign/encrypt validity
	   we can display.  Primarily a new verification context is
	   created when we have an embeded message. */
	if (emf->valid == valid
	    && (valid->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE
		|| valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE)) {
		gchar *classid, *iconpath;
		const gchar *icon;
		CamelMimePart *iconpart;

		camel_stream_printf (stream, "<table border=0 width=\"100%%\" cellpadding=3 cellspacing=0%s><tr>",
				     smime_sign_colour[valid->sign.status]);

		classid = g_strdup_printf("smime:///em-format-html/%s/icon/signed", emf->part_id->str);
		camel_stream_printf(stream, "<td valign=\"top\"><img src=\"%s\"></td><td valign=\"top\" width=\"100%%\">", classid);

		if (valid->sign.status != 0)
			icon = smime_sign_table[valid->sign.status].icon;
		else
			icon = smime_encrypt_table[valid->encrypt.status].icon;
		iconpath = e_icon_factory_get_icon_filename(icon, GTK_ICON_SIZE_DIALOG);
		iconpart = em_format_html_file_part((EMFormatHTML *)emf, "image/png", iconpath);
		if (iconpart) {
			(void)em_format_add_puri(emf, sizeof(EMFormatPURI), classid, iconpart, efh_write_image);
			camel_object_unref(iconpart);
		}
		g_free (iconpath);
		g_free(classid);

		if (valid->sign.status != CAMEL_CIPHER_VALIDITY_SIGN_NONE) {
			camel_stream_printf(stream, "%s<br>", _(smime_sign_table[valid->sign.status].shortdesc));
		}

		if (valid->encrypt.status != CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE) {
			camel_stream_printf(stream, "%s<br>", _(smime_encrypt_table[valid->encrypt.status].shortdesc));
		}

		camel_stream_printf(stream, "</td></tr></table>");
	}
}

static void
efh_text_plain(EMFormatHTML *efh, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *html_filter;
	CamelMultipart *mp;
	CamelDataWrapper *dw;
	CamelContentType *type;
	const gchar *format;
	guint32 flags;
	gint i, count, len;
	gchar *meta;
	gboolean is_fallback;
	struct _EMFormatHTMLCache *efhc;

	flags = efh->text_html_flags;

	meta = camel_object_meta_get (part, "EMF-Fallback");
	is_fallback = meta != NULL;
	g_free (meta);

	dw = camel_medium_get_content_object((CamelMedium *)part);

	/* Check for RFC 2646 flowed text. */
	if (camel_content_type_is(dw->mime_type, "text", "plain")
	    && (format = camel_content_type_param(dw->mime_type, "format"))
	    && !g_ascii_strcasecmp(format, "flowed"))
		flags |= CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED;

	/* This scans the text part for inline-encoded data, creates
	   a multipart of all the parts inside it. */

	/* FIXME: We should discard this multipart if it only contains
	   the original text, but it makes this hash lookup more complex */

	/* TODO: We could probably put this in the superclass, since
	   no knowledge of html is required - but this messes with
	   filters a bit.  Perhaps the superclass should just deal with
	   html anyway and be done with it ... */

	efhc = g_hash_table_lookup(efh->priv->text_inline_parts, ((EMFormat *)efh)->part_id->str);
	if (efhc == NULL || (mp = efhc->textmp) == NULL) {
		EMInlineFilter *inline_filter;
		CamelStream *null;
		CamelContentType *ct;

		/* if we had to snoop the part type to get here, then
		 * use that as the base type, yuck */
		if (((EMFormat *)efh)->snoop_mime_type == NULL
		    || (ct = camel_content_type_decode(((EMFormat *)efh)->snoop_mime_type)) == NULL) {
			ct = dw->mime_type;
			camel_content_type_ref(ct);
		}

		null = camel_stream_null_new();
		filtered_stream = camel_stream_filter_new_with_stream(null);
		camel_object_unref(null);
		inline_filter = em_inline_filter_new(camel_mime_part_get_encoding(part), ct);
		camel_stream_filter_add(filtered_stream, (CamelMimeFilter *)inline_filter);
		camel_data_wrapper_write_to_stream(dw, (CamelStream *)filtered_stream);
		camel_stream_close((CamelStream *)filtered_stream);
		camel_object_unref(filtered_stream);

		mp = em_inline_filter_get_multipart(inline_filter);
		if (efhc == NULL)
			efhc = efh_insert_cache(efh, ((EMFormat *)efh)->part_id->str);
		efhc->textmp = mp;

		camel_object_unref(inline_filter);
		camel_content_type_unref(ct);
	}

	filtered_stream = camel_stream_filter_new_with_stream(stream);
	html_filter = camel_mime_filter_tohtml_new(flags, efh->citation_colour);
	camel_stream_filter_add(filtered_stream, html_filter);
	camel_object_unref(html_filter);

	/* We handle our made-up multipart here, so we don't recursively call ourselves */

	len = ((EMFormat *)efh)->part_id->len;
	count = camel_multipart_get_number(mp);
	for (i=0;i<count;i++) {
		CamelMimePart *newpart = camel_multipart_get_part(mp, i);

		if (!newpart)
			continue;

		type = camel_mime_part_get_content_type(newpart);
		if (camel_content_type_is (type, "text", "*") && (is_fallback || !camel_content_type_is (type, "text", "calendar"))) {
			camel_stream_printf (stream,
					"<div style=\"border: solid #%06x 1px; background-color: #%06x; padding: 10px; color: #%06x;\">\n",
					     efh->frame_colour & 0xffffff, efh->content_colour & 0xffffff, efh->text_colour & 0xffffff);
			camel_stream_write_string(stream, "<tt>\n" EFH_MESSAGE_START);
			em_format_format_text((EMFormat *)efh, (CamelStream *)filtered_stream, (CamelDataWrapper *)newpart);
			camel_stream_flush((CamelStream *)filtered_stream);
			camel_stream_write_string(stream, "</tt>\n");
			camel_stream_write_string(stream, "</div>\n");
		} else {
			g_string_append_printf(((EMFormat *)efh)->part_id, ".inline.%d", i);
			em_format_part((EMFormat *)efh, stream, newpart);
			g_string_truncate(((EMFormat *)efh)->part_id, len);
		}
	}

	camel_object_unref(filtered_stream);
}

static void
efh_text_enriched(EMFormatHTML *efh, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *enriched;
	CamelDataWrapper *dw;
	guint32 flags = 0;

	dw = camel_medium_get_content_object((CamelMedium *)part);

	if (!strcmp(info->mime_type, "text/richtext")) {
		flags = CAMEL_MIME_FILTER_ENRICHED_IS_RICHTEXT;
		camel_stream_write_string( stream, "\n<!-- text/richtext -->\n");
	} else {
		camel_stream_write_string( stream, "\n<!-- text/enriched -->\n");
	}

	enriched = camel_mime_filter_enriched_new(flags);
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add(filtered_stream, enriched);
	camel_object_unref(enriched);

	camel_stream_printf (stream,
			     "<div style=\"border: solid #%06x 1px; background-color: #%06x; padding: 10px; color: #%06x;\">\n" EFH_MESSAGE_START,
			     efh->frame_colour & 0xffffff, efh->content_colour & 0xffffff, efh->text_colour & 0xffffff);

	em_format_format_text((EMFormat *)efh, (CamelStream *)filtered_stream, (CamelDataWrapper *)part);

	camel_object_unref(filtered_stream);
	camel_stream_write_string(stream, "</div>");
}

static void
efh_write_text_html(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri)
{
#if d(!)0
	CamelStream *out;
	gint fd;
	CamelDataWrapper *dw;

	fd = dup(STDOUT_FILENO);
	out = camel_stream_fs_new_with_fd(fd);
	printf("writing text content to frame '%s'\n", puri->cid);
	dw = camel_medium_get_content_object(puri->part);
	if (dw)
		camel_data_wrapper_write_to_stream(dw, out);
	camel_object_unref(out);
#endif
	em_format_format_text(emf, stream, (CamelDataWrapper *)puri->part);
}

static void
efh_text_html(EMFormatHTML *efh, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	const gchar *location;
	/* This is set but never used for anything */
	EMFormatPURI *puri;
	gchar *cid = NULL;

	camel_stream_printf (stream,
			     "<div style=\"border: solid #%06x 1px; background-color: #%06x; color: #%06x;\">\n"
			     "<!-- text/html -->\n" EFH_MESSAGE_START,
			     efh->frame_colour & 0xffffff, efh->content_colour & 0xffffff, efh->text_colour & 0xffffff);

	/* TODO: perhaps we don't need to calculate this anymore now base is handled better */
	/* calculate our own location string so add_puri doesn't do it
	   for us. our iframes are special cases, we need to use the
	   proper base url to access them, but other children parts
	   shouldn't blindly inherit the container's location. */
	location = camel_mime_part_get_content_location(part);
	if (location == NULL) {
		if (((EMFormat *)efh)->base)
			cid = camel_url_to_string(((EMFormat *)efh)->base, 0);
		else
			cid = g_strdup(((EMFormat *)efh)->part_id->str);
	} else {
		if (strchr(location, ':') == NULL && ((EMFormat *)efh)->base != NULL) {
			CamelURL *uri;

			uri = camel_url_new_with_base(((EMFormat *)efh)->base, location);
			cid = camel_url_to_string(uri, 0);
			camel_url_free(uri);
		} else {
			cid = g_strdup(location);
		}
	}

	puri = em_format_add_puri((EMFormat *)efh, sizeof(EMFormatPURI), cid, part, efh_write_text_html);
	d(printf("adding iframe, location %s\n", cid));
	camel_stream_printf(stream,
			    "<iframe src=\"%s\" frameborder=0 scrolling=no>could not get %s</iframe>\n"
			    "</div>\n",
			    cid, cid);
	g_free(cid);
}

/* This is a lot of code for something useless ... */
static void
efh_message_external(EMFormatHTML *efh, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	CamelContentType *type;
	const gchar *access_type;
	gchar *url = NULL, *desc = NULL;

	if (!part) {
		camel_stream_printf(stream, _("Unknown external-body part."));
		return;
	}

	/* needs to be cleaner */
	type = camel_mime_part_get_content_type(part);
	access_type = camel_content_type_param (type, "access-type");
	if (!access_type) {
		camel_stream_printf(stream, _("Malformed external-body part."));
		return;
	}

	if (!g_ascii_strcasecmp(access_type, "ftp") ||
	    !g_ascii_strcasecmp(access_type, "anon-ftp")) {
		const gchar *name, *site, *dir, *mode;
		gchar *path;
		gchar ftype[16];

		name = camel_content_type_param (type, "name");
		site = camel_content_type_param (type, "site");
		dir = camel_content_type_param (type, "directory");
		mode = camel_content_type_param (type, "mode");
		if (name == NULL || site == NULL)
			goto fail;

		/* Generate the path. */
		if (dir)
			path = g_strdup_printf("/%s/%s", *dir=='/'?dir+1:dir, name);
		else
			path = g_strdup_printf("/%s", *name=='/'?name+1:name);

		if (mode && *mode)
			sprintf(ftype, ";type=%c",  *mode);
		else
			ftype[0] = 0;

		url = g_strdup_printf ("ftp://%s%s%s", site, path, ftype);
		g_free (path);
		desc = g_strdup_printf (_("Pointer to FTP site (%s)"), url);
	} else if (!g_ascii_strcasecmp (access_type, "local-file")) {
		const gchar *name, *site;

		name = camel_content_type_param (type, "name");
		site = camel_content_type_param (type, "site");
		if (name == NULL)
			goto fail;

		url = g_filename_to_uri (name, NULL, NULL);
		if (site)
			desc = g_strdup_printf(_("Pointer to local file (%s) valid at site \"%s\""), name, site);
		else
			desc = g_strdup_printf(_("Pointer to local file (%s)"), name);
	} else if (!g_ascii_strcasecmp (access_type, "URL")) {
		const gchar *urlparam;
		gchar *s, *d;

		/* RFC 2017 */

		urlparam = camel_content_type_param (type, "url");
		if (urlparam == NULL)
			goto fail;

		/* For obscure MIMEy reasons, the URL may be split into words */
		url = g_strdup (urlparam);
		s = d = url;
		while (*s) {
			/* FIXME: use camel_isspace */
			if (!isspace ((guchar)*s))
				*d++ = *s;
			s++;
		}
		*d = 0;
		desc = g_strdup_printf (_("Pointer to remote data (%s)"), url);
	} else
		goto fail;

	camel_stream_printf(stream, "<a href=\"%s\">%s</a>", url, desc);
	g_free(url);
	g_free(desc);

	return;

fail:
	camel_stream_printf(stream, _("Pointer to unknown external data (\"%s\" type)"), access_type);
}

static void
efh_message_deliverystatus(EMFormatHTML *efh, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *html_filter;
	guint32 rgb = 0x737373;

	/* Yuck, this is copied from efh_text_plain */
	camel_stream_printf (stream,
			     "<div style=\"border: solid #%06x 1px; background-color: #%06x; padding: 10px; color: #%06x;\">\n",
			     efh->frame_colour & 0xffffff, efh->content_colour & 0xffffff, efh->text_colour & 0xffffff);

	filtered_stream = camel_stream_filter_new_with_stream(stream);
	html_filter = camel_mime_filter_tohtml_new(efh->text_html_flags, rgb);
	camel_stream_filter_add(filtered_stream, html_filter);
	camel_object_unref(html_filter);

	camel_stream_write_string(stream, "<tt>\n" EFH_MESSAGE_START);
	em_format_format_text((EMFormat *)efh, (CamelStream *)filtered_stream, (CamelDataWrapper *)part);
	camel_stream_flush((CamelStream *)filtered_stream);
	camel_stream_write_string(stream, "</tt>\n");

	camel_stream_write_string(stream, "</div>");
}

static void
emfh_write_related(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri)
{
	em_format_format_content(emf, stream, puri->part);
	camel_stream_close(stream);
}

static void
emfh_multipart_related_check(struct _EMFormatHTMLJob *job, gint cancelled)
{
	struct _EMFormatPURITree *ptree;
	EMFormatPURI *puri, *purin;
	gchar *oldpartid;

	if (cancelled)
		return;

	d(printf(" running multipart/related check task\n"));
	oldpartid = g_strdup(((EMFormat *)job->format)->part_id->str);

	ptree = job->puri_level;
	puri = (EMFormatPURI *)ptree->uri_list.head;
	purin = puri->next;
	while (purin) {
		if (puri->use_count == 0) {
			d(printf("part '%s' '%s' used '%d'\n", puri->uri?puri->uri:"", puri->cid, puri->use_count));
			if (puri->func == emfh_write_related) {
				g_string_printf(((EMFormat *)job->format)->part_id, "%s", puri->part_id);
				em_format_part((EMFormat *)job->format, (CamelStream *)job->stream, puri->part);
			}
			/* else it was probably added by a previous format this loop */
		}
		puri = purin;
		purin = purin->next;
	}

	g_string_printf(((EMFormat *)job->format)->part_id, "%s", oldpartid);
	g_free(oldpartid);
}

/* RFC 2387 */
static void
efh_multipart_related(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);
	CamelMimePart *body_part, *display_part = NULL;
	CamelContentType *content_type;
	const gchar *start;
	gint i, nparts, partidlen, displayid = 0;
	/* puri is set but never used */
	EMFormatPURI *puri;
	struct _EMFormatHTMLJob *job;

	if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	nparts = camel_multipart_get_number(mp);
	content_type = camel_mime_part_get_content_type(part);
	start = camel_content_type_param (content_type, "start");
	if (start && strlen(start)>2) {
		gint len;
		const gchar *cid;

		/* strip <>'s */
		len = strlen (start) - 2;
		start++;

		for (i=0; i<nparts; i++) {
			body_part = camel_multipart_get_part(mp, i);
			cid = camel_mime_part_get_content_id(body_part);

			if (cid && !strncmp(cid, start, len) && strlen(cid) == len) {
				display_part = body_part;
				displayid = i;
				break;
			}
		}
	} else {
		display_part = camel_multipart_get_part(mp, 0);
	}

	if (display_part == NULL) {
		em_format_part_as(emf, stream, part, "multipart/mixed");
		return;
	}

	em_format_push_level(emf);

	partidlen = emf->part_id->len;

	/* queue up the parts for possible inclusion */
	for (i = 0; i < nparts; i++) {
		body_part = camel_multipart_get_part(mp, i);
		if (body_part != display_part) {
			g_string_append_printf(emf->part_id, "related.%d", i);
			puri = em_format_add_puri(emf, sizeof(EMFormatPURI), NULL, body_part, emfh_write_related);
			g_string_truncate(emf->part_id, partidlen);
			d(printf(" part '%s' '%s' added\n", puri->uri?puri->uri:"", puri->cid));
		}
	}

	g_string_append_printf(emf->part_id, "related.%d", displayid);
	em_format_part(emf, stream, display_part);
	g_string_truncate(emf->part_id, partidlen);
	camel_stream_flush(stream);

	/* queue a job to check for un-referenced parts to add as attachments */
	job = em_format_html_job_new((EMFormatHTML *)emf, emfh_multipart_related_check, NULL);
	job->stream = stream;
	camel_object_ref(stream);
	em_format_html_job_queue((EMFormatHTML *)emf, job);

	em_format_pull_level(emf);
}

static void
efh_write_image(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri)
{
	CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)puri->part);

	d(printf("writing image '%s'\n", puri->cid));
	camel_data_wrapper_decode_to_stream(dw, stream);
	camel_stream_close(stream);
}

static void
efh_image(EMFormatHTML *efh, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	EMFormatPURI *puri;

	puri = em_format_add_puri((EMFormat *)efh, sizeof(EMFormatPURI), NULL, part, efh_write_image);
	d(printf("adding image '%s'\n", puri->cid));
	camel_stream_printf(stream, "<img hspace=10 vspace=10 src=\"%s\">", puri->cid);
}

static EMFormatHandler type_builtin_table[] = {
	{ (gchar *) "image/gif", (EMFormatFunc)efh_image },
	{ (gchar *) "image/jpeg", (EMFormatFunc)efh_image },
	{ (gchar *) "image/png", (EMFormatFunc)efh_image },
	{ (gchar *) "image/x-png", (EMFormatFunc)efh_image },
	{ (gchar *) "image/tiff", (EMFormatFunc)efh_image },
	{ (gchar *) "image/x-bmp", (EMFormatFunc)efh_image },
	{ (gchar *) "image/bmp", (EMFormatFunc)efh_image },
	{ (gchar *) "image/svg", (EMFormatFunc)efh_image },
	{ (gchar *) "image/x-cmu-raster", (EMFormatFunc)efh_image },
	{ (gchar *) "image/x-ico", (EMFormatFunc)efh_image },
	{ (gchar *) "image/x-portable-anymap", (EMFormatFunc)efh_image },
	{ (gchar *) "image/x-portable-bitmap", (EMFormatFunc)efh_image },
	{ (gchar *) "image/x-portable-graymap", (EMFormatFunc)efh_image },
	{ (gchar *) "image/x-portable-pixmap", (EMFormatFunc)efh_image },
	{ (gchar *) "image/x-xpixmap", (EMFormatFunc)efh_image },
	{ (gchar *) "text/enriched", (EMFormatFunc)efh_text_enriched },
	{ (gchar *) "text/plain", (EMFormatFunc)efh_text_plain },
	{ (gchar *) "text/html", (EMFormatFunc)efh_text_html },
	{ (gchar *) "text/richtext", (EMFormatFunc)efh_text_enriched },
	{ (gchar *) "text/*", (EMFormatFunc)efh_text_plain },
	{ (gchar *) "message/external-body", (EMFormatFunc)efh_message_external },
	{ (gchar *) "message/delivery-status", (EMFormatFunc)efh_message_deliverystatus },
	{ (gchar *) "multipart/related", (EMFormatFunc)efh_multipart_related },

	/* This is where one adds those busted, non-registered types,
	   that some idiot mailer writers out there decide to pull out
	   of their proverbials at random. */

	{ (gchar *) "image/jpg", (EMFormatFunc)efh_image },
	{ (gchar *) "image/pjpeg", (EMFormatFunc)efh_image },

	/* special internal types */

	{ (gchar *) "x-evolution/message/rfc822", (EMFormatFunc)efh_format_message }
};

static void
efh_builtin_init(EMFormatHTMLClass *efhc)
{
	gint i;

	for (i=0;i<sizeof(type_builtin_table)/sizeof(type_builtin_table[0]);i++)
		em_format_class_add_handler((EMFormatClass *)efhc, &type_builtin_table[i]);
}

/* ********************************************************************** */

/* Sigh, this is so we have a cancellable, async rendering thread */
struct _format_msg {
	MailMsg base;

	EMFormatHTML *format;
	EMFormat *format_source;
	EMHTMLStream *estream;
	CamelFolder *folder;
	gchar *uid;
	CamelMimeMessage *message;
};

static gchar *
efh_format_desc (struct _format_msg *m)
{
	return g_strdup(_("Formatting message"));
}

static void
efh_format_exec (struct _format_msg *m)
{
	struct _EMFormatHTMLJob *job;
	struct _EMFormatPURITree *puri_level;
	gint cancelled = FALSE;
	CamelURL *base;

	if (m->format->html == NULL)
		return;

	camel_stream_printf((CamelStream *)m->estream,
			    "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<html>\n"
			    "<head>\n<meta name=\"generator\" content=\"Evolution Mail Component\">\n</head>\n"
			    "<body bgcolor =\"#%06x\" text=\"#%06x\" marginwidth=6 marginheight=6>\n",
			    m->format->body_colour & 0xffffff,
			    m->format->header_colour & 0xffffff);

	/* <insert top-header stuff here> */

	if (((EMFormat *)m->format)->mode == EM_FORMAT_SOURCE) {
		em_format_format_source((EMFormat *)m->format, (CamelStream *)m->estream, (CamelMimePart *)m->message);
	} else {
		const EMFormatHandler *handle;

		handle = em_format_find_handler((EMFormat *)m->format, "x-evolution/message/prefix");
		if (handle)
			handle->handler((EMFormat *)m->format, (CamelStream *)m->estream, (CamelMimePart *)m->message, handle);
		handle = em_format_find_handler((EMFormat *)m->format, "x-evolution/message/rfc822");
		if (handle)
			handle->handler((EMFormat *)m->format, (CamelStream *)m->estream, (CamelMimePart *)m->message, handle);
		handle = em_format_find_handler((EMFormat *)m->format, "x-evolution/message/post-header-closure");
		if (handle && !((EMFormat *)m->format)->print)
			handle->handler((EMFormat *)m->format, (CamelStream *)m->estream, (CamelMimePart *)m->message, handle);

	}

	camel_stream_flush((CamelStream *)m->estream);

	puri_level = ((EMFormat *)m->format)->pending_uri_level;
	base = ((EMFormat *)m->format)->base;

	do {
		/* now dispatch any added tasks ... */
		g_mutex_lock(m->format->priv->lock);
		while ((job = (struct _EMFormatHTMLJob *)e_dlist_remhead(&m->format->priv->pending_jobs))) {
			g_mutex_unlock(m->format->priv->lock);

			/* This is an implicit check to see if the gtkhtml has been destroyed */
			if (!cancelled)
				cancelled = m->format->html == NULL;

			/* Now do an explicit check for user cancellation */
			if (!cancelled)
				cancelled = camel_operation_cancel_check(NULL);

			/* call jobs even if cancelled, so they can clean up resources */
			((EMFormat *)m->format)->pending_uri_level = job->puri_level;
			if (job->base)
				((EMFormat *)m->format)->base = job->base;
			job->callback(job, cancelled);
			((EMFormat *)m->format)->base = base;

			/* clean up the job */
			camel_object_unref(job->stream);
			if (job->base)
				camel_url_free(job->base);
			g_free(job);

			g_mutex_lock(m->format->priv->lock);
		}
		g_mutex_unlock(m->format->priv->lock);

		if (m->estream) {
			/* Closing this base stream can queue more jobs, so we need
			   to check the list again after we've finished */
			d(printf("out of jobs, closing root stream\n"));
			camel_stream_write_string((CamelStream *)m->estream, "</body>\n</html>\n");
			camel_stream_close((CamelStream *)m->estream);
			camel_object_unref(m->estream);
			m->estream = NULL;
		}

		/* e_dlist_empty is atomic and doesn't need locking */
	} while (!e_dlist_empty(&m->format->priv->pending_jobs));

	d(printf("out of jobs, done\n"));

	((EMFormat *)m->format)->pending_uri_level = puri_level;
}

static void
efh_format_done (struct _format_msg *m)
{
	d(printf("formatting finished\n"));

	m->format->load_http_now = FALSE;
	m->format->priv->format_id = -1;
	m->format->state = EM_FORMAT_HTML_STATE_NONE;
	g_signal_emit_by_name(m->format, "complete");
}

static void
efh_format_free (struct _format_msg *m)
{
	d(printf("formatter freed\n"));
	g_object_unref(m->format);
	if (m->estream) {
		camel_stream_close((CamelStream *)m->estream);
		camel_object_unref(m->estream);
	}
	if (m->folder)
		camel_object_unref(m->folder);
	g_free(m->uid);
	if (m->message)
		camel_object_unref(m->message);
	if (m->format_source)
		g_object_unref(m->format_source);
}

static MailMsgInfo efh_format_info = {
	sizeof (struct _format_msg),
	(MailMsgDescFunc) efh_format_desc,
	(MailMsgExecFunc) efh_format_exec,
	(MailMsgDoneFunc) efh_format_done,
	(MailMsgFreeFunc) efh_format_free
};

static gboolean
efh_format_timeout(struct _format_msg *m)
{
	GtkHTMLStream *hstream;
	EMFormatHTML *efh = m->format;
	struct _EMFormatHTMLPrivate *p = efh->priv;

	if (m->format->html == NULL) {
		mail_msg_unref(m);
		return FALSE;
	}

	d(printf("timeout called ...\n"));
	if (p->format_id != -1) {
		d(printf(" still waiting for cancellation to take effect, waiting ...\n"));
		return TRUE;
	}

	g_return_val_if_fail (e_dlist_empty(&p->pending_jobs), FALSE);

	d(printf(" ready to go, firing off format thread\n"));

	/* call super-class to kick it off */
	efh_parent->format_clone((EMFormat *)efh, m->folder, m->uid, m->message, m->format_source);
	em_format_html_clear_pobject(m->format);

	/* FIXME: method off EMFormat? */
	if (((EMFormat *)efh)->valid) {
		camel_cipher_validity_free(((EMFormat *)efh)->valid);
		((EMFormat *)efh)->valid = NULL;
		((EMFormat *)efh)->valid_parent = NULL;
	}

	if (m->message == NULL) {
		hstream = gtk_html_begin(efh->html);
		gtk_html_stream_close(hstream, GTK_HTML_STREAM_OK);
		mail_msg_unref(m);
		p->last_part = NULL;
	} else {
		efh->state = EM_FORMAT_HTML_STATE_RENDERING;

		if (p->last_part != m->message) {
			hstream = gtk_html_begin (efh->html);
			gtk_html_stream_printf (hstream, "<h5>%s</h5>", _("Formatting Message..."));
			gtk_html_stream_close (hstream, GTK_HTML_STREAM_OK);
		}

		hstream = NULL;
		m->estream = (EMHTMLStream *)em_html_stream_new(efh->html, hstream);

		if (p->last_part == m->message) {
			em_html_stream_set_flags (m->estream,
						  GTK_HTML_BEGIN_KEEP_SCROLL | GTK_HTML_BEGIN_KEEP_IMAGES
						  | GTK_HTML_BEGIN_BLOCK_UPDATES | GTK_HTML_BEGIN_BLOCK_IMAGES);
		} else {
			/* clear cache of inline-scanned text parts */
			g_hash_table_remove_all(p->text_inline_parts);

			p->last_part = m->message;
		}

		efh->priv->format_id = m->base.seq;
		mail_msg_unordered_push (m);
	}

	efh->priv->format_timeout_id = 0;
	efh->priv->format_timeout_msg = NULL;

	return FALSE;
}

static void efh_format_clone(EMFormat *emf, CamelFolder *folder, const gchar *uid, CamelMimeMessage *msg, EMFormat *emfsource)
{
	EMFormatHTML *efh = (EMFormatHTML *)emf;
	struct _format_msg *m;

	/* How to sub-class ?  Might need to adjust api ... */

	if (efh->html == NULL)
		return;

	d(printf("efh_format called\n"));
	if (efh->priv->format_timeout_id != 0) {
		d(printf(" timeout for last still active, removing ...\n"));
		g_source_remove(efh->priv->format_timeout_id);
		efh->priv->format_timeout_id = 0;
		mail_msg_unref(efh->priv->format_timeout_msg);
		efh->priv->format_timeout_msg = NULL;
	}

	m = mail_msg_new(&efh_format_info);
	m->format = (EMFormatHTML *)emf;
	g_object_ref(emf);
	m->format_source = emfsource;
	if (emfsource)
		g_object_ref(emfsource);
	m->folder = folder;
	if (folder)
		camel_object_ref(folder);
	m->uid = g_strdup(uid);
	m->message = msg;
	if (msg)
		camel_object_ref(msg);

	if (efh->priv->format_id == -1) {
		d(printf(" idle, forcing format\n"));
		efh_format_timeout(m);
	} else {
		d(printf(" still busy, cancelling and queuing wait\n"));
		/* cancel and poll for completion */
		mail_msg_cancel(efh->priv->format_id);
		efh->priv->format_timeout_msg = m;
		efh->priv->format_timeout_id = g_timeout_add(100, (GSourceFunc)efh_format_timeout, m);
	}
}

static void efh_format_error(EMFormat *emf, CamelStream *stream, const gchar *txt)
{
	gchar *html;

	html = camel_text_to_html (txt, CAMEL_MIME_FILTER_TOHTML_CONVERT_NL|CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_printf(stream, "<em><font color=\"red\">%s</font></em><br>", html);
	g_free(html);
}

static void
efh_format_text_header (EMFormatHTML *emfh, CamelStream *stream, const gchar *label, const gchar *value, guint32 flags)
{
	const gchar *fmt, *html;
	gchar *mhtml = NULL;
	gboolean is_rtl;

	if (value == NULL)
		return;

	while (*value == ' ')
		value++;

	if (!(flags & EM_FORMAT_HTML_HEADER_HTML))
		html = mhtml = camel_text_to_html (value, emfh->text_html_flags, 0);
	else
		html = value;

	is_rtl = gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL;
	if (emfh->simple_headers) {
		fmt = "<b>%s</b>: %s<br>";
	} else {
		if (flags & EM_FORMAT_HTML_HEADER_NOCOLUMNS) {
			if (flags & EM_FORMAT_HEADER_BOLD) {
				fmt = "<tr><td><b>%s:</b> %s</td></tr>";
			} else {
				fmt = "<tr><td>%s: %s</td></tr>";
			}
		} else if (flags & EM_FORMAT_HTML_HEADER_NODEC) {
			if (is_rtl)
				fmt = "<tr><td align=\"right\" valign=\"top\" width=\"100%%\">%2$s</td><th valign=top align=\"left\" nowrap>%1$s<b>&nbsp;</b></th></tr>";
			else
				fmt = "<tr><th align=\"right\" valign=\"top\" nowrap>%s<b>&nbsp;</b></th><td valign=top>%s</td></tr>";
		} else {

			if (flags & EM_FORMAT_HEADER_BOLD) {
				if (is_rtl)
					fmt = "<tr><td align=\"right\" valign=\"top\" width=\"100%%\">%2$s</td><th align=\"left\" nowrap>%1$s:<b>&nbsp;</b></th></tr>";
				else
					fmt = "<tr><th align=\"right\" valign=\"top\" nowrap>%s:<b>&nbsp;</b></th><td>%s</td></tr>";
			} else {
				if (is_rtl)
					fmt = "<tr><td align=\"right\" valign=\"top\" width=\"100%\">%2$s</td><td align=\"left\" nowrap>%1$s:<b>&nbsp;</b></td></tr>";
				else
					fmt = "<tr><td align=\"right\" valign=\"top\" nowrap>%s:<b>&nbsp;</b></td><td>%s</td></tr>";
			}
		}
	}

	camel_stream_printf(stream, fmt, label, html);
	g_free(mhtml);
}

static const gchar *addrspec_hdrs[] = {
	"Sender", "From", "Reply-To", "To", "Cc", "Bcc",
	"Resent-Sender", "Resent-From", "Resent-Reply-To",
	"Resent-To", "Resent-Cc", "Resent-Bcc", NULL
};

static gchar *
efh_format_address (EMFormatHTML *efh, GString *out, struct _camel_header_address *a, gchar *field)
{
	guint32 flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES;
	gchar *name, *mailto, *addr;
	gint i=0;
	gboolean wrap = FALSE;
	gchar *str = NULL;
	gint limit = mail_config_get_address_count ();

	if (field ) {
		if ((!strcmp (field, _("To")) && !(efh->header_wrap_flags & EM_FORMAT_HTML_HEADER_TO))
		    || (!strcmp (field, _("Cc")) && !(efh->header_wrap_flags & EM_FORMAT_HTML_HEADER_CC))
		    || (!strcmp (field, _("Bcc")) && !(efh->header_wrap_flags & EM_FORMAT_HTML_HEADER_BCC)))
		    wrap = TRUE;
	}

	while (a) {
		if (a->name)
			name = camel_text_to_html (a->name, flags, 0);
		else
			name = NULL;

		switch (a->type) {
		case CAMEL_HEADER_ADDRESS_NAME:
			if (name && *name) {
				gchar *real, *mailaddr;

				g_string_append_printf (out, "%s &lt;", name);
				/* rfc2368 for mailto syntax and url encoding extras */
				if ((real = camel_header_encode_phrase ((guchar *)a->name))) {
					mailaddr = g_strdup_printf("%s <%s>", real, a->v.addr);
					g_free (real);
					mailto = camel_url_encode (mailaddr, "?=&()");
					g_free (mailaddr);
				} else {
					mailto = camel_url_encode (a->v.addr, "?=&()");
				}
			} else {
				mailto = camel_url_encode (a->v.addr, "?=&()");
			}
			addr = camel_text_to_html (a->v.addr, flags, 0);
			g_string_append_printf (out, "<a href=\"mailto:%s\">%s</a>", mailto, addr);
			g_free (mailto);
			g_free (addr);

			if (name && *name)
				g_string_append (out, "&gt;");
			break;
		case CAMEL_HEADER_ADDRESS_GROUP:
			g_string_append_printf (out, "%s: ", name);
			efh_format_address (efh, out, a->v.members, field);
			g_string_append_printf (out, ";");
			break;
		default:
			g_warning ("Invalid address type");
			break;
		}

		g_free (name);

		i++;
		a = a->next;
		if (a)
			g_string_append (out, ", ");

		/* Let us add a '...' if we have more addresses */
		if (limit > 0 && wrap && a && (i>(limit-1))) {

			if (!strcmp (field, _("To"))) {

				g_string_append (out, "<a href=\"##TO##\">...</a>");
				str = g_strdup_printf ("<a href=\"##TO##\"><img src=\"%s/plus.png\" /></a>  ", EVOLUTION_ICONSDIR);

				return str;
			}
			else if (!strcmp (field, _("Cc"))) {
				g_string_append (out, "<a href=\"##CC##\">...</a>");
				str = g_strdup_printf ("<a href=\"##CC##\"><img src=\"%s/plus.png\" /></a>  ", EVOLUTION_ICONSDIR);

				return str;
			}
			else if (!strcmp (field, _("Bcc"))) {
				g_string_append (out, "<a href=\"##BCC##\">...</a>");
				str = g_strdup_printf ("<a href=\"##BCC##\"><img src=\"%s/plus.png\" /></a>  ", EVOLUTION_ICONSDIR);

				return str;
			}
		}

	}

	if (limit > 0 && i>(limit)) {

		if (!strcmp (field, _("To"))) {
			str = g_strdup_printf ("<a href=\"##TO##\"><img src=\"%s/minus.png\" /></a>  ", EVOLUTION_ICONSDIR);
		}
		else if (!strcmp (field, _("Cc"))) {
			str = g_strdup_printf ("<a href=\"##CC##\"><img src=\"%s/minus.png\" /></a>  ", EVOLUTION_ICONSDIR);
		}
		else if (!strcmp (field, _("Bcc"))) {
			str = g_strdup_printf ("<a href=\"##BCC##\"><img src=\"%s/minus.png\" /></a>  ", EVOLUTION_ICONSDIR);
		}
	}

	return str;

}

static void
canon_header_name (gchar *name)
{
	gchar *inptr = name;

	/* canonicalise the header name... first letter is
	 * capitalised and any letter following a '-' also gets
	 * capitalised */

	if (*inptr >= 'a' && *inptr <= 'z')
		*inptr -= 0x20;

	inptr++;

	while (*inptr) {
		if (inptr[-1] == '-' && *inptr >= 'a' && *inptr <= 'z')
			*inptr -= 0x20;
		else if (*inptr >= 'A' && *inptr <= 'Z')
			*inptr += 0x20;

		inptr++;
	}
}

static void
efh_format_header(EMFormat *emf, CamelStream *stream, CamelMedium *part, struct _camel_header_raw *header, guint32 flags, const gchar *charset)
{
	EMFormatHTML *efh = (EMFormatHTML *)emf;
	gchar *name, *buf, *value = NULL;
	const gchar *label, *txt;
	gboolean addrspec = FALSE;
	gchar *str_field = NULL;
	gint i;

	name = alloca(strlen(header->name)+1);
	strcpy(name, header->name);
	canon_header_name (name);

	for (i = 0; addrspec_hdrs[i]; i++) {
		if (!strcmp(name, addrspec_hdrs[i])) {
			addrspec = TRUE;
			break;
		}
	}

	label = _(name);

	if (addrspec) {
		struct _camel_header_address *addrs;
		GString *html;
		gchar *img;

		buf = camel_header_unfold (header->value);
		if (!(addrs = camel_header_address_decode (buf, emf->charset ? emf->charset : emf->default_charset))) {
			g_free (buf);
			return;
		}

		g_free (buf);

		html = g_string_new("");
		img = efh_format_address(efh, html, addrs, (gchar *)label);

		if (img) {
			str_field = g_strdup_printf ("%s%s:", img, label);
			label = str_field;
			flags |= EM_FORMAT_HTML_HEADER_NODEC;
			g_free (img);
		}

		camel_header_address_unref(addrs);
		txt = value = html->str;
		g_string_free(html, FALSE);

		flags |= EM_FORMAT_HEADER_BOLD | EM_FORMAT_HTML_HEADER_HTML;
	} else if (!strcmp (name, "Subject")) {
		buf = camel_header_unfold (header->value);
		txt = value = camel_header_decode_string (buf, charset);
		g_free (buf);

		flags |= EM_FORMAT_HEADER_BOLD;
	} else if (!strcmp(name, "X-evolution-mailer")) {
		/* pseudo-header */
		label = _("Mailer");
		txt = value = camel_header_format_ctext (header->value, charset);
		flags |= EM_FORMAT_HEADER_BOLD;
	} else if (!strcmp (name, "Date") || !strcmp (name, "Resent-Date")) {
		gint msg_offset, local_tz;
		time_t msg_date;
		struct tm local;
		gchar *html;
		gboolean hide_real_date;

		hide_real_date = !emf->show_real_date;

		txt = header->value;
		while (*txt == ' ' || *txt == '\t')
			txt++;

		html = camel_text_to_html (txt, efh->text_html_flags, 0);

		msg_date = camel_header_decode_date(txt, &msg_offset);
		e_localtime_with_offset (msg_date, &local, &local_tz);

		/* Convert message offset to minutes (e.g. -0400 --> -240) */
		msg_offset = ((msg_offset / 100) * 60) + (msg_offset % 100);
		/* Turn into offset from localtime, not UTC */
		msg_offset -= local_tz / 60;

		/* value will be freed at the end */
		if (!hide_real_date && !msg_offset) {
			/* No timezone difference; just show the real Date: header */
			txt = value = html;
		} else {
			gchar *date_str;

			date_str = e_datetime_format_format ("mail", "header",
							     DTFormatKindDateTime, msg_date);

			if (hide_real_date) {
				/* Show only the local-formatted date, losing all timezone
				   information like Outlook does. Should we attempt to show
				   it somehow? */
				txt = value = date_str;
			} else {
				txt = value = g_strdup_printf ("%s (<I>%s</I>)", html, date_str);
				g_free (date_str);
			}
			g_free (html);
		}
		flags |= EM_FORMAT_HTML_HEADER_HTML | EM_FORMAT_HEADER_BOLD;
	} else if (!strcmp(name, "Newsgroups")) {
		struct _camel_header_newsgroup *ng, *scan;
		GString *html;

		buf = camel_header_unfold (header->value);

		if (!(ng = camel_header_newsgroups_decode (buf))) {
			g_free (buf);
			return;
		}

		g_free (buf);

		html = g_string_new("");
		scan = ng;
		while (scan) {
			g_string_append_printf(html, "<a href=\"news:%s\">%s</a>", scan->newsgroup, scan->newsgroup);
			scan = scan->next;
			if (scan)
				g_string_append_printf(html, ", ");
		}

		camel_header_newsgroups_free(ng);

		txt = html->str;
		g_string_free(html, FALSE);
		flags |= EM_FORMAT_HEADER_BOLD|EM_FORMAT_HTML_HEADER_HTML;
	} else if (!strcmp (name, "Received") || !strncmp (name, "X-", 2)) {
		/* don't unfold Received nor extension headers */
		txt = value = camel_header_decode_string(header->value, charset);
	} else {
		/* don't unfold Received nor extension headers */
		buf = camel_header_unfold (header->value);
		txt = value = camel_header_decode_string (buf, charset);
		g_free (buf);
	}

	efh_format_text_header(efh, stream, label, txt, flags);

	g_free (value);
	g_free (str_field);
}

static void
efh_format_headers(EMFormatHTML *efh, CamelStream *stream, CamelMedium *part)
{
	EMFormat *emf = (EMFormat *) efh;
	EMFormatHeader *h;
	const gchar *charset;
	CamelContentType *ct;
	struct _camel_header_raw *header;
	gboolean have_icon = FALSE;
	const gchar *photo_name = NULL;
	CamelInternetAddress *cia = NULL;
	gboolean face_decoded  = FALSE, contact_has_photo = FALSE;
	guchar *face_header_value = NULL;
	gsize face_header_len = 0;
	gchar *header_sender = NULL, *header_from = NULL, *name;
	gboolean mail_from_delegate = FALSE;
	const gchar *hdr_charset;

	if (!part)
		return;

	ct = camel_mime_part_get_content_type((CamelMimePart *)part);
	charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name(charset);

	if (!efh->simple_headers)
		camel_stream_printf(stream,
				    "<font color=\"#%06x\">\n"
				    "<table cellpadding=\"0\" width=\"100%%\">",
				    efh->header_colour & 0xffffff);

	hdr_charset = emf->charset ? emf->charset : emf->default_charset;

	header = ((CamelMimePart *)part)->headers;
	while (header) {
		if (!g_ascii_strcasecmp (header->name, "Sender")) {
			struct _camel_header_address *addrs;
			GString *html;

			if (!(addrs = camel_header_address_decode (header->value, hdr_charset)))
				break;

			html = g_string_new("");
			name = efh_format_address(efh, html, addrs, header->name);

			header_sender = html->str;
			camel_header_address_unref(addrs);

			g_string_free(html, FALSE);
			g_free (name);
		} else if (!g_ascii_strcasecmp (header->name, "From")) {
			struct _camel_header_address *addrs;
			GString *html;

			if (!(addrs = camel_header_address_decode (header->value, hdr_charset)))
				break;

			html = g_string_new("");
			name = efh_format_address(efh, html, addrs, header->name);

			header_from = html->str;
			camel_header_address_unref(addrs);

			g_string_free(html, FALSE);
			g_free(name);
		} else if (!g_ascii_strcasecmp (header->name, "X-Evolution-Mail-From-Delegate")) {
			mail_from_delegate = TRUE;
		}

		header = header->next;
	}

	if (header_sender && header_from && mail_from_delegate) {
		camel_stream_printf(stream, "<tr><td><table border=1 width=\"100%%\" cellspacing=2 cellpadding=2><tr>");
		if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
			camel_stream_printf (stream, "<td align=\"right\" width=\"100%%\">");
		else
			camel_stream_printf (stream, "<td align=\"left\" width=\"100%%\">");
		/* To translators: This message suggests to the receipients that the sender of the mail is
		   different from the one listed in From field.
		*/
		camel_stream_printf(stream, _("This message was sent by <b>%s</b> on behalf of <b>%s</b>"), header_sender, header_from);
		camel_stream_printf(stream, "</td></tr></table></td></tr>");
	}

	g_free (header_sender);
	g_free (header_from);

	if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
		camel_stream_printf (stream, "<tr><td><table width=\"100%%\" border=0 cellpadding=\"0\">\n");
	else
		camel_stream_printf (stream, "<tr><td><table border=0 cellpadding=\"0\">\n");

	/* dump selected headers */
	h = (EMFormatHeader *)emf->header_list.head;
	if (emf->mode == EM_FORMAT_ALLHEADERS) {
		header = ((CamelMimePart *)part)->headers;
		while (header) {
			efh_format_header(emf, stream, part, header, EM_FORMAT_HTML_HEADER_NOCOLUMNS, charset);
			header = header->next;
		}
	} else {
		gint mailer_shown = FALSE;
		while (h->next) {
			gint mailer, face;

			header = ((CamelMimePart *)part)->headers;
			mailer = !g_ascii_strcasecmp (h->name, "X-Evolution-Mailer");
			face = !g_ascii_strcasecmp (h->name, "Face");

			while (header) {
				if (emf->show_photo && !photo_name && !g_ascii_strcasecmp (header->name, "From"))
					photo_name = header->value;

				if (!mailer_shown && mailer && (!g_ascii_strcasecmp (header->name, "X-Mailer") ||
								!g_ascii_strcasecmp (header->name, "User-Agent") ||
								!g_ascii_strcasecmp (header->name, "X-Newsreader") ||
								!g_ascii_strcasecmp (header->name, "X-MimeOLE"))) {
					struct _camel_header_raw xmailer, *use_header = NULL;

					if (!g_ascii_strcasecmp (header->name, "X-MimeOLE")) {
						for (use_header = header->next; use_header; use_header = use_header->next) {
							if (!g_ascii_strcasecmp (use_header->name, "X-Mailer") ||
							    !g_ascii_strcasecmp (use_header->name, "User-Agent") ||
							    !g_ascii_strcasecmp (use_header->name, "X-Newsreader")) {
								/* even we have X-MimeOLE, then use rather the standard one, when available */
								break;
							}
						}
					}

					if (!use_header)
						use_header = header;

					xmailer.name = (gchar *) "X-Evolution-Mailer";
					xmailer.value = use_header->value;
					mailer_shown = TRUE;

					efh_format_header (emf, stream, part, &xmailer, h->flags, charset);
					if (strstr(use_header->value, "Evolution"))
						have_icon = TRUE;
				} else if (!face_decoded && face && !g_ascii_strcasecmp (header->name, "Face")) {
					gchar *cp = header->value;

					/* Skip over spaces */
					while (*cp == ' ')
						cp++;

					face_header_value = g_base64_decode (cp, &face_header_len);
					face_header_value = g_realloc (face_header_value, face_header_len + 1);
					face_header_value[face_header_len] = 0;
					face_decoded = TRUE;
				/* Showing an encoded "Face" header makes little sense */
				} else if (!g_ascii_strcasecmp (header->name, h->name) && !face) {
					efh_format_header(emf, stream, part, header, h->flags, charset);
				}

				header = header->next;
			}
			h = h->next;
		}
	}

	if (!efh->simple_headers) {
		camel_stream_printf(stream, "</table></td>");

		if (photo_name) {
			gchar *classid;
			CamelMimePart *photopart;

			cia = camel_internet_address_new();
			camel_address_decode((CamelAddress *) cia, (const gchar *) photo_name);
			photopart = em_utils_contact_photo (cia, emf->photo_local);

			if (photopart) {
				contact_has_photo = TRUE;
				classid = g_strdup_printf("icon:///em-format-html/%s/photo/header",
				emf->part_id->str);
				camel_stream_printf(stream,
					"<td align=\"right\" valign=\"top\"><img width=64 src=\"%s\"></td>",
					classid);
				em_format_add_puri(emf, sizeof(EMFormatPURI), classid,
					photopart, efh_write_image);
				camel_object_unref(photopart);

				g_free(classid);
			}
			camel_object_unref(cia);
		}

		if (!contact_has_photo && face_decoded) {
			gchar *classid;
			CamelMimePart *part;

			part = camel_mime_part_new ();
			camel_mime_part_set_content ((CamelMimePart *) part, (const gchar *) face_header_value, face_header_len, "image/png");
			classid = g_strdup_printf("icon:///em-format-html/face/photo/header");
			camel_stream_printf(stream, "<td align=\"right\" valign=\"top\"><img width=48 src=\"%s\"></td>", classid);
			em_format_add_puri(emf, sizeof(EMFormatPURI), classid, part, efh_write_image);
			camel_object_unref(part);
		}

		if (have_icon && efh->show_icon) {
			GtkIconInfo *icon_info;
			gchar *classid;
			CamelMimePart *iconpart = NULL;

			classid = g_strdup_printf("icon:///em-format-html/%s/icon/header", emf->part_id->str);
			camel_stream_printf(stream, "<td align=\"right\" valign=\"top\"><img width=16 height=16 src=\"%s\"></td>", classid);

			icon_info = gtk_icon_theme_lookup_icon (
				gtk_icon_theme_get_default (),
				"evolution", 16, GTK_ICON_LOOKUP_NO_SVG);
			if (icon_info != NULL) {
				iconpart = em_format_html_file_part (
					(EMFormatHTML *) emf, "image/png",
					gtk_icon_info_get_filename (icon_info));
				gtk_icon_info_free (icon_info);
			}

			if (iconpart) {
				em_format_add_puri(emf, sizeof(EMFormatPURI), classid, iconpart, efh_write_image);
				camel_object_unref(iconpart);
			}
			g_free(classid);
		}
		camel_stream_printf (stream, "</tr></table>\n</font>\n");
	}
}

static void efh_format_message(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	const EMFormatHandler *handle;

	/* TODO: make this validity stuff a method */
	EMFormatHTML *efh = (EMFormatHTML *) emf;
	CamelCipherValidity *save = emf->valid, *save_parent = emf->valid_parent;

	emf->valid = NULL;
	emf->valid_parent = NULL;

	if (emf->message != (CamelMimeMessage *)part)
		camel_stream_printf(stream, "<blockquote>\n");

	if (!efh->hide_headers)
		efh_format_headers(efh, stream, (CamelMedium *)part);

	handle = em_format_find_handler(emf, "x-evolution/message/post-header");
	if (handle)
		handle->handler(emf, stream, part, handle);

	camel_stream_printf(stream, EM_FORMAT_HTML_VPAD);
	em_format_part(emf, stream, part);

	if (emf->message != (CamelMimeMessage *)part)
		camel_stream_printf(stream, "</blockquote>\n");

	camel_cipher_validity_free(emf->valid);

	emf->valid = save;
	emf->valid_parent = save_parent;
}

static void efh_format_source(EMFormat *emf, CamelStream *stream, CamelMimePart *part)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *html_filter;
	CamelDataWrapper *dw = (CamelDataWrapper *)part;

	filtered_stream = camel_stream_filter_new_with_stream ((CamelStream *) stream);
	html_filter = camel_mime_filter_tohtml_new (CAMEL_MIME_FILTER_TOHTML_CONVERT_NL
						    | CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES
						    | CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT, 0);
	camel_stream_filter_add(filtered_stream, html_filter);
	camel_object_unref(html_filter);

	camel_stream_write_string((CamelStream *)stream, "<table><tr><td><tt>");
	em_format_format_text(emf, (CamelStream *)filtered_stream, dw);
	camel_object_unref(filtered_stream);

	camel_stream_write_string(stream, "</tt></td></tr></table>");
}

static void
efh_format_attachment(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const gchar *mime_type, const EMFormatHandler *handle)
{
	gchar *text, *html;

	/* we display all inlined attachments only */

	/* this could probably be cleaned up ... */
	camel_stream_write_string(stream,
				  "<table border=1 cellspacing=0 cellpadding=0><tr><td>"
				  "<table width=10 cellspacing=0 cellpadding=0>"
				  "<tr><td></td></tr></table></td>"
				  "<td><table width=3 cellspacing=0 cellpadding=0>"
				  "<tr><td></td></tr></table></td><td><font size=-1>\n");

	/* output some info about it */
	text = em_format_describe_part(part, mime_type);
	html = camel_text_to_html(text, ((EMFormatHTML *)emf)->text_html_flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_write_string(stream, html);
	g_free(html);
	g_free(text);

	camel_stream_write_string(stream, "</font></td></tr><tr></table>");

	if (handle && em_format_is_inline(emf, emf->part_id->str, part, handle))
		handle->handler(emf, stream, part, handle);
}

static gboolean
efh_busy(EMFormat *emf)
{
	return (((EMFormatHTML *)emf)->priv->format_id != -1);
}
