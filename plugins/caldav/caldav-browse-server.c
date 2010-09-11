/*
 * caldav-browse-server.c
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <libsoup/soup.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <libecal/e-cal.h>
#include <libedataserver/e-proxy.h>
#include <libedataserverui/e-cell-renderer-color.h>
#include <libedataserverui/e-passwords.h>

#include <e-util/e-dialog-utils.h>

#include "caldav-browse-server.h"

#define XC (xmlChar *)

enum {
	CALDAV_THREAD_SHOULD_SLEEP,
	CALDAV_THREAD_SHOULD_WORK,
	CALDAV_THREAD_SHOULD_DIE
};

enum {
	COL_BOOL_IS_LOADED,
	COL_STRING_HREF,
	COL_BOOL_IS_CALENDAR,
	COL_STRING_SUPPORTS,
	COL_STRING_DISPLAYNAME,
	COL_GDK_COLOR,
	COL_BOOL_HAS_COLOR,
	COL_BOOL_SENSITIVE
};

typedef void (*process_message_cb) (GObject *dialog, const gchar *msg_path, guint status_code, const gchar *msg_body, gpointer user_data);

static void send_xml_message (xmlDocPtr doc, gboolean depth_1, const gchar *url, GObject *dialog, process_message_cb cb, gpointer cb_user_data, const gchar *info);

static gchar *
xpath_get_string (xmlXPathContextPtr xpctx, const gchar *path_format, ...)
{
	gchar *res = NULL, *path, *tmp;
	va_list args;
	xmlXPathObjectPtr obj;

	g_return_val_if_fail (xpctx != NULL, NULL);
	g_return_val_if_fail (path_format != NULL, NULL);

	va_start (args, path_format);
	tmp = g_strdup_vprintf (path_format, args);
	va_end (args);

	if (1 || strchr (tmp, '@') == NULL) {
		path = g_strconcat ("string(", tmp, ")", NULL);
		g_free (tmp);
	} else {
		path = tmp;
	}

	obj = xmlXPathEvalExpression (XC path, xpctx);
	g_free (path);

	if (obj == NULL)
		return NULL;

	if (obj->type == XPATH_STRING)
		res = g_strdup ((gchar *) obj->stringval);

	xmlXPathFreeObject (obj);

	return res;
}

static gboolean
xpath_exists (xmlXPathContextPtr xpctx, xmlXPathObjectPtr *resobj, const gchar *path_format, ...)
{
	gchar *path;
	va_list args;
	xmlXPathObjectPtr obj;

	g_return_val_if_fail (xpctx != NULL, FALSE);
	g_return_val_if_fail (path_format != NULL, FALSE);

	va_start (args, path_format);
	path = g_strdup_vprintf (path_format, args);
	va_end (args);

	obj = xmlXPathEvalExpression (XC path, xpctx);
	g_free (path);

	if (obj && (obj->type != XPATH_NODESET || xmlXPathNodeSetGetLength (obj->nodesetval) == 0)) {
		xmlXPathFreeObject (obj);
		obj = NULL;
	}

	if (resobj)
		*resobj = obj;
	else if (obj != NULL)
		xmlXPathFreeObject (obj);

	return obj != NULL;
}

static gchar *
change_url_path (const gchar *base_url, const gchar *new_path)
{
	SoupURI *suri;
	gchar *url;

	g_return_val_if_fail (base_url != NULL, NULL);
	g_return_val_if_fail (new_path != NULL, NULL);

	suri = soup_uri_new (base_url);
	if (!suri)
		return NULL;

	soup_uri_set_path (suri, new_path);

	url = soup_uri_to_string (suri, FALSE);

	soup_uri_free (suri);

	return url;
}

static void
report_error (GObject *dialog, gboolean is_fatal, const gchar *msg)
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_DIALOG (dialog));
	g_return_if_fail (msg != NULL);

	if (is_fatal) {
		GtkWidget *content_area, *w;

		content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

		w = g_object_get_data (dialog, "caldav-info-label");
		gtk_widget_hide (w);

		w = g_object_get_data (dialog, "caldav-tree-sw");
		gtk_widget_hide (w);

		w = gtk_label_new (msg);
		gtk_widget_show (w);
		gtk_box_pack_start (GTK_BOX (content_area), w, TRUE, TRUE, 10);

		w = g_object_get_data (dialog, "caldav-new-url-entry");
		if (w)
			gtk_entry_set_text (GTK_ENTRY (w), "");
	} else {
		GtkLabel *label = g_object_get_data (dialog, "caldav-info-label");

		if (label)
			gtk_label_set_text (label, msg);
	}
}

static gboolean
check_soup_status (GObject *dialog, guint status_code, const gchar *msg_body, gboolean is_fatal)
{
	gchar *msg;

	if (status_code == 207)
		return TRUE;

	if (status_code == 401 || status_code == 403) {
		msg = g_strdup (_("Authentication failed. Server requires correct login."));
	} else if (status_code == 404) {
		msg = g_strdup (_("Given URL cannot be found."));
	} else {
		const gchar *phrase = soup_status_get_phrase (status_code);

		msg = g_strdup_printf (_("Server returned unexpected data.\n%d - %s"), status_code, phrase ? phrase : _("Unknown error"));
	}

	report_error (dialog, is_fatal, msg);

	g_free (msg);

	return FALSE;
}

struct test_exists_data {
	const gchar *href;
	gboolean exists;
};

static gboolean
test_href_exists_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	struct test_exists_data *ted = user_data;
	gchar *href = NULL;

	g_return_val_if_fail (model != NULL, TRUE);
	g_return_val_if_fail (iter != NULL, TRUE);
	g_return_val_if_fail (ted != NULL, TRUE);
	g_return_val_if_fail (ted->href != NULL, TRUE);

	gtk_tree_model_get (model, iter, COL_STRING_HREF, &href, -1);

	ted->exists = href && g_ascii_strcasecmp (href, ted->href) == 0;

	g_free (href);

	return ted->exists;
}

static void
add_collection_node_to_tree (GtkTreeStore *store, GtkTreeIter *parent_iter, const gchar *href)
{
	SoupURI *suri;
	const gchar *path;
	GtkTreeIter iter, loading_iter;
	struct test_exists_data ted;
	gchar *displayname, **tmp;

	g_return_if_fail (store != NULL);
	g_return_if_fail (GTK_IS_TREE_STORE (store));
	g_return_if_fail (href != NULL);

	suri = soup_uri_new (href);
	if (suri && suri->path && (*suri->path != '/' || strlen (suri->path) > 1))
		href = suri->path;

	ted.href = href;
	ted.exists = FALSE;

	gtk_tree_model_foreach (GTK_TREE_MODEL (store), test_href_exists_cb, &ted);

	if (ted.exists) {
		if (suri)
			soup_uri_free (suri);
		return;
	}

	path = href;
	tmp = g_strsplit (path, "/", -1);

	/* parent_iter is not set for the root folder node, where whole path is shown */
	if (tmp && parent_iter) {
		/* pick the last non-empty path part */
		gint idx = 0;

		while (tmp[idx]) {
			idx++;
		}

		idx--;

		while (idx >= 0 && !tmp[idx][0]) {
			idx--;
		}

		if (idx >= 0)
			path = tmp[idx];
	}

	displayname = soup_uri_decode (path);

	gtk_tree_store_append (store, &iter, parent_iter);
	gtk_tree_store_set (store, &iter,
		COL_BOOL_IS_LOADED, FALSE,
		COL_BOOL_IS_CALENDAR, FALSE,
		COL_STRING_HREF, href,
		COL_STRING_DISPLAYNAME, displayname ? displayname : path,
		COL_BOOL_SENSITIVE, TRUE,
		-1);

	g_free (displayname);
	g_strfreev (tmp);
	if (suri)
		soup_uri_free (suri);

	/* not localized "Loading...", because will be removed on expand immediately */
	gtk_tree_store_append (store, &loading_iter, &iter);
	gtk_tree_store_set (store, &loading_iter,
		COL_BOOL_IS_LOADED, FALSE,
		COL_BOOL_IS_CALENDAR, FALSE,
		COL_STRING_DISPLAYNAME, "Loading...",
		COL_BOOL_SENSITIVE, FALSE,
		-1);
}

/* called with "caldav-thread-mutex" unlocked; 'user_data' is parent tree iter, NULL for "User's calendars" */
static void
traverse_users_calendars_cb (GObject *dialog, const gchar *msg_path, guint status_code, const gchar *msg_body, gpointer user_data)
{
	xmlDocPtr doc;
	xmlXPathContextPtr xpctx;
	xmlXPathObjectPtr xpathObj;
	GtkTreeIter *parent_iter = user_data, par_iter;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	if (!check_soup_status (dialog, status_code, msg_body, TRUE))
		return;

	g_return_if_fail (msg_body != NULL);

	doc = xmlReadMemory (msg_body, strlen (msg_body), "response.xml", NULL, 0);
	if (!doc) {
		report_error (dialog, TRUE, _("Failed to parse server response."));
		return;
	}

	xpctx = xmlXPathNewContext (doc);
	xmlXPathRegisterNs (xpctx, XC "D", XC "DAV:");
	xmlXPathRegisterNs (xpctx, XC "C", XC "urn:ietf:params:xml:ns:caldav");
	xmlXPathRegisterNs (xpctx, XC "CS", XC "http://calendarserver.org/ns/");
	xmlXPathRegisterNs (xpctx, XC "IC", XC "http://apple.com/ns/ical/");

	xpathObj = xmlXPathEvalExpression (XC "/D:multistatus/D:response", xpctx);
	if (xpathObj && xpathObj->type == XPATH_NODESET) {
		GtkWidget *tree = g_object_get_data (G_OBJECT (dialog), "caldav-tree");
		GtkTreeStore *store = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (tree)));
		GtkTreeIter iter;
		gint i, n;

		n = xmlXPathNodeSetGetLength (xpathObj->nodesetval);
		for (i = 0; i < n; i++) {
			xmlXPathObjectPtr suppObj;
			GString *supports;
			gchar *href, *displayname, *color_str;
			GdkColor color;
			gchar *str;
			guint status;
			gboolean sensitive;

			#define response(_x) "/D:multistatus/D:response[%d]/" _x
			#define prop(_x) response ("D:propstat/D:prop/" _x)

			str = xpath_get_string (xpctx, response ("D:propstat/D:status"), i + 1);
			if (!str || !soup_headers_parse_status_line (str, NULL, &status, NULL) || status != 200) {
				g_free (str);
				continue;
			}

			g_free (str);

			if (!xpath_exists (xpctx, NULL, prop ("D:resourcetype/C:calendar"), i + 1)) {
				/* not a calendar node */

				if (user_data != NULL && xpath_exists (xpctx, NULL, prop ("D:resourcetype/D:collection"), i + 1)) {
					/* can be browseable, add node for loading */
					href = xpath_get_string (xpctx, response ("D:href"), i + 1);
					if (href && *href)
						add_collection_node_to_tree (store, parent_iter, href);

					g_free (href);
				}
				continue;
			}

			href = xpath_get_string (xpctx, response ("D:href"), i + 1);
			if (!href || !*href) {
				/* href should be there always */
				g_free (href);
				continue;
			}

			displayname = xpath_get_string (xpctx, prop ("D:displayname"), i + 1);
			color_str = xpath_get_string (xpctx, prop ("IC:calendar-color"), i + 1);
			if (color_str && !gdk_color_parse (color_str, &color)) {
				g_free (color_str);
				color_str = NULL;
			}

			sensitive = FALSE;
			supports = NULL;
			suppObj = NULL;
			if (xpath_exists (xpctx, &suppObj, prop ("C:supported-calendar-component-set/C:comp"), i + 1)) {
				if (suppObj->type == XPATH_NODESET) {
					const gchar *source_type = g_object_get_data (G_OBJECT (dialog), "caldav-source-type");
					gint j, szj = xmlXPathNodeSetGetLength (suppObj->nodesetval);

					for (j = 0; j < szj; j++) {
						gchar *comp = xpath_get_string (xpctx, prop ("C:supported-calendar-component-set/C:comp[%d]/@name"), i + 1, j + 1);

						if (!comp)
							continue;

						if (!g_str_equal (comp, "VEVENT") && !g_str_equal (comp, "VTODO") && !g_str_equal (comp, "VJOURNAL")) {
							g_free (comp);
							continue;
						}

						/* this calendar source supports our type, thus can be selected */
						sensitive = sensitive || (source_type && comp && g_str_equal (source_type, comp));

						if (!supports)
							supports = g_string_new ("");
						else
							g_string_append (supports, " ");

						if (g_str_equal (comp, "VEVENT"))
							g_string_append (supports, _("Events"));
						else if (g_str_equal (comp, "VTODO"))
							g_string_append (supports, _("Tasks"));
						else if (g_str_equal (comp, "VJOURNAL"))
							g_string_append (supports, _("Memos"));

						g_free (comp);
					}
				}

				xmlXPathFreeObject (suppObj);
			}

			if (tree) {
				g_return_if_fail (store != NULL);

				if (!parent_iter) {
					/* filling "User's calendars" node */
					gtk_tree_store_append (store, &par_iter, NULL);
					gtk_tree_store_set (store, &par_iter,
						COL_BOOL_IS_LOADED, TRUE,
						COL_BOOL_IS_CALENDAR, FALSE,
						COL_STRING_DISPLAYNAME, _("User's calendars"),
						COL_BOOL_SENSITIVE, TRUE,
						-1);

					parent_iter = &par_iter;
				}

				gtk_tree_store_append (store, &iter, parent_iter);
				gtk_tree_store_set (store, &iter,
					COL_BOOL_IS_LOADED, TRUE,
					COL_BOOL_IS_CALENDAR, TRUE,
					COL_STRING_HREF, href,
					COL_STRING_SUPPORTS, supports ? supports->str : "",
					COL_STRING_DISPLAYNAME, displayname && *displayname ? displayname : href,
					COL_GDK_COLOR, color_str ? &color : NULL,
					COL_BOOL_HAS_COLOR, color_str != NULL,
					COL_BOOL_SENSITIVE, sensitive,
					-1);
			}

			g_free (href);
			g_free (displayname);
			g_free (color_str);
			if (supports)
				g_string_free (supports, TRUE);
		}

		if (parent_iter) {
			/* expand loaded node */
			GtkTreePath *path;

			path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), parent_iter);
			gtk_tree_view_expand_to_path (GTK_TREE_VIEW (tree), path);
			gtk_tree_path_free (path);
		}

		if (user_data == NULL) {
			/* it was checking for user's calendars, thus add node for browsing from the base url path or the msg_path*/
			if (msg_path && *msg_path) {
				add_collection_node_to_tree (store, NULL, msg_path);
			} else {
				SoupURI *suri;

				suri = soup_uri_new (g_object_get_data (dialog, "caldav-base-url"));

				add_collection_node_to_tree (store, NULL, (suri && suri->path && *suri->path) ? suri->path : "/");

				if (suri)
					soup_uri_free (suri);
			}
		}
	}

	if (xpathObj)
		xmlXPathFreeObject (xpathObj);
	xmlXPathFreeContext (xpctx);
	xmlFreeDoc (doc);
}

static void
fetch_folder_content (GObject *dialog, const gchar *relative_path, const GtkTreeIter *parent_iter, const gchar *op_info)
{
	xmlDocPtr doc;
	xmlNodePtr root, node;
	xmlNsPtr nsdav, nsc, nscs, nsical;
	gchar *url;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_DIALOG (dialog));
	g_return_if_fail (relative_path != NULL);

	doc = xmlNewDoc (XC "1.0");
	root = xmlNewDocNode (doc, NULL, XC "propfind", NULL);

	nsdav = xmlNewNs (root, XC "DAV:", XC "D");
	nsc = xmlNewNs (root, XC "urn:ietf:params:xml:ns:caldav", XC "C");
	nscs = xmlNewNs (root, XC "http://calendarserver.org/ns/", XC "CS");
	nsical = xmlNewNs (root, XC "http://apple.com/ns/ical/", XC "IC");

	xmlSetNs (root, nsdav);
	xmlDocSetRootElement (doc, root);

	node = xmlNewTextChild (root, nsdav, XC "prop", NULL);
	xmlNewTextChild (node, nsdav, XC "displayname", NULL);
	xmlNewTextChild (node, nsdav, XC "resourcetype", NULL);
	xmlNewTextChild (node, nsc, XC "calendar-description", NULL);
	xmlNewTextChild (node, nsc, XC "supported-calendar-component-set", NULL);
	xmlNewTextChild (node, nscs, XC "getctag", NULL);
	xmlNewTextChild (node, nsical, XC "calendar-color", NULL);

	url = change_url_path (g_object_get_data (dialog, "caldav-base-url"), relative_path);
	if (url) {
		GtkTreeIter *par_iter = NULL;

		if (parent_iter) {
			gchar *key;

			par_iter = g_new0 (GtkTreeIter, 1);
			*par_iter = *parent_iter;

			/* will be freed on dialog destroy */
			key = g_strdup_printf ("caldav-to-free-%p", par_iter);
			g_object_set_data_full (dialog, key, par_iter, g_free);
			g_free (key);
		}

		send_xml_message (doc, TRUE, url, G_OBJECT (dialog), traverse_users_calendars_cb, par_iter, op_info);
	} else {
		report_error (dialog, TRUE, _("Failed to get server URL."));
	}

	xmlFreeDoc (doc);

	g_free (url);
}

/* called with "caldav-thread-mutex" unlocked; user_data is not NULL when called second time on principal */
static void
find_users_calendar_cb (GObject *dialog, const gchar *msg_path, guint status_code, const gchar *msg_body, gpointer user_data)
{
	xmlDocPtr doc;
	xmlXPathContextPtr xpctx;
	gchar *calendar_home_set, *url;
	gboolean base_url_is_calendar = FALSE;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	if (!check_soup_status (dialog, status_code, msg_body, TRUE))
		return;

	g_return_if_fail (msg_body != NULL);

	doc = xmlReadMemory (msg_body, strlen (msg_body), "response.xml", NULL, 0);
	if (!doc) {
		report_error (dialog, TRUE, _("Failed to parse server response."));
		return;
	}

	xpctx = xmlXPathNewContext (doc);
	xmlXPathRegisterNs (xpctx, XC "D", XC "DAV:");
	xmlXPathRegisterNs (xpctx, XC "C", XC "urn:ietf:params:xml:ns:caldav");

	if (user_data == NULL)
		base_url_is_calendar = xpath_exists (xpctx, NULL, "/D:multistatus/D:response/D:propstat/D:prop/D:resourcetype/C:calendar");

	calendar_home_set = xpath_get_string (xpctx, "/D:multistatus/D:response/D:propstat/D:prop/C:calendar-home-set/D:href");
	if (user_data == NULL && (!calendar_home_set || !*calendar_home_set)) {
		g_free (calendar_home_set);

		calendar_home_set = xpath_get_string (xpctx, "/D:multistatus/D:response/D:propstat/D:prop/D:current-user-principal/D:href");
		if (!calendar_home_set || !*calendar_home_set) {
			g_free (calendar_home_set);
			calendar_home_set = xpath_get_string (xpctx, "/D:multistatus/D:response/D:propstat/D:prop/D:principal-URL/D:href");
		}

		xmlXPathFreeContext (xpctx);
		xmlFreeDoc (doc);

		if (calendar_home_set && *calendar_home_set) {
			xmlNodePtr root, node;
			xmlNsPtr nsdav, nsc;

			/* ask on principal user's calendar home address */
			doc = xmlNewDoc (XC "1.0");
			root = xmlNewDocNode (doc, NULL, XC "propfind", NULL);
			nsc = xmlNewNs (root, XC "urn:ietf:params:xml:ns:caldav", XC "C");
			nsdav = xmlNewNs (root, XC "DAV:", XC "D");
			xmlSetNs (root, nsdav);
			xmlDocSetRootElement (doc, root);

			node = xmlNewTextChild (root, nsdav, XC "prop", NULL);
			xmlNewTextChild (node, nsdav, XC "current-user-principal", NULL);
			xmlNewTextChild (node, nsc, XC "calendar-home-set", NULL);

			url = change_url_path (g_object_get_data (dialog, "caldav-base-url"), calendar_home_set);
			if (url) {
				send_xml_message (doc, TRUE, url, dialog, find_users_calendar_cb, GINT_TO_POINTER (1), _("Searching for user's calendars..."));
			} else {
				report_error (dialog, TRUE, _("Failed to get server URL."));
			}

			xmlFreeDoc (doc);

			g_free (url);
			g_free (calendar_home_set);

			return;
		}
	} else {
		xmlXPathFreeContext (xpctx);
		xmlFreeDoc (doc);
	}

	if (base_url_is_calendar && (!calendar_home_set || !*calendar_home_set)) {
		SoupURI *suri = soup_uri_new (g_object_get_data (dialog, "caldav-base-url"));
		if (suri) {
			if (suri->path && *suri->path) {
				gchar *slash;

				while (slash = strrchr (suri->path, '/'), slash && slash != suri->path) {
					if (slash[1] != 0) {
						slash[1] = 0;
						g_free (calendar_home_set);
						calendar_home_set = g_strdup (suri->path);
						break;
					}

					*slash = 0;
				}
			}
			soup_uri_free (suri);
		}
	}

	if (!calendar_home_set || !*calendar_home_set) {
		report_error (dialog, FALSE, _("Could not find any user calendar."));
	} else {
		fetch_folder_content (dialog, calendar_home_set, NULL, _("Searching for user's calendars..."));
	}

	g_free (calendar_home_set);
}

static void
redirect_handler (SoupMessage *msg, gpointer user_data)
{
	if (SOUP_STATUS_IS_REDIRECTION (msg->status_code)) {
		SoupSession *soup_session = user_data;
		SoupURI *new_uri;
		const gchar *new_loc;

		new_loc = soup_message_headers_get (msg->response_headers, "Location");
		if (!new_loc)
			return;

		new_uri = soup_uri_new_with_base (soup_message_get_uri (msg), new_loc);
		if (!new_uri) {
			soup_message_set_status_full (msg,
						      SOUP_STATUS_MALFORMED,
						      "Invalid Redirect URL");
			return;
		}

		soup_message_set_uri (msg, new_uri);
		soup_session_requeue_message (soup_session, msg);

		soup_uri_free (new_uri);
	}
}

static void
send_and_handle_redirection (SoupSession *soup_session, SoupMessage *msg)
{
	soup_message_set_flags (msg, SOUP_MESSAGE_NO_REDIRECT);
	soup_message_add_header_handler (msg, "got_body", "Location", G_CALLBACK (redirect_handler), soup_session);
	soup_session_send_message (soup_session, msg);
}

static gpointer
caldav_browse_server_thread (gpointer data)
{
	GObject *dialog = data;
	GCond *cond;
	GMutex *mutex;
	SoupSession *session;
	gint task;

	g_return_val_if_fail (dialog != NULL, NULL);
	g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

	cond = g_object_get_data (dialog, "caldav-thread-cond");
	mutex = g_object_get_data (dialog, "caldav-thread-mutex");
	session = g_object_get_data (dialog, "caldav-session");

	g_return_val_if_fail (cond != NULL, NULL);
	g_return_val_if_fail (mutex != NULL, NULL);
	g_return_val_if_fail (session != NULL, NULL);

	g_mutex_lock (mutex);

	while (task = GPOINTER_TO_INT (g_object_get_data (dialog, "caldav-thread-task")), task != CALDAV_THREAD_SHOULD_DIE) {
		if (task == CALDAV_THREAD_SHOULD_SLEEP) {
			g_cond_wait (cond, mutex);
		} else if (task == CALDAV_THREAD_SHOULD_WORK) {
			SoupMessage *message;

			g_object_set_data (dialog, "caldav-thread-task", GINT_TO_POINTER (CALDAV_THREAD_SHOULD_SLEEP));

			message = g_object_get_data (dialog, "caldav-thread-message");
			if (!message) {
				g_warning ("%s: No message to send", G_STRFUNC);
				continue;
			}

			g_object_set_data (dialog, "caldav-thread-message-sent", NULL);

			g_object_ref (message);

			g_mutex_unlock (mutex);
			send_and_handle_redirection (session, message);
			g_mutex_lock (mutex);

			g_object_set_data (dialog, "caldav-thread-message-sent", message);

			g_object_unref (message);
		}
	}

	soup_session_abort (session);
	g_object_set_data (dialog, "caldav-thread-poll", GINT_TO_POINTER (0));

	g_object_set_data (dialog, "caldav-thread-cond", NULL);
	g_object_set_data (dialog, "caldav-thread-mutex", NULL);
	g_object_set_data (dialog, "caldav-session", NULL);

	g_mutex_unlock (mutex);

	g_cond_free (cond);
	g_mutex_free (mutex);
	g_object_unref (session);

	return NULL;
}

static void
soup_authenticate (SoupSession *session, SoupMessage *msg, SoupAuth *auth, gboolean retrying, gpointer data)
{
	GObject *dialog = data;
	const gchar *username, *password;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	username = g_object_get_data (dialog, "caldav-username");
	password = g_object_get_data (dialog, "caldav-password");

	if (!username || !*username || (retrying && (!password || !*password)))
		return;

	if (!password || !*password || retrying) {
		gchar *pass, *prompt, *add = NULL;
		gchar *bold_user, *bold_host;

		if (retrying && msg && msg->reason_phrase) {
			add = g_strdup_printf (_("Previous attempt failed: %s"), msg->reason_phrase);
		} else if (retrying && msg && msg->status_code) {
			add = g_strdup_printf (_("Previous attempt failed with code %d"), msg->status_code);
		}

		bold_user = g_strconcat ("<b>", username, "</b>", NULL);
		bold_host = g_strconcat ("<b>", soup_auth_get_host (auth), "</b>", NULL);
		prompt = g_strdup_printf (_("Enter password for user %s on server %s"), bold_user, bold_host);
		g_free (bold_user);
		g_free (bold_host);
		if (add) {
			gchar *tmp;

			tmp = g_strconcat (prompt, "\n", add, NULL);

			g_free (prompt);
			prompt = tmp;
		}

		pass = e_passwords_ask_password (_("Enter password"),
			"Calendar", "caldav-search-server", prompt,
			E_PASSWORDS_REMEMBER_NEVER | E_PASSWORDS_DISABLE_REMEMBER | E_PASSWORDS_SECRET,
			NULL, GTK_WINDOW (dialog));

		g_object_set_data_full (G_OBJECT (dialog), "caldav-password", pass, g_free);

		password = pass;

		g_free (prompt);
		g_free (add);
	}

	if (!retrying || password)
		soup_auth_authenticate (auth, username, password);
}

/* the dialog is about to die, so cancel any pending operations to close the thread too */
static void
dialog_response_cb (GObject *dialog, gint response_id, gpointer user_data)
{
	GCond *cond;
	GMutex *mutex;

	g_return_if_fail (dialog == user_data);
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	cond = g_object_get_data (dialog, "caldav-thread-cond");
	mutex = g_object_get_data (dialog, "caldav-thread-mutex");

	g_return_if_fail (mutex != NULL);

	g_mutex_lock (mutex);
	g_object_set_data (dialog, "caldav-thread-task", GINT_TO_POINTER (CALDAV_THREAD_SHOULD_DIE));

	if (cond)
		g_cond_signal (cond);
	g_mutex_unlock (mutex);
}

static gboolean
check_message (GtkWindow *dialog, SoupMessage *message, const gchar *url)
{
	g_return_val_if_fail (dialog != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_DIALOG (dialog), FALSE);

	if (!message)
		e_notice (GTK_WINDOW (dialog), GTK_MESSAGE_ERROR, _("Cannot create soup message for URL '%s'"), url ? url : "[null]");

	return message != NULL;
}

static void
indicate_busy (GObject *dialog, gboolean is_busy)
{
	GtkWidget *spinner = g_object_get_data (dialog, "caldav-spinner");

	gtk_widget_set_sensitive (g_object_get_data (dialog, "caldav-tree"), !is_busy);

	if (is_busy) {
		gtk_widget_show (spinner);
	} else {
		gtk_widget_hide (spinner);
	}
}

struct poll_data {
	GObject *dialog;
	SoupMessage *message;
	process_message_cb cb;
	gpointer cb_user_data;
};

static gboolean
poll_for_message_sent_cb (gpointer data)
{
	struct poll_data *pd = data;
	GMutex *mutex;
	SoupMessage *sent_message;
	gboolean again = TRUE;
	guint status_code = -1;
	gchar *msg_path = NULL;
	gchar *msg_body = NULL;

	g_return_val_if_fail (data != NULL, FALSE);

	mutex = g_object_get_data (pd->dialog, "caldav-thread-mutex");
	/* thread most likely finished already */
	if (!mutex)
		return FALSE;

	g_mutex_lock (mutex);
	sent_message = g_object_get_data (pd->dialog, "caldav-thread-message-sent");
	again = sent_message == NULL;

	if (sent_message == pd->message) {
		GtkLabel *label = g_object_get_data (pd->dialog, "caldav-info-label");

		if (label)
			gtk_label_set_text (label, "");

		g_object_ref (pd->message);
		g_object_set_data (pd->dialog, "caldav-thread-message-sent", NULL);
		g_object_set_data (pd->dialog, "caldav-thread-message", NULL);

		if (pd->cb) {
			const SoupURI *suri = soup_message_get_uri (pd->message);

			status_code = pd->message->status_code;
			msg_body = g_strndup (pd->message->response_body->data, pd->message->response_body->length);

			if (suri && suri->path)
				msg_path = g_strdup (suri->path);
		}

		g_object_unref (pd->message);
	}

	if (!again) {
		indicate_busy (pd->dialog, FALSE);
		g_object_set_data (pd->dialog, "caldav-thread-poll", GINT_TO_POINTER (0));
	}

	g_mutex_unlock (mutex);

	if (!again && pd->cb) {
		(*pd->cb) (pd->dialog, msg_path, status_code, msg_body, pd->cb_user_data);
	}

	g_free (msg_body);
	g_free (msg_path);

	return again;
}

static void
send_xml_message (xmlDocPtr doc, gboolean depth_1, const gchar *url, GObject *dialog, process_message_cb cb, gpointer cb_user_data, const gchar *info)
{
	GCond *cond;
	GMutex *mutex;
	SoupSession *session;
	SoupMessage *message;
	xmlOutputBufferPtr buf;
	guint poll_id;
	struct poll_data *pd;

	g_return_if_fail (doc != NULL);
	g_return_if_fail (url != NULL);
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	cond = g_object_get_data (dialog, "caldav-thread-cond");
	mutex = g_object_get_data (dialog, "caldav-thread-mutex");
	session = g_object_get_data (dialog, "caldav-session");

	g_return_if_fail (cond != NULL);
	g_return_if_fail (mutex != NULL);
	g_return_if_fail (session != NULL);

	message = soup_message_new ("PROPFIND", url);
	if (!check_message (GTK_WINDOW (dialog), message, url))
		return;

	buf = xmlAllocOutputBuffer (NULL);
	xmlNodeDumpOutput (buf, doc, xmlDocGetRootElement (doc), 0, 1, NULL);
	xmlOutputBufferFlush (buf);

	soup_message_headers_append (message->request_headers, "User-Agent", "Evolution/" VERSION);
	soup_message_headers_append (message->request_headers, "Depth", depth_1 ? "1" : "0");
	soup_message_set_request (message, "application/xml", SOUP_MEMORY_COPY, (const gchar *) buf->buffer->content, buf->buffer->use);

	/* Clean up the memory */
	xmlOutputBufferClose (buf);

	g_mutex_lock (mutex);

	soup_session_abort (session);

	g_object_set_data (dialog, "caldav-thread-task", GINT_TO_POINTER (CALDAV_THREAD_SHOULD_WORK));
	g_object_set_data (dialog, "caldav-thread-message-sent", NULL);
	g_object_set_data_full (dialog, "caldav-thread-message", message, g_object_unref);

	g_cond_signal (cond);

	pd = g_new0 (struct poll_data, 1);
	pd->dialog = dialog;
	pd->message = message;
	pd->cb = cb;
	pd->cb_user_data = cb_user_data;

	indicate_busy (dialog, TRUE);

	if (info) {
		GtkLabel *label = g_object_get_data (dialog, "caldav-info-label");

		if (label)
			gtk_label_set_text (label, info);
	}

	/* polling for caldav-thread-message-sent because want to update UI, which is only possible from main thread */
	poll_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 250, poll_for_message_sent_cb, pd, g_free);

	g_object_set_data_full (dialog, "caldav-thread-poll", GINT_TO_POINTER (poll_id), (GDestroyNotify) g_source_remove);

	g_mutex_unlock (mutex);
}

static void
url_entry_changed (GtkEntry *entry, GObject *dialog)
{
	const gchar *url;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	url = gtk_entry_get_text (entry);

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, url && *url);
}

static void
tree_selection_changed_cb (GtkTreeSelection *selection, GtkEntry *url_entry)
{
	gboolean ok = FALSE;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;

	g_return_if_fail (selection != NULL);
	g_return_if_fail (url_entry != NULL);

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gchar *href = NULL;

		gtk_tree_model_get (model, &iter,
			COL_BOOL_IS_CALENDAR, &ok,
			COL_STRING_HREF, &href,
			-1);

		ok = ok && href && *href;

		if (ok)
			gtk_entry_set_text (url_entry, href);

		g_free (href);
	}

	if (!ok)
		gtk_entry_set_text (url_entry, "");
}

static void
tree_row_expanded_cb (GtkTreeView *tree, GtkTreeIter *iter, GtkTreePath *path, GObject *dialog)
{
	GtkTreeModel *model;
	gboolean is_loaded = TRUE;
	gchar *href = NULL;

	g_return_if_fail (tree != NULL);
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_DIALOG (dialog));
	g_return_if_fail (iter != NULL);

	model = gtk_tree_view_get_model (tree);
	gtk_tree_model_get (model, iter,
		COL_BOOL_IS_LOADED, &is_loaded,
		COL_STRING_HREF, &href,
		-1);

	if (!is_loaded) {
		/* unset unloaded flag */
		gtk_tree_store_set (GTK_TREE_STORE (model), iter, COL_BOOL_IS_LOADED, TRUE, -1);

		/* remove the "Loading..." node */
		while (gtk_tree_model_iter_has_child (model, iter)) {
			GtkTreeIter child;

			if (!gtk_tree_model_iter_nth_child (model, &child, iter, 0) ||
			    !gtk_tree_store_remove (GTK_TREE_STORE (model), &child))
					break;
		}

		/* fetch content */
		fetch_folder_content (dialog, href, iter, _("Searching folder content..."));
	}

	g_free (href);
}

static void
init_dialog (GtkDialog *dialog, GtkWidget **new_url_entry, const gchar *url, const gchar *username, gint source_type)
{
	GtkBox *content_area;
	GtkWidget *label, *info_box, *spinner, *info_label;
	GtkWidget *tree, *scrolled_window;
	GtkTreeStore *store;
	GtkTreeSelection *selection;
	SoupSession *session;
	EProxy *proxy;
	SoupURI *proxy_uri = NULL;
	GThread *thread;
	GError *error = NULL;
	GMutex *thread_mutex;
	GCond *thread_cond;
	const gchar *source_type_str;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (GTK_IS_DIALOG (dialog));
	g_return_if_fail (new_url_entry != NULL);
	g_return_if_fail (url != NULL);

	content_area = GTK_BOX (gtk_dialog_get_content_area (dialog));
	g_return_if_fail (content_area != NULL);

	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 240);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);

	*new_url_entry = gtk_entry_new ();
	gtk_box_pack_start (content_area, *new_url_entry, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (*new_url_entry), "changed", G_CALLBACK (url_entry_changed), dialog);

	g_object_set_data (G_OBJECT (dialog), "caldav-new-url-entry", *new_url_entry);

	label = gtk_label_new (_("List of available calendars:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (content_area, label, FALSE, FALSE, 0);

	store = gtk_tree_store_new (8,
		G_TYPE_BOOLEAN,  /* COL_BOOL_IS_LOADED     */
		G_TYPE_STRING,   /* COL_STRING_HREF        */
		G_TYPE_BOOLEAN,  /* COL_BOOL_IS_CALENDAR   */
		G_TYPE_STRING,   /* COL_STRING_SUPPORTS    */
		G_TYPE_STRING,   /* COL_STRING_DISPLAYNAME */
		GDK_TYPE_COLOR,  /* COL_GDK_COLOR          */
		G_TYPE_BOOLEAN,  /* COL_BOOL_HAS_COLOR     */
		G_TYPE_BOOLEAN); /* COL_BOOL_SENSITIVE     */

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window), tree);
	gtk_box_pack_start (content_area, scrolled_window, TRUE, TRUE, 0);

	g_object_set_data (G_OBJECT (dialog), "caldav-tree", tree);
	g_object_set_data (G_OBJECT (dialog), "caldav-tree-sw", scrolled_window);

	renderer = e_cell_renderer_color_new ();
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree), gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, _("Name"), renderer, "color", COL_GDK_COLOR, "visible", COL_BOOL_HAS_COLOR, "sensitive", COL_BOOL_SENSITIVE, NULL) - 1);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (column), renderer, TRUE);
	gtk_cell_layout_set_attributes (
		GTK_CELL_LAYOUT (column), renderer,
		"text", COL_STRING_DISPLAYNAME,
		"sensitive", COL_BOOL_SENSITIVE,
		NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, _("Supports"), renderer, "text", COL_STRING_SUPPORTS, "sensitive", COL_BOOL_SENSITIVE, NULL);

	/*renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree), -1, _("href"), renderer, "text", COL_STRING_HREF, "sensitive", COL_BOOL_SENSITIVE, NULL);*/

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	g_signal_connect (selection, "changed", G_CALLBACK (tree_selection_changed_cb), *new_url_entry);

	g_signal_connect (tree, "row-expanded", G_CALLBACK (tree_row_expanded_cb), dialog);

	info_box = gtk_hbox_new (FALSE, 2);

	spinner = gtk_spinner_new ();
	gtk_spinner_start (GTK_SPINNER (spinner));
	gtk_box_pack_start (GTK_BOX (info_box), spinner, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (dialog), "caldav-spinner", spinner);

	info_label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (info_label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (info_box), info_label, FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (dialog), "caldav-info-label", info_label);

	gtk_box_pack_start (content_area, info_box, FALSE, FALSE, 0);

	gtk_widget_show_all (GTK_WIDGET (content_area));
	gtk_widget_hide (*new_url_entry);
	gtk_widget_hide (spinner);

	session = soup_session_sync_new ();
	if (g_getenv ("CALDAV_DEBUG") != NULL) {
		SoupLogger *logger;

		logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, 100 * 1024 * 1024);
		soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
		g_object_unref (logger);
	}

	proxy = e_proxy_new ();
	e_proxy_setup_proxy (proxy);

	/* use proxy if necessary */
	if (e_proxy_require_proxy_for_uri (proxy, url)) {
		proxy_uri = e_proxy_peek_uri_for (proxy, url);
	}

	g_object_set (session, SOUP_SESSION_PROXY_URI, proxy_uri, NULL);
	g_object_unref (proxy);

	g_signal_connect (session, "authenticate", G_CALLBACK (soup_authenticate), dialog);

	switch (source_type) {
	default:
	case E_CAL_SOURCE_TYPE_EVENT:
		source_type_str = "VEVENT";
		break;
	case E_CAL_SOURCE_TYPE_TODO:
		source_type_str = "VTODO";
		break;
	case E_CAL_SOURCE_TYPE_JOURNAL:
		source_type_str = "VJOURNAL";
		break;
	}

	g_object_set_data_full (G_OBJECT (dialog), "caldav-source-type", g_strdup (source_type_str), g_free);
	g_object_set_data_full (G_OBJECT (dialog), "caldav-base-url", g_strdup (url), g_free);
	g_object_set_data_full (G_OBJECT (dialog), "caldav-username", g_strdup (username), g_free);
	g_object_set_data_full (G_OBJECT (dialog), "caldav-session", session, NULL); /* it is freed at the end of thread life */

	thread_mutex = g_mutex_new ();
	thread_cond = g_cond_new ();

	g_object_set_data (G_OBJECT (dialog), "caldav-thread-task", GINT_TO_POINTER (CALDAV_THREAD_SHOULD_SLEEP));
	g_object_set_data_full (G_OBJECT (dialog), "caldav-thread-mutex", thread_mutex, NULL); /* it is freed at the end of thread life */
	g_object_set_data_full (G_OBJECT (dialog), "caldav-thread-cond", thread_cond, NULL); /* it is freed at the end of thread life */

	/* create thread at the end, to have all properties on the dialog set */
	thread = g_thread_create (caldav_browse_server_thread, dialog, TRUE, &error);

	if (error || !thread) {
		e_notice (GTK_WINDOW (dialog), GTK_MESSAGE_ERROR, _("Failed to create thread: %s"), error ? error->message : _("Unknown error"));
		if (error)
			g_error_free (error);
	} else {
		xmlDocPtr doc;
		xmlNodePtr root, node;
		xmlNsPtr nsdav, nsc;

		g_object_set_data_full (G_OBJECT (dialog), "caldav-thread", thread, (GDestroyNotify) g_thread_join);

		doc = xmlNewDoc (XC "1.0");
		root = xmlNewDocNode (doc, NULL, XC "propfind", NULL);
		nsc = xmlNewNs (root, XC "urn:ietf:params:xml:ns:caldav", XC "C");
		nsdav = xmlNewNs (root, XC "DAV:", XC "D");
		xmlSetNs (root, nsdav);
		xmlDocSetRootElement (doc, root);

		node = xmlNewTextChild (root, nsdav, XC "prop", NULL);
		xmlNewTextChild (node, nsdav, XC "current-user-principal", NULL);
		xmlNewTextChild (node, nsdav, XC "principal-URL", NULL);
		xmlNewTextChild (node, nsdav, XC "resourcetype", NULL);
		xmlNewTextChild (node, nsc, XC "calendar-home-set", NULL);

		send_xml_message (doc, FALSE, url, G_OBJECT (dialog), find_users_calendar_cb, NULL, _("Searching for user's calendars..."));

		xmlFreeDoc (doc);
	}

	g_signal_connect (dialog, "response", G_CALLBACK (dialog_response_cb), dialog);

	url_entry_changed (GTK_ENTRY (*new_url_entry), G_OBJECT (dialog));
}

static gchar *
prepare_url (const gchar *server_url, gboolean use_ssl)
{
	gchar *url;
	gint len;

	g_return_val_if_fail (server_url != NULL, NULL);
	g_return_val_if_fail (*server_url, NULL);

	if (g_str_has_prefix (server_url, "caldav://")) {
		url = g_strconcat (use_ssl ? "https://" : "http://", server_url + 9, NULL);
	} else {
		url = g_strdup (server_url);
	}

	if (url) {
		SoupURI *suri = soup_uri_new (url);

		/* properly encode uri */
		if (suri && suri->path) {
			gchar *tmp = soup_uri_encode (suri->path, NULL);
			gchar *path = soup_uri_normalize (tmp, "/");

			soup_uri_set_path (suri, path);

			g_free (tmp);
			g_free (path);
			g_free (url);

			url = soup_uri_to_string (suri, FALSE);
		} else {
			g_free (url);
			soup_uri_free (suri);
			return NULL;
		}

		soup_uri_free (suri);
	}

	/* remove trailing slashes... */
	len = strlen (url);
	while (len--) {
		if (url[len] == '/') {
			url[len] = '\0';
		} else {
			break;
		}
	}

	/* ...and append exactly one slash */
	if (url && *url) {
		gchar *tmp = url;

		url = g_strconcat (url, "/", NULL);

		g_free (tmp);
	} else {
		g_free (url);
		url = NULL;
	}

	return url;
}

gchar *
caldav_browse_server (GtkWindow *parent, const gchar *server_url, const gchar *username, gboolean use_ssl, gint source_type)
{
	GtkWidget *dialog, *new_url_entry;
	gchar *url, *new_url = NULL;

	g_return_val_if_fail (server_url != NULL, NULL);

	url = prepare_url (server_url, use_ssl);

	if (!url || !*url) {
		e_notice (parent, GTK_MESSAGE_ERROR, _("Server URL '%s' is not a valid URL"), server_url);
		g_free (url);
		return NULL;
	}

	dialog = gtk_dialog_new_with_buttons (
			_("Browse for a CalDAV calendar"),
			parent,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);

	new_url_entry = NULL;
	init_dialog (GTK_DIALOG (dialog), &new_url_entry, url, username, source_type);

	if (new_url_entry && gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		const gchar *nu = gtk_entry_get_text (GTK_ENTRY (new_url_entry));

		if (nu && *nu)
			new_url = change_url_path (server_url, nu);
	}

	gtk_widget_destroy (dialog);

	g_free (url);

	return new_url;
}
