/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "gal-view-instance.h"

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <libedataserver/libedataserver.h>

#include "e-unicode.h"
#include "e-misc-utils.h"
#include "e-xml-utils.h"
#include "gal-view-instance-save-as-dialog.h"

G_DEFINE_TYPE (GalViewInstance, gal_view_instance, G_TYPE_OBJECT)

#define d(x)

enum {
	DISPLAY_VIEW,
	CHANGED,
	LOADED,
	LAST_SIGNAL
};

static guint gal_view_instance_signals[LAST_SIGNAL] = { 0, };

static void
gal_view_instance_changed (GalViewInstance *instance)
{
	g_return_if_fail (instance != NULL);
	g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	g_signal_emit (
		instance,
		gal_view_instance_signals[CHANGED], 0);
}

static void
gal_view_instance_display_view (GalViewInstance *instance,
                                GalView *view)
{
	g_return_if_fail (instance != NULL);
	g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	g_signal_emit (
		instance,
		gal_view_instance_signals[DISPLAY_VIEW], 0,
		view);
}

static void
save_current_view (GalViewInstance *instance)
{
	xmlDoc *doc;
	xmlNode *root;

	doc = xmlNewDoc ((const guchar *)"1.0");
	root = xmlNewNode (NULL, (const guchar *)"GalViewCurrentView");
	xmlDocSetRootElement (doc, root);

	if (instance->current_id)
		e_xml_set_string_prop_by_name (root, (const guchar *)"current_view", instance->current_id);
	if (instance->current_type)
		e_xml_set_string_prop_by_name (root, (const guchar *)"current_view_type", instance->current_type);

	if (e_xml_save_file (instance->current_view_filename, doc) == -1)
		g_warning ("Unable to save view to %s - %s", instance->current_view_filename, g_strerror (errno));
	xmlFreeDoc (doc);
}

static void
view_changed (GalView *view,
              GalViewInstance *instance)
{
	if (instance->current_id != NULL) {
		g_free (instance->current_id);
		instance->current_id = NULL;
		save_current_view (instance);
		gal_view_instance_changed (instance);
	}

	gal_view_save (view, instance->custom_filename);
}

static void
disconnect_view (GalViewInstance *instance)
{
	if (instance->current_view) {
		if (instance->view_changed_id) {
			g_signal_handler_disconnect (
				instance->current_view,
				instance->view_changed_id);
		}

		g_object_unref (instance->current_view);
	}
	g_free (instance->current_type);
	g_free (instance->current_title);
	instance->current_title = NULL;
	instance->current_type = NULL;
	instance->view_changed_id = 0;
	instance->current_view = NULL;
}

static void
connect_view (GalViewInstance *instance,
              GalView *view)
{
	GalViewClass *view_class;

	if (instance->current_view)
		disconnect_view (instance);
	instance->current_view = view;

	view_class = GAL_VIEW_GET_CLASS (view);

	instance->current_title = g_strdup (gal_view_get_title (view));
	instance->current_type = g_strdup (view_class->type_code);
	instance->view_changed_id = g_signal_connect (
		instance->current_view, "changed",
		G_CALLBACK (view_changed), instance);

	gal_view_instance_display_view (instance, instance->current_view);
}

static void
gal_view_instance_dispose (GObject *object)
{
	GalViewInstance *instance = GAL_VIEW_INSTANCE (object);

	if (instance->collection) {
		if (instance->collection_changed_id) {
			g_signal_handler_disconnect (
				instance->collection,
				instance->collection_changed_id);
		}
		g_object_unref (instance->collection);
	}

	g_free (instance->instance_id);
	g_free (instance->custom_filename);
	g_free (instance->current_view_filename);

	g_free (instance->current_id);
	disconnect_view (instance);

	g_free (instance->default_view);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (gal_view_instance_parent_class)->dispose (object);
}

static void
gal_view_instance_class_init (GalViewInstanceClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = gal_view_instance_dispose;

	gal_view_instance_signals[DISPLAY_VIEW] = g_signal_new (
		"display_view",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GalViewInstanceClass, display_view),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		GAL_TYPE_VIEW);

	gal_view_instance_signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GalViewInstanceClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	gal_view_instance_signals[LOADED] = g_signal_new (
		"loaded",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GalViewInstanceClass, loaded),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	class->display_view = NULL;
	class->changed = NULL;
}

static void
gal_view_instance_init (GalViewInstance *instance)
{
	instance->collection = NULL;

	instance->instance_id = NULL;
	instance->custom_filename = NULL;
	instance->current_view_filename = NULL;

	instance->current_title = NULL;
	instance->current_type = NULL;
	instance->current_id = NULL;
	instance->current_view = NULL;

	instance->view_changed_id = 0;
	instance->collection_changed_id = 0;

	instance->loaded = FALSE;
	instance->default_view = NULL;
}

static void
collection_changed (GalView *view,
                    GalViewInstance *instance)
{
	if (instance->current_id) {
		gchar *view_id = instance->current_id;
		instance->current_id = NULL;
		gal_view_instance_set_current_view_id (instance, view_id);
		g_free (view_id);
	}
}

static void
load_current_view (GalViewInstance *instance)
{
	xmlDoc *doc = NULL;
	xmlNode *root;
	GalView *view = NULL;

	if (g_file_test (instance->current_view_filename, G_FILE_TEST_IS_REGULAR)) {
#ifdef G_OS_WIN32
		gchar *locale_filename = g_win32_locale_filename_from_utf8 (instance->current_view_filename);
		if (locale_filename != NULL)
			doc = xmlParseFile (locale_filename);
		g_free (locale_filename);
#else
		doc = xmlParseFile (instance->current_view_filename);
#endif
	}

	if (doc == NULL) {
		gchar *view_id = g_strdup (gal_view_instance_get_default_view (instance));
		g_free (instance->current_id);
		instance->current_id = view_id;

		if (instance->current_id) {
			gint index = gal_view_collection_get_view_index_by_id (
				instance->collection,
				instance->current_id);

			if (index != -1) {
				view = gal_view_collection_get_view (
					instance->collection, index);
				view = gal_view_clone (view);
				connect_view (instance, view);
			}
		}
		return;
	}

	root = xmlDocGetRootElement (doc);
	g_free (instance->current_id);
	instance->current_id = e_xml_get_string_prop_by_name_with_default (root, (const guchar *)"current_view", NULL);

	if (instance->current_id != NULL) {
		gint index = gal_view_collection_get_view_index_by_id (
			instance->collection,
			instance->current_id);

		if (index != -1) {
			view = gal_view_collection_get_view (
				instance->collection, index);
			view = gal_view_clone (view);
		}
	}
	if (view == NULL) {
		gchar *type;
		type = e_xml_get_string_prop_by_name_with_default (root, (const guchar *)"current_view_type", NULL);
		view = gal_view_collection_load_view_from_file (
			instance->collection, type,
			instance->custom_filename);
		g_free (type);
	}

	if (view == NULL) {
		/* If everything fails, maybe due to broken setup, default to the first view in the collection. */
		view = gal_view_collection_get_view (instance->collection, 0);
		view = gal_view_clone (view);
	}

	connect_view (instance, view);

	xmlFreeDoc (doc);
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
gal_view_instance_new (GalViewCollection *collection,
                       const gchar *instance_id)
{
	GalViewInstance *instance = g_object_new (GAL_TYPE_VIEW_INSTANCE, NULL);
	if (gal_view_instance_construct (instance, collection, instance_id))
		return instance;
	else {
		g_object_unref (instance);
		return NULL;
	}
}

GalViewInstance *
gal_view_instance_construct (GalViewInstance *instance,
                             GalViewCollection *collection,
                             const gchar *instance_id)
{
	gchar *filename;
	gchar *safe_id;
	const gchar *user_directory;

	instance->collection = collection;
	if (collection)
		g_object_ref (collection);
	instance->collection_changed_id = g_signal_connect (
		collection, "changed",
		G_CALLBACK (collection_changed), instance);

	if (instance_id)
		instance->instance_id = g_strdup (instance_id);
	else
		instance->instance_id = g_strdup ("");

	safe_id = g_strdup (instance->instance_id);
	e_util_make_safe_filename (safe_id);

	user_directory =
		gal_view_collection_get_user_directory (instance->collection);

	filename = g_strdup_printf ("custom_view-%s.xml", safe_id);
	instance->custom_filename =
		g_build_filename (user_directory, filename, NULL);
	g_free (filename);

	filename = g_strdup_printf ("current_view-%s.xml", safe_id);
	instance->current_view_filename =
		g_build_filename (user_directory, filename, NULL);
	g_free (filename);

	g_free (safe_id);

	return instance;
}

/* Manipulate the current view. */
gchar *
gal_view_instance_get_current_view_id (GalViewInstance *instance)
{
	if (instance->current_id && gal_view_collection_get_view_index_by_id (instance->collection, instance->current_id) != -1)
		return g_strdup (instance->current_id);
	else
		return NULL;
}

void
gal_view_instance_set_current_view_id (GalViewInstance *instance,
                                       const gchar *view_id)
{
	GalView *view;
	gint index;

	g_return_if_fail (instance != NULL);
	g_return_if_fail (GAL_IS_VIEW_INSTANCE (instance));

	d (g_print ("%s: view_id set to %s\n", G_STRFUNC, view_id));

	if (instance->current_id && !strcmp (instance->current_id, view_id))
		return;

	g_free (instance->current_id);
	instance->current_id = g_strdup (view_id);

	index = gal_view_collection_get_view_index_by_id (instance->collection, view_id);
	if (index != -1) {
		view = gal_view_collection_get_view (instance->collection, index);
		connect_view (instance, gal_view_clone (view));
	}

	if (instance->loaded)
		save_current_view (instance);
	gal_view_instance_changed (instance);
}

GalView *
gal_view_instance_get_current_view (GalViewInstance *instance)
{
	return instance->current_view;
}

void
gal_view_instance_set_custom_view (GalViewInstance *instance,
                                   GalView *view)
{
	g_free (instance->current_id);
	instance->current_id = NULL;

	view = gal_view_clone (view);
	connect_view (instance, view);
	gal_view_save (view, instance->custom_filename);
	save_current_view (instance);
	gal_view_instance_changed (instance);
}

static void
dialog_response (GtkWidget *dialog,
                 gint id,
                 GalViewInstance *instance)
{
	if (id == GTK_RESPONSE_OK) {
		gal_view_instance_save_as_dialog_save (GAL_VIEW_INSTANCE_SAVE_AS_DIALOG (dialog));
	}
	gtk_widget_destroy (dialog);
}

void
gal_view_instance_save_as (GalViewInstance *instance)
{
	GtkWidget *dialog;

	g_return_if_fail (instance != NULL);

	dialog = gal_view_instance_save_as_dialog_new (instance);
	g_signal_connect (
		dialog, "response",
		G_CALLBACK (dialog_response), instance);
	gtk_widget_show (dialog);
}

/* This is idempotent.  Once it's been called once, the rest of the calls are ignored. */
void
gal_view_instance_load (GalViewInstance *instance)
{
	if (!instance->loaded) {
		load_current_view (instance);
		instance->loaded = TRUE;
		g_signal_emit (instance, gal_view_instance_signals[LOADED], 0);
	}
}

/* These only mean anything before gal_view_instance_load is called the first time.  */
const gchar *
gal_view_instance_get_default_view (GalViewInstance *instance)
{
	if (instance->default_view)
		return instance->default_view;
	else
		return gal_view_collection_get_default_view (instance->collection);
}

void
gal_view_instance_set_default_view (GalViewInstance *instance,
                                    const gchar *id)
{
	g_free (instance->default_view);
	instance->default_view = g_strdup (id);
}

gboolean
gal_view_instance_exists (GalViewInstance *instance)
{
	struct stat st;

	if (instance->current_view_filename && g_stat (instance->current_view_filename, &st) == 0 && st.st_size > 0 && S_ISREG (st.st_mode))
		return TRUE;
	else
		return FALSE;

}
