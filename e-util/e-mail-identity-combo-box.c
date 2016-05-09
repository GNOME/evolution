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

#include "e-mail-identity-combo-box.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#define E_MAIL_IDENTITY_COMBO_BOX_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_IDENTITY_COMBO_BOX, EMailIdentityComboBoxPrivate))

#define SOURCE_IS_MAIL_IDENTITY(source) \
	(e_source_has_extension ((source), E_SOURCE_EXTENSION_MAIL_IDENTITY))

struct _EMailIdentityComboBoxPrivate {
	ESourceRegistry *registry;
	gulong source_added_handler_id;
	gulong source_changed_handler_id;
	gulong source_removed_handler_id;

	gboolean allow_none;

	guint refresh_idle_id;

	gint refreshing;
};

enum {
	PROP_0,
	PROP_ALLOW_NONE,
	PROP_REGISTRY
};

enum {
	COLUMN_DISPLAY_NAME,
	COLUMN_UID
};

G_DEFINE_TYPE (
	EMailIdentityComboBox,
	e_mail_identity_combo_box,
	GTK_TYPE_COMBO_BOX)

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
	EMailIdentityComboBoxPrivate *priv;

	priv = E_MAIL_IDENTITY_COMBO_BOX_GET_PRIVATE (object);

	if (priv->source_added_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_added_handler_id);
		priv->source_added_handler_id = 0;
	}

	if (priv->source_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_changed_handler_id);
		priv->source_changed_handler_id = 0;
	}

	if (priv->source_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->registry,
			priv->source_removed_handler_id);
		priv->source_removed_handler_id = 0;
	}

	if (priv->refresh_idle_id > 0) {
		g_source_remove (priv->refresh_idle_id);
		priv->refresh_idle_id = 0;
	}

	g_clear_object (&priv->registry);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_identity_combo_box_parent_class)->
		dispose (object);
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

	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (list_store));
	gtk_combo_box_set_id_column (combo_box, COLUMN_UID);
	g_object_unref (list_store);

	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (cell_layout, cell_renderer, TRUE);
	gtk_cell_layout_add_attribute (
		cell_layout, cell_renderer, "text", COLUMN_DISPLAY_NAME);

	e_mail_identity_combo_box_refresh (E_MAIL_IDENTITY_COMBO_BOX (object));
}

static void
e_mail_identity_combo_box_class_init (EMailIdentityComboBoxClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (EMailIdentityComboBoxPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_identity_combo_box_set_property;
	object_class->get_property = mail_identity_combo_box_get_property;
	object_class->dispose = mail_identity_combo_box_dispose;
	object_class->constructed = mail_identity_combo_box_constructed;

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
	combo_box->priv = E_MAIL_IDENTITY_COMBO_BOX_GET_PRIVATE (combo_box);
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
		const gchar *address;

		source = E_SOURCE (link->data);
		extension = e_source_get_extension (source, extension_name);
		address = e_source_mail_identity_get_address (extension);

		if (address == NULL)
			continue;

		queue = g_hash_table_lookup (address_table, address);
		if (queue == NULL) {
			queue = g_queue_new ();
			g_hash_table_insert (
				address_table,
				g_strdup (address), queue);
		}

		g_queue_push_tail (queue, source);
	}

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESourceMailIdentity *extension;
		GtkTreeIter iter;
		GQueue *queue;
		GString *string;
		const gchar *address;
		const gchar *display_name;
		const gchar *name;
		const gchar *uid;

		source = E_SOURCE (link->data);

		extension = e_source_get_extension (source, extension_name);
		name = e_source_mail_identity_get_name (extension);
		address = e_source_mail_identity_get_address (extension);

		if (address == NULL)
			continue;

		queue = g_hash_table_lookup (address_table, address);

		display_name = e_source_get_display_name (source);
		uid = e_source_get_uid (source);

		string = g_string_sized_new (512);
		if (name && *name)
			g_string_append_printf (string, "%s <%s>", name, address);
		else
			g_string_append_printf (string, "%s", address);

		/* Show the account name for duplicate email addresses. */
		if (queue != NULL && g_queue_get_length (queue) > 1)
			g_string_append_printf (string, " (%s)", display_name);

		gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &iter,
			COLUMN_DISPLAY_NAME, string->str,
			COLUMN_UID, uid, -1);

		g_string_free (string, TRUE);
	}

	g_hash_table_destroy (address_table);

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	if (combo_box->priv->allow_none) {
		GtkTreeIter iter;

		gtk_list_store_insert (GTK_LIST_STORE (tree_model), &iter, 0);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &iter,
			COLUMN_DISPLAY_NAME, _("None"),
			COLUMN_UID, "", -1);
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
