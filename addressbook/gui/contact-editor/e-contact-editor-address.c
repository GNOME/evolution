/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-contact-editor-address.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <e-contact-editor-address.h>
#include <e-util/e-icon-factory.h>

#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkstock.h>
#include <gtk/gtklabel.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>

static void e_contact_editor_address_init		(EContactEditorAddress		 *card);
static void e_contact_editor_address_class_init	(EContactEditorAddressClass	 *klass);
static void e_contact_editor_address_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_contact_editor_address_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_contact_editor_address_dispose (GObject *object);

static void fill_in_info(EContactEditorAddress *editor);
static void extract_info(EContactEditorAddress *editor);

static GtkDialogClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_ADDRESS,
	PROP_EDITABLE
};

GType
e_contact_editor_address_get_type (void)
{
	static GType contact_editor_address_type = 0;

	if (!contact_editor_address_type) {
		static const GTypeInfo contact_editor_address_info =  {
			sizeof (EContactEditorAddressClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_contact_editor_address_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EContactEditorAddress),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_contact_editor_address_init,
		};

		contact_editor_address_type = g_type_register_static (GTK_TYPE_DIALOG, "EContactEditorAddress", &contact_editor_address_info, 0);
	}

	return contact_editor_address_type;
}

static void
e_contact_editor_address_class_init (EContactEditorAddressClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (GTK_TYPE_DIALOG);

	object_class->set_property = e_contact_editor_address_set_property;
	object_class->get_property = e_contact_editor_address_get_property;
	object_class->dispose = e_contact_editor_address_dispose;

	g_object_class_install_property (object_class, PROP_ADDRESS, 
					 g_param_spec_boxed ("address",
							     _("Address"),
							     /*_( */"XXX blurb" /*)*/,
							     e_contact_address_get_type (),
							     G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE, 
					 g_param_spec_boolean ("editable",
							       _("Editable"),
							       /*_( */"XXX blurb" /*)*/,
							       FALSE,
							       G_PARAM_READWRITE));
}

static GList *
add_to_tab_order(GList *list, GladeXML *gui, char *name)
{
	GtkWidget *widget = glade_xml_get_widget(gui, name);
	return g_list_prepend(list, widget);
}

static void
setup_tab_order(GladeXML *gui)
{
	GtkWidget *container;
	GList *list = NULL;

	container = glade_xml_get_widget(gui, "table-checkaddress");

	if (container) {
		list = add_to_tab_order(list, gui, "entry-city");
		list = add_to_tab_order(list, gui, "entry-region");
		list = add_to_tab_order(list, gui, "entry-code");
		list = add_to_tab_order(list, gui, "combo-country");
		list = g_list_reverse(list);
		e_container_change_tab_order(GTK_CONTAINER(container), list);
		g_list_free(list);
	}
}

static char * countries [] = {
	N_("United States"),
	N_("Afghanistan"),
	N_("Albania"),
	N_("Algeria"),
	N_("American Samoa"),
	N_("Andorra"),
	N_("Angola"),
	N_("Anguilla"),
	N_("Antarctica"),
	N_("Antigua And Barbuda"),
	N_("Argentina"),
	N_("Armenia"),
	N_("Aruba"),
	N_("Australia"),
	N_("Austria"),
	N_("Azerbaijan"),
	N_("Bahamas"),
	N_("Bahrain"),
	N_("Bangladesh"),
	N_("Barbados"),
	N_("Belarus"),
	N_("Belgium"),
	N_("Belize"),
	N_("Benin"),
	N_("Bermuda"),
	N_("Bhutan"),
	N_("Bolivia"),
	N_("Bosnia And Herzegowina"),
	N_("Botswana"),
	N_("Bouvet Island"),
	N_("Brazil"),
	N_("British Indian Ocean Territory"),
	N_("Brunei Darussalam"),
	N_("Bulgaria"),
	N_("Burkina Faso"),
	N_("Burundi"),
	N_("Cambodia"),
	N_("Cameroon"),
	N_("Canada"),
	N_("Cape Verde"),
	N_("Cayman Islands"),
	N_("Central African Republic"),
	N_("Chad"),
	N_("Chile"),
	N_("China"),
	N_("Christmas Island"),
	N_("Cocos (Keeling) Islands"),
	N_("Colombia"),
	N_("Comoros"),
	N_("Congo"),
	N_("Congo, The Democratic Republic Of The"),
	N_("Cook Islands"),
	N_("Costa Rica"),
	N_("Cote d'Ivoire"),
	N_("Croatia"),
	N_("Cuba"),
	N_("Cyprus"),
	N_("Czech Republic"),
	N_("Denmark"),
	N_("Djibouti"),
	N_("Dominica"),
	N_("Dominican Republic"),
	N_("Ecuador"),
	N_("Egypt"),
	N_("El Salvador"),
	N_("Equatorial Guinea"),
	N_("Eritrea"),
	N_("Estonia"),
	N_("Ethiopia"),
	N_("Falkland Islands"),
	N_("Faroe Islands"),
	N_("Fiji"),
	N_("Finland"),
	N_("France"),
	N_("French Guiana"),
	N_("French Polynesia"),
	N_("French Southern Territories"),
	N_("Gabon"),
	N_("Gambia"),
	N_("Georgia"),
	N_("Germany"),
	N_("Ghana"),
	N_("Gibraltar"),
	N_("Greece"),
	N_("Greenland"),
	N_("Grenada"),
	N_("Guadeloupe"),
	N_("Guam"),
	N_("Guatemala"),
	N_("Guernsey"),
	N_("Guinea"),
	N_("Guinea-bissau"),
	N_("Guyana"),
	N_("Haiti"),
	N_("Heard And McDonald Islands"),
	N_("Holy See"),
	N_("Honduras"),
	N_("Hong Kong"),
	N_("Hungary"),
	N_("Iceland"),
	N_("India"),
	N_("Indonesia"),
	N_("Iran"),
	N_("Iraq"),
	N_("Ireland"),
	N_("Isle of Man"),
	N_("Israel"),
	N_("Italy"),
	N_("Jamaica"),
	N_("Japan"),
	N_("Jersey"),
	N_("Jordan"),
	N_("Kazakhstan"),
	N_("Kenya"),
	N_("Kiribati"),
	N_("Korea, Democratic People's Republic Of"),
	N_("Korea, Republic Of"),
	N_("Kuwait"),
	N_("Kyrgyzstan"),
	N_("Laos"),
	N_("Latvia"),
	N_("Lebanon"),
	N_("Lesotho"),
	N_("Liberia"),
	N_("Libya"),
	N_("Liechtenstein"),
	N_("Lithuania"),
	N_("Luxembourg"),
	N_("Macao"),
	N_("Macedonia"),
	N_("Madagascar"),
	N_("Malawi"),
	N_("Malaysia"),
	N_("Maldives"),
	N_("Mali"),
	N_("Malta"),
	N_("Marshall Islands"),
	N_("Martinique"),
	N_("Mauritania"),
	N_("Mauritius"),
	N_("Mayotte"),
	N_("Mexico"),
	N_("Micronesia"),
	N_("Moldova, Republic Of"),
	N_("Monaco"),
	N_("Mongolia"),
	N_("Montserrat"),
	N_("Morocco"),
	N_("Mozambique"),
	N_("Myanmar"),
	N_("Namibia"),
	N_("Nauru"),
	N_("Nepal"),
	N_("Netherlands"),
	N_("Netherlands Antilles"),
	N_("New Caledonia"),
	N_("New Zealand"),
	N_("Nicaragua"),
	N_("Niger"),
	N_("Nigeria"),
	N_("Niue"),
	N_("Norfolk Island"),
	N_("Northern Mariana Islands"),
	N_("Norway"),
	N_("Oman"),
	N_("Pakistan"),
	N_("Palau"),
	N_("Palestinian Territory"),
	N_("Panama"),
	N_("Papua New Guinea"),
	N_("Paraguay"),
	N_("Peru"),
	N_("Philippines"),
	N_("Pitcairn"),
	N_("Poland"),
	N_("Portugal"),
	N_("Puerto Rico"),
	N_("Qatar"),
	N_("Reunion"),
	N_("Romania"),
	N_("Russian Federation"),
	N_("Rwanda"),
	N_("Saint Kitts And Nevis"),
	N_("Saint Lucia"),
	N_("Saint Vincent And The Grena-dines"),
	N_("Samoa"),
	N_("San Marino"),
	N_("Sao Tome And Principe"),
	N_("Saudi Arabia"),
	N_("Senegal"),
	N_("Serbia And Montenegro"),
	N_("Seychelles"),
	N_("Sierra Leone"),
	N_("Singapore"),
	N_("Slovakia"),
	N_("Slovenia"),
	N_("Solomon Islands"),
	N_("Somalia"),
	N_("South Africa"),
	N_("South Georgia And The South Sandwich Islands"),
	N_("Spain"),
	N_("Sri Lanka"),
	N_("St. Helena"),
	N_("St. Pierre And Miquelon"),
	N_("Sudan"),
	N_("Suriname"),
	N_("Svalbard And Jan Mayen Islands"),
	N_("Swaziland"),
	N_("Sweden"),
	N_("Switzerland"),
	N_("Syria"),
	N_("Taiwan"),
	N_("Tajikistan"),
	N_("Tanzania, United Republic Of"),
	N_("Thailand"),
	N_("Timor-Leste"),
	N_("Togo"),
	N_("Tokelau"),
	N_("Tonga"),
	N_("Trinidad And Tobago"),
	N_("Tunisia"),
	N_("Turkey"),
	N_("Turkmenistan"),
	N_("Turks And Caicos Islands"),
	N_("Tuvalu"),
	N_("Uganda"),
	N_("Ukraine"),
	N_("United Arab Emirates"),
	N_("United Kingdom"),
	N_("United States Minor Outlying Islands"),
	N_("Uruguay"),
	N_("Uzbekistan"),
	N_("Vanuatu"),
	N_("Venezuela"),
	N_("Viet Nam"),
	N_("Virgin Islands, British"),
	N_("Virgin Islands, U.S."),
	N_("Wallis And Futuna Islands"),
	N_("Western Sahara"),
	N_("Yemen"),
	N_("Zambia"),
	N_("Zimbabwe"),
	NULL
};

static int
compare_func (const void *voida, const void *voidb)
{
	char * const *stringa = voida, * const *stringb = voidb;

	return strcoll (*stringa, *stringb);
}

static void
fill_in_countries (GladeXML *gui)
{
	GtkCombo *combo;
	combo = (GtkCombo *) glade_xml_get_widget(gui, "combo-country");
	if (combo && GTK_IS_COMBO (combo)) {
		static gboolean sorted = FALSE;
		static GList *country_list;
		if (!sorted) {
			int i;
			char *locale;

			for (i = 0; countries[i]; i++) {
				countries[i] = _(countries[i]);
			}

			locale = setlocale (LC_COLLATE, NULL);
			qsort (countries + 1, i - 1, sizeof (countries[0]), compare_func);
			country_list = NULL;
			for (i = 0; countries[i]; i++) {
				country_list = g_list_prepend (country_list, countries[i]);
			}
			country_list = g_list_reverse (country_list);
			sorted = TRUE;
		}
		gtk_combo_set_popdown_strings (combo, country_list);
	}
}

static void
e_contact_editor_address_init (EContactEditorAddress *e_contact_editor_address)
{
	GladeXML *gui;
	GtkWidget *widget;
	GList *icon_list;

	gtk_dialog_add_buttons (GTK_DIALOG (e_contact_editor_address),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_resizable(GTK_WINDOW(e_contact_editor_address), TRUE);

	e_contact_editor_address->address = NULL;

	gui = glade_xml_new (EVOLUTION_GLADEDIR "/fulladdr.glade", NULL, NULL);
	e_contact_editor_address->gui = gui;

	setup_tab_order (gui);
	fill_in_countries (gui);

	widget = glade_xml_get_widget(gui, "dialog-checkaddress");
	gtk_window_set_title (GTK_WINDOW (e_contact_editor_address),
			      GTK_WINDOW (widget)->title);

	widget = glade_xml_get_widget(gui, "table-checkaddress");
	g_object_ref(widget);
	gtk_container_remove(GTK_CONTAINER(widget->parent), widget);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (e_contact_editor_address)->vbox), widget, TRUE, TRUE, 0);
	g_object_unref(widget);

	icon_list = e_icon_factory_get_icon_list ("stock_contact");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (e_contact_editor_address), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}
}

void
e_contact_editor_address_dispose (GObject *object)
{
	EContactEditorAddress *e_contact_editor_address = E_CONTACT_EDITOR_ADDRESS(object);

	if (e_contact_editor_address->gui) {
		g_object_unref(e_contact_editor_address->gui);
		e_contact_editor_address->gui = NULL;
	}

	if (e_contact_editor_address->address) {
		e_contact_address_free (e_contact_editor_address->address);
		e_contact_editor_address->address = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

GtkWidget*
e_contact_editor_address_new (const EContactAddress *address)
{
	GtkWidget *widget = g_object_new (E_TYPE_CONTACT_EDITOR_ADDRESS, NULL);

	g_object_set (widget,
		      "address", address,
		      NULL);

	return widget;
}

static void
e_contact_editor_address_set_property (GObject *object, guint prop_id,
				       const GValue *value, GParamSpec *pspec)
{
	EContactEditorAddress *e_contact_editor_address;

	e_contact_editor_address = E_CONTACT_EDITOR_ADDRESS (object);
	
	switch (prop_id){
	case PROP_ADDRESS:
		if (e_contact_editor_address->address)
			g_boxed_free (e_contact_address_get_type (), e_contact_editor_address->address);

		e_contact_editor_address->address = g_value_dup_boxed (value);
		fill_in_info (e_contact_editor_address);
		break;
	case PROP_EDITABLE: {
		int i;
		char *widget_names[] = {
			"entry-street",
			"entry-city",
			"entry-ext",
			"entry-po",
			"entry-region",
			"combo-country",
			"entry-code",
			"label-street",
			"label-city",
			"label-ext",
			"label-po",
			"label-region",
			"label-country",
			"label-code",
			NULL
		};
		e_contact_editor_address->editable = g_value_get_boolean (value) ? TRUE : FALSE;
		for (i = 0; widget_names[i] != NULL; i ++) {
			GtkWidget *w = glade_xml_get_widget(e_contact_editor_address->gui, widget_names[i]);
			if (GTK_IS_ENTRY (w)) {
				gtk_editable_set_editable (GTK_EDITABLE (w),
							   e_contact_editor_address->editable);
			}
			else if (GTK_IS_COMBO (w)) {
				gtk_editable_set_editable (GTK_EDITABLE (GTK_COMBO (w)->entry),
							   e_contact_editor_address->editable);
				gtk_widget_set_sensitive (GTK_COMBO (w)->button, e_contact_editor_address->editable);
			}
			else if (GTK_IS_LABEL (w)) {
				gtk_widget_set_sensitive (w, e_contact_editor_address->editable);
			}
		}
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_contact_editor_address_get_property (GObject *object, guint prop_id,
				       GValue *value, GParamSpec *pspec)
{
	EContactEditorAddress *e_contact_editor_address;

	e_contact_editor_address = E_CONTACT_EDITOR_ADDRESS (object);

	switch (prop_id) {
	case PROP_ADDRESS:
		extract_info (e_contact_editor_address);
		g_value_set_static_boxed (value, e_contact_editor_address->address);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean (value, e_contact_editor_address->editable ? TRUE : FALSE);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
fill_in_field(EContactEditorAddress *editor, char *field, char *string)
{
	GtkEntry *entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, field));
	if (entry) {
		if (string)
			gtk_entry_set_text(entry, string);
		else
			gtk_entry_set_text(entry, "");
	}
}

static void
fill_in_info(EContactEditorAddress *editor)
{
	EContactAddress *address = editor->address;

	if (address) {
		fill_in_field (editor, "entry-street" , address->street  );
		fill_in_field (editor, "entry-po"     , address->po      );
		fill_in_field (editor, "entry-ext"    , address->ext     );
		fill_in_field (editor, "entry-city"   , address->locality);
		fill_in_field (editor, "entry-region" , address->region  );
		fill_in_field (editor, "entry-code"   , address->code    );
		fill_in_field (editor, "entry-country", address->country );
	}
}

static char *
extract_field(EContactEditorAddress *editor, char *field)
{
	GtkEntry *entry = GTK_ENTRY(glade_xml_get_widget(editor->gui, field));
	if (entry)
		return g_strdup (gtk_entry_get_text(entry));
	else
		return NULL;
}

static void
extract_info(EContactEditorAddress *editor)
{
	EContactAddress *address = editor->address;

	if (address) {
		g_boxed_free (e_contact_address_get_type (), address);
	}

	address = g_new0 (EContactAddress, 1);
	editor->address = address;

	address->street   = extract_field(editor, "entry-street" );
	address->po       = extract_field(editor, "entry-po"     );
	address->ext      = extract_field(editor, "entry-ext"    );
	address->locality = extract_field(editor, "entry-city"   );
	address->region   = extract_field(editor, "entry-region" );
	address->code     = extract_field(editor, "entry-code"   );
	address->country  = extract_field(editor, "entry-country");
}
