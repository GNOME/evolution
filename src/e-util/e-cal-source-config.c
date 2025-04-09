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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include "e-misc-utils.h"
#include "e-cal-source-config.h"

struct _ECalSourceConfigPrivate {
	ECalClientSourceType source_type;
	GtkWidget *color_button;
	GtkWidget *default_button;
};

enum {
	PROP_0,
	PROP_SOURCE_TYPE
};

G_DEFINE_TYPE_WITH_PRIVATE (ECalSourceConfig, e_cal_source_config, E_TYPE_SOURCE_CONFIG)

static ESource *
cal_source_config_ref_default (ESourceConfig *config)
{
	ECalSourceConfig *self;
	ESourceRegistry *registry;

	self = E_CAL_SOURCE_CONFIG (config);
	registry = e_source_config_get_registry (config);

	if (self->priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS)
		return e_source_registry_ref_default_calendar (registry);
	else if (self->priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS)
		return e_source_registry_ref_default_memo_list (registry);
	else if (self->priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS)
		return e_source_registry_ref_default_task_list (registry);

	g_return_val_if_reached (NULL);
}

static void
cal_source_config_set_default (ESourceConfig *config,
                               ESource *source)
{
	ECalSourceConfig *self;
	ESourceRegistry *registry;

	self = E_CAL_SOURCE_CONFIG (config);
	registry = e_source_config_get_registry (config);

	if (self->priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS)
		e_source_registry_set_default_calendar (registry, source);
	else if (self->priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS)
		e_source_registry_set_default_memo_list (registry, source);
	else if (self->priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS)
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
	ECalSourceConfig *self = E_CAL_SOURCE_CONFIG (object);

	g_clear_object (&self->priv->color_button);
	g_clear_object (&self->priv->default_button);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_source_config_parent_class)->dispose (object);
}

static void
cal_source_config_constructed (GObject *object)
{
	ECalSourceConfig *self;
	ESource *original_source;
	ESourceConfig *config;
	GtkWidget *widget;
	const gchar *label;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_source_config_parent_class)->constructed (object);

	config = E_SOURCE_CONFIG (object);
	self = E_CAL_SOURCE_CONFIG (object);

	widget = gtk_color_button_new ();
	self->priv->color_button = g_object_ref_sink (widget);
	gtk_widget_show (widget);

	switch (self->priv->source_type) {
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
	self->priv->default_button = g_object_ref_sink (widget);

	original_source = e_source_config_get_original_source (config);
	e_source_config_insert_widget (
		config, NULL, _("Color:"), self->priv->color_button);

	if (!original_source || !e_util_guess_source_is_readonly (original_source)) {
		gtk_widget_show (widget);

		if (original_source != NULL) {
			gboolean active;
			ESource *default_source;

			default_source = cal_source_config_ref_default (config);
			active = e_source_equal (original_source, default_source);
			g_object_set (self->priv->default_button, "active", active, NULL);
			g_object_unref (default_source);
		}
		e_source_config_insert_widget (
			config, NULL, NULL, self->priv->default_button);
	}
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
		/* From the HIG https://developer.gnome.org/hig/reference/palette.html , as of 2023-09-29 */
		"#62a0ea", /* Blue 2 */
		"#1c71d8", /* Blue 4 */
		"#57e389", /* Green 2 */
		"#2ec27e", /* Green 4 */
		"#f8e45c", /* Yellow 2 */
		"#f5c211", /* Yellow 4 */
		"#ffbe6f", /* Orange 1 */
		"#ff7800", /* Orange 3 */
		"#ed333b", /* Red 2 */
		"#c01c28", /* Red 4 */
		"#c061cb", /* Purple 2 */
		"#813d9c"  /* Purple 4 */
	};

	return colors[g_random_int_range (0, G_N_ELEMENTS (colors))];
}

static void
cal_source_config_init_candidate (ESourceConfig *config,
                                  ESource *scratch_source)
{
	ECalSourceConfig *self;
	ESourceExtension *extension;
	const gchar *extension_name;

	/* Chain up to parent's init_candidate() method. */
	E_SOURCE_CONFIG_CLASS (e_cal_source_config_parent_class)->init_candidate (config, scratch_source);

	self = E_CAL_SOURCE_CONFIG (config);

	extension_name = e_source_config_get_backend_extension_name (config);
	extension = e_source_get_extension (scratch_source, extension_name);

	/* Preselect a random color on a new source */
	if (!e_source_config_get_original_source (config))
		e_source_selectable_set_color (E_SOURCE_SELECTABLE (extension), choose_initial_color ());

	e_binding_bind_property_full (
		extension, "color",
		self->priv->color_button, "rgba",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_string_to_rgba,
		e_binding_transform_rgba_to_string,
		NULL, (GDestroyNotify) NULL);

	if (self->priv->source_type == E_CAL_CLIENT_SOURCE_TYPE_EVENTS &&
	    g_strcmp0 (e_source_backend_get_backend_name (E_SOURCE_BACKEND (extension)), "contacts") != 0 &&
	    g_strcmp0 (e_source_backend_get_backend_name (E_SOURCE_BACKEND (extension)), "weather") != 0) {
		ESourceAlarms *alarms_extension;
		GtkWidget *widget;

		widget = gtk_check_button_new_with_mnemonic (_("Show reminder _before every event"));
		e_source_config_insert_widget (config, scratch_source, NULL, widget);
		gtk_widget_show (widget);

		alarms_extension = e_source_get_extension (scratch_source, E_SOURCE_EXTENSION_ALARMS);
		e_binding_bind_property (
			alarms_extension, "for-every-event",
			widget, "active",
			G_BINDING_BIDIRECTIONAL |
			G_BINDING_SYNC_CREATE);
	}
}

static void
cal_source_config_commit_changes (ESourceConfig *config,
                                  ESource *scratch_source)
{
	ECalSourceConfig *self;
	GtkToggleButton *toggle_button;
	ESource *default_source;

	self = E_CAL_SOURCE_CONFIG (config);
	toggle_button = GTK_TOGGLE_BUTTON (self->priv->default_button);

	/* Chain up to parent's commit_changes() method. */
	E_SOURCE_CONFIG_CLASS (e_cal_source_config_parent_class)->commit_changes (config, scratch_source);

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
	config->priv = e_cal_source_config_get_instance_private (config);
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
