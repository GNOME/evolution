/*
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
 * Authors:
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <camel/camel.h>

#include "shell/e-shell-utils.h"

#include "calendar-config.h"
#include "comp-util.h"
#include "e-calendar-view.h"
#include "itip-utils.h"

#include "e-cal-component-preview.h"

#define E_CAL_COMPONENT_PREVIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_COMPONENT_PREVIEW, ECalComponentPreviewPrivate))

G_DEFINE_TYPE (
	ECalComponentPreview,
	e_cal_component_preview,
	E_TYPE_WEB_VIEW)

struct _ECalComponentPreviewPrivate {
	/* information about currently showing component in a preview;
	 * if it didn't change then the preview is not updated */
	gchar *cal_uid;
	gchar *comp_uid;
	ICalTime *comp_last_modified;
	gint comp_sequence;

	ECalClient *client;
	ECalComponent *comp;
	ICalTimezone *timezone;
	gboolean use_24_hour_format;
};

#define HTML_HEADER "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<html>\n" \
                    "<head>\n<meta name=\"generator\" content=\"Evolution Calendar Component\">\n" \
		    "<meta name=\"color-scheme\" content=\"light dark\">\n" \
		    "<link type=\"text/css\" rel=\"stylesheet\" href=\"evo-file://$EVOLUTION_WEBKITDATADIR/webview.css\">\n" \
		    "</head>"

static void
clear_comp_info (ECalComponentPreview *preview)
{
	ECalComponentPreviewPrivate *priv;

	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));

	priv = preview->priv;

	g_free (priv->cal_uid);
	priv->cal_uid = NULL;
	g_free (priv->comp_uid);
	priv->comp_uid = NULL;
	priv->comp_sequence = -1;

	g_clear_object (&priv->comp_last_modified);
	g_clear_object (&priv->client);
	g_clear_object (&priv->comp);
	g_clear_object (&priv->timezone);
}

/* Stores information about actually shown component and
 * returns whether component in the preview changed */
static gboolean
update_comp_info (ECalComponentPreview *preview,
		  ECalClient *client,
		  ECalComponent *comp,
		  ICalTimezone *zone,
		  gboolean use_24_hour_format)
{
	ECalComponentPreviewPrivate *priv;
	gboolean changed;

	g_return_val_if_fail (preview != NULL, TRUE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview), TRUE);

	priv = preview->priv;

	if (!E_IS_CAL_COMPONENT (comp) || !E_IS_CAL_CLIENT (client)) {
		changed = !priv->cal_uid;
		clear_comp_info (preview);
	} else {
		ESource *source;
		const gchar *uid;
		gchar *cal_uid;
		gchar *comp_uid;
		ICalTime *comp_last_modified;
		gint comp_sequence;

		source = e_client_get_source (E_CLIENT (client));
		cal_uid = g_strdup (e_source_get_uid (source));
		uid = e_cal_component_get_uid (comp);
		comp_uid = g_strdup (uid);
		comp_last_modified = e_cal_component_get_last_modified (comp);
		comp_sequence = e_cal_component_get_sequence (comp);
		if (comp_sequence < 0)
			comp_sequence = 0;

		changed = !priv->cal_uid || !priv->comp_uid || !cal_uid || !comp_uid ||
			  !g_str_equal (priv->cal_uid, cal_uid) ||
			  !g_str_equal (priv->comp_uid, comp_uid) ||
			  priv->comp_sequence != comp_sequence ||
			  priv->comp != comp ||
			  priv->client != client;

		if (comp_last_modified && priv->comp_last_modified)
			changed = changed || i_cal_time_compare (priv->comp_last_modified, comp_last_modified) != 0;
		else
			changed = changed || comp_last_modified != priv->comp_last_modified;

		clear_comp_info (preview);

		priv->cal_uid = cal_uid;
		priv->comp_uid = comp_uid;
		priv->comp_sequence = comp_sequence;
		priv->comp_last_modified = comp_last_modified;

		priv->comp = g_object_ref (comp);
		priv->client = g_object_ref (client);
		priv->timezone = i_cal_timezone_copy (zone);
		priv->use_24_hour_format = use_24_hour_format;
	}

	return changed;
}

static void
cal_component_preview_write_html (ECalComponentPreview *preview,
                                  GString *buffer)
{
	g_string_append (buffer, HTML_HEADER);
	g_string_append (buffer, "<body class=\"-e-web-view-background-color -e-web-view-text-color calpreview\">");

	cal_comp_util_write_to_html (buffer, preview->priv->client, preview->priv->comp, preview->priv->timezone, E_COMP_TO_HTML_FLAG_ALLOW_ICONS);

	/* close document */
	g_string_append (buffer, "</body></html>");
}

static void
load_comp (ECalComponentPreview *preview)
{
	GString *buffer;

	if (!preview->priv->comp) {
		e_cal_component_preview_clear (preview);
		return;
	}

	buffer = g_string_sized_new (4096);
	cal_component_preview_write_html (preview, buffer);
	e_web_view_load_string (E_WEB_VIEW (preview), buffer->str);
	g_string_free (buffer, TRUE);
}

static void
cal_component_preview_web_process_terminated_cb (ECalComponentPreview *preview,
						 WebKitWebProcessTerminationReason reason)
{
	EAlertSink *alert_sink;
	const gchar *tagid;

	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));

	tagid = "system:webkit-web-process-crashed";

	if (preview->priv->comp) {
		ECalComponentVType vtype;

		vtype = e_cal_component_get_vtype (preview->priv->comp);
		if (vtype == E_CAL_COMPONENT_EVENT)
			tagid = "calendar:webkit-web-process-crashed-event";
		else if (vtype == E_CAL_COMPONENT_TODO)
			tagid = "calendar:webkit-web-process-crashed-task";
		else if (vtype == E_CAL_COMPONENT_JOURNAL)
			tagid = "calendar:webkit-web-process-crashed-memo";
	}

	/* Cannot use the EWebView, because it places the alerts inside itself */
	alert_sink = e_shell_utils_find_alternate_alert_sink (GTK_WIDGET (preview));
	if (alert_sink)
		e_alert_submit (alert_sink, tagid, NULL);
}

static void
cal_component_preview_finalize (GObject *object)
{
	clear_comp_info (E_CAL_COMPONENT_PREVIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_component_preview_parent_class)->finalize (object);
}

static void
e_cal_component_preview_class_init (ECalComponentPreviewClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ECalComponentPreviewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = cal_component_preview_finalize;
}

static void
e_cal_component_preview_init (ECalComponentPreview *preview)
{
	preview->priv = E_CAL_COMPONENT_PREVIEW_GET_PRIVATE (preview);

	g_signal_connect (
		preview, "web-process-terminated",
		G_CALLBACK (cal_component_preview_web_process_terminated_cb), NULL);
}

GtkWidget *
e_cal_component_preview_new (void)
{
	return g_object_new (E_TYPE_CAL_COMPONENT_PREVIEW, NULL);
}

void
e_cal_component_preview_display (ECalComponentPreview *preview,
                                 ECalClient *client,
                                 ECalComponent *comp,
                                 ICalTimezone *zone,
                                 gboolean use_24_hour_format)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	/* do not update preview when setting the same component as last time,
	 * which even didn't change */
	if (!update_comp_info (preview, client, comp, zone, use_24_hour_format))
		return;

	load_comp (preview);
}

void
e_cal_component_preview_clear (ECalComponentPreview *preview)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));

	clear_comp_info (preview);
	e_web_view_clear (E_WEB_VIEW (preview));
}
