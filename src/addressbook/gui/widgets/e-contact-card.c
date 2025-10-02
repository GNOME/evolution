/*
 * SPDX-FileCopyrightText: (C) 2023 Red Hat (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <gtk/gtk-a11y.h>
#include <libebook/libebook.h>

#include "eab-book-util.h"
#include "eab-gui-util.h"

#include "e-contact-card.h"

#define N_ROWS 5

#define E_TYPE_CONTACT_CARD_ACCESSIBLE e_contact_card_accessible_get_type ()
G_DECLARE_FINAL_TYPE (EContactCardAccessible, e_contact_card_accessible, E, CONTACT_CARD_ACCESSIBLE, GtkContainerAccessible)

struct _EContactCardAccessible {
	GtkContainerAccessible parent;
};

G_DEFINE_TYPE (EContactCardAccessible, e_contact_card_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE)

static void
e_contact_card_accessible_init (EContactCardAccessible *accessible)
{
}

static void
e_contact_card_accessible_initialize (AtkObject *obj,
				      gpointer   data)
{
	ATK_OBJECT_CLASS (e_contact_card_accessible_parent_class)->initialize (obj, data);
	obj->role = ATK_ROLE_TABLE_CELL;
}

static AtkStateSet *
e_contact_card_accessible_ref_state_set (AtkObject *obj)
{
	AtkStateSet *state_set;
	GtkWidget *widget;

	state_set = ATK_OBJECT_CLASS (e_contact_card_accessible_parent_class)->ref_state_set (obj);
	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
	if (widget) {
		atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);

		if ((gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_SELECTED) != 0)
			atk_state_set_add_state (state_set, ATK_STATE_SELECTED);
	}

	return state_set;
}

static void
e_contact_card_accessible_class_init (EContactCardAccessibleClass *klass)
{
	AtkObjectClass *object_class = ATK_OBJECT_CLASS (klass);

	object_class->initialize = e_contact_card_accessible_initialize;
	object_class->ref_state_set = e_contact_card_accessible_ref_state_set;
}

/* ************************************************************************* */

typedef struct _Row {
	GtkLabel *label;
	GtkLabel *value;
} Row;

struct _EContactCardPrivate {
	EContact *contact;
	GtkCssProvider *css_provider;

	GtkLabel *header;
	GtkWidget *spinner;
	GtkImage *image;
	GtkWidget *rows_grid;
	Row rows[N_ROWS];

	gboolean have_image;
};

enum {
	PROP_0,
	PROP_CSS_PROVIDER,
	LAST_PROP
};

static GParamSpec *obj_props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (EContactCard, e_contact_card, GTK_TYPE_EVENT_BOX)

static void
e_contact_card_get_preferred_width (GtkWidget *widget,
				    gint *minimum_width,
				    gint *natural_width)
{
	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_contact_card_parent_class)->get_preferred_width (widget, minimum_width, natural_width);

	if ((*minimum_width) > 321)
		*minimum_width = 321;
	if ((*natural_width) > 321)
		*natural_width = 321;
}

static void
e_contact_card_get_preferred_width_for_height (GtkWidget *widget,
					       gint height,
					       gint *minimum_width,
					       gint *natural_width)
{
	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_contact_card_parent_class)->get_preferred_width_for_height (widget, height, minimum_width, natural_width);

	if ((*minimum_width) > 321)
		*minimum_width = 321;
	if ((*natural_width) > 321)
		*natural_width = 321;
}

static void
e_contact_card_show_all (GtkWidget *widget)
{
	EContactCard *self = E_CONTACT_CARD (widget);

	/* Chain up to parent's method. */
	GTK_WIDGET_CLASS (e_contact_card_parent_class)->show_all (widget);

	gtk_widget_set_visible (self->priv->spinner, !self->priv->contact);
	gtk_widget_set_visible (GTK_WIDGET (self->priv->image), self->priv->contact && self->priv->have_image);
}

static void
e_contact_card_set_property (GObject *object,
			     guint property_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	EContactCard *self = E_CONTACT_CARD (object);

	switch (property_id) {
	case PROP_CSS_PROVIDER:
		g_clear_object (&self->priv->css_provider);
		self->priv->css_provider = g_value_dup_object (value);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_contact_card_get_property (GObject *object,
			     guint property_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	EContactCard *self = E_CONTACT_CARD (object);

	switch (property_id) {
	case PROP_CSS_PROVIDER:
		g_value_set_object (value, self->priv->css_provider);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_contact_card_constructed (GObject *object)
{
	EContactCard *self = E_CONTACT_CARD (object);
	GtkWidget *widget;
	GtkGrid *grid;
	GtkBox *vbox, *hbox;
	guint ii;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_contact_card_parent_class)->constructed (object);

	widget = GTK_WIDGET (self);

	gtk_widget_set_can_focus (widget, TRUE);

	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	g_object_set (widget,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		NULL);

	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "econtent");
	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_container_add (GTK_CONTAINER (self), widget);

	vbox = GTK_BOX (widget);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (widget,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		"visible", TRUE,
		NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "eheader");
	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);

	hbox = GTK_BOX (widget);

	widget = gtk_label_new ("");
	g_object_set (widget,
		"hexpand", FALSE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		"ellipsize", PANGO_ELLIPSIZE_END,
		"xalign", 0.0,
		"visible", TRUE,
		NULL);
	self->priv->header = GTK_LABEL (widget);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "eheaderlabel");
	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

	widget = gtk_image_new ();
	g_object_set (widget,
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", FALSE,
		"pixel-size", 24,
		"icon-name", "stock_contact-list",
		NULL);
	self->priv->image = GTK_IMAGE (widget);
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "eheaderimage");
	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_box_pack_end (hbox, widget, FALSE, FALSE, 0);

	widget = gtk_spinner_new ();
	g_object_set (widget,
		"hexpand", TRUE,
		"halign", GTK_ALIGN_CENTER,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"visible", FALSE,
		NULL);
	self->priv->spinner = widget;
	gtk_style_context_add_class (gtk_widget_get_style_context (widget), "eheaderspinner");
	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_box_pack_end (hbox, widget, FALSE, FALSE, 0);

	widget = gtk_grid_new ();
	g_object_set (widget,
		"halign", GTK_ALIGN_FILL,
		"valign", GTK_ALIGN_FILL,
		"column-homogeneous", FALSE,
		"column-spacing", 4,
		"row-homogeneous", FALSE,
		"row-spacing", 4,
		"visible", TRUE,
		NULL);

	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
		GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);

	self->priv->rows_grid = widget;

	grid = GTK_GRID (widget);

	for (ii = 0; ii < N_ROWS; ii++) {
		widget = gtk_label_new ("");
		g_object_set (widget,
			"hexpand", FALSE,
			"halign", GTK_ALIGN_START,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"visible", TRUE,
			"sensitive", FALSE,
			"ellipsize", PANGO_ELLIPSIZE_END,
			NULL);
		self->priv->rows[ii].label = GTK_LABEL (widget);
		gtk_style_context_add_class (gtk_widget_get_style_context (widget), "erowlabel");
		gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
			GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		gtk_grid_attach (grid, widget, 0, ii, 1, 1);

		widget = gtk_label_new ("");
		g_object_set (widget,
			"hexpand", FALSE,
			"halign", GTK_ALIGN_START,
			"vexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"visible", TRUE,
			"ellipsize", PANGO_ELLIPSIZE_END,
			NULL);
		self->priv->rows[ii].value = GTK_LABEL (widget);
		gtk_style_context_add_class (gtk_widget_get_style_context (widget), "erowvalue");
		gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
			GTK_STYLE_PROVIDER (self->priv->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		gtk_grid_attach (grid, widget, 1, ii, 1, 1);
	}
}

static void
e_contact_card_finalize (GObject *object)
{
	EContactCard *self = E_CONTACT_CARD (object);

	g_clear_object (&self->priv->css_provider);
	g_clear_object (&self->priv->contact);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_contact_card_parent_class)->finalize (object);
}

static void
e_contact_card_class_init (EContactCardClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->get_preferred_width = e_contact_card_get_preferred_width;
	widget_class->get_preferred_width_for_height = e_contact_card_get_preferred_width_for_height;
	widget_class->show_all = e_contact_card_show_all;

	gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_LIST_ITEM);
	gtk_widget_class_set_accessible_type (widget_class, E_TYPE_CONTACT_CARD_ACCESSIBLE);
	gtk_widget_class_set_css_name (widget_class, "EContactCard");

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = e_contact_card_set_property;
	object_class->get_property = e_contact_card_get_property;
	object_class->constructed = e_contact_card_constructed;
	object_class->finalize = e_contact_card_finalize;

	/**
	 * EContactCard:css-provider:
	 *
	 * A #GtkCssProvider to use for the contact card.
	 *
	 * Since: 3.50
	 **/
	obj_props[PROP_CSS_PROVIDER] =
		g_param_spec_object ("css-provider", NULL, NULL,
			GTK_TYPE_CSS_PROVIDER,
			G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

	g_object_class_install_properties (object_class, G_N_ELEMENTS (obj_props), obj_props);
}

static void
e_contact_card_init (EContactCard *self)
{
	self->priv = e_contact_card_get_instance_private (self);
}

/**
 * e_contact_card_new:
 * @css_provider: a #GtkCssProvider to use for internal widgets
 *
 * Creates a new #EContactCard
 *
 * Returns: (transfer full): a new #EContactCard
 *
 * Since: 3.50
 **/
GtkWidget *
e_contact_card_new (GtkCssProvider *css_provider)
{
	return g_object_new (E_TYPE_CONTACT_CARD,
		"css-provider", css_provider,
		NULL);
}

EContact *
e_contact_card_get_contact (EContactCard *self)
{
	g_return_val_if_fail (E_IS_CONTACT_CARD (self), 0);

	return self->priv->contact;
}

void
e_contact_card_set_contact (EContactCard *self,
			    EContact *contact)
{
	g_return_if_fail (E_IS_CONTACT_CARD (self));
	if (contact) {
		g_return_if_fail (E_IS_CONTACT (contact));
		g_object_ref (contact);
	}

	g_clear_object (&self->priv->contact);
	self->priv->contact = contact;

	e_contact_card_update (self);
}

static guint
e_contact_card_limit_lines_by_value (gchar *inout_text,
				     GtkLabel *label,
				     GtkLabel *value,
				     guint row)
{
	gchar *ptr;
	guint n_lines = 0;

	for (ptr = inout_text ? strchr (inout_text, '\n') : NULL; ptr && n_lines + row < N_ROWS; ptr = strchr (ptr + 1, '\n')) {
		n_lines++;
		if (n_lines + row >= N_ROWS) {
			*ptr = '\0';
			break;
		}
	}

	n_lines++;
	if (n_lines == 1)
		gtk_widget_set_valign (GTK_WIDGET (label), GTK_ALIGN_CENTER);
	else
		gtk_widget_set_valign (GTK_WIDGET (label), GTK_ALIGN_START);

	return n_lines;
}

void
e_contact_card_update (EContactCard *self)
{
	guint row;

	if (self->priv->contact) {
		EContactField field;
		gboolean has_email = FALSE, has_fax = FALSE, has_voice = FALSE;
		gchar *file_as;
		gboolean is_list;
		guint used_rows, grid_row_spacing;
		gint grid_allocated_height = 0, used_height = 0;

		grid_allocated_height = gtk_widget_get_allocated_height (self->priv->rows_grid);
		grid_row_spacing = gtk_grid_get_row_spacing (GTK_GRID (self->priv->rows_grid));

		if (gtk_widget_get_visible (self->priv->spinner)) {
			gtk_spinner_stop (GTK_SPINNER (self->priv->spinner));
			gtk_widget_set_visible (self->priv->spinner, FALSE);
			gtk_widget_set_visible (GTK_WIDGET (self->priv->header), TRUE);
		}

		file_as = e_contact_get (self->priv->contact, E_CONTACT_FILE_AS);
		gtk_label_set_label (self->priv->header, file_as ? file_as : "");

		is_list = e_contact_get (self->priv->contact, E_CONTACT_IS_LIST) != NULL;
		row = 0;
		used_rows = 0;

		for (field = E_CONTACT_FULL_NAME;
		     field != (E_CONTACT_LAST_SIMPLE_STRING -1) &&
		     row < N_ROWS && used_rows < N_ROWS &&
		     used_height <= grid_allocated_height;
		     field++) {
			gboolean is_email = FALSE;

			if (field == E_CONTACT_FAMILY_NAME || field == E_CONTACT_GIVEN_NAME ||
			    (has_voice && field == E_CONTACT_PHONE_OTHER) ||
			    (has_fax && field == E_CONTACT_PHONE_OTHER_FAX))
				continue;

			if (field == E_CONTACT_FULL_NAME && is_list)
				continue;

			if (field == E_CONTACT_NICKNAME && eab_fullname_matches_nickname (self->priv->contact))
				continue;

			if (field == E_CONTACT_EMAIL_1 || field == E_CONTACT_EMAIL_2 || field == E_CONTACT_EMAIL_3 || field == E_CONTACT_EMAIL_4) {
				if (has_email)
					continue;
				has_email = TRUE;
				is_email = TRUE;
			}

			if (is_email) {
				GList *emails, *link;

				emails = e_vcard_get_attributes_by_name (E_VCARD (self->priv->contact), EVC_EMAIL);

				for (link = emails; link && row < N_ROWS && used_rows < N_ROWS; link = g_list_next (link)) {
					EVCardAttribute *attr = link->data;
					GList *values;
					gchar *value_tmp = NULL, *value = NULL;
					const gchar *label = NULL;
					gchar *parsed_name = NULL, *parsed_email = NULL, *attr_value;

					/* do not use name for fields in the contact list */
					if (is_list) {
						label = "";
					} else {
						label = eab_get_email_label_text (attr);;
					}

					values = e_vcard_attribute_get_values (attr);
					attr_value = (values && values->data) ? g_strstrip (g_strdup (values->data)) : NULL;

					if (eab_parse_qp_email (attr_value, &parsed_name, &parsed_email)) {
						value_tmp = g_strdup_printf ("%s <%s>", parsed_name, parsed_email);
						value = value_tmp;
					} else {
						value = attr_value;
					}

					if (value && *value) {
						gint min_height = 0;

						used_rows += e_contact_card_limit_lines_by_value (value, self->priv->rows[row].label, self->priv->rows[row].value, used_rows);
						gtk_label_set_label (self->priv->rows[row].label, label);
						gtk_label_set_label (self->priv->rows[row].value, value);

						gtk_widget_get_preferred_height (GTK_WIDGET (self->priv->rows[row].value), &min_height, NULL);

						/* this can happen with odd text encoding saved in the contact */
						if (!min_height) {
							PangoLayout *layout = gtk_label_get_layout (self->priv->rows[row].value);
							gint szy;

							pango_layout_get_pixel_size (layout, NULL, &szy);

							min_height = szy;
						}

						used_height += min_height;
						if (row > 0)
							used_height += grid_row_spacing;

						gtk_widget_set_visible (GTK_WIDGET (self->priv->rows[row].label), used_height <= grid_allocated_height);
						gtk_widget_set_visible (GTK_WIDGET (self->priv->rows[row].value), used_height <= grid_allocated_height);
						row++;
					}

					g_free (parsed_name);
					g_free (parsed_email);
					g_free (attr_value);
					g_free (value_tmp);
				}

				g_list_free (emails);
			} else {
				gchar *value;

				value = e_contact_get (self->priv->contact, field);

				if (value && *value && e_util_strcmp0 (value, file_as) != 0) {
					EContactField label_field = field;
					gint min_height = 0;

					/* No need for the "Label" word in the row header */
					if (label_field == E_CONTACT_ADDRESS_LABEL_HOME)
						label_field = E_CONTACT_ADDRESS_HOME;
					else if (label_field == E_CONTACT_ADDRESS_LABEL_WORK)
						label_field = E_CONTACT_ADDRESS_WORK;
					else if (label_field == E_CONTACT_ADDRESS_LABEL_OTHER)
						label_field = E_CONTACT_ADDRESS_OTHER;

					used_rows += e_contact_card_limit_lines_by_value (value, self->priv->rows[row].label, self->priv->rows[row].value, used_rows);
					gtk_label_set_label (self->priv->rows[row].label, is_list ? "" : e_contact_pretty_name (label_field));
					gtk_label_set_label (self->priv->rows[row].value, value);

					gtk_widget_get_preferred_height (GTK_WIDGET (self->priv->rows[row].value), &min_height, NULL);

					/* this can happen with odd text encoding saved in the contact */
					if (!min_height) {
						PangoLayout *layout = gtk_label_get_layout (self->priv->rows[row].value);
						gint szy;

						pango_layout_get_pixel_size (layout, NULL, &szy);

						min_height = szy;
					}

					used_height += min_height;
					if (row > 0)
						used_height += grid_row_spacing;

					gtk_widget_set_visible (GTK_WIDGET (self->priv->rows[row].label), used_height <= grid_allocated_height);
					gtk_widget_set_visible (GTK_WIDGET (self->priv->rows[row].value), used_height <= grid_allocated_height);
					row++;

					has_voice = has_voice ||
						field == E_CONTACT_PHONE_BUSINESS ||
						field == E_CONTACT_PHONE_BUSINESS_2 ||
						field == E_CONTACT_PHONE_HOME ||
						field == E_CONTACT_PHONE_HOME_2;
					has_fax = has_fax ||
						field == E_CONTACT_PHONE_BUSINESS_FAX ||
						field == E_CONTACT_PHONE_HOME_FAX;
				}

				g_free (value);
			}
		}

		g_free (file_as);

		while (row < N_ROWS) {
			gtk_label_set_label (self->priv->rows[row].label, "");
			gtk_label_set_label (self->priv->rows[row].value, "");
			gtk_widget_set_visible (GTK_WIDGET (self->priv->rows[row].label), used_rows < N_ROWS);
			gtk_widget_set_visible (GTK_WIDGET (self->priv->rows[row].value), used_rows < N_ROWS);
			row++;
			used_rows++;
		}

		self->priv->have_image = is_list;
		gtk_widget_set_visible (GTK_WIDGET (self->priv->image), self->priv->have_image);
	} else {
		self->priv->have_image = FALSE;
		gtk_widget_set_visible (GTK_WIDGET (self->priv->header), FALSE);
		gtk_widget_set_visible (GTK_WIDGET (self->priv->image), FALSE);
		gtk_widget_set_visible (self->priv->spinner, TRUE);
		gtk_spinner_start (GTK_SPINNER (self->priv->spinner));

		for (row = 0; row < N_ROWS; row++) {
			gtk_label_set_label (self->priv->rows[row].label, "");
			gtk_label_set_label (self->priv->rows[row].value, "");
		}
	}
}
