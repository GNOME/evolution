/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * gal-view-instance.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <util/e-i18n.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtksignal.h>
#include <gnome-xml/parser.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-dialog.h>
#include <gal/util/e-util.h>
#include <gal/util/e-xml-utils.h>
#include <gal/widgets/e-unicode.h>
#include "gal-view-instance.h"
#include "gal-view-instance-save-as-dialog.h"
#include "gal-define-views-dialog.h"
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtkcheckmenuitem.h>

#define GVI_CLASS(e) ((GalViewInstanceClass *)((GtkObject *)e)->klass)

#define PARENT_TYPE gtk_object_get_type ()

static GtkObjectClass *gal_view_instance_parent_class;

static const EPopupMenu separator = E_POPUP_SEPARATOR;
static const EPopupMenu terminator = E_POPUP_TERMINATOR;


#define d(x)

enum {
	DISPLAY_VIEW,
	CHANGED,
	LAST_SIGNAL
};

static guint gal_view_instance_signals [LAST_SIGNAL] = { 0, };

static void
gal_view_instance_changed (GalViewInstance *instance)
{
	g_return_if_fail (instance != NULL);
	g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	gtk_signal_emit (GTK_OBJECT (instance),
			 gal_view_instance_signals [CHANGED]);
}

static void
gal_view_instance_display_view (GalViewInstance *instance, GalView *view)
{
	g_return_if_fail (instance != NULL);
	g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	gtk_signal_emit (GTK_OBJECT (instance),
			 gal_view_instance_signals [DISPLAY_VIEW],
			 view);
}

static void
save_current_view (GalViewInstance *instance)
{
	xmlDoc *doc;
	xmlNode *root;

	doc = xmlNewDoc("1.0");
	root = xmlNewNode (NULL, "GalViewCurrentView");
	xmlDocSetRootElement(doc, root);

	if (instance->current_id)
		e_xml_set_string_prop_by_name (root, "current_view", instance->current_id);
	if (instance->current_type)
		e_xml_set_string_prop_by_name (root, "current_view_type", instance->current_type);

	xmlSaveFile(instance->current_view_filename, doc);
	xmlFreeDoc(doc);
}

static void
view_changed (GalView *view, GalViewInstance *instance)
{
	if (instance->current_id != NULL) {
		g_free (instance->current_id);
		instance->current_id = NULL;
		save_current_view (instance);
		gal_view_instance_changed(instance);
	}

	gal_view_save (view, instance->custom_filename);
}

static void
disconnect_view (GalViewInstance *instance)
{
	if (instance->current_view) {
		if (instance->view_changed_id) {
			gtk_signal_disconnect (GTK_OBJECT (instance->current_view),
					       instance->view_changed_id);
		}

		gtk_object_unref (GTK_OBJECT (instance->current_view));
	}
	g_free (instance->current_type);
	g_free (instance->current_title);
	instance->current_title = NULL;
	instance->current_type = NULL;
	instance->view_changed_id = 0;
	instance->current_view = NULL;
}

static void
connect_view (GalViewInstance *instance, GalView *view)
{
	if (instance->current_view)
		disconnect_view (instance);
	instance->current_view = view;

	instance->current_title = g_strdup (gal_view_get_title(view));
	instance->current_type = g_strdup (gal_view_get_type_code(view));
	instance->view_changed_id =
		gtk_signal_connect(GTK_OBJECT(instance->current_view), "changed",
				   GTK_SIGNAL_FUNC(view_changed), instance);

	gal_view_instance_display_view (instance, instance->current_view);
}

static void
gal_view_instance_destroy (GtkObject *object)
{
	GalViewInstance *instance = GAL_VIEW_INSTANCE(object);

	if (instance->collection)
		gtk_object_unref (GTK_OBJECT (instance->collection));

	g_free (instance->instance_id);
	g_free (instance->custom_filename);
	g_free (instance->current_view_filename);

	g_free (instance->current_id);
	disconnect_view (instance);

	g_free (instance->default_view);

	if (gal_view_instance_parent_class->destroy)
		(*gal_view_instance_parent_class->destroy)(object);
}

static void
gal_view_instance_class_init (GtkObjectClass *object_class)
{
	GalViewInstanceClass *klass = GAL_VIEW_INSTANCE_CLASS(object_class);
	gal_view_instance_parent_class = gtk_type_class (PARENT_TYPE);
	
	object_class->destroy = gal_view_instance_destroy;

	gal_view_instance_signals [DISPLAY_VIEW] =
		gtk_signal_new ("display_view",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GalViewInstanceClass, display_view),
				gtk_marshal_NONE__OBJECT,
				GTK_TYPE_NONE, 1, GAL_VIEW_TYPE);

	gal_view_instance_signals [CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				E_OBJECT_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GalViewInstanceClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	E_OBJECT_CLASS_ADD_SIGNALS (object_class, gal_view_instance_signals, LAST_SIGNAL);

	klass->display_view = NULL;
	klass->changed      = NULL;
}

static void
gal_view_instance_init (GalViewInstance *instance)
{
	instance->collection            = NULL;

	instance->instance_id           = NULL;
	instance->custom_filename       = NULL;
	instance->current_view_filename = NULL;

	instance->current_title         = NULL;
	instance->current_type          = NULL;
	instance->current_id            = NULL;
	instance->current_view          = NULL;

	instance->view_changed_id       = 0;
	instance->collection_changed_id       = 0;

	instance->loaded = FALSE;
	instance->default_view = NULL;
}

/**
 * gal_view_instance_get_type:
 *
 */
guint
gal_view_instance_get_type (void)
{
	static guint type = 0;
	
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GalViewInstance",
			sizeof (GalViewInstance),
			sizeof (GalViewInstanceClass),
			(GtkClassInitFunc) gal_view_instance_class_init,
			(GtkObjectInitFunc) gal_view_instance_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (PARENT_TYPE, &info);
	}

  return type;
}

static void
load_current_view (GalViewInstance *instance)
{
	xmlDoc *doc;
	xmlNode *root;
	GalView *view = NULL;

	doc = xmlParseFile(instance->current_view_filename);

	if (doc == NULL) {
		instance->current_id = g_strdup (gal_view_instance_get_default_view (instance));

		if (instance->current_id) {
			int index = gal_view_collection_get_view_index_by_id (instance->collection,
									      instance->current_id);

			if (index != -1) {
				view = gal_view_collection_get_view (instance->collection,
								     index);
				view = gal_view_clone(view);
				connect_view (instance, view);
			}
		}
		return;
	}

	root = xmlDocGetRootElement(doc);
	instance->current_id = e_xml_get_string_prop_by_name_with_default (root, "current_view", NULL);

	if (instance->current_id != NULL) {
		int index = gal_view_collection_get_view_index_by_id (instance->collection,
								      instance->current_id);

		if (index != -1) {
			view = gal_view_collection_get_view (instance->collection,
							     index);
			view = gal_view_clone(view);
		}
	}
	if (view == NULL) {
		char *type;
		type = e_xml_get_string_prop_by_name_with_default (root, "current_view_type", NULL);
		view = gal_view_collection_load_view_from_file (instance->collection,
								type,
								instance->custom_filename);
		g_free (type);
	}

	connect_view (instance, view);

	xmlFreeDoc(doc);
}

/**
 * gal_view_instance_new:
 * @collection: This %GalViewCollection should be loaded before being passed to this function.
 * @instance_id: Which instance of this type of object is this (for most of evo, this is the folder id.)
 * 
 * Create a new %GalViewInstance.
 * 
 * Return value: The new %GalViewInstance.
 **/
GalViewInstance *
gal_view_instance_new (GalViewCollection *collection, const char *instance_id)
{
	GalViewInstance *instance = gtk_type_new(gal_view_instance_get_type());
	if (gal_view_instance_construct (instance, collection, instance_id))
		return instance;
	else {
		gtk_object_unref (GTK_OBJECT (instance));
		return NULL;
	}
}

GalViewInstance *
gal_view_instance_construct (GalViewInstance *instance, GalViewCollection *collection, const char *instance_id)
{
	char *filename;
	char *safe_id;

	g_return_val_if_fail (gal_view_collection_loaded (collection), NULL);

	instance->collection = collection;
	if (collection)
		gtk_object_ref (GTK_OBJECT (collection));

	if (instance_id)
		instance->instance_id = g_strdup (instance_id);
	else
		instance->instance_id = g_strdup ("");

	safe_id = g_strdup (instance->instance_id);
	e_filename_make_safe (safe_id);

	filename = g_strdup_printf ("custom_view-%s.xml", safe_id);
	instance->custom_filename = g_concat_dir_and_file (instance->collection->local_dir, filename);
	g_free (filename);

	filename = g_strdup_printf ("current_view-%s.xml", safe_id);
	instance->current_view_filename = g_concat_dir_and_file (instance->collection->local_dir, filename);
	g_free (filename);

	g_free (safe_id);

	return instance;
}

/* Manipulate the current view. */
char *
gal_view_instance_get_current_view_id (GalViewInstance *instance)
{
	if (instance->current_id && gal_view_collection_get_view_index_by_id (instance->collection, instance->current_id) != -1)
		return g_strdup (instance->current_id);
	else
		return NULL;
}

void
gal_view_instance_set_current_view_id (GalViewInstance *instance, const char *view_id)
{
	GalView *view;
	int index;

	g_return_if_fail (instance != NULL);
	g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	d(g_print("%s: view_id set to %s\n", __FUNCTION__, view_id));

	if (instance->current_id && !strcmp (instance->current_id, view_id))
		return;

	g_free (instance->current_id);
	instance->current_id = g_strdup (view_id);

	index = gal_view_collection_get_view_index_by_id (instance->collection, view_id);
	if (index != -1) {
		view = gal_view_collection_get_view (instance->collection, index);
		connect_view (instance, gal_view_clone (view));
	}

	save_current_view (instance);
	gal_view_instance_changed(instance);
	gal_view_instance_display_view (instance, instance->current_view);
}

GalView *
gal_view_instance_get_current_view (GalViewInstance *instance)
{
	return instance->current_view;
}

void
gal_view_instance_set_custom_view (GalViewInstance *instance, GalView *view)
{
	g_free (instance->current_id);
	instance->current_id = NULL;

	view = gal_view_clone (view);
	connect_view (instance, view);
	gal_view_save (view, instance->custom_filename);
	save_current_view (instance);
	gal_view_instance_changed(instance);
}

static void
dialog_clicked(GtkWidget *dialog, int button, GalViewInstance *instance)
{
	if (button == 0) {
		gal_view_instance_save_as_dialog_save (GAL_VIEW_INSTANCE_SAVE_AS_DIALOG (dialog));
	}
	gnome_dialog_close(GNOME_DIALOG(dialog));
}

void
gal_view_instance_save_as (GalViewInstance *instance)
{
	GtkWidget *dialog = gal_view_instance_save_as_dialog_new(instance);
	gtk_signal_connect(GTK_OBJECT(dialog), "clicked",
			   GTK_SIGNAL_FUNC(dialog_clicked), instance);
	gtk_widget_show(dialog);
}

/* This is idempotent.  Once it's been called once, the rest of the calls are ignored. */
void
gal_view_instance_load (GalViewInstance *instance)
{
	if (!instance->loaded) {
		load_current_view (instance);
		instance->loaded = TRUE;
	}
}

/* These only mean anything before gal_view_instance_load is called the first time.  */
const char *
gal_view_instance_get_default_view (GalViewInstance *instance)
{
	if (instance->default_view)
		return instance->default_view;
	else
		return gal_view_collection_get_default_view (instance->collection);
}

void
gal_view_instance_set_default_view (GalViewInstance *instance, const char *id)
{
	g_free (instance->default_view);
	instance->default_view = g_strdup (id);
}

gboolean
gal_view_instance_exists (GalViewInstance *instance)
{
	struct stat st;

	if (instance->current_view_filename && stat (instance->current_view_filename, &st) == 0 && st.st_size > 0 && S_ISREG (st.st_mode))
		return TRUE;
	else
		return FALSE;
	
}

typedef struct {
	GalViewInstance *instance;
	char *id;
} ListenerClosure;

static void
view_item_cb (GtkWidget *widget,
	      gpointer user_data)
{
	ListenerClosure *closure = user_data;

	if (GTK_CHECK_MENU_ITEM (widget)->active) {
		gal_view_instance_set_current_view_id (closure->instance, closure->id);
	}
}

static void
add_popup_radio_item (EPopupMenu *menu_item,
		      gchar *title,
		      void (*fn) (GtkWidget *widget, gpointer closure),
		      gpointer closure,
		      gboolean value)
{
	const EPopupMenu menu_item_struct = 
		E_POPUP_RADIO_ITEM_CC (title,
				       fn,
				       closure,
				       0,
				       value);

	e_popup_menu_copy_1 (menu_item, &menu_item_struct);
}

static void
add_popup_menu_item (EPopupMenu *menu_item,
		     gchar *title,
		     void (*fn) (GtkWidget *widget, gpointer closure),
		     gpointer closure)
{
	const EPopupMenu menu_item_struct = 
		E_POPUP_ITEM_CC (title,
				 fn,
				 closure,
				 0);

	e_popup_menu_copy_1 (menu_item, &menu_item_struct);
}

static void
define_views_dialog_clicked(GtkWidget *dialog, int button, GalViewInstance *instance)
{
	if (button == 0) {
		gal_view_collection_save(instance->collection);
	}
	gnome_dialog_close(GNOME_DIALOG(dialog));
}

static void
define_views_cb(GtkWidget *widget,
		GalViewInstance *instance)
{
	GtkWidget *dialog = gal_define_views_dialog_new(instance->collection);
	gtk_signal_connect(GTK_OBJECT(dialog), "clicked",
			   GTK_SIGNAL_FUNC(define_views_dialog_clicked), instance);
	gtk_widget_show(dialog);
}

static void
save_current_view_cb(GtkWidget *widget,
		     GalViewInstance      *instance)
{
	gal_view_instance_save_as (instance);
}

EPopupMenu *
gal_view_instance_get_popup_menu (GalViewInstance *instance)
{
	EPopupMenu *ret_val;
	int length;
	int i;
	gboolean found = FALSE;
	char *id;

	length = gal_view_collection_get_count(instance->collection);
	id = gal_view_instance_get_current_view_id (instance);

	ret_val = g_new (EPopupMenu, length + 6);

	for (i = 0; i < length; i++) {
		gboolean value = FALSE;
		GalViewCollectionItem *item = gal_view_collection_get_view_item(instance->collection, i);
		ListenerClosure *closure;

		closure            = g_new (ListenerClosure, 1);
		closure->instance  = instance;
		closure->id        = item->id;
		gtk_object_ref (GTK_OBJECT(closure->instance));

		if (!found && id && !strcmp (id, item->id)) {
			found = TRUE;
			value = TRUE;
		}

		add_popup_radio_item (ret_val + i, item->title, GTK_SIGNAL_FUNC (view_item_cb), closure, value);
	}

	if (!found) {
		e_popup_menu_copy_1 (ret_val + i++, &separator);

		add_popup_radio_item (ret_val + i++, N_("Custom View"), NULL, NULL, TRUE);
		add_popup_menu_item (ret_val + i++, N_("Save Custom View"), GTK_SIGNAL_FUNC (save_current_view_cb), instance);
	}

	e_popup_menu_copy_1 (ret_val + i++, &separator);
	add_popup_menu_item (ret_val + i++, N_("Define Views"), GTK_SIGNAL_FUNC (define_views_cb), instance);
	e_popup_menu_copy_1 (ret_val + i++, &terminator);

	return ret_val;
}

void
gal_view_instance_free_popup_menu (GalViewInstance *instance, EPopupMenu *menu)
{
	int i;
	/* This depends on the first non-custom closure to be a separator or a terminator. */
	for (i = 0; menu[i].name && *(menu[i].name); i++) { 
		gtk_object_unref (GTK_OBJECT ( ((ListenerClosure *)(menu[i].closure))->instance));
		g_free (menu[i].closure);
	}

	e_popup_menu_free (menu);
}
