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

struct _ECalComponentPreviewPrivate {
	EAttachmentStore *attachment_store;
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

G_DEFINE_TYPE_WITH_PRIVATE (ECalComponentPreview, e_cal_component_preview, E_TYPE_WEB_VIEW)

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

	if (changed && priv->attachment_store)
		e_attachment_store_remove_all (priv->attachment_store);

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
cal_component_preview_attachment_loaded (EAttachment *attachment,
					 GAsyncResult *result,
					 GWeakRef *weak_ref)
{
	ECalComponentPreview *self;

	self = g_weak_ref_get (weak_ref);
	if (self) {
		GFileInfo *file_info;
		gpointer parent;

		file_info = e_attachment_ref_file_info (attachment);
		if (file_info) {
			const gchar *prefer_filename;

			prefer_filename = g_object_get_data (G_OBJECT (attachment), "prefer-filename");

			if (prefer_filename && *prefer_filename) {
				g_file_info_set_display_name (file_info, prefer_filename);
				g_object_notify (G_OBJECT (attachment), "file-info");
			}

			g_object_unref (file_info);
		}

		parent = gtk_widget_get_toplevel (GTK_WIDGET (self));
		parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

		e_attachment_load_handle_error (attachment, result, parent);

		g_object_unref (self);
	}

	e_weak_ref_free (weak_ref);
}

static EAttachment *
cal_component_preview_create_attachment (ECalComponentPreview *self,
					 ICalProperty *attach_prop)
{
	EAttachment *attachment = NULL;
	ICalAttach *attach;
	gpointer parent;

	g_return_val_if_fail (I_CAL_IS_PROPERTY (attach_prop), NULL);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (self));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	attach = i_cal_property_get_attach (attach_prop);
	if (attach) {
		gchar *uri = NULL, *filename;

		filename = cal_comp_util_dup_attach_filename (attach_prop, TRUE);

		if (i_cal_attach_get_is_url (attach)) {
			const gchar *data;

			data = i_cal_attach_get_url (attach);
			uri = i_cal_value_decode_ical_string (data);
		} else {
			ICalParameter *encoding_par = i_cal_property_get_first_parameter (attach_prop, I_CAL_ENCODING_PARAMETER);
			if (encoding_par) {
				gchar *str_value = i_cal_property_get_value_as_string (attach_prop);

				if (str_value) {
					ICalParameterEncoding encoding = i_cal_parameter_get_encoding (encoding_par);
					guint8 *data = NULL;
					gsize data_len = 0;

					switch (encoding) {
					case I_CAL_ENCODING_8BIT:
						data = (guint8 *) str_value;
						data_len = strlen (str_value);
						str_value = NULL;
						break;
					case I_CAL_ENCODING_BASE64:
						data = g_base64_decode (str_value, &data_len);
						break;
					default:
						break;
					}

					if (data) {
						CamelMimePart *mime_part;
						ICalParameter *param;
						const gchar *fmttype = NULL;

						param = i_cal_property_get_first_parameter (attach_prop, I_CAL_FMTTYPE_PARAMETER);
						if (param) {
							fmttype = i_cal_parameter_get_fmttype (param);
							if (fmttype && !*fmttype)
								fmttype = NULL;
						}

						if (!fmttype || !*fmttype)
							fmttype = "application/octet-stream";

						mime_part = camel_mime_part_new ();

						camel_mime_part_set_content (mime_part, (const gchar *) data, data_len, fmttype);
						camel_mime_part_set_disposition (mime_part, "attachment");
						if (filename && *filename)
							camel_mime_part_set_filename (mime_part, filename);
						camel_mime_part_set_encoding (mime_part, CAMEL_TRANSFER_ENCODING_8BIT);

						attachment = e_attachment_new ();
						e_attachment_set_mime_part (attachment, mime_part);
						e_attachment_load_async (attachment, (GAsyncReadyCallback) e_attachment_load_handle_error, parent);

						g_object_unref (mime_part);
						g_clear_object (&param);
					}

					g_free (str_value);
					g_free (data);
				}

				g_object_unref (encoding_par);
			}
		}

		if (uri) {
			GFileInfo *file_info;
			ICalParameter *param;

			attachment = e_attachment_new_for_uri (uri);

			file_info = g_file_info_new ();
			g_file_info_set_content_type (file_info, "application/octet-stream");

			param = i_cal_property_get_first_parameter (attach_prop, I_CAL_FMTTYPE_PARAMETER);
			if (param) {
				const gchar *fmttype;

				fmttype = i_cal_parameter_get_fmttype (param);
				if (fmttype && *fmttype)
					g_file_info_set_content_type (file_info, fmttype);

				g_clear_object (&param);
			}

			if (g_ascii_strncasecmp (uri, "http://", 7) == 0 ||
			    g_ascii_strncasecmp (uri, "https://", 8) == 0 ||
			    g_ascii_strncasecmp (uri, "ftp://", 6) == 0) {
				GIcon *icon;

				icon = g_themed_icon_new ("emblem-web");

				g_file_info_set_icon (file_info, icon);

				g_clear_object (&icon);
			}

			if (filename && *filename)
				g_file_info_set_display_name (file_info, filename);

			e_attachment_set_file_info (attachment, file_info);

			g_clear_object (&file_info);

			if (g_ascii_strncasecmp (uri, "file://", 7) == 0) {
				if (filename && *filename)
					g_object_set_data_full (G_OBJECT (attachment), "prefer-filename", g_steal_pointer (&filename), g_free);
				e_attachment_load_async (attachment, (GAsyncReadyCallback) cal_component_preview_attachment_loaded, e_weak_ref_new (self));
			}
		}

		g_object_unref (attach);
		g_free (filename);
		g_free (uri);
	}

	return attachment;
}

static void
load_comp (ECalComponentPreview *preview)
{
	GString *buffer;

	if (!preview->priv->comp) {
		e_cal_component_preview_clear (preview);
		return;
	}

	if (preview->priv->attachment_store)
		e_attachment_store_remove_all (preview->priv->attachment_store);

	buffer = g_string_sized_new (4096);
	cal_component_preview_write_html (preview, buffer);
	e_web_view_load_string (E_WEB_VIEW (preview), buffer->str);
	g_string_free (buffer, TRUE);

	if (preview->priv->attachment_store) {
		ICalComponent *icomp;
		ICalProperty *prop;

		e_attachment_store_remove_all (preview->priv->attachment_store);

		icomp = e_cal_component_get_icalcomponent (preview->priv->comp);

		for (prop = i_cal_component_get_first_property (icomp, I_CAL_ATTACH_PROPERTY);
		     prop;
		     g_object_unref (prop), prop = i_cal_component_get_next_property (icomp, I_CAL_ATTACH_PROPERTY)) {
			EAttachment *attachment;

			attachment = cal_component_preview_create_attachment (preview, prop);
			if (attachment) {
				e_attachment_store_add_attachment (preview->priv->attachment_store, attachment);
				g_object_unref (attachment);
			}
		}
	}
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
	ECalComponentPreview *preview = E_CAL_COMPONENT_PREVIEW (object);

	clear_comp_info (preview);

	g_clear_object (&preview->priv->attachment_store);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_component_preview_parent_class)->finalize (object);
}

static void
e_cal_component_preview_class_init (ECalComponentPreviewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = cal_component_preview_finalize;
}

static void
e_cal_component_preview_init (ECalComponentPreview *preview)
{
	preview->priv = e_cal_component_preview_get_instance_private (preview);

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

	if (preview->priv->attachment_store)
		e_attachment_store_remove_all (preview->priv->attachment_store);
}

void
e_cal_component_preview_set_attachment_store (ECalComponentPreview *preview,
					      EAttachmentStore *store)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));

	if (preview->priv->attachment_store == store)
		return;

	g_set_object (&preview->priv->attachment_store, store);
	load_comp (preview);
}

EAttachmentStore *
e_cal_component_preview_get_attachment_store (ECalComponentPreview *preview)
{
	g_return_val_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview), NULL);

	return preview->priv->attachment_store;
}
