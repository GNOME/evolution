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
#include <evolution-services/executive-summary-html-view.h>
#include <liboaf/liboaf.h>
#include <libgnomevfs/gnome-vfs.h>

int xmlSubstituteEntitiesDefaultValue = 1;	/* DV thinks of everything */

static int wipe_trackers = FALSE;
static int running_views = 0;

static BonoboGenericFactory *factory = NULL;
#define RDF_SUMMARY_ID "OAFIID:GNOME_Evolution_Summary_rdf_SummaryComponentFactory"

enum {
	PROPERTY_TITLE,
	PROPERTY_ICON
};

struct _RdfSummary {
	BonoboObject *component;
	BonoboObject *view;
	BonoboPropertyBag *bag;
	BonoboPropertyControl *property_control;

	GtkWidget *rdf;
	GtkWidget *g_limit;

	char *title;
	char *icon;
	char *location;
	int limit;
};
typedef struct _RdfSummary RdfSummary;

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
	   RdfSummary *summary,
	   GString *html)
{
	BonoboArg *arg;
	xmlNodePtr walk;
	xmlNodePtr rewalk = root;
	xmlNodePtr channel = NULL;
	xmlNodePtr image = NULL;
	xmlNodePtr item[16];
	int items = 0;
	int limit = summary->limit;
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

	arg = bonobo_arg_new (BONOBO_ARG_STRING);
	BONOBO_ARG_SET_STRING (arg, t);
	bonobo_property_bag_set_value (summary->bag,
				       "window_title", (const BonoboArg *) arg,
				       NULL);
	bonobo_arg_release (arg);

#if 0
	tmp = g_strdup_printf ("%s",
			       layer_find(channel->childs, "description", ""));
	g_string_append (html, tmp);
	g_free (tmp);
#endif

	if (image && !wipe_trackers) {		
		char *icon;

		icon = layer_find_url (image->childs, "url", "apple-red.png");
		arg = bonobo_arg_new (BONOBO_ARG_STRING);
		BONOBO_ARG_SET_STRING (arg, icon);
		bonobo_property_bag_set_value (summary->bag,
					       "window_icon", 
					       (const BonoboArg *) arg, NULL);
		bonobo_arg_release (arg);
			
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
	RdfSummary *summary = (RdfSummary *) data;

	g_free (summary->title);
	g_free (summary->icon);
	g_free (summary);

	running_views--;
	if (running_views <= 0) {
		gtk_main_quit ();
	}
}

static int
download (RdfSummary *summary)
{
	ExecutiveSummaryHtmlView *view;
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

	g_print ("Starting download\n");
	view = EXECUTIVE_SUMMARY_HTML_VIEW (summary->view);
	result = gnome_vfs_open (&handle, summary->location, 
				 GNOME_VFS_OPEN_READ);
	if (result != GNOME_VFS_OK) {
		char *emsg;

		emsg = g_strdup_printf ("<b>Cannot open location:<br>%s</b>",
					summary->location);
		executive_summary_html_view_set_html (view, emsg);
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
			executive_summary_html_view_set_html (view,
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

	tree_walk (doc->root, summary, html);
	executive_summary_html_view_set_html (view, html->str);
	g_string_free (html, TRUE);

	g_print ("Finished Download\n");
	return FALSE;
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg *arg,
	  guint arg_id,
	  gpointer user_data)
{
	RdfSummary *summary = (RdfSummary *) user_data;

	switch (arg_id) {
	case PROPERTY_TITLE:
		BONOBO_ARG_SET_STRING (arg, summary->title);
		break;

	case PROPERTY_ICON:
		BONOBO_ARG_SET_STRING (arg, summary->icon);
		break;
		
	default:
		break;
	}
}

static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg *arg,
	  guint arg_id,
	  gpointer user_data)
{
	RdfSummary *summary = (RdfSummary *) user_data;

	switch (arg_id) {
	case PROPERTY_TITLE:
		if (summary->title)
			g_free (summary->title);

		summary->title = g_strdup (BONOBO_ARG_GET_STRING (arg));
		bonobo_property_bag_notify_listeners (bag, "window_title",
						      arg, NULL);
		break;

	case PROPERTY_ICON:
		if (summary->icon)
			g_free (summary->icon);

		summary->icon = g_strdup (BONOBO_ARG_GET_STRING (arg));
		bonobo_property_bag_notify_listeners (bag, "window_icon",
						      arg, NULL);
		break;

	default:
		break;
	}
}

static void
entry_changed (GtkEntry *entry,
		  RdfSummary *summary)
{
	bonobo_property_control_changed (summary->property_control, NULL);
}

static BonoboControl *
property_control (BonoboPropertyControl *property_control,
		  int page_num,
		  gpointer user_data)
{
	BonoboControl *control;
	RdfSummary *summary = (RdfSummary *) user_data;
	GtkWidget *container, *label, *hbox;
	char *climit;

	container = gtk_vbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (container), 2);
	hbox = gtk_hbox_new (FALSE, 2);

	label = gtk_label_new ("Location:");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	summary->rdf = gtk_entry_new ();
	if (summary->location)
		gtk_entry_set_text (GTK_ENTRY (summary->rdf), summary->location);

	gtk_signal_connect (GTK_OBJECT (summary->rdf), "changed",
			    GTK_SIGNAL_FUNC (entry_changed), summary);

	gtk_box_pack_start (GTK_BOX (hbox), summary->rdf, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 2);

	label = gtk_label_new ("Maximum number of entries:");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	summary->g_limit = gtk_entry_new ();
	climit = g_strdup_printf ("%d", summary->limit);
	gtk_entry_set_text (GTK_ENTRY (summary->g_limit), climit);
	g_free (climit);

	gtk_signal_connect (GTK_OBJECT (summary->g_limit), "changed",
			    GTK_SIGNAL_FUNC (entry_changed), summary);

	gtk_box_pack_start (GTK_BOX (hbox), summary->g_limit, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);
	gtk_widget_show_all (container);

	control = bonobo_control_new (container);
	return control;
}

static void
property_action (GtkObject *property_control,
		 int page_num,
		 Bonobo_PropertyControl_Action action,
		 RdfSummary *summary)
{
	switch (action) {
	case Bonobo_PropertyControl_APPLY:
		g_free (summary->location);
		summary->location = g_strdup (gtk_entry_get_text (GTK_ENTRY (summary->rdf)));
		summary->limit = atoi (gtk_entry_get_text (GTK_ENTRY (summary->g_limit)));
		g_idle_add ((GSourceFunc) download, summary);
		break;
 
	case Bonobo_PropertyControl_HELP:
		g_print ("HELP: Page %d!\n", page_num);
		break;

	default:
		break;
	}
}

static BonoboObject *
create_view (ExecutiveSummaryComponentFactory *_factory,
	     void *closure)
{
	RdfSummary *summary;
	BonoboObject *component, *view;
	BonoboEventSource *event_source;
	BonoboPropertyBag *bag;
	BonoboPropertyControl *property;
	char *html = "<b>Loading RDF file. . .<br>Please wait</b>";
	
	summary = g_new (RdfSummary, 1);
	summary->icon = g_strdup ("apple-green.png");
	summary->title = g_strdup ("Downloading...");
	summary->location = g_strdup ("http://news.gnome.org/gnome-news/rdf");
	summary->limit = 10;

	component = executive_summary_component_new ();
	gtk_signal_connect (GTK_OBJECT (component), "destroy",
			    GTK_SIGNAL_FUNC (view_destroyed), summary);

	summary->component = component;

	/* Share the event source between the ExecutiveSummaryHtmlView and the
	   BonoboPropertyControl as we can only have one Bonobo::EventSource
	   interface aggregated */
	event_source = bonobo_event_source_new ();

	view = executive_summary_html_view_new_full (event_source);
	summary->view = view;
	executive_summary_html_view_set_html (EXECUTIVE_SUMMARY_HTML_VIEW (view),
					      html);
	bonobo_object_add_interface (component, view);

	bag = bonobo_property_bag_new (get_prop, set_prop, summary);
	summary->bag = bag;
	bonobo_property_bag_add (bag,
				 "window_title", PROPERTY_TITLE,
				 BONOBO_ARG_STRING, NULL,
				 "The title of this component's window", 0);
	bonobo_property_bag_add (bag,
				 "window_icon", PROPERTY_ICON,
				 BONOBO_ARG_STRING, NULL,
				 "The icon for this component's window", 0);
	bonobo_object_add_interface (component, BONOBO_OBJECT(bag));
				 
	property = bonobo_property_control_new_full (property_control, 1,
						     event_source,
						     summary);
	summary->property_control = property;

	gtk_signal_connect (GTK_OBJECT (property), "action",
			    GTK_SIGNAL_FUNC (property_action), summary);

	bonobo_object_add_interface (component, BONOBO_OBJECT(property));

	running_views++;
	gtk_timeout_add (5000, (GSourceFunc) download, summary);

	return component;
}

static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    void *closure)
{
	BonoboObject *component_factory;

	component_factory = executive_summary_component_factory_new (create_view, NULL);
	return component_factory;
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

	if (factory != NULL)
		bonobo_object_unref (BONOBO_OBJECT (factory));

	return 0;
}
	
