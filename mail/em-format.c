/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <string.h>

#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>

#include <gconf/gconf-client.h>

#include <e-util/e-msgport.h>
#include <camel/camel-url.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-multipart.h>
#include <camel/camel-multipart-encrypted.h>
#include <camel/camel-multipart-signed.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-gpg-context.h>
#include <camel/camel-string-utils.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-stream-null.h>
#include <camel/camel-mime-filter-charset.h>
#include <camel/camel-mime-filter-windows.h>

#include "em-format.h"

#define d(x)

struct _EMFormatPrivate {
	GConfClient *gconf;
	guint charset_id;
	char *gconf_charset;
};

static void emf_builtin_init(EMFormatClass *);
static const char *emf_snoop_part(CamelMimePart *part);

static void emf_format_clone(EMFormat *emf, CamelMedium *msg, EMFormat *emfsource);
static gboolean emf_busy(EMFormat *emf);

enum {
	EMF_COMPLETE,
	EMF_LAST_SIGNAL,
};

static guint emf_signals[EMF_LAST_SIGNAL];
static GObjectClass *emf_parent;

static void
gconf_charset_changed (GConfClient *client, guint cnxn_id,
		       GConfEntry *entry, gpointer user_data)
{
	struct _EMFormatPrivate *priv = ((EMFormat *) user_data)->priv;
	
	g_free (priv->gconf_charset);
	priv->gconf_charset = gconf_client_get_string (priv->gconf, "/apps/evolution/mail/format/charset", NULL);
}

static void
emf_init(GObject *o)
{
	struct _EMFormatPrivate *priv;
	EMFormat *emf = (EMFormat *)o;
	
	priv = emf->priv = g_new (struct _EMFormatPrivate, 1);
	priv->gconf = gconf_client_get_default ();
	gconf_client_add_dir (priv->gconf, "/apps/evolution/mail/format/charset", 			      
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	priv->charset_id = gconf_client_notify_add (priv->gconf, "/apps/evolution/mail/format/charset",
						    gconf_charset_changed, emf, NULL, NULL);
	priv->gconf_charset = gconf_client_get_string (priv->gconf, "/apps/evolution/mail/format/charset", NULL);
	
	emf->inline_table = g_hash_table_new(NULL, NULL);
	e_dlist_init(&emf->header_list);
	em_format_default_headers(emf);
}

static void
emf_finalise(GObject *o)
{
	struct _EMFormatPrivate *priv = ((EMFormat *) o)->priv;
	EMFormat *emf = (EMFormat *)o;
	
	gconf_client_notify_remove (priv->gconf, priv->charset_id);
	priv->charset_id = 0;
	g_object_unref (priv->gconf);
	g_free (priv->gconf_charset);
	g_free (priv);
	
	if (emf->session)
		camel_object_unref(emf->session);

	if (emf->inline_table)
		g_hash_table_destroy(emf->inline_table);

	em_format_clear_headers(emf);
	g_free(emf->charset);
	
	/* FIXME: check pending jobs */
	
	((GObjectClass *)emf_parent)->finalize(o);
}

static void
emf_base_init(EMFormatClass *emfklass)
{
	emfklass->type_handlers = g_hash_table_new(g_str_hash, g_str_equal);
	emf_builtin_init(emfklass);
}

static void
emf_class_init(GObjectClass *klass)
{
	((EMFormatClass *)klass)->type_handlers = g_hash_table_new(g_str_hash, g_str_equal);
	emf_builtin_init((EMFormatClass *)klass);

	klass->finalize = emf_finalise;
	((EMFormatClass *)klass)->format_clone = emf_format_clone;
	((EMFormatClass *)klass)->busy = emf_busy;

	emf_signals[EMF_COMPLETE] =
		g_signal_new("complete",
			     G_OBJECT_CLASS_TYPE (klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET (EMFormatClass, complete),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0);
}

GType
em_format_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMFormatClass),
			(GBaseInitFunc)emf_base_init, NULL,
			(GClassInitFunc)emf_class_init,
			NULL, NULL,
			sizeof(EMFormat), 0,
			(GInstanceInitFunc)emf_init
		};
		emf_parent = g_type_class_ref(G_TYPE_OBJECT);
		type = g_type_register_static(G_TYPE_OBJECT, "EMFormat", &info, 0);
	}

	return type;
}

/**
 * em_format_class_add_handler:
 * @emfc: EMFormatClass
 * @info: Callback information.
 * 
 * Add a mime type handler to this class.  This is only used by implementing
 * classes.
 *
 * When a mime type described by @info is encountered, the callback will
 * be invoked.  Note that @info may be extended by sub-classes if
 * they require additional context information.
 *
 * Use a mime type of "foo/ *" to insert a fallback handler for type "foo".
 **/
void
em_format_class_add_handler(EMFormatClass *emfc, EMFormatHandler *info)
{
	g_hash_table_insert(emfc->type_handlers, info->mime_type, info);
	/* FIXME: do we care?  This is really gui stuff */
	/*
	  if (info->applications == NULL)
	  info->applications = gnome_vfs_mime_get_short_list_applications(info->mime_type);*/
}


/**
 * em_format_class_remove_handler:
 * @emfc: EMFormatClass
 * @mime_type: mime-type of handler to remove
 *
 * Remove a mime type handler from this class. This is only used by
 * implementing classes.
 **/
void
em_format_class_remove_handler (EMFormatClass *emfc, const char *mime_type)
{
	g_hash_table_remove (emfc->type_handlers, mime_type);
}


/**
 * em_format_find_handler:
 * @emf: 
 * @mime_type: 
 * 
 * Find a format handler by @mime_type.
 * 
 * Return value: NULL if no handler is available.
 **/
const EMFormatHandler *
em_format_find_handler(EMFormat *emf, const char *mime_type)
{
	EMFormatClass *emfc = (EMFormatClass *)G_OBJECT_GET_CLASS(emf);

	return g_hash_table_lookup(emfc->type_handlers, mime_type);
}

/**
 * em_format_fallback_handler:
 * @emf: 
 * @mime_type: 
 * 
 * Try to find a format handler based on the major type of the @mime_type.
 *
 * The subtype is replaced with "*" and a lookup performed.
 * 
 * Return value: 
 **/
const EMFormatHandler *
em_format_fallback_handler(EMFormat *emf, const char *mime_type)
{
	char *mime, *s;

	s = strchr(mime_type, '/');
	if (s == NULL)
		mime = (char *)mime_type;
	else {
		size_t len = (s-mime_type)+1;

		mime = alloca(len+2);
		strncpy(mime, mime_type, len);
		strcpy(mime+len, "*");
	}

	return em_format_find_handler(emf, mime);
}

/**
 * em_format_add_puri:
 * @emf: 
 * @size: 
 * @cid: Override the autogenerated content id.
 * @part: 
 * @func: 
 * 
 * Add a pending-uri handler.  When formatting parts that reference
 * other parts, a pending-uri (PURI) can be used to track the reference.
 *
 * @size is used to allocate the structure, so that it can be directly
 * subclassed by implementors.
 * 
 * @cid can be used to override the key used to retreive the PURI, if NULL,
 * then the content-location and the content-id of the @part are stored
 * as lookup keys for the part.
 *
 * FIXME: This may need a free callback.
 *
 * Return value: A new PURI, with a referenced copy of @part, and the cid
 * always set.  The uri will be set if one is available.  Clashes
 * are resolved by forgetting the old PURI in the global index.
 **/
EMFormatPURI *
em_format_add_puri(EMFormat *emf, size_t size, const char *cid, CamelMimePart *part, EMFormatPURIFunc func)
{
	EMFormatPURI *puri;
	const char *tmp;
	static unsigned int uriid;

	g_assert(size >= sizeof(*puri));
	puri = g_malloc0(size);

	puri->format = emf;
	puri->func = func;
	puri->use_count = 0;
	puri->cid = g_strdup(cid);

	if (part) {
		camel_object_ref(part);
		puri->part = part;
	}

	if (part != NULL && cid == NULL) {
		tmp = camel_mime_part_get_content_id(part);
		if (tmp)
			puri->cid = g_strdup_printf("cid:%s", tmp);
		else
			puri->cid = g_strdup_printf("em-no-cid-%u", uriid++);

		d(printf("built cid '%s'\n", puri->cid));

		/* not quite same as old behaviour, it also put in the relative uri and a fallback for no parent uri */
		tmp = camel_mime_part_get_content_location(part);
		puri->uri = NULL;
		if (tmp == NULL) {
			if (emf->base)
				puri->uri = camel_url_to_string(emf->base, 0);
		} else {
			if (strchr(tmp, ':') == NULL && emf->base != NULL) {
				CamelURL *uri;

				uri = camel_url_new_with_base(emf->base, tmp);
				puri->uri = camel_url_to_string(uri, 0);
				camel_url_free(uri);
			} else {
				puri->uri = g_strdup(tmp);
			}
		}
	}

	g_assert(puri->cid != NULL);
	g_assert(emf->pending_uri_level != NULL);
	g_assert(emf->pending_uri_table != NULL);

	e_dlist_addtail(&emf->pending_uri_level->uri_list, (EDListNode *)puri);

	if (puri->uri)
		g_hash_table_insert(emf->pending_uri_table, puri->uri, puri);
	g_hash_table_insert(emf->pending_uri_table, puri->cid, puri);

	return puri;
}

/**
 * em_format_push_level:
 * @emf: 
 * 
 * This is used to build a heirarchy of visible PURI objects based on
 * the structure of the message.  Used by multipart/alternative formatter.
 *
 * FIXME: This could probably also take a uri so it can automaticall update
 * the base location.
 **/
void
em_format_push_level(EMFormat *emf)
{
	struct _EMFormatPURITree *purilist;

	d(printf("em_format_push_level\n"));
	purilist = g_malloc0(sizeof(*purilist));
	e_dlist_init(&purilist->children);
	e_dlist_init(&purilist->uri_list);
	purilist->parent = emf->pending_uri_level;
	if (emf->pending_uri_tree == NULL) {
		emf->pending_uri_tree = purilist;
	} else {
		e_dlist_addtail(&emf->pending_uri_level->children, (EDListNode *)purilist);
	}
	emf->pending_uri_level = purilist;
}

/**
 * em_format_pull_level:
 * @emf: 
 * 
 * Drop a level of visibility back to the parent.  Note that
 * no PURI values are actually freed.
 **/
void
em_format_pull_level(EMFormat *emf)
{
	d(printf("em_format_pull_level\n"));
	emf->pending_uri_level = emf->pending_uri_level->parent;
}

/**
 * em_format_find_visible_puri:
 * @emf: 
 * @uri: 
 * 
 * Search for a PURI based on the visibility defined by :push_level()
 * and :pull_level().
 * 
 * Return value: 
 **/
EMFormatPURI *
em_format_find_visible_puri(EMFormat *emf, const char *uri)
{
	EMFormatPURI *pw;
	struct _EMFormatPURITree *ptree;

	d(printf("checking for visible uri '%s'\n", uri));

	ptree = emf->pending_uri_level;
	while (ptree) {
		pw = (EMFormatPURI *)ptree->uri_list.head;
		while (pw->next) {
			d(printf(" pw->uri = '%s' pw->cid = '%s\n", pw->uri?pw->uri:"", pw->cid));
			if ((pw->uri && !strcmp(pw->uri, uri)) || !strcmp(pw->cid, uri))
				return pw;
			pw = pw->next;
		}
		ptree = ptree->parent;
	}

	return NULL;
}

/**
 * em_format_find_puri:
 * @emf: 
 * @uri: 
 * 
 * Search for a PURI based on a uri.  Both the content-id
 * and content-location are checked.
 * 
 * Return value: 
 **/
EMFormatPURI *
em_format_find_puri(EMFormat *emf, const char *uri)
{
	return g_hash_table_lookup(emf->pending_uri_table, uri);
}

static void
emf_clear_puri_node(struct _EMFormatPURITree *node)
{
	{
		EMFormatPURI *pw, *pn;

		/* clear puri's at this level */
		pw = (EMFormatPURI *)node->uri_list.head;
		pn = pw->next;
		while (pn) {
			g_free(pw->uri);
			g_free(pw->cid);
			if (pw->part)
				camel_object_unref(pw->part);
			g_free(pw);
			pw = pn;
			pn = pn->next;
		}
	}

	{
		struct _EMFormatPURITree *cw, *cn;

		/* clear child nodes */
		cw = (struct _EMFormatPURITree *)node->children.head;
		cn = cw->next;
		while (cn) {
			emf_clear_puri_node(cw);
			cw = cn;
			cn = cn->next;
		}
	}

	g_free(node);
}

/**
 * em_format_clear_puri_tree:
 * @emf: 
 * 
 * For use by implementors to clear out the message structure
 * data.
 **/
void
em_format_clear_puri_tree(EMFormat *emf)
{
	d(printf("clearing pending uri's\n"));

	if (emf->pending_uri_table) {
		g_hash_table_destroy(emf->pending_uri_table);
		emf_clear_puri_node(emf->pending_uri_tree);
		emf->pending_uri_level = NULL;
		emf->pending_uri_tree = NULL;
	}
	emf->pending_uri_table = g_hash_table_new(g_str_hash, g_str_equal);
	em_format_push_level(emf);
}

/* use mime_type == NULL  to force showing as application/octet-stream */
void
em_format_part_as(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const char *mime_type)
{
	const EMFormatHandler *handle = NULL;

	if (mime_type != NULL) {
		if (g_ascii_strcasecmp(mime_type, "application/octet-stream") == 0)
			mime_type = emf_snoop_part(part);

		handle = em_format_find_handler(emf, mime_type);
		if (handle == NULL)
			handle = em_format_fallback_handler(emf, mime_type);

		if (handle != NULL
		    && !em_format_is_attachment(emf, part)) {
			d(printf("running handler for type '%s'\n", mime_type));
			handle->handler(emf, stream, part, handle);
			return;
		}
		d(printf("this type is an attachment? '%s'\n", mime_type));
	} else {
		mime_type = "application/octet-stream";
	}

	((EMFormatClass *)G_OBJECT_GET_CLASS(emf))->format_attachment(emf, stream, part, mime_type, handle);
}

void
em_format_part(EMFormat *emf, CamelStream *stream, CamelMimePart *part)
{
	char *mime_type;
	CamelDataWrapper *dw;

	dw = camel_medium_get_content_object((CamelMedium *)part);
	mime_type = camel_data_wrapper_get_mime_type(dw);
	camel_strdown(mime_type);
	em_format_part_as(emf, stream, part, mime_type);
	g_free(mime_type);
}

static void
emf_clone_inlines(void *key, void *val, void *data)
{
	g_hash_table_insert(((EMFormat *)data)->inline_table, key, val);
}

static void
emf_format_clone(EMFormat *emf, CamelMedium *msg, EMFormat *emfsource)
{
	em_format_clear_puri_tree(emf);

	if (emf != emfsource) {
		g_hash_table_destroy(emf->inline_table);
		emf->inline_table = g_hash_table_new(NULL, NULL);
		if (emfsource) {
			/* We clone the current state here */
			g_hash_table_foreach(emfsource->inline_table, emf_clone_inlines, emf);
			emf->mode = emfsource->mode;
			g_free(emf->charset);
			emf->charset = g_strdup(emfsource->charset);
			/* FIXME: clone headers shown */
		}
	}

	if (msg != emf->message) {
		if (emf->message)
			camel_object_unref(emf->message);
		if (msg)
			camel_object_ref(msg);
		emf->message = msg;
	}
}

static gboolean
emf_busy(EMFormat *emf)
{
	return FALSE;
}

/**
 * em_format_format_clone:
 * @emf: Mail formatter.
 * @msg: Mail message.
 * @emfsource: Used as a basis for user-altered layout, e.g. inline viewed
 * attachments.
 * 
 * Format a message @msg.  If @emfsource is non NULL, then the status of
 * inlined expansion and so forth is copied direction from @emfsource.
 *
 * By passing the same value for @emf and @emfsource, you can perform
 * a display refresh, or it can be used to generate an identical layout,
 * e.g. to print what the user has shown inline.
 **/
/* e_format_format_clone is a macro */

/**
 * em_format_set_session:
 * @emf: 
 * @s: 
 * 
 * Set the CamelSession to be used for signature verification and decryption
 * purposes.  If this is not set, then signatures cannot be verified or
 * encrypted messages viewed.
 **/
void
em_format_set_session(EMFormat *emf, struct _CamelSession *s)
{
	if (s)
		camel_object_ref(s);
	if (emf->session)
		camel_object_unref(emf->session);
	emf->session = s;
}

/**
 * em_format_set_mode:
 * @emf: 
 * @type: 
 * 
 * Set display mode, EM_FORMAT_SOURCE, EM_FORMAT_ALLHEADERS, or
 * EM_FORMAT_NORMAL.
 **/
void
em_format_set_mode(EMFormat *emf, em_format_mode_t type)
{
	if (emf->mode == type)
		return;

	emf->mode = type;

	/* force redraw if type changed afterwards */
	if (emf->message)
		em_format_format_clone(emf, emf->message, emf);
}

/**
 * em_format_set_charset:
 * @emf: 
 * @charset: 
 * 
 * set override charset on formatter.  message will be redisplayed if
 * required.
 **/
void
em_format_set_charset(EMFormat *emf, const char *charset)
{
	if ((emf->charset && charset && g_ascii_strcasecmp(emf->charset, charset) == 0)
	    || (emf->charset == NULL && charset == NULL)
	    || (emf->charset == charset))
		return;

	g_free(emf->charset);
	emf->charset = g_strdup(charset);

	if (emf->message)
		em_format_format_clone(emf, emf->message, emf);
}

/**
 * em_format_clear_headers:
 * @emf: 
 * 
 * Clear the list of headers to be displayed.  This will force all headers to
 * be shown.
 **/
void
em_format_clear_headers(EMFormat *emf)
{
	EMFormatHeader *eh;

	while ((eh = (EMFormatHeader *)e_dlist_remhead(&emf->header_list)))
		g_free(eh);
}

static const struct {
	const char *name;
	guint32 flags;
} default_headers[] = {
	{ N_("From"), EM_FORMAT_HEADER_BOLD },
	{ N_("Reply-To"), EM_FORMAT_HEADER_BOLD },
	{ N_("To"), EM_FORMAT_HEADER_BOLD },
	{ N_("Cc"), EM_FORMAT_HEADER_BOLD },
	{ N_("Bcc"), EM_FORMAT_HEADER_BOLD },
	{ N_("Subject"), EM_FORMAT_HEADER_BOLD },
	{ N_("Date"), EM_FORMAT_HEADER_BOLD },
	{ "x-evolution-mailer", 0 }, /* DO NOT translate */
};

/**
 * em_format_default_headers:
 * @emf: 
 * 
 * Set the headers to show to the default list.
 *
 * From, Reply-To, To, Cc, Bcc, Subject and Date.
 **/
void
em_format_default_headers(EMFormat *emf)
{
	int i;

	em_format_clear_headers(emf);
	for (i=0;i<sizeof(default_headers)/sizeof(default_headers[0]);i++)
		em_format_add_header(emf, default_headers[i].name, default_headers[i].flags);
}

/**
 * em_format_add_header:
 * @emf: 
 * @name: The name of the header, as it will appear during output.
 * @flags: EM_FORMAT_HEAD_* defines to control display attributes.
 * 
 * Add a specific header to show.  If any headers are set, they will
 * be displayed in the order set by this function.  Certain known
 * headers included in this list will be shown using special
 * formatting routines.
 **/
void em_format_add_header(EMFormat *emf, const char *name, guint32 flags)
{
	EMFormatHeader *h;

	h = g_malloc(sizeof(*h) + strlen(name));
	h->flags = flags;
	strcpy(h->name, name);
	e_dlist_addtail(&emf->header_list, (EDListNode *)h);
}

/**
 * em_format_is_attachment:
 * @emf: 
 * @part: Part to check.
 * 
 * Returns true if the part is an attachment.
 *
 * A part is not considered an attachment if it is a
 * multipart, or a text part with no filename.  It is used
 * to determine if an attachment header should be displayed for
 * the part.
 *
 * Content-Disposition is not checked.
 * 
 * Return value: TRUE/FALSE
 **/
int em_format_is_attachment(EMFormat *emf, CamelMimePart *part)
{
	/*CamelContentType *ct = camel_mime_part_get_content_type(part);*/
	CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)part);

	/*printf("checking is attachment %s/%s\n", ct->type, ct->subtype);*/
	return !(/*camel_content_type_is (ct, "message", "*")
		   ||*/ camel_content_type_is (dw->mime_type, "multipart", "*")
		 || (camel_content_type_is (dw->mime_type, "text", "*")
		     && camel_mime_part_get_filename(part) == NULL));

}

/**
 * em_format_is_inline:
 * @emf: 
 * @part: 
 * 
 * Returns true if the part should be displayed inline.  Any part with
 * a Content-Disposition of inline, or any message type is displayed
 * inline.
 *
 * ::set_inline() called on the same part will override any calculated
 * value.
 * 
 * Return value: 
 **/
int em_format_is_inline(EMFormat *emf, CamelMimePart *part)
{
	void *dummy, *override;
	const char *tmp;
	CamelContentType *ct;

	if (g_hash_table_lookup_extended(emf->inline_table, part, &dummy, &override))
		return GPOINTER_TO_INT(override);

	tmp = camel_mime_part_get_disposition(part);
	if (tmp)
		return g_ascii_strcasecmp(tmp, "inline") == 0;

	/* messages are always inline? */
	ct = camel_mime_part_get_content_type(part);

	return camel_content_type_is (ct, "message", "*");
}

/**
 * em_format_set_inline:
 * @emf: 
 * @part: 
 * @state: 
 * 
 * Force the attachment @part to be expanded or hidden explictly to match
 * @state.  This is used only to record the change for a redraw or
 * cloned layout render and does not force a redraw.
 **/
void em_format_set_inline(EMFormat *emf, CamelMimePart *part, int state)
{
	g_hash_table_insert(emf->inline_table, part, GINT_TO_POINTER(state));
}

/* should this be virtual? */
void
em_format_format_content(EMFormat *emf, CamelStream *stream, CamelMimePart *part)
{
	CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)part);

	if (camel_content_type_is (dw->mime_type, "text", "*"))
		em_format_format_text(emf, stream, dw);
	else
		camel_data_wrapper_decode_to_stream(dw, stream);
}

/**
 * em_format_format_content:
 * @emf: 
 * @stream: Where to write the converted text
 * @part: Part whose container is to be formatted
 * 
 * Decode/output a part's content to @stream.
 **/
void
em_format_format_text(EMFormat *emf, CamelStream *stream, CamelDataWrapper *dw)
{
	CamelStreamFilter *filter_stream;
	CamelMimeFilterCharset *filter;
	const char *charset = NULL;
	
	if (emf->charset) {
		charset = emf->charset;
	} else if (dw->mime_type
		   && (charset = camel_content_type_param (dw->mime_type, "charset"))
		   && g_ascii_strncasecmp(charset, "iso-8859-", 9) == 0) {
		CamelMimeFilterWindows *windows;
		CamelStream *null;

		/* Since a few Windows mailers like to claim they sent
		 * out iso-8859-# encoded text when they really sent
		 * out windows-cp125#, do some simple sanity checking
		 * before we move on... */

		null = camel_stream_null_new();
		filter_stream = camel_stream_filter_new_with_stream(null);
		camel_object_unref(null);
		
		windows = (CamelMimeFilterWindows *)camel_mime_filter_windows_new(charset);
		camel_stream_filter_add(filter_stream, (CamelMimeFilter *)windows);
		
		camel_data_wrapper_decode_to_stream(dw, (CamelStream *)filter_stream);
		camel_stream_flush((CamelStream *)filter_stream);
		camel_object_unref(filter_stream);
		
		charset = camel_mime_filter_windows_real_charset (windows);
		camel_object_unref(windows);
	} else if (charset == NULL) {
		charset = emf->priv->gconf_charset;
	}
	
	filter_stream = camel_stream_filter_new_with_stream(stream);
	
	if ((filter = camel_mime_filter_charset_new_convert(charset, "UTF-8"))) {
		camel_stream_filter_add(filter_stream, (CamelMimeFilter *) filter);
		camel_object_unref(filter);
	}
	
	camel_data_wrapper_decode_to_stream(dw, (CamelStream *)filter_stream);
	camel_stream_flush((CamelStream *)filter_stream);
	camel_object_unref(filter_stream);
}

/**
 * em_format_describe_part:
 * @part: 
 * @mimetype: 
 * 
 * Generate a simple textual description of a part, @mime_type represents the
 * the content.
 * 
 * Return value: 
 **/
char *
em_format_describe_part(CamelMimePart *part, const char *mime_type)
{
	GString *stext;
	const char *text;
	char *out;

	stext = g_string_new("");
	text = gnome_vfs_mime_get_description(mime_type);
	g_string_append_printf(stext, _("%s attachment"), text?text:mime_type);
	if ((text = camel_mime_part_get_filename (part)))
		g_string_append_printf(stext, " (%s)", text);
	if ((text = camel_mime_part_get_description(part)))
		g_string_append_printf(stext, ", \"%s\"", text);

	out = stext->str;
	g_string_free(stext, FALSE);

	return out;
}

/* ********************************************************************** */

/* originally from mail-identify.c */
static const char *
emf_snoop_part(CamelMimePart *part)
{
	const char *filename, *name_type = NULL, *magic_type = NULL;
	CamelDataWrapper *dw;
	
	filename = camel_mime_part_get_filename (part);
	if (filename) {
		/* GNOME-VFS will misidentify TNEF attachments as MPEG */
		if (!strcmp (filename, "winmail.dat"))
			return "application/vnd.ms-tnef";
		
		name_type = gnome_vfs_mime_type_from_name(filename);
	}
	
	dw = camel_medium_get_content_object((CamelMedium *)part);
	if (!camel_data_wrapper_is_offline(dw)) {
		CamelStreamMem *mem = (CamelStreamMem *)camel_stream_mem_new();

		if (camel_data_wrapper_decode_to_stream(dw, (CamelStream *)mem) > 0)
			magic_type = gnome_vfs_get_mime_type_for_data(mem->buffer->data, mem->buffer->len);
		camel_object_unref(mem);
	}

	d(printf("snooped part, magic_type '%s' name_type '%s'\n", magic_type, name_type));

	/* If GNOME-VFS doesn't recognize the data by magic, but it
	 * contains English words, it will call it text/plain. If the
	 * filename-based check came up with something different, use
	 * that instead and if it returns "application/octet-stream"
	 * try to do better with the filename check.
	 */
	
	if (magic_type) {
		if (name_type
		    && (!strcmp(magic_type, "text/plain")
			|| !strcmp(magic_type, "application/octet-stream")))
			return name_type;
		else
			return magic_type;
	} else
		return name_type;

	/* We used to load parts to check their type, we dont anymore,
	   see bug #11778 for some discussion */
}

/* RFC 1740 */
static void
emf_multipart_appledouble(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);
	
	if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	/* try the data fork for something useful, doubtful but who knows */
	em_format_part(emf, stream, camel_multipart_get_part(mp, 1));
}

/* RFC ??? */
static void
emf_multipart_mixed(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);
	int i, nparts;

	if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(emf, stream, part);
		return;
	}
	
	nparts = camel_multipart_get_number(mp);	
	for (i = 0; i < nparts; i++) {
		/* FIXME: separate part markers ...
		if (i != 0)
		camel_stream_printf(stream, "<hr>\n");*/
		
		part = camel_multipart_get_part(mp, i);
		em_format_part(emf, stream, part);
	}
}

/* RFC 1740 */
static void
emf_multipart_alternative(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);
	int i, nparts;
	CamelMimePart *best = NULL;

	if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	/* as pre rfc, find the last part we know how to display */
	nparts = camel_multipart_get_number(mp);
	for (i = 0; i < nparts; i++) {
		CamelMimePart *part = camel_multipart_get_part(mp, i);
		CamelContentType *type = camel_mime_part_get_content_type (part);
		char *mime_type = camel_content_type_simple (type);
		
		camel_strdown (mime_type);

		/*if (want_plain && !strcmp (mime_type, "text/plain"))
		  return part;*/

		if (em_format_find_handler(emf, mime_type)
		    || (best == NULL && em_format_fallback_handler(emf, mime_type)))
			best = part;

		g_free(mime_type);
	}

	if (best)
		em_format_part(emf, stream, best);
	else
		emf_multipart_mixed(emf, stream, part, info);
}

static void
emf_multipart_encrypted(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipartEncrypted *mpe;
	CamelMimePart *mime_part;
	CamelCipherContext *cipher;
	CamelException ex;
	const char *protocol;

	/* Currently we only handle RFC2015-style PGP encryption. */
	protocol = camel_content_type_param (((CamelDataWrapper *) part)->mime_type, "protocol");
	if (!protocol || strcmp (protocol, "application/pgp-encrypted") != 0)
		return emf_multipart_mixed(emf, stream, part, info);
	
	mpe = (CamelMultipartEncrypted *)camel_medium_get_content_object((CamelMedium *)part);

	if (!CAMEL_IS_MULTIPART_ENCRYPTED(mpe)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	camel_exception_init (&ex);
	cipher = camel_gpg_context_new(emf->session);
	mime_part = camel_multipart_encrypted_decrypt(mpe, cipher, &ex);
	camel_object_unref(cipher);
	
	if (camel_exception_is_set(&ex)) {
		/* FIXME: error handler */
		em_format_format_error(emf, stream, camel_exception_get_description(&ex));
		camel_exception_clear(&ex);
		return;
	}

	em_format_part(emf, stream, mime_part);
	camel_object_unref(mime_part);
}

static void
emf_write_related(EMFormat *emf, CamelStream *stream, EMFormatPURI *puri)
{
	em_format_format_content(emf, stream, puri->part);
	camel_stream_close(stream);
}

/* RFC 2387 */
static void
emf_multipart_related(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);
	CamelMimePart *body_part, *display_part = NULL;
	CamelContentType *content_type;
	const char *location, *start;
	int i, nparts;
	CamelURL *base_save = NULL;
	struct _EMFormatPURITree *ptree;
	EMFormatPURI *puri, *purin;

	if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	/* FIXME: put this stuff in a shared function */
	nparts = camel_multipart_get_number(mp);	
	content_type = camel_mime_part_get_content_type(part);
	start = camel_content_type_param (content_type, "start");
	if (start && strlen(start)>2) {
		int len;
		const char *cid;

		/* strip <>'s */
		len = strlen (start) - 2;
		start++;
		
		for (i=0; i<nparts; i++) {
			body_part = camel_multipart_get_part(mp, i);
			cid = camel_mime_part_get_content_id(body_part);
			
			if (cid && !strncmp(cid, start, len) && strlen(cid) == len) {
				display_part = body_part;
				break;
			}
		}
	} else {
		display_part = camel_multipart_get_part(mp, 0);
	}
	
	if (display_part == NULL) {
		emf_multipart_mixed(emf, stream, part, info);
		return;
	}
	
	/* stack of present location and pending uri's */
	location = camel_mime_part_get_content_location(part);
	if (location) {
		d(printf("setting content location %s\n", location));
		base_save = emf->base;
		emf->base = camel_url_new(location, NULL);
	}
	em_format_push_level(emf);

	/* queue up the parts for possible inclusion */
	for (i = 0; i < nparts; i++) {
		body_part = camel_multipart_get_part(mp, i);
		if (body_part != display_part) {
			puri = em_format_add_puri(emf, sizeof(EMFormatPURI), NULL, body_part, emf_write_related);
			d(printf(" part '%s' '%s' added\n", puri->uri?puri->uri:"", puri->cid));
		}
	}
	
	em_format_part(emf, stream, display_part);
	camel_stream_flush(stream);

	ptree = emf->pending_uri_level;
	puri = (EMFormatPURI *)ptree->uri_list.head;
	purin = puri->next;
	while (purin) {
		if (purin->use_count == 0) {
			d(printf("part '%s' '%s' used '%d'\n", purin->uri?purin->uri:"", purin->cid, purin->use_count));
			if (purin->func == emf_write_related)
				em_format_part(emf, stream, puri->part);
			else
				printf("unreferenced uri generated by format code: %s\n", purin->uri?purin->uri:purin->cid);
		}
		puri = purin;
		purin = purin->next;
	}
	em_format_pull_level(emf);
	
	if (location) {
		camel_url_free(emf->base);
		emf->base = base_save;
	}
}

/* this is only a fallback implementation, implementations should override */
static void
emf_multipart_signed(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelMimePart *cpart, *spart;
	CamelMultipartSigned *mps;
	CamelCipherValidity *valid = NULL;
	CamelException ex;
	const char *message = NULL;
	gboolean good = FALSE;
	CamelCipherContext *cipher;

	mps = (CamelMultipartSigned *)camel_medium_get_content_object((CamelMedium *)part);
	if (!CAMEL_IS_MULTIPART_SIGNED(mps)
	    || (cpart = camel_multipart_get_part((CamelMultipart *)mps, CAMEL_MULTIPART_SIGNED_CONTENT)) == NULL) {
		em_format_format_source(emf, stream, part);
		return;
	}

	em_format_part(emf, stream, cpart);

	spart = camel_multipart_get_part((CamelMultipart *)mps, CAMEL_MULTIPART_SIGNED_SIGNATURE);
	camel_exception_init(&ex);
	if (spart == NULL) {
		message = _("No signature present");
	} else if (emf->session == NULL) {
		message = _("Session not initialised");
	} else if ((cipher = camel_gpg_context_new(emf->session)) == NULL) {
		message = _("Could not create signature verfication context");
	} else {
		valid = camel_multipart_signed_verify(mps, cipher, &ex);
		camel_object_unref(cipher);
		if (valid) {
			good = camel_cipher_validity_get_valid(valid);
			message = camel_cipher_validity_get_description(valid);
		} else {
			message = camel_exception_get_description(&ex);
		}
	}
	
	if (good)
		em_format_format_error(emf, stream, _("This message is digitally signed and has been found to be authentic."));
	else
		em_format_format_error(emf, stream, _("This message is digitally signed but can not be proven to be authentic."));
	
	if (message)
		em_format_format_error(emf, stream, message);
	
	camel_exception_clear(&ex);
	camel_cipher_validity_free(valid);
}

/* this is only a fallback, any implementer should implement */
static void
emf_message_rfc822(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const EMFormatHandler *info)
{
	CamelDataWrapper *dw = camel_medium_get_content_object((CamelMedium *)part);

	if (!CAMEL_IS_MIME_MESSAGE(dw)) {
		em_format_format_source(emf, stream, part);
		return;
	}

	em_format_format_message(emf, stream, (CamelMedium *)dw);
}

static EMFormatHandler type_builtin_table[] = {
	{ "multipart/alternative", emf_multipart_alternative },
	{ "multipart/appledouble", emf_multipart_appledouble },
	{ "multipart/encrypted", emf_multipart_encrypted },
	{ "multipart/mixed", emf_multipart_mixed },
	{ "multipart/signed", emf_multipart_signed },
	{ "multipart/related", emf_multipart_related },
	{ "multipart/*", emf_multipart_mixed },
	{ "message/rfc822", emf_message_rfc822 },
	{ "message/news", emf_message_rfc822 },
	{ "message/*", emf_message_rfc822 },
};

static void
emf_builtin_init(EMFormatClass *klass)
{
	int i;

	for (i=0;i<sizeof(type_builtin_table)/sizeof(type_builtin_table[0]);i++)
		g_hash_table_insert(klass->type_handlers, type_builtin_table[i].mime_type, &type_builtin_table[i]);
}
