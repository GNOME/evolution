/*
 * e-source-config.c
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

#include "e-source-config.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include "e-interval-chooser.h"
#include "e-marshal.h"
#include "e-misc-utils.h"
#include "e-source-config-backend.h"

#define E_SOURCE_CONFIG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SOURCE_CONFIG, ESourceConfigPrivate))

typedef struct _Candidate Candidate;

struct _ESourceConfigPrivate {
	ESource *original_source;
	ESource *collection_source;
	ESourceRegistry *registry;

	GHashTable *backends;
	GPtrArray *candidates;

	GtkWidget *type_label;
	GtkWidget *type_combo;
	GtkWidget *name_label;
	GtkWidget *name_entry;
	GtkWidget *backend_box;
	GtkSizeGroup *size_group;

	gboolean complete;
};

struct _Candidate {
	GtkWidget *page;
	ESource *scratch_source;
	ESourceConfigBackend *backend;
	gulong changed_handler_id;
};

enum {
	PROP_0,
	PROP_COLLECTION_SOURCE,
	PROP_COMPLETE,
	PROP_ORIGINAL_SOURCE,
	PROP_REGISTRY
};

enum {
	CHECK_COMPLETE,
	COMMIT_CHANGES,
	INIT_CANDIDATE,
	RESIZE_WINDOW,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (
	ESourceConfig,
	e_source_config,
	GTK_TYPE_BOX,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
source_config_init_backends (ESourceConfig *config)
{
	GList *list, *iter;

	config->priv->backends = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	e_extensible_load_extensions (E_EXTENSIBLE (config));

	list = e_extensible_list_extensions (
		E_EXTENSIBLE (config), E_TYPE_SOURCE_CONFIG_BACKEND);

	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		ESourceConfigBackend *backend;
		ESourceConfigBackendClass *class;

		backend = E_SOURCE_CONFIG_BACKEND (iter->data);
		class = E_SOURCE_CONFIG_BACKEND_GET_CLASS (backend);

		if (class->backend_name != NULL)
			g_hash_table_insert (
				config->priv->backends,
				g_strdup (class->backend_name),
				g_object_ref (backend));
	}

	g_list_free (list);
}

static gint
source_config_compare_sources (gconstpointer a,
                               gconstpointer b,
                               gpointer user_data)
{
	ESource *source_a;
	ESource *source_b;
	ESource *parent_a;
	ESource *parent_b;
	ESourceConfig *config;
	ESourceRegistry *registry;
	const gchar *parent_uid_a;
	const gchar *parent_uid_b;
	gint result;

	source_a = E_SOURCE (a);
	source_b = E_SOURCE (b);
	config = E_SOURCE_CONFIG (user_data);

	if (e_source_equal (source_a, source_b))
		return 0;

	/* "On This Computer" always comes first. */

	parent_uid_a = e_source_get_parent (source_a);
	parent_uid_b = e_source_get_parent (source_b);

	if (g_strcmp0 (parent_uid_a, "local-stub") == 0)
		return -1;

	if (g_strcmp0 (parent_uid_b, "local-stub") == 0)
		return 1;

	registry = e_source_config_get_registry (config);

	parent_a = e_source_registry_ref_source (registry, parent_uid_a);
	parent_b = e_source_registry_ref_source (registry, parent_uid_b);

	g_return_val_if_fail (parent_a != NULL, 1);
	g_return_val_if_fail (parent_b != NULL, -1);

	result = e_source_compare_by_display_name (parent_a, parent_b);

	g_object_unref (parent_a);
	g_object_unref (parent_b);

	return result;
}

static void
source_config_add_candidate (ESourceConfig *config,
                             ESource *scratch_source,
                             ESourceConfigBackend *backend)
{
	Candidate *candidate;
	GtkBox *backend_box;
	GtkLabel *type_label;
	GtkComboBoxText *type_combo;
	GtkWidget *widget;
	ESource *parent_source;
	ESourceRegistry *registry;
	const gchar *display_name;
	const gchar *parent_uid;
	gulong handler_id;

	backend_box = GTK_BOX (config->priv->backend_box);
	type_label = GTK_LABEL (config->priv->type_label);
	type_combo = GTK_COMBO_BOX_TEXT (config->priv->type_combo);

	registry = e_source_config_get_registry (config);
	parent_uid = e_source_get_parent (scratch_source);
	parent_source = e_source_registry_ref_source (registry, parent_uid);
	g_return_if_fail (parent_source != NULL);

	candidate = g_slice_new (Candidate);
	candidate->backend = g_object_ref (backend);
	candidate->scratch_source = g_object_ref (scratch_source);

	/* Do not show the page here. */
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (backend_box, widget, FALSE, FALSE, 0);
	candidate->page = g_object_ref_sink (widget);

	g_ptr_array_add (config->priv->candidates, candidate);

	display_name = e_source_get_display_name (parent_source);
	gtk_combo_box_text_append_text (type_combo, display_name);
	gtk_label_set_text (type_label, display_name);

	/* Make sure the combo box has a valid active item before
	 * adding widgets.  Otherwise we'll get run-time warnings
	 * as property bindings are set up. */
	if (gtk_combo_box_get_active (GTK_COMBO_BOX (type_combo)) == -1)
		gtk_combo_box_set_active (GTK_COMBO_BOX (type_combo), 0);

	/* Bind the standard widgets to the new scratch source. */
	g_signal_emit (
		config, signals[INIT_CANDIDATE], 0,
		candidate->scratch_source);

	/* Insert any backend-specific widgets. */
	e_source_config_backend_insert_widgets (
		candidate->backend, candidate->scratch_source);

	handler_id = g_signal_connect_swapped (
		candidate->scratch_source, "changed",
		G_CALLBACK (e_source_config_check_complete), config);

	candidate->changed_handler_id = handler_id;

	/* Trigger the "changed" handler we just connected to set the
	 * initial "complete" state based on the widgets we just added. */
	e_source_changed (candidate->scratch_source);

	g_object_unref (parent_source);
}

static void
source_config_free_candidate (Candidate *candidate)
{
	g_signal_handler_disconnect (
		candidate->scratch_source,
		candidate->changed_handler_id);

	g_object_unref (candidate->page);
	g_object_unref (candidate->scratch_source);
	g_object_unref (candidate->backend);

	g_slice_free (Candidate, candidate);
}

static Candidate *
source_config_get_active_candidate (ESourceConfig *config)
{
	GtkComboBox *type_combo;
	gint index;

	type_combo = GTK_COMBO_BOX (config->priv->type_combo);
	index = gtk_combo_box_get_active (type_combo);
	g_return_val_if_fail (index >= 0, NULL);

	return g_ptr_array_index (config->priv->candidates, index);
}

static void
source_config_type_combo_changed_cb (GtkComboBox *type_combo,
                                     ESourceConfig *config)
{
	Candidate *candidate;
	GPtrArray *array;
	gint index;

	array = config->priv->candidates;

	for (index = 0; index < array->len; index++) {
		candidate = g_ptr_array_index (array, index);
		gtk_widget_hide (candidate->page);
	}

	index = gtk_combo_box_get_active (type_combo);
	if (index == CLAMP (index, 0, array->len)) {
		candidate = g_ptr_array_index (array, index);
		gtk_widget_show (candidate->page);
	}

	e_source_config_resize_window (config);
	e_source_config_check_complete (config);
}

static gboolean
source_config_init_for_adding_source_foreach (gpointer key,
                                              gpointer value,
                                              gpointer user_data)
{
	ESource *scratch_source;
	ESourceBackend *extension;
	ESourceConfig *config;
	ESourceConfigBackend *backend;
	ESourceConfigBackendClass *class;
	const gchar *extension_name;

	scratch_source = E_SOURCE (key);
	backend = E_SOURCE_CONFIG_BACKEND (value);
	config = E_SOURCE_CONFIG (user_data);

	/* This may not be the correct backend name for the child of a
	 * collection.  For example, the "yahoo" collection backend uses
	 * the "caldav" calender backend for calendar children.  But the
	 * ESourceCollectionBackend can override our setting if needed. */
	class = E_SOURCE_CONFIG_BACKEND_GET_CLASS (backend);
	extension_name = e_source_config_get_backend_extension_name (config);
	extension = e_source_get_extension (scratch_source, extension_name);
	e_source_backend_set_backend_name (extension, class->backend_name);

	source_config_add_candidate (config, scratch_source, backend);

	return FALSE;  /* don't stop traversal */
}

static void
source_config_init_for_adding_source (ESourceConfig *config)
{
	GList *list, *link;
	ESourceRegistry *registry;
	GTree *scratch_source_tree;

	/* Candidates are drawn from two sources:
	 *
	 * ESourceConfigBackend classes that specify a fixed parent UID,
	 * meaning there exists one only possible parent source for any
	 * scratch source created by the backend.  The fixed parent UID
	 * should be a built-in "stub" placeholder ("local-stub", etc).
	 *
	 * -and-
	 *
	 * Collection sources.  We let ESourceConfig subclasses gather
	 * eligible collection sources to serve as parents for scratch
	 * sources.  A scratch source is matched to a backend based on
	 * the collection's backend name.  The "calendar-enabled" and
	 * "contacts-enabled" settings also factor into eligibility.
	 */

	/* Use a GTree instead of a GHashTable so inserted scratch
	 * sources automatically sort themselves by their parent's
	 * display name. */
	scratch_source_tree = g_tree_new_full (
		source_config_compare_sources, config,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) g_object_unref);

	registry = e_source_config_get_registry (config);

	/* First pick out the backends with a fixed parent UID. */

	list = g_hash_table_get_values (config->priv->backends);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESourceConfigBackend *backend;
		ESourceConfigBackendClass *class;
		ESource *scratch_source;
		ESource *parent_source;
		gboolean parent_is_disabled;

		backend = E_SOURCE_CONFIG_BACKEND (link->data);
		class = E_SOURCE_CONFIG_BACKEND_GET_CLASS (backend);

		if (class->parent_uid == NULL)
			continue;

		/* Verify the fixed parent UID is valid. */
		parent_source = e_source_registry_ref_source (
			registry, class->parent_uid);
		if (parent_source == NULL) {
			g_warning (
				"%s: %sClass specifies "
				"an invalid parent_uid '%s'",
				G_STRFUNC,
				G_OBJECT_TYPE_NAME (backend),
				class->parent_uid);
			continue;
		}
		parent_is_disabled = !e_source_get_enabled (parent_source);
		g_object_unref (parent_source);

		/* It's unusual for a fixed parent source to be disabled.
		 * A user would have to go out of his way to do this, but
		 * we should honor it regardless. */
		if (parent_is_disabled)
			continue;

		/* Some backends don't allow new sources to be created.
		 * The "contacts" calendar backend is one such example. */
		if (!e_source_config_backend_allow_creation (backend))
			continue;

		scratch_source = e_source_new (NULL, NULL, NULL);
		g_return_if_fail (scratch_source != NULL);

		e_source_set_parent (scratch_source, class->parent_uid);

		g_tree_insert (
			scratch_source_tree,
			g_object_ref (scratch_source),
			g_object_ref (backend));

		g_object_unref (scratch_source);
	}

	g_list_free (list);

	/* Next gather eligible collection sources to serve as parents. */

	list = e_source_config_list_eligible_collections (config);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *parent_source;
		ESource *scratch_source;
		ESourceBackend *extension;
		ESourceConfigBackend *backend = NULL;
		const gchar *backend_name;
		const gchar *parent_uid;

		parent_source = E_SOURCE (link->data);
		parent_uid = e_source_get_uid (parent_source);

		extension = e_source_get_extension (
			parent_source, E_SOURCE_EXTENSION_COLLECTION);
		backend_name = e_source_backend_get_backend_name (extension);

		if (backend_name != NULL)
			backend = g_hash_table_lookup (
				config->priv->backends, backend_name);

		if (backend == NULL)
			continue;

		/* Some backends disallow creating certain types of
		 * resources.  For example, the Exchange Web Services
		 * backend disallows creating new memo lists. */
		if (!e_source_config_backend_allow_creation (backend))
			continue;

		scratch_source = e_source_new (NULL, NULL, NULL);
		g_return_if_fail (scratch_source != NULL);

		e_source_set_parent (scratch_source, parent_uid);

		g_tree_insert (
			scratch_source_tree,
			g_object_ref (scratch_source),
			g_object_ref (backend));

		g_object_unref (scratch_source);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* XXX GTree doesn't get as much love as GHashTable.
	 *     It's missing an equivalent to GHashTableIter. */
	g_tree_foreach (
		scratch_source_tree,
		source_config_init_for_adding_source_foreach, config);

	g_tree_unref (scratch_source_tree);
}

static void
source_config_init_for_editing_source (ESourceConfig *config)
{
	ESource *original_source;
	ESource *scratch_source;
	ESourceBackend *extension;
	ESourceConfigBackend *backend;
	GDBusObject *dbus_object;
	const gchar *backend_name;
	const gchar *extension_name;

	original_source = e_source_config_get_original_source (config);
	g_return_if_fail (original_source != NULL);

	extension_name = e_source_config_get_backend_extension_name (config);
	extension = e_source_get_extension (original_source, extension_name);
	backend_name = e_source_backend_get_backend_name (extension);
	g_return_if_fail (backend_name != NULL);

	/* Special-case Google calendars to use 'google' editor, instead
	   of the 'caldav' editor, even they use 'caldav' calendar backend. */
	if (g_ascii_strcasecmp (backend_name, "caldav") == 0 &&
	    g_strcmp0 (e_source_get_parent (original_source), "google-stub") == 0)
		backend_name = "google";

	backend = g_hash_table_lookup (config->priv->backends, backend_name);
	g_return_if_fail (backend != NULL);

	dbus_object = e_source_ref_dbus_object (original_source);
	g_return_if_fail (dbus_object != NULL);

	scratch_source = e_source_new (dbus_object, NULL, NULL);
	g_return_if_fail (scratch_source != NULL);

	source_config_add_candidate (config, scratch_source, backend);

	g_object_unref (scratch_source);
	g_object_unref (dbus_object);
}

static void
source_config_set_original_source (ESourceConfig *config,
                                   ESource *original_source)
{
	g_return_if_fail (config->priv->original_source == NULL);

	if (original_source != NULL)
		g_object_ref (original_source);

	config->priv->original_source = original_source;
}

static void
source_config_set_registry (ESourceConfig *config,
                            ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (config->priv->registry == NULL);

	config->priv->registry = g_object_ref (registry);
}

static void
source_config_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ORIGINAL_SOURCE:
			source_config_set_original_source (
				E_SOURCE_CONFIG (object),
				g_value_get_object (value));
			return;

		case PROP_REGISTRY:
			source_config_set_registry (
				E_SOURCE_CONFIG (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_config_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_COLLECTION_SOURCE:
			g_value_set_object (
				value,
				e_source_config_get_collection_source (
				E_SOURCE_CONFIG (object)));
			return;

		case PROP_COMPLETE:
			g_value_set_boolean (
				value,
				e_source_config_check_complete (
				E_SOURCE_CONFIG (object)));
			return;

		case PROP_ORIGINAL_SOURCE:
			g_value_set_object (
				value,
				e_source_config_get_original_source (
				E_SOURCE_CONFIG (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_source_config_get_registry (
				E_SOURCE_CONFIG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
source_config_dispose (GObject *object)
{
	ESourceConfigPrivate *priv;

	priv = E_SOURCE_CONFIG_GET_PRIVATE (object);

	if (priv->original_source != NULL) {
		g_object_unref (priv->original_source);
		priv->original_source = NULL;
	}

	if (priv->collection_source != NULL) {
		g_object_unref (priv->collection_source);
		priv->collection_source = NULL;
	}

	if (priv->registry != NULL) {
		g_object_unref (priv->registry);
		priv->registry = NULL;
	}

	if (priv->type_label != NULL) {
		g_object_unref (priv->type_label);
		priv->type_label = NULL;
	}

	if (priv->type_combo != NULL) {
		g_object_unref (priv->type_combo);
		priv->type_combo = NULL;
	}

	if (priv->name_label != NULL) {
		g_object_unref (priv->name_label);
		priv->name_label = NULL;
	}

	if (priv->name_entry != NULL) {
		g_object_unref (priv->name_entry);
		priv->name_entry = NULL;
	}

	if (priv->backend_box != NULL) {
		g_object_unref (priv->backend_box);
		priv->backend_box = NULL;
	}

	if (priv->size_group != NULL) {
		g_object_unref (priv->size_group);
		priv->size_group = NULL;
	}

	g_hash_table_remove_all (priv->backends);
	g_ptr_array_set_size (priv->candidates, 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_source_config_parent_class)->dispose (object);
}

static void
source_config_finalize (GObject *object)
{
	ESourceConfigPrivate *priv;

	priv = E_SOURCE_CONFIG_GET_PRIVATE (object);

	g_hash_table_destroy (priv->backends);
	g_ptr_array_free (priv->candidates, TRUE);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_source_config_parent_class)->finalize (object);
}

static void
source_config_constructed (GObject *object)
{
	ESourceConfig *config;
	ESourceRegistry *registry;
	ESource *original_source;
	ESource *collection_source = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_source_config_parent_class)->constructed (object);

	config = E_SOURCE_CONFIG (object);
	registry = e_source_config_get_registry (config);
	original_source = e_source_config_get_original_source (config);

	/* If we have an original source, see if it's part
	 * of a collection and note the collection source. */
	if (original_source != NULL) {
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_COLLECTION;
		collection_source = e_source_registry_find_extension (
			registry, original_source, extension_name);
		config->priv->collection_source = collection_source;
	}

	if (original_source != NULL)
		e_source_config_insert_widget (
			config, NULL, _("Type:"),
			config->priv->type_label);
	else
		e_source_config_insert_widget (
			config, NULL, _("Type:"),
			config->priv->type_combo);

	/* If the original source is part of a collection then we assume
	 * the display name is server-assigned and not user-assigned, at
	 * least not assigned through Evolution. */
	if (collection_source != NULL)
		e_source_config_insert_widget (
			config, NULL, _("Name:"),
			config->priv->name_label);
	else
		e_source_config_insert_widget (
			config, NULL, _("Name:"),
			config->priv->name_entry);

	source_config_init_backends (config);
}

static void
source_config_realize (GtkWidget *widget)
{
	ESourceConfig *config;
	ESource *original_source;

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_source_config_parent_class)->realize (widget);

	/* Do this after constructed() so subclasses can fully
	 * initialize themselves before we add candidates. */

	config = E_SOURCE_CONFIG (widget);
	original_source = e_source_config_get_original_source (config);

	if (original_source == NULL)
		source_config_init_for_adding_source (config);
	else
		source_config_init_for_editing_source (config);

	/* Connect this signal AFTER we're done adding candidates
	 * so we don't trigger check_complete() before candidates
	 * have a chance to insert widgets. */
	g_signal_connect (
		config->priv->type_combo, "changed",
		G_CALLBACK (source_config_type_combo_changed_cb), config);

	/* Trigger the callback to make sure the right page
	 * is selected, window is an appropriate size, etc. */
	g_signal_emit_by_name (config->priv->type_combo, "changed");
}

static GList *
source_config_list_eligible_collections (ESourceConfig *config)
{
	ESourceRegistry *registry;
	GQueue trash = G_QUEUE_INIT;
	GList *list, *link;
	const gchar *extension_name;

	extension_name = E_SOURCE_EXTENSION_COLLECTION;
	registry = e_source_config_get_registry (config);

	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		gboolean elligible;

		elligible =
			e_source_get_enabled (source) &&
			e_source_get_remote_creatable (source);

		if (!elligible)
			g_queue_push_tail (&trash, link);
	}

	/* Remove ineligible collections from the list. */
	while ((link = g_queue_pop_head (&trash)) != NULL) {
		g_object_unref (link->data);
		list = g_list_delete_link (list, link);
	}

	return list;
}

static void
source_config_init_candidate (ESourceConfig *config,
                              ESource *scratch_source)
{
	e_binding_bind_object_text_property (
		scratch_source, "display-name",
		config->priv->name_label, "label",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_object_text_property (
		scratch_source, "display-name",
		config->priv->name_entry, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}

static gboolean
source_config_check_complete (ESourceConfig *config,
                              ESource *scratch_source)
{
	GtkEntry *name_entry;
	GtkComboBox *type_combo;
	const gchar *text;
	gboolean correct;

	/* Make sure the Type: combo box has a valid item. */
	type_combo = GTK_COMBO_BOX (config->priv->type_combo);
	if (gtk_combo_box_get_active (type_combo) < 0)
		return FALSE;

	/* Make sure the Name: entry field is not empty. */
	name_entry = GTK_ENTRY (config->priv->name_entry);
	text = gtk_entry_get_text (name_entry);
	correct = text != NULL && *text != '\0';

	e_util_set_entry_issue_hint (config->priv->name_entry, correct ? NULL : _("Name cannot be empty"));

	return correct;
}

static void
source_config_commit_changes (ESourceConfig *config,
                              ESource *scratch_source)
{
	/* Placeholder so subclasses can safely chain up. */
}

static void
source_config_resize_window (ESourceConfig *config)
{
	GtkWidget *toplevel;

	/* Expand or shrink our parent window vertically to accommodate
	 * the newly selected backend's options.  Some backends have tons
	 * of options, some have few.  This avoids the case where you
	 * select a backend with tons of options and then a backend with
	 * few options and wind up with lots of unused vertical space. */

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (config));

	if (GTK_IS_WINDOW (toplevel)) {
		GtkWindow *window = GTK_WINDOW (toplevel);
		GtkAllocation allocation;

		gtk_widget_get_allocation (toplevel, &allocation);
		gtk_window_resize (window, allocation.width, 1);
	}
}

static gboolean
source_config_check_complete_accumulator (GSignalInvocationHint *ihint,
                                          GValue *return_accu,
                                          const GValue *handler_return,
                                          gpointer unused)
{
	gboolean v_boolean;

	/* Abort emission if a handler returns FALSE. */
	v_boolean = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accu, v_boolean);

	return v_boolean;
}

static void
e_source_config_class_init (ESourceConfigClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	g_type_class_add_private (class, sizeof (ESourceConfigPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = source_config_set_property;
	object_class->get_property = source_config_get_property;
	object_class->dispose = source_config_dispose;
	object_class->finalize = source_config_finalize;
	object_class->constructed = source_config_constructed;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = source_config_realize;

	class->list_eligible_collections =
		source_config_list_eligible_collections;
	class->init_candidate = source_config_init_candidate;
	class->check_complete = source_config_check_complete;
	class->commit_changes = source_config_commit_changes;
	class->resize_window = source_config_resize_window;

	g_object_class_install_property (
		object_class,
		PROP_COLLECTION_SOURCE,
		g_param_spec_object (
			"collection-source",
			"Collection Source",
			"The collection ESource to which "
			"the ESource being edited belongs",
			E_TYPE_SOURCE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_COMPLETE,
		g_param_spec_boolean (
			"complete",
			"Complete",
			"Are the required fields complete?",
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_ORIGINAL_SOURCE,
		g_param_spec_object (
			"original-source",
			"Original Source",
			"The original ESource",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Registry of ESources",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[CHECK_COMPLETE] = g_signal_new (
		"check-complete",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceConfigClass, check_complete),
		source_config_check_complete_accumulator, NULL,
		e_marshal_BOOLEAN__OBJECT,
		G_TYPE_BOOLEAN, 1,
		E_TYPE_SOURCE);

	signals[COMMIT_CHANGES] = g_signal_new (
		"commit-changes",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceConfigClass, commit_changes),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SOURCE);

	signals[INIT_CANDIDATE] = g_signal_new (
		"init-candidate",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceConfigClass, init_candidate),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SOURCE);

	signals[RESIZE_WINDOW] = g_signal_new (
		"resize-window",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ESourceConfigClass, resize_window),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_source_config_init (ESourceConfig *config)
{
	GPtrArray *candidates;
	GtkSizeGroup *size_group;
	PangoAttribute *attr;
	PangoAttrList *attr_list;
	GtkWidget *widget;

	/* The candidates array holds scratch ESources, one for each
	 * item in the "type" combo box.  Scratch ESources are never
	 * added to the registry, so backend extensions can make any
	 * changes they want to them.  Whichever scratch ESource is
	 * "active" (selected in the "type" combo box) when the user
	 * clicks OK wins and is written to disk.  The others are
	 * discarded. */
	candidates = g_ptr_array_new_with_free_func (
		(GDestroyNotify) source_config_free_candidate);

	/* The size group is used for caption labels. */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_box_set_spacing (GTK_BOX (config), 6);

	gtk_orientable_set_orientation (
		GTK_ORIENTABLE (config), GTK_ORIENTATION_VERTICAL);

	config->priv = E_SOURCE_CONFIG_GET_PRIVATE (config);
	config->priv->candidates = candidates;
	config->priv->size_group = size_group;

	attr_list = pango_attr_list_new ();

	attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	pango_attr_list_insert (attr_list, attr);

	/* Either the source type combo box or the label is shown,
	 * never both.  But we create both widgets and keep them
	 * both up-to-date because it makes the logic simpler. */

	widget = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_attributes (GTK_LABEL (widget), attr_list);
	config->priv->type_label = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = gtk_combo_box_text_new ();
	config->priv->type_combo = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	/* Similarly for the display name.  Either the text entry
	 * or the label is shown, depending on whether the source
	 * is a collection member (new sources never are). */

	widget = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_label_set_attributes (GTK_LABEL (widget), attr_list);
	config->priv->name_label = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	widget = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (widget), TRUE);
	config->priv->name_entry = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	/* The backend box holds backend-specific options.  Each backend
	 * gets a child widget.  Only one child widget is visible at once. */
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_end (GTK_BOX (config), widget, TRUE, TRUE, 0);
	config->priv->backend_box = g_object_ref (widget);
	gtk_widget_show (widget);

	pango_attr_list_unref (attr_list);
}

GtkWidget *
e_source_config_new (ESourceRegistry *registry,
                     ESource *original_source)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	if (original_source != NULL)
		g_return_val_if_fail (E_IS_SOURCE (original_source), NULL);

	return g_object_new (
		E_TYPE_SOURCE_CONFIG, "registry", registry,
		"original-source", original_source, NULL);
}

void
e_source_config_insert_widget (ESourceConfig *config,
                               ESource *scratch_source,
                               const gchar *caption,
                               GtkWidget *widget)
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *label;

	g_return_if_fail (E_IS_SOURCE_CONFIG (config));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (scratch_source == NULL)
		vbox = GTK_WIDGET (config);
	else
		vbox = e_source_config_get_page (config, scratch_source);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

	e_binding_bind_property (
		widget, "visible",
		hbox, "visible",
		G_BINDING_SYNC_CREATE);

	label = gtk_label_new (caption);
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
	gtk_size_group_add_widget (config->priv->size_group, label);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);
}

GtkWidget *
e_source_config_get_page (ESourceConfig *config,
                          ESource *scratch_source)
{
	Candidate *candidate;
	GtkWidget *page = NULL;
	GPtrArray *array;
	gint index;

	g_return_val_if_fail (E_IS_SOURCE_CONFIG (config), NULL);
	g_return_val_if_fail (E_IS_SOURCE (scratch_source), NULL);

	array = config->priv->candidates;

	for (index = 0; page == NULL && index < array->len; index++) {
		candidate = g_ptr_array_index (array, index);
		if (e_source_equal (scratch_source, candidate->scratch_source))
			page = candidate->page;
	}

	g_return_val_if_fail (GTK_IS_BOX (page), NULL);

	return page;
}

const gchar *
e_source_config_get_backend_extension_name (ESourceConfig *config)
{
	ESourceConfigClass *class;

	g_return_val_if_fail (E_IS_SOURCE_CONFIG (config), NULL);

	class = E_SOURCE_CONFIG_GET_CLASS (config);
	g_return_val_if_fail (class->get_backend_extension_name != NULL, NULL);

	return class->get_backend_extension_name (config);
}

GList *
e_source_config_list_eligible_collections (ESourceConfig *config)
{
	ESourceConfigClass *class;

	g_return_val_if_fail (E_IS_SOURCE_CONFIG (config), NULL);

	class = E_SOURCE_CONFIG_GET_CLASS (config);
	g_return_val_if_fail (class->list_eligible_collections != NULL, NULL);

	return class->list_eligible_collections (config);
}

gboolean
e_source_config_check_complete (ESourceConfig *config)
{
	Candidate *candidate;
	gboolean complete;

	g_return_val_if_fail (E_IS_SOURCE_CONFIG (config), FALSE);

	candidate = source_config_get_active_candidate (config);
	g_return_val_if_fail (candidate != NULL, FALSE);

	g_signal_emit (
		config, signals[CHECK_COMPLETE], 0,
		candidate->scratch_source, &complete);

	complete &= e_source_config_backend_check_complete (
		candidate->backend, candidate->scratch_source);

	/* XXX Emitting "notify::complete" may cause this function
	 *     to be called repeatedly by signal handlers.  The IF
	 *     check below should break any recursive cycles.  Not
	 *     very efficient but I think we can live with it. */

	if (complete != config->priv->complete) {
		config->priv->complete = complete;
		g_object_notify (G_OBJECT (config), "complete");
	}

	return complete;
}

ESource *
e_source_config_get_original_source (ESourceConfig *config)
{
	g_return_val_if_fail (E_IS_SOURCE_CONFIG (config), NULL);

	return config->priv->original_source;
}

ESource *
e_source_config_get_collection_source (ESourceConfig *config)
{
	g_return_val_if_fail (E_IS_SOURCE_CONFIG (config), NULL);

	return config->priv->collection_source;
}

ESourceRegistry *
e_source_config_get_registry (ESourceConfig *config)
{
	g_return_val_if_fail (E_IS_SOURCE_CONFIG (config), NULL);

	return config->priv->registry;
}

void
e_source_config_resize_window (ESourceConfig *config)
{
	g_return_if_fail (E_IS_SOURCE_CONFIG (config));

	g_signal_emit (config, signals[RESIZE_WINDOW], 0);
}

/* Helper for e_source_config_commit() */
static void
source_config_commit_cb (GObject *object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	GSimpleAsyncResult *simple;
	GError *error = NULL;

	simple = G_SIMPLE_ASYNC_RESULT (user_data);

	e_source_registry_commit_source_finish (
		E_SOURCE_REGISTRY (object), result, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);

	g_simple_async_result_complete (simple);
	g_object_unref (simple);
}

void
e_source_config_commit (ESourceConfig *config,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
	GSimpleAsyncResult *simple;
	ESourceRegistry *registry;
	Candidate *candidate;

	g_return_if_fail (E_IS_SOURCE_CONFIG (config));

	registry = e_source_config_get_registry (config);

	candidate = source_config_get_active_candidate (config);
	g_return_if_fail (candidate != NULL);

	e_source_config_backend_commit_changes (
		candidate->backend, candidate->scratch_source);

	g_signal_emit (
		config, signals[COMMIT_CHANGES], 0,
		candidate->scratch_source);

	simple = g_simple_async_result_new (
		G_OBJECT (config), callback,
		user_data, e_source_config_commit);

	e_source_registry_commit_source (
		registry, candidate->scratch_source,
		cancellable, source_config_commit_cb, simple);
}

gboolean
e_source_config_commit_finish (ESourceConfig *config,
                               GAsyncResult *result,
                               GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (config),
		e_source_config_commit), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

void
e_source_config_add_refresh_interval (ESourceConfig *config,
                                      ESource *scratch_source)
{
	GtkWidget *widget;
	GtkWidget *container;
	ESourceExtension *extension;
	const gchar *extension_name;

	g_return_if_fail (E_IS_SOURCE_CONFIG (config));
	g_return_if_fail (E_IS_SOURCE (scratch_source));

	extension_name = E_SOURCE_EXTENSION_REFRESH;
	extension = e_source_get_extension (scratch_source, extension_name);

	widget = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
	e_source_config_insert_widget (config, scratch_source, NULL, widget);
	gtk_widget_show (widget);

	container = widget;

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_show (widget);

	container = widget;

	/* Translators: This is the first of a sequence of widgets:
	 * "Refresh every [NUMERIC_ENTRY] [TIME_UNITS_COMBO]" */
	widget = gtk_label_new (_("Refresh every"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	widget = e_interval_chooser_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);

	e_binding_bind_property (
		extension, "interval-minutes",
		widget, "interval-minutes",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}

void
e_source_config_add_secure_connection (ESourceConfig *config,
                                       ESource *scratch_source)
{
	GtkWidget *widget;
	ESourceExtension *extension;
	const gchar *extension_name;
	const gchar *label;

	g_return_if_fail (E_IS_SOURCE_CONFIG (config));
	g_return_if_fail (E_IS_SOURCE (scratch_source));

	extension_name = E_SOURCE_EXTENSION_SECURITY;
	extension = e_source_get_extension (scratch_source, extension_name);

	label = _("Use a secure connection");
	widget = gtk_check_button_new_with_label (label);
	e_source_config_insert_widget (config, scratch_source, NULL, widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		extension, "secure",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}

static gboolean
secure_to_port_cb (GBinding *binding,
                   const GValue *source_value,
                   GValue *target_value,
                   gpointer user_data)
{
	GObject *authentication_extension;
	guint16 port;

	authentication_extension = g_binding_get_target (binding);

	port = e_source_authentication_get_port (
		E_SOURCE_AUTHENTICATION (authentication_extension));

	if (port == 80 || port == 443 || port == 0)
		port = g_value_get_boolean (source_value) ? 443 : 80;

	g_value_set_uint (target_value, port);

	return TRUE;
}

static gboolean
webdav_source_ssl_trust_to_sensitive_cb (GBinding *binding,
                                         const GValue *source_value,
                                         GValue *target_value,
                                         gpointer user_data)
{
	const gchar *ssl_trust = g_value_get_string (source_value);

	g_value_set_boolean (target_value, ssl_trust && *ssl_trust);

	return TRUE;
}

static void
webdav_unset_ssl_trust_clicked_cb (GtkWidget *button,
                                   ESourceWebdav *extension)
{
	e_source_webdav_set_ssl_trust (extension, NULL);
}

void
e_source_config_add_secure_connection_for_webdav (ESourceConfig *config,
                                                  ESource *scratch_source)
{
	GtkWidget *widget;
	ESourceExtension *extension;
	ESourceAuthentication *authentication_extension;
	const gchar *extension_name;
	const gchar *label;

	g_return_if_fail (E_IS_SOURCE_CONFIG (config));
	g_return_if_fail (E_IS_SOURCE (scratch_source));

	extension_name = E_SOURCE_EXTENSION_SECURITY;
	extension = e_source_get_extension (scratch_source, extension_name);

	label = _("Use a secure connection");
	widget = gtk_check_button_new_with_label (label);
	e_source_config_insert_widget (config, scratch_source, NULL, widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		extension, "secure",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	authentication_extension =
		e_source_get_extension (scratch_source, extension_name);

	e_binding_bind_property_full (
		extension, "secure",
		authentication_extension, "port",
		G_BINDING_DEFAULT,
		secure_to_port_cb,
		NULL, NULL, NULL);

	extension_name = E_SOURCE_EXTENSION_WEBDAV_BACKEND;
	extension = e_source_get_extension (scratch_source, extension_name);

	widget = gtk_button_new_with_mnemonic (
		_("Unset _trust for SSL certificate"));
	gtk_widget_set_margin_left (widget, 24);
	e_source_config_insert_widget (config, scratch_source, NULL, widget);
	gtk_widget_show (widget);

	e_binding_bind_property_full (
		extension, "ssl-trust",
		widget, "sensitive",
		G_BINDING_SYNC_CREATE,
		webdav_source_ssl_trust_to_sensitive_cb,
		NULL, NULL, NULL);

	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (webdav_unset_ssl_trust_clicked_cb), extension);
}

GtkWidget *
e_source_config_add_user_entry (ESourceConfig *config,
                                ESource *scratch_source)
{
	GtkWidget *widget;
	ESource *original_source;
	ESourceExtension *extension;
	const gchar *extension_name;

	g_return_val_if_fail (E_IS_SOURCE_CONFIG (config), NULL);
	g_return_val_if_fail (E_IS_SOURCE (scratch_source), NULL);

	extension_name = E_SOURCE_EXTENSION_AUTHENTICATION;
	extension = e_source_get_extension (scratch_source, extension_name);

	original_source = e_source_config_get_original_source (config);

	widget = gtk_entry_new ();
	e_source_config_insert_widget (
		config, scratch_source, _("User"), widget);
	gtk_widget_show (widget);

	e_binding_bind_object_text_property (
		extension, "user",
		widget, "text",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* If this is a new data source, initialize the
	 * GtkEntry to the user name of the current user. */
	if (original_source == NULL)
		gtk_entry_set_text (GTK_ENTRY (widget), g_get_user_name ());

	return widget;
}

