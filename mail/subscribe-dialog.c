/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* subscribe-dialog.c: Subscribe dialog */
/*
 *  Authors: Chris Toshok <toshok@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h> 
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>

#include <gtkhtml/gtkhtml.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>

#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-tree.h>

#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-table.h>

#include <gal/e-table/e-tree-scrolled.h>
#include <gal/e-table/e-tree-memory-callbacks.h>
#include <gal/e-table/e-tree.h>

#include <gal/e-paned/e-hpaned.h>

#include "e-util/e-html-utils.h"
#include "e-util/e-gui-utils.h"
#include "mail.h"
#include "mail-tools.h"
#include "mail-mt.h"
#include "camel/camel-exception.h"
#include "camel/camel-store.h"
#include "camel/camel-session.h"
#include "subscribe-dialog.h"

#include "art/empty.xpm"
#include "art/mark.xpm"


#define DEFAULT_STORE_TABLE_WIDTH         200
#define DEFAULT_WIDTH                     500
#define DEFAULT_HEIGHT                    300

#define PARENT_TYPE (gtk_object_get_type ())

#ifdef JUST_FOR_TRANSLATORS
static char *list [] = {
	N_("Folder"),
	N_("Store"),
};
#endif

#define FOLDER_ETREE_SPEC "<ETableSpecification cursor-mode=\"line\"> \
        <ETableColumn model_col=\"0\" pixbuf=\"subscribed-image\" expansion=\"0.0\" minimum_width=\"16\" resizable=\"false\" cell=\"cell_toggle\" compare=\"integer\"/> \
        <ETableColumn model_col=\"1\" _title=\"Folder\" expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"cell_tree\" compare=\"string\"/> \
	<ETableState>                   			        \
		<column source=\"0\"/>                                  \
		<column source=\"1\"/>                                  \
	        <grouping></grouping>                                   \
	</ETableState>                  			        \
</ETableSpecification>"

#define STORE_ETABLE_SPEC "<ETableSpecification cursor-mode=\"line\"> \
        <ETableColumn model_col=\"0\" _title=\"Store\" expansion=\"1.0\" minimum_width=\"20\" resizable=\"true\" cell=\"string\" compare=\"string\"/> \
	<ETableState>                   			        \
		<column source=\"0\"/>                                  \
	        <grouping></grouping>                                   \
	</ETableState>                  			        \
</ETableSpecification>"

enum {
	FOLDER_COL_SUBSCRIBED,
	FOLDER_COL_NAME,
	FOLDER_COL_LAST
};

enum {
	STORE_COL_NAME,
	STORE_COL_LAST
};

static GtkObjectClass *subscribe_dialog_parent_class;

static void build_tree (SubscribeDialog *sc, CamelStore *store);

static EPixmap pixmaps [] = {
	E_PIXMAP ("/Toolbar/SubscribeFolder",	"buttons/fetch-mail.png"),	/* XXX */
	E_PIXMAP ("/Toolbar/UnsubscribeFolder",	"buttons/compose-message.png"),	/* XXX */
	E_PIXMAP ("/Toolbar/RefreshList",	"buttons/forward.png"),	/* XXX */
	E_PIXMAP_END
};

static GtkWidget*
make_folder_search_widget (GtkSignalFunc start_search_func,
			   gpointer user_data_for_search)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data_for_search);
	GtkWidget *search_hbox = gtk_hbox_new (FALSE, 0);

	sc->search_entry = gtk_entry_new ();

	if (start_search_func) {
		gtk_signal_connect (GTK_OBJECT (sc->search_entry), "activate",
				    start_search_func,
				    user_data_for_search);
	}
	
	/* add the search entry to the our search_vbox */
	gtk_box_pack_start (GTK_BOX (search_hbox),
			    gtk_label_new(_("Display folders starting with:")),
			    FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (search_hbox), sc->search_entry,
			    FALSE, TRUE, 3);

	return search_hbox;
}


/* Our async operations */

typedef void (*SubscribeGetStoreCallback)(SubscribeDialog *sc, CamelStore *store, gpointer cb_data);
typedef void (*SubscribeFolderCallback)(SubscribeDialog *sc, gboolean success, gpointer cb_data);

/* ** GET STORE ******************************************************* */

struct _get_store_msg {
	struct _mail_msg msg;

	SubscribeDialog *sc;
	char *url;
	SubscribeGetStoreCallback cb;
	gpointer cb_data;
	CamelStore *store;
};

static char *get_store_desc(struct _mail_msg *mm, int done)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;

	return g_strdup_printf(_("Getting store for \"%s\""), m->url);
}

static void get_store_get(struct _mail_msg *mm)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;

	m->store = camel_session_get_store (session, m->url, &mm->ex);
}

static void get_store_got(struct _mail_msg *mm)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;

	m->cb(m->sc, m->store, m->cb_data);
}

static void get_store_free(struct _mail_msg *mm)
{
	struct _get_store_msg *m = (struct _get_store_msg *)mm;

	if (m->store)
		camel_object_unref((CamelObject *)m->store);
	g_free(m->url);
}

static struct _mail_msg_op get_store_op = {
	get_store_desc,
	get_store_get,
	get_store_got,
	get_store_free,
};

static void
subscribe_do_get_store (SubscribeDialog *sc, const char *url, SubscribeGetStoreCallback cb, gpointer cb_data)
{
	struct _get_store_msg *m;
	int id;

	g_return_if_fail (url != NULL);

	m = mail_msg_new(&get_store_op, NULL, sizeof(*m));
	m->sc = sc;
	m->url = g_strdup(url);
	m->cb = cb;
	m->cb_data = cb_data;

	id = m->msg.seq;
	e_thread_put(mail_thread_queued, (EMsg *)m);
	mail_msg_wait(id);
}

/* ** SUBSCRIBE FOLDER ******************************************************* */
/* Given a CamelFolderInfo, construct the corresponding
 * EvolutionStorage path to it.
 */
static char *
storage_tree_path (CamelFolderInfo *info)
{
	int len;
	CamelFolderInfo *i;
	char *path, *p;

	for (len = 0, i = info; i; i = i->parent)
		len += strlen (i->name) + 1;

	/* We do this backwards because that's the way the pointers point. */
	path = g_malloc (len + 1);
	p = path + len;
	*p = '\0';
	for (i = info; i; i = i->parent) {
		len = strlen (i->name);
		p -= len;
		memcpy (p, i->name, len);
		*--p = '/';
	}

	return path;
}

/* ********************************************************************** */
/* Subscribe folder */

struct _subscribe_msg {
	struct _mail_msg msg;

	SubscribeDialog *sc;
	CamelStore *store;
	gboolean subscribe;
	SubscribeFolderCallback cb;
	gpointer cb_data;

	char *path;
	char *name;
	char *full_name;
	char *url;
};

static char *subscribe_folder_desc(struct _mail_msg *mm, int done)
{
	struct _subscribe_msg *m = (struct _subscribe_msg *)mm;

	if (m->subscribe)
		return g_strdup_printf(_("Subscribing to folder \"%s\""), m->name);
	else
		return g_strdup_printf(_("Unsubscribing to folder \"%s\""), m->name);
}

static void subscribe_folder_subscribe(struct _mail_msg *mm)
{
	struct _subscribe_msg *m = (struct _subscribe_msg *)mm;
	
	if (m->subscribe)
		camel_store_subscribe_folder(m->store, m->full_name, &mm->ex);
	else
		camel_store_unsubscribe_folder(m->store, m->full_name, &mm->ex);
}

static void
recursive_add_folder (EvolutionStorage *storage, const char *path, const char *name, const char *url)
{
	char *parent, *pname, *p;

	p = strrchr (path, '/');
	if (p && p != path) {
		parent = g_strndup (path, p - path);
		if (!evolution_storage_folder_exists (storage, parent)) {
			p = strrchr (parent, '/');
			if (p)
				pname = g_strdup (p + 1);
			else
				pname = g_strdup ("");
			recursive_add_folder (storage, parent, pname, "");
			g_free (pname);
		}
		g_free (parent);
	}

	evolution_storage_new_folder (storage, path, name, "mail", url, name, FALSE);
}

static void subscribe_folder_subscribed(struct _mail_msg *mm)
{
	struct _subscribe_msg *m = (struct _subscribe_msg *)mm;
	
	if (!camel_exception_is_set (&mm->ex)) {
		if (m->subscribe)
			recursive_add_folder(m->sc->storage, m->path, m->name, m->url);
		else
			evolution_storage_removed_folder(m->sc->storage, m->path);
	}
	
	if (m->cb)
		m->cb(m->sc, !camel_exception_is_set(&mm->ex), m->cb_data);
}

static void subscribe_folder_free(struct _mail_msg *mm)
{
	struct _subscribe_msg *m = (struct _subscribe_msg *)mm;

	g_free(m->path);
	g_free(m->name);
	g_free(m->full_name);
	g_free(m->url);
	
	camel_object_unref((CamelObject *)m->store);
	/* in wrong thread to do this?
	   gtk_object_unref (GTK_OBJECT (input->sc));*/
}

static struct _mail_msg_op subscribe_folder_op = {
	subscribe_folder_desc,
	subscribe_folder_subscribe,
	subscribe_folder_subscribed,
	subscribe_folder_free,
};

static void
subscribe_do_subscribe_folder (SubscribeDialog *sc, CamelStore *store, CamelFolderInfo *info,
			       gboolean subscribe, SubscribeFolderCallback cb, gpointer cb_data)
{
	struct _subscribe_msg *m;
	
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (info);

	m = mail_msg_new(&subscribe_folder_op, NULL, sizeof(*m));
	m->sc = sc;
	m->store = store;
	camel_object_ref((CamelObject *)store);
	m->subscribe = subscribe;
	m->cb = cb;
	m->cb_data = cb_data;

	m->path = storage_tree_path (info);
	m->name = g_strdup (info->name);
	m->full_name = g_strdup (info->full_name);
	m->url = g_strdup (info->url);

	/*
	  gtk_object_ref (GTK_OBJECT (sc));*/

	e_thread_put(mail_thread_new, (EMsg *)m);
}



static gboolean
folder_info_subscribed (SubscribeDialog *sc, CamelFolderInfo *info)
{
	return camel_store_folder_subscribed (sc->store, info->full_name);
}

static void
node_changed_cb (SubscribeDialog *sc, gboolean changed, gpointer data)
{
	ETreePath node = data;

	if (changed)
		e_tree_model_node_data_changed (sc->folder_model, node);
}

static void
subscribe_folder_info (SubscribeDialog *sc, CamelFolderInfo *info, ETreePath node)
{
	/* folders without urls cannot be subscribed to */
	if (info->url == NULL)
		return;
	
	subscribe_do_subscribe_folder (sc, sc->store, info, TRUE, node_changed_cb, node);
}

static void
unsubscribe_folder_info (SubscribeDialog *sc, CamelFolderInfo *info, ETreePath node)
{
	/* folders without urls cannot be subscribed to */
	if (info->url == NULL)
		return;
	
	subscribe_do_subscribe_folder (sc, sc->store, info, FALSE, node_changed_cb, node);
}

static void
subscribe_close (BonoboUIComponent *uic,
		 void *user_data, const char *path)
{
	SubscribeDialog *sc = (SubscribeDialog*)user_data;

	gtk_widget_destroy (sc->app);
}

static void
subscribe_select_all (BonoboUIComponent *uic,
		      void *user_data, const char *path)
{
	SubscribeDialog *sc = (SubscribeDialog*)user_data;
	ETreeScrolled *scrolled = E_TREE_SCROLLED (sc->folder_etree);
	
	e_tree_select_all (e_tree_scrolled_get_tree(scrolled));
}

static void
subscribe_invert_selection (BonoboUIComponent *uic,
			    void *user_data, const char *path)
{
	SubscribeDialog *sc = (SubscribeDialog*)user_data;
	ETreeScrolled *scrolled = E_TREE_SCROLLED (sc->folder_etree);
	
	e_tree_invert_selection (e_tree_scrolled_get_tree(scrolled));
}

static void
subscribe_folder_foreach (int model_row, gpointer closure)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (closure);
	ETreePath node = e_tree_node_at_row (e_tree_scrolled_get_tree(E_TREE_SCROLLED(sc->folder_etree)), model_row);
	CamelFolderInfo *info = e_tree_memory_node_get_data (E_TREE_MEMORY(sc->folder_model), node);

	if (!folder_info_subscribed (sc, info))
		subscribe_folder_info (sc, info, node);
}

static void
subscribe_folders (BonoboUIComponent *componet, gpointer user_data, const char *cname)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	e_tree_selected_row_foreach (e_tree_scrolled_get_tree(E_TREE_SCROLLED(sc->folder_etree)),
				      subscribe_folder_foreach, sc);
}

static void
unsubscribe_folder_foreach (int model_row, gpointer closure)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (closure);
	ETreePath node = e_tree_node_at_row (e_tree_scrolled_get_tree(E_TREE_SCROLLED(sc->folder_etree)), model_row);
	CamelFolderInfo *info = e_tree_memory_node_get_data (E_TREE_MEMORY(sc->folder_model), node);

	if (folder_info_subscribed(sc, info))
		unsubscribe_folder_info (sc, info, node);
}


static void
unsubscribe_folders (BonoboUIComponent *component, gpointer user_data, const char *cname)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	e_tree_selected_row_foreach (e_tree_scrolled_get_tree(E_TREE_SCROLLED(sc->folder_etree)),
				     unsubscribe_folder_foreach, sc);
}

static void
subscribe_refresh_list (BonoboUIComponent *component, gpointer user_data, const char *cname)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	e_utf8_gtk_entry_set_text (GTK_ENTRY (sc->search_entry), "");
	if (sc->search_top) {
		g_free (sc->search_top);
		sc->search_top = NULL;
	}
	if (sc->store)
		build_tree (sc, sc->store);
}

static void
subscribe_search (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);
	char* search_pattern = e_utf8_gtk_entry_get_text(GTK_ENTRY(widget));

	if (sc->search_top) {
		g_free (sc->search_top);
		sc->search_top = NULL;
	}

	if (search_pattern && *search_pattern)
		sc->search_top = search_pattern;

	if (sc->store)
		build_tree (sc, sc->store);
}


#if 0
/* HTML Helpers */
static void
html_size_req (GtkWidget *widget, GtkRequisition *requisition)
{
	if (GTK_LAYOUT (widget)->height > 90)
		requisition->height = 90;
	else
		requisition->height = GTK_LAYOUT (widget)->height;
}

/* Returns a GtkHTML which is already inside a GtkScrolledWindow. If
 * @white is TRUE, the GtkScrolledWindow will be inside a GtkFrame.
 */
static GtkWidget *
html_new (gboolean white)
{
	GtkWidget *html, *scrolled, *frame;
	GtkStyle *style;
	
	html = gtk_html_new ();
	GTK_LAYOUT (html)->height = 0;
	gtk_signal_connect (GTK_OBJECT (html), "size_request",
			    GTK_SIGNAL_FUNC (html_size_req), NULL);
	gtk_html_set_editable (GTK_HTML (html), FALSE);
	style = gtk_rc_get_style (html);
	if (style) {
		gtk_html_set_default_background_color (GTK_HTML (html),
						       white ? &style->white :
						       &style->bg[0]);
	}
	gtk_widget_set_sensitive (html, FALSE);
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scrolled), html);
	
	if (white) {
		frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (frame),
					   GTK_SHADOW_ETCHED_IN);
		gtk_container_add (GTK_CONTAINER (frame), scrolled);
		gtk_widget_show_all (frame);
	} else
		gtk_widget_show_all (scrolled);
	
	return html;
}

static void
put_html (GtkHTML *html, char *text)
{
	GtkHTMLStream *handle;
	
	text = e_text_to_html (text, (E_TEXT_TO_HTML_CONVERT_NL |
				      E_TEXT_TO_HTML_CONVERT_SPACES |
				      E_TEXT_TO_HTML_CONVERT_URLS));
	handle = gtk_html_begin (html);
	gtk_html_write (html, handle, "<HTML><BODY>", 12);
	gtk_html_write (html, handle, text, strlen (text));
	gtk_html_write (html, handle, "</BODY></HTML>", 14);
	g_free (text);
	gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
}
#endif


/* etree stuff for the subscribe ui */

static int
folder_etree_column_count (ETreeModel *etm, void *data)
{
	return FOLDER_COL_LAST;
}

static void*
folder_etree_duplicate_value (ETreeModel *etm, int col, const void *val, void *data)
{
	return g_strdup (val);
}

static void
folder_etree_free_value (ETreeModel *etm, int col, void *val, void *data)
{
	g_free (val);
}

static void*
folder_etree_init_value (ETreeModel *etm, int col, void *data)
{
	return g_strdup ("");
}

static gboolean
folder_etree_value_is_empty (ETreeModel *etm, int col, const void *val, void *data)
{
	return !(val && *(char *)val);
}

static char*
folder_etree_value_to_string (ETreeModel *etm, int col, const void *val, void *data)
{
	return g_strdup(val);
}

static GdkPixbuf*
folder_etree_icon_at (ETreeModel *etree, ETreePath path, void *model_data)
{
	return NULL; /* XXX no icons for now */
}

static void*
folder_etree_value_at (ETreeModel *etree, ETreePath path, int col, void *model_data)
{
	SubscribeDialog *dialog = SUBSCRIBE_DIALOG (model_data);
	CamelFolderInfo *info = e_tree_memory_node_get_data (E_TREE_MEMORY(etree), path);

	if (col == FOLDER_COL_NAME) {
		return info->name;
	}
	else /* FOLDER_COL_SUBSCRIBED */ {
		/* folders without urls cannot be subscribed to */
		if (info->url == NULL)
			return GINT_TO_POINTER(0); /* empty */
		else if (!folder_info_subscribed(dialog, info))
			return GINT_TO_POINTER(0); /* XXX unchecked */
		else
			return GUINT_TO_POINTER (1); /* checked */
	}
}

static void
folder_etree_set_value_at (ETreeModel *etree, ETreePath path, int col, const void *val, void *model_data)
{
	/* nothing */
}

static gboolean
folder_etree_is_editable (ETreeModel *etree, ETreePath path, int col, void *model_data)
{
	return FALSE;
}



static int
store_etable_col_count (ETableModel *etm, void *data)
{
	return STORE_COL_LAST;
}

static int
store_etable_row_count (ETableModel *etm, void *data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);

	return g_list_length (sc->store_list);
}

static void*
store_etable_value_at (ETableModel *etm, int col, int row, void *data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);
	CamelStore *store = (CamelStore*)g_list_nth_data (sc->store_list, row);

	return camel_service_get_name (CAMEL_SERVICE (store), TRUE);
}

static void
store_etable_set_value_at (ETableModel *etm, int col, int row, const void *val, void *data)
{
	/* nada */
}

static gboolean
store_etable_is_editable (ETableModel *etm, int col, int row, void *data)
{
	return FALSE;
}

static void*
store_etable_duplicate_value (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup (val);
}

static void
store_etable_free_value (ETableModel *etm, int col, void *val, void *data)
{
	g_free (val);
}

static void*
store_etable_initialize_value (ETableModel *etm, int col, void *data)
{
	return g_strdup ("");
}

static gboolean
store_etable_value_is_empty (ETableModel *etm, int col, const void *val, void *data)
{
	return !(val && *(char *)val);
}

static char*
store_etable_value_to_string (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup(val);
}



static void
build_etree_from_folder_info (SubscribeDialog *sc, ETreePath parent, CamelFolderInfo *info)
{
	CamelFolderInfo *i;

	if (info == NULL)
		return;

	for (i = info; i; i = i->sibling) {
		ETreePath node = e_tree_memory_node_insert (E_TREE_MEMORY(sc->folder_model), parent, -1, i);
		build_etree_from_folder_info (sc, node, i->child);
	}
}

static void
build_tree (SubscribeDialog *sc, CamelStore *store)
{
	CamelException *ex = camel_exception_new();

	/* free up the existing CamelFolderInfo* if there is any */
	if (sc->folder_info)
		camel_store_free_folder_info (sc->store, sc->folder_info);
	if (sc->storage)
		gtk_object_unref (GTK_OBJECT (sc->storage));

	sc->store = store;
	sc->storage = mail_lookup_storage (sc->store);
	sc->folder_info = camel_store_get_folder_info (sc->store, sc->search_top, TRUE, TRUE, FALSE, ex);

	if (camel_exception_is_set (ex)) {
		printf ("camel_store_get_folder_info failed\n");
		camel_exception_free (ex);
		return;
	}

	e_tree_memory_node_remove (E_TREE_MEMORY(sc->folder_model), sc->folder_root);
	sc->folder_root = e_tree_memory_node_insert (E_TREE_MEMORY(sc->folder_model), NULL,
						    0, NULL);


	build_etree_from_folder_info (sc, sc->folder_root, sc->folder_info);

	camel_exception_free (ex);
}

static void
storage_selected_cb (ETree *table, int row, gpointer data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);
	CamelStore *store = (CamelStore*)g_list_nth_data (sc->store_list, row);

	build_tree (sc, store);
}



static void
folder_toggle_cb (ETree *tree, int row, ETreePath path, int col, GdkEvent *event, gpointer data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);
	CamelFolderInfo *info = e_tree_memory_node_get_data (E_TREE_MEMORY(sc->folder_model), path);

	if (folder_info_subscribed(sc, info))
		unsubscribe_folder_info (sc, info, path);
	else
		subscribe_folder_info (sc, info, path);

	e_tree_model_node_data_changed (sc->folder_model, path);
}



#define EXAMPLE_DESCR "And the beast shall come forth surrounded by a roiling cloud of vengeance.\n" \
"  The house of the unbelievers shall be razed and they shall be scorched to the\n" \
"          earth. Their tags shall blink until the end of days. \n" \
"                 from The Book of Mozilla, 12:10"

static BonoboUIVerb verbs [] = {
	/* File Menu */
	BONOBO_UI_VERB ("FileCloseWin", subscribe_close),

	/* Edit Menu */
	BONOBO_UI_VERB ("EditSelectAll", subscribe_select_all),
	BONOBO_UI_VERB ("EditInvertSelection", subscribe_invert_selection),
	
	/* Folder Menu / Toolbar */
	BONOBO_UI_VERB ("SubscribeFolder", subscribe_folders),
	BONOBO_UI_VERB ("UnsubscribeFolder", unsubscribe_folders),

	/* Toolbar Specific */
	BONOBO_UI_VERB ("RefreshList", subscribe_refresh_list),

	BONOBO_UI_VERB_END
};

static void
store_cb (SubscribeDialog *sc, CamelStore *store, gpointer data)
{
	if (!store)
		return;

	if (camel_store_supports_subscriptions (store)) {
		camel_object_ref((CamelObject *)store);
		sc->store_list = g_list_prepend (sc->store_list, store);
		e_table_model_row_inserted (sc->store_model, 0);
	}
}

static void
populate_store_foreach (MailConfigService *service, SubscribeDialog *sc)
{
	g_return_if_fail (service->url != NULL);
	
	subscribe_do_get_store (sc, service->url, store_cb, NULL);
}

static void
populate_store_list (SubscribeDialog *sc)
{
	const GSList *news;
	GSList *sources;
	
	sources = mail_config_get_sources ();
	g_slist_foreach (sources, (GFunc)populate_store_foreach, sc);
	g_slist_free (sources);
	
	news = mail_config_get_news ();
	g_slist_foreach ((GSList *)news, (GFunc)populate_store_foreach, sc);
	
	e_table_model_changed (sc->store_model);
}

static void
subscribe_dialog_gui_init (SubscribeDialog *sc)
{
	ETableExtras *extras;
	ECell *cell;
	GdkPixbuf *toggles[2];
	BonoboUIComponent *component;
	BonoboUIContainer *container;
	GtkWidget         *folder_search_widget;
	BonoboControl     *search_control;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	/* Construct the app */
	sc->app = bonobo_window_new ("subscribe-dialog", _("Manage Subscriptions"));

	/* Build the menu and toolbar */
	container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (container, BONOBO_WINDOW (sc->app));

	/* set up the bonobo stuff */
	component = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (
		component, bonobo_object_corba_objref (BONOBO_OBJECT (container)));
	
	bonobo_ui_component_add_verb_list_with_data (
		component, verbs, sc);

	bonobo_ui_component_freeze (component, NULL);

	bonobo_ui_util_set_ui (component, EVOLUTION_DATADIR,
			       "evolution-subscribe.xml",
			       "evolution-subscribe");

	e_pixmaps_update (component, pixmaps);

	bonobo_ui_component_thaw (component, NULL);

	sc->table = gtk_table_new (1, 2, FALSE);

	sc->hpaned = e_hpaned_new ();

	folder_search_widget = make_folder_search_widget (subscribe_search, sc);
	gtk_widget_show_all (folder_search_widget);
	search_control = bonobo_control_new (folder_search_widget);

	bonobo_ui_component_object_set (component, "/Toolbar/FolderSearch",
					bonobo_object_corba_objref (BONOBO_OBJECT (search_control)),
					NULL);
					
	/* set our our contents */
#if 0
	sc->description = html_new (TRUE);
	put_html (GTK_HTML (sc->description), EXAMPLE_DESCR);

	gtk_table_attach (GTK_TABLE (sc->table), sc->description->parent->parent,
			  0, 1, 0, 1,
			  GTK_FILL | GTK_EXPAND,
			  0,
			  0, 0);
#endif

	/* set up the store etable */
	sc->store_model = e_table_simple_new (store_etable_col_count,
					      store_etable_row_count,
					      store_etable_value_at,
					      store_etable_set_value_at,
					      store_etable_is_editable,
					      store_etable_duplicate_value,
					      store_etable_free_value,
					      store_etable_initialize_value,
					      store_etable_value_is_empty,
					      store_etable_value_to_string,
					      sc);

	extras = e_table_extras_new ();

	sc->store_etable = e_table_scrolled_new (E_TABLE_MODEL(sc->store_model),
						 extras, STORE_ETABLE_SPEC, NULL);

	gtk_object_sink (GTK_OBJECT (extras));

	gtk_signal_connect (GTK_OBJECT (e_table_scrolled_get_table(E_TABLE_SCROLLED (sc->store_etable))),
			    "cursor_change", GTK_SIGNAL_FUNC (storage_selected_cb),
			    sc);

	/* set up the folder etable */
	sc->folder_model = e_tree_memory_callbacks_new (folder_etree_icon_at,

							folder_etree_column_count,

							NULL,
							NULL,

							NULL,
							NULL,

							folder_etree_value_at,
							folder_etree_set_value_at,
							folder_etree_is_editable,

							folder_etree_duplicate_value,
							folder_etree_free_value,
							folder_etree_init_value,
							folder_etree_value_is_empty,
							folder_etree_value_to_string,

							sc);

	e_tree_memory_set_expanded_default (E_TREE_MEMORY(sc->folder_model), TRUE);

	sc->folder_root = e_tree_memory_node_insert (E_TREE_MEMORY(sc->folder_model), NULL,
						     0, NULL);

	toggles[0] = gdk_pixbuf_new_from_xpm_data ((const char **)empty_xpm);
	toggles[1] = gdk_pixbuf_new_from_xpm_data ((const char **)mark_xpm);

	extras = e_table_extras_new ();

	cell = e_cell_text_new(NULL, GTK_JUSTIFY_LEFT);

	e_table_extras_add_cell (extras, "cell_text", cell);
	e_table_extras_add_cell (extras, "cell_toggle", e_cell_toggle_new (0, 2, toggles));
	e_table_extras_add_cell (extras, "cell_tree", e_cell_tree_new(NULL, NULL, TRUE, cell));

	gtk_object_set (GTK_OBJECT (cell),
			"bold_column", FOLDER_COL_SUBSCRIBED,
			NULL);

	e_table_extras_add_pixbuf (extras, "subscribed-image", toggles[1]);

	sc->folder_etree = e_tree_scrolled_new (E_TREE_MODEL(sc->folder_model),
						 extras, FOLDER_ETREE_SPEC, NULL);

	e_tree_root_node_set_visible (e_tree_scrolled_get_tree(E_TREE_SCROLLED(sc->folder_etree)), FALSE);

	gtk_object_sink (GTK_OBJECT (extras));
	gdk_pixbuf_unref(toggles[0]);
	gdk_pixbuf_unref(toggles[1]);

	gtk_signal_connect (GTK_OBJECT (e_tree_scrolled_get_tree(E_TREE_SCROLLED (sc->folder_etree))),
			    "double_click", GTK_SIGNAL_FUNC (folder_toggle_cb),
			    sc);
	gtk_table_attach (
		GTK_TABLE (sc->table), sc->folder_etree,
		0, 1, 1, 3,
		GTK_FILL | GTK_EXPAND,
		GTK_FILL | GTK_EXPAND,
		0, 0);

	e_paned_add1 (E_PANED (sc->hpaned), sc->store_etable);
	e_paned_add2 (E_PANED (sc->hpaned), sc->table);
	e_paned_set_position (E_PANED (sc->hpaned), DEFAULT_STORE_TABLE_WIDTH);

	bonobo_window_set_contents (BONOBO_WINDOW (sc->app), sc->hpaned);

#if 0
	gtk_widget_show (sc->description);
#endif

	gtk_widget_show (sc->folder_etree);
	gtk_widget_show (sc->table);
	gtk_widget_show (sc->store_etable);
	gtk_widget_show (sc->hpaned);

	/* FIXME: Session management and stuff?  */
	gtk_window_set_default_size (GTK_WINDOW (sc->app),
				     DEFAULT_WIDTH, DEFAULT_HEIGHT);

	populate_store_list (sc);
}

static void
subscribe_dialog_destroy (GtkObject *object)
{
	SubscribeDialog *sc;

	sc = SUBSCRIBE_DIALOG (object);

	/* free our folder information */
	e_tree_memory_node_remove (E_TREE_MEMORY(sc->folder_model), sc->folder_root);
	gtk_object_unref (GTK_OBJECT (sc->folder_model));
	if (sc->folder_info)
		camel_store_free_folder_info (sc->store, sc->folder_info);

	/* free our store information */
	gtk_object_unref (GTK_OBJECT (sc->store_model));
	g_list_foreach (sc->store_list, (GFunc)gtk_object_unref, NULL);

	/* free our storage */
	if (sc->storage)
		gtk_object_unref (GTK_OBJECT (sc->storage));

	/* free our search */
	if (sc->search_top)
		g_free (sc->search_top);

	subscribe_dialog_parent_class->destroy (object);
}

static void
subscribe_dialog_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = subscribe_dialog_destroy;

	subscribe_dialog_parent_class = gtk_type_class (PARENT_TYPE);
}

static void
subscribe_dialog_init (GtkObject *object)
{
}

static void
subscribe_dialog_construct (GtkObject *object, GNOME_Evolution_Shell shell)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (object);

	/*
	 * Our instance data
	 */
	sc->shell = shell;
	sc->store = NULL;
	sc->storage = NULL;
	sc->folder_info = NULL;
	sc->store_list = NULL;
	sc->search_top = NULL;

	subscribe_dialog_gui_init (sc);
}

GtkWidget *
subscribe_dialog_new (GNOME_Evolution_Shell shell)
{
	SubscribeDialog *subscribe_dialog;

	subscribe_dialog = gtk_type_new (subscribe_dialog_get_type ());

	subscribe_dialog_construct (GTK_OBJECT (subscribe_dialog), shell);

	return GTK_WIDGET (subscribe_dialog->app);
}

E_MAKE_TYPE (subscribe_dialog, "SubscribeDialog", SubscribeDialog, subscribe_dialog_class_init, subscribe_dialog_init, PARENT_TYPE);

