/*
 * e-settings-deprecated.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

/* This class is different from the others in this module.  Its purpose
 * is to transfer values from deprecated GSettings keys to the preferred
 * keys on startup, and keep them synchronized at all times for backward
 * compatibility. */

#include "e-settings-deprecated.h"

#include <shell/e-shell.h>
#include <libemail-engine/e-mail-enums.h>

#define E_SETTINGS_DEPRECATED_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_DEPRECATED, ESettingsDeprecatedPrivate))

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
};

/* Flag values used in the "working-days" key. */
enum {
	DEPRECATED_WORKING_DAYS_SUNDAY    = 1 << 0,
	DEPRECATED_WORKING_DAYS_MONDAY    = 1 << 1,
	DEPRECATED_WORKING_DAYS_TUESDAY   = 1 << 2,
	DEPRECATED_WORKING_DAYS_WEDNESDAY = 1 << 3,
	DEPRECATED_WORKING_DAYS_THURSDAY  = 1 << 4,
	DEPRECATED_WORKING_DAYS_FRIDAY    = 1 << 5,
	DEPRECATED_WORKING_DAYS_SATURDAY  = 1 << 6
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsDeprecated,
	e_settings_deprecated,
	E_TYPE_EXTENSION)

static void
settings_deprecated_week_start_day_name_cb (GSettings *settings,
                                            const gchar *key)
{
	GDateWeekday weekday;
	gint tm_wday;

	weekday = g_settings_get_enum (settings, "week-start-day-name");
	tm_wday = e_weekday_to_tm_wday (weekday);
	g_settings_set_int (settings, "week-start-day", tm_wday);
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
	g_settings_set_int (settings, "working-days", flags);
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
	g_settings_set_int (settings, "working-days", flags);
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
	g_settings_set_int (settings, "working-days", flags);
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
	g_settings_set_int (settings, "working-days", flags);
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
	g_settings_set_int (settings, "working-days", flags);
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
	g_settings_set_int (settings, "working-days", flags);
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
	g_settings_set_int (settings, "working-days", flags);
}

static void
settings_deprecated_browser_close_on_reply_policy_cb (GSettings *settings,
                                                      const gchar *key)
{
	const gchar *deprecated_key = "prompt-on-reply-close-browser";

	switch (g_settings_get_enum (settings, key)) {
		case E_AUTOMATIC_ACTION_POLICY_ALWAYS:
			g_settings_set_string (
				settings, deprecated_key, "always");
			break;
		case E_AUTOMATIC_ACTION_POLICY_NEVER:
			g_settings_set_string (
				settings, deprecated_key, "never");
			break;
		default:
			g_settings_set_string (
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
	g_settings_set_int (settings, "forward-style", style);
}

static void
settings_deprecated_reply_style_name_cb (GSettings *settings,
                                         const gchar *key)
{
	/* XXX The "reply-style" key uses a completely different
	 *     numbering than the EMailReplyStyle enum.  *sigh* */
	switch (g_settings_get_enum (settings, "reply-style-name")) {
		case E_MAIL_REPLY_STYLE_QUOTED:
			g_settings_set_int (settings, "reply-style", 2);
			break;
		case E_MAIL_REPLY_STYLE_DO_NOT_QUOTE:
			g_settings_set_int (settings, "reply-style", 3);
			break;
		case E_MAIL_REPLY_STYLE_ATTACH:
			g_settings_set_int (settings, "reply-style", 0);
			break;
		case E_MAIL_REPLY_STYLE_OUTLOOK:
			g_settings_set_int (settings, "reply-style", 1);
			break;
		default:
			g_warn_if_reached ();
	}
}

static void
settings_deprecated_image_loading_policy_cb (GSettings *settings,
                                             const gchar *key)
{
	EMailImageLoadingPolicy policy;

	policy = g_settings_get_enum (settings, "image-loading-policy");
	g_settings_set_int (settings, "load-http-images", policy);
}

static void
settings_deprecated_dispose (GObject *object)
{
	ESettingsDeprecatedPrivate *priv;

	priv = E_SETTINGS_DEPRECATED_GET_PRIVATE (object);

	if (priv->week_start_day_name_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->week_start_day_name_handler_id);
		priv->week_start_day_name_handler_id = 0;
	}

	if (priv->work_day_monday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_monday_handler_id);
		priv->work_day_monday_handler_id = 0;
	}

	if (priv->work_day_tuesday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_tuesday_handler_id);
		priv->work_day_tuesday_handler_id = 0;
	}

	if (priv->work_day_wednesday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_wednesday_handler_id);
		priv->work_day_wednesday_handler_id = 0;
	}

	if (priv->work_day_thursday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_thursday_handler_id);
		priv->work_day_thursday_handler_id = 0;
	}

	if (priv->work_day_friday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_friday_handler_id);
		priv->work_day_friday_handler_id = 0;
	}

	if (priv->work_day_saturday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_saturday_handler_id);
		priv->work_day_saturday_handler_id = 0;
	}

	if (priv->work_day_sunday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_sunday_handler_id);
		priv->work_day_sunday_handler_id = 0;
	}

	if (priv->browser_close_on_reply_policy_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->mail_settings,
			priv->browser_close_on_reply_policy_handler_id);
		priv->browser_close_on_reply_policy_handler_id = 0;
	}

	if (priv->forward_style_name_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->mail_settings,
			priv->forward_style_name_handler_id);
		priv->forward_style_name_handler_id = 0;
	}

	if (priv->reply_style_name_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->mail_settings,
			priv->reply_style_name_handler_id);
		priv->reply_style_name_handler_id = 0;
	}

	if (priv->image_loading_policy_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->mail_settings,
			priv->image_loading_policy_handler_id);
		priv->image_loading_policy_handler_id = 0;
	}

	g_clear_object (&priv->calendar_settings);
	g_clear_object (&priv->mail_settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_settings_deprecated_parent_class)->dispose (object);
}

static void
settings_deprecated_constructed (GObject *object)
{
	ESettingsDeprecatedPrivate *priv;
	gulong handler_id;
	gchar *string_value;
	gint int_value;

	priv = E_SETTINGS_DEPRECATED_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_deprecated_parent_class)->
		constructed (object);

	/* Migrate values from deprecated to preferred keys. */

	int_value = g_settings_get_int (
		priv->calendar_settings, "week-start-day");
	g_settings_set_enum (
		priv->calendar_settings, "week-start-day-name",
		e_weekday_from_tm_wday (int_value));

	int_value = g_settings_get_int (
		priv->calendar_settings, "working-days");
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-monday",
		(int_value & DEPRECATED_WORKING_DAYS_MONDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-tuesday",
		(int_value & DEPRECATED_WORKING_DAYS_TUESDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-wednesday",
		(int_value & DEPRECATED_WORKING_DAYS_WEDNESDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-thursday",
		(int_value & DEPRECATED_WORKING_DAYS_THURSDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-friday",
		(int_value & DEPRECATED_WORKING_DAYS_FRIDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-saturday",
		(int_value & DEPRECATED_WORKING_DAYS_SATURDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-sunday",
		(int_value & DEPRECATED_WORKING_DAYS_SUNDAY) != 0);

	string_value = g_settings_get_string (
		priv->mail_settings, "prompt-on-reply-close-browser");
	if (g_strcmp0 (string_value, "always") == 0) {
		g_settings_set_enum (
			priv->mail_settings,
			"browser-close-on-reply-policy",
			E_AUTOMATIC_ACTION_POLICY_ALWAYS);
	} else if (g_strcmp0 (string_value, "never") == 0) {
		g_settings_set_enum (
			priv->mail_settings,
			"browser-close-on-reply-policy",
			E_AUTOMATIC_ACTION_POLICY_NEVER);
	} else {
		g_settings_set_enum (
			priv->mail_settings,
			"browser-close-on-reply-policy",
			E_AUTOMATIC_ACTION_POLICY_ASK);
	}
	g_free (string_value);

	int_value = g_settings_get_int (
		priv->mail_settings, "forward-style");
	g_settings_set_enum (
		priv->mail_settings, "forward-style-name", int_value);

	/* XXX The "reply-style" key uses a completely different
	 *     numbering than the EMailReplyStyle enum.  *sigh* */
	switch (g_settings_get_int (priv->mail_settings, "reply-style")) {
		case 0:
			g_settings_set_enum (
				priv->mail_settings,
				"reply-style-name",
				E_MAIL_REPLY_STYLE_ATTACH);
			break;
		case 1:
			g_settings_set_enum (
				priv->mail_settings,
				"reply-style-name",
				E_MAIL_REPLY_STYLE_OUTLOOK);
			break;
		case 2:
			g_settings_set_enum (
				priv->mail_settings,
				"reply-style-name",
				E_MAIL_REPLY_STYLE_QUOTED);
			break;
		case 3:
			g_settings_set_enum (
				priv->mail_settings,
				"reply-style-name",
				E_MAIL_REPLY_STYLE_DO_NOT_QUOTE);
			break;
		default:
			/* do nothing */
			break;
	}

	int_value = g_settings_get_int (
		priv->mail_settings, "load-http-images");
	g_settings_set_enum (
		priv->mail_settings, "image-loading-policy", int_value);

	/* Write changes back to the deprecated keys. */

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::week-start-day-name",
		G_CALLBACK (settings_deprecated_week_start_day_name_cb), NULL);
	priv->week_start_day_name_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-monday",
		G_CALLBACK (settings_deprecated_work_day_monday_cb), NULL);
	priv->work_day_monday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-tuesday",
		G_CALLBACK (settings_deprecated_work_day_tuesday_cb), NULL);
	priv->work_day_tuesday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-wednesday",
		G_CALLBACK (settings_deprecated_work_day_wednesday_cb), NULL);
	priv->work_day_wednesday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-thursday",
		G_CALLBACK (settings_deprecated_work_day_thursday_cb), NULL);
	priv->work_day_thursday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-friday",
		G_CALLBACK (settings_deprecated_work_day_friday_cb), NULL);
	priv->work_day_friday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-saturday",
		G_CALLBACK (settings_deprecated_work_day_saturday_cb), NULL);
	priv->work_day_saturday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-sunday",
		G_CALLBACK (settings_deprecated_work_day_sunday_cb), NULL);
	priv->work_day_sunday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->mail_settings, "changed::browser-close-on-reply-policy",
		G_CALLBACK (settings_deprecated_browser_close_on_reply_policy_cb), NULL);
	priv->browser_close_on_reply_policy_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->mail_settings, "changed::forward-style-name",
		G_CALLBACK (settings_deprecated_forward_style_name_cb), NULL);
	priv->forward_style_name_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->mail_settings, "changed::reply-style-name",
		G_CALLBACK (settings_deprecated_reply_style_name_cb), NULL);
	priv->reply_style_name_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->mail_settings, "changed::image-loading-policy",
		G_CALLBACK (settings_deprecated_image_loading_policy_cb), NULL);
}

static void
e_settings_deprecated_class_init (ESettingsDeprecatedClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsDeprecatedPrivate));

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

	extension->priv = E_SETTINGS_DEPRECATED_GET_PRIVATE (extension);

	settings = g_settings_new ("org.gnome.evolution.calendar");
	extension->priv->calendar_settings = settings;

	settings = g_settings_new ("org.gnome.evolution.mail");
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

