/*
 * e-summary-rdf.c: RDF summary bit.
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 *          
 * Based on code by Alan Cox
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <gal/widgets/e-unicode.h>
#include <libgnomevfs/gnome-vfs.h>
#include "e-summary.h"

struct _ESummaryRDF {
	GList *rdfs;

	char *html;
};

typedef struct _RDF {
	char *uri;
	char *html;
	GnomeVFSAsyncHandle *handle;
	GString *string;
	char *buffer;

	xmlDocPtr cache;
	ESummary *summary;

	gboolean shown;
} RDF;

int xmlSubstituteEntitiesDefaultValue = 1;
static int wipe_trackers = FALSE;

char *
e_summary_rdf_get_html (ESummary *summary)
{
	GList *rdfs;
	char *html;
	GString *string;

	if (summary->rdf == NULL) {
		return NULL;
	}

	string = g_string_new ("");
	for (rdfs = summary->rdf->rdfs; rdfs; rdfs = rdfs->next) {
		if (((RDF *)rdfs->data)->html == NULL) {
			continue;
		}

		g_string_append (string, ((RDF *)rdfs->data)->html);
	}

	html = string->str;
	g_string_free (string, FALSE);
	return html;
}

/************ RDF Parser *******************/

static char *
layer_find (xmlNodePtr node, 
	    char *match, 
	    char *fail)
{
	while (node!=NULL) {
#ifdef RDF_DEBUG
		xmlDebugDumpNode (stdout, node, 32);
		printf("%s.\n", node->name);
#endif
		if (strcasecmp (node->name, match)==0) {
			if (node->childs != NULL && node->childs->content != NULL) {
				return node->childs->content;
			} else {
				return fail;
			}
		}
		node = node->next;
	}
	return fail;
}

static char *
layer_find_url (xmlNodePtr node, 
		char *match, 
		char *fail)
{
	char *p = layer_find (node, match, fail);
	char *r = p;
	static char *wb = NULL;
	char *w;
	
	if (wb) {
		g_free (wb);
	}
	
	wb = w = g_malloc (3 * strlen (p));
	
	if (*r == ' ') r++;	/* Fix UF bug */

	while (*r) {
		if (memcmp (r, "&amp;", 5) == 0) {
			*w++ = '&';
			r += 5;
			continue;
		}
		if (memcmp (r, "&lt;", 4) == 0) {
			*w++ = '<';
			r += 4;
			continue;
		}
		if (memcmp (r, "&gt;", 4) == 0) {
			*w++ = '>';
			r += 4;
			continue;
		}
		if (*r == '"' || *r == ' '){
			*w++ = '%';
			*w++ = "0123456789ABCDEF"[*r/16];
			*w++ = "0123456789ABCDEF"[*r&15];
			r++;
			continue;
		}
		*w++ = *r++;
	}
	*w = 0;
	return wb;
}

static void 
tree_walk (xmlNodePtr root,
	   RDF *r,
	   GString *html)
{
	xmlNodePtr walk;
	xmlNodePtr rewalk = root;
	xmlNodePtr channel = NULL;
	xmlNodePtr image = NULL;
	xmlNodePtr item[16];
	int items = 0;
	int limit = 10;
	int i;
	char *t, *u;
	char *tmp;

	/* FIXME: Need arrows */
	if (r->shown == FALSE) {
		char *p;

		/* FIXME: Hash table & UID */
		p = g_strdup_printf ("<font size=\"-2\"><a href=\"rdf://%d\">(+)</a></font> ", r);
		g_string_append (html, p);
		g_free (p);
	} else {
		char *p;

		/* FIXME: Hash table & UID */
		p = g_strdup_printf ("<font size=\"-2\"><a href=\"rdf://%d\">(-)</a></font>", r);
		g_string_append (html, p);
		g_free (p);
	}
	
	do {
		walk = rewalk;
		rewalk = NULL;
		
		while (walk!=NULL){
#ifdef RDF_DEBUG
			printf ("%p, %s\n", walk, walk->name);
#endif
			if (strcasecmp (walk->name, "rdf") == 0) {
				rewalk = walk->childs;
				walk = walk->next;
				continue;
			}
			if (strcasecmp (walk->name, "rss") == 0){
				rewalk = walk->childs;
				walk = walk->next;
				continue;
			}
			/* This is the channel top level */
#ifdef RDF_DEBUG
			printf ("Top level '%s'.\n", walk->name);
#endif
			if (strcasecmp (walk->name, "channel") == 0) {
				channel = walk;
				rewalk = channel->childs;
			}
			if (strcasecmp (walk->name, "image") == 0) {
				image = walk;
				g_print ("Image\n");
			}
			if (strcasecmp (walk->name, "item") == 0 && items < 16) {
				item[items++] = walk;
			}
			walk = walk->next;
		}
	}
	while (rewalk);
	
	if (channel == NULL) {
		fprintf(stderr, "No channel definition.\n");
		return;
	}

	t = layer_find(channel->childs, "title", "");
	u = layer_find(channel->childs, "link", "");

	if (*u != '\0') {
		char *full;

		full = g_strdup_printf ("<a href=\"%s\">", u);
		g_string_append (html, full);
	}
	g_string_append (html, e_utf8_from_locale_string (t));
	if (*u != '\0') {
		g_string_append (html, "</a>");
	}
	g_string_append (html, "</b></dt>");

	if (r->shown == FALSE) {
		return;
	}

	g_string_append (html, "<ul>");

	items = MIN (limit, items);
	for (i = 0; i < items; i++) {
		char *p = layer_find (item[i]->childs, "title", "No information");
		
		if (wipe_trackers) {
			char *p = layer_find_url (item[i]->childs, "link", "");
			char *x = strchr (p, '?');
			unsigned char *r, *w;
			int n;
			if (x == NULL)
				continue;
			x++;
			r = x;
			w = x;
			while (*r) {
				if (*r == '+') {
					*w++ = ' ';
				} else if (*r == '%') {
					sscanf (r+1, "%02x", &n);
					*w++ = n;
					r += 2;
				} else {
					*w++ = *r;
					}
				r++;
			}
			*w = 0;
			tmp = g_strdup_printf ("<LI><font size=\"-1\"><A href=\"%s\">\n", x+4);
			g_string_append (html, tmp);
			g_free (tmp);
		}
		else {
			tmp = g_strdup_printf ("<LI><font size=\"-1\"><A href=\"%s\">\n", layer_find_url(item[i]->childs, "link", ""));
			g_string_append (html, tmp);
			g_free (tmp);
		}
		
		tmp = g_strdup_printf ("%s\n</A></font></li>", e_utf8_from_locale_string (p));
		g_string_append (html, tmp);
		g_free (tmp);
	}
	g_string_append (html, "</UL></dl>");
}

static void
display_doc (RDF *r)
{
	GString *html;

	html = g_string_new ("<dl><dt><img src=\"ico-rdf.png\" align=\"middle\" "
			     "width=\"48\" height=\"48\"><b>");

	tree_walk (r->cache->root, r, html);

	if (r->html != NULL) {
		g_free (r->html);
	}
	r->html = html->str;
	g_string_free (html, FALSE);

	e_summary_draw (r->summary);
}

static void
close_callback (GnomeVFSAsyncHandle *handle,
		GnomeVFSResult result,
		RDF *r)
{
	char *xml;
	xmlDocPtr doc;

	if (r->handle == NULL) {
		g_free (r->buffer);
		g_string_free (r->string, TRUE);
		return;
	}

	r->handle = NULL;
	g_free (r->buffer);
	xml = r->string->str;
	g_string_free (r->string, FALSE);

	if (r->cache != NULL) {
		xmlFreeDoc (r->cache);
	}

	doc = xmlParseMemory (xml, strlen (xml));
	if (doc == NULL) {
		if (r->html != NULL) {
			g_free (r->html);
		}
		r->html = g_strdup ("<b>Error parsing XML</b>");

		e_summary_draw (r->summary);
		g_free (xml);
		return;
	}

	g_free (xml);
	r->cache = doc;

	/* Draw it */
	display_doc (r);
}

static void
read_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSResult result,
	       gpointer buffer,
	       GnomeVFSFileSize bytes_requested,
	       GnomeVFSFileSize bytes_read,
	       RDF *r)
{
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		r->html = g_strdup ("<b>Error downloading RDF</b>");

		e_summary_draw (r->summary);
		r->handle = NULL;
		gnome_vfs_async_close (handle, 
				       (GnomeVFSAsyncCloseCallback) close_callback, r);
		return;
	}

	if (bytes_read == 0) {
		gnome_vfs_async_close (handle,
				       (GnomeVFSAsyncCloseCallback) close_callback, r);
	} else {
		*((char *) buffer + bytes_read) = 0;
		g_string_append (r->string, (const char *) buffer);
		gnome_vfs_async_read (handle, buffer, 4095,
				      (GnomeVFSAsyncReadCallback) read_callback, r);
	}
}

static void
open_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSResult result,
	       RDF *r)
{
	if (result != GNOME_VFS_OK) {
		r->html = g_strdup ("<b>Error downloading RDF</b>");

		e_summary_draw (r->summary);
		return;
	}

	r->string = g_string_new ("");
	r->buffer = g_new (char, 4096);

	gnome_vfs_async_read (handle, r->buffer, 4095,
			      (GnomeVFSAsyncReadCallback) read_callback, r);
}

static void
e_summary_rdf_add_uri (ESummary *summary,
		       const char *uri)
{
	RDF *r;

	r = g_new0 (RDF, 1);
	r->summary = summary;
	r->uri = g_strdup (uri);
	r->shown = TRUE;
	summary->rdf->rdfs = g_list_prepend (summary->rdf->rdfs, r);

	gnome_vfs_async_open (&r->handle, r->uri, GNOME_VFS_OPEN_READ,
			      (GnomeVFSAsyncOpenCallback) open_callback, r);
}

static void
e_summary_rdf_protocol (ESummary *summary,
			const char *uri,
			void *closure)
{
	RDF *r;
	int a;

	a = atoi (uri + 6);
	if (a == 0) {
		g_warning ("A == 0");
		return;
	}

	r = (RDF *) GINT_TO_POINTER (a);
	r->shown = !r->shown;

	display_doc (r);
}

void
e_summary_rdf_init (ESummary *summary)
{
	ESummaryRDF *rdf;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	rdf = g_new0 (ESummaryRDF, 1);
	summary->rdf = rdf;

	e_summary_add_protocol_listener (summary, "rdf", e_summary_rdf_protocol, rdf);
	e_summary_rdf_add_uri (summary, "http://news.gnome.org/gnome-news/rdf");
	return;
}
