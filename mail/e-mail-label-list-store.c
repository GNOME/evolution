/*
 * e-mail-label-list-store.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-label-list-store.h"

#include <glib/gi18n.h>
#include <camel/camel.h>
#include <e-util/e-util.h>

#define E_MAIL_LABEL_LIST_STORE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_LABEL_LIST_STORE, EMailLabelListStorePrivate))

#define E_MAIL_LABEL_LIST_STORE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_LABEL_LIST_STORE, EMailLabelListStorePrivate))

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _EMailLabelListStorePrivate {
	GHashTable *tag_index; /* gchar *tag_name ~> GtkTreeIter * */
	GSettings *mail_settings;
	guint idle_changed_id;
};

static struct {
	const gchar *label_name;
	const gchar *label_color;
	const gchar *label_tag;
} label_defaults[] = {
	{ N_("I_mportant"), "#EF2929", "$Labelimportant" },	/* red */
	{ N_("_Work"),      "#F57900", "$Labelwork" },		/* orange */
	{ N_("_Personal"),  "#4E9A06", "$Labelpersonal" },	/* green */
	{ N_("_To Do"),     "#3465A4", "$Labeltodo" },		/* blue */
	{ N_("_Later"),     "#75507B", "$Labellater" }		/* purple */
};

/* Forward Declarations */
static void	e_mail_label_list_store_interface_init
						(GtkTreeModelIface *iface);
static void	labels_settings_changed_cb	(GSettings *settings,
						 const gchar *key,
						 gpointer user_data);

G_DEFINE_TYPE_WITH_CODE (
	EMailLabelListStore,
	e_mail_label_list_store,
	GTK_TYPE_LIST_STORE,
	G_IMPLEMENT_INTERFACE (
		GTK_TYPE_TREE_MODEL,
		e_mail_label_list_store_interface_init))

static gchar *
mail_label_list_store_tag_from_name (const gchar *label_name)
{
	gchar *label_tag;
	gchar *temp;

	/* Thunderbird compatible */
	temp = g_ascii_strdown (label_name, -1);
	g_strdelimit (temp, " ()/{%*<>\\\"", '_');
	label_tag = camel_utf8_utf7 (temp);
	g_free (temp);

	return label_tag;
}

static gchar *
mail_label_list_store_encode_label (const gchar *label_name,
                                      const gchar *label_color,
                                      const gchar *label_tag)
{
	GString *string;

	/* Encoded Form: <name> ':' <color> [ '|' <tag> ] */

	string = g_string_new (label_name);
	g_string_append_printf (string, ":%s", label_color);

	if (label_tag != NULL)
		g_string_append_printf (string, "|%s", label_tag);

	return g_string_free (string, FALSE);
}

static void
mail_label_list_store_fill_tag_index (EMailLabelListStore *store)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_hash_table_remove_all (store->priv->tag_index);

	model = GTK_TREE_MODEL (store);
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		gchar *tag;

		tag = e_mail_label_list_store_get_tag (store, &iter);
		if (!tag)
			continue;

		g_hash_table_insert (store->priv->tag_index, tag, gtk_tree_iter_copy (&iter));
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
mail_label_list_store_ensure_defaults (EMailLabelListStore *store)
{
	gint ii;

	for (ii = 0; ii < G_N_ELEMENTS (label_defaults); ii++) {
		GtkTreeIter iter;
		const gchar *label_name;
		const gchar *label_color;
		const gchar *label_tag;
		gchar *encoded;

		label_name = gettext (label_defaults[ii].label_name);
		label_color = label_defaults[ii].label_color;
		label_tag = label_defaults[ii].label_tag;

		if (e_mail_label_list_store_lookup (store, label_tag, &iter))
			continue;

		encoded = mail_label_list_store_encode_label (
			label_name, label_color, label_tag);

		gtk_list_store_insert_with_values (
			GTK_LIST_STORE (store),
			NULL, -1, 0, encoded, -1);

		g_free (encoded);
	}
}

static gchar *
mail_label_list_store_get_stock_id (EMailLabelListStore *store,
                                    const gchar *color_spec)
{
	EMailLabelListStoreClass *class;
	GtkIconFactory *icon_factory;
	GdkColor color;
	gchar *stock_id;

	class = E_MAIL_LABEL_LIST_STORE_GET_CLASS (store);
	icon_factory = class->icon_factory;

	if (!gdk_color_parse (color_spec, &color))
		return NULL;

	stock_id = g_strdup_printf ("evolution-label-%s", color_spec);

	/* Themes need not be taken into account here.
	 * It's just a solid block of a user-chosen color. */
	if (gtk_icon_factory_lookup (icon_factory, stock_id) == NULL) {
		GtkIconSet *icon_set;
		GdkPixbuf *pixbuf;
		guint32 pixel;

		pixel = ((color.red & 0xFF00) << 16) +
			((color.green & 0xFF00) << 8) +
			(color.blue & 0xFF00);

		pixbuf = gdk_pixbuf_new (
			GDK_COLORSPACE_RGB, FALSE, 8, 16, 16);
		gdk_pixbuf_fill (pixbuf, pixel);

		icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
		gtk_icon_factory_add (icon_factory, stock_id, icon_set);
		gtk_icon_set_unref (icon_set);

		g_object_unref (pixbuf);
	}

	return stock_id;
}

static void
mail_label_list_store_dispose (GObject *object)
{
	EMailLabelListStorePrivate *priv;

	priv = E_MAIL_LABEL_LIST_STORE_GET_PRIVATE (object);

	if (priv->idle_changed_id) {
		g_source_remove (priv->idle_changed_id);
		priv->idle_changed_id = 0;
	}

	if (priv->mail_settings != NULL) {
		g_object_unref (priv->mail_settings);
		priv->mail_settings = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_label_list_store_parent_class)->
		dispose (object);
}

static void
mail_label_list_store_finalize (GObject *object)
{
	EMailLabelListStorePrivate *priv;

	priv = E_MAIL_LABEL_LIST_STORE_GET_PRIVATE (object);

	g_hash_table_destroy (priv->tag_index);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_label_list_store_parent_class)->
		finalize (object);
}

static gboolean
labels_model_changed_idle_cb (gpointer user_data)
{
	EMailLabelListStore *store = user_data;
	GPtrArray *array;
	GtkTreeIter tmp_iter;
	gboolean iter_set;

	g_return_val_if_fail (E_IS_MAIL_LABEL_LIST_STORE (store), FALSE);

	store->priv->idle_changed_id = 0;

	/* Make sure we don't enter an infinite synchronizing loop */
	g_signal_handlers_block_by_func (
		store->priv->mail_settings,
		labels_settings_changed_cb, store);

	/* Build list to store in GSettings */

	array = g_ptr_array_new ();

	iter_set = gtk_tree_model_get_iter_first (
		GTK_TREE_MODEL (store), &tmp_iter);

	while (iter_set) {
		gchar *string;

		gtk_tree_model_get (
			GTK_TREE_MODEL (store), &tmp_iter,
			0, &string, -1);
		g_ptr_array_add (array, string);

		iter_set = gtk_tree_model_iter_next (
			GTK_TREE_MODEL (store), &tmp_iter);
	}

	g_ptr_array_add (array, NULL);

	g_settings_set_strv (
		store->priv->mail_settings, "labels",
		(const gchar * const *) array->pdata);

	g_ptr_array_foreach (array, (GFunc) g_free, NULL);
	g_ptr_array_free (array, TRUE);

	g_signal_handlers_unblock_by_func (
		store->priv->mail_settings,
		labels_settings_changed_cb, store);

	mail_label_list_store_fill_tag_index (store);

	g_signal_emit (store, signals[CHANGED], 0);

	return FALSE;
}

static void
labels_model_changed_cb (EMailLabelListStore *store)
{
	g_return_if_fail (E_IS_MAIL_LABEL_LIST_STORE (store));

	mail_label_list_store_fill_tag_index (store);

	/* do the actual save and signal emission on idle,
	 * to accumulate as many changes as possible */
	if (!store->priv->idle_changed_id)
		store->priv->idle_changed_id = g_idle_add (labels_model_changed_idle_cb, store);
}

static void
labels_settings_changed_cb (GSettings *settings,
                            const gchar *key,
                            gpointer user_data)
{
	EMailLabelListStore *store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GHashTable *changed_labels;
	gchar **strv;
	gint i;

	store = E_MAIL_LABEL_LIST_STORE (user_data);
	model = GTK_TREE_MODEL (store);

	strv = g_settings_get_strv (store->priv->mail_settings, "labels");

	/* Check if any label changed first, because GSettings can claim
	 * change when nothing changed at all */
	changed_labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gchar *label_str = NULL;

			gtk_tree_model_get (model, &iter, 0, &label_str, -1);

			if (label_str)
				g_hash_table_insert (changed_labels, label_str, NULL);
		} while (gtk_tree_model_iter_next (model, &iter));

		for (i = 0; strv[i] != NULL; i++) {
			if (!g_hash_table_remove (changed_labels, strv[i])) {
				g_hash_table_insert (changed_labels, g_strdup (""), NULL);
				break;
			}
		}
	} else {
		/* nothing in the store, thus fill it (pretend change) */
		g_hash_table_insert (changed_labels, g_strdup (""), NULL);
	}

	/* Nothing changed */
	if (g_hash_table_size (changed_labels) == 0) {
		g_hash_table_destroy (changed_labels);
		g_strfreev (strv);

		mail_label_list_store_fill_tag_index (store);
		return;
	}

	g_hash_table_destroy (changed_labels);

	/* Make sure we don't enter an infinite synchronizing loop */
	g_signal_handlers_block_by_func (
		store, labels_model_changed_cb, store);

	gtk_list_store_clear (GTK_LIST_STORE (store));

	for (i = 0; strv[i] != NULL; i++) {
		GtkTreeIter iter;

		gtk_list_store_insert_with_values (
			GTK_LIST_STORE (store), &iter, -1, 0, strv[i], -1);
	}

	g_strfreev (strv);

	g_signal_handlers_unblock_by_func (
		store, labels_model_changed_cb, store);

	mail_label_list_store_fill_tag_index (store);
}

static void
mail_label_list_store_constructed (GObject *object)
{
	EMailLabelListStore *store;

	store = E_MAIL_LABEL_LIST_STORE (object);

	/* Connect to GSettings' change notifications */
	store->priv->mail_settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_signal_connect (
		store->priv->mail_settings, "changed::labels",
		G_CALLBACK (labels_settings_changed_cb), store);
	labels_settings_changed_cb (
		store->priv->mail_settings, "labels", store);

	/* Connect to ListStore change notifications */
	g_signal_connect_swapped (
		store, "row-inserted",
		G_CALLBACK (labels_model_changed_cb), store);
	g_signal_connect_swapped (
		store, "row-changed",
		G_CALLBACK (labels_model_changed_cb), store);
	g_signal_connect_swapped (
		store, "row-deleted",
		G_CALLBACK (labels_model_changed_cb), store);
	g_signal_connect_swapped (
		store, "rows-reordered",
		G_CALLBACK (labels_model_changed_cb), store);

	mail_label_list_store_ensure_defaults (store);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_label_list_store_parent_class)->constructed (object);
}

static void
e_mail_label_list_store_class_init (EMailLabelListStoreClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailLabelListStorePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = mail_label_list_store_dispose;
	object_class->finalize = mail_label_list_store_finalize;
	object_class->constructed = mail_label_list_store_constructed;

	class->icon_factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (class->icon_factory);

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_mail_label_list_store_interface_init (GtkTreeModelIface *iface)
{
}

static void
e_mail_label_list_store_init (EMailLabelListStore *store)
{
	GHashTable *tag_index;
	GType type = G_TYPE_STRING;

	tag_index = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_iter_free);

	store->priv = E_MAIL_LABEL_LIST_STORE_GET_PRIVATE (store);
	store->priv->tag_index = tag_index;

	/* XXX While it may seem awkward to cram the label name and color
	 *     into a single string column, we do it for the benefit of
	 *     letting GSettings keep the model in sync.
	 *
	 * XXX There's a valid argument to be made that this information
	 *     doesn't belong in GSettings in the first place.  A key file
	 *     under $(user_data_dir)/mail would work better. */
	gtk_list_store_set_column_types (GTK_LIST_STORE (store), 1, &type);
}

EMailLabelListStore *
e_mail_label_list_store_new (void)
{
	return g_object_new (E_TYPE_MAIL_LABEL_LIST_STORE, NULL);
}

gchar *
e_mail_label_list_store_get_name (EMailLabelListStore *store,
                                  GtkTreeIter *iter)
{
	gchar *encoded;
	gchar *result;
	gchar **strv;

	/* Encoded Form: <name> ':' <color> [ '|' <tag> ] */

	g_return_val_if_fail (E_IS_MAIL_LABEL_LIST_STORE (store), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter, 0, &encoded, -1);

	strv = g_strsplit_set (encoded, ":|", 3);

	if (g_strv_length (strv) >= 2)
		result = g_strdup (gettext (strv[0]));
	else
		result = NULL;

	g_strfreev (strv);
	g_free (encoded);

	return result;
}

gboolean
e_mail_label_list_store_get_color (EMailLabelListStore *store,
                                   GtkTreeIter *iter,
                                   GdkColor *color)
{
	gchar *encoded;
	gchar **strv;
	gboolean valid;

	/* Encoded Form: <name> ':' <color> [ '|' <tag> ] */

	g_return_val_if_fail (E_IS_MAIL_LABEL_LIST_STORE (store), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (color != NULL, FALSE);

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter, 0, &encoded, -1);

	strv = g_strsplit_set (encoded, ":|", 3);

	if (g_strv_length (strv) >= 2)
		valid = gdk_color_parse (strv[1], color);
	else
		valid = FALSE;

	g_strfreev (strv);
	g_free (encoded);

	return valid;
}

gchar *
e_mail_label_list_store_get_stock_id (EMailLabelListStore *store,
                                      GtkTreeIter *iter)
{
	gchar *encoded;
	gchar *result;
	gchar **strv;

	/* Encoded Form: <name> ':' <color> [ '|' <tag> ] */

	g_return_val_if_fail (E_IS_MAIL_LABEL_LIST_STORE (store), NULL);
	g_return_val_if_fail (iter != NULL, NULL);
	gtk_tree_model_get (GTK_TREE_MODEL (store), iter, 0, &encoded, -1);

	strv = g_strsplit_set (encoded, ":|", 3);

	if (g_strv_length (strv) >= 2)
		result = mail_label_list_store_get_stock_id (store, strv[1]);
	else
		result = NULL;

	g_strfreev (strv);
	g_free (encoded);

	return result;
}

gchar *
e_mail_label_list_store_get_tag (EMailLabelListStore *store,
                                 GtkTreeIter *iter)
{
	gchar *encoded;
	gchar *result;
	gchar **strv;

	/* Encoded Form: <name> ':' <color> [ '|' <tag> ] */

	g_return_val_if_fail (E_IS_MAIL_LABEL_LIST_STORE (store), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	gtk_tree_model_get (GTK_TREE_MODEL (store), iter, 0, &encoded, -1);

	strv = g_strsplit_set (encoded, ":|", 3);

	/* XXX I guess for historical reasons the default label tags have
	 *     a "$Label" prefix, but the default list in GSettings doesn't
	 *     include tags.  That's why the <tag> part is optional.
	 *     So if we're missing the <tag> part, look it up in the
	 *     hard-coded default list above.
	 *
	 *     Not sure I got my facts straight here.  Double check. */
	if (g_strv_length (strv) >= 3)
		result = g_strdup (strv[2]);
	else {
		gint ii;

		result = NULL;

		for (ii = 0; ii < G_N_ELEMENTS (label_defaults); ii++) {
			const gchar *label_name;
			const gchar *label_tag;

			label_name = label_defaults[ii].label_name;
			label_tag = label_defaults[ii].label_tag;

			if (strcmp (strv[0], label_name) == 0) {
				result = g_strdup (label_tag);
				break;
			}
		}
	}

	/* XXX Still no luck?  The label list in GSettings must be screwed up.
	 *     We must not return NULL because the tag is used as a key in
	 *     the index hash table, so generate a tag from the name. */
	if (result == NULL)
		result = mail_label_list_store_tag_from_name (strv[0]);

	g_strfreev (strv);
	g_free (encoded);

	return result;
}

void
e_mail_label_list_store_set (EMailLabelListStore *store,
                             GtkTreeIter *iter,
                             const gchar *name,
                             const GdkColor *color)
{
	gchar *encoded;
	gchar *label_color;
	gchar *label_tag = NULL;

	g_return_if_fail (E_IS_MAIL_LABEL_LIST_STORE (store));
	g_return_if_fail (name != NULL);
	g_return_if_fail (color != NULL);

	label_color = gdk_color_to_string (color);

	if (iter != NULL)
		label_tag = e_mail_label_list_store_get_tag (store, iter);
	if (label_tag == NULL)
		label_tag = mail_label_list_store_tag_from_name (name);

	encoded = mail_label_list_store_encode_label (
		name, label_color, label_tag);

	/* We use gtk_list_store_insert_with_values() so the data is
	 * in place when the 'row-inserted' signal is emitted and our
	 * row_inserted() method executes. */
	if (iter != NULL)
		gtk_list_store_set (
			GTK_LIST_STORE (store), iter, 0, encoded, -1);
	else
		gtk_list_store_insert_with_values (
			GTK_LIST_STORE (store), NULL, -1, 0, encoded, -1);

	g_free (label_color);
	g_free (label_tag);
	g_free (encoded);
}

gboolean
e_mail_label_list_store_lookup (EMailLabelListStore *store,
                                const gchar *tag,
                                GtkTreeIter *iter)
{
	GtkTreeIter *stored_iter;

	g_return_val_if_fail (E_IS_MAIL_LABEL_LIST_STORE (store), FALSE);
	g_return_val_if_fail (tag != NULL, FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	stored_iter = g_hash_table_lookup (store->priv->tag_index, tag);

	if (!stored_iter)
		return FALSE;

	*iter = *stored_iter;

	return TRUE;
}

gboolean
e_mail_label_tag_is_default (const gchar *tag)
{
	g_return_val_if_fail (tag != NULL, FALSE);

	return g_str_has_prefix (tag, "$Label");
}
