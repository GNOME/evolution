/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* RDF viewer Evolution Executive Summary Component.
 * Bonoboised by Iain Holmes  <iain@ximian.com>
 * Copyright (C) 2000 Ximian, Inc.
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
#include <gnome-xml/parser.h>
#include <gnome-xml/xmlmemory.h>

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-persist-stream.h>
#include <bonobo/bonobo-property.h>
#include <bonobo/bonobo-property-bag.h>
#include <bonobo/bonobo-property-control.h>
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
	GtkWidget *g_title;
	GtkWidget *g_update;
	GtkWidget *g_update_container;
	GtkAdjustment *adjustment;

	char *title;
	char *icon;
	char *location;
	int limit;
	gboolean showtitle;

	gboolean usetimer;
	int time;
	int timer;

	GString *str;
	char *buffer;

	GnomeVFSAsyncHandle *handle;

	xmlDocPtr cache;
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

	if (summary->showtitle) {
		BONOBO_ARG_SET_STRING (arg, t);
	} else {
		BONOBO_ARG_SET_STRING (arg, "");
	}

	bonobo_property_bag_set_value (summary->bag,
				       "window_title", 
				       (const BonoboArg *) arg, NULL);
	bonobo_arg_release (arg);

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

	g_warning ("RDF: Point 1");
	if (summary->handle)
		gnome_vfs_async_cancel (summary->handle);

	g_warning ("RDF: Point 2");

	if (summary->cache != NULL)
		xmlFreeDoc (summary->cache);

	g_free (summary->title);
	g_free (summary->icon);
	g_free (summary);

	running_views--;
	g_print ("Running_views: %d\n", running_views);
	if (running_views <= 0) {
		gtk_main_quit ();
	}
	g_warning ("RDF: Point 3");
}

/* PersistStream callbacks */
static void
load_from_stream (BonoboPersistStream *ps,
		  Bonobo_Stream stream,
		  Bonobo_Persist_ContentType type,
		  gpointer data,
		  CORBA_Environment *ev)
{
	RdfSummary *summary = (RdfSummary *) data;
	char *str;
	xmlChar *xml_str;
	xmlDocPtr doc;
	xmlNodePtr root, children;

	if (*type && g_strcasecmp (type, "application/x-rdf-summary") != 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	bonobo_stream_client_read_string (stream, &str, ev);
	if (ev->_major != CORBA_NO_EXCEPTION || str == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	doc = xmlParseDoc ((xmlChar *) str);
	
	if (doc == NULL) {
		g_warning ("Bad data: %s!", str);
		g_free (str);
		return;
	}

	g_free (str);

	root = doc->root;
	children = root->childs;
	while (children) {
		if (strcasecmp (children->name, "location") == 0) {
			xml_str = xmlNodeListGetString (doc, children->childs, 1);
			summary->location = g_strdup (xml_str);
			xmlFree (xml_str);

			children = children->next;
			continue;
		} 

		if (strcasecmp (children->name, "limit") == 0) {
			xml_str = xmlNodeListGetString (doc, children->childs, 1);
			summary->limit = atoi (xml_str);
			xmlFree (xml_str);

			children = children->next;
			continue;
		}

		if (strcasecmp (children->name, "showtitle") == 0) {
			xml_str = xmlNodeListGetString (doc, children->childs, 1);
			summary->showtitle = atoi (xml_str);
			xmlFree (xml_str);

			children = children->next;
			continue;
		}

		if (strcasecmp (children->name, "usetimer") == 0) {
			xml_str = xmlNodeListGetString (doc, children->childs, 1);
			summary->usetimer = atoi (xml_str);
			xmlFree (xml_str);

			children = children->next;
			continue;
		}

		if (strcasecmp (children->name, "timer") == 0) {
			xml_str = xmlNodeListGetString (doc, children->childs, 1);
			summary->time = atoi (xml_str);
			xmlFree (xml_str);

			children = children->next;
			continue;
		}

		g_print ("Unknown name: %s\n", children->name);
		children = children->next;
	}
	xmlFreeDoc (doc);
}

static char *
summary_to_string (RdfSummary *summary)
{
	xmlChar *out_str;
	int out_len = 0;
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNsPtr ns;
	char *tmp_str;
	
	doc = xmlNewDoc ("1.0");
	ns = xmlNewGlobalNs (doc, "http://www.ximian.com", "rdf");
	
	doc->root = xmlNewDocNode (doc, ns, "rdf-summary", NULL);

	xmlNewChild (doc->root, ns, "location", summary->location);
	tmp_str = g_strdup_printf ("%d", summary->limit);
	xmlNewChild (doc->root, ns, "limit", tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf ("%d", summary->showtitle);
	xmlNewChild (doc->root, ns, "showtitle", tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf ("%d", summary->usetimer);
	xmlNewChild (doc->root, ns, "usetimer", tmp_str);
	g_free (tmp_str);

	tmp_str = g_strdup_printf ("%d", summary->time);
	xmlNewChild (doc->root, ns, "timer", tmp_str);
	g_free (tmp_str);

	xmlDocDumpMemory (doc, &out_str, &out_len);

	return out_str;
}

static void
save_to_stream (BonoboPersistStream *ps,
		const Bonobo_Stream stream,
		Bonobo_Persist_ContentType type,
		gpointer data,
		CORBA_Environment *ev)
{
	RdfSummary *summary = (RdfSummary *) data;
	char *str;

	if (*type && g_strcasecmp (type, "application/x-rdf-summary") != 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	str = summary_to_string (summary);
	if (str)
		bonobo_stream_client_printf (stream, TRUE, ev, str);
	xmlFree (str);

	return;
}

static Bonobo_Persist_ContentTypeList *
content_types (BonoboPersistStream *ps,
	       void *closure,
	       CORBA_Environment *ev)
{
	return bonobo_persist_generate_content_types (1, "application/x-rdf-summary");
}

static void 
display_doc (RdfSummary *summary)
{
	GString *html;

	html = g_string_new ("");

	tree_walk (summary->cache->root, summary, html);
	executive_summary_html_view_set_html (EXECUTIVE_SUMMARY_HTML_VIEW (summary->view), html->str);
	g_string_free (html, TRUE);
}

static void
close_callback (GnomeVFSAsyncHandle *handle,
		GnomeVFSResult result,
		RdfSummary *summary)
{
	xmlDocPtr doc;
	char *xml;

	if (summary == NULL)
		return;

	summary->handle = NULL;
	g_free (summary->buffer);
	xml = summary->str->str;
	g_string_free (summary->str, FALSE);

	if (summary->cache != NULL)
		xmlFreeDoc (summary->cache);

	doc = xmlParseMemory (xml, strlen (xml));
	if (doc == NULL) {
		char *emsg;
		BonoboArg *arg;

		arg = bonobo_arg_new (BONOBO_ARG_STRING);
		BONOBO_ARG_SET_STRING (arg, _("Error"));
		bonobo_property_bag_set_value (summary->bag,
					       "window_title", 
					       (const BonoboArg *) arg,
					       NULL);
		bonobo_arg_release (arg);
		
		emsg = g_strdup_printf ("<b>Cannot open location:<br>%s</b>",
					summary->location);
		executive_summary_html_view_set_html (EXECUTIVE_SUMMARY_HTML_VIEW (summary->view), emsg);
		g_free (emsg);
		g_free (xml);
		return;
	}
	
	g_free (xml);

	/* Cache it for later */
	summary->cache = doc;

	/* Draw it */
	display_doc (summary);
}

static void
read_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSResult result,
	       gpointer buffer,
	       GnomeVFSFileSize bytes_requested,
	       GnomeVFSFileSize bytes_read,
	       RdfSummary *summary)
{
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		char *emsg;
		BonoboArg *arg;

		arg = bonobo_arg_new (BONOBO_ARG_STRING);
		BONOBO_ARG_SET_STRING (arg, _("Error"));
		bonobo_property_bag_set_value (summary->bag,
					       "window_title", 
					       (const BonoboArg *) arg,
					       NULL);
		bonobo_arg_release (arg);
		
		emsg = g_strdup_printf ("<b>Cannot open location:<br>%s</b>",
					summary->location);
		executive_summary_html_view_set_html (EXECUTIVE_SUMMARY_HTML_VIEW (summary->view), emsg);
		g_free (emsg);
		gnome_vfs_async_close (handle, 
				       (GnomeVFSAsyncCloseCallback) close_callback,
				       NULL);
		g_print ("NULLING\n");
		summary->handle = NULL;
	}

	if (bytes_read == 0) {
		/* EOF */
		gnome_vfs_async_close (handle, 
				       (GnomeVFSAsyncCloseCallback) close_callback,
				       summary);
	} else {
		*((char *) buffer + bytes_read) = 0;
		g_string_append (summary->str, (const char *) buffer);
		gnome_vfs_async_read (handle, buffer, 4095, 
				      (GnomeVFSAsyncReadCallback) read_callback,
				      summary);
	}
}

static void
open_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSResult result,
	       RdfSummary *summary)
{
	GList *uri;
	char *buffer;

	if (result != GNOME_VFS_OK) {
		char *emsg;
		BonoboArg *arg;

		arg = bonobo_arg_new (BONOBO_ARG_STRING);
		BONOBO_ARG_SET_STRING (arg, _("Error"));
		bonobo_property_bag_set_value (summary->bag,
					       "window_title", 
					       (const BonoboArg *) arg,
					       NULL);
		bonobo_arg_release (arg);
		
		emsg = g_strdup_printf ("<b>Cannot open location:<br>%s</b>",
					summary->location);
		executive_summary_html_view_set_html (EXECUTIVE_SUMMARY_HTML_VIEW (summary->view), emsg);
		g_free (emsg);
		summary->handle = NULL;
		return;
	}

	summary->str = g_string_new ("");
	summary->buffer = g_new (char, 4096);

	gnome_vfs_async_read (handle, summary->buffer, 4095, 
			      (GnomeVFSAsyncReadCallback) read_callback, 
			      summary);
}

static int
download (RdfSummary *summary)
{
	GnomeVFSAsyncHandle *handle;
	char *html = "<b>Loading RDF file. . .<br>Please wait</b>";
	
	executive_summary_html_view_set_html (EXECUTIVE_SUMMARY_HTML_VIEW (summary->view),
					      html);

	gnome_vfs_async_open (&handle, summary->location, GNOME_VFS_OPEN_READ,
			      (GnomeVFSAsyncOpenCallback) open_callback, 
			      summary);

	summary->handle = handle;
	return FALSE;
}

static void
download_cb (GtkWidget *w,
	     RdfSummary *summary)
{
	download (summary);
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg *arg,
	  guint arg_id,
	  CORBA_Environment *ev,
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
	  CORBA_Environment *ev,
	  gpointer user_data)
{
	RdfSummary *summary = (RdfSummary *) user_data;

	switch (arg_id) {
	case PROPERTY_TITLE:
		if (summary->title)
			g_free (summary->title);

		summary->title = g_strdup (BONOBO_ARG_GET_STRING (arg));
		g_print ("Notify listener!\n");
		bonobo_property_bag_notify_listeners (bag, "window_title",
						      arg, NULL);
		break;

	case PROPERTY_ICON:
		if (summary->icon)
			g_free (summary->icon);

		summary->icon = g_strdup (BONOBO_ARG_GET_STRING (arg));
		g_print ("Notify listener 2\n");
		bonobo_property_bag_notify_listeners (bag, "window_icon",
						      arg, NULL);
		break;

	default:
		break;
	}
}

static void
item_changed (GtkWidget *widget,
	      RdfSummary *summary)
{
	if (widget == summary->g_update) {
		summary->usetimer = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		gtk_widget_set_sensitive (summary->g_update_container, 
					  summary->usetimer);
	}

	bonobo_property_control_changed (summary->property_control, NULL);
}

static BonoboControl *
property_control (BonoboPropertyControl *property_control,
		  int page_num,
		  gpointer user_data)
{
	BonoboControl *control;
	RdfSummary *summary = (RdfSummary *) user_data;
	GtkWidget *container, *label, *hbox, *spinner, *button;
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
			    GTK_SIGNAL_FUNC (item_changed), summary);

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
			    GTK_SIGNAL_FUNC (item_changed), summary);

	gtk_box_pack_start (GTK_BOX (hbox), summary->g_limit, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 2);

	/* FIXME: Do this better? */
	label = gtk_label_new ("Show window title");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	summary->g_title = gtk_check_button_new ();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (summary->g_title),
				      summary->showtitle);

	gtk_signal_connect (GTK_OBJECT (summary->g_title), "toggled",
			    GTK_SIGNAL_FUNC (item_changed), summary);

	gtk_box_pack_start (GTK_BOX (hbox), summary->g_title, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 2);

	/* Update */
	hbox = gtk_hbox_new (FALSE, 2);
	label = gtk_label_new (_("Update automatically"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	summary->g_update = gtk_check_button_new ();
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (summary->g_update),
				      summary->usetimer);

	gtk_signal_connect (GTK_OBJECT (summary->g_update), "toggled",
			    GTK_SIGNAL_FUNC (item_changed), summary);
	gtk_box_pack_start (GTK_BOX (hbox), summary->g_update, TRUE, TRUE, 0);

	button = gtk_button_new_with_label (_("Update now"));
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (download_cb), summary);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);

	/* Timer */
	hbox = gtk_hbox_new (FALSE, 2);
	summary->g_update_container = hbox;

	label = gtk_label_new (_("Update every "));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	summary->adjustment = GTK_ADJUSTMENT(gtk_adjustment_new (summary->time / 1000 / 60,
								 1.0, 1000.0, 1.0, 10.0, 1.0));
	spinner = gtk_spin_button_new (summary->adjustment, 1.0, 0);
	gtk_signal_connect (GTK_OBJECT (spinner), "changed",
			    GTK_SIGNAL_FUNC (item_changed), summary);

	gtk_box_pack_start (GTK_BOX (hbox), spinner, FALSE, FALSE, 0);
	
	label = gtk_label_new (_("minutes"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (container), hbox, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (hbox, summary->usetimer);

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
	gboolean changed = FALSE;
	char *old_location;

	switch (action) {
	case Bonobo_PropertyControl_APPLY:
		old_location = summary->location;
		summary->showtitle = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (summary->g_title));
		summary->location = g_strdup (gtk_entry_get_text (GTK_ENTRY (summary->rdf)));
		if (strcmp (old_location, summary->location) != 0) 
			changed = TRUE;

		summary->limit = atoi (gtk_entry_get_text (GTK_ENTRY (summary->g_limit)));
		summary->time = summary->adjustment->value * 60 * 1000;
		if (summary->timer)
			gtk_timeout_remove (summary->timer);
		summary->timer = gtk_timeout_add (summary->time, 
						  (GSourceFunc)download, summary);
		
		if (changed)
			g_idle_add ((GSourceFunc) download, summary);
		else
			g_idle_add ((GSourceFunc) display_doc, summary);

		g_free (old_location);
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
	BonoboPersistStream *stream;
	BonoboPropertyBag *bag;
	BonoboPropertyControl *property;
	
	summary = g_new (RdfSummary, 1);
	summary->icon = g_strdup ("apple-green.png");
	summary->title = g_strdup ("Downloading...");
	summary->location = g_strdup ("http://news.gnome.org/gnome-news/rdf");
	summary->limit = 10;
	summary->showtitle = TRUE;

	summary->cache = NULL;
	
	summary->usetimer = TRUE;
	summary->time = 600000; /* 10 minutes */
	summary->timer = gtk_timeout_add (summary->time, (GSourceFunc) download,
					  summary);

	component = executive_summary_component_new ();
	gtk_signal_connect (GTK_OBJECT (component), "destroy",
			    GTK_SIGNAL_FUNC (view_destroyed), summary);

	summary->component = component;

	/* Share the event source between the ExecutiveSummaryHtmlView and the
	   BonoboPropertyControl as we can only have one Bonobo::EventSource
	   interface aggregated */
	event_source = bonobo_event_source_new ();

	/* Summary::HtmlView */
	view = executive_summary_html_view_new_full (event_source);

	summary->view = view;
	bonobo_object_add_interface (component, view);

	/* Bonobo::PropertyBag */
	bag = bonobo_property_bag_new_full (get_prop, set_prop, 
					    event_source, summary);

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

	/* Bonobo::PropertyControl */
	property = bonobo_property_control_new_full (property_control, 1,
						     event_source,
						     summary);
	summary->property_control = property;

	gtk_signal_connect (GTK_OBJECT (property), "action",
			    GTK_SIGNAL_FUNC (property_action), summary);

	bonobo_object_add_interface (component, BONOBO_OBJECT(property));

	/* Bonobo::PersistStream */
	stream = bonobo_persist_stream_new (load_from_stream, save_to_stream,
					    NULL, content_types, summary);
	bonobo_object_add_interface (component, BONOBO_OBJECT (stream));

	running_views++;
	gtk_idle_add ((GSourceFunc) download, summary);

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

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

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
}
	
