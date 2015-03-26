/*
 * e-mail-signature-combo-box.c
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

#include "e-mail-signature-combo-box.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#define E_MAIL_SIGNATURE_COMBO_BOX_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_SIGNATURE_COMBO_BOX, EMailSignatureComboBoxPrivate))

#define SOURCE_IS_MAIL_SIGNATURE(source) \
	(e_source_has_extension ((source), E_SOURCE_EXTENSION_MAIL_SIGNATURE))

struct _EMailSignatureComboBoxPrivate {
	ESourceRegistry *registry;
	guint refresh_idle_id;
	gchar *identity_uid;
};

enum {
	PROP_0,
	PROP_IDENTITY_UID,
	PROP_REGISTRY
};

enum {
	COLUMN_DISPLAY_NAME,
	COLUMN_UID
};

G_DEFINE_TYPE (
	EMailSignatureComboBox,
	e_mail_signature_combo_box,
	GTK_TYPE_COMBO_BOX)

static gboolean
mail_signature_combo_box_refresh_idle_cb (EMailSignatureComboBox *combo_box)
{
	/* The refresh function will clear the idle ID. */
	e_mail_signature_combo_box_refresh (combo_box);

	return FALSE;
}

static void
mail_signature_combo_box_registry_changed (ESourceRegistry *registry,
                                           ESource *source,
                                           EMailSignatureComboBox *combo_box)
{
	/* If the ESource in question has a "Mail Signature" extension,
	 * schedule a refresh of the tree model.  Otherwise ignore it.
	 * We use an idle callback to limit how frequently we refresh
	 * the tree model, in case the registry is emitting lots of
	 * signals at once. */

	if (!SOURCE_IS_MAIL_SIGNATURE (source))
		return;

	if (combo_box->priv->refresh_idle_id > 0)
		return;

	combo_box->priv->refresh_idle_id = g_idle_add (
		(GSourceFunc) mail_signature_combo_box_refresh_idle_cb,
		combo_box);
}

static gboolean
mail_signature_combo_box_identity_to_signature (GBinding *binding,
                                                const GValue *source_value,
                                                GValue *target_value,
                                                gpointer user_data)
{
	EMailSignatureComboBox *combo_box;
	ESourceRegistry *registry;
	GObject *source_object;
	ESource *source;
	ESourceMailIdentity *extension;
	const gchar *identity_uid;
	const gchar *signature_uid = "none";
	const gchar *extension_name;

	/* Source and target are the same object. */
	source_object = g_binding_get_source (binding);
	combo_box = E_MAIL_SIGNATURE_COMBO_BOX (source_object);
	registry = e_mail_signature_combo_box_get_registry (combo_box);

	identity_uid = g_value_get_string (source_value);
	if (identity_uid == NULL)
		return FALSE;

	source = e_source_registry_ref_source (registry, identity_uid);
	if (source == NULL)
		return FALSE;

	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		return FALSE;
	}

	extension = e_source_get_extension (source, extension_name);
	signature_uid = e_source_mail_identity_get_signature_uid (extension);
	g_value_set_string (target_value, signature_uid);

	g_object_unref (source);

	return TRUE;
}

static void
mail_signature_combo_box_set_registry (EMailSignatureComboBox *combo_box,
                                       ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (combo_box->priv->registry == NULL);

	combo_box->priv->registry = g_object_ref (registry);

	g_signal_connect (
		registry, "source-added",
		G_CALLBACK (mail_signature_combo_box_registry_changed),
		combo_box);

	g_signal_connect (
		registry, "source-changed",
		G_CALLBACK (mail_signature_combo_box_registry_changed),
		combo_box);

	g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (mail_signature_combo_box_registry_changed),
		combo_box);
}

static void
mail_signature_combo_box_set_property (GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_UID:
			e_mail_signature_combo_box_set_identity_uid (
				E_MAIL_SIGNATURE_COMBO_BOX (object),
				g_value_get_string (value));
			return;

		case PROP_REGISTRY:
			mail_signature_combo_box_set_registry (
				E_MAIL_SIGNATURE_COMBO_BOX (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_combo_box_get_property (GObject *object,
                                       guint property_id,
                                       GValue *value,
                                       GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_IDENTITY_UID:
			g_value_set_string (
				value,
				e_mail_signature_combo_box_get_identity_uid (
				E_MAIL_SIGNATURE_COMBO_BOX (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_mail_signature_combo_box_get_registry (
				E_MAIL_SIGNATURE_COMBO_BOX (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_signature_combo_box_dispose (GObject *object)
{
	EMailSignatureComboBoxPrivate *priv;

	priv = E_MAIL_SIGNATURE_COMBO_BOX_GET_PRIVATE (object);

	if (priv->registry != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->registry, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->refresh_idle_id > 0) {
		g_source_remove (priv->refresh_idle_id);
		priv->refresh_idle_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_signature_combo_box_parent_class)->
		dispose (object);
}

static void
mail_signature_combo_box_finalize (GObject *object)
{
	EMailSignatureComboBoxPrivate *priv;

	priv = E_MAIL_SIGNATURE_COMBO_BOX_GET_PRIVATE (object);

	g_free (priv->identity_uid);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_signature_combo_box_parent_class)->
		finalize (object);
}

static void
mail_signature_combo_box_constructed (GObject *object)
{
	GtkListStore *list_store;
	GtkComboBox *combo_box;
	GtkCellLayout *cell_layout;
	GtkCellRenderer *cell_renderer;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_signature_combo_box_parent_class)->constructed (object);

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

	e_binding_bind_property_full (
		combo_box, "identity-uid",
		combo_box, "active-id",
		G_BINDING_DEFAULT,
		mail_signature_combo_box_identity_to_signature,
		NULL,
		NULL, (GDestroyNotify) NULL);

	e_mail_signature_combo_box_refresh (
		E_MAIL_SIGNATURE_COMBO_BOX (object));
}

static void
e_mail_signature_combo_box_class_init (EMailSignatureComboBoxClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (
		class, sizeof (EMailSignatureComboBoxPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_signature_combo_box_set_property;
	object_class->get_property = mail_signature_combo_box_get_property;
	object_class->dispose = mail_signature_combo_box_dispose;
	object_class->finalize = mail_signature_combo_box_finalize;
	object_class->constructed = mail_signature_combo_box_constructed;

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_UID,
		g_param_spec_string (
			"identity-uid",
			"Identity UID",
			NULL,
			NULL,
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
e_mail_signature_combo_box_init (EMailSignatureComboBox *combo_box)
{
	combo_box->priv = E_MAIL_SIGNATURE_COMBO_BOX_GET_PRIVATE (combo_box);
}

GtkWidget *
e_mail_signature_combo_box_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_MAIL_SIGNATURE_COMBO_BOX,
		"registry", registry, NULL);
}

void
e_mail_signature_combo_box_refresh (EMailSignatureComboBox *combo_box)
{
	ESourceRegistry *registry;
	GtkComboBox *gtk_combo_box;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	ESource *source;
	GList *list, *link;
	const gchar *extension_name;
	const gchar *saved_uid;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

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

	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	registry = e_mail_signature_combo_box_get_registry (combo_box);
	list = e_source_registry_list_sources (registry, extension_name);

	/* The "None" option always comes first. */

	gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (tree_model), &iter,
		COLUMN_DISPLAY_NAME, _("None"),
		COLUMN_UID, "none", -1);

	/* The "autogenerated" UID has special meaning. */

	gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (tree_model), &iter,
		COLUMN_DISPLAY_NAME, _("Autogenerated"),
		COLUMN_UID, E_MAIL_SIGNATURE_AUTOGENERATED_UID, -1);

	/* Followed by the other mail signatures, alphabetized. */

	for (link = list; link != NULL; link = g_list_next (link)) {
		GtkTreeIter iter;
		const gchar *display_name;
		const gchar *uid;

		source = E_SOURCE (link->data);
		display_name = e_source_get_display_name (source);
		uid = e_source_get_uid (source);

		gtk_list_store_append (GTK_LIST_STORE (tree_model), &iter);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &iter,
			COLUMN_DISPLAY_NAME, display_name,
			COLUMN_UID, uid, -1);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Try and restore the previous selected source, or else "None". */

	if (saved_uid != NULL)
		gtk_combo_box_set_active_id (gtk_combo_box, saved_uid);

	if (gtk_combo_box_get_active_id (gtk_combo_box) == NULL)
		gtk_combo_box_set_active (gtk_combo_box, 0);
}

ESourceRegistry *
e_mail_signature_combo_box_get_registry (EMailSignatureComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->registry;
}

const gchar *
e_mail_signature_combo_box_get_identity_uid (EMailSignatureComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->identity_uid;
}

void
e_mail_signature_combo_box_set_identity_uid (EMailSignatureComboBox *combo_box,
                                             const gchar *identity_uid)
{
	const gchar *active_id;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	if (g_strcmp0 (combo_box->priv->identity_uid, identity_uid) == 0)
		return;

	g_free (combo_box->priv->identity_uid);
	combo_box->priv->identity_uid = g_strdup (identity_uid);

	g_object_notify (G_OBJECT (combo_box), "identity-uid");

	/* If "Autogenerated" is selected, emit a "changed" signal as
	 * a hint to whomever is listening to reload the signature. */
	active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));
	if (g_strcmp0 (active_id, E_MAIL_SIGNATURE_AUTOGENERATED_UID) == 0)
		g_signal_emit_by_name (combo_box, "changed");
}

/**************** e_mail_signature_combo_box_load_selected() *****************/

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	gchar *contents;
	gsize length;
	gboolean is_html;
};

static void
load_context_free (LoadContext *context)
{
	g_free (context->contents);
	g_slice_free (LoadContext, context);
}

static void
mail_signature_combo_box_autogenerate (EMailSignatureComboBox *combo_box,
                                       LoadContext *context)
{
	ESourceMailIdentity *extension;
	ESourceRegistry *registry;
	ESource *source;
	GString *buffer;
	const gchar *extension_name;
	const gchar *identity_uid;
	const gchar *text;
	gchar *escaped;

	identity_uid = e_mail_signature_combo_box_get_identity_uid (combo_box);

	/* If we have no mail identity UID, handle it as though
	 * "None" were selected.  No need to report an error. */
	if (identity_uid == NULL)
		return;

	registry = e_mail_signature_combo_box_get_registry (combo_box);
	source = e_source_registry_ref_source (registry, identity_uid);

	/* If the mail identity lookup fails, handle it as though
	 * "None" were selected.  No need to report an error. */
	if (source == NULL)
		return;

	/* If the source is not actually a mail identity, handle it as
	 * though "None" were selected.  No need to report an error. */
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;
	if (!e_source_has_extension (source, extension_name)) {
		g_object_unref (source);
		return;
	}

	extension = e_source_get_extension (source, extension_name);

	/* The autogenerated signature format is:
	 *
	 *   <NAME> <ADDRESS>
	 *   <ORGANIZATION>
	 *
	 * The <ADDRESS> is a mailto link and
	 * the <ORGANIZATION> line is optional.
	 */

	buffer = g_string_sized_new (512);

	text = e_source_mail_identity_get_name (extension);
	escaped = (text != NULL) ? g_markup_escape_text (text, -1) : NULL;
	if (escaped != NULL && *escaped != '\0')
		g_string_append (buffer, escaped);
	g_free (escaped);

	text = e_source_mail_identity_get_address (extension);
	escaped = (text != NULL) ? g_markup_escape_text (text, -1) : NULL;
	if (escaped != NULL && *escaped != '\0')
		g_string_append_printf (
			buffer, " &lt;<a href=\"mailto:%s\">%s</a>&gt;",
			escaped, escaped);
	g_free (escaped);

	text = e_source_mail_identity_get_organization (extension);
	escaped = (text != NULL) ? g_markup_escape_text (text, -1) : NULL;
	if (escaped != NULL && *escaped != '\0')
		g_string_append_printf (buffer, "<br>%s", escaped);
	g_free (escaped);

	context->length = buffer->len;
	context->contents = g_string_free (buffer, FALSE);
	context->is_html = TRUE;

	g_object_unref (source);
}

static void
mail_signature_combo_box_load_cb (ESource *source,
                                  GAsyncResult *result,
                                  GSimpleAsyncResult *simple)
{
	ESourceMailSignature *extension;
	LoadContext *context;
	const gchar *extension_name;
	const gchar *mime_type;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	e_source_mail_signature_load_finish (
		source, result, &context->contents, &context->length, &error);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_simple_async_result_complete (simple);
		g_object_unref (simple);
		g_error_free (error);
		return;
	}

	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	extension = e_source_get_extension (source, extension_name);
	mime_type = e_source_mail_signature_get_mime_type (extension);
	context->is_html = (g_strcmp0 (mime_type, "text/html") == 0);

	g_simple_async_result_complete (simple);

	g_object_unref (simple);
}

void
e_mail_signature_combo_box_load_selected (EMailSignatureComboBox *combo_box,
                                          gint io_priority,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
	GSimpleAsyncResult *simple;
	ESourceRegistry *registry;
	LoadContext *context;
	ESource *source;
	const gchar *active_id;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	context = g_slice_new0 (LoadContext);

	simple = g_simple_async_result_new (
		G_OBJECT (combo_box), callback, user_data,
		e_mail_signature_combo_box_load_selected);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) load_context_free);

	active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));

	if (active_id == NULL) {
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		return;
	}

	if (g_strcmp0 (active_id, E_MAIL_SIGNATURE_AUTOGENERATED_UID) == 0) {
		mail_signature_combo_box_autogenerate (combo_box, context);
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		return;
	}

	registry = e_mail_signature_combo_box_get_registry (combo_box);
	source = e_source_registry_ref_source (registry, active_id);

	/* If for some reason the ESource lookup fails, handle it as
	 * though "None" were selected.  No need to report an error. */
	if (source == NULL) {
		g_simple_async_result_complete_in_idle (simple);
		g_object_unref (simple);
		return;
	}

	e_source_mail_signature_load (
		source, io_priority, cancellable, (GAsyncReadyCallback)
		mail_signature_combo_box_load_cb, simple);

	g_object_unref (source);
}

gboolean
e_mail_signature_combo_box_load_selected_finish (EMailSignatureComboBox *combo_box,
                                                 GAsyncResult *result,
                                                 gchar **contents,
                                                 gsize *length,
                                                 gboolean *is_html,
                                                 GError **error)
{
	GSimpleAsyncResult *simple;
	LoadContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (combo_box),
		e_mail_signature_combo_box_load_selected), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	if (contents != NULL) {
		*contents = context->contents;
		context->contents = NULL;
	}

	if (length != NULL)
		*length = context->length;

	if (is_html != NULL)
		*is_html = context->is_html;

	return TRUE;
}
