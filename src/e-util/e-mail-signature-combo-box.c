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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-mail-signature-combo-box.h"

#define SOURCE_IS_MAIL_SIGNATURE(source) \
	(e_source_has_extension ((source), E_SOURCE_EXTENSION_MAIL_SIGNATURE))

struct _EMailSignatureComboBoxPrivate {
	ESourceRegistry *registry;
	guint refresh_idle_id;
	gchar *identity_uid;
	gchar *identity_name;
	gchar *identity_address;
	gint max_natural_width;
	gint last_natural_width;
};

enum {
	PROP_0,
	PROP_IDENTITY_UID,
	PROP_IDENTITY_NAME,
	PROP_IDENTITY_ADDRESS,
	PROP_REGISTRY
};

enum {
	COLUMN_DISPLAY_NAME,
	COLUMN_UID
};

G_DEFINE_TYPE_WITH_PRIVATE (EMailSignatureComboBox, e_mail_signature_combo_box, GTK_TYPE_COMBO_BOX)

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
	source_object = g_binding_dup_source (binding);
	combo_box = E_MAIL_SIGNATURE_COMBO_BOX (source_object);
	registry = e_mail_signature_combo_box_get_registry (combo_box);
	g_clear_object (&source_object);

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
mail_signature_combo_box_get_preferred_width (GtkWidget *widget,
					      gint *minimum_width,
					      gint *natural_width)
{
	EMailSignatureComboBox *self = E_MAIL_SIGNATURE_COMBO_BOX (widget);

	GTK_WIDGET_CLASS (e_mail_signature_combo_box_parent_class)->get_preferred_width (widget, minimum_width, natural_width);

	self->priv->last_natural_width = *natural_width;

	if (self->priv->max_natural_width > 0) {
		if (*natural_width > self->priv->max_natural_width)
			*natural_width = self->priv->max_natural_width;
		if (*minimum_width > *natural_width)
			*minimum_width = *natural_width;
	}
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

		case PROP_IDENTITY_NAME:
			e_mail_signature_combo_box_set_identity_name (
				E_MAIL_SIGNATURE_COMBO_BOX (object),
				g_value_get_string (value));
			return;

		case PROP_IDENTITY_ADDRESS:
			e_mail_signature_combo_box_set_identity_address (
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

		case PROP_IDENTITY_NAME:
			g_value_set_string (
				value,
				e_mail_signature_combo_box_get_identity_name (
				E_MAIL_SIGNATURE_COMBO_BOX (object)));
			return;

		case PROP_IDENTITY_ADDRESS:
			g_value_set_string (
				value,
				e_mail_signature_combo_box_get_identity_address (
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
	EMailSignatureComboBox *self = E_MAIL_SIGNATURE_COMBO_BOX (object);

	if (self->priv->registry != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->registry, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->registry);
	}

	if (self->priv->refresh_idle_id > 0) {
		g_source_remove (self->priv->refresh_idle_id);
		self->priv->refresh_idle_id = 0;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_signature_combo_box_parent_class)->dispose (object);
}

static void
mail_signature_combo_box_finalize (GObject *object)
{
	EMailSignatureComboBox *self = E_MAIL_SIGNATURE_COMBO_BOX (object);

	g_free (self->priv->identity_uid);
	g_free (self->priv->identity_name);
	g_free (self->priv->identity_address);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_signature_combo_box_parent_class)->finalize (object);
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
	g_object_set (cell_renderer,
		"ellipsize", PANGO_ELLIPSIZE_END,
		NULL);
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
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_signature_combo_box_set_property;
	object_class->get_property = mail_signature_combo_box_get_property;
	object_class->dispose = mail_signature_combo_box_dispose;
	object_class->finalize = mail_signature_combo_box_finalize;
	object_class->constructed = mail_signature_combo_box_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->get_preferred_width = mail_signature_combo_box_get_preferred_width;

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
		PROP_IDENTITY_NAME,
		g_param_spec_string (
			"identity-name",
			"Identity Name",
			NULL,
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_IDENTITY_ADDRESS,
		g_param_spec_string (
			"identity-address",
			"Identity Address",
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
	combo_box->priv = e_mail_signature_combo_box_get_instance_private (combo_box);
	combo_box->priv->max_natural_width = 100;
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
		GtkTreeIter titer;
		const gchar *display_name;
		const gchar *uid;

		source = E_SOURCE (link->data);
		display_name = e_source_get_display_name (source);
		uid = e_source_get_uid (source);

		gtk_list_store_append (GTK_LIST_STORE (tree_model), &titer);

		gtk_list_store_set (
			GTK_LIST_STORE (tree_model), &titer,
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

static void
mail_signature_combo_box_emit_changed_for_autogenerated (EMailSignatureComboBox *combo_box)
{
	const gchar *active_id;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	/* If "Autogenerated" is selected, emit a "changed" signal as
	 * a hint to whomever is listening to reload the signature. */
	active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));
	if (g_strcmp0 (active_id, E_MAIL_SIGNATURE_AUTOGENERATED_UID) == 0)
		g_signal_emit_by_name (combo_box, "changed");
}

static void
mail_signature_combo_box_set_identity_uid (EMailSignatureComboBox *combo_box,
					   const gchar *identity_uid,
					   gboolean can_emit_changed)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	if (g_strcmp0 (combo_box->priv->identity_uid, identity_uid) == 0)
		return;

	g_free (combo_box->priv->identity_uid);
	combo_box->priv->identity_uid = g_strdup (identity_uid);

	g_object_notify (G_OBJECT (combo_box), "identity-uid");

	if (can_emit_changed)
		mail_signature_combo_box_emit_changed_for_autogenerated (combo_box);
}

void
e_mail_signature_combo_box_set_identity_uid (EMailSignatureComboBox *combo_box,
                                             const gchar *identity_uid)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	mail_signature_combo_box_set_identity_uid (combo_box, identity_uid, TRUE);
}

const gchar *
e_mail_signature_combo_box_get_identity_name (EMailSignatureComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->identity_name;
}

static void
mail_signature_combo_box_set_identity_name (EMailSignatureComboBox *combo_box,
					    const gchar *identity_name,
					    gboolean can_emit_changed)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	if (g_strcmp0 (combo_box->priv->identity_name, identity_name) == 0)
		return;

	g_free (combo_box->priv->identity_name);
	combo_box->priv->identity_name = g_strdup (identity_name);

	g_object_notify (G_OBJECT (combo_box), "identity-name");

	if (can_emit_changed)
		mail_signature_combo_box_emit_changed_for_autogenerated (combo_box);
}

void
e_mail_signature_combo_box_set_identity_name (EMailSignatureComboBox *combo_box,
					      const gchar *identity_name)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	mail_signature_combo_box_set_identity_name (combo_box, identity_name, TRUE);
}

const gchar *
e_mail_signature_combo_box_get_identity_address (EMailSignatureComboBox *combo_box)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box), NULL);

	return combo_box->priv->identity_address;
}

static void
mail_signature_combo_box_set_identity_address (EMailSignatureComboBox *combo_box,
					       const gchar *identity_address,
					       gboolean can_emit_changed)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	if (g_strcmp0 (combo_box->priv->identity_address, identity_address) == 0)
		return;

	g_free (combo_box->priv->identity_address);
	combo_box->priv->identity_address = g_strdup (identity_address);

	g_object_notify (G_OBJECT (combo_box), "identity-address");

	if (can_emit_changed)
		mail_signature_combo_box_emit_changed_for_autogenerated (combo_box);
}

void
e_mail_signature_combo_box_set_identity_address (EMailSignatureComboBox *combo_box,
						 const gchar *identity_address)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	mail_signature_combo_box_set_identity_address (combo_box, identity_address, TRUE);
}

void
e_mail_signature_combo_box_set_identity (EMailSignatureComboBox *combo_box,
					 const gchar *identity_uid,
					 const gchar *identity_name,
					 const gchar *identity_address)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	g_object_freeze_notify (G_OBJECT (combo_box));

	mail_signature_combo_box_set_identity_uid (combo_box, identity_uid, FALSE);
	mail_signature_combo_box_set_identity_name (combo_box, identity_name, FALSE);
	mail_signature_combo_box_set_identity_address (combo_box, identity_address, FALSE);

	g_object_thaw_notify (G_OBJECT (combo_box));

	mail_signature_combo_box_emit_changed_for_autogenerated (combo_box);
}

gint
e_mail_signature_combo_box_get_max_natural_width (EMailSignatureComboBox *self)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (self), -1);

	return self->priv->max_natural_width;
}

void
e_mail_signature_combo_box_set_max_natural_width (EMailSignatureComboBox *self,
						  gint value)
{
	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (self));

	if (self->priv->max_natural_width != value) {
		self->priv->max_natural_width = value;
		gtk_widget_queue_resize (GTK_WIDGET (self));
	}
}

gint
e_mail_signature_combo_box_get_last_natural_width (EMailSignatureComboBox *self)
{
	g_return_val_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (self), -1);

	return self->priv->last_natural_width;
}

/**************** e_mail_signature_combo_box_load_selected() *****************/

typedef struct _LoadContext LoadContext;

struct _LoadContext {
	gchar *contents;
	gsize length;
	EContentEditorMode editor_mode;
};

static void
load_context_free (LoadContext *context)
{
	g_clear_pointer (&context->contents, g_free);
	g_free (context);
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
	const gchar *identity_name;
	const gchar *identity_address;
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

	identity_name = e_mail_signature_combo_box_get_identity_name (combo_box);
	identity_address = e_mail_signature_combo_box_get_identity_address (combo_box);

	if (identity_address && !*identity_address)
		identity_address = NULL;

	text = (identity_address && identity_name && *identity_name) ? identity_name : e_source_mail_identity_get_name (extension);
	escaped = (text != NULL) ? g_markup_escape_text (text, -1) : NULL;
	if (escaped != NULL && *escaped != '\0')
		g_string_append (buffer, escaped);
	g_free (escaped);

	text = identity_address ? identity_address : e_source_mail_identity_get_address (extension);
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
	context->editor_mode = E_CONTENT_EDITOR_MODE_HTML;

	g_object_unref (source);
}

static void
mail_signature_combo_box_load_cb (GObject *source_object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	ESource *source;
	GTask *task;
	ESourceMailSignature *extension;
	LoadContext *context;
	const gchar *extension_name;
	const gchar *mime_type;
	GError *error = NULL;

	source = E_SOURCE (source_object);
	task = G_TASK (user_data);
	context = g_new0 (LoadContext, 1);
	e_source_mail_signature_load_finish (
		source, result, &context->contents, &context->length, &error);

	if (error != NULL) {
		g_clear_pointer (&context, load_context_free);
		g_task_return_error (task, g_steal_pointer (&error));
		g_object_unref (task);
		return;
	}

	extension_name = E_SOURCE_EXTENSION_MAIL_SIGNATURE;
	extension = e_source_get_extension (source, extension_name);
	mime_type = e_source_mail_signature_get_mime_type (extension);

	if (g_strcmp0 (mime_type, "text/html") == 0)
		context->editor_mode = E_CONTENT_EDITOR_MODE_HTML;
	else if (g_strcmp0 (mime_type, "text/markdown") == 0)
		context->editor_mode = E_CONTENT_EDITOR_MODE_MARKDOWN;
	else if (g_strcmp0 (mime_type, "text/markdown-plain") == 0)
		context->editor_mode = E_CONTENT_EDITOR_MODE_MARKDOWN_PLAIN_TEXT;
	else if (g_strcmp0 (mime_type, "text/markdown-html") == 0)
		context->editor_mode = E_CONTENT_EDITOR_MODE_MARKDOWN_HTML;
	else
		context->editor_mode = E_CONTENT_EDITOR_MODE_PLAIN_TEXT;

	g_task_return_pointer (task, g_steal_pointer (&context), (GDestroyNotify) load_context_free);

	g_object_unref (task);
}

void
e_mail_signature_combo_box_load_selected (EMailSignatureComboBox *combo_box,
                                          gint io_priority,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer user_data)
{
	GTask *task;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *active_id;

	g_return_if_fail (E_IS_MAIL_SIGNATURE_COMBO_BOX (combo_box));

	task = g_task_new (combo_box, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_signature_combo_box_load_selected);

	active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo_box));

	if (active_id == NULL) {
		LoadContext *context = g_new0 (LoadContext, 1);
		g_task_return_pointer (task, context, (GDestroyNotify) load_context_free);
		g_object_unref (task);
		return;
	}

	if (g_strcmp0 (active_id, E_MAIL_SIGNATURE_AUTOGENERATED_UID) == 0) {
		LoadContext *context = g_new0 (LoadContext, 1);
		mail_signature_combo_box_autogenerate (combo_box, context);
		g_task_return_pointer (task, context, (GDestroyNotify) load_context_free);
		g_object_unref (task);
		return;
	}

	registry = e_mail_signature_combo_box_get_registry (combo_box);
	source = e_source_registry_ref_source (registry, active_id);

	/* If for some reason the ESource lookup fails, handle it as
	 * though "None" were selected.  No need to report an error. */
	if (source == NULL) {
		LoadContext *context = g_new0 (LoadContext, 1);
		g_task_return_pointer (task, context, (GDestroyNotify) load_context_free);
		g_object_unref (task);
		return;
	}

	e_source_mail_signature_load (
		source, io_priority, cancellable,
		mail_signature_combo_box_load_cb, g_steal_pointer (&task));

	g_object_unref (source);
}

gboolean
e_mail_signature_combo_box_load_selected_finish (EMailSignatureComboBox *combo_box,
                                                 GAsyncResult *result,
                                                 gchar **contents,
                                                 gsize *length,
                                                 EContentEditorMode *out_editor_mode,
                                                 GError **error)
{
	LoadContext *context;

	g_return_val_if_fail (g_task_is_valid (result, combo_box), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_signature_combo_box_load_selected), FALSE);

	context = g_task_propagate_pointer (G_TASK (result), error);
	if (!context)
		return FALSE;

	if (contents != NULL)
		*contents = g_steal_pointer (&context->contents);

	if (length != NULL)
		*length = context->length;

	if (out_editor_mode)
		*out_editor_mode = context->editor_mode;

	g_clear_pointer (&context, load_context_free);
	return TRUE;
}
