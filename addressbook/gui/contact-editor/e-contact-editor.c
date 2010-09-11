/*
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
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include "eab-editor.h"
#include "e-contact-editor.h"

#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libebook/e-address-western.h>
#include <libedataserverui/e-book-auth-util.h>
#include <libedataserverui/e-categories-dialog.h>
#include <libedataserverui/e-category-completion.h>
#include <libedataserverui/e-source-combo-box.h>

#include "addressbook/printing/e-contact-print.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "e-util/e-util.h"
#include "e-util/e-alert-dialog.h"
#include "misc/e-dateedit.h"
#include "misc/e-image-chooser.h"
#include "misc/e-url-entry.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-util-private.h"
#include "shell/e-shell.h"

#include "eab-contact-merging.h"

#include "e-contact-editor-fullname.h"

#define EMAIL_SLOTS   4
#define PHONE_SLOTS   8
#define IM_SLOTS      4
#define ADDRESS_SLOTS 3

#define EVOLUTION_UI_SLOT_PARAM "X-EVOLUTION-UI-SLOT"

/* IM columns */
enum {
	COLUMN_IM_ICON,
	COLUMN_IM_SERVICE,
	COLUMN_IM_SCREENNAME,
	COLUMN_IM_LOCATION,
	COLUMN_IM_LOCATION_TYPE,
	COLUMN_IM_SERVICE_FIELD,
	NUM_IM_COLUMNS
};

static void e_contact_editor_init		(EContactEditor		 *editor);
static void e_contact_editor_class_init	(EContactEditorClass	 *klass);
static void e_contact_editor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void e_contact_editor_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void e_contact_editor_dispose (GObject *object);

static void e_contact_editor_raise	    (EABEditor *editor);
static void e_contact_editor_show	    (EABEditor *editor);
static void e_contact_editor_save_contact   (EABEditor *editor, gboolean should_close);
static void e_contact_editor_close	    (EABEditor *editor);
static gboolean e_contact_editor_is_valid   (EABEditor *editor);
static gboolean e_contact_editor_is_changed (EABEditor *editor);
static GtkWindow* e_contact_editor_get_window (EABEditor *editor);

static void save_contact (EContactEditor *ce, gboolean should_close);
static void entry_activated (EContactEditor *editor);

static void set_entry_text(EContactEditor *editor, GtkEntry *entry, const gchar *string);
static void sensitize_ok (EContactEditor *ce);

static EABEditorClass *parent_class = NULL;

/* The arguments we take */
enum {
	PROP_0,
	PROP_SOURCE_BOOK,
	PROP_TARGET_BOOK,
	PROP_CONTACT,
	PROP_IS_NEW_CONTACT,
	PROP_EDITABLE,
	PROP_CHANGED,
	PROP_WRITABLE_FIELDS,
	PROP_REQUIRED_FIELDS
};

enum {
	DYNAMIC_LIST_EMAIL,
	DYNAMIC_LIST_PHONE,
	DYNAMIC_LIST_ADDRESS
};

static struct {
	EContactField field_id;
	const gchar *type_1;
	const gchar *type_2;
}
phones[] = {
	{ E_CONTACT_PHONE_ASSISTANT,    EVC_X_ASSISTANT,       NULL    },
	{ E_CONTACT_PHONE_BUSINESS,     "WORK",                "VOICE" },
	{ E_CONTACT_PHONE_BUSINESS_FAX, "WORK",                "FAX"   },
	{ E_CONTACT_PHONE_CALLBACK,     EVC_X_CALLBACK,        NULL    },
	{ E_CONTACT_PHONE_CAR,          "CAR",                 NULL    },
	{ E_CONTACT_PHONE_COMPANY,      "X-EVOLUTION-COMPANY", NULL    },
	{ E_CONTACT_PHONE_HOME,         "HOME",                "VOICE" },
	{ E_CONTACT_PHONE_HOME_FAX,     "HOME",                "FAX"   },
	{ E_CONTACT_PHONE_ISDN,         "ISDN",                NULL    },
	{ E_CONTACT_PHONE_MOBILE,       "CELL",                NULL    },
	{ E_CONTACT_PHONE_OTHER,        "VOICE",               NULL    },
	{ E_CONTACT_PHONE_OTHER_FAX,    "FAX",                 NULL    },
	{ E_CONTACT_PHONE_PAGER,        "PAGER",               NULL    },
	{ E_CONTACT_PHONE_PRIMARY,      "PREF",                NULL    },
	{ E_CONTACT_PHONE_RADIO,        EVC_X_RADIO,           NULL    },
	{ E_CONTACT_PHONE_TELEX,        EVC_X_TELEX,           NULL    },
	{ E_CONTACT_PHONE_TTYTDD,       EVC_X_TTYTDD,          NULL    }
};

/* Defaults from the table above */
static const gint phones_default[] = { 1, 6, 9, 2, 7, 12, 10, 10 };

static EContactField addresses[] = {
	E_CONTACT_ADDRESS_WORK,
	E_CONTACT_ADDRESS_HOME,
	E_CONTACT_ADDRESS_OTHER
};

static EContactField address_labels[] = {
	E_CONTACT_ADDRESS_LABEL_WORK,
	E_CONTACT_ADDRESS_LABEL_HOME,
	E_CONTACT_ADDRESS_LABEL_OTHER
};

static const gchar *address_name[] = {
	"work",
	"home",
	"other"
};

static struct {
	EContactField field;
	const gchar *pretty_name;
}
im_service[] =
{
	{ E_CONTACT_IM_AIM,       N_ ("AIM")       },
	{ E_CONTACT_IM_JABBER,    N_ ("Jabber")    },
	{ E_CONTACT_IM_YAHOO,     N_ ("Yahoo")     },
	{ E_CONTACT_IM_GADUGADU,  N_ ("Gadu-Gadu") },
	{ E_CONTACT_IM_MSN,       N_ ("MSN")       },
	{ E_CONTACT_IM_ICQ,       N_ ("ICQ")       },
	{ E_CONTACT_IM_GROUPWISE, N_ ("GroupWise") },
	{ E_CONTACT_IM_SKYPE,     N_ ("Skype") }
};

/* Defaults from the table above */
static const gint im_service_default[] = { 0, 2, 4, 5 };

static struct {
	const gchar *name;
	const gchar *pretty_name;
}
common_location[] =
{
	{ "WORK",  N_ ("Work")  },
	{ "HOME",  N_ ("Home")  },
	{ "OTHER", N_ ("Other") }
};

/* Default from the table above */
static const gint email_default[] = { 0, 1, 2, 2 };

#define STRING_IS_EMPTY(x)      (!(x) || !(*(x)))
#define STRING_MAKE_NON_NULL(x) ((x) ? (x) : "")

static void
e_contact_editor_contact_added (EABEditor *editor,
                                const GError *error,
                                EContact *contact)
{
	if (!error)
		return;

	if (g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED))
		return;

	eab_error_dialog (_("Error adding contact"), error);
}

static void
e_contact_editor_contact_modified (EABEditor *editor,
                                   const GError *error,
                                   EContact *contact)
{
	if (!error)
		return;

	if (g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED))
		return;

	eab_error_dialog (_("Error modifying contact"), error);
}

static void
e_contact_editor_contact_deleted (EABEditor *editor,
                                  const GError *error,
                                  EContact *contact)
{
	if (!error)
		return;

	if (g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED))
		return;

	eab_error_dialog (_("Error removing contact"), error);
}

static void
e_contact_editor_closed (EABEditor *editor)
{
	g_object_unref (editor);
}

GType
e_contact_editor_get_type (void)
{
	static GType contact_editor_type = 0;

	if (!contact_editor_type) {
		static const GTypeInfo contact_editor_info =  {
			sizeof (EContactEditorClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_contact_editor_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EContactEditor),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_contact_editor_init,
		};

		contact_editor_type = g_type_register_static (EAB_TYPE_EDITOR, "EContactEditor", &contact_editor_info, 0);
	}

	return contact_editor_type;
}

static void
e_contact_editor_class_init (EContactEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	EABEditorClass *editor_class = EAB_EDITOR_CLASS (klass);

	parent_class = g_type_class_ref (EAB_TYPE_EDITOR);

	object_class->set_property = e_contact_editor_set_property;
	object_class->get_property = e_contact_editor_get_property;
	object_class->dispose = e_contact_editor_dispose;

	editor_class->raise = e_contact_editor_raise;
	editor_class->show = e_contact_editor_show;
	editor_class->close = e_contact_editor_close;
	editor_class->is_valid = e_contact_editor_is_valid;
	editor_class->save_contact = e_contact_editor_save_contact;
	editor_class->is_changed = e_contact_editor_is_changed;
	editor_class->get_window = e_contact_editor_get_window;
	editor_class->contact_added = e_contact_editor_contact_added;
	editor_class->contact_modified = e_contact_editor_contact_modified;
	editor_class->contact_deleted = e_contact_editor_contact_deleted;
	editor_class->editor_closed = e_contact_editor_closed;

	g_object_class_install_property (object_class, PROP_SOURCE_BOOK,
					 g_param_spec_object ("source_book",
							      "Source Book",
							      NULL,
							      E_TYPE_BOOK,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_TARGET_BOOK,
					 g_param_spec_object ("target_book",
							      "Target Book",
							      NULL,
							      E_TYPE_BOOK,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CONTACT,
					 g_param_spec_object ("contact",
							      "Contact",
							      NULL,
							      E_TYPE_CONTACT,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_IS_NEW_CONTACT,
					 g_param_spec_boolean ("is_new_contact",
							       "Is New Contact",
							       NULL,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_WRITABLE_FIELDS,
					 g_param_spec_object ("writable_fields",
							      "Writable Fields",
							      NULL,
							      E_TYPE_LIST,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_REQUIRED_FIELDS,
					 g_param_spec_object ("required_fields",
							      "Required Fields",
							      NULL,
							      E_TYPE_LIST,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       "Editable",
							       NULL,
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_CHANGED,
					 g_param_spec_boolean ("changed",
							       "Changed",
							       NULL,
							       FALSE,
							       G_PARAM_READWRITE));
}

static void
entry_activated (EContactEditor *editor)
{
	save_contact (editor, TRUE);
}

/* FIXME: Linear time... */
static gboolean
is_field_supported (EContactEditor *editor, EContactField field_id)
{
	EList       *fields;
	const gchar *field;
	EIterator   *iter;

	fields = editor->writable_fields;
	if (!fields)
		return FALSE;

	field = e_contact_field_name (field_id);
	if (!field)
		return FALSE;

	for (iter = e_list_get_iterator (fields);
	     e_iterator_is_valid (iter);
	     e_iterator_next (iter)) {
		const gchar *this_field = e_iterator_get (iter);

		if (!this_field)
			continue;

		if (!strcmp (field, this_field))
			return TRUE;
	}

	return FALSE;
}

/* This function tells you whether name_to_style will make sense.  */
static gboolean
style_makes_sense (const EContactName *name, const gchar *company, gint style)
{
	switch (style) {
	case 0: /* Fall Through */
	case 1:
		return TRUE;
        case 2:
                if (name) {
                        if (name->additional && *name->additional)
                                return TRUE;
                        else
                                return FALSE;
                }
	case 3:
		if (company && *company)
			return TRUE;
		else
			return FALSE;
	case 4: /* Fall Through */
	case 5:
		if (company && *company && name && ((name->given && *name->given) || (name->family && *name->family)))
			return TRUE;
		else
			return FALSE;
	default:
		return FALSE;
	}
}

static gchar *
name_to_style (const EContactName *name, const gchar *company, gint style)
{
	gchar *string;
	gchar *strings[4], **stringptr;
        gchar *midstring[4], **midstrptr;
	gchar *substring;
	switch (style) {
	case 0:
		stringptr = strings;
		if (name) {
			if (name->family && *name->family)
				*(stringptr++) = name->family;
			if (name->given && *name->given)
				*(stringptr++) = name->given;
		}
		*stringptr = NULL;
		string = g_strjoinv(", ", strings);
		break;
	case 1:
		stringptr = strings;
		if (name) {
			if (name->given && *name->given)
				*(stringptr++) = name->given;
			if (name->family && *name->family)
				*(stringptr++) = name->family;
		}
		*stringptr = NULL;
		string = g_strjoinv(" ", strings);
		break;
        case 2:
                midstrptr=midstring;
                if (name) {
                        if (name->family && *name->family)
				*(midstrptr++) = name->family;
                       if (name->given && *name->given)
				*(midstrptr++) = name->given;
                 }
                *midstrptr = NULL;
                stringptr = strings;
                *(stringptr++) = g_strjoinv(", ", midstring);
		if (name) {
			if (name->additional && *name->additional)
				*(stringptr++) = name->additional;
		}
		*stringptr = NULL;
		string = g_strjoinv(" ", strings);
		break;
	case 3:
		string = g_strdup(company);
		break;
	case 4: /* Fall Through */
	case 5:
		stringptr = strings;
		if (name) {
			if (name->family && *name->family)
				*(stringptr++) = name->family;
			if (name->given && *name->given)
				*(stringptr++) = name->given;
		}
		*stringptr = NULL;
		substring = g_strjoinv(", ", strings);
		if (!(company && *company))
			company = "";
		if (style == 4)
			string = g_strdup_printf("%s (%s)", substring, company);
		else
			string = g_strdup_printf("%s (%s)", company, substring);
		g_free(substring);
		break;
	default:
		string = g_strdup("");
	}
	return string;
}

static gint
file_as_get_style (EContactEditor *editor)
{
	GtkEntry *file_as = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (e_builder_get_widget(editor->builder, "combo-file-as"))));
	GtkEntry *company_w = GTK_ENTRY (e_builder_get_widget (editor->builder, "entry-company"));
	gchar *filestring;
	gchar *trystring;
	EContactName *name = editor->name;
	const gchar *company;
	gint i;

	if (!(file_as && GTK_IS_ENTRY(file_as)))
		return -1;

	company = gtk_entry_get_text (GTK_ENTRY (company_w));
	filestring = g_strdup (gtk_entry_get_text (file_as));

	for (i = 0; i < 6; i++) {
		trystring = name_to_style (name, company, i);
		if (!strcmp(trystring, filestring)) {
			g_free(trystring);
			g_free(filestring);
			return i;
		}
		g_free(trystring);
	}
	g_free (filestring);
	return -1;
}

static void
file_as_set_style (EContactEditor *editor, gint style)
{
	gchar *string;
	gint i;
	GList *strings = NULL;
	GtkComboBox *combo_file_as = GTK_COMBO_BOX (e_builder_get_widget (editor->builder, "combo-file-as"));
	GtkEntry *company_w = GTK_ENTRY (e_builder_get_widget (editor->builder, "entry-company"));
	const gchar *company;

	if (!(combo_file_as && GTK_IS_COMBO_BOX_ENTRY (combo_file_as)))
		return;

	company = gtk_entry_get_text (GTK_ENTRY (company_w));

	if (style == -1) {
		string = gtk_combo_box_get_active_text (combo_file_as);
		strings = g_list_append (strings, string);
	}

	for (i = 0; i < 6; i++) {
		if (style_makes_sense (editor->name, company, i)) {
			gchar *u;
			u = name_to_style (editor->name, company, i);
			if (!STRING_IS_EMPTY (u))
				strings = g_list_append (strings, u);
			else
				g_free (u);
		}
	}

	if (combo_file_as) {
		GList *l;

		gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (combo_file_as)));

		for (l = strings; l; l = l->next) {
			gtk_combo_box_append_text (combo_file_as, l->data);
		}
	}

	g_list_foreach (strings, (GFunc) g_free, NULL);
	g_list_free (strings);

	if (style != -1) {
		string = name_to_style (editor->name, company, style);
		set_entry_text (editor, GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo_file_as))), string);
		g_free (string);
	}
}

static void
name_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	gint style = 0;
	const gchar *string;

	style = file_as_get_style (editor);
	e_contact_name_free (editor->name);
	string = gtk_entry_get_text (GTK_ENTRY (widget));
	editor->name = e_contact_name_from_string (string);
	file_as_set_style (editor, style);

	sensitize_ok (editor);
	if (string && !*string)
		gtk_window_set_title (GTK_WINDOW (editor->app), _("Contact Editor"));
}

static void
file_as_combo_changed (GtkWidget *widget, EContactEditor *editor)
{
	gchar *string = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));

	if (string && *string) {
		gchar *title;
		title = g_strdup_printf (_("Contact Editor - %s"), string);
		gtk_window_set_title (GTK_WINDOW (editor->app), title);
		g_free (title);
	}
	else {
		gtk_window_set_title (GTK_WINDOW (editor->app), _("Contact Editor"));
	}
	sensitize_ok (editor);

	g_free (string);
}

static void
company_entry_changed (GtkWidget *widget, EContactEditor *editor)
{
	gint style = 0;

	style = file_as_get_style (editor);
	file_as_set_style (editor, style);
}

static void
update_file_as_combo (EContactEditor *editor)
{
	file_as_set_style (editor, file_as_get_style (editor));
}

static void
fill_in_source_field (EContactEditor *editor)
{
	GtkWidget *source_menu;

	if (!editor->target_book)
		return;

	source_menu = e_builder_get_widget (
		editor->builder, "source-combo-box-source");

	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (source_menu),
		e_book_get_source (editor->target_book));
}

static void
sensitize_ok (EContactEditor *ce)
{
	GtkWidget *widget;
	gboolean   allow_save;
	GtkWidget *entry_fullname = e_builder_get_widget (ce->builder, "entry-fullname" );
	GtkWidget *entry_file_as = gtk_bin_get_child (GTK_BIN (e_builder_get_widget (ce->builder, "combo-file-as")));
	GtkWidget *company_name = e_builder_get_widget (ce->builder, "entry-company");
	const gchar *name_entry_string = gtk_entry_get_text (GTK_ENTRY (entry_fullname));
	const gchar *file_as_entry_string = gtk_entry_get_text (GTK_ENTRY (entry_file_as));
	const gchar *company_name_string = gtk_entry_get_text (GTK_ENTRY (company_name));

	allow_save = ce->target_editable && ce->changed ? TRUE : FALSE;

	if (!strcmp (name_entry_string, "") || !strcmp (file_as_entry_string, "")) {
		if (strcmp (company_name_string , "")) {
			allow_save = TRUE;
		}
		else
			allow_save = FALSE;
	}
	widget = e_builder_get_widget (ce->builder, "button-ok");
	gtk_widget_set_sensitive (widget, allow_save);
}

static void
object_changed (GObject *object, EContactEditor *editor)
{
	if (!editor->target_editable) {
		g_warning ("non-editable contact editor has an editable field in it.");
		return;
	}

	if (!editor->changed) {
		editor->changed = TRUE;
		sensitize_ok (editor);
	}
}

static void
image_chooser_changed (GtkWidget *widget, EContactEditor *editor)
{
	editor->image_set = TRUE;
	editor->image_changed = TRUE;
}

static void
set_entry_text (EContactEditor *editor, GtkEntry *entry, const gchar *string)
{
	const gchar *oldstring = gtk_entry_get_text (entry);

	if (!string)
		string = "";

	if (strcmp (string, oldstring)) {
		g_signal_handlers_block_matched (entry, G_SIGNAL_MATCH_DATA,
						 0, 0, NULL, NULL, editor);
		gtk_entry_set_text (entry, string);
		g_signal_handlers_unblock_matched (entry, G_SIGNAL_MATCH_DATA,
						   0, 0, NULL, NULL, editor);
	}
}

static void
set_combo_box_active (EContactEditor *editor, GtkComboBox *combo_box, gint active)
{
	g_signal_handlers_block_matched (combo_box, G_SIGNAL_MATCH_DATA,
					 0, 0, NULL, NULL, editor);
	gtk_combo_box_set_active (combo_box, active);
	g_signal_handlers_unblock_matched (combo_box, G_SIGNAL_MATCH_DATA,
					   0, 0, NULL, NULL, editor);
}

static void
init_email_record_location (EContactEditor *editor, gint record)
{
	GtkComboBox *location_combo_box;
	GtkWidget *email_entry;
	gchar     *widget_name;
	gint       i;

	widget_name = g_strdup_printf ("entry-email-%d", record);
	email_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("combobox-email-%d", record);
	location_combo_box = GTK_COMBO_BOX (e_builder_get_widget (editor->builder, widget_name));
	g_free (widget_name);

	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (location_combo_box)));

	for (i = 0; i < G_N_ELEMENTS (common_location); i++) {
		gtk_combo_box_append_text (location_combo_box, _(common_location[i].pretty_name));
	}

	g_signal_connect_swapped (location_combo_box, "changed", G_CALLBACK (gtk_widget_grab_focus), email_entry);
	g_signal_connect (location_combo_box, "changed", G_CALLBACK (object_changed), editor);
	g_signal_connect (email_entry, "changed", G_CALLBACK (object_changed), editor);
	g_signal_connect_swapped (email_entry, "activate", G_CALLBACK (entry_activated), editor);
}

static void
fill_in_email_record (EContactEditor *editor, gint record, const gchar *address, gint location)
{
	GtkWidget *location_combo_box;
	GtkWidget *email_entry;
	gchar     *widget_name;

	widget_name = g_strdup_printf ("combobox-email-%d", record);
	location_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("entry-email-%d", record);
	email_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	set_combo_box_active (editor, GTK_COMBO_BOX (location_combo_box),
				 location >= 0 ? location : email_default[2]);
	set_entry_text (editor, GTK_ENTRY (email_entry), address ? address : "");
}

static void
extract_email_record (EContactEditor *editor, gint record, gchar **address, gint *location)
{
	GtkWidget *location_combo_box;
	GtkWidget *email_entry;
	gchar *widget_name;
	const gchar *text;

	widget_name = g_strdup_printf ("combobox-email-%d", record);
	location_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("entry-email-%d", record);
	email_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	text = gtk_entry_get_text (GTK_ENTRY (email_entry));
	*address  = g_strstrip (g_strdup (text));
	*location = gtk_combo_box_get_active (GTK_COMBO_BOX (location_combo_box));
}

static const gchar *
email_index_to_location (gint index)
{
	return common_location[index].name;
}

static const gchar *
im_index_to_location (gint index)
{
	return common_location[index].name;
}

static void
phone_index_to_type (gint index, const gchar **type_1, const gchar **type_2)
{
	*type_1 = phones [index].type_1;
	*type_2 = phones [index].type_2;
}

static gint
get_email_location (EVCardAttribute *attr)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (common_location); i++) {
		if (e_vcard_attribute_has_type (attr, common_location[i].name))
			return i;
	}

	return -1;
}

static gint
get_im_location (EVCardAttribute *attr)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (common_location); i++) {
		if (e_vcard_attribute_has_type (attr, common_location[i].name))
			return i;
	}

	return -1;
}

static gint
get_phone_type (EVCardAttribute *attr)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (phones); i++) {
		if (e_vcard_attribute_has_type (attr, phones[i].type_1) &&
		    (phones[i].type_2 == NULL || e_vcard_attribute_has_type (attr, phones[i].type_2)))
			return i;
	}

	return -1;
}

static EVCardAttributeParam *
get_ui_slot_param (EVCardAttribute *attr)
{
	EVCardAttributeParam *param = NULL;
	GList                *param_list;
	GList                *l;

	param_list = e_vcard_attribute_get_params (attr);

	for (l = param_list; l; l = g_list_next (l)) {
		const gchar *str;

		param = l->data;

		str = e_vcard_attribute_param_get_name (param);
		if (!g_ascii_strcasecmp (str, EVOLUTION_UI_SLOT_PARAM))
			break;

		param = NULL;
	}

	return param;
}

static gint
get_ui_slot (EVCardAttribute *attr)
{
	EVCardAttributeParam *param;
	gint                  slot = -1;

	param = get_ui_slot_param (attr);

	if (param) {
		GList *value_list;

		value_list = e_vcard_attribute_param_get_values (param);
		slot = atoi (value_list->data);
	}

	return slot;
}

static void
set_ui_slot (EVCardAttribute *attr, gint slot)
{
	EVCardAttributeParam *param;
	gchar                *slot_str;

	param = get_ui_slot_param (attr);
	if (!param) {
		param = e_vcard_attribute_param_new (EVOLUTION_UI_SLOT_PARAM);
		e_vcard_attribute_add_param (attr, param);
	}

	e_vcard_attribute_param_remove_values (param);

	slot_str = g_strdup_printf ("%d", slot);
	e_vcard_attribute_param_add_value (param, slot_str);
	g_free (slot_str);
}

static gint
alloc_ui_slot (EContactEditor *editor, const gchar *widget_base, gint preferred_slot, gint num_slots)
{
	gchar       *widget_name;
	GtkWidget   *widget;
	const gchar *entry_contents;
	gint         i;

	/* See if we can get the preferred slot */

	if (preferred_slot >= 1) {
		widget_name = g_strdup_printf ("%s-%d", widget_base, preferred_slot);
		widget = e_builder_get_widget (editor->builder, widget_name);
		entry_contents = gtk_entry_get_text (GTK_ENTRY (widget));
		g_free (widget_name);

		if (STRING_IS_EMPTY (entry_contents))
			return preferred_slot;
	}

	/* Find first empty slot */

	for (i = 1; i <= num_slots; i++) {
		widget_name = g_strdup_printf ("%s-%d", widget_base, i);
		widget = e_builder_get_widget (editor->builder, widget_name);
		entry_contents = gtk_entry_get_text (GTK_ENTRY (widget));
		g_free (widget_name);

		if (STRING_IS_EMPTY (entry_contents))
			return i;
	}

	return -1;
}

static void
free_attr_list (GList *attr_list)
{
	GList *l;

	for (l = attr_list; l; l = g_list_next (l)) {
		EVCardAttribute *attr = l->data;
		e_vcard_attribute_free (attr);
	}

	g_list_free (attr_list);
}

static void
fill_in_email (EContactEditor *editor)
{
	GList *email_attr_list;
	GList *l;
	gint   record_n;

	/* Clear */

	for (record_n = 1; record_n <= EMAIL_SLOTS; record_n++) {
		fill_in_email_record (editor, record_n, NULL, email_default[record_n - 1]);
	}

	/* Fill in */

	email_attr_list = e_contact_get_attributes (editor->contact, E_CONTACT_EMAIL);

	for (record_n = 1, l = email_attr_list; l && record_n <= EMAIL_SLOTS; l = g_list_next (l)) {
		EVCardAttribute *attr = l->data;
		gchar           *email_address;
		gint             slot;

		email_address = e_vcard_attribute_get_value (attr);
		slot = alloc_ui_slot (editor, "entry-email", get_ui_slot (attr), EMAIL_SLOTS);
		if (slot < 1)
			break;

		fill_in_email_record (editor, slot, email_address,
				      get_email_location (attr));

		record_n++;
	}
}

static void
extract_email (EContactEditor *editor)
{
	GList *attr_list = NULL;
	GList *old_attr_list;
	GList *l, *l_next;
	gint   i;

	for (i = 1; i <= EMAIL_SLOTS; i++) {
		gchar *address;
		gint   location;

		extract_email_record (editor, i, &address, &location);

		if (!STRING_IS_EMPTY (address)) {
			EVCardAttribute *attr;
			attr = e_vcard_attribute_new ("", e_contact_vcard_attribute (E_CONTACT_EMAIL));

			if (location >= 0)
				e_vcard_attribute_add_param_with_value (attr,
									e_vcard_attribute_param_new (EVC_TYPE),
									email_index_to_location (location));

			e_vcard_attribute_add_value (attr, address);
			set_ui_slot (attr, i);

			attr_list = g_list_append (attr_list, attr);
		}

		g_free (address);
	}

	/* Splice in the old attributes, minus the EMAIL_SLOTS first */

	old_attr_list = e_contact_get_attributes (editor->contact, E_CONTACT_EMAIL);
	for (l = old_attr_list, i = 1; l && i <= EMAIL_SLOTS; l = l_next, i++) {
		l_next = g_list_next (l);

		e_vcard_attribute_free (l->data);
		l = g_list_delete_link (l, l);
	}

	old_attr_list = l;
	attr_list = g_list_concat (attr_list, old_attr_list);

	e_contact_set_attributes (editor->contact, E_CONTACT_EMAIL, attr_list);

	free_attr_list (attr_list);
}

static void
sensitize_email_record (EContactEditor *editor, gint record, gboolean enabled)
{
	GtkWidget *location_combo_box;
	GtkWidget *email_entry;
	gchar     *widget_name;

	widget_name = g_strdup_printf ("combobox-email-%d", record);
	location_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("entry-email-%d", record);
	email_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	gtk_widget_set_sensitive (location_combo_box, enabled);
	gtk_editable_set_editable (GTK_EDITABLE (email_entry), enabled);
}

static void
sensitize_email (EContactEditor *editor)
{
	gint i;

	for (i = 1; i <= EMAIL_SLOTS; i++) {
		gboolean enabled = TRUE;

		if (!editor->target_editable)
			enabled = FALSE;

		if (E_CONTACT_FIRST_EMAIL_ID + i - 1 <= E_CONTACT_LAST_EMAIL_ID &&
		    !is_field_supported (editor, E_CONTACT_FIRST_EMAIL_ID + i - 1))
			enabled = FALSE;

		sensitize_email_record (editor, i, enabled);
	}
}

static void
init_item_sensitiveable_combo_box (GtkComboBox *combo)
{
	GtkCellRenderer *cell;
	GtkListStore *store;

	g_return_if_fail (combo != NULL);
	g_return_if_fail (GTK_IS_COMBO_BOX (combo));

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
                                  "text", 0,
                                  "sensitive", 1,
                                  NULL);
}

/* EContact can get attributes by field ID only, and there is none for TEL, so we need this */
static GList *
get_attributes_named (EVCard *vcard, const gchar *attr_name)
{
	GList *attr_list_in;
	GList *attr_list_out = NULL;
	GList *l;

	attr_list_in = e_vcard_get_attributes (vcard);

	for (l = attr_list_in; l; l = g_list_next (l)) {
		EVCardAttribute *attr = l->data;
		const gchar *name;

		name = e_vcard_attribute_get_name (attr);

		if (!g_ascii_strcasecmp (attr_name, name)) {
			attr_list_out = g_list_append (attr_list_out, e_vcard_attribute_copy (attr));
		}
	}

	return attr_list_out;
}

/* EContact can set attributes by field ID only, and there is none for TEL, so we need this */
static void
set_attributes_named (EVCard *vcard, const gchar *attr_name, GList *attr_list)
{
	GList *l;

	e_vcard_remove_attributes (vcard, NULL, attr_name);

	for (l = attr_list; l; l = g_list_next (l)) {
		EVCardAttribute *attr = l->data;

		e_vcard_add_attribute (vcard, e_vcard_attribute_copy (attr));
	}
}

static void
set_arrow_image (EContactEditor *editor,
		 const gchar *arrow_widget,
		 gboolean expanded)
{
	GtkWidget *arrow;

	arrow  = e_builder_get_widget (editor->builder, arrow_widget);
	if (expanded)
		gtk_arrow_set (GTK_ARROW (arrow), GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	else
		gtk_arrow_set (GTK_ARROW (arrow), GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
}

static void
expand_widget_list (EContactEditor *editor,
		    const gchar **widget_names,
		    gboolean expanded)
{
	gint i;
	for (i = 0; widget_names[i]; i++)
		gtk_widget_set_visible (
			e_builder_get_widget (editor->builder, widget_names[i]),
			expanded);
}

static void
expand_web (EContactEditor *editor, gboolean expanded)
{
	const gchar *names[] = {
		"label-videourl", "label-fburl",
		"entry-videourl", "entry-fburl",
		NULL
	};
	set_arrow_image (editor, "arrow-web-expand", expanded);
	expand_widget_list (editor, names, expanded);
}

static void
expand_phone (EContactEditor *editor, gboolean expanded)
{
	const gchar *names[] = {
		"entry-phone-2", "combobox-phone-2",
		"entry-phone-4", "combobox-phone-4",
		"table-phone-extended", NULL
	};
	set_arrow_image (editor, "arrow-phone-expand", expanded);
	expand_widget_list (editor, names, expanded);
}

static void
expand_mail (EContactEditor *editor, gboolean expanded)
{
	GtkTable  *table;
	GtkWidget *check;
	const gchar *names[] = {
		"entry-email-2", "combobox-email-2",
		"entry-email-3", "combobox-email-3",
		"entry-email-4", "combobox-email-4",
		NULL
	};
	set_arrow_image (editor, "arrow-mail-expand", expanded);
	expand_widget_list (editor, names, expanded);

	/* move 'use html mail' into position */
	check = e_builder_get_widget (editor->builder, "checkbutton-htmlmail");
	table = GTK_TABLE (e_builder_get_widget (editor->builder, "email-table"));
	if (check != NULL && table != NULL) {
		GtkWidget *parent;

		g_object_ref (check);
		parent = gtk_widget_get_parent (check);
		gtk_container_remove (GTK_CONTAINER (parent), check);
		if (expanded)
			gtk_table_attach_defaults (table, check, 0, 4, 2, 3);
		else
			gtk_table_attach_defaults (table, check, 2, 4, 0, 1);
		g_object_unref (check);
	}
}

static void
init_email (EContactEditor *editor)
{
	gint i;

	for (i = 1; i <= EMAIL_SLOTS; i++)
		init_email_record_location (editor, i);

	expand_mail (editor, !editor->compress_ui);
}

static void
fill_in_phone_record (EContactEditor *editor, gint record, const gchar *phone, gint phone_type)
{
	GtkWidget *phone_type_combo_box;
	GtkWidget *phone_entry;
	gchar     *widget_name;

	widget_name = g_strdup_printf ("combobox-phone-%d", record);
	phone_type_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("entry-phone-%d", record);
	phone_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	set_combo_box_active (editor, GTK_COMBO_BOX (phone_type_combo_box),
				 phone_type >= 0 ? phone_type :
				 phones_default[record - 1]);
	set_entry_text (editor, GTK_ENTRY (phone_entry), phone ? phone : "");

	if (phone && *phone && record >= 5)
		expand_phone (editor, TRUE);
}

static void
extract_phone_record (EContactEditor *editor, gint record, gchar **phone, gint *phone_type)
{
	GtkWidget *phone_type_combo_box;
	GtkWidget *phone_entry;
	gchar     *widget_name;

	widget_name = g_strdup_printf ("combobox-phone-%d", record);
	phone_type_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("entry-phone-%d", record);
	phone_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	*phone      = g_strdup (gtk_entry_get_text (GTK_ENTRY (phone_entry)));
	*phone_type = gtk_combo_box_get_active (GTK_COMBO_BOX (phone_type_combo_box));
}

static void
fill_in_phone (EContactEditor *editor)
{
	GList *phone_attr_list;
	GList *l;
	gint   record_n;

	/* Clear */

	for (record_n = 1; record_n <= PHONE_SLOTS; record_n++) {
		fill_in_phone_record (editor, record_n, NULL, -1);
	}

	/* Fill in */

	phone_attr_list = get_attributes_named (E_VCARD (editor->contact), "TEL");

	for (record_n = 1, l = phone_attr_list; l && record_n <= PHONE_SLOTS; l = g_list_next (l)) {
		EVCardAttribute *attr = l->data;
		gchar           *phone;
		gint             slot;

		phone = e_vcard_attribute_get_value (attr);
		slot = alloc_ui_slot (editor, "entry-phone", get_ui_slot (attr), PHONE_SLOTS);
		if (slot < 1)
			break;

		fill_in_phone_record (editor, slot, phone,
				      get_phone_type (attr));

		record_n++;
	}
}

static void
extract_phone (EContactEditor *editor)
{
	GList *attr_list = NULL;
	GList *old_attr_list;
	GList *l, *l_next;
	gint   i;

	for (i = 1; i <= PHONE_SLOTS; i++) {
		gchar *phone;
		gint   phone_type;

		extract_phone_record (editor, i, &phone, &phone_type);

		if (!STRING_IS_EMPTY (phone)) {
			EVCardAttribute *attr;

			attr = e_vcard_attribute_new ("", "TEL");

			if (phone_type >= 0) {
				const gchar *type_1;
				const gchar *type_2;

				phone_index_to_type (phone_type, &type_1, &type_2);

				e_vcard_attribute_add_param_with_value (
					attr, e_vcard_attribute_param_new (EVC_TYPE), type_1);

				if (type_2)
					e_vcard_attribute_add_param_with_value (
						attr, e_vcard_attribute_param_new (EVC_TYPE), type_2);

			}

			e_vcard_attribute_add_value (attr, phone);
			set_ui_slot (attr, i);

			attr_list = g_list_append (attr_list, attr);
		}

		g_free (phone);
	}

	/* Splice in the old attributes, minus the PHONE_SLOTS first */

	old_attr_list = get_attributes_named (E_VCARD (editor->contact), "TEL");
	for (l = old_attr_list, i = 1; l && i <= PHONE_SLOTS; l = l_next, i++) {
		l_next = g_list_next (l);

		e_vcard_attribute_free (l->data);
		l = g_list_delete_link (l, l);
	}

	old_attr_list = l;
	attr_list = g_list_concat (attr_list, old_attr_list);

	set_attributes_named (E_VCARD (editor->contact), "TEL", attr_list);

	free_attr_list (attr_list);
}

static void
init_phone_record_type (EContactEditor *editor, gint record)
{
	GtkWidget *phone_type_combo_box;
	GtkWidget *phone_entry;
	gchar     *widget_name;
	gint       i;
	GtkListStore *store;

	widget_name = g_strdup_printf ("entry-phone-%d", record);
	phone_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("combobox-phone-%d", record);
	phone_type_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	init_item_sensitiveable_combo_box (GTK_COMBO_BOX (phone_type_combo_box));

	store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (phone_type_combo_box)));
	for (i = 0; i < G_N_ELEMENTS (phones); i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0, e_contact_pretty_name (phones[i].field_id),
			1, TRUE,
			-1);
	}

	g_signal_connect_swapped (phone_type_combo_box, "changed", G_CALLBACK (gtk_widget_grab_focus), phone_entry);
	g_signal_connect (phone_type_combo_box, "changed", G_CALLBACK (object_changed), editor);
	g_signal_connect (phone_entry, "changed", G_CALLBACK (object_changed), editor);
	g_signal_connect_swapped (phone_entry, "activate", G_CALLBACK (entry_activated), editor);
}

static void
init_phone (EContactEditor *editor)
{
	gint i;

	expand_phone (editor, FALSE);

	for (i = 1; i <= PHONE_SLOTS; i++)
		init_phone_record_type (editor, i);
}

static void
sensitize_phone_types (EContactEditor *editor, GtkWidget *combo_box)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint i;
	gboolean valid;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	for (i = 0; i < G_N_ELEMENTS (phones); i++) {
		if (!valid) {
			g_warning (G_STRLOC ": Unexpected end of phone items in combo box");
			return;
		}

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			1, is_field_supported (editor, phones[i].field_id),
			-1);

		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
sensitize_phone_record (EContactEditor *editor, gint record, gboolean enabled)
{
	GtkWidget *phone_type_combo_box;
	GtkWidget *phone_entry;
	gchar     *widget_name;

	widget_name = g_strdup_printf ("combobox-phone-%d", record);
	phone_type_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("entry-phone-%d", record);
	phone_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	gtk_widget_set_sensitive (phone_type_combo_box, enabled);
	gtk_editable_set_editable (GTK_EDITABLE (phone_entry), enabled);

	sensitize_phone_types (editor, phone_type_combo_box);
}

static void
sensitize_phone (EContactEditor *editor)
{
	gint i;

	for (i = 1; i <= PHONE_SLOTS; i++) {
		gboolean enabled = TRUE;

		if (!editor->target_editable)
			enabled = FALSE;

		sensitize_phone_record (editor, i, enabled);
	}
}

static void
init_im_record_location (EContactEditor *editor, gint record)
{

#ifdef ENABLE_IM_LOCATION
	GtkWidget *location_combo_box;
	GtkListStore *store;
	gint i;
	gchar *widget_name;

	widget_name = g_strdup_printf ("combobox-im-location-%d", record);
	location_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	init_item_sensitiveable_combo_box (GTK_COMBO_BOX (location_combo_box));

	store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (location_combo_box)));
	for (i = 0; i < G_N_ELEMENTS (common_location); i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0, _(common_location[i].pretty_name),
			1, TRUE,
			-1);
	}

	g_signal_connect (location_combo_box, "changed", G_CALLBACK (object_changed), editor);
#endif
}

static void
init_im_record_service (EContactEditor *editor, gint record)
{
	GtkWidget *service_combo_box;
	GtkListStore *store;
	GtkWidget *name_entry;
	gchar     *widget_name;
	gint       i;

	widget_name = g_strdup_printf ("entry-im-name-%d", record);
	name_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("combobox-im-service-%d", record);
	service_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	if (editor->compress_ui && record > 2) {
		gtk_widget_hide (name_entry);
		gtk_widget_hide (service_combo_box);
	}

	init_item_sensitiveable_combo_box (GTK_COMBO_BOX (service_combo_box));

	store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (service_combo_box)));
	for (i = 0; i < G_N_ELEMENTS (im_service); i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0, im_service[i].pretty_name,
			1, TRUE,
			-1);
	}

	g_signal_connect_swapped (service_combo_box, "changed", G_CALLBACK (gtk_widget_grab_focus), name_entry);
	g_signal_connect (service_combo_box, "changed", G_CALLBACK (object_changed), editor);
	g_signal_connect (name_entry, "changed", G_CALLBACK (object_changed), editor);
	g_signal_connect_swapped (name_entry, "activate", G_CALLBACK (entry_activated), editor);
}

static void
init_im (EContactEditor *editor)
{
	gint i;

	for (i = 1; i <= IM_SLOTS; i++) {
		init_im_record_service  (editor, i);
		init_im_record_location (editor, i);
	}
}

static void
fill_in_im_record (EContactEditor *editor, gint record, gint service, const gchar *name, gint location)
{
	GtkWidget *service_combo_box;
#ifdef ENABLE_IM_LOCATION
	GtkWidget *location_combo_box;
#endif
	GtkWidget *name_entry;
	gchar     *widget_name;

	widget_name = g_strdup_printf ("combobox-im-service-%d", record);
	service_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

#ifdef ENABLE_IM_LOCATION
	widget_name = g_strdup_printf ("combobox-im-location-%d", record);
	location_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);
#endif

	widget_name = g_strdup_printf ("entry-im-name-%d", record);
	name_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

#ifdef ENABLE_IM_LOCATION
	set_combo_box_active (editor, GTK_COMBO_BOX (location_combo_box),
				 location >= 0 ? location : 0);
#endif
	set_combo_box_active (editor, GTK_COMBO_BOX (service_combo_box),
				 service >= 0 ? service : im_service_default[record - 1]);
	set_entry_text (editor, GTK_ENTRY (name_entry), name ? name : "");
}

static void
fill_in_im (EContactEditor *editor)
{
	GList *im_attr_list;
	GList *l;
	gint   record_n;
	gint   i;

	/* Clear */

	for (record_n = 1; record_n <= IM_SLOTS; record_n++) {
		fill_in_im_record (editor, record_n, -1, NULL, -1);
	}

	/* Fill in */

	for (record_n = 1, i = 0; i < G_N_ELEMENTS (im_service); i++) {
		im_attr_list = e_contact_get_attributes (editor->contact, im_service[i].field);

		for (l = im_attr_list; l && record_n <= IM_SLOTS; l = g_list_next (l)) {
			EVCardAttribute *attr = l->data;
			gchar           *im_name;
			gint             slot;

			im_name = e_vcard_attribute_get_value (attr);
			slot = alloc_ui_slot (editor, "entry-im-name", get_ui_slot (attr), IM_SLOTS);
			if (slot < 1)
				break;

			fill_in_im_record (editor, slot, i, im_name,
					   get_im_location (attr));

			record_n++;
		}
	}
}

static void
extract_im_record (EContactEditor *editor, gint record, gint *service, gchar **name, gint *location)
{
	GtkWidget *service_combo_box;
#ifdef ENABLE_IM_LOCATION
	GtkWidget *location_combo_box;
#endif
	GtkWidget *name_entry;
	gchar     *widget_name;

	widget_name = g_strdup_printf ("combobox-im-service-%d", record);
	service_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

#ifdef ENABLE_IM_LOCATION
	widget_name = g_strdup_printf ("combobox-im-location-%d", record);
	location_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);
#endif

	widget_name = g_strdup_printf ("entry-im-name-%d", record);
	name_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	*name  = g_strdup (gtk_entry_get_text (GTK_ENTRY (name_entry)));
	*service = gtk_combo_box_get_active (GTK_COMBO_BOX (service_combo_box));
#ifdef ENABLE_IM_LOCATION
	*location = gtk_combo_box_get_active (GTK_COMBO_BOX (location_combo_box));
#else
	*location = 1; /* set everything to HOME */
#endif
}

static void
extract_im (EContactEditor *editor)
{
	GList **service_attr_list;
	gint    remaining_slots = IM_SLOTS;
	gint    i;

	service_attr_list = g_new0 (GList *, G_N_ELEMENTS (im_service));

	for (i = 1; i <= IM_SLOTS; i++) {
		EVCardAttribute *attr;
		gchar           *name;
		gint             service;
		gint             location;

		extract_im_record (editor, i, &service, &name, &location);

		if (!STRING_IS_EMPTY (name)) {
			attr = e_vcard_attribute_new ("", e_contact_vcard_attribute (im_service [service].field));

			if (location >= 0)
				e_vcard_attribute_add_param_with_value (attr,
									e_vcard_attribute_param_new (EVC_TYPE),
									im_index_to_location (location));

			e_vcard_attribute_add_value (attr, name);
			set_ui_slot (attr, i);

			service_attr_list[service] = g_list_append (service_attr_list[service], attr);
		}

		g_free (name);
	}

	for (i = 0; i < G_N_ELEMENTS (im_service); i++) {
		GList *old_service_attr_list;
		gint   filled_in_slots;
		GList *l, *l_next;
		gint   j;

		/* Splice in the old attributes, minus the filled_in_slots first */

		old_service_attr_list = e_contact_get_attributes (editor->contact, im_service[i].field);
		filled_in_slots = MIN (remaining_slots, g_list_length (old_service_attr_list));
		remaining_slots -= filled_in_slots;

		for (l = old_service_attr_list, j = 0; l && j < filled_in_slots; l = l_next, j++) {
			l_next = g_list_next (l);

			e_vcard_attribute_free (l->data);
			l = g_list_delete_link (l, l);
		}

		old_service_attr_list = l;
		service_attr_list[i] = g_list_concat (service_attr_list[i], old_service_attr_list);

		e_contact_set_attributes (editor->contact, im_service[i].field,
					  service_attr_list[i]);

		free_attr_list (service_attr_list[i]);
	}

	g_free (service_attr_list);
}
static void
sensitize_im_types (EContactEditor *editor, GtkWidget *combo_box)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint i;
	gboolean valid;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	for (i = 0; i < G_N_ELEMENTS (im_service); i++) {
		if (!valid) {
			g_warning (G_STRLOC ": Unexpected end of im items in combo box");
			return;
		}

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			1, is_field_supported (editor, im_service[i].field),
			-1);

		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
sensitize_im_record (EContactEditor *editor, gint record, gboolean enabled)
{
	GtkWidget *service_combo_box;
#ifdef ENABLE_IM_LOCATION
	GtkWidget *location_combo_box;
#endif
	GtkWidget *name_entry;
	gchar     *widget_name;

	widget_name = g_strdup_printf ("combobox-im-service-%d", record);
	service_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

#ifdef ENABLE_IM_LOCATION
	widget_name = g_strdup_printf ("combobox-im-location-%d", record);
	location_combo_box = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);
#endif

	widget_name = g_strdup_printf ("entry-im-name-%d", record);
	name_entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	gtk_widget_set_sensitive (service_combo_box, enabled);
#ifdef ENABLE_IM_LOCATION
	gtk_widget_set_sensitive (location_combo_box, enabled);
#endif
	gtk_editable_set_editable (GTK_EDITABLE (name_entry), enabled);
	sensitize_im_types (editor, service_combo_box);
}

static void
sensitize_im (EContactEditor *editor)
{
	gint i;
	gboolean enabled;
	gboolean no_ims_supported;

	enabled = editor->target_editable;
	no_ims_supported = TRUE;

	for (i = 0; i < G_N_ELEMENTS (im_service); i++)
		if (is_field_supported (editor, im_service[i].field)) {
			no_ims_supported = FALSE;
			break;
		}

	if (no_ims_supported)
		enabled = FALSE;

	for (i = 1; i <= IM_SLOTS; i++) {
		sensitize_im_record (editor, i, enabled);
	}
}

static void
init_personal (EContactEditor *editor)
{
	gtk_expander_set_expanded (
		GTK_EXPANDER (e_builder_get_widget (editor->builder,
						    "expander-personal-misc")),
		!editor->compress_ui);

	expand_web (editor, !editor->compress_ui);
}

static void
init_address_textview (EContactEditor *editor, gint record)
{
	gchar         *textview_name;
	GtkWidget     *textview;
	GtkTextBuffer *text_buffer;

	textview_name = g_strdup_printf ("textview-%s-address", address_name [record]);
	textview = e_builder_get_widget (editor->builder, textview_name);
	g_free (textview_name);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	g_signal_connect (text_buffer, "changed", G_CALLBACK (object_changed), editor);
}

static void
init_address_field (EContactEditor *editor, gint record, const gchar *widget_field_name)
{
	gchar     *entry_name;
	GtkWidget *entry;

	entry_name = g_strdup_printf ("entry-%s-%s", address_name [record], widget_field_name);
	entry = e_builder_get_widget (editor->builder, entry_name);
	g_free (entry_name);

	g_signal_connect (entry, "changed", G_CALLBACK (object_changed), editor);
	g_signal_connect_swapped (entry, "activate", G_CALLBACK (entry_activated), editor);
}

static void
init_address_record (EContactEditor *editor, gint record)
{
	init_address_textview (editor, record);
	init_address_field (editor, record, "city");
	init_address_field (editor, record, "state");
	init_address_field (editor, record, "zip");
	init_address_field (editor, record, "country");
	init_address_field (editor, record, "pobox");
}

static void
init_address (EContactEditor *editor)
{
	gint i;

	for (i = 0; i < ADDRESS_SLOTS; i++)
		init_address_record (editor, i);

	gtk_expander_set_expanded (
		GTK_EXPANDER (e_builder_get_widget (editor->builder,
						    "expander-address-other")),
		!editor->compress_ui);
}

static void
fill_in_address_textview (EContactEditor *editor, gint record, EContactAddress *address)
{
	gchar         *textview_name;
	GtkWidget     *textview;
	GtkTextBuffer *text_buffer;
	GtkTextIter    iter_end, iter_start;

	textview_name = g_strdup_printf ("textview-%s-address", address_name [record]);
	textview = e_builder_get_widget (editor->builder, textview_name);
	g_free (textview_name);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	gtk_text_buffer_set_text (text_buffer, address->street ? address->street : "", -1);

	gtk_text_buffer_get_end_iter (text_buffer, &iter_end);
	if (address->ext && *address->ext) {
		gtk_text_buffer_insert (text_buffer, &iter_end, "\n", -1);
		gtk_text_buffer_insert (text_buffer, &iter_end, address->ext, -1);
	} else {
		gtk_text_buffer_insert (text_buffer, &iter_end, "", -1);
	}
	gtk_text_buffer_get_iter_at_line (text_buffer, &iter_start, 0);
	gtk_text_buffer_place_cursor (text_buffer, &iter_start);
}

static void
fill_in_address_label_textview (EContactEditor *editor, gint record, const gchar *label)
{
	gchar         *textview_name;
	GtkWidget     *textview;
	GtkTextBuffer *text_buffer;

	textview_name = g_strdup_printf ("textview-%s-address", address_name [record]);
	textview = e_builder_get_widget (editor->builder, textview_name);
	g_free (textview_name);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	gtk_text_buffer_set_text (text_buffer, label ? label : "", -1);
}

static void
fill_in_address_field (EContactEditor *editor, gint record, const gchar *widget_field_name,
		       const gchar *string)
{
	gchar     *entry_name;
	GtkWidget *entry;

	entry_name = g_strdup_printf ("entry-%s-%s", address_name [record], widget_field_name);
	entry = e_builder_get_widget (editor->builder, entry_name);
	g_free (entry_name);

	set_entry_text (editor, GTK_ENTRY (entry), string);
}

static void
fill_in_address_record (EContactEditor *editor, gint record)
{
	EContactAddress *address;
	gchar           *address_label;

	address = e_contact_get (editor->contact, addresses[record]);
	address_label = e_contact_get (editor->contact, address_labels[record]);

	if (address &&
	    (!STRING_IS_EMPTY (address->street)   ||
	     !STRING_IS_EMPTY (address->ext)      ||
	     !STRING_IS_EMPTY (address->locality) ||
	     !STRING_IS_EMPTY (address->region)   ||
	     !STRING_IS_EMPTY (address->code)     ||
	     !STRING_IS_EMPTY (address->po)       ||
	     !STRING_IS_EMPTY (address->country))) {
		fill_in_address_textview (editor, record, address);
		fill_in_address_field (editor, record, "city", address->locality);
		fill_in_address_field (editor, record, "state", address->region);
		fill_in_address_field (editor, record, "zip", address->code);
		fill_in_address_field (editor, record, "country", address->country);
		fill_in_address_field (editor, record, "pobox", address->po);
	} else if (!STRING_IS_EMPTY (address_label)) {
		fill_in_address_label_textview (editor, record, address_label);
	}

	g_free (address_label);
	if (address)
		g_boxed_free (e_contact_address_get_type (), address);
}

static void
fill_in_address (EContactEditor *editor)
{
	gint i;

	for (i = 0; i < ADDRESS_SLOTS; i++)
		fill_in_address_record (editor, i);
}

static void
extract_address_textview (EContactEditor *editor, gint record, EContactAddress *address)
{
	gchar         *textview_name;
	GtkWidget     *textview;
	GtkTextBuffer *text_buffer;
	GtkTextIter    iter_1, iter_2;

	textview_name = g_strdup_printf ("textview-%s-address", address_name [record]);
	textview = e_builder_get_widget (editor->builder, textview_name);
	g_free (textview_name);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	gtk_text_buffer_get_start_iter (text_buffer, &iter_1);

	/* Skip blank lines */
	while (gtk_text_iter_get_chars_in_line (&iter_1) < 1 &&
	       !gtk_text_iter_is_end (&iter_1))
		gtk_text_iter_forward_line (&iter_1);

	if (gtk_text_iter_is_end (&iter_1))
		return;

	iter_2 = iter_1;
	gtk_text_iter_forward_to_line_end (&iter_2);

	/* Extract street (first line of text) */
	address->street = gtk_text_iter_get_text (&iter_1, &iter_2);

	iter_1 = iter_2;
	gtk_text_iter_forward_line (&iter_1);

	if (gtk_text_iter_is_end (&iter_1))
		return;

	gtk_text_iter_forward_to_end (&iter_2);

	/* Extract extended address (remaining lines of text) */
	address->ext = gtk_text_iter_get_text (&iter_1, &iter_2);
}

static gchar *
extract_address_field (EContactEditor *editor, gint record, const gchar *widget_field_name)
{
	gchar     *entry_name;
	GtkWidget *entry;

	entry_name = g_strdup_printf ("entry-%s-%s", address_name [record], widget_field_name);
	entry = e_builder_get_widget (editor->builder, entry_name);
	g_free (entry_name);

	return g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
}

static gchar *
append_to_address_label (gchar *address_label, const gchar *part, gboolean newline)
{
	gchar *new_address_label;

	if (STRING_IS_EMPTY (part))
		return address_label;

	if (address_label)
		new_address_label = g_strjoin (newline ? "\n" : ", ", address_label, part, NULL);
	else
		new_address_label = g_strdup (part);

	g_free (address_label);
	return new_address_label;
}

static void
set_address_label (EContact *contact, EContactField field, EContactAddress *address)
{
	gchar *address_label = NULL;

	if (address) {
		address_label = append_to_address_label (address_label, address->street, TRUE);
		address_label = append_to_address_label (address_label, address->ext, TRUE);
		address_label = append_to_address_label (address_label, address->locality, TRUE);
		address_label = append_to_address_label (address_label, address->region, FALSE);
		address_label = append_to_address_label (address_label, address->code, TRUE);
		address_label = append_to_address_label (address_label, address->po, TRUE);
		address_label = append_to_address_label (address_label, address->country, TRUE);
	}

	e_contact_set (contact, field, address_label);
	g_free (address_label);
}

static void
extract_address_record (EContactEditor *editor, gint record)
{
	EContactAddress *address;

	address = g_new0 (EContactAddress, 1);

	extract_address_textview (editor, record, address);
	address->locality = extract_address_field (editor, record, "city");
	address->region   = extract_address_field (editor, record, "state");
	address->code     = extract_address_field (editor, record, "zip");
	address->country  = extract_address_field (editor, record, "country");
	address->po       = extract_address_field (editor, record, "pobox");

	if (!STRING_IS_EMPTY (address->street)   ||
	    !STRING_IS_EMPTY (address->ext)      ||
	    !STRING_IS_EMPTY (address->locality) ||
	    !STRING_IS_EMPTY (address->region)   ||
	    !STRING_IS_EMPTY (address->code)     ||
	    !STRING_IS_EMPTY (address->po)       ||
	    !STRING_IS_EMPTY (address->country)) {
		e_contact_set (editor->contact, addresses[record], address);
		set_address_label (editor->contact, address_labels[record], address);
	}
	else {
		e_contact_set (editor->contact, addresses[record], NULL);
		set_address_label (editor->contact, address_labels[record], NULL);
	}

	g_boxed_free (e_contact_address_get_type (), address);
}

static void
extract_address (EContactEditor *editor)
{
	gint i;

	for (i = 0; i < ADDRESS_SLOTS; i++)
		extract_address_record (editor, i);
}

static void
sensitize_address_textview (EContactEditor *editor, gint record, gboolean enabled)
{
	gchar         *widget_name;
	GtkWidget     *textview;
	GtkWidget     *label;

	widget_name = g_strdup_printf ("textview-%s-address", address_name [record]);
	textview = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("label-%s-address", address_name [record]);
	label = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), enabled);
	gtk_widget_set_sensitive (label, enabled);
}

static void
sensitize_address_field (EContactEditor *editor, gint record, const gchar *widget_field_name,
			 gboolean enabled)
{
	gchar     *widget_name;
	GtkWidget *entry;
	GtkWidget *label;

	widget_name = g_strdup_printf ("entry-%s-%s", address_name [record], widget_field_name);
	entry = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("label-%s-%s", address_name [record], widget_field_name);
	label = e_builder_get_widget (editor->builder, widget_name);
	g_free (widget_name);

	gtk_editable_set_editable (GTK_EDITABLE (entry), enabled);
	gtk_widget_set_sensitive (label, enabled);
}

static void
sensitize_address_record (EContactEditor *editor, gint record, gboolean enabled)
{
	sensitize_address_textview (editor, record, enabled);
	sensitize_address_field (editor, record, "city", enabled);
	sensitize_address_field (editor, record, "state", enabled);
	sensitize_address_field (editor, record, "zip", enabled);
	sensitize_address_field (editor, record, "country", enabled);
	sensitize_address_field (editor, record, "pobox", enabled);
}

static void
sensitize_address (EContactEditor *editor)
{
	gint i;

	for (i = 0; i < ADDRESS_SLOTS; i++) {
		gboolean enabled = TRUE;

		if (!editor->target_editable ||
		    !(is_field_supported (editor, addresses[i]) ||
		      is_field_supported (editor, address_labels[i])))
			enabled = FALSE;

		sensitize_address_record (editor, i, enabled);
	}
}

typedef struct {
	const gchar   *widget_name;
	gint           field_id;      /* EContactField or -1 */
	gboolean       process_data;  /* If we should extract/fill in contents */
	gboolean       desensitize_for_read_only;
}
FieldMapping;

/* Table of widgets that interact with simple fields. This table is used to:
 *
 * - Fill in data.
 * - Extract data.
 * - Set sensitivity based on backend capabilities.
 * - Set sensitivity based on book writeability. */

static FieldMapping simple_field_map[] = {
	{ "entry-homepage",       E_CONTACT_HOMEPAGE_URL, TRUE,  TRUE  },
	{ "accellabel-homepage",  E_CONTACT_HOMEPAGE_URL, FALSE, TRUE  },

	{ "entry-jobtitle",       E_CONTACT_TITLE,        TRUE,  TRUE  },
	{ "label-jobtitle",       E_CONTACT_TITLE,        FALSE, TRUE  },

	{ "entry-company",        E_CONTACT_ORG,          TRUE,  TRUE  },
	{ "label-company",        E_CONTACT_ORG,          FALSE, TRUE  },

	{ "entry-department",     E_CONTACT_ORG_UNIT,     TRUE,  TRUE  },
	{ "label-department",     E_CONTACT_ORG_UNIT,     FALSE, TRUE  },

	{ "entry-profession",     E_CONTACT_ROLE,         TRUE,  TRUE  },
	{ "label-profession",     E_CONTACT_ROLE,         FALSE, TRUE  },

	{ "entry-manager",        E_CONTACT_MANAGER,      TRUE,  TRUE  },
	{ "label-manager",        E_CONTACT_MANAGER,      FALSE, TRUE  },

	{ "entry-assistant",      E_CONTACT_ASSISTANT,    TRUE,  TRUE  },
	{ "label-assistant",      E_CONTACT_ASSISTANT,    FALSE, TRUE  },

	{ "entry-nickname",       E_CONTACT_NICKNAME,     TRUE,  TRUE  },
	{ "label-nickname",       E_CONTACT_NICKNAME,     FALSE, TRUE  },

	{ "dateedit-birthday",    E_CONTACT_BIRTH_DATE,   TRUE,  TRUE  },
	{ "label-birthday",       E_CONTACT_BIRTH_DATE,   FALSE, TRUE  },

	{ "dateedit-anniversary", E_CONTACT_ANNIVERSARY,  TRUE,  TRUE  },
	{ "label-anniversary",    E_CONTACT_ANNIVERSARY,  FALSE, TRUE  },

	{ "entry-spouse",         E_CONTACT_SPOUSE,       TRUE,  TRUE  },
	{ "label-spouse",         E_CONTACT_SPOUSE,       FALSE, TRUE  },

	{ "entry-office",         E_CONTACT_OFFICE,       TRUE,  TRUE  },
	{ "label-office",         E_CONTACT_OFFICE,       FALSE, TRUE  },

	{ "text-comments",        E_CONTACT_NOTE,         TRUE,  TRUE  },
	{ "label-comments",       E_CONTACT_NOTE,         FALSE, TRUE  },

	{ "entry-fullname",       E_CONTACT_FULL_NAME,    TRUE,  TRUE  },
	{ "button-fullname",      E_CONTACT_FULL_NAME,    FALSE, TRUE  },

	{ "entry-categories",     E_CONTACT_CATEGORIES,   TRUE,  TRUE  },
	{ "button-categories",    E_CONTACT_CATEGORIES,   FALSE, TRUE  },

	{ "entry-weblog",         E_CONTACT_BLOG_URL,     TRUE,  TRUE  },
	{ "label-weblog",         E_CONTACT_BLOG_URL,     FALSE, TRUE  },

	{ "entry-caluri",         E_CONTACT_CALENDAR_URI, TRUE,  TRUE  },
	{ "label-caluri",         E_CONTACT_CALENDAR_URI, FALSE, TRUE  },

	{ "entry-fburl",          E_CONTACT_FREEBUSY_URL, TRUE,  TRUE  },
	{ "label-fburl",          E_CONTACT_FREEBUSY_URL, FALSE, TRUE  },

	{ "entry-videourl",       E_CONTACT_VIDEO_URL,    TRUE,  TRUE  },
	{ "label-videourl",       E_CONTACT_VIDEO_URL,    FALSE, TRUE  },

	{ "checkbutton-htmlmail", E_CONTACT_WANTS_HTML,   TRUE,  TRUE  },

	{ "image-chooser",        E_CONTACT_PHOTO,        TRUE,  TRUE  },
	{ "button-image",         E_CONTACT_PHOTO,        FALSE, TRUE  },

	{ "combo-file-as",        E_CONTACT_FILE_AS,      TRUE,  TRUE  },
	{ "accellabel-fileas",    E_CONTACT_FILE_AS,      FALSE, TRUE  },
};

static void
init_simple_field (EContactEditor *editor, GtkWidget *widget)
{
	GObject *changed_object = NULL;

	if (GTK_IS_ENTRY (widget)) {
		changed_object = G_OBJECT (widget);
		g_signal_connect_swapped (widget, "activate", G_CALLBACK (entry_activated), editor);
	}
	else if (GTK_IS_COMBO_BOX_ENTRY (widget)) {
		changed_object = G_OBJECT (/*gtk_bin_get_child (GTK_BIN*/ (widget)/*)*/);
		g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (widget)), "activate", G_CALLBACK (entry_activated), editor);
	}
	else if (GTK_IS_TEXT_VIEW (widget)) {
		changed_object = G_OBJECT (gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget)));
	}
	else if (E_IS_URL_ENTRY (widget)) {
		changed_object = G_OBJECT (e_url_entry_get_entry (E_URL_ENTRY (widget)));
		g_signal_connect_swapped (GTK_WIDGET (changed_object), "activate",
					  G_CALLBACK (entry_activated), editor);
	}
	else if (E_IS_DATE_EDIT (widget)) {
		changed_object = G_OBJECT (widget);
	}
	else if (E_IS_IMAGE_CHOOSER (widget)) {
		changed_object = G_OBJECT (widget);
		g_signal_connect (widget, "changed", G_CALLBACK (image_chooser_changed), editor);
	}
	else if (GTK_IS_TOGGLE_BUTTON (widget)) {
		g_signal_connect (widget, "toggled", G_CALLBACK (object_changed), editor);
	}

	if (changed_object)
		g_signal_connect (changed_object, "changed", G_CALLBACK (object_changed), editor);
}

static void
fill_in_simple_field (EContactEditor *editor, GtkWidget *widget, gint field_id)
{
	EContact *contact;

	contact = editor->contact;

	g_signal_handlers_block_matched (widget, G_SIGNAL_MATCH_DATA,
					 0, 0, NULL, NULL, editor);

	if (GTK_IS_ENTRY (widget)) {
		gchar *text = e_contact_get (contact, field_id);
		gtk_entry_set_text (GTK_ENTRY (widget), STRING_MAKE_NON_NULL (text));
		g_free (text);
	}
	else if (GTK_IS_COMBO_BOX_ENTRY (widget)) {
		gchar *text = e_contact_get (contact, field_id);
		gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))), STRING_MAKE_NON_NULL (text));
		g_free (text);
	}
	else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
		gchar         *text   = e_contact_get (contact, field_id);
		gtk_text_buffer_set_text (buffer, STRING_MAKE_NON_NULL (text), -1);
		g_free (text);
	}
	else if (E_IS_URL_ENTRY (widget)) {
		GtkWidget *entry = e_url_entry_get_entry (E_URL_ENTRY (widget));
		gchar     *text  = e_contact_get (contact, field_id);
		gtk_entry_set_text (GTK_ENTRY (entry), STRING_MAKE_NON_NULL (text));
		g_free (text);
	}
	else if (E_IS_DATE_EDIT (widget)) {
		EContactDate *date = e_contact_get (contact, field_id);
		if (date)
			e_date_edit_set_date (E_DATE_EDIT (widget),
					      date->year,
					      date->month,
					      date->day);
		else
			e_date_edit_set_time (E_DATE_EDIT (widget), -1);

		e_contact_date_free (date);
	}
	else if (E_IS_IMAGE_CHOOSER (widget)) {
		EContactPhoto *photo = e_contact_get (contact, field_id);
		if (photo && photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
			e_image_chooser_set_image_data (E_IMAGE_CHOOSER (widget),
							(gchar *)photo->data.inlined.data,
							photo->data.inlined.length);
			editor->image_set = TRUE;
		}
		else {
			gchar *file_name = e_icon_factory_get_icon_filename ("avatar-default", GTK_ICON_SIZE_DIALOG);
			e_image_chooser_set_from_file (E_IMAGE_CHOOSER (widget), file_name);
			editor->image_set = FALSE;
			g_free (file_name);
		}
		editor->image_changed = FALSE;
		e_contact_photo_free (photo);
	}
	else if (GTK_IS_TOGGLE_BUTTON (widget)) {
		gboolean val = e_contact_get (contact, field_id) != NULL;

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), val);
	}
	else {
		g_warning (G_STRLOC ": Unhandled widget class in mappings!");
	}

	g_signal_handlers_unblock_matched (widget, G_SIGNAL_MATCH_DATA,
					   0, 0, NULL, NULL, editor);
}

static void
extract_simple_field (EContactEditor *editor, GtkWidget *widget, gint field_id)
{
	EContact *contact;

	contact = editor->contact;

	if (GTK_IS_ENTRY (widget)) {
		const gchar *text = gtk_entry_get_text (GTK_ENTRY (widget));
		e_contact_set (contact, field_id, (gchar *) text);
	}
	else if (GTK_IS_COMBO_BOX_ENTRY (widget)) {
		gchar *text = gtk_combo_box_get_active_text (GTK_COMBO_BOX (widget));

		e_contact_set (contact, field_id, text);

		g_free (text);
	}
	else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
		GtkTextIter    start, end;
		gchar         *text;

		gtk_text_buffer_get_start_iter (buffer, &start);
		gtk_text_buffer_get_end_iter   (buffer, &end);
		text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
		e_contact_set (contact, field_id, text);
		g_free (text);
	}
	else if (E_IS_URL_ENTRY (widget)) {
		GtkWidget   *entry = e_url_entry_get_entry (E_URL_ENTRY (widget));
		const gchar *text  = gtk_entry_get_text (GTK_ENTRY (entry));
		e_contact_set (contact, field_id, (gchar *) text);
	}
	else if (E_IS_DATE_EDIT (widget)) {
		EContactDate date;
		if (e_date_edit_get_date (E_DATE_EDIT (widget),
					  (gint *)&date.year,
					  (gint *)&date.month,
					  (gint *)&date.day))
			e_contact_set (contact, field_id, &date);
		else
			e_contact_set (contact, field_id, NULL);
	}
	else if (E_IS_IMAGE_CHOOSER (widget)) {
		EContactPhoto photo;
		photo.type = E_CONTACT_PHOTO_TYPE_INLINED;
		photo.data.inlined.mime_type = NULL;
		if (editor->image_changed)
		{
			gchar *img_buff = NULL;
			if (editor->image_set &&
			    e_image_chooser_get_image_data (E_IMAGE_CHOOSER (widget),
							    &img_buff, &photo.data.inlined.length)) {
				GdkPixbuf *pixbuf, *new;
				GdkPixbufLoader *loader = gdk_pixbuf_loader_new();

				photo.data.inlined.data = (guchar *)img_buff;
				img_buff = NULL;
				gdk_pixbuf_loader_write (loader, photo.data.inlined.data, photo.data.inlined.length, NULL);
				gdk_pixbuf_loader_close (loader, NULL);

				pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
				if (pixbuf) {
					gint width, height, prompt_response;

					g_object_ref (pixbuf);

					height = gdk_pixbuf_get_height (pixbuf);
					width = gdk_pixbuf_get_width (pixbuf);
					if ((height > 96 || width > 96)) {

						prompt_response =
							e_alert_run_dialog_for_args
							(GTK_WINDOW (editor->app),
							 "addressbook:prompt-resize",
							 NULL);

						if (prompt_response == GTK_RESPONSE_YES) {
							if ( width > height) {
								height = height * 96 / width;
								width = 96;
							} else {
								width = width *96 / height;
								height = 96;
							}

							new = e_icon_factory_pixbuf_scale (pixbuf, width, height);
							if (new) {
								GdkPixbufFormat *format = gdk_pixbuf_loader_get_format (loader);
								gchar *format_name = gdk_pixbuf_format_get_name (format);
								g_free(photo.data.inlined.data);
								gdk_pixbuf_save_to_buffer (new, &img_buff,
											   &photo.data.inlined.length,
											   format_name, NULL, NULL);
								photo.data.inlined.data = (guchar *)img_buff;
								img_buff = NULL;
								g_free (format_name);
								g_object_unref (new);
							}
						}
						else if (prompt_response == GTK_RESPONSE_CANCEL) {
							g_object_unref (pixbuf);
							g_object_unref (loader);
							return;
						}
					}
					g_object_unref (pixbuf);
				}
				editor->image_changed = FALSE;
				g_object_unref (loader);

				e_contact_set (contact, field_id, &photo);

				g_free (photo.data.inlined.data);

			}
			else {
				editor->image_changed = FALSE;
				e_contact_set (contact, E_CONTACT_PHOTO, NULL);
			}
		}
	}
	else if (GTK_IS_TOGGLE_BUTTON (widget)) {
		gboolean val = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

		e_contact_set (contact, field_id, val?(gpointer)1:NULL);
	}
	else {
		g_warning (G_STRLOC ": Unhandled widget class in mappings!");
	}
}

static void
sensitize_simple_field (GtkWidget *widget, gboolean enabled)
{
	if (GTK_IS_ENTRY (widget))
		gtk_editable_set_editable (GTK_EDITABLE (widget), enabled);
	else if (GTK_IS_TEXT_VIEW (widget))
		gtk_text_view_set_editable (GTK_TEXT_VIEW (widget), enabled);
	else if (E_IS_URL_ENTRY (widget)) {
		GtkWidget *entry = e_url_entry_get_entry (E_URL_ENTRY (widget));
		gtk_editable_set_editable (GTK_EDITABLE (entry), enabled);
	}
	else if (E_IS_DATE_EDIT (widget))
		e_date_edit_set_editable (E_DATE_EDIT (widget), enabled);
	else
		gtk_widget_set_sensitive (widget, enabled);
}

static void
init_simple (EContactEditor *editor)
{
	GtkWidget *widget;
	gint       i;

	for (i = 0; i < G_N_ELEMENTS (simple_field_map); i++) {
		widget = e_builder_get_widget (editor->builder, simple_field_map[i].widget_name);
		if (!widget)
			continue;

		init_simple_field (editor, widget);
	}

	/* --- Special cases --- */

	/* Update file_as */

	widget = e_builder_get_widget (editor->builder, "entry-fullname");
	g_signal_connect (widget, "changed", G_CALLBACK (name_entry_changed), editor);
	widget = e_builder_get_widget (editor->builder, "combo-file-as");
	gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (widget), 0);
	g_signal_connect (widget, "changed", G_CALLBACK (file_as_combo_changed), editor);
	widget = e_builder_get_widget (editor->builder, "entry-company");
	g_signal_connect (widget, "changed", G_CALLBACK (company_entry_changed), editor);
}

static void
fill_in_simple (EContactEditor *editor)
{
	EContactName *name;
	gchar *filename;
	gint          i;

	for (i = 0; i < G_N_ELEMENTS (simple_field_map); i++) {
		GtkWidget *widget;

		if (simple_field_map[i].field_id < 0 ||
		    !simple_field_map[i].process_data)
			continue;

		widget = e_builder_get_widget (editor->builder, simple_field_map[i].widget_name);
		if (!widget)
			continue;

		fill_in_simple_field (editor, widget, simple_field_map[i].field_id);
	}

	/* --- Special cases --- */

	/* Update broken-up name */

	g_object_get (editor->contact,
		      "name", &name,
		      NULL);

	if (editor->name)
		e_contact_name_free (editor->name);

	editor->name = name;

	/* Update the contact editor title */

	filename = (gchar *) e_contact_get (editor->contact, E_CONTACT_FILE_AS);

	if (filename) {
		gchar *title;
		title = g_strdup_printf (_("Contact Editor - %s"), filename);
		gtk_window_set_title (GTK_WINDOW (editor->app), title);
		g_free (title);
	}
	else
		gtk_window_set_title (GTK_WINDOW (editor->app), _("Contact Editor"));

	/* Update file_as combo options */

	update_file_as_combo (editor);
}

static void
extract_simple (EContactEditor *editor)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (simple_field_map); i++) {
		GtkWidget *widget;

		if (simple_field_map[i].field_id < 0 ||
		    !simple_field_map[i].process_data)
			continue;

		widget = e_builder_get_widget (editor->builder, simple_field_map[i].widget_name);
		if (!widget)
			continue;

		extract_simple_field (editor, widget, simple_field_map[i].field_id);
	}

	/* Special cases */

	e_contact_set (editor->contact, E_CONTACT_NAME, editor->name);
}

static void
sensitize_simple (EContactEditor *editor)
{
	gint i;

	for (i = 0; i < G_N_ELEMENTS (simple_field_map); i++) {
		GtkWidget *widget;
		gboolean   enabled = TRUE;

		widget = e_builder_get_widget (editor->builder, simple_field_map[i].widget_name);
		if (!widget)
			continue;

		if (simple_field_map[i].field_id >= 0 &&
		    !is_field_supported (editor, simple_field_map[i].field_id))
			enabled = FALSE;

		if (simple_field_map[i].desensitize_for_read_only &&
		    !editor->target_editable)
			enabled = FALSE;

		sensitize_simple_field (widget, enabled);
	}
}

static void
fill_in_all (EContactEditor *editor)
{
	fill_in_source_field (editor);
	fill_in_simple       (editor);
	fill_in_email        (editor);
	fill_in_phone        (editor);
	fill_in_im           (editor);
	fill_in_address      (editor);
}

static void
extract_all (EContactEditor *editor)
{
	extract_simple  (editor);
	extract_email   (editor);
	extract_phone   (editor);
	extract_im      (editor);
	extract_address (editor);
}

static void
sensitize_all (EContactEditor *editor)
{
	sensitize_ok      (editor);
	sensitize_simple  (editor);
	sensitize_email   (editor);
	sensitize_phone   (editor);
	sensitize_im      (editor);
	sensitize_address (editor);
}

static void
init_all (EContactEditor *editor)
{
	GtkRequisition tab_req;

	init_simple   (editor);
	init_email    (editor);
	init_phone    (editor);
	init_im       (editor);
	init_personal (editor);
	init_address  (editor);

	/* with so many scrolled windows, we need to
	   do some manual sizing */
	gtk_widget_size_request (
		e_builder_get_widget (editor->builder, "vbox-size-leader"), &tab_req);
	gtk_widget_set_size_request (
		e_builder_get_widget (editor->builder, "scrolledwindow-size-leader"),
		tab_req.width, tab_req.height + 8);
}

static void
contact_editor_book_loaded_cb (ESource *source,
                               GAsyncResult *result,
                               EContactEditor *editor)
{
	EBook *book;
	GError *error = NULL;

	book = e_load_book_source_finish (source, result, &error);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (book == NULL);
		g_error_free (error);
		goto exit;

	} else if (error != NULL) {
		GtkWidget *source_combo_box;
		GtkWindow *parent;

		g_warn_if_fail (book == NULL);

		parent = eab_editor_get_window (EAB_EDITOR (editor));
		eab_load_error_dialog (GTK_WIDGET (parent), source, error);

		source_combo_box = e_builder_get_widget (
			editor->builder, "source-combo-box-source");
		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (source_combo_box), source);

		g_error_free (error);
		goto exit;
	}

	g_return_if_fail (E_IS_BOOK (book));

	/* FIXME Write a private contact_editor_set_target_book(). */
	g_object_set (editor, "target_book", book, NULL);

	g_object_unref (book);

exit:
	g_object_unref (editor);
}

static void
source_changed (ESourceComboBox *source_combo_box, EContactEditor *editor)
{
	ESource *source;
	GtkWindow *parent;

	source = e_source_combo_box_get_active (source_combo_box);
	parent = eab_editor_get_window (EAB_EDITOR (editor));

	if (editor->cancellable != NULL) {
		g_cancellable_cancel (editor->cancellable);
		g_object_unref (editor->cancellable);
		editor->cancellable = NULL;
	}

	if (e_source_equal (e_book_get_source (editor->target_book), source))
		return;

	if (e_source_equal (e_book_get_source (editor->source_book), source)) {
		g_object_set (editor, "target_book", editor->source_book, NULL);
		return;
	}

	editor->cancellable = g_cancellable_new ();

	e_load_book_source_async (
		source, parent, editor->cancellable,
		(GAsyncReadyCallback) contact_editor_book_loaded_cb,
		g_object_ref (editor));
}

static void
full_name_response (GtkDialog *dialog, gint response, EContactEditor *editor)
{
	EContactName *name;
	GtkWidget *fname_widget;
	gint style = 0;
	gboolean editable = FALSE;

	g_object_get (dialog,
		      "editable", &editable,
		      NULL);

	if (editable && response == GTK_RESPONSE_OK) {
		g_object_get (dialog,
			      "name", &name,
			      NULL);

		style = file_as_get_style(editor);

		fname_widget = e_builder_get_widget(editor->builder, "entry-fullname");
		if (fname_widget && GTK_IS_ENTRY (fname_widget)) {
			gchar *full_name = e_contact_name_to_string(name);
			const gchar *old_full_name = gtk_entry_get_text (GTK_ENTRY(fname_widget));

			if (strcmp (full_name, old_full_name))
				gtk_entry_set_text (GTK_ENTRY (fname_widget), full_name);
			g_free(full_name);
		}

		e_contact_name_free(editor->name);
		editor->name = name;

		file_as_set_style(editor, style);
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
	editor->fullname_dialog = NULL;
}

static gint
full_name_editor_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	if (widget) {
		if (GTK_IS_WIDGET (widget))
			gtk_widget_destroy (widget);
	}
	return TRUE;
}

static void
full_name_clicked (GtkWidget *button, EContactEditor *editor)
{
	GtkDialog *dialog;
	gboolean fullname_supported;

	if (editor->fullname_dialog) {
		gtk_window_present (GTK_WINDOW (editor->fullname_dialog));
		return;
	}

	dialog = GTK_DIALOG (e_contact_editor_fullname_new (editor->name));
	fullname_supported = is_field_supported (editor, E_CONTACT_FULL_NAME);

	g_object_set (dialog,
		      "editable", fullname_supported & editor->target_editable,
		      NULL);

	g_signal_connect(dialog, "response",
			G_CALLBACK (full_name_response), editor);

	/* Close the fullname dialog if the editor is closed */
	g_signal_connect_swapped (EAB_EDITOR (editor), "editor_closed",
			    G_CALLBACK (full_name_editor_delete_event_cb), GTK_WIDGET (dialog));

	gtk_widget_show (GTK_WIDGET (dialog));
	editor->fullname_dialog = GTK_WIDGET (dialog);
}

static void
categories_response (GtkDialog *dialog, gint response, EContactEditor *editor)
{
	const gchar *categories;
	GtkWidget *entry = e_builder_get_widget(editor->builder, "entry-categories");

	if (response == GTK_RESPONSE_OK) {
		categories = e_categories_dialog_get_categories (E_CATEGORIES_DIALOG (dialog));
		if (entry && GTK_IS_ENTRY(entry))
			gtk_entry_set_text (GTK_ENTRY (entry), categories);
		else
			e_contact_set (editor->contact, E_CONTACT_CATEGORIES, (gchar *)categories);
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
	editor->categories_dialog = NULL;
}

static gint
categories_editor_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	if (widget) {
		if (GTK_IS_WIDGET (widget))
			gtk_widget_destroy(widget);
		}
	return TRUE;
}

static void
categories_clicked (GtkWidget *button, EContactEditor *editor)
{
	gchar *categories = NULL;
	GtkDialog *dialog;
	GtkWidget *entry = e_builder_get_widget(editor->builder, "entry-categories");

	if (entry && GTK_IS_ENTRY(entry))
		categories = g_strdup (gtk_entry_get_text(GTK_ENTRY(entry)));
	else if (editor->contact)
		categories = e_contact_get (editor->contact, E_CONTACT_CATEGORIES);

	if (editor->categories_dialog != NULL) {
		gtk_window_present (GTK_WINDOW(editor->categories_dialog));
		g_free (categories);
		return;
	}else if (!(dialog = GTK_DIALOG (e_categories_dialog_new (categories)))) {
		e_alert_run_dialog_for_args (GTK_WINDOW (editor->app),
					     "addressbook:edit-categories",
					     NULL);
		g_free (categories);
		return;
	}

	g_signal_connect(dialog, "response",
			G_CALLBACK (categories_response), editor);

	/* Close the category dialog if the editor is closed*/
	g_signal_connect_swapped (EAB_EDITOR (editor), "editor_closed",
			    G_CALLBACK (categories_editor_delete_event_cb), GTK_WIDGET (dialog));

	gtk_widget_show(GTK_WIDGET(dialog));
	g_free (categories);

	editor->categories_dialog = GTK_WIDGET (dialog);
}

static void
image_selected (EContactEditor *editor)
{
	gchar     *file_name;
	GtkWidget *image_chooser;

	file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (editor->file_selector));

	if (!file_name)
		return;

	image_chooser = e_builder_get_widget (editor->builder, "image-chooser");

	g_signal_handlers_block_by_func (image_chooser, image_chooser_changed, editor);
	e_image_chooser_set_from_file (E_IMAGE_CHOOSER (image_chooser), file_name);
	g_signal_handlers_unblock_by_func (image_chooser, image_chooser_changed, editor);

	editor->image_set = TRUE;
	editor->image_changed = TRUE;
	object_changed (G_OBJECT (image_chooser), editor);
}

static void
image_cleared (EContactEditor *editor)
{
	GtkWidget *image_chooser;
	gchar     *file_name;

	image_chooser = e_builder_get_widget (editor->builder, "image-chooser");

	file_name = e_icon_factory_get_icon_filename ("avatar-default", GTK_ICON_SIZE_DIALOG);

	g_signal_handlers_block_by_func (image_chooser, image_chooser_changed, editor);
	e_image_chooser_set_from_file (E_IMAGE_CHOOSER (image_chooser), file_name);
	g_signal_handlers_unblock_by_func (image_chooser, image_chooser_changed, editor);

	g_free (file_name);

	editor->image_set = FALSE;
	editor->image_changed = TRUE;
	object_changed (G_OBJECT (image_chooser), editor);
}

static void
file_chooser_response (GtkWidget *widget, gint response, EContactEditor *editor)
{
	if (response == GTK_RESPONSE_ACCEPT)
		image_selected (editor);
	else if (response == GTK_RESPONSE_NO)
		image_cleared (editor);

	gtk_widget_hide (editor->file_selector);
}

static gboolean
file_selector_deleted (GtkWidget *widget)
{
	gtk_widget_hide (widget);
	return TRUE;
}

static void
update_preview_cb (GtkFileChooser *file_chooser, gpointer data)
{
	GtkWidget *preview;
	gchar *filename = NULL;
	GdkPixbuf *pixbuf;

	gtk_file_chooser_set_preview_widget_active (file_chooser, TRUE);
	preview = GTK_WIDGET (data);
	filename = gtk_file_chooser_get_preview_filename (file_chooser);
	if (filename == NULL)
		return;

	pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 128, 128, NULL);
	if (!pixbuf) {
		gchar *alternate_file;
		alternate_file = e_icon_factory_get_icon_filename ("avatar-default", GTK_ICON_SIZE_DIALOG);
		if (alternate_file) {
			pixbuf = gdk_pixbuf_new_from_file_at_size (alternate_file, 128, 128, NULL);
			g_free (alternate_file);
		}
	}
	g_free (filename);

	gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
	if (pixbuf)
		g_object_unref (pixbuf);
}

static void
image_clicked (GtkWidget *button, EContactEditor *editor)
{
	const gchar *title = _("Please select an image for this contact");
	const gchar *no_image = _("_No image");
	GtkImage *preview;

	if (!editor->file_selector) {
		editor->file_selector = gtk_file_chooser_dialog_new (title,
								     GTK_WINDOW (editor->app),
								     GTK_FILE_CHOOSER_ACTION_OPEN,
								     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
								     GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
								     no_image, GTK_RESPONSE_NO,
								     NULL);
		preview = GTK_IMAGE (gtk_image_new());
		gtk_file_chooser_set_preview_widget ((GtkFileChooser *)editor->file_selector, GTK_WIDGET (preview));
		g_signal_connect (editor->file_selector, "update-preview",
				 G_CALLBACK (update_preview_cb), preview);
		gtk_dialog_set_default_response (GTK_DIALOG (editor->file_selector), GTK_RESPONSE_ACCEPT);

		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (editor->file_selector), g_get_home_dir ());

		g_signal_connect (editor->file_selector, "response",
				  G_CALLBACK (file_chooser_response), editor);

		g_signal_connect_after (editor->file_selector,
					"delete-event", G_CALLBACK (file_selector_deleted),
					editor->file_selector);
	}

	/* Display the dialog */

	gtk_window_set_modal (GTK_WINDOW (editor->file_selector), TRUE);
	gtk_window_present (GTK_WINDOW (editor->file_selector));
}

typedef struct {
	EContactEditor *ce;
	gboolean should_close;
	gchar *new_id;
} EditorCloseStruct;

static void
contact_moved_cb (EBook *book, const GError *error, EditorCloseStruct *ecs)
{
	EContactEditor *ce = ecs->ce;
	gboolean should_close = ecs->should_close;

	gtk_widget_set_sensitive (ce->app, TRUE);
	ce->in_async_call = FALSE;

	e_contact_set (ce->contact, E_CONTACT_UID, ecs->new_id);

	eab_editor_contact_deleted (EAB_EDITOR (ce), error, ce->contact);

	ce->is_new_contact = FALSE;

	if (should_close) {
		eab_editor_close (EAB_EDITOR (ce));
	}
	else {
		ce->changed = FALSE;

		g_object_ref (ce->target_book);
		g_object_unref (ce->source_book);
		ce->source_book = ce->target_book;

		sensitize_all (ce);
	}

	g_object_unref (ce);
	g_free (ecs->new_id);
	g_free (ecs);
}

static void
contact_added_cb (EBook *book, const GError *error, const gchar *id, EditorCloseStruct *ecs)
{
	EContactEditor *ce = ecs->ce;
	gboolean should_close = ecs->should_close;

	if (ce->source_book != ce->target_book && e_book_is_writable (ce->source_book) &&
	    !error && ce->is_new_contact == FALSE) {
		ecs->new_id = g_strdup (id);
		e_book_remove_contact_async (ce->source_book, ce->contact,
					     (EBookAsyncCallback) contact_moved_cb, ecs);
		return;
	}

	gtk_widget_set_sensitive (ce->app, TRUE);
	ce->in_async_call = FALSE;

	e_contact_set (ce->contact, E_CONTACT_UID, (gchar *) id);

	eab_editor_contact_added (EAB_EDITOR (ce), error, ce->contact);

	if (!error) {
		ce->is_new_contact = FALSE;

		if (should_close) {
			eab_editor_close (EAB_EDITOR (ce));
		}
		else {
			ce->changed = FALSE;
			sensitize_all (ce);
		}
	}

	g_object_unref (ce);
	g_free (ecs);
}

static void
contact_modified_cb (EBook *book, const GError *error, EditorCloseStruct *ecs)
{
	EContactEditor *ce = ecs->ce;
	gboolean should_close = ecs->should_close;

	gtk_widget_set_sensitive (ce->app, TRUE);
	ce->in_async_call = FALSE;

	eab_editor_contact_modified (EAB_EDITOR (ce), error, ce->contact);

	if (!error) {
		if (should_close) {
			eab_editor_close (EAB_EDITOR (ce));
		}
		else {
			ce->changed = FALSE;
			sensitize_all (ce);
		}
	}

	g_object_unref (ce);
	g_free (ecs);
}

/* Emits the signal to request saving a contact */
static void
real_save_contact (EContactEditor *ce, gboolean should_close)
{
	EditorCloseStruct *ecs;

	ecs = g_new0 (EditorCloseStruct, 1);
	ecs->ce = ce;
	g_object_ref (ecs->ce);

	ecs->should_close = should_close;

	gtk_widget_set_sensitive (ce->app, FALSE);
	ce->in_async_call = TRUE;

	if (ce->source_book != ce->target_book) {
		/* Two-step move; add to target, then remove from source */
		eab_merging_book_add_contact (ce->target_book, ce->contact,
					      (EBookIdAsyncCallback) contact_added_cb, ecs);
	} else {
		if (ce->is_new_contact)
			eab_merging_book_add_contact (ce->target_book, ce->contact,
						      (EBookIdAsyncCallback) contact_added_cb, ecs);
		else
			eab_merging_book_commit_contact (ce->target_book, ce->contact,
							 (EBookAsyncCallback) contact_modified_cb, ecs);
	}
}

static void
save_contact (EContactEditor *ce, gboolean should_close)
{
	gchar *uid;
	const gchar *name_entry_string, *file_as_entry_string, *company_name_string;
	GtkWidget *entry_fullname, *entry_file_as, *company_name;

	if (!ce->target_book)
		return;

	if (ce->target_editable && !e_book_is_writable (ce->source_book)) {
		if (e_alert_run_dialog_for_args (GTK_WINDOW (ce->app),
						 "addressbook:prompt-move",
						 NULL) == GTK_RESPONSE_NO)
			return;
	}

	entry_fullname = e_builder_get_widget (ce->builder, "entry-fullname" );
	entry_file_as = gtk_bin_get_child (GTK_BIN (e_builder_get_widget (ce->builder, "combo-file-as")));
	company_name = e_builder_get_widget (ce->builder, "entry-company");
	name_entry_string = gtk_entry_get_text (GTK_ENTRY (entry_fullname));
	file_as_entry_string = gtk_entry_get_text (GTK_ENTRY (entry_file_as));
	company_name_string = gtk_entry_get_text (GTK_ENTRY (company_name));

	if (strcmp (company_name_string , "")) {
		if (!strcmp (name_entry_string, ""))
			gtk_entry_set_text (GTK_ENTRY (entry_fullname), company_name_string);
		if (!strcmp (file_as_entry_string, ""))
			gtk_entry_set_text (GTK_ENTRY (entry_file_as), company_name_string);
	}

	extract_all (ce);

	if (!e_contact_editor_is_valid (EAB_EDITOR (ce))) {
		uid = e_contact_get (ce->contact, E_CONTACT_UID);
		g_object_unref (ce->contact);
		ce->contact = e_contact_new ();
		if (uid) {
			e_contact_set (ce->contact, E_CONTACT_UID, uid);
			g_free (uid);
		}
		return;
	}

	real_save_contact (ce, should_close);
}

static void
e_contact_editor_save_contact (EABEditor *editor, gboolean should_close)
{
	save_contact (E_CONTACT_EDITOR (editor), should_close);
}

/* Closes the dialog box and emits the appropriate signals */
static void
e_contact_editor_close (EABEditor *editor)
{
	EContactEditor *ce = E_CONTACT_EDITOR (editor);

	if (ce->app != NULL) {
		gtk_widget_destroy (ce->app);
		ce->app = NULL;
		eab_editor_closed (editor);
	}
}

static const EContactField  non_string_fields[] = {
	E_CONTACT_FULL_NAME,
	E_CONTACT_ADDRESS,
	E_CONTACT_ADDRESS_HOME,
	E_CONTACT_ADDRESS_WORK,
	E_CONTACT_ADDRESS_OTHER,
	E_CONTACT_EMAIL,
	E_CONTACT_IM_AIM,
	E_CONTACT_IM_GROUPWISE,
	E_CONTACT_IM_JABBER,
	E_CONTACT_IM_YAHOO,
	E_CONTACT_IM_GADUGADU,
	E_CONTACT_IM_MSN,
	E_CONTACT_IM_ICQ,
	E_CONTACT_IM_SKYPE,
	E_CONTACT_PHOTO,
	E_CONTACT_LOGO,
	E_CONTACT_X509_CERT,
	E_CONTACT_CATEGORY_LIST,
	E_CONTACT_BIRTH_DATE,
	E_CONTACT_ANNIVERSARY

};

static gboolean
is_non_string_field (EContactField id)
{
	gint count = sizeof (non_string_fields) / sizeof (EContactField);
	gint i;
	for (i = 0; i < count; i++)
		if (id == non_string_fields[i])
			return TRUE;
	return FALSE;

}

/* insert checks here (date format, for instance, etc.) */
static gboolean
e_contact_editor_is_valid (EABEditor *editor)
{
	EContactEditor *ce = E_CONTACT_EDITOR (editor);
	GtkWidget *widget;
	gboolean validation_error = FALSE;
	EIterator *iter;
	GString *errmsg = g_string_new (_("The contact data is invalid:\n\n"));

	widget = e_builder_get_widget (ce->builder, "dateedit-birthday");
	if (!(e_date_edit_date_is_valid (E_DATE_EDIT (widget)))) {
		g_string_append_printf (errmsg, _("'%s' has an invalid format"),
					e_contact_pretty_name (E_CONTACT_BIRTH_DATE));
		validation_error = TRUE;
	}

	widget = e_builder_get_widget (ce->builder, "dateedit-anniversary");
	if (!(e_date_edit_date_is_valid (E_DATE_EDIT (widget)))) {
		g_string_append_printf (errmsg, _("%s'%s' has an invalid format"),
					validation_error ? ",\n" : "",
					e_contact_pretty_name (E_CONTACT_ANNIVERSARY));
		validation_error = TRUE;
	}

	iter = e_list_get_iterator (ce->required_fields);
	for (e_iterator_last (iter);
	     e_iterator_is_valid (iter);
	     e_iterator_prev (iter)) {
		const gchar *field_name = e_iterator_get (iter);
		EContactField  field_id = e_contact_field_id (field_name);

		if (is_non_string_field (field_id)) {
			if (e_contact_get_const (ce->contact, field_id) == NULL) {
				g_string_append_printf (errmsg, _("%s'%s' is empty"),
							validation_error ? ",\n" : "",
							e_contact_pretty_name (field_id));
				validation_error = TRUE;
				break;
			}

		} else {
			const gchar *text = e_contact_get_const (ce->contact, field_id);

			if (STRING_IS_EMPTY (text)) {
				g_string_append_printf (errmsg, _("%s'%s' is empty"),
							validation_error ? ",\n" : "",
							e_contact_pretty_name (field_id));
				validation_error = TRUE;
				break;
			}

		}
	}

	if (validation_error) {
		g_string_append (errmsg, ".");
		e_alert_run_dialog_for_args (GTK_WINDOW (ce->app),
					     "addressbook:generic-error",
					     _("Invalid contact."), errmsg->str,
					     NULL);
		g_string_free (errmsg, TRUE);
		return FALSE;
	}
	else {
		g_string_free (errmsg, TRUE);
		return TRUE;
	}
}

static gboolean
e_contact_editor_is_changed (EABEditor *editor)
{
	return E_CONTACT_EDITOR (editor)->changed;
}

static GtkWindow*
e_contact_editor_get_window (EABEditor *editor)
{
	return GTK_WINDOW (E_CONTACT_EDITOR (editor)->app);
}

static void
file_save_and_close_cb (GtkWidget *widget, gpointer data)
{
	EContactEditor *ce;

	ce = E_CONTACT_EDITOR (data);
	save_contact (ce, TRUE);
}

static void
file_cancel_cb (GtkWidget *widget, gpointer data)
{
	EContactEditor *ce = E_CONTACT_EDITOR (data);

	eab_editor_close (EAB_EDITOR (ce));
}

/* Callback used when the dialog box is destroyed */
static gint
app_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	EContactEditor *ce;

	ce = E_CONTACT_EDITOR (data);

	/* if we're saving, don't allow the dialog to close */
	if (ce->in_async_call)
		return TRUE;

	if (ce->changed) {
		switch (eab_prompt_save_dialog (GTK_WINDOW (ce->app))) {
			case GTK_RESPONSE_YES:
				eab_editor_save_contact (EAB_EDITOR (ce), TRUE);
				return TRUE;

			case GTK_RESPONSE_NO:
				break;

			case GTK_RESPONSE_CANCEL:
			default:
				return TRUE;

		}
	}

	eab_editor_close (EAB_EDITOR (ce));
	return TRUE;
}

static void
show_help_cb (GtkWidget *widget, gpointer data)
{
	/* FIXME Pass a proper parent window. */
	e_display_help (NULL, "usage-contact-cards");
}

static GList *
add_to_tab_order(GList *list, GtkBuilder *builder, const gchar *name)
{
	GtkWidget *widget = e_builder_get_widget(builder, name);
	return g_list_prepend(list, widget);
}

static void
setup_tab_order(GtkBuilder *builder)
{
	GtkWidget *container;
	GList *list = NULL;
/*
	container = e_builder_get_widget(builder, "table-contact-editor-general");

	if (container) {
		list = add_to_tab_order(list, builder, "entry-fullname");
		list = add_to_tab_order(list, builder, "entry-jobtitle");
		list = add_to_tab_order(list, builder, "entry-company");
		list = add_to_tab_order(list, builder, "combo-file-as");
		list = add_to_tab_order(list, builder, "entry-phone-1");
		list = add_to_tab_order(list, builder, "entry-phone-2");
		list = add_to_tab_order(list, builder, "entry-phone-3");
		list = add_to_tab_order(list, builder, "entry-phone-4");

		list = add_to_tab_order(list, builder, "entry-email1");
		list = add_to_tab_order(list, builder, "alignment-htmlmail");
		list = add_to_tab_order(list, builder, "entry-web");
		list = add_to_tab_order(list, builder, "entry-homepage");
		list = add_to_tab_order(list, builder, "button-fulladdr");
		list = add_to_tab_order(list, builder, "text-address");
		list = g_list_reverse(list);
		e_container_change_tab_order(GTK_CONTAINER(container), list);
		g_list_free(list);
	}
*/

	container = e_builder_get_widget (builder, "table-home-address");
	gtk_container_get_focus_chain (GTK_CONTAINER (container), &list);

	list = add_to_tab_order (list, builder, "scrolledwindow-home-address");
	list = add_to_tab_order (list, builder, "entry-home-city");
	list = add_to_tab_order (list, builder, "entry-home-zip");
	list = add_to_tab_order (list, builder, "entry-home-state");
	list = add_to_tab_order (list, builder, "entry-home-pobox");
	list = add_to_tab_order (list, builder, "entry-home-country");
	list = g_list_reverse (list);

	gtk_container_set_focus_chain (GTK_CONTAINER (container), list);
	g_list_free (list);

	container = e_builder_get_widget (builder, "table-work-address");
	gtk_container_get_focus_chain (GTK_CONTAINER (container), &list);

	list = add_to_tab_order (list, builder, "scrolledwindow-work-address");
	list = add_to_tab_order (list, builder, "entry-work-city");
	list = add_to_tab_order (list, builder, "entry-work-zip");
	list = add_to_tab_order (list, builder, "entry-work-state");
	list = add_to_tab_order (list, builder, "entry-work-pobox");
	list = add_to_tab_order (list, builder, "entry-work-country");
	list = g_list_reverse (list);

	gtk_container_set_focus_chain (GTK_CONTAINER (container), list);
	g_list_free (list);

	container = e_builder_get_widget (builder, "table-other-address");
	gtk_container_get_focus_chain (GTK_CONTAINER (container), &list);

	list = add_to_tab_order (list, builder, "scrolledwindow-other-address");
	list = add_to_tab_order (list, builder, "entry-other-city");
	list = add_to_tab_order (list, builder, "entry-other-zip");
	list = add_to_tab_order (list, builder, "entry-other-state");
	list = add_to_tab_order (list, builder, "entry-other-pobox");
	list = add_to_tab_order (list, builder, "entry-other-country");
	list = g_list_reverse (list);

	gtk_container_set_focus_chain (GTK_CONTAINER (container), list);
	g_list_free (list);
}

static void
expand_web_toggle (EContactEditor *ce)
{
	GtkWidget *widget;

	widget = e_builder_get_widget (ce->builder, "label-videourl");
	expand_web (ce, !gtk_widget_get_visible (widget));
}

static void
expand_phone_toggle (EContactEditor *ce)
{
	GtkWidget *phone_ext_table;

	phone_ext_table = e_builder_get_widget (ce->builder, "table-phone-extended");
	expand_phone (ce, !gtk_widget_get_visible (phone_ext_table));
}

static void
expand_mail_toggle (EContactEditor *ce)
{
	GtkWidget *mail;

	mail = e_builder_get_widget (ce->builder, "entry-email-4");
	expand_mail (ce, !gtk_widget_get_visible (mail));
}

static void
e_contact_editor_init (EContactEditor *e_contact_editor)
{
	GtkBuilder *builder;
	EShell *shell;
	GtkWidget *container;
	GtkWidget *widget, *label;
	GtkEntryCompletion *completion;

	/* FIXME The shell should be obtained
	 *       through a constructor property. */
	shell = e_shell_get_default ();

	e_contact_editor->name = e_contact_name_new();

	e_contact_editor->contact = NULL;
	e_contact_editor->changed = FALSE;
	e_contact_editor->image_set = FALSE;
	e_contact_editor->image_changed = FALSE;
	e_contact_editor->in_async_call = FALSE;
	e_contact_editor->target_editable = TRUE;
	e_contact_editor->fullname_dialog = NULL;
	e_contact_editor->categories_dialog = NULL;
	e_contact_editor->compress_ui = e_shell_get_express_mode (shell);

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "contact-editor.ui");

	e_contact_editor->builder = builder;

	setup_tab_order(builder);

	e_contact_editor->app = e_builder_get_widget (builder, "contact editor");
	widget = e_contact_editor->app;

	gtk_widget_ensure_style (widget);
	gtk_window_set_type_hint (GTK_WINDOW (widget), GDK_WINDOW_TYPE_HINT_NORMAL);
	container = gtk_dialog_get_action_area (GTK_DIALOG (widget));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);
	container = gtk_dialog_get_content_area (GTK_DIALOG (widget));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	init_all (e_contact_editor);

	widget = e_builder_get_widget (e_contact_editor->builder, "button-image");
	g_signal_connect (widget, "clicked", G_CALLBACK (image_clicked), e_contact_editor);

	widget = e_builder_get_widget(e_contact_editor->builder, "button-fullname");
	g_signal_connect (widget, "clicked", G_CALLBACK (full_name_clicked), e_contact_editor);
	widget = e_builder_get_widget(e_contact_editor->builder, "button-categories");
	g_signal_connect (widget, "clicked", G_CALLBACK (categories_clicked), e_contact_editor);
	widget = e_builder_get_widget (e_contact_editor->builder, "source-combo-box-source");
	e_util_set_source_combo_box_list (widget, "/apps/evolution/addressbook/sources");
	g_signal_connect (widget, "changed", G_CALLBACK (source_changed), e_contact_editor);
	label = e_builder_get_widget (e_contact_editor->builder, "where-label");
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	widget = e_builder_get_widget (e_contact_editor->builder, "button-ok");
	g_signal_connect (widget, "clicked", G_CALLBACK (file_save_and_close_cb), e_contact_editor);
	widget = e_builder_get_widget (e_contact_editor->builder, "button-cancel");
	g_signal_connect (widget, "clicked", G_CALLBACK (file_cancel_cb), e_contact_editor);
	widget = e_builder_get_widget (e_contact_editor->builder, "button-help");
	g_signal_connect (widget, "clicked", G_CALLBACK (show_help_cb), e_contact_editor);
	widget = e_builder_get_widget (e_contact_editor->builder, "button-web-expand");
	g_signal_connect_swapped (widget, "clicked", G_CALLBACK (expand_web_toggle), e_contact_editor);
	widget = e_builder_get_widget (e_contact_editor->builder, "button-phone-expand");
	g_signal_connect_swapped (widget, "clicked", G_CALLBACK (expand_phone_toggle), e_contact_editor);
	widget = e_builder_get_widget (e_contact_editor->builder, "button-mail-expand");
	g_signal_connect_swapped (widget, "clicked", G_CALLBACK (expand_mail_toggle), e_contact_editor);

	widget = e_builder_get_widget (e_contact_editor->builder, "entry-fullname");
	if (widget)
		gtk_widget_grab_focus (widget);

	widget = e_builder_get_widget (e_contact_editor->builder, "entry-categories");
	completion = e_category_completion_new ();
	gtk_entry_set_completion (GTK_ENTRY (widget), completion);
	g_object_unref (completion);

	/* Connect to the deletion of the dialog */

	g_signal_connect (e_contact_editor->app, "delete_event",
			    G_CALLBACK (app_delete_event_cb), e_contact_editor);

	/* set the icon */
	gtk_window_set_icon_name (GTK_WINDOW (e_contact_editor->app), "contact-editor");

	/* show window */
	gtk_widget_show (e_contact_editor->app);

	e_shell_watch_window (shell, GTK_WINDOW (e_contact_editor->app));
}

static void
e_contact_editor_dispose (GObject *object)
{
	EContactEditor *e_contact_editor = E_CONTACT_EDITOR(object);

	if (e_contact_editor->file_selector != NULL) {
		gtk_widget_destroy (e_contact_editor->file_selector);
		e_contact_editor->file_selector = NULL;
	}

	if (e_contact_editor->writable_fields) {
		g_object_unref(e_contact_editor->writable_fields);
		e_contact_editor->writable_fields = NULL;
	}
	if (e_contact_editor->required_fields) {
		g_object_unref (e_contact_editor->required_fields);
		e_contact_editor->required_fields = NULL;
	}
	if (e_contact_editor->contact) {
		g_object_unref(e_contact_editor->contact);
		e_contact_editor->contact = NULL;
	}

	if (e_contact_editor->source_book) {
		g_object_unref(e_contact_editor->source_book);
		e_contact_editor->source_book = NULL;
	}

	if (e_contact_editor->target_book) {
		g_signal_handler_disconnect (e_contact_editor->target_book, e_contact_editor->target_editable_id);
		g_object_unref(e_contact_editor->target_book);
		e_contact_editor->target_book = NULL;
	}

	if (e_contact_editor->name) {
		e_contact_name_free(e_contact_editor->name);
		e_contact_editor->name = NULL;
	}

	if (e_contact_editor->builder) {
		g_object_unref(e_contact_editor->builder);
		e_contact_editor->builder = NULL;
	}

	if (e_contact_editor->cancellable != NULL) {
		g_object_unref (e_contact_editor->cancellable);
		e_contact_editor->cancellable = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
supported_fields_cb (EBook *book, const GError *error,
		     EList *fields, EContactEditor *ce)
{
	if (!g_slist_find (eab_editor_get_all_editors (), ce)) {
		g_warning ("supported_fields_cb called for book that's still around, but contact editor that's been destroyed.");
		return;
	}

	g_object_set (ce,
		      "writable_fields", fields,
		      NULL);

	eab_editor_show (EAB_EDITOR (ce));

	sensitize_all (ce);
}

static void
required_fields_cb (EBook *book, const GError *error,
		    EList *fields, EContactEditor *ce)
{

	if (!g_slist_find (eab_editor_get_all_editors (), ce)) {
		g_warning ("supported_fields_cb called for book that's still around, but contact editor that's been destroyed.");
		return;
	}

	g_object_set (ce,
		      "required_fields", fields,
		      NULL);

}

EABEditor *
e_contact_editor_new (EShell *shell,
                      EBook *book,
                      EContact *contact,
                      gboolean is_new_contact,
                      gboolean editable)
{
	EABEditor *editor;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (E_IS_BOOK (book), NULL);
	g_return_val_if_fail (E_IS_CONTACT (contact), NULL);

	editor = g_object_new (E_TYPE_CONTACT_EDITOR, "shell", shell, NULL);

	g_object_set (editor,
		      "source_book", book,
		      "contact", contact,
		      "is_new_contact", is_new_contact,
		      "editable", editable,
		      NULL);

	if (book)
		e_book_get_supported_fields_async (
			book, (EBookEListAsyncCallback)supported_fields_cb, editor);

	return editor;
}

static void
writable_changed (EBook *book, gboolean writable, EContactEditor *ce)
{
	gint new_target_editable;
	gboolean changed = FALSE;

	new_target_editable = e_book_is_writable (ce->target_book);

	if (ce->target_editable != new_target_editable)
		changed = TRUE;

	ce->target_editable = new_target_editable;

	if (changed)
		sensitize_all (ce);
}

static void
e_contact_editor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	EContactEditor *editor;

	editor = E_CONTACT_EDITOR (object);

	switch (prop_id) {
	case PROP_SOURCE_BOOK: {
		gboolean  writable;
		gboolean  changed = FALSE;
		EBook    *source_book;

		source_book = E_BOOK (g_value_get_object (value));

		if (source_book == editor->source_book)
			break;

		if (editor->source_book)
			g_object_unref(editor->source_book);

		editor->source_book = source_book;
		g_object_ref (editor->source_book);

		if (!editor->target_book) {
			editor->target_book = editor->source_book;
			g_object_ref (editor->target_book);

			editor->target_editable_id = g_signal_connect (editor->target_book, "writable_status",
								       G_CALLBACK (writable_changed), editor);

			e_book_get_supported_fields_async (editor->target_book,
							   (EBookEListAsyncCallback) supported_fields_cb, editor);
			e_book_get_required_fields_async (editor->target_book,
							  (EBookEListAsyncCallback)  required_fields_cb, editor);
		}

		writable = e_book_is_writable (editor->target_book);
		if (writable != editor->target_editable) {
			editor->target_editable = writable;
			changed = TRUE;
		}

		if (changed)
			sensitize_all (editor);

		break;
	}

	case PROP_TARGET_BOOK: {
		gboolean  writable;
		gboolean  changed = FALSE;
		EBook    *target_book;

		target_book = E_BOOK (g_value_get_object (value));

		if (target_book == editor->target_book)
			break;

		if (editor->target_book) {
			g_signal_handler_disconnect (editor->target_book, editor->target_editable_id);
			g_object_unref(editor->target_book);
		}

		editor->target_book = target_book;
		g_object_ref (editor->target_book);

		editor->target_editable_id = g_signal_connect (editor->target_book, "writable_status",
							       G_CALLBACK (writable_changed), editor);

		e_book_get_supported_fields_async (editor->target_book,
						   (EBookEListAsyncCallback) supported_fields_cb, editor);

		e_book_get_required_fields_async (editor->target_book,
						  (EBookEListAsyncCallback)  required_fields_cb, editor);
		if (!editor->is_new_contact)
			editor->changed = TRUE;

		writable = e_book_is_writable (editor->target_book);

		if (writable != editor->target_editable) {
			editor->target_editable = writable;
			changed = TRUE;
		}

		if (changed)
			sensitize_all (editor);

		break;
	}

	case PROP_CONTACT:
		if (editor->contact)
			g_object_unref(editor->contact);
		editor->contact = e_contact_duplicate(E_CONTACT(g_value_get_object (value)));
		fill_in_all (editor);
		editor->changed = FALSE;
		break;

	case PROP_IS_NEW_CONTACT:
		editor->is_new_contact = g_value_get_boolean (value) ? TRUE : FALSE;
		break;

	case PROP_EDITABLE: {
		gboolean new_value = g_value_get_boolean (value) ? TRUE : FALSE;
		gboolean changed = (editor->target_editable != new_value);

		editor->target_editable = new_value;

		if (changed)
			sensitize_all (editor);
		break;
	}

	case PROP_CHANGED: {
		gboolean new_value = g_value_get_boolean (value) ? TRUE : FALSE;
		gboolean changed = (editor->changed != new_value);

		editor->changed = new_value;

		if (changed)
			sensitize_ok (editor);
		break;
	}
	case PROP_WRITABLE_FIELDS:
		if (editor->writable_fields)
			g_object_unref(editor->writable_fields);

		editor->writable_fields = g_value_get_object (value);
		if (editor->writable_fields)
			g_object_ref (editor->writable_fields);
		else
			editor->writable_fields = e_list_new(NULL, NULL, NULL);

		sensitize_all (editor);
		break;
	case PROP_REQUIRED_FIELDS:
		if (editor->required_fields)
			g_object_unref (editor->required_fields);
		editor->required_fields = g_value_get_object (value);
		if (editor->required_fields)
			g_object_ref (editor->required_fields);
		else
			editor->required_fields = e_list_new (NULL, NULL, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_contact_editor_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	EContactEditor *e_contact_editor;

	e_contact_editor = E_CONTACT_EDITOR (object);

	switch (prop_id) {
	case PROP_SOURCE_BOOK:
		g_value_set_object (value, e_contact_editor->source_book);
		break;

	case PROP_TARGET_BOOK:
		g_value_set_object (value, e_contact_editor->target_book);
		break;

	case PROP_CONTACT:
		extract_all (e_contact_editor);
		g_value_set_object (value, e_contact_editor->contact);
		break;

	case PROP_IS_NEW_CONTACT:
		g_value_set_boolean (value, e_contact_editor->is_new_contact ? TRUE : FALSE);
		break;

	case PROP_EDITABLE:
		g_value_set_boolean (value, e_contact_editor->target_editable ? TRUE : FALSE);
		break;

	case PROP_CHANGED:
		g_value_set_boolean (value, e_contact_editor->changed ? TRUE : FALSE);
		break;

	case PROP_WRITABLE_FIELDS:
		if (e_contact_editor->writable_fields)
			g_value_set_object (value, e_list_duplicate (e_contact_editor->writable_fields));
		else
			g_value_set_object (value, NULL);
		break;
	case PROP_REQUIRED_FIELDS:
		if (e_contact_editor->required_fields)
			g_value_set_object (value, e_list_duplicate (e_contact_editor->required_fields));
		else
			g_value_set_object (value, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * e_contact_editor_raise:
 * @config: The %EContactEditor object.
 *
 * Raises the dialog associated with this %EContactEditor object.
 */
static void
e_contact_editor_raise (EABEditor *editor)
{
	EContactEditor *ce = E_CONTACT_EDITOR (editor);
	GdkWindow *window;

	window = gtk_widget_get_window (ce->app);

	if (window != NULL)
		gdk_window_raise (window);
}

/**
 * e_contact_editor_show:
 * @ce: The %EContactEditor object.
 *
 * Shows the dialog associated with this %EContactEditor object.
 */
static void
e_contact_editor_show (EABEditor *editor)
{
	EContactEditor *ce = E_CONTACT_EDITOR (editor);
	gtk_widget_show (ce->app);
}
