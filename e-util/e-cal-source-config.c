/*
 * e-cal-source-config.c
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

#include "e-cal-source-config.h"

#include <config.h>
#include <glib/gi18n-lib.h>

#include "e-misc-utils.h"

#define E_CAL_SOURCE_CONFIG_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SOURCE_CONFIG, ECalSourceConfigPrivate))

struct _ECalSourceConfigPrivate {
	ECalClientSourceType source_type;
	GtkWidget *color_button;
	GtkWidget *default_button;
};

enum {
	PROP_0,
	PROP_SOURCE_TYPE
};

G_DEFINE_TYPE (
	ECalSourceConfig,
	e_cal_source_config,
	E_TYPE_SOURCE_CONFIG)

static ESource *
cal_source_config_ref_default (ESourceConfig *config)
{
	ECalSourceConfigPrivate *priv;
	ESourceRegistry *registry;

	priv = E_CAL_SOURCE_CONFIG_GET_PRIVATE (config);
	registry = e_source_config_get_registry (config);

	if (priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS)
		return e_source_registry_ref_default_calendar (registry);
	else if (priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS)
		return e_source_registry_ref_default_memo_list (registry);
	else if (priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS)
		return e_source_registry_ref_default_task_list (registry);

	g_return_val_if_reached (NULL);
}

static void
cal_source_config_set_default (ESourceConfig *config,
                               ESource *source)
{
	ECalSourceConfigPrivate *priv;
	ESourceRegistry *registry;

	priv = E_CAL_SOURCE_CONFIG_GET_PRIVATE (config);
	registry = e_source_config_get_registry (config);

	if (priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS)
		e_source_registry_set_default_calendar (registry, source);
	else if (priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS)
		e_source_registry_set_default_memo_list (registry, source);
	else if (priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS)
		e_source_registry_set_default_task_list (registry, source);
}

static void
cal_source_config_set_source_type (ECalSourceConfig *config,
                                   ECalClientSourceType source_type)
{
	config->priv->source_type = source_type;
}

static void
cal_source_config_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_TYPE:
			cal_source_config_set_source_type (
				E_CAL_SOURCE_CONFIG (object),
				g_value_get_enum (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_source_config_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_TYPE:
			g_value_set_enum (
				value,
				e_cal_source_config_get_source_type (
				E_CAL_SOURCE_CONFIG (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_source_config_dispose (GObject *object)
{
	ECalSourceConfigPrivate *priv;

	priv = E_CAL_SOURCE_CONFIG_GET_PRIVATE (object);

	if (priv->color_button != NULL) {
		g_object_unref (priv->color_button);
		priv->color_button = NULL;
	}

	if (priv->default_button != NULL) {
		g_object_unref (priv->default_button);
		priv->default_button = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_source_config_parent_class)->dispose (object);
}

static void
cal_source_config_constructed (GObject *object)
{
	ECalSourceConfigPrivate *priv;
	ESource *default_source;
	ESource *original_source;
	ESourceConfig *config;
	GObjectClass *class;
	GtkWidget *widget;
	const gchar *label;

	/* Chain up to parent's constructed() method. */
	class = G_OBJECT_CLASS (e_cal_source_config_parent_class);
	class->constructed (object);

	config = E_SOURCE_CONFIG (object);
	priv = E_CAL_SOURCE_CONFIG_GET_PRIVATE (object);

	widget = gtk_color_button_new ();
	priv->color_button = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	switch (priv->source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			label = _("Mark as default calendar");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			label = _("Mark as default task list");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			label = _("Mark as default memo list");
			break;
		default:
			/* No need to translate this string. */
			label = "Invalid ECalSourceType value";
			g_warn_if_reached ();
	}

	widget = gtk_check_button_new_with_label (label);
	priv->default_button = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	default_source = cal_source_config_ref_default (config);
	original_source = e_source_config_get_original_source (config);

	if (original_source != NULL) {
		gboolean active;

		active = e_source_equal (original_source, default_source);
		g_object_set (priv->default_button, "active", active, NULL);
	}

	g_object_unref (default_source);

	e_source_config_insert_widget (
		config, NULL, _("Color:"), priv->color_button);

	e_source_config_insert_widget (
		config, NULL, NULL, priv->default_button);
}

static const gchar *
cal_source_config_get_backend_extension_name (ESourceConfig *config)
{
	ECalSourceConfig *cal_config;
	const gchar *extension_name;

	cal_config = E_CAL_SOURCE_CONFIG (config);

	switch (e_cal_source_config_get_source_type (cal_config)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		default:
			g_return_val_if_reached (NULL);
	}

	return extension_name;
}

static GList *
cal_source_config_list_eligible_collections (ESourceConfig *config)
{
	GQueue trash = G_QUEUE_INIT;
	GList *list, *link;

	/* Chain up to parent's list_eligible_collections() method. */
	list = E_SOURCE_CONFIG_CLASS (e_cal_source_config_parent_class)->
		list_eligible_collections (config);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceCollection *extension;
		const gchar *extension_name;

		extension_name = E_SOURCE_EXTENSION_COLLECTION;
		extension = e_source_get_extension (source, extension_name);

		if (!e_source_collection_get_calendar_enabled (extension))
			g_queue_push_tail (&trash, link);
	}

	/* Remove ineligible collections from the list. */
	while ((link = g_queue_pop_head (&trash)) != NULL) {
		g_object_unref (link->data);
		list = g_list_delete_link (list, link);
	}

	return list;
}

static const gchar *
choose_initial_color (void)
{
	static const gchar *colors[] = {
		"#BECEDD", /* 190 206 221     Blue */
		"#E2F0EF", /* 226 240 239     Light Blue */
		"#C6E2B7", /* 198 226 183     Green */
		"#E2F0D3", /* 226 240 211     Light Green */
		"#E2D4B7", /* 226 212 183     Khaki */
		"#EAEAC1", /* 234 234 193     Light Khaki */
		"#F0B8B7", /* 240 184 183     Pink */
		"#FED4D3", /* 254 212 211     Light Pink */
		"#E2C6E1", /* 226 198 225     Purple */
		"#F0E2EF"  /* 240 226 239     Light Purple */
	};

	return colors[g_random_int_range (0, G_N_ELEMENTS (colors))];
}

static void
cal_source_config_init_candidate (ESourceConfig *config,
                                  ESource *scratch_source)
{
	ECalSourceConfigPrivate *priv;
	ESourceConfigClass *class;
	ESourceExtension *extension;
	const gchar *extension_name;

	/* Chain up to parent's init_candidate() method. */
	class = E_SOURCE_CONFIG_CLASS (e_cal_source_config_parent_class);
	class->init_candidate (config, scratch_source);

	priv = E_CAL_SOURCE_CONFIG_GET_PRIVATE (config);

	extension_name = e_source_config_get_backend_extension_name (config);
	extension = e_source_get_extension (scratch_source, extension_name);

	/* Preselect a random color on a new source */
	if (!e_source_config_get_original_source (config))
		e_source_selectable_set_color (E_SOURCE_SELECTABLE (extension), choose_initial_color ());

	e_binding_bind_property_full (
		extension, "color",
		priv->color_button, "color",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_string_to_color,
		e_binding_transform_color_to_string,
		NULL, (GDestroyNotify) NULL);
}

static void
cal_source_config_commit_changes (ESourceConfig *config,
                                  ESource *scratch_source)
{
	ECalSourceConfigPrivate *priv;
	GtkToggleButton *toggle_button;
	ESourceConfigClass *class;
	ESource *default_source;

	priv = E_CAL_SOURCE_CONFIG_GET_PRIVATE (config);
	toggle_button = GTK_TOGGLE_BUTTON (priv->default_button);

	/* Chain up to parent's commit_changes() method. */
	class = E_SOURCE_CONFIG_CLASS (e_cal_source_config_parent_class);
	class->commit_changes (config, scratch_source);

	default_source = cal_source_config_ref_default (config);

	/* The default setting is a little tricky to get right.  If
	 * the toggle button is active, this ESource is now the default.
	 * That much is simple.  But if the toggle button is NOT active,
	 * then we have to inspect the old default.  If this ESource WAS
	 * the default, reset the default to 'system'.  If this ESource
	 * WAS NOT the old default, leave it alone. */
	if (gtk_toggle_button_get_active (toggle_button))
		cal_source_config_set_default (config, scratch_source);
	else if (e_source_equal (scratch_source, default_source))
		cal_source_config_set_default (config, NULL);

	g_object_unref (default_source);
}

static void
e_cal_source_config_class_init (ECalSourceConfigClass *class)
{
	GObjectClass *object_class;
	ESourceConfigClass *source_config_class;

	g_type_class_add_private (class, sizeof (ECalSourceConfigPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = cal_source_config_set_property;
	object_class->get_property = cal_source_config_get_property;
	object_class->dispose = cal_source_config_dispose;
	object_class->constructed = cal_source_config_constructed;

	source_config_class = E_SOURCE_CONFIG_CLASS (class);
	source_config_class->get_backend_extension_name =
		cal_source_config_get_backend_extension_name;
	source_config_class->list_eligible_collections =
		cal_source_config_list_eligible_collections;
	source_config_class->init_candidate = cal_source_config_init_candidate;
	source_config_class->commit_changes = cal_source_config_commit_changes;

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_TYPE,
		g_param_spec_enum (
			"source-type",
			"Source Type",
			"The iCalendar object type",
			E_TYPE_CAL_CLIENT_SOURCE_TYPE,
			E_CAL_CLIENT_SOURCE_TYPE_EVENTS,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_cal_source_config_init (ECalSourceConfig *config)
{
	config->priv = E_CAL_SOURCE_CONFIG_GET_PRIVATE (config);
}

GtkWidget *
e_cal_source_config_new (ESourceRegistry *registry,
                         ESource *original_source,
                         ECalClientSourceType source_type)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	if (original_source != NULL)
		g_return_val_if_fail (E_IS_SOURCE (original_source), NULL);

	return g_object_new (
		E_TYPE_CAL_SOURCE_CONFIG, "registry", registry,
		"original-source", original_source, "source-type",
		source_type, NULL);
}

ECalClientSourceType
e_cal_source_config_get_source_type (ECalSourceConfig *config)
{
	g_return_val_if_fail (E_IS_CAL_SOURCE_CONFIG (config), 0);

	return config->priv->source_type;
}

void
e_cal_source_config_add_offline_toggle (ECalSourceConfig *config,
                                        ESource *scratch_source)
{
	GtkWidget *widget;
	ESourceExtension *extension;
	const gchar *extension_name;
	const gchar *label;

	g_return_if_fail (E_IS_CAL_SOURCE_CONFIG (config));
	g_return_if_fail (E_IS_SOURCE (scratch_source));

	extension_name = E_SOURCE_EXTENSION_OFFLINE;
	extension = e_source_get_extension (scratch_source, extension_name);

	switch (e_cal_source_config_get_source_type (config)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			label = _("Copy calendar contents locally "
				  "for offline operation");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			label = _("Copy task list contents locally "
				  "for offline operation");
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			label = _("Copy memo list contents locally "
				  "for offline operation");
			break;
		default:
			g_return_if_reached ();
	}

	widget = gtk_check_button_new_with_label (label);
	e_source_config_insert_widget (
		E_SOURCE_CONFIG (config), scratch_source, NULL, widget);
	gtk_widget_show (widget);

	e_binding_bind_property (
		extension, "stay-synchronized",
		widget, "active",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);
}
