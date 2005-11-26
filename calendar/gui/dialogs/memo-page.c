/* Evolution calendar - Main page of the memo editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <libedataserverui/e-source-option-menu.h>
#include <widgets/misc/e-dateedit.h>

#include "common/authentication.h"
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-categories-config.h"
#include "e-util/e-util-private.h"
#include "../calendar-config.h"
#include "comp-editor.h"
#include "comp-editor-util.h"
#include "e-send-options-utils.h"
#include "memo-page.h"


/* Private part of the TaskPage structure */
struct _MemoPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	GtkWidget *memo_content;

	GtkWidget *classification;

	GtkWidget *categories_btn;
	GtkWidget *categories;

	GtkWidget *source_selector;

	gboolean updating;
};

static const int classification_map[] = {
	E_CAL_COMPONENT_CLASS_PUBLIC,
	E_CAL_COMPONENT_CLASS_PRIVATE,
	E_CAL_COMPONENT_CLASS_CONFIDENTIAL,
	-1
};



static void memo_page_finalize (GObject *object);

static GtkWidget *memo_page_get_widget (CompEditorPage *page);
static void memo_page_focus_main_widget (CompEditorPage *page);
static gboolean memo_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean memo_page_fill_component (CompEditorPage *page, ECalComponent *comp);

G_DEFINE_TYPE (MemoPage, memo_page, TYPE_COMP_EDITOR_PAGE);



/**
 * memo_page_get_type:
 * 
 * Registers the #TaskPage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #TaskPage class.
 **/

/* Class initialization function for the memo page */
static void
memo_page_class_init (MemoPageClass *klass)
{
	CompEditorPageClass *editor_page_class;
	GObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) klass;
	object_class = (GObjectClass *) klass;

	editor_page_class->get_widget = memo_page_get_widget;
	editor_page_class->focus_main_widget = memo_page_focus_main_widget;
	editor_page_class->fill_widgets = memo_page_fill_widgets;
	editor_page_class->fill_component = memo_page_fill_component;
	
	object_class->finalize = memo_page_finalize;
}

/* Object initialization function for the memo page */
static void
memo_page_init (MemoPage *tpage)
{
	MemoPagePrivate *priv;

	priv = g_new0 (MemoPagePrivate, 1);
	tpage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;
	priv->memo_content = NULL;
	priv->classification = NULL;
	priv->categories_btn = NULL;
	priv->categories = NULL;

	priv->updating = FALSE;
}

/* Destroy handler for the memo page */
static void
memo_page_finalize (GObject *object)
{
	MemoPage *tpage;
	MemoPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MEMO_PAGE (object));

	tpage = MEMO_PAGE (object);
	priv = tpage->priv;

	if (priv->main)
		gtk_widget_unref (priv->main);

	if (priv->xml) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	g_free (priv);
	tpage->priv = NULL;

	if (G_OBJECT_CLASS (memo_page_parent_class)->finalize)
		(* G_OBJECT_CLASS (memo_page_parent_class)->finalize) (object);
}



/* get_widget handler for the task page */
static GtkWidget *
memo_page_get_widget (CompEditorPage *page)
{
	MemoPage *tpage;
	MemoPagePrivate *priv;

	tpage = MEMO_PAGE (page);
	priv = tpage->priv;

	return priv->main;
}

/* focus_main_widget handler for the memo page */
static void
memo_page_focus_main_widget (CompEditorPage *page)
{
	MemoPage *tpage;
	MemoPagePrivate *priv;

	tpage = MEMO_PAGE (page);
	priv = tpage->priv;

	gtk_widget_grab_focus (priv->memo_content);
}

/* Fills the widgets with default values */
static void
clear_widgets (MemoPage *tpage)
{
	MemoPagePrivate *priv;

	priv = tpage->priv;

	/* memo content */
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content)), "", 0);

	/* Classification */
	e_dialog_option_menu_set (priv->classification, E_CAL_COMPONENT_CLASS_PRIVATE, classification_map);

	/* Categories */
	e_dialog_editable_set (priv->categories, NULL);
}

/* Decode the radio button group for classifications */
static ECalComponentClassification
classification_get (GtkWidget *widget)
{
	return e_dialog_option_menu_get (widget, classification_map);
}

static void
sensitize_widgets (MemoPage *mpage)
{
	gboolean read_only;
	MemoPagePrivate *priv;
	
	priv = mpage->priv;

	if (!e_cal_is_read_only (COMP_EDITOR_PAGE (mpage)->client, &read_only, NULL))
		read_only = TRUE;
	
	gtk_widget_set_sensitive (priv->memo_content, !read_only);
	gtk_widget_set_sensitive (priv->classification, !read_only);
	gtk_widget_set_sensitive (priv->categories_btn, !read_only);
	gtk_entry_set_editable (GTK_ENTRY (priv->categories), !read_only);
}

/* fill_widgets handler for the memo page */
static gboolean
memo_page_fill_widgets (CompEditorPage *page, ECalComponent *comp)
{
	MemoPage *tpage;
	MemoPagePrivate *priv;
	ECalComponentClassification cl;
	GSList *l;
	const char *categories;
	ESource *source;

	tpage = MEMO_PAGE (page);
	priv = tpage->priv;

	priv->updating = TRUE;
	
	/* Clean the screen */
	clear_widgets (tpage);

	e_cal_component_get_description_list (comp, &l);
	if (l && l->data) {
		ECalComponentText *dtext;
		
		dtext = l->data;
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content)),
					  dtext->value ? dtext->value : "", -1);
	} else {
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content)),
					  "", 0);
	}
	e_cal_component_free_text_list (l);

	/* Classification. */
	e_cal_component_get_classification (comp, &cl);

	switch (cl) {
	case E_CAL_COMPONENT_CLASS_PUBLIC:
	case E_CAL_COMPONENT_CLASS_PRIVATE:
	case E_CAL_COMPONENT_CLASS_CONFIDENTIAL:
		break;
	default:
		/* default to PUBLIC */
		cl = E_CAL_COMPONENT_CLASS_PUBLIC;
                break;
	}
	e_dialog_option_menu_set (priv->classification, cl, classification_map);

	/* Categories */
	e_cal_component_get_categories (comp, &categories);
	e_dialog_editable_set (priv->categories, categories);

	/* Source */
	source = e_cal_get_source (page->client);
	e_source_option_menu_select (E_SOURCE_OPTION_MENU (priv->source_selector), source);

	priv->updating = FALSE;

	sensitize_widgets (tpage);

	return TRUE;
}

/* fill_component handler for the memo page */
static gboolean
memo_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	MemoPage *tpage;
	MemoPagePrivate *priv;
	char *cat, *str;
	int i;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;

	tpage = MEMO_PAGE (page);
	priv = tpage->priv;
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content));

	/* Memo Content */

	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	if (!str || strlen (str) == 0){
		e_cal_component_set_description_list (comp, NULL);
		e_cal_component_set_summary(comp, NULL);
	}
	else {
		int idxToUse = -1, nstr = strlen(str);
		gboolean foundNL = FALSE;
		GSList l;
		ECalComponentText text, sumText;
		char *txt;

		for(i = 0; i<nstr && i<50; i++){
			if(str[i] == '\n'){
				idxToUse = i;
				foundNL = TRUE;
				break;
			}
		}
		
		if(foundNL == FALSE){
			if(nstr > 50){
				sumText.value = txt = g_strndup(str, 50);
			}
			else{
				sumText.value = txt = g_strdup(str);
			}
		}
		else{
			sumText.value = txt = g_strndup(str, idxToUse); /* cuts off '\n' */
		}
		
		sumText.altrep = NULL;

		text.value = str;
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		e_cal_component_set_summary(comp, &sumText);
		e_cal_component_set_description_list (comp, &l);
		
		g_free(txt);
	}

	if (str)
		g_free (str);

	/* Classification. */
	e_cal_component_set_classification (comp, classification_get (priv->classification));

	/* Categories */
	cat = e_dialog_editable_get (priv->categories);
	str = comp_editor_strip_categories (cat);
	if (cat)
		g_free (cat);

	e_cal_component_set_categories (comp, str);

	if (str)
		g_free (str);

	return TRUE;
}




/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (MemoPage *tpage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (tpage);
	MemoPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;

	priv = tpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("memo-page");
	if (!priv->main){
		g_warning("couldn't find memo-page!");
		return FALSE;
	}

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	   it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups) {
		page->accel_group = accel_groups->data;
		gtk_accel_group_ref (page->accel_group);
	}

	gtk_widget_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);

	priv->memo_content = GW ("memo_content");

	priv->classification = GW ("classification");

	priv->categories_btn = GW ("categories-button");
	priv->categories = GW ("categories");

	priv->source_selector = GW ("source");

#undef GW

	return (priv->classification
		&& priv->memo_content
		&& priv->categories_btn
		&& priv->categories);
}

/* Callback used when the categories button is clicked; we must bring up the
 * category list dialog.
 */
static void
categories_clicked_cb (GtkWidget *button, gpointer data)
{
	MemoPage *tpage;
	MemoPagePrivate *priv;
	GtkWidget *entry;

	tpage = MEMO_PAGE (data);
	priv = tpage->priv;

	entry = priv->categories;
	e_categories_config_open_dialog_for_entry (GTK_ENTRY (entry));
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	MemoPage *tpage;
	MemoPagePrivate *priv;
	
	tpage = MEMO_PAGE (data);
	priv = tpage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (tpage));
}

static void
source_changed_cb (GtkWidget *widget, ESource *source, gpointer data)
{
	MemoPage *tpage;
	MemoPagePrivate *priv;

	tpage = MEMO_PAGE (data);
	priv = tpage->priv;

	if (!priv->updating) {
		ECal *client;

		client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
		if (!client || !e_cal_open (client, FALSE, NULL)) {
			GtkWidget *dialog;

			if (client)
				g_object_unref (client);

			e_source_option_menu_select (E_SOURCE_OPTION_MENU (priv->source_selector),
						     e_cal_get_source (COMP_EDITOR_PAGE (tpage)->client));

			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open memos in '%s'."),
							 e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		} else {
			comp_editor_notify_client_changed (
				COMP_EDITOR (gtk_widget_get_toplevel (priv->main)),
				client);
			sensitize_widgets (tpage);
		}
	}
}

/* Hooks the widget signals */
static gboolean
init_widgets (MemoPage *tpage)
{
	MemoPagePrivate *priv;
	GtkTextBuffer *text_buffer;

	priv = tpage->priv;

	/* Memo Content */
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content));

	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->memo_content), GTK_WRAP_WORD);

	/* Categories button */
	g_signal_connect((priv->categories_btn), "clicked",
			    G_CALLBACK (categories_clicked_cb), tpage);

	/* Source selector */
	g_signal_connect((priv->source_selector), "source_selected",
			 G_CALLBACK (source_changed_cb), tpage);

	/* Connect the default signal handler to use to make sure the "changed"
	   field gets set whenever a field is changed. */

	/* Belongs to priv->memo_content */
	g_signal_connect ((text_buffer), "changed",
			  G_CALLBACK (field_changed_cb), tpage);

	g_signal_connect((priv->classification), "changed",
			    G_CALLBACK (field_changed_cb), tpage);
	g_signal_connect((priv->categories), "changed",
			    G_CALLBACK (field_changed_cb), tpage);
	
	return TRUE;
}


/**
 * memo_page_construct:
 * @tpage: An memo page.
 * 
 * Constructs an memo page by loading its Glade data.
 * 
 * Return value: The same object as @tpage, or NULL if the widgets could not be
 * created.
 **/
MemoPage *
memo_page_construct (MemoPage *tpage)
{
	MemoPagePrivate *priv;
	char *gladefile;

	priv = tpage->priv;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "memo-page.glade",
				      NULL);
	priv->xml = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);

	if (!priv->xml) {
		g_message ("memo_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (tpage)) {
		g_message ("memo_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	if (!init_widgets (tpage)) {
		g_message ("memo_page_construct(): " 
			   "Could not initialize the widgets!");
		return NULL;
	}

	return tpage;
}

/**
 * memo_page_new:
 * 
 * Creates a new memo page.
 * 
 * Return value: A newly-created task page, or NULL if the page could
 * not be created.
 **/
MemoPage *
memo_page_new (void)
{
	MemoPage *tpage;

	tpage = gtk_type_new (TYPE_MEMO_PAGE);
	if (!memo_page_construct (tpage)) {
		g_object_unref (tpage);
		return NULL;
	}

	return tpage;
}

GtkWidget *memo_page_create_source_option_menu (void);

GtkWidget *
memo_page_create_source_option_menu (void)
{
	GtkWidget   *menu;
	GConfClient *gconf_client;
	ESourceList *source_list;

	gconf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (gconf_client, "/apps/evolution/memos/sources");

	menu = e_source_option_menu_new (source_list);
	g_object_unref (source_list);

	gtk_widget_show (menu);
	return menu;
}
