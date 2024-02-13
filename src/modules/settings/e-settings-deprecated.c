/*
 * e-settings-deprecated.c
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

/* This class is different from the others in this module.  Its purpose
 * is to transfer values from deprecated GSettings keys to the preferred
 * keys on startup, and keep them synchronized at all times for backward
 * compatibility. */

#include "e-settings-deprecated.h"

#include <shell/e-shell.h>
#include <mail/e-mail-enums.h>
#include <em-format/e-mail-formatter-enums.h>

struct _ESettingsDeprecatedPrivate {
	GSettings *calendar_settings;
	gulong week_start_day_name_handler_id;
	gulong work_day_monday_handler_id;
	gulong work_day_tuesday_handler_id;
	gulong work_day_wednesday_handler_id;
	gulong work_day_thursday_handler_id;
	gulong work_day_friday_handler_id;
	gulong work_day_saturday_handler_id;
	gulong work_day_sunday_handler_id;

	GSettings *mail_settings;
	gulong browser_close_on_reply_policy_handler_id;
	gulong forward_style_name_handler_id;
	gulong reply_style_name_handler_id;
	gulong image_loading_policy_handler_id;
	gulong show_headers_handler_id;
};

/* Flag values used in the "working-days" key. */
enum {
	DEPRECATED_WORKING_DAYS_SUNDAY = 1 << 0,
	DEPRECATED_WORKING_DAYS_MONDAY = 1 << 1,
	DEPRECATED_WORKING_DAYS_TUESDAY = 1 << 2,
	DEPRECATED_WORKING_DAYS_WEDNESDAY = 1 << 3,
	DEPRECATED_WORKING_DAYS_THURSDAY = 1 << 4,
	DEPRECATED_WORKING_DAYS_FRIDAY = 1 << 5,
	DEPRECATED_WORKING_DAYS_SATURDAY = 1 << 6
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESettingsDeprecated, e_settings_deprecated, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (ESettingsDeprecated))

static void
settings_deprecated_header_start_element (GMarkupParseContext *context,
                                          const gchar *element_name,
                                          const gchar **attribute_names,
                                          const gchar **attribute_values,
                                          gpointer user_data,
                                          GError **error)
{
	GVariantBuilder *builder = user_data;
	const gchar *name = NULL;
	const gchar *enabled = NULL;

	/* The enabled flag is determined by the presence of an "enabled"
	 * attribute, but the actual value of the "enabled" attribute, if
	 * present, is just an empty string.  It's pretty convoluted. */
	g_markup_collect_attributes (
		element_name,
		attribute_names,
		attribute_values,
		error,
		G_MARKUP_COLLECT_STRING,
		"name", &name,
		G_MARKUP_COLLECT_STRING |
		G_MARKUP_COLLECT_OPTIONAL,
		"enabled", &enabled,
		G_MARKUP_COLLECT_INVALID);

	if (name != NULL)
		g_variant_builder_add (
			builder, "(sb)", name, (enabled != NULL));
}

static void
e_settings_deprecated_set_int_with_change_test (GSettings *settings,
                                                const gchar *key,
                                                gint value)
{
	if (g_settings_get_int (settings, key) != value)
		g_settings_set_int (settings, key, value);
}

static void
e_settings_deprecated_set_string_with_change_test (GSettings *settings,
                                                   const gchar *key,
                                                   const gchar *value)
{
	gchar *stored = g_settings_get_string (settings, key);

	if (g_strcmp0 (stored, value) != 0)
		g_settings_set_string (settings, key, value);

	g_free (stored);
}

static void
e_settings_deprecated_set_strv_with_change_test (GSettings *settings,
                                                 const gchar *key,
                                                 const gchar * const *value)
{
	gchar **stored = g_settings_get_strv (settings, key);
	gboolean changed;
	gint ii;

	changed = !stored || !value;
	for (ii = 0; !changed && stored[ii] && value[ii]; ii++) {
		changed = g_strcmp0 (stored[ii], value[ii]) != 0;
	}

	changed = changed || stored[ii] != NULL || value[ii] != NULL;

	if (changed)
		g_settings_set_strv (settings, key, value);

	g_strfreev (stored);
}

static void
settings_deprecated_header_parse_xml (const gchar *xml,
                                      GVariantBuilder *builder)
{
	static GMarkupParser parser = {
		settings_deprecated_header_start_element, };

	GMarkupParseContext *context;

	context = g_markup_parse_context_new (&parser, 0, builder, NULL);
	g_markup_parse_context_parse (context, xml, -1, NULL);
	g_markup_parse_context_end_parse (context, NULL);
	g_markup_parse_context_free (context);
}

static GVariant *
settings_deprecated_header_strv_to_variant (gchar **strv)
{
	GVariantBuilder builder;
	guint ii, length;

	length = g_strv_length (strv);

	/* Disregard an empty list. */
	if (length == 0)
		return NULL;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(sb)"));

	for (ii = 0; ii < length; ii++)
		settings_deprecated_header_parse_xml (strv[ii], &builder);

	return g_variant_builder_end (&builder);
}

static void
settings_deprecated_week_start_day_name_cb (GSettings *settings,
                                            const gchar *key)
{
	GDateWeekday weekday;
	gint tm_wday;

	weekday = g_settings_get_enum (settings, "week-start-day-name");
	tm_wday = e_weekday_to_tm_wday (weekday);
	e_settings_deprecated_set_int_with_change_test (settings, "week-start-day", tm_wday);
}

static void
settings_deprecated_work_day_monday_cb (GSettings *settings,
                                        const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-monday"))
		flags |= DEPRECATED_WORKING_DAYS_MONDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_MONDAY;
	e_settings_deprecated_set_int_with_change_test (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_tuesday_cb (GSettings *settings,
                                         const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-tuesday"))
		flags |= DEPRECATED_WORKING_DAYS_TUESDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_TUESDAY;
	e_settings_deprecated_set_int_with_change_test (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_wednesday_cb (GSettings *settings,
                                           const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-wednesday"))
		flags |= DEPRECATED_WORKING_DAYS_WEDNESDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_WEDNESDAY;
	e_settings_deprecated_set_int_with_change_test (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_thursday_cb (GSettings *settings,
                                          const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-thursday"))
		flags |= DEPRECATED_WORKING_DAYS_THURSDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_THURSDAY;
	e_settings_deprecated_set_int_with_change_test (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_friday_cb (GSettings *settings,
                                        const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-friday"))
		flags |= DEPRECATED_WORKING_DAYS_FRIDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_FRIDAY;
	e_settings_deprecated_set_int_with_change_test (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_saturday_cb (GSettings *settings,
                                          const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-saturday"))
		flags |= DEPRECATED_WORKING_DAYS_SATURDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_SATURDAY;
	e_settings_deprecated_set_int_with_change_test (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_sunday_cb (GSettings *settings,
                                        const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-sunday"))
		flags |= DEPRECATED_WORKING_DAYS_SUNDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_SUNDAY;
	e_settings_deprecated_set_int_with_change_test (settings, "working-days", flags);
}

static void
settings_deprecated_browser_close_on_reply_policy_cb (GSettings *settings,
                                                      const gchar *key)
{
	const gchar *deprecated_key = "prompt-on-reply-close-browser";

	switch (g_settings_get_enum (settings, key)) {
		case E_AUTOMATIC_ACTION_POLICY_ALWAYS:
			e_settings_deprecated_set_string_with_change_test (
				settings, deprecated_key, "always");
			break;
		case E_AUTOMATIC_ACTION_POLICY_NEVER:
			e_settings_deprecated_set_string_with_change_test (
				settings, deprecated_key, "never");
			break;
		default:
			e_settings_deprecated_set_string_with_change_test (
				settings, deprecated_key, "ask");
			break;
	}
}

static void
settings_deprecated_forward_style_name_cb (GSettings *settings,
                                           const gchar *key)
{
	EMailForwardStyle style;

	style = g_settings_get_enum (settings, "forward-style-name");
	e_settings_deprecated_set_int_with_change_test (settings, "forward-style", style);
}

static void
settings_deprecated_reply_style_name_cb (GSettings *settings,
                                         const gchar *key)
{
	EMailReplyStyle style;

	style = g_settings_get_enum (settings, "reply-style-name");
	e_settings_deprecated_set_int_with_change_test (settings, "reply-style", style);
}

static void
settings_deprecated_image_loading_policy_cb (GSettings *settings,
                                             const gchar *key)
{
	EImageLoadingPolicy policy;

	policy = g_settings_get_enum (settings, "image-loading-policy");
	e_settings_deprecated_set_int_with_change_test (settings, "load-http-images", policy);
}

static void
settings_deprecated_show_headers_cb (GSettings *settings,
                                     const gchar *key)
{
	GVariant *variant;
	gsize ii, n_children;
	gchar **strv = NULL;

	variant = g_settings_get_value (settings, key);
	n_children = g_variant_n_children (variant);

	strv = g_new0 (gchar *, n_children + 1);

	for (ii = 0; ii < n_children; ii++) {
		const gchar *name = NULL;
		gboolean enabled = FALSE;

		g_variant_get_child (
			variant, ii, "(&sb)", &name, &enabled);

		strv[ii] = g_strdup_printf (
			"<?xml version=\"1.0\"?>\n"
			"<header name=\"%s\"%s/>\n",
			name, enabled ? " enabled=\"\"" : "");
	}

	e_settings_deprecated_set_strv_with_change_test (
		settings, "headers",
		(const gchar * const *) strv);

	g_strfreev (strv);

	g_variant_unref (variant);
}

static void
settings_deprecated_dispose (GObject *object)
{
	ESettingsDeprecated *self = E_SETTINGS_DEPRECATED (object);

	if (self->priv->week_start_day_name_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->calendar_settings,
			self->priv->week_start_day_name_handler_id);
		self->priv->week_start_day_name_handler_id = 0;
	}

	if (self->priv->work_day_monday_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->calendar_settings,
			self->priv->work_day_monday_handler_id);
		self->priv->work_day_monday_handler_id = 0;
	}

	if (self->priv->work_day_tuesday_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->calendar_settings,
			self->priv->work_day_tuesday_handler_id);
		self->priv->work_day_tuesday_handler_id = 0;
	}

	if (self->priv->work_day_wednesday_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->calendar_settings,
			self->priv->work_day_wednesday_handler_id);
		self->priv->work_day_wednesday_handler_id = 0;
	}

	if (self->priv->work_day_thursday_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->calendar_settings,
			self->priv->work_day_thursday_handler_id);
		self->priv->work_day_thursday_handler_id = 0;
	}

	if (self->priv->work_day_friday_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->calendar_settings,
			self->priv->work_day_friday_handler_id);
		self->priv->work_day_friday_handler_id = 0;
	}

	if (self->priv->work_day_saturday_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->calendar_settings,
			self->priv->work_day_saturday_handler_id);
		self->priv->work_day_saturday_handler_id = 0;
	}

	if (self->priv->work_day_sunday_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->calendar_settings,
			self->priv->work_day_sunday_handler_id);
		self->priv->work_day_sunday_handler_id = 0;
	}

	if (self->priv->browser_close_on_reply_policy_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->mail_settings,
			self->priv->browser_close_on_reply_policy_handler_id);
		self->priv->browser_close_on_reply_policy_handler_id = 0;
	}

	if (self->priv->forward_style_name_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->mail_settings,
			self->priv->forward_style_name_handler_id);
		self->priv->forward_style_name_handler_id = 0;
	}

	if (self->priv->reply_style_name_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->mail_settings,
			self->priv->reply_style_name_handler_id);
		self->priv->reply_style_name_handler_id = 0;
	}

	if (self->priv->image_loading_policy_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->mail_settings,
			self->priv->image_loading_policy_handler_id);
		self->priv->image_loading_policy_handler_id = 0;
	}

	if (self->priv->show_headers_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->mail_settings,
			self->priv->show_headers_handler_id);
		self->priv->show_headers_handler_id = 0;
	}

	g_clear_object (&self->priv->calendar_settings);
	g_clear_object (&self->priv->mail_settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_settings_deprecated_parent_class)->dispose (object);
}

static void
settings_deprecated_constructed (GObject *object)
{
	ESettingsDeprecated *self = E_SETTINGS_DEPRECATED (object);
	GVariant *variant;
	gulong handler_id;
	gchar *string_value;
	gchar **strv_value;
	gint int_value;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_deprecated_parent_class)->constructed (object);

	/* Migrate values from deprecated to preferred keys. */

	int_value = g_settings_get_int (
		self->priv->calendar_settings, "week-start-day");
	g_settings_set_enum (
		self->priv->calendar_settings, "week-start-day-name",
		e_weekday_from_tm_wday (int_value));

	int_value = g_settings_get_int (
		self->priv->calendar_settings, "working-days");
	g_settings_set_boolean (
		self->priv->calendar_settings, "work-day-monday",
		(int_value & DEPRECATED_WORKING_DAYS_MONDAY) != 0);
	g_settings_set_boolean (
		self->priv->calendar_settings, "work-day-tuesday",
		(int_value & DEPRECATED_WORKING_DAYS_TUESDAY) != 0);
	g_settings_set_boolean (
		self->priv->calendar_settings, "work-day-wednesday",
		(int_value & DEPRECATED_WORKING_DAYS_WEDNESDAY) != 0);
	g_settings_set_boolean (
		self->priv->calendar_settings, "work-day-thursday",
		(int_value & DEPRECATED_WORKING_DAYS_THURSDAY) != 0);
	g_settings_set_boolean (
		self->priv->calendar_settings, "work-day-friday",
		(int_value & DEPRECATED_WORKING_DAYS_FRIDAY) != 0);
	g_settings_set_boolean (
		self->priv->calendar_settings, "work-day-saturday",
		(int_value & DEPRECATED_WORKING_DAYS_SATURDAY) != 0);
	g_settings_set_boolean (
		self->priv->calendar_settings, "work-day-sunday",
		(int_value & DEPRECATED_WORKING_DAYS_SUNDAY) != 0);

	string_value = g_settings_get_string (
		self->priv->mail_settings, "prompt-on-reply-close-browser");
	if (g_strcmp0 (string_value, "always") == 0) {
		g_settings_set_enum (
			self->priv->mail_settings,
			"browser-close-on-reply-policy",
			E_AUTOMATIC_ACTION_POLICY_ALWAYS);
	} else if (g_strcmp0 (string_value, "never") == 0) {
		g_settings_set_enum (
			self->priv->mail_settings,
			"browser-close-on-reply-policy",
			E_AUTOMATIC_ACTION_POLICY_NEVER);
	} else {
		g_settings_set_enum (
			self->priv->mail_settings,
			"browser-close-on-reply-policy",
			E_AUTOMATIC_ACTION_POLICY_ASK);
	}
	g_free (string_value);

	int_value = g_settings_get_int (
		self->priv->mail_settings, "forward-style");
	g_settings_set_enum (
		self->priv->mail_settings, "forward-style-name", int_value);

	strv_value = g_settings_get_strv (self->priv->mail_settings, "headers");
	variant = settings_deprecated_header_strv_to_variant (strv_value);
	if (variant != NULL)
		g_settings_set_value (
			self->priv->mail_settings, "show-headers", variant);
	else
		g_settings_reset (self->priv->mail_settings, "show-headers");
	g_strfreev (strv_value);

	int_value = g_settings_get_int (
		self->priv->mail_settings, "reply-style");
	g_settings_set_enum (
		self->priv->mail_settings, "reply-style-name", int_value);

	int_value = g_settings_get_int (
		self->priv->mail_settings, "load-http-images");
	g_settings_set_enum (
		self->priv->mail_settings, "image-loading-policy", int_value);

	/* Write changes back to the deprecated keys. */

	handler_id = g_signal_connect (
		self->priv->calendar_settings, "changed::week-start-day-name",
		G_CALLBACK (settings_deprecated_week_start_day_name_cb), NULL);
	self->priv->week_start_day_name_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->calendar_settings, "changed::work-day-monday",
		G_CALLBACK (settings_deprecated_work_day_monday_cb), NULL);
	self->priv->work_day_monday_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->calendar_settings, "changed::work-day-tuesday",
		G_CALLBACK (settings_deprecated_work_day_tuesday_cb), NULL);
	self->priv->work_day_tuesday_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->calendar_settings, "changed::work-day-wednesday",
		G_CALLBACK (settings_deprecated_work_day_wednesday_cb), NULL);
	self->priv->work_day_wednesday_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->calendar_settings, "changed::work-day-thursday",
		G_CALLBACK (settings_deprecated_work_day_thursday_cb), NULL);
	self->priv->work_day_thursday_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->calendar_settings, "changed::work-day-friday",
		G_CALLBACK (settings_deprecated_work_day_friday_cb), NULL);
	self->priv->work_day_friday_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->calendar_settings, "changed::work-day-saturday",
		G_CALLBACK (settings_deprecated_work_day_saturday_cb), NULL);
	self->priv->work_day_saturday_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->calendar_settings, "changed::work-day-sunday",
		G_CALLBACK (settings_deprecated_work_day_sunday_cb), NULL);
	self->priv->work_day_sunday_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->mail_settings, "changed::browser-close-on-reply-policy",
		G_CALLBACK (settings_deprecated_browser_close_on_reply_policy_cb), NULL);
	self->priv->browser_close_on_reply_policy_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->mail_settings, "changed::forward-style-name",
		G_CALLBACK (settings_deprecated_forward_style_name_cb), NULL);
	self->priv->forward_style_name_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->mail_settings, "changed::reply-style-name",
		G_CALLBACK (settings_deprecated_reply_style_name_cb), NULL);
	self->priv->reply_style_name_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->mail_settings, "changed::image-loading-policy",
		G_CALLBACK (settings_deprecated_image_loading_policy_cb), NULL);
	self->priv->image_loading_policy_handler_id = handler_id;

	handler_id = g_signal_connect (
		self->priv->mail_settings, "changed::show-headers",
		G_CALLBACK (settings_deprecated_show_headers_cb), NULL);
	self->priv->show_headers_handler_id = handler_id;
}

static void
e_settings_deprecated_class_init (ESettingsDeprecatedClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = settings_deprecated_dispose;
	object_class->constructed = settings_deprecated_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_settings_deprecated_class_finalize (ESettingsDeprecatedClass *class)
{
}

static void
e_settings_deprecated_init (ESettingsDeprecated *extension)
{
	GSettings *settings;

	extension->priv = e_settings_deprecated_get_instance_private (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	extension->priv->calendar_settings = settings;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	extension->priv->mail_settings = settings;
}

void
e_settings_deprecated_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_deprecated_register_type (type_module);
}

