/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_WEBDAV_BROWSER_H
#define E_WEBDAV_BROWSER_H

#include <gtk/gtk.h>
#include <libedataserverui/libedataserverui.h>

/* Standard GObject macros */
#define E_TYPE_WEBDAV_BROWSER \
	(e_webdav_browser_get_type ())
#define E_WEBDAV_BROWSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_WEBDAV_BROWSER, EWebDAVBrowser))
#define E_WEBDAV_BROWSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_WEBDAV_BROWSER, EWebDAVBrowserClass))
#define E_IS_WEBDAV_BROWSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_WEBDAV_BROWSER))
#define E_IS_WEBDAV_BROWSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_WEBDAV_BROWSER))
#define E_WEBDAV_BROWSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_WEBDAV_BROWSER, EWebDAVBrowserClass))

G_BEGIN_DECLS

typedef struct _EWebDAVBrowser EWebDAVBrowser;
typedef struct _EWebDAVBrowserClass EWebDAVBrowserClass;
typedef struct _EWebDAVBrowserPrivate EWebDAVBrowserPrivate;

/**
 * EWebDAVBrowser:
 *
 * Contains only private data that should be read and manipulated using
 * the functions below.
 *
 * Since: 3.26
 **/
struct _EWebDAVBrowser {
	GtkGrid parent;
	EWebDAVBrowserPrivate *priv;
};

struct _EWebDAVBrowserClass {
	GtkGridClass parent_class;
};

GType		e_webdav_browser_get_type		(void) G_GNUC_CONST;
GtkWidget *	e_webdav_browser_new			(ECredentialsPrompter *credentials_prompter);
ECredentialsPrompter *
		e_webdav_browser_get_credentials_prompter
							(EWebDAVBrowser *webdav_browser);
void		e_webdav_browser_set_source		(EWebDAVBrowser *webdav_browser,
							 ESource *source);
ESource *	e_webdav_browser_ref_source		(EWebDAVBrowser *webdav_browser);
void		e_webdav_browser_abort			(EWebDAVBrowser *webdav_browser);
void		e_webdav_browser_refresh		(EWebDAVBrowser *webdav_browser);

G_END_DECLS

#endif /* E_WEBDAV_BROWSER_H */
