/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* RDF viewer Evolution Executive Summary Component.
 * Bonoboised by Iain Holmes  <iain@helixcode.com>
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Based on code from Portaloo
 * Channel retrieval tool
 *
 * (C) 1998 Alan Cox.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <glib.h>
#include <gnome.h>
#include <bonobo.h>
#include <gnome-xml/parser.h>

#include <evolution-services/executive-summary-component.h>
#include <evolution-services/executive-summary-component-view.h>
#include <liboaf/liboaf.h>
#include <libgnomevfs/gnome-vfs.h>

int xmlSubstituteEntitiesDefaultValue = 1;	/* DV thinks of everything */

static int wipe_trackers = FALSE;
static int running_views = 0;

static BonoboGenericFactory *factory = NULL;
#define RDF_SUMMARY_ID "OAFIID:GNOME_Evolution_Summary_rdf_SummaryComponentFactory"

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
		free (wb);
	}
	
	wb = w = malloc (3 * strlen (p));
	
	if (w == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}
	
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
	   ExecutiveSummaryComponentView *view,
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
	char *t;
	char n[512];
	char *tmp;
	
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
		exit(1);
	}

	t = layer_find(channel->childs, "title", "No title");
/*  	g_string_append (html, tmp); */
	executive_summary_component_view_set_title (view, t);

	tmp = g_strdup_printf ("%s",
			       layer_find(channel->childs, "description", ""));
	g_string_append (html, tmp);
	g_free (tmp);

	if (image && !wipe_trackers) {		
		g_print ("URL: %s\n", layer_find_url (image->childs, "url",
						      "green-apple.png"));
		executive_summary_component_view_set_icon (view,
							   layer_find_url 
							   (image->childs, 
							    "url", "apple-green.png"));
	}

	g_string_append (html, "<br clear=all><FONT size=\"-1\" face=\"helvetica\"><P><UL>\n");

	for (i = 0; i < items; i++) {
		char *p = layer_find (item[i]->childs, "title", "No information");
		
		if(i == limit)
			g_string_append (html, "--\n");
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
			tmp = g_strdup_printf ("<LI><A href=\"%s\">\n", x+4);
			g_string_append (html, tmp);
			g_free (tmp);
		}
		else {
			tmp = g_strdup_printf ("<LI><A href=\"%s\">\n", layer_find_url(item[i]->childs, "link", ""));
			g_string_append (html, tmp);
			g_free (tmp);
		}

		tmp = g_strdup_printf ("%s\n</A>\n", p);
		g_string_append (html, tmp);
		g_free (tmp);
	}
	g_string_append (html, "</UL></FONT>\n");
}

/********* ExecutiveSummaryComponent section **************/
static void 
view_destroyed (GtkObject *object,
		gpointer data)
{
	running_views--;
	if (running_views <= 0) {
		gtk_main_quit ();
	}
}

static int
download (ExecutiveSummaryComponentView *view)
{
	GString *rdf;
	GString *html;
	char *xml;
	GnomeVFSHandle *handle = NULL;
	GnomeVFSResult result;
	xmlDocPtr doc;
	char *location;
	int len = 0;

	/* Download the RDF file here */
	/* Then parse it */
	/* The update it */

	location = "/home/iain/gnotices.rdf";
	result = gnome_vfs_open (&handle, location, GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		char *emsg;

		emsg = g_strdup_printf ("<b>Cannot open location:<br>%s</b>",
					location);
		executive_summary_component_view_set_html (view, emsg);
		g_free (emsg);
		return FALSE;
	}

	rdf = g_string_new ("");

	while (1) {
		char buffer[4096];
		GnomeVFSFileSize size;

		memset (buffer, 0x00, 4096);

		result = gnome_vfs_read (handle, buffer, 4096, &size);
		if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
			executive_summary_component_view_set_html (view,
								   "<b>Error reading data.</b>");
			g_string_free (rdf, TRUE);
			return FALSE;
		}

		if (size == 0) {
			break;
		}

		rdf = g_string_append (rdf, buffer);
		len += size;
	}

	gnome_vfs_close (handle);
	xml = rdf->str;
	g_string_free (rdf, FALSE);

	doc = xmlParseMemory (xml, len);
	if (doc == NULL) {
		g_warning ("Unable to parse document.");
		return FALSE;
	}
	
	g_free (xml);
	html = g_string_new ("");

	tree_walk (doc->root, view, html);
	executive_summary_component_view_set_html (view, html->str);
	g_string_free (html, TRUE);
	return FALSE;
}

static void
create_view (ExecutiveSummaryComponent *component,
	     ExecutiveSummaryComponentView *view,
	     void *closure)
{
	char *html = "<b>Loading RDF file. . .<br>Please wait</b>";
	
	executive_summary_component_view_construct (view, component, NULL,
						    html, "Downloading",
						    "apple-green.png");
	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    GTK_SIGNAL_FUNC (view_destroyed), NULL);
	running_views++;
	g_idle_add (download, view);
}

static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    void *closure)
{
	ExecutiveSummaryComponent *component;

	component = executive_summary_component_new (create_view, NULL);
	return BONOBO_OBJECT (component);
}

static void
factory_init (void)
{
	if (factory != NULL) {
		return;
	}

	factory = bonobo_generic_factory_new (RDF_SUMMARY_ID, factory_fn, NULL);
	if (factory == NULL) {
		g_error ("Cannot initialize factory");
	}
}

int 
main (int argc, 
      char *argv[])
{
	CORBA_ORB orb;

	gnome_init_with_popt_table ("RDF-Summary", VERSION,
				    argc, argv, oaf_popt_options, 0, NULL);
	orb = oaf_init (argc, argv);
	gnome_vfs_init ();

	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error ("Could not initialize Bonobo");
	}

	factory_init ();
	bonobo_main ();
	return 0;

#if 0
	doc=xmlParseMemory(document, docp);
	
	if(doc==NULL)
	{
		fprintf(stderr, "Unable to parse document.\n");
		exit(1);
	}
	
	tree_walk(doc->root);
	
	if(rename(buf, nam))
		perror("rename");
	return 0;
#endif

}
	
