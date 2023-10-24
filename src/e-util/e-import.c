/*
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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#ifdef HAVE_IMPORT_H
#include <import.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "e-import.h"

#include <glib/gi18n.h>

#define d(x)

typedef struct _EImportImporters EImportImporters;

struct _EImportImporters {
	EImportImporter *importer;
	EImportImporterFunc free;
	gpointer data;
};

typedef struct _EImportPrivate {
	gboolean widget_complete;
} EImportPrivate;

enum {
	PROP_0,
	PROP_WIDGET_COMPLETE
};

G_DEFINE_TYPE_WITH_PRIVATE (EImport, e_import, G_TYPE_OBJECT)

static void
import_finalize (GObject *object)
{
	EImport *import = E_IMPORT (object);

	g_free (import->id);

	/* Chain up to parent's finalize () method. */
	G_OBJECT_CLASS (e_import_parent_class)->finalize (object);
}

static void
import_set_property (GObject *object,
		     guint property_id,
		     const GValue *value,
		     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WIDGET_COMPLETE:
			e_import_set_widget_complete (E_IMPORT (object), g_value_get_boolean (value));
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
import_get_property (GObject *object,
		     guint property_id,
		     GValue *value,
		     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_WIDGET_COMPLETE:
			g_value_set_boolean (value, e_import_get_widget_complete (E_IMPORT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
import_target_free (EImport *import,
                    EImportTarget *target)
{
	switch (target->type) {
	case E_IMPORT_TARGET_URI: {
		EImportTargetURI *s = (EImportTargetURI *) target;

		g_free (s->uri_src);
		g_free (s->uri_dest);
		break; }
	default:
		break;
	}

	g_datalist_clear (&target->data);
	g_free (target);
	g_object_unref (import);
}

static void
e_import_class_init (EImportClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = import_finalize;
	object_class->set_property = import_set_property;
	object_class->get_property = import_get_property;

	class->target_free = import_target_free;

	g_object_class_install_property (
		object_class,
		PROP_WIDGET_COMPLETE,
		g_param_spec_boolean (
			"widget-complete",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));
}

static void
e_import_init (EImport *import)
{
	EImportPrivate *priv = e_import_get_instance_private (import);

	priv->widget_complete = TRUE;
}

gboolean
e_import_get_widget_complete (EImport *import)
{
	EImportPrivate *priv;

	g_return_val_if_fail (E_IS_IMPORT (import), FALSE);

	priv = e_import_get_instance_private (import);

	return priv->widget_complete;
}

void
e_import_set_widget_complete (EImport *import,
			      gboolean value)
{
	EImportPrivate *priv;

	g_return_if_fail (E_IS_IMPORT (import));

	priv = e_import_get_instance_private (import);

	if ((priv->widget_complete ? 1 : 0) != (value ? 1 : 0)) {
		priv->widget_complete = value;

		g_object_notify (G_OBJECT (import), "widget-complete");
	}
}

/**
 * e_import_construct:
 * @import: The instance to initialise.
 * @id: The name of the instance.
 *
 * Used by implementing classes to initialise base parameters.
 *
 * Return value: @ep is returned.
 **/
EImport *
e_import_construct (EImport *import,
                    const gchar *id)
{
	import->id = g_strdup (id);

	return import;
}

EImport *
e_import_new (const gchar *id)
{
	EImport *import;

	import = g_object_new (E_TYPE_IMPORT, NULL);

	return e_import_construct (import, id);
}

/**
 * e_import_import:
 * @import: an #EImport
 * @target: Target to import.
 * @importer: Importer to use.
 * @status: Status callback, called with progress information.
 * @done: Complete callback, will always be called once complete.
 * @data: user data for callback functions
 *
 * Run the import function of the selected importer.  Once the
 * importer has finished, it MUST call the e_import_complete ()
 * function.  This allows importers to run in synchronous or
 * asynchronous mode.
 *
 * When complete, the @done callback will be called.
 **/
void
e_import_import (EImport *import,
                 EImportTarget *target,
                 EImportImporter *importer,
                 EImportStatusFunc status,
                 EImportCompleteFunc done,
                 gpointer data)
{
	g_return_if_fail (importer != NULL);

	import->status = status;
	import->done = done;
	import->done_data = data;

	importer->import (import, target, importer);
}

void
e_import_cancel (EImport *import,
                 EImportTarget *t,
                 EImportImporter *im)
{
	if (im->cancel)
		im->cancel (import, t, im);
}

/**
 * e_import_get_widget:
 * @import: an #EImport
 * @target: Target of interest
 * @importer: Importer to get widget of
 *
 * Gets a widget that the importer uses to configure its
 * destination.  This widget should be packed into a container
 * widget.  It should not be shown_all.
 *
 * Return value: NULL if the importer doesn't support/require
 * a destination.
 **/
GtkWidget *
e_import_get_widget (EImport *import,
                     EImportTarget *target,
                     EImportImporter *importer)
{
	g_return_val_if_fail (importer != NULL, NULL);
	g_return_val_if_fail (target != NULL, NULL);

	return importer->get_widget (import, target, importer);
}

/**
 * e_import_get_preview_widget:
 * @import: an #EImport
 * @target: Target of interest
 * @im: Importer to get a preview widget of
 *
 * Gets a widget that the importer uses to preview data to be
 * imported.  This widget should be packed into a container
 * widget.  It should not be shown_all.
 *
 * Return value: NULL if the importer doesn't support preview.
 **/
GtkWidget *
e_import_get_preview_widget (EImport *import,
                     EImportTarget *target,
                     EImportImporter *im)
{
	g_return_val_if_fail (im != NULL, NULL);
	g_return_val_if_fail (target != NULL, NULL);

	if (!im->get_preview)
		return NULL;

	return im->get_preview (import, target, im);
}

/**
 * e_import_complete:
 * @import: an #EImport
 * @target: Target just completed (unused currently)
 * @error: a #GError for the operation, %NULL when succeeded
 *
 * Signify that an import is complete.  This must be called by
 * importer implementations when they are done.
 **/
void
e_import_complete (EImport *import,
                   EImportTarget *target,
		   const GError *error)
{
	if (import->done)
		import->done (import, error, import->done_data);
}

void
e_import_status (EImport *import,
                 EImportTarget *target,
                 const gchar *what,
                 gint pc)
{
	if (import->status)
		import->status (import, what, pc, import->done_data);
}

/**
 * e_import_get_importers:
 * @import: an #EImport
 * @target: an #EImportTarget
 *
 * Get a list of importers.  If @target is supplied, then only
 * importers which support the type and location specified by the
 * target are listed.  If @target is NULL, then all importers are
 * listed.
 *
 * Return value: A list of importers.  The list should be freed when
 * no longer needed.
 **/
GSList *
e_import_get_importers (EImport *import,
                        EImportTarget *target)
{
	GSList *importers = NULL;
	GList *link;

	link = E_IMPORT_GET_CLASS (import)->importers;

	while (link != NULL) {
		EImportImporters *ei = link->data;

		if (target == NULL
		    || (ei->importer->type == target->type
			&& ei->importer->supported (import, target, ei->importer))) {
			importers = g_slist_append (importers, ei->importer);
		}

		link = g_list_next (link);
	}

	return importers;
}

/* ********************************************************************** */

static gint
importer_compare (EImportImporters *node_a,
                  EImportImporters *node_b)
{
	gint pri_a = node_a->importer->pri;
	gint pri_b = node_b->importer->pri;

	return (pri_a == pri_b) ? 0 : (pri_a < pri_b) ? -1 : 1;
}

/**
 * e_import_class_add_importer:
 * @klass: An initialised implementing instance of EImport.
 * @importer: Importer to add.
 * @freefunc: If supplied, called to free the importer node
 * when it is no longer needed.
 * @data: Data for the callback.
 *
 **/
void
e_import_class_add_importer (EImportClass *klass,
                             EImportImporter *importer,
                             EImportImporterFunc freefunc,
                             gpointer data)
{
	EImportImporters *node;

	node = g_malloc (sizeof (*node));
	node->importer = importer;
	node->free = freefunc;
	node->data = data;

	klass->importers = g_list_sort (
		g_list_prepend (klass->importers, node),
		(GCompareFunc) importer_compare);
}

/**
 * e_import_target_new:
 * @import: an #EImport
 * @type: type, up to implementor
 * @size: Size of object to allocate.
 *
 * Allocate a new import target suitable for this class.  Implementing
 * classes will define the actual content of the target.
 **/
gpointer
e_import_target_new (EImport *import,
                     gint type,
                     gsize size)
{
	EImportTarget *target;

	if (size < sizeof (EImportTarget)) {
		g_warning ("Size less than size of EImportTarget\n");
		size = sizeof (EImportTarget);
	}

	target = g_malloc0 (size);
	target->import = g_object_ref (import);
	target->type = type;

	g_datalist_init (&target->data);

	return target;
}

/**
 * e_import_target_free:
 * @import: an #EImport
 * @target: the target to free
 *
 * Free a target.  The implementing class can override this method to
 * free custom targets.
 **/
void
e_import_target_free (EImport *import,
                      gpointer target)
{
	E_IMPORT_GET_CLASS (import)->target_free (
		import, (EImportTarget *) target);
}

EImportTargetURI *
e_import_target_new_uri (EImport *import,
                         const gchar *uri_src,
                         const gchar *uri_dst)
{
	EImportTargetURI *t;

	t = e_import_target_new (import, E_IMPORT_TARGET_URI, sizeof (*t));
	t->uri_src = g_strdup (uri_src);
	t->uri_dest = g_strdup (uri_dst);

	return t;
}

EImportTargetHome *
e_import_target_new_home (EImport *import)
{
	return e_import_target_new (
		import, E_IMPORT_TARGET_HOME, sizeof (EImportTargetHome));
}

/* it can end in the middle of the data structure or a Unicode letter */
static gboolean
import_util_read_file_contents_with_limit (const gchar *filename,
					   gsize size_limit,
					   gchar **out_content,
					   gsize *out_length,
					   GError **error)
{
	GByteArray *bytes;
	GFile *file;
	GFileInputStream *stream;
	GInputStream *istream;
	guint8 buff[10240];
	gsize to_read;

	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (out_content != NULL, FALSE);

	file = g_file_new_for_path (filename);
	stream = g_file_read (file, NULL, error);

	if (!stream) {
		g_clear_object (&file);
		return FALSE;
	}

	istream = G_INPUT_STREAM (stream);
	to_read = size_limit ? MIN (size_limit, sizeof (buff)) : sizeof (buff);
	bytes = g_byte_array_new ();

	while (!size_limit || bytes->len < size_limit) {
		gsize did_read = 0;

		if (!g_input_stream_read_all (istream, buff, to_read, &did_read, NULL, error)) {
			g_byte_array_free (bytes, TRUE);
			g_clear_object (&stream);
			g_clear_object (&file);

			return FALSE;
		}

		if (!did_read)
			break;

		g_byte_array_append (bytes, buff, did_read);
	}

	/* zero-terminate the array, but do not count the NUL byte into the `out_length` */
	buff[0] = 0;
	g_byte_array_append (bytes, buff, 1);

	if (out_length)
		*out_length = bytes->len - 1;
	*out_content = (gchar *) g_byte_array_free (bytes, FALSE);

	g_clear_object (&stream);
	g_clear_object (&file);

	return TRUE;
}

/**
 * e_import_util_get_file_contents:
 * @filename: a local file name to read the contents from
 * @size_limit: up to how many bytes to read, 0 for the whole file
 * @error: (nullable): a return location for a #GError, or %NULL
 *
 * Reads the @filename content and returns it in a single-byte encoding.
 * The content can be cut around @size_limit bytes.
 *
 * Returns: (transfer full) (nullable): the file content, or %NULL on error,
 *    in which case the @error is set.
 *
 * Since: 3.42
 **/
gchar *
e_import_util_get_file_contents (const gchar *filename,
				 gsize size_limit,
				 GError **error)
{
	gchar *raw_content = NULL;
	gsize length = 0;
	gunichar2 *utf16;
	gboolean is_utf16, is_utf16_swapped;
	gchar *res = NULL;

	g_return_val_if_fail (filename != NULL, NULL);

	if (!import_util_read_file_contents_with_limit (filename, size_limit, &raw_content, &length, error))
		return NULL;

	if (length < 2)
		return raw_content;

	utf16 = (gunichar2 *) raw_content;

	/* check the UTF-16 BOM */
	is_utf16 = *utf16 == ((gunichar2) 0xFEFF);
	is_utf16_swapped = *utf16 == ((gunichar2) 0xFFFE);

	if (length > 4 && !is_utf16 && !is_utf16_swapped) {
		/* Only guess it can be UTF-16 without the leading BOM, which can fail
		   when the first two characters are encoded into multiple bytes... */
		is_utf16 = utf16[0] && !(utf16[0] & 0xFF00) && utf16[1] && !(utf16[1] & 0xFF00);
		is_utf16_swapped = utf16[0] && !(utf16[0] & 0xFF) && utf16[1] && !(utf16[1] & 0xFF);
	}

	if (is_utf16 || is_utf16_swapped) {
		glong len = length / 2;

		/* Swap the bytes, to match the local endianness */
		if (is_utf16_swapped) {
			gunichar2 *pos_str;
			gsize npos;

			for (npos = 0, pos_str = utf16; npos < len; npos++, pos_str++) {
				*pos_str = GUINT16_SWAP_LE_BE (*pos_str);
			}
		}

		if (*utf16 == ((gunichar2) 0xFEFF)) {
			utf16++;
			len--;
		}

		res = g_utf16_to_utf8 (utf16, len, NULL, NULL, NULL);

		if (res) {
			g_free (raw_content);
			return res;
		}

		/* Return back any changes */
		if (len != length / 2) {
			utf16--;
			len++;
		}

		if (is_utf16_swapped) {
			gunichar2 *pos_str;
			gsize npos;

			for (npos = 0, pos_str = utf16; npos < len; npos++, pos_str++) {
				*pos_str = GUINT16_SWAP_LE_BE (*pos_str);
			}
		}
	}

	if (g_utf8_validate (raw_content, -1, NULL))
		return raw_content;

	res = g_locale_to_utf8 (raw_content, length, NULL, NULL, NULL);

	if (res)
		g_free (raw_content);
	else
		res = raw_content;

	return res;
}

/* ********************************************************************** */

/* Import menu plugin handler */

/*
 * <e-plugin
 *   class="org.gnome.mail.plugin.import:1.0"
 *   id="org.gnome.mail.plugin.import.item:1.0"
 *   type="shlib"
 *   location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
 *   name="imap"
 *   description="IMAP4 and IMAP4v1 mail store">
 *   <hook class="org.gnome.mail.importMenu:1.0"
 *         handler="HandleImport">
 *   <menu id="any" target="select">
 *    <item
 *     type="item|toggle|radio|image|submenu|bar"
 *     active
 *     path="foo/bar"
 *     label="label"
 *     icon="foo"
 *     activate="ep_view_emacs"/>
 *   </menu>
 * </e-plugin>
 */

#define emph ((EImportHook *)eph)

static const EImportHookTargetMask eih_no_masks[] = {
	{ NULL }
};

static const EImportHookTargetMap eih_targets[] = {
	{ "uri", E_IMPORT_TARGET_URI, eih_no_masks },
	{ "home", E_IMPORT_TARGET_HOME, eih_no_masks },
	{ NULL }
};

G_DEFINE_TYPE (
	EImportHook,
	e_import_hook,
	E_TYPE_PLUGIN_HOOK)

static gboolean
eih_supported (EImport *ei,
               EImportTarget *target,
               EImportImporter *im)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *) im;
	EImportHook *hook = im->user_data;

	return e_plugin_invoke (hook->hook.plugin, ihook->supported, target) != NULL;
}

static GtkWidget *
eih_get_widget (EImport *ei,
                EImportTarget *target,
                EImportImporter *im)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *) im;
	EImportHook *hook = im->user_data;

	return e_plugin_invoke (hook->hook.plugin, ihook->get_widget, target);
}

static void
eih_import (EImport *ei,
            EImportTarget *target,
            EImportImporter *im)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *) im;
	EImportHook *hook = im->user_data;

	e_plugin_invoke (hook->hook.plugin, ihook->import, target);
}

static void
eih_cancel (EImport *ei,
            EImportTarget *target,
            EImportImporter *im)
{
	struct _EImportHookImporter *ihook = (EImportHookImporter *) im;
	EImportHook *hook = im->user_data;

	e_plugin_invoke (hook->hook.plugin, ihook->cancel, target);
}

static void
eih_free_importer (EImportImporter *im,
                   gpointer data)
{
	EImportHookImporter *ihook = (EImportHookImporter *) im;

	g_free (ihook->supported);
	g_free (ihook->get_widget);
	g_free (ihook->import);
	g_free (ihook);
}

static struct _EImportHookImporter *
emph_construct_importer (EPluginHook *eph,
                         xmlNodePtr root)
{
	struct _EImportHookImporter *item;
	EImportHookTargetMap *map;
	EImportHookClass *class = (EImportHookClass *) G_OBJECT_GET_CLASS (eph);
	gchar *tmp;

	d (printf ("  loading import item\n"));
	item = g_malloc0 (sizeof (*item));

	tmp = (gchar *) xmlGetProp (root, (const guchar *)"target");
	if (tmp == NULL)
		goto error;
	map = g_hash_table_lookup (class->target_map, tmp);
	xmlFree (tmp);
	if (map == NULL)
		goto error;

	item->importer.type = map->id;
	item->supported = e_plugin_xml_prop (root, "supported");
	item->get_widget = e_plugin_xml_prop (root, "get-widget");
	item->import = e_plugin_xml_prop (root, "import");
	item->cancel = e_plugin_xml_prop (root, "cancel");

	item->importer.name = e_plugin_xml_prop (root, "name");
	item->importer.description = e_plugin_xml_prop (root, "description");

	item->importer.user_data = eph;

	if (item->import == NULL || item->supported == NULL)
		goto error;

	item->importer.supported = eih_supported;
	item->importer.import = eih_import;
	if (item->get_widget)
		item->importer.get_widget = eih_get_widget;
	if (item->cancel)
		item->importer.cancel = eih_cancel;

	return item;
error:
	d (printf ("error!\n"));
	eih_free_importer ((EImportImporter *) item, NULL);
	return NULL;
}

static gint
emph_construct (EPluginHook *eph,
                EPlugin *ep,
                xmlNodePtr root)
{
	xmlNodePtr node;
	EImportClass *class;

	d (printf ("loading import hook\n"));

	if (E_PLUGIN_HOOK_CLASS (e_import_hook_parent_class)->
		construct (eph, ep, root) == -1)
		return -1;

	class = E_IMPORT_HOOK_GET_CLASS (eph)->import_class;

	node = root->children;
	while (node) {
		if (strcmp ((gchar *) node->name, "importer") == 0) {
			struct _EImportHookImporter *ihook;

			ihook = emph_construct_importer (eph, node);
			if (ihook) {
				e_import_class_add_importer (
					class, &ihook->importer,
					eih_free_importer, eph);
				emph->importers = g_slist_append (
					emph->importers, ihook);
			}
		}
		node = node->next;
	}

	eph->plugin = ep;

	return 0;
}

static void
e_import_hook_class_init (EImportHookClass *class)
{
	EPluginHookClass *plugin_hook_class;
	gint ii;

	plugin_hook_class = E_PLUGIN_HOOK_CLASS (class);
	plugin_hook_class->id = "org.gnome.evolution.import:1.0";
	plugin_hook_class->construct = emph_construct;

	/** @HookClass: Evolution Importers
	 * @Id: org.gnome.evolution.import:1.0
	 * @Target: EImportTarget
	 *
	 * A hook for data importers.
	 **/

	class->target_map = g_hash_table_new (g_str_hash, g_str_equal);
	class->import_class = g_type_class_ref (E_TYPE_IMPORT);

	for (ii = 0; eih_targets[ii].type; ii++)
		e_import_hook_class_add_target_map (class, &eih_targets[ii]);
}

static void
e_import_hook_init (EImportHook *hook)
{
}

/**
 * e_import_hook_class_add_target_map:
 *
 * @class: The dervied EimportHook class.
 * @map: A map used to describe a single EImportTarget type for this
 * class.
 *
 * Add a targe tmap to a concrete derived class of EImport.  The
 * target map enumates the target types available for the implenting
 * class.
 **/
void
e_import_hook_class_add_target_map (EImportHookClass *class,
                                    const EImportHookTargetMap *map)
{
	g_hash_table_insert (
		class->target_map, (gpointer) map->type, (gpointer) map);
}
