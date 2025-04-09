/*
 * SPDX-FileCopyrightText: (C) 2025 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>
#include <libecal/libecal.h>

#include "e-util/e-util.h"
#include "e-calendar-view.h"
#include "comp-util.h"

#include "e-cal-component-widget.h"

/**
 * SECTION: e-cal-component-widget
 * @include: calendar/gui/e-cal-component-widget.h
 * @short_description: a widget representing an ECalComponent
 *
 * The #ECalComponentWidget represents an #ECalComponent in the user
 * interface.
 *
 * The object is not thread-safe, it's meant to be used only from
 * the main/GUI thread.
 *
 * Since: 3.58
 **/

struct _ECalComponentWidget {
	GtkBox parent;

	ECalClient *client;
	ECalComponent *component;
	ESourceRegistry *registry;

	GtkCssProvider *css_provider;
	GtkWidget *time_label;
	GtkWidget *text_label;
	gint row_height;
	gboolean with_transparency;
};

G_DEFINE_TYPE (ECalComponentWidget, e_cal_component_widget, GTK_TYPE_BOX)

enum {
	PROP_0,
	PROP_CLIENT,
	PROP_COMPONENT,
	PROP_REGISTRY,
	PROP_TIME_VISIBLE,
	PROP_WITH_TRANSPARENCY,
	N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
e_cal_component_widget_update_styles (ECalComponentWidget *self)
{
	ICalComponent *icomp;
	GdkRGBA bg_color = { 0, }, fg_color;
	PangoLayout *layout;
	gint height = 0;
	gchar *str = NULL;
	GError *local_error = NULL;

	layout = gtk_widget_create_pango_layout (self->text_label, NULL);
	/* just some random string with an accent and a part below the base line */
	pango_layout_set_text (layout, "Řjgm", -1);
	pango_layout_get_pixel_size (layout, NULL, &height);
	g_clear_object (&layout);

	self->row_height = MAX (height, 10);

	gtk_label_set_lines (GTK_LABEL (self->text_label), MAX (gtk_widget_get_allocated_height (self->text_label) /  self->row_height, 1));

	icomp = e_cal_component_get_icalcomponent (self->component);

	cal_comp_util_set_color_for_component (self->client, icomp, &str);

	if (!str || !gdk_rgba_parse (&bg_color, str)) {
		g_free (str);
		return;
	}

	fg_color = e_utils_get_text_color_for_background (&bg_color);

	g_free (str);

	#define color_hex(_val) (((gint32) (255 * (_val))) & 0xFF)

	str = g_strdup_printf ("ECalComponentWidget label { color: #%02x%02x%02x; } "
		"ECalComponentWidget { background: #%02x%02x%02x; border: solid 1px #%02x%02x%02x; } "
		"ECalComponentWidget.transparent { opacity: 0.6; }",
		color_hex (fg_color.red), color_hex (fg_color.green), color_hex (fg_color.blue),
		color_hex (bg_color.red), color_hex (bg_color.green), color_hex (bg_color.blue),
		color_hex (fg_color.red), color_hex (fg_color.green), color_hex (fg_color.blue));

	#undef color_hex

	if (!gtk_css_provider_load_from_data (self->css_provider, str, -1, &local_error))
		g_warning ("%s: Failed to set CSS \"%s\": %s", G_STRFUNC, str, local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);
	g_free (str);
}

static void
e_cal_component_widget_show_all (GtkWidget *widget)
{
	gtk_widget_set_visible (widget, TRUE);
}

static void
e_cal_component_widget_style_updated (GtkWidget *widget)
{
	ECalComponentWidget *self = E_CAL_COMPONENT_WIDGET (widget);

	GTK_WIDGET_CLASS (e_cal_component_widget_parent_class)->style_updated (widget);

	e_cal_component_widget_update_styles (self);
}

static void
e_cal_component_widget_size_allocate (GtkWidget *widget,
				      GtkAllocation *allocation)
{
	ECalComponentWidget *self = E_CAL_COMPONENT_WIDGET (widget);

	GTK_WIDGET_CLASS (e_cal_component_widget_parent_class)->size_allocate (widget, allocation);

	gtk_label_set_lines (GTK_LABEL (self->text_label), MAX (allocation->height / self->row_height, 1));
}

static void
e_cal_component_widget_set_property (GObject *object,
				     guint prop_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	ECalComponentWidget *self = E_CAL_COMPONENT_WIDGET (object);

	switch (prop_id) {
	case PROP_CLIENT:
		g_set_object (&self->client, g_value_get_object (value));
		break;
	case PROP_COMPONENT:
		g_set_object (&self->component, g_value_get_object (value));
		break;
	case PROP_REGISTRY:
		g_set_object (&self->registry, g_value_get_object (value));
		break;
	case PROP_TIME_VISIBLE:
		e_cal_component_widget_set_time_visible (self, g_value_get_boolean (value));
		break;
	case PROP_WITH_TRANSPARENCY:
		e_cal_component_widget_set_with_transparency (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_cal_component_widget_get_property (GObject *object,
				     guint prop_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	ECalComponentWidget *self = E_CAL_COMPONENT_WIDGET (object);

	switch (prop_id) {
	case PROP_CLIENT:
		g_value_set_object (value, e_cal_component_widget_get_client (self));
		break;
	case PROP_COMPONENT:
		g_value_set_object (value, e_cal_component_widget_get_component (self));
		break;
	case PROP_REGISTRY:
		g_value_set_object (value, e_cal_component_widget_get_registry (self));
		break;
	case PROP_TIME_VISIBLE:
		g_value_set_boolean (value, e_cal_component_widget_get_time_visible (self));
		break;
	case PROP_WITH_TRANSPARENCY:
		g_value_set_boolean (value, e_cal_component_widget_get_with_transparency (self));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_cal_component_widget_constructed (GObject *object)
{
	ECalComponentWidget *self = E_CAL_COMPONENT_WIDGET (object);
	GtkStyleContext *style_context;

	G_OBJECT_CLASS (e_cal_component_widget_parent_class)->constructed (object);

	g_object_set (object,
		"orientation", GTK_ORIENTATION_HORIZONTAL,
		"spacing", 2,
		"visible", TRUE,
		NULL);

	self->css_provider = gtk_css_provider_new ();

	style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (style_context, "view");
	gtk_style_context_add_provider (style_context,
		GTK_STYLE_PROVIDER (self->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	self->time_label = gtk_label_new ("");
	g_object_set (self->time_label,
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		"xalign", 0.0,
		"yalign", 0.0,
		"visible", TRUE,
		"margin-start", 2,
		"margin-end", 2,
		"margin-top", 2,
		"margin-bottom", 2,
		"single-line-mode", TRUE,
		NULL);

	style_context = gtk_widget_get_style_context (self->time_label);
	gtk_style_context_add_class (style_context, "etime");
	gtk_style_context_add_provider (style_context,
		GTK_STYLE_PROVIDER (self->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	self->text_label = gtk_label_new ("");
	g_object_set (self->text_label,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"xalign", 0.0,
		"yalign", 0.0,
		"visible", TRUE,
		"margin-start", 2,
		"margin-end", 2,
		"margin-top", 2,
		"margin-bottom", 2,
		"ellipsize", PANGO_ELLIPSIZE_END,
		"lines", 1,
		"max-width-chars", 60,
		"width-chars", 7,
		"wrap", TRUE,
		NULL);
	style_context = gtk_widget_get_style_context (self->text_label);
	gtk_style_context_add_class (style_context, "etext");
	gtk_style_context_add_provider (style_context,
		GTK_STYLE_PROVIDER (self->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_box_pack_start (GTK_BOX (self), self->time_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (self), self->text_label, TRUE, TRUE, 0);

	e_cal_component_widget_update (self);
}

static void
e_cal_component_widget_finalize (GObject *object)
{
	ECalComponentWidget *self = E_CAL_COMPONENT_WIDGET (object);

	g_clear_object (&self->client);
	g_clear_object (&self->component);
	g_clear_object (&self->registry);
	g_clear_object (&self->css_provider);

	G_OBJECT_CLASS (e_cal_component_widget_parent_class)->finalize (object);
}

static void
e_cal_component_widget_class_init (ECalComponentWidgetClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_cal_component_widget_set_property;
	object_class->get_property = e_cal_component_widget_get_property;
	object_class->constructed = e_cal_component_widget_constructed;
	object_class->finalize = e_cal_component_widget_finalize;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->show_all = e_cal_component_widget_show_all;
	widget_class->style_updated = e_cal_component_widget_style_updated;
	widget_class->size_allocate = e_cal_component_widget_size_allocate;

	/**
	 * ECalComponentWidget:client:
	 *
	 * An #ECalClient of the set component. It can be set only during construction.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_CLIENT] = g_param_spec_object ("client", NULL, NULL,
		E_TYPE_CAL_CLIENT,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * ECalComponentWidget:component:
	 *
	 * An #ECalComponent being set on the widget. It can be set only during construction.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_COMPONENT] = g_param_spec_object ("component", NULL, NULL,
		E_TYPE_CAL_COMPONENT,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * ECalComponentWidget:registry:
	 *
	 * An #ESourceRegistry. It can be set only during construction.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_REGISTRY] = g_param_spec_object ("registry", NULL, NULL,
		E_TYPE_SOURCE_REGISTRY,
		G_PARAM_READWRITE |
		G_PARAM_CONSTRUCT_ONLY |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * ECalComponentWidget:time-visible:
	 *
	 * Whether the time part is shown.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_TIME_VISIBLE] = g_param_spec_boolean ("time-visible", NULL, NULL, TRUE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	/**
	 * ECalComponentWidget:with-transparency:
	 *
	 * Whether to use transparency to "highlight" that the component is set transparent.
	 * The default value is to use the transparency.
	 *
	 * Since: 3.58
	 **/
	properties[PROP_WITH_TRANSPARENCY] = g_param_spec_boolean ("with-transparency", NULL, NULL, TRUE,
		G_PARAM_READWRITE |
		G_PARAM_STATIC_STRINGS |
		G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

	gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "ECalComponentWidget");
}

static void
e_cal_component_widget_init (ECalComponentWidget *self)
{
	self->row_height = 10;
	self->with_transparency = TRUE;
}

/**
 * e_cal_component_widget_new:
 * @client: an #ECalClient
 * @component: an #ECalComponent
 * @registry: an #ESourceRegistry
 *
 * Creates a new #ECalComponentWidget, which will show
 * the @component information from the @client. The @registry
 * is used to get proper tooltip.
 *
 * Returns: (transfer full): a new #ECalComponentWidget
 **/
GtkWidget *
e_cal_component_widget_new (ECalClient *client,
			    ECalComponent *component,
			    ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_CAL_CLIENT (client), NULL);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (component), NULL);
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (E_TYPE_CAL_COMPONENT_WIDGET,
		"client", client,
		"component", component,
		"registry", registry,
		NULL);
}

/**
 * e_cal_component_widget_get_client:
 * @self: an #ECalComponentWidget
 *
 * Gets an #ECalClient the @self was created with.
 *
 * Returns: (transfer none): an #ECalClient
 *
 * Since: 3.58
 **/
ECalClient *
e_cal_component_widget_get_client (ECalComponentWidget *self)
{
	g_return_val_if_fail (E_IS_CAL_COMPONENT_WIDGET (self), NULL);

	return self->client;
}

/**
 * e_cal_component_widget_get_component:
 * @self: an #ECalComponentWidget
 *
 * Gets an #ECalComponent the @self was created with.
 *
 * Returns: (transfer none): an #ECalComponent
 *
 * Since: 3.58
 **/
ECalComponent *
e_cal_component_widget_get_component (ECalComponentWidget *self)
{
	g_return_val_if_fail (E_IS_CAL_COMPONENT_WIDGET (self), NULL);

	return self->component;
}

/**
 * e_cal_component_widget_update_component:
 * @self: an #ECalComponentWidget
 * @client: an #ECalClient
 * @component: an #ECalComponent
 *
 * Updates the internal @client and @component. It should be used when
 * the instances changed.
 *
 * This also calls e_cal_component_widget_update().
 *
 * Since: 3.56
 **/
void
e_cal_component_widget_update_component (ECalComponentWidget *self,
					 ECalClient *client,
					 ECalComponent *component)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_WIDGET (self));
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (E_IS_CAL_COMPONENT (component));

	g_set_object (&self->client, client);
	g_set_object (&self->component, component);

	e_cal_component_widget_update (self);
}

/**
 * e_cal_component_widget_get_registry:
 * @self: an #ECalComponentWidget
 *
 * Gets an #ESourceRegistry the @self was created with.
 *
 * Returns: (transfer none): an #ESourceRegistry
 *
 * Since: 3.58
 **/
ESourceRegistry *
e_cal_component_widget_get_registry (ECalComponentWidget *self)
{
	g_return_val_if_fail (E_IS_CAL_COMPONENT_WIDGET (self), NULL);

	return self->registry;
}

/**
 * e_cal_component_widget_get_time_visible:
 * @self: an #ECalComponentWidget
 *
 * Gets whether the time part is visible.
 *
 * Returns: whether the time part is visible
 *
 * Since: 3.58
 **/
gboolean
e_cal_component_widget_get_time_visible (ECalComponentWidget *self)
{
	g_return_val_if_fail (E_IS_CAL_COMPONENT_WIDGET (self), FALSE);

	return gtk_widget_get_visible (self->time_label);
}

/**
 * e_cal_component_widget_set_time_visible:
 * @self: an #ECalComponentWidget
 * @value: value to set
 *
 * Sets whether the time part is visible.
 *
 * Since: 3.58
 **/
void
e_cal_component_widget_set_time_visible (ECalComponentWidget *self,
					 gboolean value)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_WIDGET (self));

	if ((!gtk_widget_get_visible (self->time_label)) == (!value))
		return;

	gtk_widget_set_visible (self->time_label, value);

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIME_VISIBLE]);
}

/**
 * e_cal_component_widget_get_with_transparency:
 * @self: an #ECalComponentWidget
 *
 * Gets whether the transparent components should be "highlighted" with transparency.
 *
 * Returns: whether the transparency is used
 *
 * Since: 3.58
 **/
gboolean
e_cal_component_widget_get_with_transparency (ECalComponentWidget *self)
{
	g_return_val_if_fail (E_IS_CAL_COMPONENT_WIDGET (self), FALSE);

	return self->with_transparency;
}

/**
 * e_cal_component_widget_set_with_transparency:
 * @self: an #ECalComponentWidget
 * @value: value to set
 *
 * Sets whether the transparent components should be "highlighted" with transparency.
 *
 * Since: 3.58
 **/
void
e_cal_component_widget_set_with_transparency (ECalComponentWidget *self,
					      gboolean value)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_WIDGET (self));

	if ((!self->with_transparency) == (!value))
		return;

	self->with_transparency = value;

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WITH_TRANSPARENCY]);
}

/**
 * e_cal_component_widget_update:
 * @self: an #ECalComponentWidget
 *
 * Forces update of the @self content. It's for example when the component
 * it had been created with changed.
 *
 * Since: 3.58
 **/
void
e_cal_component_widget_update (ECalComponentWidget *self)
{
	ICalComponent *icomp;
	gchar *str;

	g_return_if_fail (E_IS_CAL_COMPONENT_WIDGET (self));
	g_return_if_fail (E_IS_CAL_COMPONENT (self->component));

	e_cal_component_widget_update_styles (self);

	icomp = e_cal_component_get_icalcomponent (self->component);

	str = e_calendar_view_dup_component_summary (icomp);
	gtk_label_set_label (GTK_LABEL (self->text_label), str ? str : _("No Summary"));
	g_free (str);

	str = cal_comp_util_dup_tooltip (self->component, self->client, self->registry, e_cal_client_get_default_timezone (self->client));
	gtk_widget_set_tooltip_markup (GTK_WIDGET (self), str);
	g_free (str);

	if (self->with_transparency && e_cal_component_get_transparency (self->component) == E_CAL_COMPONENT_TRANSP_TRANSPARENT)
		gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "transparent");
	else
		gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (self)), "transparent");
}
