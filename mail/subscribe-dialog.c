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

#include <config.h>
#include <gnome.h>
#include "subscribe-dialog.h"
#include "e-util/e-html-utils.h"
#include "e-title-bar.h"
#include <gtkhtml/gtkhtml.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-tree.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-tree-simple.h>
#include <gal/e-paned/e-hpaned.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h> 
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>

#include "mail.h"
#include "mail-tools.h"
#include "mail-threads.h"
#include "camel/camel.h"

#include "art/empty.xpm"
#include "art/mark.xpm"


#define DEFAULT_STORE_TABLE_WIDTH         200
#define DEFAULT_WIDTH                     500
#define DEFAULT_HEIGHT                    300

#define PARENT_TYPE (gtk_object_get_type ())

#define FOLDER_ETABLE_SPEC "<ETableSpecification cursor-mode=\"line\"> \
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

static void
set_pixmap (BonoboUIComponent *component,
	    const char        *xml_path,
	    const char        *icon)
{
	char *path;
	GdkPixbuf *pixbuf;

	path = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons", icon);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_return_if_fail (pixbuf != NULL);

	bonobo_ui_util_set_pixbuf (component, xml_path, pixbuf);

	gdk_pixbuf_unref (pixbuf);

	g_free (path);
}

static void
update_pixmaps (BonoboUIComponent *component)
{
	set_pixmap (component, "/Toolbar/SubscribeFolder", "fetch-mail.png"); /* XXX */
	set_pixmap (component, "/Toolbar/UnsubscribeFolder", "compose-message.png"); /* XXX */
	set_pixmap (component, "/Toolbar/RefreshList", "forward.png"); /* XXX */
}

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

typedef struct get_store_input_s {
	SubscribeDialog *sc;
	gchar *url;
	SubscribeGetStoreCallback cb;
	gpointer cb_data;
} get_store_input_t;

typedef struct get_store_data_s {
	CamelStore *store;
} get_store_data_t;

static gchar *
describe_get_store (gpointer in_data, gboolean gerund)
{
	get_store_input_t *input = (get_store_input_t *) in_data;

	if (gerund) {
		return g_strdup_printf (_("Getting store for \"%s\""), input->url);
	}
	else {
		return g_strdup_printf (_("Get store for \"%s\""), input->url);
	}
}

static void
setup_get_store (gpointer in_data, gpointer op_data, CamelException *ex)
{
}

static void
do_get_store (gpointer in_data, gpointer op_data, CamelException *ex)
{
	get_store_input_t *input = (get_store_input_t *)in_data;
	get_store_data_t *data = (get_store_data_t*)op_data;

	mail_tool_camel_lock_up ();
	data->store = camel_session_get_store (session, input->url, ex);
	mail_tool_camel_lock_down ();

	if (camel_exception_is_set (ex))
		data->store = NULL;
}

static void
cleanup_get_store (gpointer in_data, gpointer op_data, CamelException *ex)
{
	get_store_input_t *input = (get_store_input_t *)in_data;
	get_store_data_t *data = (get_store_data_t*)op_data;

	input->cb (input->sc, data->store, input->cb_data);

	g_free (input->url);
}

static const mail_operation_spec op_get_store = {
        describe_get_store,
        sizeof (get_store_data_t),
        setup_get_store,
        do_get_store,
        cleanup_get_store
};

static void
subscribe_do_get_store (SubscribeDialog *sc, const char *url, SubscribeGetStoreCallback cb, gpointer cb_data)
{
	get_store_input_t *input;

        input = g_new (get_store_input_t, 1);
	input->sc = sc;
	input->url = g_strdup (url);
	input->cb = cb;
	input->cb_data = cb_data;

        mail_operation_queue (&op_get_store, input, TRUE);
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

typedef struct subscribe_folder_input_s {
	SubscribeDialog *sc;
	CamelStore *store;
	CamelFolderInfo *info;
	gboolean subscribe;
	SubscribeFolderCallback cb;
	gpointer cb_data;
} subscribe_folder_input_t;

typedef struct subscribe_folder_data_s {
	char *path;
	char *name;
	char *url;
} subscribe_folder_data_t;

static gchar *
describe_subscribe_folder (gpointer in_data, gboolean gerund)
{
	subscribe_folder_input_t *input = (subscribe_folder_input_t *) in_data;
	
	if (gerund) {
		if (input->subscribe)
			return g_strdup_printf
				(_("Subscribing to folder \"%s\""),
				 input->info->name);
		else
			return g_strdup_printf
				(_("Unsubscribing from folder \"%s\""),
				 input->info->name);
	} else {
		if (input->subscribe)
			return g_strdup_printf (_("Subscribe to folder \"%s\""),
						input->info->name);
		else
			return g_strdup_printf (_("Unsubscribe from folder \"%s\""),
						input->info->name);
	}
}

static void
setup_subscribe_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	subscribe_folder_input_t *input = (subscribe_folder_input_t *) in_data;
	subscribe_folder_data_t *data = (subscribe_folder_data_t *) op_data;
	
	data->path = storage_tree_path (input->info);
	data->name = g_strdup (input->info->name);
	data->url = g_strdup (input->info->url);
	
	camel_object_ref (CAMEL_OBJECT (input->store));
	gtk_object_ref (GTK_OBJECT (input->sc));
}

static void
do_subscribe_folder (gpointer in_data, gpointer op_data, CamelException *ex)
{
	subscribe_folder_input_t *input = (subscribe_folder_input_t *) in_data;
	subscribe_folder_data_t *data = (subscribe_folder_data_t *) op_data;
	
	mail_tool_camel_lock_up ();
	if (input->subscribe)
		camel_store_subscribe_folder (input->store, data->name, ex);
	else
		camel_store_unsubscribe_folder (input->store, data->name, ex);
	mail_tool_camel_lock_down ();
}

static void
cleanup_subscribe_folder (gpointer in_data, gpointer op_data,
			  CamelException *ex)
{
	subscribe_folder_input_t *input = (subscribe_folder_input_t *) in_data;
	subscribe_folder_data_t *data = (subscribe_folder_data_t *) op_data;
	
	if (!camel_exception_is_set (ex)) {
		if (input->subscribe)
			evolution_storage_new_folder (input->sc->storage,
						      data->path,
						      data->name, "mail",
						      data->url,
						      _("(No description)") /* XXX */,
						      FALSE);
		else
			evolution_storage_removed_folder (input->sc->storage, data->path);
	}
	
	if (input->cb)
		input->cb (input->sc, !camel_exception_is_set (ex), input->cb_data);
	
	g_free (data->path);
	g_free (data->name);
	g_free (data->url);
	
	camel_object_unref (CAMEL_OBJECT (input->store));
	gtk_object_unref (GTK_OBJECT (input->sc));
}

static const mail_operation_spec op_subscribe_folder = {
	describe_subscribe_folder,
	sizeof (subscribe_folder_data_t),
	setup_subscribe_folder,
	do_subscribe_folder,
	cleanup_subscribe_folder
};

static void
subscribe_do_subscribe_folder (SubscribeDialog *sc, CamelStore *store, CamelFolderInfo *info,
			       gboolean subscribe, SubscribeFolderCallback cb, gpointer cb_data)
{
	subscribe_folder_input_t *input;
	
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (info);
	
	input = g_new (subscribe_folder_input_t, 1);
	input->sc = sc;
	input->store = store;
	input->info = info;
	input->subscribe = subscribe;
	input->cb = cb;
	input->cb_data = cb_data;
	
	mail_operation_queue (&op_subscribe_folder, input, TRUE);
}



static gboolean
folder_info_subscribed (SubscribeDialog *sc, CamelFolderInfo *info)
{
	return camel_store_folder_subscribed (sc->store, info->full_name);
}

static void
node_changed_cb (SubscribeDialog *sc, gboolean changed, gpointer data)
{
	ETreePath *node = data;

	if (changed)
		e_tree_model_node_changed (sc->folder_model, node);
}

static void
subscribe_folder_info (SubscribeDialog *sc, CamelFolderInfo *info, ETreePath *node)
{
	/* folders without urls cannot be subscribed to */
	if (info->url == NULL)
		return;
	
	subscribe_do_subscribe_folder (sc, sc->store, info, TRUE, node_changed_cb, node);
}

static void
unsubscribe_folder_info (SubscribeDialog *sc, CamelFolderInfo *info, ETreePath *node)
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
	ETableScrolled *scrolled = E_TABLE_SCROLLED (sc->folder_etable);
	
	e_table_select_all (scrolled->table);
}

static void
subscribe_invert_selection (BonoboUIComponent *uic,
			    void *user_data, const char *path)
{
	SubscribeDialog *sc = (SubscribeDialog*)user_data;
	ETableScrolled *scrolled = E_TABLE_SCROLLED (sc->folder_etable);
	
	e_table_invert_selection (scrolled->table);
}

static void
subscribe_folder_foreach (int model_row, gpointer closure)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (closure);
	ETreePath *node = e_tree_model_node_at_row (sc->folder_model, model_row);
	CamelFolderInfo *info = e_tree_model_node_get_data (sc->folder_model, node);

	if (!folder_info_subscribed (sc, info))
		subscribe_folder_info (sc, info, node);
}

static void
subscribe_folders (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	e_table_selected_row_foreach (E_TABLE_SCROLLED(sc->folder_etable)->table,
				      subscribe_folder_foreach, sc);
}

static void
unsubscribe_folder_foreach (int model_row, gpointer closure)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (closure);
	ETreePath *node = e_tree_model_node_at_row (sc->folder_model, model_row);
	CamelFolderInfo *info = e_tree_model_node_get_data (sc->folder_model, node);

	if (folder_info_subscribed(sc, info))
		unsubscribe_folder_info (sc, info, node);
}


static void
unsubscribe_folders (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	e_table_selected_row_foreach (E_TABLE_SCROLLED(sc->folder_etable)->table,
				      unsubscribe_folder_foreach, sc);
}

static void
subscribe_refresh_list (GtkWidget *widget, gpointer user_data)
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


/* etable stuff for the subscribe ui */

static int
folder_etable_col_count (ETableModel *etm, void *data)
{
	return FOLDER_COL_LAST;
}

static void*
folder_etable_duplicate_value (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup (val);
}

static void
folder_etable_free_value (ETableModel *etm, int col, void *val, void *data)
{
	g_free (val);
}

static void*
folder_etable_init_value (ETableModel *etm, int col, void *data)
{
	return g_strdup ("");
}

static gboolean
folder_etable_value_is_empty (ETableModel *etm, int col, const void *val, void *data)
{
	return !(val && *(char *)val);
}

static char*
folder_etable_value_to_string (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup(val);
}

static GdkPixbuf*
folder_etree_icon_at (ETreeModel *etree, ETreePath *path, void *model_data)
{
	return NULL; /* XXX no icons for now */
}

static void*
folder_etree_value_at (ETreeModel *etree, ETreePath *path, int col, void *model_data)
{
	SubscribeDialog *dialog = SUBSCRIBE_DIALOG (model_data);
	CamelFolderInfo *info = e_tree_model_node_get_data (etree, path);

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
folder_etree_set_value_at (ETreeModel *etree, ETreePath *path, int col, const void *val, void *model_data)
{
	/* nothing */
}

static gboolean
folder_etree_is_editable (ETreeModel *etree, ETreePath *path, int col, void *model_data)
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
build_etree_from_folder_info (SubscribeDialog *sc, ETreePath *parent, CamelFolderInfo *info)
{
	CamelFolderInfo *i;

	if (info == NULL)
		return;

	for (i = info; i; i = i->sibling) {
		ETreePath *node = e_tree_model_node_insert (sc->folder_model, parent, -1, i);
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

	e_tree_model_node_remove (sc->folder_model, sc->folder_root);
	sc->folder_root = e_tree_model_node_insert (sc->folder_model, NULL,
						    0, NULL);


	build_etree_from_folder_info (sc, sc->folder_root, sc->folder_info);

	camel_exception_free (ex);
}

static void
storage_selected_cb (ETable *table, int row, gpointer data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);
	CamelStore *store = (CamelStore*)g_list_nth_data (sc->store_list, row);

	build_tree (sc, store);
}



static void
folder_toggle_cb (ETable *table, int row, int col, GdkEvent *event, gpointer data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);
	ETreePath *node = e_tree_model_node_at_row (sc->folder_model, row);
	CamelFolderInfo *info = e_tree_model_node_get_data (sc->folder_model, node);

	if (folder_info_subscribed(sc, info))
		unsubscribe_folder_info (sc, info, node);
	else
		subscribe_folder_info (sc, info, node);

	e_tree_model_node_changed (sc->folder_model, node);
}



#define EXAMPLE_DESCR "And the beast shall come forth surrounded by a roiling cloud of vengeance.\n" \
"  The house of the unbelievers shall be razed and they shall be scorched to the\n" \
"          earth. Their tags shall blink until the end of days. \n" \
"                 from The Book of Mozilla, 12:10"

static BonoboUIVerb verbs [] = {
	/* File Menu */
	BONOBO_UI_UNSAFE_VERB ("FileCloseWin", subscribe_close),

	/* Edit Menu */
	BONOBO_UI_UNSAFE_VERB ("EditSelectAll", subscribe_select_all),
	BONOBO_UI_UNSAFE_VERB ("EditInvertSelection", subscribe_invert_selection),
	
	/* Folder Menu / Toolbar */
	BONOBO_UI_UNSAFE_VERB ("SubscribeFolder", subscribe_folders),
	BONOBO_UI_UNSAFE_VERB ("UnsubscribeFolder", unsubscribe_folders),

	/* Toolbar Specific */
	BONOBO_UI_UNSAFE_VERB ("RefreshList", subscribe_refresh_list),

	BONOBO_UI_VERB_END
};

static void
store_cb (SubscribeDialog *sc, CamelStore *store, gpointer data)
{
	if (!store)
		return;

	if (camel_store_supports_subscriptions (store)) {
		sc->store_list = g_list_prepend (sc->store_list, store);
		e_table_model_row_inserted (sc->store_model, 0);
	}
	else {
		camel_object_unref (CAMEL_OBJECT (store));
	}
}

static void
populate_store_foreach (MailConfigService *service, SubscribeDialog *sc)
{
	subscribe_do_get_store (sc, service->url, store_cb, NULL);
}

static void
populate_store_list (SubscribeDialog *sc)
{
	GSList *sources;

	sources = mail_config_get_sources ();
	g_slist_foreach (sources, (GFunc)populate_store_foreach, sc);
	sources = mail_config_get_news ();
	g_slist_foreach (sources, (GFunc)populate_store_foreach, sc);

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
	sc->app = bonobo_window_new ("subscribe-dialog", "Manage Subscriptions");

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

	update_pixmaps (component);

	bonobo_ui_component_thaw (component, NULL);

	sc->table = gtk_table_new (1, 2, FALSE);

	sc->hpaned = e_hpaned_new ();

	folder_search_widget = make_folder_search_widget (subscribe_search, sc);
	gtk_widget_show_all (folder_search_widget);
	search_control = bonobo_control_new (folder_search_widget);

	bonobo_ui_component_object_set (
		component, "/Toolbar/FolderSearch",
		bonobo_object_corba_objref (BONOBO_OBJECT (search_control)), NULL);
					
	/* set our our contents */
#if 0
	sc->description = html_new (TRUE);
	put_html (GTK_HTML (sc->description), EXAMPLE_DESCR);

	gtk_table_attach (
		GTK_TABLE (sc->table), sc->description->parent->parent,
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
	sc->folder_model = e_tree_simple_new (folder_etable_col_count,
					      folder_etable_duplicate_value,
					      folder_etable_free_value,
					      folder_etable_init_value,
					      folder_etable_value_is_empty,
					      folder_etable_value_to_string,
					      folder_etree_icon_at,
					      folder_etree_value_at,
					      folder_etree_set_value_at,
					      folder_etree_is_editable,
					      sc);

	sc->folder_root = e_tree_model_node_insert (sc->folder_model, NULL,
						    0, NULL);

	e_tree_model_root_node_set_visible (sc->folder_model, FALSE);
	e_tree_model_set_expanded_default (sc->folder_model, TRUE);

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

	sc->folder_etable = e_table_scrolled_new (E_TABLE_MODEL(sc->folder_model),
						  extras, FOLDER_ETABLE_SPEC, NULL);

	gtk_object_sink (GTK_OBJECT (extras));
	gdk_pixbuf_unref(toggles[0]);
	gdk_pixbuf_unref(toggles[1]);

	gtk_signal_connect (GTK_OBJECT (e_table_scrolled_get_table(E_TABLE_SCROLLED (sc->folder_etable))),
			    "double_click", GTK_SIGNAL_FUNC (folder_toggle_cb),
			    sc);
	gtk_table_attach (
		GTK_TABLE (sc->table), sc->folder_etable,
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

	gtk_widget_show (sc->folder_etable);
	gtk_widget_show (sc->table);
	gtk_widget_show (sc->store_etable);
	gtk_widget_show (sc->hpaned);

	/* FIXME: Session management and stuff?  */
	gtk_window_set_default_size (
		GTK_WINDOW (sc->app),
		DEFAULT_WIDTH, DEFAULT_HEIGHT);

	populate_store_list (sc);
}

static void
subscribe_dialog_destroy (GtkObject *object)
{
	SubscribeDialog *sc;

	sc = SUBSCRIBE_DIALOG (object);

	/* free our folder information */
	e_tree_model_node_remove (sc->folder_model, sc->folder_root);
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

