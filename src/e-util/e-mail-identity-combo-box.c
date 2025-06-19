/*
 * e-mail-identity-combo-box.c
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
 */

/**
 * SECTION: e-mail-identity-combo-box
 * @include: e-util/e-util.h
 * @short_description: Combo box of mail identities
 *
 * #EMailIdentity is a combo box of available mail identities, as described
 * by #ESource instances with an #ESourceMailIdentity extension.  For
 * convenience, the combo box model's #GtkComboBox:id-column is populated
 * with #ESource #ESource:uid strings.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-mail-identity-combo-box.h"

#define SOURCE_IS_MAIL_IDENTITY(source) \
	(e_source_has_extension ((source), E_SOURCE_EXTENSION_MAIL_IDENTITY))

struct _EMailIdentityComboBoxPrivate {
	ESourceRegistry *registry;
	gulong source_added_handler_id;
	gulong source_changed_handler_id;
	gulong source_removed_handler_id;

	gchar *none_title;

	gboolean allow_none;
	gboolean allow_aliases;

	guint refresh_idle_id;

	gint refreshing;
	gint max_natural_width;
	gint last_natural_width;
};

enum {
	PROP_0,
	PROP_ALLOW_ALIASES,
	PROP_ALLOW_NONE,
	PROP_REGISTRY
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailIdentityComboBox, e_mail_identity_combo_box, GTK_TYPE_COMBO_BOX)

static gboolean
mail_identity_combo_box_refresh_idle_cb (gpointer user_data)
{
	EMailIdentityComboBox *combo_box = user_data;

	/* The refresh function will clear the idle ID. */
	e_mail_identity_combo_box_refresh (combo_box);

	return FALSE;
}

static void
mail_identity_combo_box_schedule_refresh (EMailIdentityComboBox *combo_box)
{
	/* Use an idle callback to limit how frequently we refresh
	 * the tree model in case the registry is emitting lots of
	 * signals at once. */

	if (combo_box->priv->refresh_idle_id == 0) {
		combo_box->priv->refresh_idle_id = g_idle_add (
			mail_identity_combo_box_refresh_idle_cb, combo_box);
	}
}

static void
mail_identity_combo_box_source_added_cb (ESourceRegistry *registry,
                                         ESource *source,
                                         EMailIdentityComboBox *combo_box)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY))
		mail_identity_combo_box_schedule_refresh (combo_box);
}

static void
mail_identity_combo_box_source_changed_cb (ESourceRegistry *registry,
                                           ESource *source,
                                           EMailIdentityComboBox *combo_box)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY))
		mail_identity_combo_box_schedule_refresh (combo_box);
}

static void
mail_identity_combo_box_source_removed_cb (ESourceRegistry *registry,
                                           ESource *source,
                                           EMailIdentityComboBox *combo_box)
{
	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_IDENTITY))
		mail_identity_combo_box_schedule_refresh (combo_box);
}

static void
mail_identity_combo_box_activate_default (EMailIdentityComboBox *combo_box)
{
	ESource *source;
	ESourceRegistry *registry;

	registry = e_mail_identity_combo_box_get_registry (combo_box);
	source = e_source_registry_ref_default_mail_identity (registry);

	if (source != NULL) {
		const gchar *uid = e_source_get_uid (source);
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), uid);
		g_object_unref (source);
	}
}

static void
mail_identity_combo_box_get_preferred_width (GtkWidget *widget,
					     gint *minimum_width,
					     gint *natural_width)
{
	EMailIdentityComboBox *self = E_MAIL_IDENTITY_COMBO_BOX (widget);

	GTK_WIDGET_CLASS (e_mail_identity_combo_box_parent_class)->get_preferred_width (widget, minimum_width, natural_width);

	self->priv->last_natural_width = *natural_width;

	if (self->priv->max_natural_width > 0) {
		if (*natural_width > self->priv->max_natural_width)
			*natural_width = self->priv->max_natural_width;
		if (*minimum_width > *natural_width)
			*minimum_width = *natural_width;
	}
}

static void
mail_identity_combo_box_set_registry (EMailIdentityComboBox *combo_box,
                                      ESourceRegistry *registry)
{
	gulong handler_id;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (combo_box->priv->registry == NULL);

	combo_box->priv->registry = g_object_ref (registry);

	handler_id = g_signal_connect (
		registry, "source-added",
		G_CALLBACK (mail_identity_combo_box_source_added_cb),
		combo_box);
	combo_box->priv->source_added_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (mail_identity_combo_box_source_changed_cb),
		combo_box);
	combo_box->priv->source_changed_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (mail_identity_combo_box_source_removed_cb),
		combo_box);
	combo_box->priv->source_removed_handler_id = handler_id;
}

static void
mail_identity_combo_box_set_property (GObject *object,
                                      guint property_id,
                                      const GValue *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALLOW_ALIASES:
			e_mail_identity_combo_box_set_allow_aliases (
				E_MAIL_IDENTITY_COMBO_BOX (object),
				g_value_get_boolean (value));
			return;

		case PROP_ALLOW_NONE:
			e_mail_identity_combo_box_set_allow_none (
				E_MAIL_IDENTITY_COMBO_BOX (object),
				g_value_get_boolean (value));
			return;

		case PROP_REGISTRY:
			mail_identity_combo_box_set_registry (
				E_MAIL_IDENTITY_COMBO_BOX (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_identity_combo_box_get_property (GObject *object,
                                      guint property_id,
                                      GValue *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ALLOW_ALIASES:
			g_value_set_boolean (
				value,
				e_mail_identity_combo_box_get_allow_aliases (
				E_MAIL_IDENTITY_COMBO_BOX (object)));
			return;

		case PROP_ALLOW_NONE:
			g_value_set_boolean (
				value,
				e_mail_identity_combo_box_get_allow_none (
				E_MAIL_IDENTITY_COMBO_BOX (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_identity_combo_box_get_registry (
				E_MAIL_IDENTITY_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_identity_combo_box_dispose (GObject *object)
{
	EMailIdentityComboBox *self = E_MAIL_IDENTITY_COMBO_BOX (object);

	if (self->priv->source_added_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_added_handler_id);
		self->priv->source_added_handler_id = 0;
	}

	if (self->priv->source_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_changed_handler_id);
		self->priv->source_changed_handler_id = 0;
	}

	if (self->priv->source_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_removed_handler_id);
		self->priv->source_removed_handler_id = 0;
	}

	if (self->priv->refresh_idle_id > 0) {
		g_source_remove (self->priv->refresh_idle_id);
		self->priv->refresh_idle_id = 0;
	}

	g_clear_pointer (&self->priv->none_title, g_free);
	g_clear_object (&self->priv->registry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_identity_combo_box_parent_class)->dispose (object);
}

static void
mail_identity_combo_box_constructed (GObject *object)
{
	GtkListStore *list_store;
	GtkComboBox *combo_box;
	GtkCellLayout *cell_layout;
	GtkCellRenderer *cell_renderer;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_identity_combo_box_parent_class)->constructed (object);

	combo_box = GTK_COMBO_BOX (object);
	cell_layout = GTK_CELL_LAYOUT (object);

	list_store = gtk_list_store_new (5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (list_store));
	gtk_combo_box_set_id_column (combo_box, E_MAIL_IDENTITY_COMBO_BOX_COLUMN_COMBO_ID);
	g_object_unref (list_store);

	cell_renderer = gtk_cell_renderer_text_new ();
	g_object_set (cell_renderer,
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);
	gtk_cell_layout_pack_start (cell_layout, cell_renderer, TRUE);
	gtk_cell_layout_add_attribute (
		cell_layout, cell_renderer, "text", E_MAIL_IDENTITY_COMBO_BOX_COLUMN_DISPLAY_NAME);

	e_mail_identity_combo_box_refresh (E_MAIL_IDENTITY_COMBO_BOX (object));
}

static void
e_mail_identity_combo_box_class_init (EMailIdentityComboBoxClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_identity_combo_box_set_property;
	object_class->get_property = mail_identity_combo_box_get_property;
	object_class->dispose = mail_identity_combo_box_dispose;
	object_class->constructed = mail_identity_combo_box_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = mail_identity_combo_box_get_preferred_width;

	g_object_class_install_property (
		object_class,
		PROP_ALLOW_ALIASES,
		g_param_spec_boolean (
			"allow-aliases",
			"Allow separate items with identity aliases",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ALLOW_NONE,
		g_param_spec_boolean (
			"allow-none",
			"Allow None Item",
			NULL,
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			NULL,
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_mail_identity_combo_box_init (EMailIdentityComboBox *combo_box)
{
	combo_box->priv = e_mail_identity_combo_box_get_instance_private (combo_box);
	combo_box->priv->max_natural_width = 100;
}

/**
 * e_mail_identity_combo_box_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #EMailIdentityComboBox widget using #ESource instances in
 * @registry.
 *
 * Returns: a new #EMailIdentityComboBox
 **/
GtkWidget *
e_mail_identity_combo_box_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_MAIL_IDENTITY_COMBO_BOX,
		"registry", registry, NULL);
}

static gint
compare_identity_sources_cb (gconstpointer aa,
			     gconstpointer bb,
			     gpointer user_data)
{
	ESource *a_source = (ESource *) aa;
	ESource *b_source = (ESource *) bb;
	GHashTable *indexes = user_data;
	gint a_index = 0, b_index = 0;

	if (indexes && e_source_get_uid (a_source) && e_source_get_uid (b_source)) {
		a_index = GPOINTER_TO_INT (g_hash_table_lookup (indexes, e_source_get_uid (a_source)));
		if (!a_index && e_source_get_parent (a_source))
			a_index = GPOINTER_TO_INT (g_hash_table_lookup (indexes, e_source_get_parent (a_source)));

		b_index = GPOINTER_TO_INT (g_hash_table_lookup (indexes, e_source_get_uid (b_source)));
		if (!b_index && e_source_get_parent (b_source))
			b_index = GPOINTER_TO_INT (g_hash_table_lookup (indexes, e_source_get_parent (b_source)));
	}

	if (a_index == b_index) {
		ESourceMailIdentity *a_identity = e_source_get_extension (a_source, E_SOURCE_EXTENSION_MAIL_IDENTITY);
		ESourceMailIdentity *b_identity = e_source_get_extension (b_source, E_SOURCE_EXTENSION_MAIL_IDENTITY);
		const gchar *a_value, *b_value;

		b_index = 0;

		a_value = e_source_mail_identity_get_name (a_identity);
		b_value = e_source_mail_identity_get_name (b_identity);

		a_index = (a_value && b_value) ? g_utf8_collate (a_value, b_value) : g_strcmp0 (a_value, b_value);

		if (!a_index) {
			a_index = g_strcmp0 (e_source_mail_identity_get_address (a_identity),
					     e_source_mail_identity_get_address (b_identity));
		}
	}

	return a_index - b_index;
}

/* This is in e-util/, compiled before mail/, thus cannot reach EMailAccountStore,
   thus copy the code. */
static GList * /* ESource * */
mail_identity_combo_box_sort_sources (GList *sources) /* ESource * */
{
	gchar *sort_order_filename;
	GHashTable *indexes; /* gchar *uid ~> gint index */

	if (!sources)
		return sources;

	indexes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	sort_order_filename = g_build_filename (e_get_user_config_dir (), "mail", "sortorder.ini", NULL);
	if (g_file_test (sort_order_filename, G_FILE_TEST_IS_REGULAR)) {
		GKeyFile *key_file;

		key_file = g_key_file_new ();
		if (g_key_file_load_from_file (key_file, sort_order_filename, G_KEY_FILE_NONE, NULL)) {
			gchar **service_uids;
			gsize ii, length;

			service_uids = g_key_file_get_string_list (key_file, "Accounts", "SortOrder", &length, NULL);

			for (ii = 0; ii < length; ii++) {
				const gchar *uid = service_uids[ii];

				if (uid && *uid)
					g_hash_table_insert (indexes, g_strdup (uid), GINT_TO_POINTER (ii + 1));
			}

			g_strfreev (service_uids);
		}

		g_key_file_free (key_file);
	}

	g_free (sort_order_filename);

	sources = g_list_sort_with_data (sources, compare_identity_sources_cb, indexes);

	g_hash_table_destroy (indexes);

	return sources;
}

static gchar *
mail_identity_combo_box_build_alias_id (const gchar *identity_uid,
					const gchar *name,
					const gchar *address)
{
	g_return_val_if_fail (identity_uid != NULL, NULL);

	if (!address || !*address)
		return g_strdup (identity_uid);

	return g_strconcat (identity_uid, "\n", address, "\n", name, NULL);
}

static void
mail_identity_combo_box_add_address (GtkListStore *list_store,
				     GHashTable *address_table,
				     const gchar *name,
				     const gchar *address,
				     gboolean is_alias_entry,
				     const gchar *stored_alias_name,
				     const gchar *identity_uid,
				     const gchar *identity_display_name)
{
	GtkTreeIter iter;
	GQueue *queue;
	GString *string;
	gchar *alias_id;

	g_return_if_fail (GTK_IS_LIST_STORE (list_store));
	g_return_if_fail (address_table != NULL);

	if (!address || !*address)
		return;

	queue = g_hash_table_lookup (address_table, address);

	string = g_string_sized_new (512);
	if (name && *name)
		g_string_append_printf (string, "%s <%s>", name, address);
	else
		g_string_append_printf (string, "%s", address);

	/* Show the account name for duplicate email addresses. */
	if (queue != NULL && g_queue_get_length (queue) > 1)
		g_string_append_printf (string, " (%s)", identity_display_name);

	alias_id = mail_identity_combo_box_build_alias_id (identity_uid, stored_alias_name, address);

	gtk_list_store_append (list_store, &iter);

	gtk_list_store_set (list_store, &iter,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_DISPLAY_NAME, string->str,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_COMBO_ID, is_alias_entry ? alias_id : identity_uid,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_UID, identity_uid,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_NAME, is_alias_entry ? name : NULL,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_ADDRESS, is_alias_entry ? address : NULL,
		-1);

	g_string_free (string, TRUE);
	g_free (alias_id);
}

/**
 * e_mail_identity_combo_box_refresh:
 * @combo_box: an #EMailIdentityComboBox
 *
 * Rebuilds the combo box model with an updated list of #ESource instances
 * that describe a mail identity, without disrupting the previously active
 * item (if possible).
 *
 * This function is called automatically in response to #ESourceRegistry
 * signals which are pertinent to the @combo_box.
 **/
void
e_mail_identity_combo_box_refresh (EMailIdentityComboBox *combo_box)
{
	ESourceRegistry *registry;
	GtkTreeModel *tree_model;
	GtkComboBox *gtk_combo_box;
	ESource *source;
	GList *list, *link;
	GHashTable *address_table;
	const gchar *extension_name;
	const gchar *saved_uid;

	g_return_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box));

	g_atomic_int_inc (&combo_box->priv->refreshing);

	if (combo_box->priv->refresh_idle_id > 0) {
		g_source_remove (combo_box->priv->refresh_idle_id);
		combo_box->priv->refresh_idle_id = 0;
	}

	gtk_combo_box = GTK_COMBO_BOX (combo_box);
	tree_model = gtk_combo_box_get_model (gtk_combo_box);

	/* This is an interned string, which means it's safe
	 * to use even after clearing the combo box model. */
	saved_uid = gtk_combo_box_get_active_id (gtk_combo_box);

	gtk_list_store_clear (GTK_LIST_STORE (tree_model));

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	registry = e_mail_identity_combo_box_get_registry (combo_box);
	list = e_source_registry_list_enabled (registry, extension_name);

	list = mail_identity_combo_box_sort_sources (list);

	/* Build a hash table of GQueues by email address so we can
	 * spot duplicate email addresses.  Then if the GQueue for a
	 * given email address has multiple elements, we use a more
	 * verbose description in the combo box. */

	address_table = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_queue_free);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESourceMailIdentity *extension;
		GQueue *queue;
		GHashTable *aliases;
		const gchar *address;

		source = E_SOURCE (link->data);

		if (!e_util_identity_can_send (registry, source))
			continue;

		extension = e_source_get_extension (source, extension_name);
		address = e_source_mail_identity_get_address (extension);

		if (address) {
			queue = g_hash_table_lookup (address_table, address);
			if (queue == NULL) {
				queue = g_queue_new ();
				g_hash_table_insert (
					address_table,
					g_strdup (address), queue);
			}

			g_queue_push_tail (queue, source);
		}

		if (!e_mail_identity_combo_box_get_allow_aliases (combo_box))
			continue;

		aliases = e_source_mail_identity_get_aliases_as_hash_table (extension);
		if (aliases) {
			GHashTableIter iter;
			gpointer key;

			g_hash_table_iter_init (&iter, aliases);
			while (g_hash_table_iter_next (&iter, &key, NULL)) {
				address = key;

				if (address && *address) {
					queue = g_hash_table_lookup (address_table, address);
					if (queue) {
						if (!g_queue_find (queue, source))
							g_queue_push_tail (queue, source);
					} else {
						queue = g_queue_new ();
						g_hash_table_insert (
							address_table,
							g_strdup (address), queue);

						g_queue_push_tail (queue, source);
					}
				}
			}
			g_hash_table_destroy (aliases);
		}
	}

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESourceMailIdentity *extension;
		const gchar *address;
		const gchar *name;
		const gchar *uid;
		const gchar *display_name;
		gchar *aliases;

		source = E_SOURCE (link->data);

		if (!e_util_identity_can_send (registry, source))
			continue;

		uid = e_source_get_uid (source);
		display_name = e_source_get_display_name (source);
		extension = e_source_get_extension (source, extension_name);
		name = e_source_mail_identity_get_name (extension);
		address = e_source_mail_identity_get_address (extension);

		mail_identity_combo_box_add_address (GTK_LIST_STORE (tree_model),
			address_table, name, address, FALSE, NULL, uid, display_name);

		if (!e_mail_identity_combo_box_get_allow_aliases (combo_box))
			continue;

		aliases = e_source_mail_identity_dup_aliases (extension);
		if (aliases && *aliases) {
			CamelInternetAddress *inet_address;
			gint ii, len;

			inet_address = camel_internet_address_new ();
			len = camel_address_decode (CAMEL_ADDRESS (inet_address), aliases);

			for (ii = 0; ii < len; ii++) {
				const gchar *alias_name = NULL, *alias_address = NULL;

				if (camel_internet_address_get (inet_address, ii, &alias_name, &alias_address) &&
				    alias_address && *alias_address) {
					if (!alias_name || !*alias_name)
						alias_name = NULL;

					mail_identity_combo_box_add_address (GTK_LIST_STORE (tree_model),
						address_table, alias_name ? alias_name : name, alias_address, TRUE, alias_name, uid, display_name);
				}
			}

			g_clear_object (&inet_address);
		}

		g_free (aliases);
	}

	g_hash_table_destroy (address_table);

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (combo_box->priv->allow_none) {
		GtkTreeIter iter;

		gtk_list_store_insert (GTK_LIST_STORE (tree_model), &iter, 0);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &iter,
			E_MAIL_IDENTITY_COMBO_BOX_COLUMN_DISPLAY_NAME, e_mail_identity_combo_box_get_none_title (combo_box),
			E_MAIL_IDENTITY_COMBO_BOX_COLUMN_UID, "",
			E_MAIL_IDENTITY_COMBO_BOX_COLUMN_COMBO_ID, "",
			-1);
	}

	/* Try and restore the previous selected source, or else pick
	 * the default identity of the default mail account.  If even
	 * that fails, just pick the first item. */

	if (saved_uid != NULL)
		gtk_combo_box_set_active_id (gtk_combo_box, saved_uid);

	if (!combo_box->priv->allow_none &&
	    gtk_combo_box_get_active_id (gtk_combo_box) == NULL)
		mail_identity_combo_box_activate_default (combo_box);

	if (gtk_combo_box_get_active_id (gtk_combo_box) == NULL)
		gtk_combo_box_set_active (gtk_combo_box, 0);

	if (g_atomic_int_dec_and_test (&combo_box->priv->refreshing)) {
		if (g_strcmp0 (gtk_combo_box_get_active_id (gtk_combo_box), saved_uid) != 0)
			g_signal_emit_by_name (gtk_combo_box, "changed");
	}
}

/**
 * e_mail_identity_combo_box_get_registry:
 * @combo_box: an #EMailIdentityComboBox
 *
 * Returns the #ESourceRegistry passed to e_mail_identity_combo_box_new().
 *
 * Returns: an #ESourceRegistry
 **/
ESourceRegistry *
e_mail_identity_combo_box_get_registry (EMailIdentityComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->registry;
}

/**
 * e_mail_identity_combo_box_get_allow_none:
 * @combo_box: an #EMailIdentityComboBox
 *
 * Returns whether to append the mail identity list with a "None" item for
 * use cases where the user may wish to opt out of choosing a mail identity.
 *
 * Returns: whether to include a "None" option
 **/
gboolean
e_mail_identity_combo_box_get_allow_none (EMailIdentityComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box), FALSE);

	return combo_box->priv->allow_none;
}

/**
 * e_mail_identity_combo_box_set_allow_none:
 * @combo_box: an #EMailIdentityComboBox
 * @allow_none: whether to include a "None" option
 *
 * Sets whether to append the mail identity list with a "None" item for use
 * cases where the user may wish to opt out of choosing a mail identity.
 *
 * Changing this property will automatically rebuild the combo box model.
 **/
void
e_mail_identity_combo_box_set_allow_none (EMailIdentityComboBox *combo_box,
                                          gboolean allow_none)
{
	g_return_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box));

	if (allow_none == combo_box->priv->allow_none)
		return;

	combo_box->priv->allow_none = allow_none;

	g_object_notify (G_OBJECT (combo_box), "allow-none");

	e_mail_identity_combo_box_refresh (combo_box);
}

/**
 * e_mail_identity_combo_box_get_none_title:
 * @combo_box: an #EMailIdentityComboBox
 *
 * Returns: what title the none item should have
 *
 * Since: 3.42
 **/
const gchar *
e_mail_identity_combo_box_get_none_title (EMailIdentityComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box), NULL);

	if (combo_box->priv->none_title)
		return combo_box->priv->none_title;

	return _("None");
}

/**
 * e_mail_identity_combo_box_set_none_title:
 * @combo_box: an #EMailIdentityComboBox
 * @none_title: (nullable): a title to use, or %NULL
 *
 * Set what title the none item should have. This is a user visible string, thus
 * it should be localized. Use %NULL to reset to the default "None" title.
 *
 * Since: 3.42
 **/
void
e_mail_identity_combo_box_set_none_title (EMailIdentityComboBox *combo_box,
					  const gchar *none_title)
{
	g_return_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box));

	if (combo_box->priv->none_title != none_title) {
		g_free (combo_box->priv->none_title);
		combo_box->priv->none_title = g_strdup (none_title);
	}
}

/**
 * e_mail_identity_combo_box_get_allow_aliases:
 * @combo_box: an #EMailIdentityComboBox
 *
 * Returns whether to show also aliases of the mail identities.
 *
 * Returns: whether to show also aliases of the mail identities
 *
 * Since: 3.24
 **/
gboolean
e_mail_identity_combo_box_get_allow_aliases (EMailIdentityComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box), FALSE);

	return combo_box->priv->allow_aliases;
}

/**
 * e_mail_identity_combo_box_set_allow_aliases:
 * @combo_box: an #EMailIdentityComboBox
 * @allow_aliases: whether to show also aliases of the mail identities
 *
 * Sets whether to show also aliases of the mail identities.
 *
 * Changing this property will automatically rebuild the combo box model.
 *
 * Since: 3.24
 **/
void
e_mail_identity_combo_box_set_allow_aliases (EMailIdentityComboBox *combo_box,
					     gboolean allow_aliases)
{
	g_return_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box));

	if (allow_aliases == combo_box->priv->allow_aliases)
		return;

	combo_box->priv->allow_aliases = allow_aliases;

	g_object_notify (G_OBJECT (combo_box), "allow-aliases");

	e_mail_identity_combo_box_refresh (combo_box);
}

/**
 * e_mail_identity_combo_box_get_active_uid:
 * @combo_box: an #EMailIdentityComboBox
 * @identity_uid: (out) (transfer full): identity UID of the currently active item
 * @alias_name: (out) (nullable) (transfer full): alias name of the currently active item
 * @alias_address: (out) (nullable) (transfer full): alias address of the currently active item
 *
 * Sets identity UID, used name and used address for the currently
 * active item in the @combo_box. Both @alias_name and @alias_address
 * are optional.
 *
 * Returns: Whether any item was selected. If %FALSE is returned, then
 *   the values of the output arguments are unchanged. Free the returned
 *   values with g_free() when done with them.
 *
 * Since: 3.24
 **/
gboolean
e_mail_identity_combo_box_get_active_uid (EMailIdentityComboBox *combo_box,
					  gchar **identity_uid,
					  gchar **alias_name,
					  gchar **alias_address)
{
	GtkTreeIter iter;
	gchar *name = NULL, *address = NULL;

	g_return_val_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box), FALSE);
	g_return_val_if_fail (identity_uid != NULL, FALSE);

	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &iter))
		return FALSE;

	gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box)), &iter,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_UID, identity_uid,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_NAME, &name,
		E_MAIL_IDENTITY_COMBO_BOX_COLUMN_ADDRESS, &address,
		-1);

	if (alias_name)
		*alias_name = name;
	else
		g_free (name);

	if (alias_address)
		*alias_address = address;
	else
		g_free (address);

	return TRUE;
}

/**
 * e_mail_identity_combo_box_set_active_uid:
 * @combo_box: an #EMailIdentityComboBox
 * @identity_uid: identity UID to select
 * @alias_name: (nullable): alias name to select
 * @alias_address: (nullable): alias address to select
 *
 * Selects an item which corresponds to @identity_uid. If the @alias_address
 * is specified, then it will try to select an alias entry with this address
 * for this identity UID. If no such can be found, then picks the main
 * @identity_uid item instead.
 *
 * Returns: Whether such identity_uid had been found and selected.
 *
 * Since: 3.24
 **/
gboolean
e_mail_identity_combo_box_set_active_uid (EMailIdentityComboBox *combo_box,
					  const gchar *identity_uid,
					  const gchar *alias_name,
					  const gchar *alias_address)
{
	gchar *alias_id;
	gboolean found;

	g_return_val_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box), FALSE);
	g_return_val_if_fail (identity_uid != NULL, FALSE);

	alias_id = mail_identity_combo_box_build_alias_id (identity_uid, alias_name, alias_address);

	found = gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), alias_id);

	g_free (alias_id);

	if (!found && alias_address && *alias_address) {
		/* Match the first alias_address, not considering the name at all */
		GtkTreeModel *model;
		GtkTreeIter iter;

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			do {
				gchar *uid = NULL, *address = NULL;

				gtk_tree_model_get (model, &iter,
					E_MAIL_IDENTITY_COMBO_BOX_COLUMN_UID, &uid,
					E_MAIL_IDENTITY_COMBO_BOX_COLUMN_ADDRESS, &address,
					-1);

				found = g_strcmp0 (uid, identity_uid) == 0 &&
					address && g_ascii_strcasecmp (address, alias_address) == 0;

				g_free (uid);
				g_free (address);

				if (found)
					gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_box), &iter);
			} while (!found && gtk_tree_model_iter_next (model, &iter));
		}
	}

	if (!found && alias_address)
		found = gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo_box), identity_uid);

	return found;
}

/**
 * e_mail_identity_combo_box_get_refreshing:
 * @combo_box: an #EMailIdentityComboBox
 *
 * Returns: Whether the combo box content is currently refreshing.
 *
 * Since: 3.16
 **/
gboolean
e_mail_identity_combo_box_get_refreshing (EMailIdentityComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (combo_box), FALSE);

	return combo_box->priv->refreshing != 0;
}

gint
e_mail_identity_combo_box_get_max_natural_width (EMailIdentityComboBox *self)
{
	g_return_val_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (self), -1);

	return self->priv->max_natural_width;
}

void
e_mail_identity_combo_box_set_max_natural_width (EMailIdentityComboBox *self,
						 gint value)
{
	g_return_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (self));

	if (self->priv->max_natural_width != value) {
		self->priv->max_natural_width = value;
		gtk_widget_queue_resize (GTK_WIDGET (self));
	}
}

gint
e_mail_identity_combo_box_get_last_natural_width (EMailIdentityComboBox *self)
{
	g_return_val_if_fail (E_IS_MAIL_IDENTITY_COMBO_BOX (self), -1);

	return self->priv->last_natural_width;
}
