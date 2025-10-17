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
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "eab-editor.h"
#include "e-contact-editor.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libedataserverui/libedataserverui.h>

#include "shell/e-shell.h"
#include "e-util/e-util.h"

#include "addressbook/printing/e-contact-print.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/util/eab-book-util.h"

#include "eab-contact-merging.h"

#include "e-contact-editor-fullname.h"
#include "e-contact-editor-dyntable.h"

#ifdef ENABLE_SMIME
#include "smime/lib/e-cert.h"
#endif

#define SLOTS_PER_LINE 2
#define SLOTS_IN_COLLAPSED_STATE SLOTS_PER_LINE
#define EMAIL_SLOTS   50
#define PHONE_SLOTS   50
#define SIP_SLOTS     4
#define IM_SLOTS      50
#define ADDRESS_SLOTS 3

/* represents index in address_name */
#define ADDRESS_SLOT_HOME  1
#define ADDRESS_SLOT_WORK  0
#define ADDRESS_SLOT_OTHER 2

#define CHECK_PHONE 	1
#define CHECK_SIP 	2
#define CHECK_IM	3
#define CHECK_HOME	4
#define CHECK_WORK	5
#define CHECK_OTHER	6
#define CHECK_WEB	7
#define CHECK_JOB	8
#define CHECK_DATES	9
#define CHECK_MISC	10
#define CHECK_NOTE	11
#define CHECK_CERTS	12

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

typedef struct {
	GWeakRef *editor_weak_ref; /* EContactEditor * */
	ESource *source;
} ConnectClosure;

static void	e_contact_editor_set_property	(GObject *object,
						 guint property_id,
						 const GValue *value,
						 GParamSpec *pspec);
static void	e_contact_editor_get_property	(GObject *object,
						 guint property_id,
						 GValue *value,
						 GParamSpec *pspec);
static void	e_contact_editor_constructed	(GObject *object);
static void	e_contact_editor_dispose	(GObject *object);
static void	e_contact_editor_raise		(EABEditor *editor);
static void	e_contact_editor_show		(EABEditor *editor);
static void	e_contact_editor_save_contact	(EABEditor *editor,
						 gboolean should_close);
static void	e_contact_editor_close		(EABEditor *editor);
static gboolean	e_contact_editor_is_valid	(EABEditor *editor);
static gboolean	e_contact_editor_is_changed	(EABEditor *editor);
static GtkWindow *
		e_contact_editor_get_window	(EABEditor *editor);
static void	save_contact			(EContactEditor *ce,
						 gboolean should_close);
static void	entry_activated			(EContactEditor *editor);

static void	set_entry_text			(EContactEditor *editor,
						 GtkEntry *entry,
						 const gchar *string);
static void	sensitize_ok			(EContactEditor *ce);

enum {
	PROP_0,
	PROP_SOURCE_CLIENT,
	PROP_TARGET_CLIENT,
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

/* Defaults selected from eab_phone_types */
static const gint phones_default[] = { 1, 9, 6, 2, 7, 12, 10, 10 };

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

/* Defaults selected from eab_get_im_type_labels */
static const gint im_service_default[] = { 0, 2, 4, 5 };


/* Default from the table above */
static const gint email_default[] = { 0, 1, 2, 2 };
static const gint sips_default[] = { 0, 1, 2, 2 };

#define STRING_IS_EMPTY(x)      (!(x) || !(*(x)))
#define STRING_MAKE_NON_NULL(x) ((x) ? (x) : "")

struct _EContactEditorPrivate
{
	/* item specific fields */
	EBookClient *source_client;
	EBookClient *target_client;
	EContact *contact;

	GtkBuilder *builder;
	GtkWidget *app;

	GtkWidget *image_selector;
	GtkFileChooserNative *image_selector_native;

	EContactName *name;

	/* Whether we are editing a new contact or an existing one */
	guint is_new_contact : 1;

	/* Whether an image is associated with a contact. */
	guint image_set : 1;

	/* Whether the contact has been changed since bringing up the contact editor */
	guint changed : 1;

	/* Wheter should check for contact to merge. Only when name or email are changed */
	guint check_merge : 1;

	/* Whether the contact editor will accept modifications, save */
	guint target_editable : 1;

	/* Whether an async wombat call is in progress */
	guint in_async_call : 1;

	/* Whether an image is changed */
	guint image_changed : 1;

	/* Whether to try to reduce space used */
	guint compress_ui : 1;

	GSList *writable_fields;

	GSList *required_fields;

	GCancellable *cancellable;

	/* signal ids for "writable_status" */
	gint target_editable_id;

	GtkWidget *fullname_dialog;
	GtkWidget *categories_dialog;

	EUIManager *ui_manager;
	EFocusTracker *focus_tracker;
};

G_DEFINE_TYPE_WITH_PRIVATE (EContactEditor, e_contact_editor, EAB_TYPE_EDITOR)

static void
connect_closure_free (ConnectClosure *connect_closure)
{
	e_weak_ref_free (connect_closure->editor_weak_ref);
	g_clear_object (&connect_closure->source);
	g_slice_free (ConnectClosure, connect_closure);
}

static void
e_contact_editor_contact_added (EABEditor *editor,
                                const GError *error,
                                EContact *contact)
{
	GtkWindow *window;
	const gchar *message;

	if (error == NULL)
		return;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	window = eab_editor_get_window (editor);
	message = _("Error adding contact");

	eab_error_dialog (NULL, window, message, error);
}

static void
e_contact_editor_contact_modified (EABEditor *editor,
                                   const GError *error,
                                   EContact *contact)
{
	GtkWindow *window;
	const gchar *message;

	if (error == NULL)
		return;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	window = eab_editor_get_window (editor);
	message = _("Error modifying contact");

	eab_error_dialog (NULL, window, message, error);
}

static void
e_contact_editor_contact_deleted (EABEditor *editor,
                                  const GError *error,
                                  EContact *contact)
{
	GtkWindow *window;
	const gchar *message;

	if (error == NULL)
		return;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		return;

	window = eab_editor_get_window (editor);
	message = _("Error removing contact");

	eab_error_dialog (NULL, window, message, error);
}

static void
e_contact_editor_closed (EABEditor *editor)
{
	g_object_unref (editor);
}

static void
e_contact_editor_class_init (EContactEditorClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	EABEditorClass *editor_class = EAB_EDITOR_CLASS (class);

	object_class->set_property = e_contact_editor_set_property;
	object_class->get_property = e_contact_editor_get_property;
	object_class->constructed = e_contact_editor_constructed;
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

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_CLIENT,
		g_param_spec_object (
			"source_client",
			"Source EBookClient",
			NULL,
			E_TYPE_BOOK_CLIENT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_TARGET_CLIENT,
		g_param_spec_object (
			"target_client",
			"Target EBookClient",
			NULL,
			E_TYPE_BOOK_CLIENT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CONTACT,
		g_param_spec_object (
			"contact",
			"Contact",
			NULL,
			E_TYPE_CONTACT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_IS_NEW_CONTACT,
		g_param_spec_boolean (
			"is_new_contact",
			"Is New Contact",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_WRITABLE_FIELDS,
		g_param_spec_pointer (
			"writable_fields",
			"Writable Fields",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_REQUIRED_FIELDS,
		g_param_spec_pointer (
			"required_fields",
			"Required Fields",
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CHANGED,
		g_param_spec_boolean (
			"changed",
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
is_field_supported (EContactEditor *editor,
                    EContactField field_id)
{
	GSList      *fields, *iter;
	const gchar *field;

	fields = editor->priv->writable_fields;
	if (!fields)
		return FALSE;

	field = e_contact_field_name (field_id);
	if (!field)
		return FALSE;

	for (iter = fields; iter; iter = iter->next) {
		const gchar *this_field = iter->data;

		if (!this_field)
			continue;

		if (!strcmp (field, this_field))
			return TRUE;
	}

	return FALSE;
}

/* This function tells you whether name_to_style will make sense.  */
static gboolean
style_makes_sense (const EContactName *name,
                   const gchar *company,
                   gint style)
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
		return FALSE;
	case 3:
		if (company && *company)
			return TRUE;
		else
			return FALSE;
	case 4: /* Fall Through */
	case 5:
		if (company && *company && name &&
			((name->given && *name->given) ||
			 (name->family && *name->family)))
			return TRUE;
		else
			return FALSE;
	default:
		return FALSE;
	}
}

static gchar *
name_to_style (const EContactName *name,
               const gchar *company,
               gint style)
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
		string = g_strjoinv (", ", strings);
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
		string = g_strjoinv (" ", strings);
		break;
	case 2:
		midstrptr = midstring;
		if (name) {
			if (name->family && *name->family)
				*(midstrptr++) = name->family;
			if (name->given && *name->given)
				*(midstrptr++) = name->given;
		}
		*midstrptr = NULL;
		stringptr = strings;
		substring = g_strjoinv (", ", midstring);
		*(stringptr++) = substring;
		if (name) {
			if (name->additional && *name->additional)
				*(stringptr++) = name->additional;
		}
		*stringptr = NULL;
		string = g_strjoinv (" ", strings);
		g_free (substring);
		break;
	case 3:
		string = g_strdup (company);
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
		substring = g_strjoinv (", ", strings);
		if (!(company && *company))
			company = "";
		if (style == 4)
			string = g_strdup_printf ("%s (%s)", substring, company);
		else
			string = g_strdup_printf ("%s (%s)", company, substring);
		g_free (substring);
		break;
	default:
		string = g_strdup ("");
	}
	return string;
}

static gint
file_as_get_style (EContactEditor *editor)
{
	GtkEntry *file_as = GTK_ENTRY (
		gtk_bin_get_child (GTK_BIN (
		e_builder_get_widget (editor->priv->builder, "combo-file-as"))));
	GtkEntry *company_w = GTK_ENTRY (
		e_builder_get_widget (editor->priv->builder, "entry-company"));
	const gchar *filestring;
	gchar *trystring;
	EContactName *name = editor->priv->name;
	const gchar *company;
	gint i;

	if (!(file_as && GTK_IS_ENTRY (file_as)))
		return -1;

	company = gtk_entry_get_text (GTK_ENTRY (company_w));
	filestring = gtk_entry_get_text (file_as);

	for (i = 0; i < 6; i++) {
		trystring = name_to_style (name, company, i);
		if (!strcmp (trystring, filestring)) {
			g_free (trystring);
			return i;
		}
		g_free (trystring);
	}
	return -1;
}

static void
file_as_set_style (EContactEditor *editor,
                   gint style)
{
	gchar *string;
	gint i;
	GList *strings = NULL;
	GtkComboBox *combo_file_as = GTK_COMBO_BOX (
		e_builder_get_widget (editor->priv->builder, "combo-file-as"));
	GtkEntry *company_w = GTK_ENTRY (
		e_builder_get_widget (editor->priv->builder, "entry-company"));
	const gchar *company;

	if (!(combo_file_as && GTK_IS_COMBO_BOX (combo_file_as)))
		return;

	company = gtk_entry_get_text (GTK_ENTRY (company_w));

	if (style == -1) {
		GtkWidget *entry;

		entry = gtk_bin_get_child (GTK_BIN (combo_file_as));
		if (entry) {
			string = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
			strings = g_list_append (strings, string);
		}
	}

	for (i = 0; i < 6; i++) {
		if (style_makes_sense (editor->priv->name, company, i)) {
			gchar *u;
			u = name_to_style (editor->priv->name, company, i);
			if (!STRING_IS_EMPTY (u))
				strings = g_list_append (strings, u);
			else
				g_free (u);
		}
	}

	if (combo_file_as) {
		GList *l;
		GtkListStore *list_store;
		GtkTreeIter iter;

		list_store = GTK_LIST_STORE (
			gtk_combo_box_get_model (combo_file_as));

		gtk_list_store_clear (list_store);

		for (l = strings; l; l = l->next) {
			gtk_list_store_append (list_store, &iter);
			gtk_list_store_set (list_store, &iter, 0, l->data, -1);
		}
	}

	g_list_foreach (strings, (GFunc) g_free, NULL);
	g_list_free (strings);

	if (style != -1) {
		string = name_to_style (editor->priv->name, company, style);
		set_entry_text (
			editor, GTK_ENTRY (gtk_bin_get_child (
			GTK_BIN (combo_file_as))), string);
		g_free (string);
	}
}

static void
name_entry_changed (GtkWidget *widget,
                    EContactEditor *editor)
{
	gint style = 0;
	const gchar *string;

	style = file_as_get_style (editor);
	e_contact_name_free (editor->priv->name);
	string = gtk_entry_get_text (GTK_ENTRY (widget));
	editor->priv->name = e_contact_name_from_string (string);
	file_as_set_style (editor, style);

	editor->priv->check_merge = TRUE;

	sensitize_ok (editor);
	if (string && !*string)
		gtk_window_set_title (
			GTK_WINDOW (editor->priv->app), _("Contact Editor"));
}

static void
file_as_combo_changed (GtkWidget *widget,
                       EContactEditor *editor)
{
	GtkWidget *entry;
	gchar *string = NULL;

	entry = gtk_bin_get_child (GTK_BIN (widget));
	if (entry)
		string = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	if (string && *string) {
		gchar *title;
		title = g_strdup_printf (_("Contact Editor — %s"), string);
		gtk_window_set_title (GTK_WINDOW (editor->priv->app), title);
		g_free (title);
	}
	else {
		gtk_window_set_title (
			GTK_WINDOW (editor->priv->app), _("Contact Editor"));
	}
	sensitize_ok (editor);

	g_free (string);
}

static void
company_entry_changed (GtkWidget *widget,
                       EContactEditor *editor)
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

	if (!editor->priv->target_client)
		return;

	source_menu = e_builder_get_widget (
		editor->priv->builder, "client-combo-box");

	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (source_menu),
		e_client_get_source (E_CLIENT (editor->priv->target_client)));
}

static void
sensitize_ok (EContactEditor *ce)
{
	GtkWidget *widget;
	gboolean   allow_save;
	GtkWidget *entry_fullname =
		e_builder_get_widget (ce->priv->builder, "entry-fullname");
	GtkWidget *entry_file_as =
		gtk_bin_get_child (GTK_BIN (
		e_builder_get_widget (ce->priv->builder, "combo-file-as")));
	GtkWidget *company_name =
		e_builder_get_widget (ce->priv->builder, "entry-company");
	const gchar *name_entry_string =
		gtk_entry_get_text (GTK_ENTRY (entry_fullname));
	const gchar *file_as_entry_string =
		gtk_entry_get_text (GTK_ENTRY (entry_file_as));
	const gchar *company_name_string =
		gtk_entry_get_text (GTK_ENTRY (company_name));

	allow_save = ce->priv->target_editable && ce->priv->changed;

	if (!strcmp (name_entry_string, "") ||
	    !strcmp (file_as_entry_string, "")) {
		if (strcmp (company_name_string , "")) {
			allow_save = TRUE;
		}
		else
			allow_save = FALSE;
	}
	widget = e_builder_get_widget (ce->priv->builder, "button-ok");
	gtk_widget_set_sensitive (widget, allow_save);
}

static void
object_changed (GObject *object,
                EContactEditor *editor)
{
	if (!editor->priv->target_editable) {
		g_warning ("non-editable contact editor has an editable field in it.");
		return;
	}

	if (!editor->priv->check_merge && GTK_IS_WIDGET (object)) {
		const gchar *widget_name;

		widget_name = gtk_widget_get_name (GTK_WIDGET (object));

		if (widget_name &&
		    ((g_str_equal (widget_name, "fullname")) ||
		     (g_str_equal (widget_name, "nickname")) ||
		     (g_str_equal (widget_name, "file-as")) ||
		     (g_str_has_prefix (widget_name, "email-"))))
			editor->priv->check_merge = TRUE;
	}

	if (!editor->priv->changed) {
		editor->priv->changed = TRUE;
		sensitize_ok (editor);
	}
}

static void
image_chooser_changed (GtkWidget *widget,
                       EContactEditor *editor)
{
	editor->priv->image_set = TRUE;
	editor->priv->image_changed = TRUE;
}

static void
set_entry_text (EContactEditor *editor,
                GtkEntry *entry,
                const gchar *string)
{
	const gchar *oldstring = gtk_entry_get_text (entry);

	if (!string)
		string = "";

	if (strcmp (string, oldstring)) {
		g_signal_handlers_block_matched (
			entry, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
		gtk_entry_set_text (entry, string);
		g_signal_handlers_unblock_matched (
			entry, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
	}
}

static void
init_email_record_location (EContactEditor *editor)
{
	GtkWidget *w;
	GtkListStore *store;
	gint i, n_elements;
	EContactEditorDynTable *dyntable;
	const EABTypeLabel *email_types = eab_get_email_type_labels (&n_elements);

	w = e_builder_get_widget (editor->priv->builder, "mail-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);
	store = e_contact_editor_dyntable_get_combo_store (dyntable);

	for (i = 0; i < n_elements; i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    DYNTABLE_COMBO_COLUMN_TEXT, _(email_types[i].text),
		                    DYNTABLE_COMBO_COLUMN_SENSITIVE, TRUE,
		                    -1);
	}

	e_contact_editor_dyntable_set_combo_defaults (dyntable, email_default, G_N_ELEMENTS (email_default));
}

static void
e_contact_editor_preserve_attrs_above_slots (EVCard *vcard,
					     const gchar *attr_name,
					     guint max_slots,
					     GList **inout_attr_list)
{
	GList *existing, *link, *add_attrs = NULL;

	existing = e_vcard_get_attributes_by_name (vcard, attr_name);

	for (link = g_list_nth (existing, max_slots); link; link = g_list_next (link)) {
		EVCardAttribute *attr = link->data;

		add_attrs = g_list_prepend (add_attrs, e_vcard_attribute_copy (attr));
	}

	if (add_attrs)
		*inout_attr_list = g_list_concat (*inout_attr_list, add_attrs);

	g_list_free (existing);
}

static void
fill_in_email (EContactEditor *editor)
{
	GList *email_attr_list;
	GList *l;
	GtkWidget *w;
	EContactEditorDynTable *dyntable;
	GtkListStore *data_store;
	GtkTreeIter iter;

	w = e_builder_get_widget (editor->priv->builder, "mail-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);

	/* Clear */

	e_contact_editor_dyntable_clear_data (dyntable);

	/* Fill in */

	data_store = e_contact_editor_dyntable_extract_data (dyntable);

	email_attr_list = e_vcard_get_attributes_by_name (E_VCARD (editor->priv->contact), EVC_EMAIL);

	for (l = email_attr_list; l; l = g_list_next (l)) {
		EVCardAttribute *attr = l->data;
		gchar           *email_address;
		gint             email_location;

		email_address = e_vcard_attribute_get_value (attr);
		email_location = eab_get_email_type_index (attr);

		gtk_list_store_append (data_store, &iter);
		gtk_list_store_set (data_store, &iter,
		                    DYNTABLE_STORE_COLUMN_SORTORDER, EMAIL_SLOTS + 1,
		                    DYNTABLE_STORE_COLUMN_SELECTED_ITEM, email_location,
		                    DYNTABLE_STORE_COLUMN_ENTRY_STRING, email_address,
		                    -1);

		g_free (email_address);
	}

	g_list_free (email_attr_list);

	e_contact_editor_dyntable_fill_in_data (dyntable);
}

static void
extract_email (EContactEditor *editor)
{
	GList *attr_list = NULL;
	GtkWidget *w;
	EContactEditorDynTable *dyntable;
	EVCard *vcard;
	GtkListStore *data_store;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean valid;

	vcard = E_VCARD (editor->priv->contact);
	w = e_builder_get_widget (editor->priv->builder, "mail-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);
	data_store = e_contact_editor_dyntable_extract_data (dyntable);
	tree_model = GTK_TREE_MODEL (data_store);

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);
	while (valid) {
		gchar *address = NULL;
		gint   location;
		EVCardAttribute *attr;

		attr = e_vcard_attribute_new (NULL, e_contact_vcard_attribute (E_CONTACT_EMAIL));

		gtk_tree_model_get (tree_model,&iter,
		                   DYNTABLE_STORE_COLUMN_SELECTED_ITEM, &location,
		                   DYNTABLE_STORE_COLUMN_ENTRY_STRING, &address,
		                   -1);

		if (location >= 0) {
			const gchar *type;
			eab_email_index_to_type (location, &type);
			e_vcard_attribute_add_param_with_value (
				attr,
				e_vcard_attribute_param_new (EVC_TYPE),
				type);
		}

		e_vcard_attribute_add_value_take (attr, g_steal_pointer (&address));

		attr_list = g_list_prepend (attr_list, attr);

		valid = gtk_tree_model_iter_next (tree_model, &iter);
	}

	attr_list = g_list_reverse (attr_list);

	e_contact_editor_preserve_attrs_above_slots (vcard, EVC_EMAIL, EMAIL_SLOTS, &attr_list);
	e_vcard_remove_attributes (vcard, NULL, EVC_EMAIL);
	e_vcard_append_attributes_take (vcard, g_steal_pointer (&attr_list));
}

static void
sensitize_email (EContactEditor *editor)
{
	gboolean enabled = FALSE;
	GtkWidget *w;
	EContactEditorDynTable *dyntable;
	guint max_entries = SLOTS_IN_COLLAPSED_STATE;

	w = e_builder_get_widget (editor->priv->builder, "mail-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);

	if (editor->priv->target_editable) {
		if (is_field_supported (editor, E_CONTACT_EMAIL)) {
			enabled = TRUE;
			max_entries = EMAIL_SLOTS;
		} else if (is_field_supported (editor, E_CONTACT_EMAIL_4)) {
			enabled = TRUE;
			max_entries = 4;
		} else if (is_field_supported (editor, E_CONTACT_EMAIL_3)) {
			enabled = TRUE;
			max_entries = 3;
		} else if (is_field_supported (editor, E_CONTACT_EMAIL_2)) {
			enabled = TRUE;
			max_entries = 2;
		} else if (is_field_supported (editor, E_CONTACT_EMAIL_1)) {
			enabled = TRUE;
			max_entries = 1;
		}
	}

	gtk_widget_set_sensitive (w, enabled);
	e_contact_editor_dyntable_set_max_entries (dyntable, max_entries);
}

static void
row_added_cb (GtkExpander *expander)
{
	/* newly added row is always visible, setting expanded=true */
	gtk_expander_set_expanded (expander, TRUE);
}

static void
init_email (EContactEditor *editor)
{
	EContactEditorDynTable *dyntable;
	GtkExpander *expander;

	expander = GTK_EXPANDER (
			e_builder_get_widget (editor->priv->builder, "expander-contact-email"));
	dyntable = E_CONTACT_EDITOR_DYNTABLE (
			e_builder_get_widget (editor->priv->builder, "mail-dyntable"));

	e_contact_editor_dyntable_set_max_entries (dyntable, EMAIL_SLOTS);
	e_contact_editor_dyntable_set_num_columns (dyntable, SLOTS_PER_LINE, TRUE);
	e_contact_editor_dyntable_set_show_min (dyntable, SLOTS_IN_COLLAPSED_STATE);

	g_signal_connect (
		dyntable, "changed",
		G_CALLBACK (object_changed), editor);
	g_signal_connect_swapped (
		dyntable, "activate",
		G_CALLBACK (entry_activated), editor);
	g_signal_connect_swapped (
		dyntable, "row-added",
		G_CALLBACK (row_added_cb), expander);

	init_email_record_location (editor);

	gtk_expander_set_expanded (expander, TRUE);
}

static void
fill_in_phone (EContactEditor *editor)
{
	GList *tel_attr_list;
	GList *l;
	GtkWidget *w;
	EContactEditorDynTable *dyntable;
	GtkListStore *data_store;
	GtkTreeIter iter;

	w = e_builder_get_widget (editor->priv->builder, "phone-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);

	/* Clear */

	e_contact_editor_dyntable_clear_data (dyntable);

	/* Fill in */

	tel_attr_list = e_vcard_get_attributes_by_name (E_VCARD (editor->priv->contact), EVC_TEL);

	data_store = e_contact_editor_dyntable_extract_data (dyntable);

	for (l = tel_attr_list; l; l = g_list_next (l)) {
		EVCardAttribute *attr = l->data;
		gchar *phone;
		gint phone_type;

		phone_type = eab_get_phone_type_index (attr);
		phone = e_vcard_attribute_get_value (attr);

		gtk_list_store_append (data_store, &iter);
		gtk_list_store_set (data_store,&iter,
		                   DYNTABLE_STORE_COLUMN_SORTORDER, PHONE_SLOTS + 1,
		                   DYNTABLE_STORE_COLUMN_SELECTED_ITEM, phone_type,
		                   DYNTABLE_STORE_COLUMN_ENTRY_STRING, phone,
		                   -1);

		g_free (phone);
	}

	e_contact_editor_dyntable_fill_in_data (dyntable);

	g_list_free (tel_attr_list);
}

static void
extract_phone (EContactEditor *editor)
{
	GList *tel_attr_list = NULL;
	GtkWidget *w;
	EContactEditorDynTable *dyntable;
	EVCard *vcard;
	GtkListStore *data_store;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean valid;

	vcard = E_VCARD (editor->priv->contact);
	w = e_builder_get_widget (editor->priv->builder, "phone-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);
	data_store = e_contact_editor_dyntable_extract_data (dyntable);
	tree_model = GTK_TREE_MODEL (data_store);

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);
	while (valid) {
		gint phone_type;
		gchar *phone = NULL;
		EVCardAttribute *attr;

		gtk_tree_model_get (tree_model,&iter,
		                   DYNTABLE_STORE_COLUMN_SELECTED_ITEM, &phone_type,
		                   DYNTABLE_STORE_COLUMN_ENTRY_STRING, &phone,
		                   -1);

		attr = e_vcard_attribute_new ("", EVC_TEL);
		if (phone_type >= 0) {
			const gchar *type_1;
			const gchar *type_2;

			eab_phone_index_to_type (phone_type, &type_1, &type_2);

			e_vcard_attribute_add_param_with_value (
				attr, e_vcard_attribute_param_new (EVC_TYPE), type_1);

			if (type_2)
				e_vcard_attribute_add_param_with_value (
					attr, e_vcard_attribute_param_new (EVC_TYPE), type_2);
		}

		e_vcard_attribute_add_value_take (attr, g_steal_pointer (&phone));

		tel_attr_list = g_list_prepend (tel_attr_list, attr);

		valid = gtk_tree_model_iter_next (tree_model, &iter);
	}

	tel_attr_list = g_list_reverse (tel_attr_list);

	e_contact_editor_preserve_attrs_above_slots (vcard, EVC_TEL, PHONE_SLOTS, &tel_attr_list);
	e_vcard_remove_attributes (vcard, NULL, EVC_TEL);
	e_vcard_append_attributes_take (vcard, g_steal_pointer (&tel_attr_list));
}

static void
init_phone_record_type (EContactEditor *editor)
{
	GtkWidget *w;
	GtkListStore *store;
	gint i, n_elements;
	EContactEditorDynTable *dyntable;
	const EABTypeLabel *eab_phone_types;

	w = e_builder_get_widget (editor->priv->builder, "phone-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);
	store = e_contact_editor_dyntable_get_combo_store (dyntable);
	eab_phone_types = eab_get_phone_type_labels (&n_elements);

	for (i = 0; i < n_elements; i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    DYNTABLE_COMBO_COLUMN_TEXT, _(eab_phone_types[i].text),
		                    DYNTABLE_COMBO_COLUMN_SENSITIVE, TRUE,
		                    -1);
	}

	e_contact_editor_dyntable_set_combo_defaults (dyntable, phones_default, G_N_ELEMENTS (phones_default));
}

static void
init_phone (EContactEditor *editor)
{
	EContactEditorDynTable *dyntable;
	GtkExpander *expander;

	expander = GTK_EXPANDER (
			e_builder_get_widget (editor->priv->builder, "expander-contact-phone"));
	dyntable = E_CONTACT_EDITOR_DYNTABLE (
			e_builder_get_widget (editor->priv->builder, "phone-dyntable"));

	e_contact_editor_dyntable_set_max_entries (dyntable, PHONE_SLOTS);
	e_contact_editor_dyntable_set_num_columns (dyntable, SLOTS_PER_LINE, TRUE);
	e_contact_editor_dyntable_set_show_min (dyntable, SLOTS_IN_COLLAPSED_STATE);

	g_signal_connect (
		dyntable, "changed",
		G_CALLBACK (object_changed), editor);
	g_signal_connect_swapped (
		dyntable, "activate",
		G_CALLBACK (entry_activated), editor);
	g_signal_connect_swapped (
		dyntable, "row-added",
		G_CALLBACK (row_added_cb), expander);

	init_phone_record_type (editor);

	gtk_expander_set_expanded (expander, TRUE);
}

static void
sensitize_phone_types (EContactEditor *editor)
{
	GtkWidget *w;
	GtkListStore *listStore;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint i, n_elements;
	gboolean valid;
	const EABTypeLabel *eab_phone_types;

	w = e_builder_get_widget (editor->priv->builder, "phone-dyntable");
	listStore = e_contact_editor_dyntable_get_combo_store (E_CONTACT_EDITOR_DYNTABLE (w));
	model = GTK_TREE_MODEL (listStore);

	valid = gtk_tree_model_get_iter_first (model, &iter);

	eab_phone_types = eab_get_phone_type_labels (&n_elements);
	for (i = 0; i < n_elements; i++) {
		if (!valid) {
			g_warning (G_STRLOC ": Unexpected end of phone items in combo box");
			return;
		}

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		                    DYNTABLE_COMBO_COLUMN_SENSITIVE,
		                    is_field_supported (editor, eab_phone_types[i].field_id),
		                    -1);

		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
sensitize_phone (EContactEditor *editor)
{
	GtkWidget *w;
	gboolean enabled = FALSE;
	gint i, n_elements;
	const EABTypeLabel *eab_phone_types;

	w = e_builder_get_widget (editor->priv->builder, "phone-dyntable");

	eab_phone_types = eab_get_phone_type_labels (&n_elements);
	if (editor->priv->target_editable) {
		enabled = is_field_supported (editor, E_CONTACT_TEL);
		for (i = 0; i < n_elements && !enabled; i++) {
			enabled = is_field_supported (editor, eab_phone_types[i].field_id);
		}
	}

	gtk_widget_set_sensitive (w, enabled);

	sensitize_phone_types (editor);
}

static void
fill_in_sip (EContactEditor *editor)
{
	GList *sip_attr_list;
	GList *l;
	GtkWidget *w;
	EContactEditorDynTable *dyntable;
	GtkListStore *data_store;
	GtkTreeIter iter;

	w = e_builder_get_widget (editor->priv->builder, "sip-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);

	/* Clear */

	e_contact_editor_dyntable_clear_data (dyntable);

	/* Fill in */

	sip_attr_list = e_vcard_get_attributes_by_name (E_VCARD (editor->priv->contact), EVC_X_SIP);

	data_store = e_contact_editor_dyntable_extract_data (dyntable);

	for (l = sip_attr_list; l; l = g_list_next (l)) {
		EVCardAttribute *attr = l->data;
		gchar *sip;
		gint sip_type;

		sip_type = eab_get_sip_type_index (attr);
		sip = e_vcard_attribute_get_value (attr);

		if (sip_type < 0)
			sip_type = 2;

		gtk_list_store_append (data_store, &iter);
		gtk_list_store_set (data_store,&iter,
		                   DYNTABLE_STORE_COLUMN_SORTORDER, -1,
		                   DYNTABLE_STORE_COLUMN_SELECTED_ITEM, sip_type,
		                   DYNTABLE_STORE_COLUMN_ENTRY_STRING, sip,
		                   -1);

		g_free (sip);
	}

	e_contact_editor_dyntable_fill_in_data (dyntable);
	g_list_free (sip_attr_list);
}

static void
extract_sip (EContactEditor *editor)
{
	GList *sip_attr_list = NULL;
	GtkWidget *w;
	EContactEditorDynTable *dyntable;
	EVCard *vcard;
	GtkListStore *data_store;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean valid;

	vcard = E_VCARD (editor->priv->contact);
	w = e_builder_get_widget (editor->priv->builder, "sip-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);
	data_store = e_contact_editor_dyntable_extract_data (dyntable);
	tree_model = GTK_TREE_MODEL (data_store);

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);
	while (valid) {
		gint sip_type;
		gchar *sip = NULL;
		EVCardAttribute *attr;

		gtk_tree_model_get (tree_model,&iter,
		                   DYNTABLE_STORE_COLUMN_SELECTED_ITEM, &sip_type,
		                   DYNTABLE_STORE_COLUMN_ENTRY_STRING, &sip,
		                   -1);

		attr = e_vcard_attribute_new (NULL, EVC_X_SIP);
		if (sip_type >= 0) {
			const gchar *type_1;

			eab_sip_index_to_type (sip_type, &type_1);

			e_vcard_attribute_add_param_with_value (
				attr, e_vcard_attribute_param_new (EVC_TYPE), type_1);
		}

		e_vcard_attribute_add_value_take (attr, g_steal_pointer (&sip));

		sip_attr_list = g_list_prepend (sip_attr_list, attr);

		valid = gtk_tree_model_iter_next (tree_model, &iter);
	}

	sip_attr_list = g_list_reverse (sip_attr_list);

	e_contact_editor_preserve_attrs_above_slots (vcard, EVC_X_SIP, SIP_SLOTS, &sip_attr_list);
	e_vcard_remove_attributes (vcard, NULL, EVC_X_SIP);
	e_vcard_append_attributes_take (vcard, g_steal_pointer (&sip_attr_list));
}

static void
init_sip_record_type (EContactEditor *editor)
{
	GtkWidget *w;
	GtkListStore *store;
	gint i, n_elements;
	EContactEditorDynTable *dyntable;
	const EABTypeLabel *sip_types = eab_get_sip_type_labels (&n_elements);

	w = e_builder_get_widget (editor->priv->builder, "sip-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);
	store = e_contact_editor_dyntable_get_combo_store (dyntable);

	for (i = 0; i < n_elements; i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    DYNTABLE_COMBO_COLUMN_TEXT, _(sip_types[i].text),
		                    DYNTABLE_COMBO_COLUMN_SENSITIVE, TRUE,
		                    -1);
	}

	e_contact_editor_dyntable_set_combo_defaults (dyntable, sips_default, G_N_ELEMENTS (sips_default));
}

static void
init_sip (EContactEditor *editor)
{
	EContactEditorDynTable *dyntable;
	GtkExpander *expander;

	expander = GTK_EXPANDER (
			e_builder_get_widget (editor->priv->builder, "expander-contact-sip"));
	dyntable = E_CONTACT_EDITOR_DYNTABLE (
			e_builder_get_widget (editor->priv->builder, "sip-dyntable"));

	e_contact_editor_dyntable_set_max_entries (dyntable, SIP_SLOTS);
	e_contact_editor_dyntable_set_num_columns (dyntable, SLOTS_PER_LINE, TRUE);
	e_contact_editor_dyntable_set_show_min (dyntable, SLOTS_IN_COLLAPSED_STATE);

	g_signal_connect (
		dyntable, "changed",
		G_CALLBACK (object_changed), editor);
	g_signal_connect_swapped (
		dyntable, "activate",
		G_CALLBACK (entry_activated), editor);
	g_signal_connect_swapped (
		dyntable, "row-added",
		G_CALLBACK (row_added_cb), expander);

	init_sip_record_type (editor);

	gtk_expander_set_expanded (expander, TRUE);
}

static gboolean
check_dyntable_for_data (EContactEditor *editor,
                         const gchar *name)
{
	EContactEditorDynTable *dyntable;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;

	dyntable   = E_CONTACT_EDITOR_DYNTABLE (e_builder_get_widget (editor->priv->builder, name));
	tree_model = GTK_TREE_MODEL (e_contact_editor_dyntable_extract_data (dyntable));

	return gtk_tree_model_get_iter_first (tree_model, &iter);
}

static void
extract_address_textview (EContactEditor *editor,
                          gint record,
                          EContactAddress *address)
{
	gchar         *textview_name;
	GtkWidget     *textview;
	GtkTextBuffer *text_buffer;
	GtkTextIter    iter_1, iter_2;

	textview_name = g_strdup_printf ("textview-%s-address", address_name[record]);
	textview = e_builder_get_widget (editor->priv->builder, textview_name);
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
extract_address_field (EContactEditor *editor,
                       gint record,
                       const gchar *widget_field_name)
{
	gchar     *entry_name;
	GtkWidget *entry;

	entry_name = g_strdup_printf (
		"entry-%s-%s", address_name[record], widget_field_name);
	entry = e_builder_get_widget (editor->priv->builder, entry_name);
	g_free (entry_name);

	return g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
}

static void
extract_address_from_gui (EContactEditor* editor,
                          EContactAddress* address,
                          gint record)
{
	extract_address_textview (editor, record, address);
	address->locality = extract_address_field (editor, record, "city");
	address->region = extract_address_field (editor, record, "state");
	address->code = extract_address_field (editor, record, "zip");
	address->country = extract_address_field (editor, record, "country");
	address->po = extract_address_field (editor, record, "pobox");
}

static gboolean
check_address_for_data (EContactEditor *editor,
                        gint record)
{
	gboolean has_data = FALSE;
	EContactAddress *address;

	address = e_contact_address_new ();

	extract_address_from_gui (editor, address, record);
	if (!STRING_IS_EMPTY (address->street)   ||
	    !STRING_IS_EMPTY (address->ext)      ||
	    !STRING_IS_EMPTY (address->locality) ||
	    !STRING_IS_EMPTY (address->region)   ||
	    !STRING_IS_EMPTY (address->code)     ||
	    !STRING_IS_EMPTY (address->po)       ||
	    !STRING_IS_EMPTY (address->country)) {
		has_data = TRUE;
	}

	e_contact_address_free (address);

	return has_data;
}

static gboolean
check_web_for_data (EContactEditor *editor)
{
	GtkBuilder *b = editor->priv->builder;

	return  !STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-homepage")))) ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-weblog"))))   ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-caluri"))))   ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-fburl"))))    ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-videourl")))) ;
}

static gboolean
check_job_for_data (EContactEditor *editor)
{
	GtkBuilder *b = editor->priv->builder;

	return  !STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-manager"))))    ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-assistant"))))  ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-profession")))) ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-jobtitle"))))   ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-company"))))    ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-department")))) ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-office")))) ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-orgdir"))));
}

static gboolean
date_empty (ESplitDateEdit *split_date_edit)
{
	guint year = 0, month = 0, day = 0;

	e_split_date_edit_get_ymd (split_date_edit, &year, &month, &day);

	return !year && !month && !day;
}

static gboolean
check_dates_for_data (EContactEditor *editor)
{
	GtkBuilder *b = editor->priv->builder;

	return  !STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-birthplace")))) ||
		!STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-deathplace")))) ||
		!date_empty (E_SPLIT_DATE_EDIT (e_builder_get_widget (b, "dateedit-anniversary"))) ||
		!date_empty (E_SPLIT_DATE_EDIT (e_builder_get_widget (b, "dateedit-birthday"))) ||
		!date_empty (E_SPLIT_DATE_EDIT (e_builder_get_widget (b, "dateedit-deathday")));
}

static gboolean
check_misc_for_data (EContactEditor *editor)
{
	GtkBuilder *b = editor->priv->builder;

	return  !STRING_IS_EMPTY (gtk_entry_get_text (GTK_ENTRY (e_builder_get_widget (b, "entry-spouse"))));
}

static gboolean
check_notes_for_data (EContactEditor *editor)
{
	GtkWidget *tv = e_builder_get_widget (editor->priv->builder, "text-comments");
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));

	return gtk_text_buffer_get_char_count (buffer) > 0;
}

static gboolean
check_certs_for_data (EContactEditor *editor)
{
	GtkWidget *treeview = e_builder_get_widget (editor->priv->builder, "certs-treeview");
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
	return model && gtk_tree_model_get_iter_first (model, &iter);
}

static gboolean
check_section_for_data (EContactEditor *editor,
                        gint check)
{
	gboolean has_data = TRUE;

	switch (check) {
	case CHECK_PHONE:
		has_data = check_dyntable_for_data (editor, "phone-dyntable");
		break;
	case CHECK_SIP:
		has_data = check_dyntable_for_data (editor, "sip-dyntable");
		break;
	case CHECK_IM:
		has_data = check_dyntable_for_data (editor, "im-dyntable");
		break;
	case CHECK_HOME:
		has_data = check_address_for_data (editor, ADDRESS_SLOT_HOME);
		break;
	case CHECK_WORK:
		has_data = check_address_for_data (editor, ADDRESS_SLOT_WORK);
		break;
	case CHECK_OTHER:
		has_data = check_address_for_data (editor, ADDRESS_SLOT_OTHER);
		break;
	case CHECK_WEB:
		has_data = check_web_for_data (editor);
		break;
	case CHECK_JOB:
		has_data = check_job_for_data (editor);
		break;
	case CHECK_DATES:
		has_data = check_dates_for_data (editor);
		break;
	case CHECK_MISC:
		has_data = check_misc_for_data (editor);
		break;
	case CHECK_NOTE:
		has_data = check_notes_for_data (editor);
		break;
	case CHECK_CERTS:
		has_data = check_certs_for_data (editor);
		break;
	default:
		g_warning ("unknown data check requested");
	}

	return has_data;
}

static void
config_sensitize_item (EContactEditor *editor,
                       const gchar *item_name,
                       gint check)
{
	GtkWidget *item;
	gboolean has_data;

	has_data = check_section_for_data (editor, check);
	item     = e_builder_get_widget (editor->priv->builder, item_name);

	if (has_data) {
		gtk_widget_set_sensitive (item, FALSE);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
	} else {
		gtk_widget_set_sensitive (item, TRUE);
	}
}

static void
config_sensitize_cb (GtkWidget *button,
                     EContactEditor *editor)
{
	config_sensitize_item (editor, "menuitem-config-phone", CHECK_PHONE);
	config_sensitize_item (editor, "menuitem-config-sip", CHECK_SIP);
	config_sensitize_item (editor, "menuitem-config-im", CHECK_IM);

	config_sensitize_item (editor, "menuitem-config-web", CHECK_WEB);
	config_sensitize_item (editor, "menuitem-config-job", CHECK_JOB);
	config_sensitize_item (editor, "menuitem-config-dates", CHECK_DATES);
	config_sensitize_item (editor, "menuitem-config-misc", CHECK_MISC);

	config_sensitize_item (editor, "menuitem-config-home", CHECK_HOME);
	config_sensitize_item (editor, "menuitem-config-work", CHECK_WORK);
	config_sensitize_item (editor, "menuitem-config-other", CHECK_OTHER);

	config_sensitize_item (editor, "menuitem-config-notes", CHECK_NOTE);
	config_sensitize_item (editor, "menuitem-config-certs", CHECK_CERTS);
}

/*
 * get the value from GSettings and check if there is data in the widget.
 * if no data is found set_visible (value), set_visible (true) otherwise
 *
 * Returns: the new visibility
 */
static gboolean
configure_widget_visibility (EContactEditor *editor,
                             GSettings *settings,
                             const gchar *widget_name,
                             const gchar *settings_name,
                             gint check)
{
	gboolean  config, has_data;
	GtkWidget *widget;

	config = g_settings_get_boolean (settings, settings_name);
	widget = e_builder_get_widget (editor->priv->builder, widget_name);
	has_data = check_section_for_data (editor, check);

	gtk_widget_set_visible (widget, config || has_data);

	return config || has_data;
}

static void
configure_visibility (EContactEditor *editor)
{
	gboolean show_tab;
	GSettings *settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	configure_widget_visibility (editor, settings, "vbox-contact-phone", "editor-show-contact-phone", CHECK_PHONE);
	configure_widget_visibility (editor, settings, "vbox-contact-sip",   "editor-show-contact-sip",   CHECK_SIP);
	configure_widget_visibility (editor, settings, "vbox-contact-im",    "editor-show-contact-im",    CHECK_IM);

	show_tab  = configure_widget_visibility (editor, settings, "frame-mailing-home",     "editor-show-mailing-home",  CHECK_HOME);
	show_tab |= configure_widget_visibility (editor, settings, "frame-mailing-work",     "editor-show-mailing-work",  CHECK_WORK);
	show_tab |= configure_widget_visibility (editor, settings, "expander-address-other", "editor-show-mailing-other", CHECK_OTHER);
	gtk_widget_set_visible (
			e_builder_get_widget (editor->priv->builder, "scrolledwindow-mailing"),
			show_tab);

	show_tab  = configure_widget_visibility (editor, settings, "expander-personal-web",   "editor-show-personal-web",   CHECK_WEB);
	show_tab |= configure_widget_visibility (editor, settings, "expander-personal-job",   "editor-show-personal-job",   CHECK_JOB);
	show_tab |= configure_widget_visibility (editor, settings, "expander-personal-dates", "editor-show-personal-dates", CHECK_DATES);
	show_tab |= configure_widget_visibility (editor, settings, "expander-personal-misc",  "editor-show-personal-misc",  CHECK_MISC);
	gtk_widget_set_visible (
			e_builder_get_widget (editor->priv->builder, "scrolledwindow-personal"),
			show_tab);

	configure_widget_visibility (editor, settings, "scrolledwindow-notes", "editor-show-notes", CHECK_NOTE);
	configure_widget_visibility (editor, settings, "certs-grid", "editor-show-certs", CHECK_CERTS);

	g_object_unref (settings);
}

static void
config_menuitem_save (EContactEditor *editor,
                      GSettings *settings,
                      const gchar *item_name,
                      const gchar *key)
{
	GtkWidget *item;
	gboolean active, sensitive;

	item      = e_builder_get_widget (editor->priv->builder, item_name);
	active    = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item));
	sensitive = gtk_widget_get_sensitive (item);

	if (sensitive)
		g_settings_set_boolean (settings, key, active);
}

static void
config_save_cb (GtkWidget *button,
                EContactEditor *editor)
{
	GSettings *settings;

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	config_menuitem_save (editor, settings, "menuitem-config-phone", "editor-show-contact-phone");
	config_menuitem_save (editor, settings, "menuitem-config-sip",   "editor-show-contact-sip");
	config_menuitem_save (editor, settings, "menuitem-config-im",    "editor-show-contact-im");

	config_menuitem_save (editor, settings, "menuitem-config-web",   "editor-show-personal-web");
	config_menuitem_save (editor, settings, "menuitem-config-job",   "editor-show-personal-job");
	config_menuitem_save (editor, settings, "menuitem-config-dates", "editor-show-personal-dates");
	config_menuitem_save (editor, settings, "menuitem-config-misc",  "editor-show-personal-misc");

	config_menuitem_save (editor, settings, "menuitem-config-home",  "editor-show-mailing-home");
	config_menuitem_save (editor, settings, "menuitem-config-work",  "editor-show-mailing-work");
	config_menuitem_save (editor, settings, "menuitem-config-other", "editor-show-mailing-other");

	config_menuitem_save (editor, settings, "menuitem-config-notes", "editor-show-notes");
	config_menuitem_save (editor, settings, "menuitem-config-certs", "editor-show-certs");

	g_object_unref (settings);

	configure_visibility (editor);
}

static void
init_config_menuitem (EContactEditor *editor,
                      GSettings *settings,
                      const gchar *item_name,
                      const gchar *key)
{
	gboolean show;
	GtkWidget *item;

	show = g_settings_get_boolean (settings, key);
	item = e_builder_get_widget (editor->priv->builder, item_name);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), show);

	g_signal_connect (
		item, "activate",
		G_CALLBACK (config_save_cb), editor);
}

static void
init_config (EContactEditor *editor)
{
	GtkWidget *button, *menu;
	GSettings *settings;

	button = e_builder_get_widget (editor->priv->builder, "button-config");
	menu   = e_builder_get_widget (editor->priv->builder, "menu-editor-config");
	gtk_menu_button_set_popup (GTK_MENU_BUTTON (button), menu);

	/* save resources by only doing the data checks and sensitizing upon request,
	 * instead of doing it with each change in object_changed()
	 */
	g_signal_connect (
		button, "clicked",
		G_CALLBACK (config_sensitize_cb), editor);

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");

	init_config_menuitem (editor, settings, "menuitem-config-phone", "editor-show-contact-phone");
	init_config_menuitem (editor, settings, "menuitem-config-sip",   "editor-show-contact-sip");
	init_config_menuitem (editor, settings, "menuitem-config-im",    "editor-show-contact-im");

	init_config_menuitem (editor, settings, "menuitem-config-web",   "editor-show-personal-web");
	init_config_menuitem (editor, settings, "menuitem-config-job",   "editor-show-personal-job");
	init_config_menuitem (editor, settings, "menuitem-config-dates", "editor-show-personal-dates");
	init_config_menuitem (editor, settings, "menuitem-config-misc",  "editor-show-personal-misc");

	init_config_menuitem (editor, settings, "menuitem-config-home",  "editor-show-mailing-home");
	init_config_menuitem (editor, settings, "menuitem-config-work",  "editor-show-mailing-work");
	init_config_menuitem (editor, settings, "menuitem-config-other", "editor-show-mailing-other");

	init_config_menuitem (editor, settings, "menuitem-config-notes", "editor-show-notes");
	init_config_menuitem (editor, settings, "menuitem-config-certs", "editor-show-certs");

	g_object_unref (settings);
}

static void
sensitize_sip_types (EContactEditor *editor)
{
	GtkWidget *w;
	GtkListStore *listStore;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint i, n_elements;
	gboolean valid;
	const EABTypeLabel *sip_types = eab_get_sip_type_labels (&n_elements);

	w = e_builder_get_widget (editor->priv->builder, "sip-dyntable");
	listStore = e_contact_editor_dyntable_get_combo_store (E_CONTACT_EDITOR_DYNTABLE (w));
	model = GTK_TREE_MODEL (listStore);

	valid = gtk_tree_model_get_iter_first (model, &iter);

	for (i = 0; i < n_elements; i++) {
		if (!valid) {
			g_warning (G_STRLOC ": Unexpected end of sip items in combo box");
			return;
		}

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		                    DYNTABLE_COMBO_COLUMN_SENSITIVE,
		                    is_field_supported (editor, sip_types[i].field_id),
		                    -1);

		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
sensitize_sip (EContactEditor *editor)
{
	GtkWidget *w;
	gboolean enabled = TRUE;

	w = e_builder_get_widget (editor->priv->builder, "sip-dyntable");

	if (!editor->priv->target_editable ||
	    !is_field_supported (editor, E_CONTACT_SIP))
		enabled = FALSE;

	gtk_widget_set_sensitive (w, enabled);

	sensitize_sip_types (editor);
}

static void
init_im_record_type (EContactEditor *editor)
{
	GtkWidget *w;
	GtkListStore *store;
	gint i, n_elements;
	EContactEditorDynTable *dyntable;
	const EABTypeLabel *im_service;

	w = e_builder_get_widget (editor->priv->builder, "im-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);
	store = e_contact_editor_dyntable_get_combo_store (dyntable);

	im_service = eab_get_im_type_labels (&n_elements);
	for (i = 0; i < n_elements; i++) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
		                    DYNTABLE_COMBO_COLUMN_TEXT, _(im_service[i].text),
		                    DYNTABLE_COMBO_COLUMN_SENSITIVE, TRUE,
		                    -1);
	}

	e_contact_editor_dyntable_set_combo_defaults (dyntable, im_service_default, G_N_ELEMENTS (im_service_default));
}

static void
init_im (EContactEditor *editor)
{
	EContactEditorDynTable *dyntable;
	GtkExpander *expander;

	expander = GTK_EXPANDER (
			e_builder_get_widget (editor->priv->builder, "expander-contact-im"));
	dyntable = E_CONTACT_EDITOR_DYNTABLE (
			e_builder_get_widget (editor->priv->builder, "im-dyntable"));

	e_contact_editor_dyntable_set_max_entries (dyntable, IM_SLOTS);
	e_contact_editor_dyntable_set_num_columns (dyntable, SLOTS_PER_LINE, TRUE);
	e_contact_editor_dyntable_set_show_min (dyntable, SLOTS_IN_COLLAPSED_STATE);

	g_signal_connect (
		dyntable, "changed",
		G_CALLBACK (object_changed), editor);
	g_signal_connect_swapped (
		dyntable, "activate",
		G_CALLBACK (entry_activated), editor);
	g_signal_connect_swapped (
		dyntable, "row-added",
		G_CALLBACK (row_added_cb), expander);

	init_im_record_type (editor);

	gtk_expander_set_expanded (expander, TRUE);
}

static void
fill_in_im (EContactEditor *editor)
{
	gint n_im_services = -1;
	const EABTypeLabel *im_services = eab_get_im_type_labels (&n_im_services);
	GList *im_list;
	GList *l;
	GtkWidget *w;
	EContactEditorDynTable *dyntable;
	GtkListStore *data_store;
	GtkTreeIter iter;

	w = e_builder_get_widget (editor->priv->builder, "im-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);

	/* Clear */

	e_contact_editor_dyntable_clear_data (dyntable);

	/* Fill in */

	data_store = e_contact_editor_dyntable_extract_data (dyntable);

	im_list = e_contact_get (editor->priv->contact, E_CONTACT_IMPP);

	for (l = im_list; l; l = g_list_next (l)) {
		const gchar *impp_value = l->data;
		gchar cut_scheme[128] = { 0, }; /* if anyone uses scheme longer than this, then... well... */
		EContactField field;
		guint scheme_len = 0;
		gint service_type = -1;

		if (!impp_value)
			continue;

		field = e_contact_impp_scheme_to_field (impp_value, &scheme_len);

		if (field == E_CONTACT_FIELD_LAST) {
			const gchar *ptr = strchr (impp_value, ':');
			if (ptr)
				scheme_len = ptr - impp_value + 1;
		} else {
			for (service_type = 0; service_type < n_im_services; service_type++) {
				if (field == im_services[service_type].field_id)
					break;
			}

			if (service_type >= n_im_services)
				service_type = -1;
		}

		if (scheme_len > 0 && scheme_len < sizeof (cut_scheme))
			strncpy (cut_scheme, impp_value, scheme_len - 1);

		gtk_list_store_append (data_store, &iter);
		gtk_list_store_set (data_store, &iter,
		                    DYNTABLE_STORE_COLUMN_SORTORDER, IM_SLOTS + 1, /* attach at the end */
		                    DYNTABLE_STORE_COLUMN_SELECTED_ITEM, service_type,
				    DYNTABLE_STORE_COLUMN_SELECTED_TEXT, service_type == -1 && cut_scheme[0] ? cut_scheme : NULL,
		                    DYNTABLE_STORE_COLUMN_ENTRY_STRING, scheme_len ? impp_value + scheme_len : impp_value,
		                    -1);
	}

	g_list_free_full (im_list, g_free);

	e_contact_editor_dyntable_fill_in_data (dyntable);
}

static void
extract_im (EContactEditor *editor)
{
	gint n_im_services = -1;
	const EABTypeLabel *im_services = eab_get_im_type_labels (&n_im_services);
	GList *impps = NULL; /* gchar * */
	GtkWidget *w;
	EContactEditorDynTable *dyntable;
	GtkListStore *data_store;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean valid;

	w = e_builder_get_widget (editor->priv->builder, "im-dyntable");
	dyntable = E_CONTACT_EDITOR_DYNTABLE (w);
	data_store = e_contact_editor_dyntable_extract_data (dyntable);
	tree_model = GTK_TREE_MODEL (data_store);

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);
	while (valid) {
		gint service_type;
		gchar *im_name = NULL;
		gchar *im_scheme = NULL, *value;

		gtk_tree_model_get (tree_model,&iter,
		                   DYNTABLE_STORE_COLUMN_SELECTED_ITEM, &service_type,
		                   DYNTABLE_STORE_COLUMN_SELECTED_TEXT, &im_scheme,
		                   DYNTABLE_STORE_COLUMN_ENTRY_STRING, &im_name,
		                   -1);

		if (service_type < 0 || service_type >= n_im_services) {
			value = g_strconcat (im_scheme && *im_scheme ? im_scheme : "impp", ":", im_name, NULL);
		} else {
			const gchar *scheme = e_contact_impp_field_to_scheme (im_services[service_type].field_id);
			value = g_strconcat (scheme, im_name, NULL);
		}

		impps = g_list_prepend (impps, value);

		g_free (im_name);
		g_free (im_scheme);

		valid = gtk_tree_model_iter_next (tree_model, &iter);
	}

	impps = g_list_reverse (impps);

	e_contact_set (editor->priv->contact, E_CONTACT_IMPP, impps);

	g_list_free_full (impps, g_free);
}

static void
sensitize_im_types (EContactEditor *editor)
{
	GtkWidget *w;
	GtkListStore *list_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint i, n_elements;
	gboolean valid;
	const EABTypeLabel *im_service = eab_get_im_type_labels (&n_elements);

	w = e_builder_get_widget (editor->priv->builder, "im-dyntable");
	list_store = e_contact_editor_dyntable_get_combo_store (E_CONTACT_EDITOR_DYNTABLE (w));
	model = GTK_TREE_MODEL (list_store);

	valid = gtk_tree_model_get_iter_first (model, &iter);

	for (i = 0; i < n_elements; i++) {
		if (!valid) {
			g_warning (G_STRLOC ": Unexpected end of im items in combo box");
			return;
		}

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			DYNTABLE_COMBO_COLUMN_SENSITIVE,
			is_field_supported (editor, im_service[i].field_id),
			-1);

		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
sensitize_im (EContactEditor *editor)
{
	gint i, n_elements;
	gboolean enabled;
	gboolean no_ims_supported;
	GtkWidget *w;
	const EABTypeLabel *im_service = eab_get_im_type_labels (&n_elements);

	enabled = editor->priv->target_editable;
	no_ims_supported = TRUE;

	for (i = 0; i < n_elements; i++)
		if (is_field_supported (editor, im_service[i].field_id)) {
			no_ims_supported = FALSE;
			break;
		}

	if (no_ims_supported)
		enabled = FALSE;

	w = e_builder_get_widget (editor->priv->builder, "im-dyntable");
	gtk_widget_set_sensitive (w, enabled);

	sensitize_im_types (editor);
}

static void
init_address_textview (EContactEditor *editor,
                       gint record)
{
	gchar *textview_name;
	GtkWidget *textview;
	GtkTextBuffer *text_buffer;

	textview_name = g_strdup_printf (
		"textview-%s-address", address_name[record]);
	textview = e_builder_get_widget (editor->priv->builder, textview_name);
	g_free (textview_name);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));

	g_signal_connect (
		text_buffer, "changed",
		G_CALLBACK (object_changed), editor);
}

static void
init_address_field (EContactEditor *editor,
                    gint record,
                    const gchar *widget_field_name)
{
	gchar *entry_name;
	GtkWidget *entry;

	entry_name = g_strdup_printf (
		"entry-%s-%s", address_name[record], widget_field_name);
	entry = e_builder_get_widget (editor->priv->builder, entry_name);
	g_free (entry_name);

	g_signal_connect (
		entry, "changed",
		G_CALLBACK (object_changed), editor);
	g_signal_connect_swapped (
		entry, "activate",
		G_CALLBACK (entry_activated), editor);
}

static void
init_address_record (EContactEditor *editor,
                     gint record)
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
				GTK_EXPANDER (e_builder_get_widget (editor->priv->builder, "expander-address-other")),
				!editor->priv->compress_ui);
}

static void
fill_in_address_textview (EContactEditor *editor,
                          gint record,
                          EContactAddress *address)
{
	gchar         *textview_name;
	GtkWidget     *textview;
	GtkTextBuffer *text_buffer;
	GtkTextIter    iter_end, iter_start;

	textview_name = g_strdup_printf ("textview-%s-address", address_name[record]);
	textview = e_builder_get_widget (editor->priv->builder, textview_name);
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
fill_in_address_label_textview (EContactEditor *editor,
                                gint record,
                                const gchar *label)
{
	gchar         *textview_name;
	GtkWidget     *textview;
	GtkTextBuffer *text_buffer;

	textview_name = g_strdup_printf (
		"textview-%s-address", address_name[record]);
	textview = e_builder_get_widget (editor->priv->builder, textview_name);
	g_free (textview_name);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
	gtk_text_buffer_set_text (text_buffer, label ? label : "", -1);
}

static void
fill_in_address_field (EContactEditor *editor,
                       gint record,
                       const gchar *widget_field_name,
                       const gchar *string)
{
	gchar     *entry_name;
	GtkWidget *entry;

	entry_name = g_strdup_printf (
		"entry-%s-%s", address_name[record], widget_field_name);
	entry = e_builder_get_widget (editor->priv->builder, entry_name);
	g_free (entry_name);

	set_entry_text (editor, GTK_ENTRY (entry), string);
}

static void
fill_in_address_record (EContactEditor *editor,
                        gint record)
{
	EContactAddress *address;
	gchar           *address_label;

	address = e_contact_get (editor->priv->contact, addresses[record]);
	address_label = e_contact_get (editor->priv->contact, address_labels[record]);

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

static gchar *
append_to_address_label (gchar *address_label,
                         const gchar *part,
                         gboolean newline)
{
	gchar *new_address_label;

	if (STRING_IS_EMPTY (part))
		return address_label;

	if (address_label)
		new_address_label = g_strjoin (
			newline ? "\n" : ", ",
			address_label, part, NULL);
	else
		new_address_label = g_strdup (part);

	g_free (address_label);
	return new_address_label;
}

static void
set_address_label (EContact *contact,
                   EContactField label_field,
                   EContactField address_field,
                   EContactAddress *address)
{
	gchar *address_label = NULL;
	gboolean format_address;
	GSettings *settings;

	if (!address) {
		e_contact_set (contact, label_field, NULL);
		return;
	}

	settings = e_util_ref_settings ("org.gnome.evolution.addressbook");
	format_address = g_settings_get_boolean (settings, "address-formatting");
	g_object_unref (settings);

	if (format_address)
		address_label = eab_format_address (contact, address_field);

	if (!format_address || !address_label) {
		address_label = append_to_address_label (
			address_label, address->street, TRUE);
		address_label = append_to_address_label (
			address_label, address->ext, TRUE);
		address_label = append_to_address_label (
			address_label, address->locality, TRUE);
		address_label = append_to_address_label (
			address_label, address->region, FALSE);
		address_label = append_to_address_label (
			address_label, address->code, TRUE);
		address_label = append_to_address_label (
			address_label, address->po, TRUE);
		address_label = append_to_address_label (
			address_label, address->country, TRUE);
	}

	e_contact_set (contact, label_field, address_label);
	g_free (address_label);
}

static void
extract_address_record (EContactEditor *editor,
                        gint record)
{
	EContactAddress *address;

	address = e_contact_address_new ();

	extract_address_from_gui (editor, address, record);
	if (!STRING_IS_EMPTY (address->street)   ||
	    !STRING_IS_EMPTY (address->ext)      ||
	    !STRING_IS_EMPTY (address->locality) ||
	    !STRING_IS_EMPTY (address->region)   ||
	    !STRING_IS_EMPTY (address->code)     ||
	    !STRING_IS_EMPTY (address->po)       ||
	    !STRING_IS_EMPTY (address->country)) {
		e_contact_set (editor->priv->contact, addresses[record], address);
		set_address_label (editor->priv->contact, address_labels[record], addresses[record], address);
	}
	else {
		e_contact_set (editor->priv->contact, addresses[record], NULL);
		set_address_label (editor->priv->contact, address_labels[record], addresses[record], NULL);
	}

	e_contact_address_free (address);
}

static void
extract_address (EContactEditor *editor)
{
	gint i;

	for (i = 0; i < ADDRESS_SLOTS; i++)
		extract_address_record (editor, i);
}

static void
sensitize_address_textview (EContactEditor *editor,
                            gint record,
                            gboolean enabled)
{
	gchar         *widget_name;
	GtkWidget     *textview;
	GtkWidget     *label;

	widget_name = g_strdup_printf ("textview-%s-address", address_name[record]);
	textview = e_builder_get_widget (editor->priv->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf ("label-%s-address", address_name[record]);
	label = e_builder_get_widget (editor->priv->builder, widget_name);
	g_free (widget_name);

	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), enabled);
	gtk_widget_set_sensitive (label, enabled);
}

static void
sensitize_address_field (EContactEditor *editor,
                         gint record,
                         const gchar *widget_field_name,
                         gboolean enabled)
{
	gchar     *widget_name;
	GtkWidget *entry;
	GtkWidget *label;

	widget_name = g_strdup_printf (
		"entry-%s-%s", address_name[record], widget_field_name);
	entry = e_builder_get_widget (editor->priv->builder, widget_name);
	g_free (widget_name);

	widget_name = g_strdup_printf (
		"label-%s-%s", address_name[record], widget_field_name);
	label = e_builder_get_widget (editor->priv->builder, widget_name);
	g_free (widget_name);

	gtk_editable_set_editable (GTK_EDITABLE (entry), enabled);
	gtk_widget_set_sensitive (label, enabled);
}

static void
sensitize_address_record (EContactEditor *editor,
                          gint record,
                          gboolean enabled)
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

		if (!editor->priv->target_editable ||
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

	{ "entry-birthplace",     E_CONTACT_BIRTHPLACE,   TRUE,  TRUE  },
	{ "label-birthplace",     E_CONTACT_BIRTHPLACE,   FALSE, TRUE  },

	{ "dateedit-anniversary", E_CONTACT_ANNIVERSARY,  TRUE,  TRUE  },
	{ "label-anniversary",    E_CONTACT_ANNIVERSARY,  FALSE, TRUE  },

	{ "dateedit-deathday",    E_CONTACT_DEATHDATE,    TRUE,  TRUE  },
	{ "label-deathday",       E_CONTACT_DEATHDATE,    FALSE, TRUE  },

	{ "entry-deathplace",     E_CONTACT_DEATHPLACE,   TRUE,  TRUE  },
	{ "label-deathplace",     E_CONTACT_DEATHPLACE,   FALSE, TRUE  },

	{ "entry-spouse",         E_CONTACT_SPOUSE,       TRUE,  TRUE  },
	{ "label-spouse",         E_CONTACT_SPOUSE,       FALSE, TRUE  },

	{ "entry-office",         E_CONTACT_OFFICE,       TRUE,  TRUE  },
	{ "label-office",         E_CONTACT_OFFICE,       FALSE, TRUE  },

	{ "text-comments",        E_CONTACT_NOTE,         TRUE,  TRUE  },

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
init_simple_field (EContactEditor *editor,
                   GtkWidget *widget)
{
	GObject *changed_object = NULL;

	if (GTK_IS_ENTRY (widget)) {
		changed_object = G_OBJECT (widget);
		g_signal_connect_swapped (
			widget, "activate",
			G_CALLBACK (entry_activated), editor);

	} else if (GTK_IS_COMBO_BOX (widget)) {
		changed_object = G_OBJECT (widget);
		g_signal_connect_swapped (
			gtk_bin_get_child (GTK_BIN (widget)), "activate",
			G_CALLBACK (entry_activated), editor);

	} else if (GTK_IS_TEXT_VIEW (widget)) {
		changed_object = G_OBJECT (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget)));

	} else if (E_IS_URL_ENTRY (widget)) {
		changed_object = G_OBJECT (widget);
		g_signal_connect_swapped (
			changed_object, "activate",
			G_CALLBACK (entry_activated), editor);

	} else if (E_IS_SPLIT_DATE_EDIT (widget)) {
		changed_object = G_OBJECT (widget);

	} else if (E_IS_IMAGE_CHOOSER (widget)) {
		changed_object = G_OBJECT (widget);
		g_signal_connect (
			widget, "changed",
			G_CALLBACK (image_chooser_changed), editor);

	} else if (GTK_IS_TOGGLE_BUTTON (widget)) {
		g_signal_connect (
			widget, "toggled",
			G_CALLBACK (object_changed), editor);
	}

	if (changed_object)
		g_signal_connect (
			changed_object, "changed",
			G_CALLBACK (object_changed), editor);
}

static void
fill_in_simple_field (EContactEditor *editor,
                      GtkWidget *widget,
                      gint field_id)
{
	EContact *contact;

	contact = editor->priv->contact;

	g_signal_handlers_block_matched (
		widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);

	if (GTK_IS_ENTRY (widget)) {
		gchar *text = e_contact_get (contact, field_id);
		gtk_entry_set_text (GTK_ENTRY (widget), STRING_MAKE_NON_NULL (text));
		g_free (text);

	} else if (GTK_IS_COMBO_BOX (widget)) {
		gchar *text = e_contact_get (contact, field_id);
		gtk_entry_set_text (
			GTK_ENTRY (gtk_bin_get_child (GTK_BIN (widget))),
			STRING_MAKE_NON_NULL (text));
		g_free (text);

	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
		gchar *text = e_contact_get (contact, field_id);
		gtk_text_buffer_set_text (buffer, STRING_MAKE_NON_NULL (text), -1);
		g_free (text);

	} else if (E_IS_URL_ENTRY (widget)) {
		gchar *text = e_contact_get (contact, field_id);
		gtk_entry_set_text (
			GTK_ENTRY (widget), STRING_MAKE_NON_NULL (text));
		g_free (text);

	} else if (E_IS_SPLIT_DATE_EDIT (widget)) {
		EContactDate *date = e_contact_get (contact, field_id);
		e_split_date_edit_set_ymd (E_SPLIT_DATE_EDIT (widget), date ? date->year : 0, date ? date->month : 0, date ? date->day : 0);
		if (date)
			e_contact_date_free (date);

	} else if (E_IS_IMAGE_CHOOSER (widget)) {
		EContactPhoto *photo = e_contact_get (contact, field_id);
		editor->priv->image_set = FALSE;
		if (photo && photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
			e_image_chooser_set_image_data (
				E_IMAGE_CHOOSER (widget),
				(gchar *) photo->data.inlined.data,
				photo->data.inlined.length);
			editor->priv->image_set = TRUE;
		} else if (photo && photo->type == E_CONTACT_PHOTO_TYPE_URI) {
			gchar *file_name = g_filename_from_uri (photo->data.uri, NULL, NULL);
			if (file_name) {
				e_image_chooser_set_from_file (
					E_IMAGE_CHOOSER (widget),
					file_name);
				editor->priv->image_set = TRUE;
				g_free (file_name);
			}
		}

		if (!editor->priv->image_set) {
			gchar *file_name;

			file_name = e_icon_factory_get_icon_filename (
				"avatar-default", GTK_ICON_SIZE_DIALOG);
			e_image_chooser_set_from_file (
				E_IMAGE_CHOOSER (widget), file_name);
			editor->priv->image_set = FALSE;
			g_free (file_name);
		}

		editor->priv->image_changed = FALSE;
		e_contact_photo_free (photo);

	} else if (GTK_IS_TOGGLE_BUTTON (widget)) {
		gboolean val = e_contact_get (contact, field_id) != NULL;

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), val);

	} else {
		g_warning (G_STRLOC ": Unhandled widget class in mappings!");
	}

	g_signal_handlers_unblock_matched (
		widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
}

static void
extract_simple_field (EContactEditor *editor,
                      GtkWidget *widget,
                      gint field_id)
{
	EContact *contact;

	contact = editor->priv->contact;

	if (GTK_IS_ENTRY (widget)) {
		const gchar *text = gtk_entry_get_text (GTK_ENTRY (widget));
		e_contact_set (contact, field_id, (text && *text) ? (gchar *) text : NULL);

	} else if (GTK_IS_COMBO_BOX_TEXT (widget)) {
		gchar *text = NULL;

		if (gtk_combo_box_get_has_entry (GTK_COMBO_BOX (widget))) {
			GtkWidget *entry = gtk_bin_get_child (GTK_BIN (widget));

			text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
		}

		if (!text)
			text = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (widget));

		e_contact_set (contact, field_id, (text && *text) ? text : NULL);

		g_free (text);
	} else if (GTK_IS_COMBO_BOX (widget)) {
		GtkTreeIter iter;
		gchar *text = NULL;

		if (gtk_combo_box_get_has_entry (GTK_COMBO_BOX (widget))) {
			GtkWidget *entry = gtk_bin_get_child (GTK_BIN (widget));

			text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
		}

		if (!text && gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter)) {
			GtkListStore *store;

			store = GTK_LIST_STORE (
				gtk_combo_box_get_model (
				GTK_COMBO_BOX (widget)));

			gtk_tree_model_get (
				GTK_TREE_MODEL (store), &iter,
				0, &text,
				-1);
		}

		e_contact_set (contact, field_id, (text && *text) ? text : NULL);

		g_free (text);

	} else if (GTK_IS_TEXT_VIEW (widget)) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
		GtkTextIter    start, end;
		gchar         *text;

		gtk_text_buffer_get_start_iter (buffer, &start);
		gtk_text_buffer_get_end_iter   (buffer, &end);
		text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
		e_contact_set (contact, field_id, (text && *text) ? text : NULL);
		g_free (text);

	} else if (E_IS_URL_ENTRY (widget)) {
		const gchar *text = gtk_entry_get_text (GTK_ENTRY (widget));
		e_contact_set (contact, field_id, (text && *text) ? (gchar *) text : NULL);

	} else if (E_IS_SPLIT_DATE_EDIT (widget)) {
		EContactDate date;
		e_split_date_edit_get_ymd (E_SPLIT_DATE_EDIT (widget), &date.year, &date.month, &date.day);
		if (date.year != 0 || date.month != 0 || date.day != 0)
			e_contact_set (contact, field_id, &date);
		else
			e_contact_set (contact, field_id, NULL);

	} else if (E_IS_IMAGE_CHOOSER (widget)) {
		EContactPhoto photo;
		photo.type = E_CONTACT_PHOTO_TYPE_INLINED;
		photo.data.inlined.mime_type = NULL;
		if (editor->priv->image_changed) {
			gchar *img_buff = NULL;
			if (editor->priv->image_set &&
			    e_image_chooser_get_image_data (
					E_IMAGE_CHOOSER (widget),
					&img_buff, &photo.data.inlined.length)) {
				GdkPixbuf *pixbuf, *new;
				GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();

				photo.data.inlined.data = (guchar *) img_buff;
				img_buff = NULL;
				gdk_pixbuf_loader_write (
					loader,
					photo.data.inlined.data,
					photo.data.inlined.length, NULL);
				gdk_pixbuf_loader_close (loader, NULL);

				pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
				if (pixbuf) {
					gint width, height, prompt_response;

					g_object_ref (pixbuf);

					height = gdk_pixbuf_get_height (pixbuf);
					width = gdk_pixbuf_get_width (pixbuf);
					if ((height > 1024 || width > 1024)) {

						prompt_response =
							e_alert_run_dialog_for_args
							(GTK_WINDOW (editor->priv->app),
							 "addressbook:prompt-resize",
							 NULL);

						if (prompt_response == GTK_RESPONSE_YES) {
							if (width > height) {
								height = height * 1024 / width;
								width = 1024;
							} else {
								width = width * 1024 / height;
								height = 1024;
							}

							new = e_icon_factory_pixbuf_scale (pixbuf, width, height);
							if (new) {
								GdkPixbufFormat *format =
									gdk_pixbuf_loader_get_format (loader);
								gchar *format_name =
									gdk_pixbuf_format_get_name (format);
								g_free (photo.data.inlined.data);
								gdk_pixbuf_save_to_buffer (
									new, &img_buff,
									&photo.data.inlined.length,
									format_name, NULL, NULL);
								photo.data.inlined.data = (guchar *) img_buff;
								img_buff = NULL;
								g_free (format_name);
								g_object_unref (new);
							}
						} else if (prompt_response == GTK_RESPONSE_CANCEL) {
							g_object_unref (pixbuf);
							g_object_unref (loader);
							return;
						}
					}
					g_object_unref (pixbuf);
				}
				editor->priv->image_changed = FALSE;
				g_object_unref (loader);

				e_contact_set (contact, field_id, &photo);

				g_free (photo.data.inlined.data);

			} else {
				editor->priv->image_changed = FALSE;
				e_contact_set (contact, field_id, NULL);
			}
		}

	} else if (GTK_IS_TOGGLE_BUTTON (widget)) {
		gboolean val;

		val = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
		e_contact_set (contact, field_id, val ? (gpointer) 1 : NULL);

	} else {
		g_warning (G_STRLOC ": Unhandled widget class in mappings!");
	}
}

static void
sensitize_simple_field (GtkWidget *widget,
                        gboolean enabled)
{
	if (GTK_IS_ENTRY (widget))
		gtk_editable_set_editable (GTK_EDITABLE (widget), enabled);
	else if (GTK_IS_TEXT_VIEW (widget))
		gtk_text_view_set_editable (GTK_TEXT_VIEW (widget), enabled);
	else
		gtk_widget_set_sensitive (widget, enabled);
}

static void
init_gender_combo (GtkComboBoxText *combo_text)
{
	gtk_combo_box_text_remove_all (combo_text);
	gtk_combo_box_text_append (combo_text, "", "");
	gtk_combo_box_text_append (combo_text, "M", C_("gender-sex", "Male"));
	gtk_combo_box_text_append (combo_text, "F", C_("gender-sex", "Female"));
	gtk_combo_box_text_append (combo_text, "O", C_("gender-sex", "Other"));
	gtk_combo_box_text_append (combo_text, "U", C_("gender-sex", "Unknown"));
	gtk_combo_box_text_append (combo_text, "N", C_("gender-sex", "Not Applicable"));
}

static void
init_simple (EContactEditor *editor)
{
	GtkWidget *widget;
	gint       i;

	for (i = 0; i < G_N_ELEMENTS (simple_field_map); i++) {
		widget = e_builder_get_widget (
			editor->priv->builder, simple_field_map[i].widget_name);
		if (!widget)
			continue;

		init_simple_field (editor, widget);
	}

	/* --- Special cases --- */

	widget = e_builder_get_widget (editor->priv->builder, "combo-gender");
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (object_changed), editor);
	init_gender_combo (GTK_COMBO_BOX_TEXT (widget));

	widget = e_builder_get_widget (editor->priv->builder, "entry-gender");
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (object_changed), editor);

	/* Update file_as */

	widget = e_builder_get_widget (editor->priv->builder, "entry-fullname");
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (name_entry_changed), editor);

	widget = e_builder_get_widget (editor->priv->builder, "combo-file-as");
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (widget), 0);
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (file_as_combo_changed), editor);

	widget = e_builder_get_widget (editor->priv->builder, "entry-company");
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (company_entry_changed), editor);
}

static void
fill_in_simple (EContactEditor *editor)
{
	EContactName *name;
	EContactGender *gender;
	GtkWidget *widget;
	gchar *filename;
	gint          i;

	for (i = 0; i < G_N_ELEMENTS (simple_field_map); i++) {
		if (simple_field_map[i].field_id < 0 ||
		    !simple_field_map[i].process_data)
			continue;

		widget = e_builder_get_widget (
			editor->priv->builder, simple_field_map[i].widget_name);
		if (!widget)
			continue;

		fill_in_simple_field (
			editor, widget, simple_field_map[i].field_id);
	}

	/* --- Special cases --- */

	gender = e_contact_get (editor->priv->contact, E_CONTACT_GENDER);

	widget = e_builder_get_widget (editor->priv->builder, "combo-gender");
	if (gender) {
		GtkComboBox *combo = GTK_COMBO_BOX (widget);

		gtk_combo_box_set_active_id (combo, e_contact_gender_sex_to_string (gender->sex));

		if (gtk_combo_box_get_active (combo) == -1)
			gtk_combo_box_set_active_id (combo, "");
	} else {
		gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), "");
	}

	widget = e_builder_get_widget (editor->priv->builder, "entry-gender");
	if (gender && gender->identity)
		gtk_entry_set_text (GTK_ENTRY (widget), gender->identity);
	else
		gtk_entry_set_text (GTK_ENTRY (widget), "");

	g_clear_pointer (&gender, e_contact_gender_free);

	/* Update broken-up name */

	g_object_get (editor->priv->contact, "name", &name, NULL);

	if (editor->priv->name)
		e_contact_name_free (editor->priv->name);

	editor->priv->name = name;

	/* Update the contact editor title */

	filename = (gchar *) e_contact_get (editor->priv->contact, E_CONTACT_FILE_AS);

	if (filename) {
		gchar *title;
		title = g_strdup_printf (_("Contact Editor — %s"), filename);
		gtk_window_set_title (GTK_WINDOW (editor->priv->app), title);
		g_free (title);
		g_free (filename);
	} else
		gtk_window_set_title (
			GTK_WINDOW (editor->priv->app), _("Contact Editor"));

	/* Update file_as combo options */

	update_file_as_combo (editor);
}

static void
extract_simple (EContactEditor *editor)
{
	GtkWidget *widget;
	gint i;

	for (i = 0; i < G_N_ELEMENTS (simple_field_map); i++) {
		if (simple_field_map[i].field_id < 0 ||
		    !simple_field_map[i].process_data)
			continue;

		widget = e_builder_get_widget (
			editor->priv->builder, simple_field_map[i].widget_name);
		if (!widget)
			continue;

		extract_simple_field (
			editor, widget, simple_field_map[i].field_id);
	}

	/* Special cases */

	widget = e_builder_get_widget (editor->priv->builder, "combo-gender");
	if (widget && gtk_widget_is_sensitive (widget)) {
		EContactGender gender = { 0, };
		const gchar *active_id;

		active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (widget));
		if (g_strcmp0 (active_id, "M") == 0)
			gender.sex = E_CONTACT_GENDER_SEX_MALE;
		else if (g_strcmp0 (active_id, "F") == 0)
			gender.sex = E_CONTACT_GENDER_SEX_FEMALE;
		else if (g_strcmp0 (active_id, "O") == 0)
			gender.sex = E_CONTACT_GENDER_SEX_OTHER;
		else if (g_strcmp0 (active_id, "N") == 0)
			gender.sex = E_CONTACT_GENDER_SEX_NOT_APPLICABLE;
		else if (g_strcmp0 (active_id, "U") == 0)
			gender.sex = E_CONTACT_GENDER_SEX_UNKNOWN;
		else
			gender.sex = E_CONTACT_GENDER_SEX_NOT_SET;

		widget = e_builder_get_widget (editor->priv->builder, "entry-gender");
		gender.identity = widget ? g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)))) : NULL;
		if (gender.identity && !*gender.identity)
			g_clear_pointer (&gender.identity, g_free);

		if (gender.identity != NULL || gender.sex != E_CONTACT_GENDER_SEX_NOT_SET)
			e_contact_set (editor->priv->contact, E_CONTACT_GENDER, &gender);
		else
			e_contact_set (editor->priv->contact, E_CONTACT_GENDER, NULL);

		g_free (gender.identity);
	} else {
		e_contact_set (editor->priv->contact, E_CONTACT_GENDER, NULL);
	}

	e_contact_set (editor->priv->contact, E_CONTACT_NAME, editor->priv->name);
}

static void
sensitize_simple (EContactEditor *editor)
{
	const gchar *gender_fields[] = { "label-gender", "combo-gender", "entry-gender" };
	GtkWidget *widget;
	gint i;

	for (i = 0; i < G_N_ELEMENTS (simple_field_map); i++) {
		gboolean   enabled = TRUE;

		widget = e_builder_get_widget (
			editor->priv->builder, simple_field_map[i].widget_name);
		if (!widget)
			continue;

		if (simple_field_map[i].field_id >= 0 &&
		    !is_field_supported (editor, simple_field_map[i].field_id))
			enabled = FALSE;

		if (simple_field_map[i].desensitize_for_read_only &&
		    !editor->priv->target_editable)
			enabled = FALSE;

		sensitize_simple_field (widget, enabled);
	}

	/* Special cases */

	for (i = 0; i < G_N_ELEMENTS (gender_fields); i++) {
		gboolean   enabled = TRUE;

		widget = e_builder_get_widget (editor->priv->builder, gender_fields[i]);
		if (!widget)
			continue;

		if (!is_field_supported (editor, E_CONTACT_GENDER))
			enabled = FALSE;

		if (!editor->priv->target_editable)
			enabled = FALSE;

		sensitize_simple_field (widget, enabled);
	}
}

static FieldMapping multival_map[] = {
	{ "entry-orgdir",		E_CONTACT_ORG_DIRECTORY, TRUE,  TRUE },
	{ "label-orgdir",		E_CONTACT_ORG_DIRECTORY, FALSE, TRUE },

	{ "entry-expertise",		E_CONTACT_EXPERTISE,	TRUE,  TRUE },
	{ "label-expertise",		E_CONTACT_EXPERTISE,	FALSE, TRUE },

	{ "entry-hobby",		E_CONTACT_HOBBY,	TRUE,  TRUE },
	{ "label-hobby",		E_CONTACT_HOBBY,	FALSE, TRUE },

	{ "entry-interest",		E_CONTACT_INTEREST,	TRUE,  TRUE },
	{ "label-interest",		E_CONTACT_INTEREST,	FALSE, TRUE }
};

static void
init_multival (EContactEditor *editor)
{
	GtkWidget *widget;
	gint       i;

	for (i = 0; i < G_N_ELEMENTS (multival_map); i++) {
		widget = e_builder_get_widget (editor->priv->builder, multival_map[i].widget_name);
		if (!widget)
			continue;

		init_simple_field (editor, widget);
	}
}

static void
fill_in_multival_field (EContactEditor *editor,
			GtkWidget *widget,
			gint field_id)
{
	EContact *contact;

	contact = editor->priv->contact;

	g_signal_handlers_block_matched (widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);

	if (GTK_IS_ENTRY (widget)) {
		GList *list = e_contact_get (contact, field_id), *link;
		GString *str = g_string_new (NULL);

		for (link = list; link; link = g_list_next (link)) {
			gchar *value = link->data;

			if (!value)
				continue;

			value = g_strstrip (value);

			if (*value) {
				if (str->len)
					g_string_append_c (str, ',');
				g_string_append (str, value);
			}
		}

		gtk_entry_set_text (GTK_ENTRY (widget), str->str);

		g_list_free_full (list, g_free);
		g_string_free (str, TRUE);
	} else {
		g_warning (G_STRLOC ": Unhandled widget class in mappings!");
	}

	g_signal_handlers_unblock_matched (widget, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, editor);
}

static void
fill_in_multival (EContactEditor *editor)
{
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (multival_map); ii++) {
		GtkWidget *widget;

		if (multival_map[ii].field_id < 0 ||
		    !multival_map[ii].process_data)
			continue;

		widget = e_builder_get_widget (editor->priv->builder, multival_map[ii].widget_name);
		if (!widget)
			continue;

		fill_in_multival_field (editor, widget, multival_map[ii].field_id);
	}
}

static void
extract_multival_field (EContactEditor *editor,
			GtkWidget *widget,
			gint field_id)
{
	EContact *contact;

	contact = editor->priv->contact;

	if (GTK_IS_ENTRY (widget)) {
		const gchar *text = gtk_entry_get_text (GTK_ENTRY (widget));
		gchar **split_text = g_strsplit (text, ",", -1);
		GList *values = NULL;
		guint ii;

		for (ii = 0; split_text && split_text[ii]; ii++) {
			gchar *value = g_strstrip (split_text[ii]);

			if (*value)
				values = g_list_prepend (values, value);
		}

		values = g_list_reverse (values);

		e_contact_set (contact, field_id, values);

		g_list_free (values);
		g_strfreev (split_text);
	} else {
		g_warning (G_STRLOC ": Unhandled widget class in mappings!");
	}
}

static void
extract_multival (EContactEditor *editor)
{
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (multival_map); ii++) {
		GtkWidget *widget;

		if (multival_map[ii].field_id < 0 ||
		    !multival_map[ii].process_data)
			continue;

		widget = e_builder_get_widget (editor->priv->builder, multival_map[ii].widget_name);
		if (!widget)
			continue;

		extract_multival_field (editor, widget, multival_map[ii].field_id);
	}
}

static void
sensitize_multival (EContactEditor *editor)
{
	guint ii;

	for (ii = 0; ii < G_N_ELEMENTS (multival_map); ii++) {
		GtkWidget *widget;
		gboolean   enabled = TRUE;

		widget = e_builder_get_widget (editor->priv->builder, multival_map[ii].widget_name);
		if (!widget)
			continue;

		if (simple_field_map[ii].field_id >= 0 &&
		    !is_field_supported (editor, multival_map[ii].field_id))
			enabled = FALSE;

		if (simple_field_map[ii].desensitize_for_read_only &&
		    !editor->priv->target_editable)
			enabled = FALSE;

		sensitize_simple_field (widget, enabled);
	}
}

enum CertKind {
	CERT_KIND_X509,
	CERT_KIND_PGP
};

enum CertColumns {
	CERT_COLUMN_SUBJECT_STRING,
	CERT_COLUMN_KIND_STRING,
	CERT_COLUMN_KIND_INT,
	CERT_COLUMN_CERT_BYTES,
	N_CERT_COLUMNS
};

static void
cert_tab_selection_changed_cb (GtkTreeSelection *selection,
			       EContactEditor *editor)
{
	GtkWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean has_selected;

	g_return_if_fail (E_IS_CONTACT_EDITOR (editor));

	has_selected = gtk_tree_selection_get_selected (selection, &model, &iter);

	widget = e_builder_get_widget (editor->priv->builder, "cert-remove-btn");
	gtk_widget_set_sensitive (widget, has_selected);

	widget = e_builder_get_widget (editor->priv->builder, "cert-load-pgp-btn");
	gtk_widget_set_sensitive (widget, has_selected && is_field_supported (editor, E_CONTACT_PGP_CERT));

	widget = e_builder_get_widget (editor->priv->builder, "cert-load-x509-btn");
	gtk_widget_set_sensitive (widget, has_selected && is_field_supported (editor, E_CONTACT_X509_CERT));

	widget = e_builder_get_widget (editor->priv->builder, "cert-save-btn");
	gtk_widget_set_sensitive (widget, has_selected);

	widget = e_builder_get_widget (editor->priv->builder, "cert-preview-scw");
	widget = gtk_bin_get_child (GTK_BIN (widget));

	if (GTK_IS_VIEWPORT (widget))
		widget = gtk_bin_get_child (GTK_BIN (widget));

	g_return_if_fail (E_IS_CERTIFICATE_WIDGET (widget));

	if (has_selected) {
		GBytes *cert_bytes = NULL;

		gtk_tree_model_get (model, &iter, CERT_COLUMN_CERT_BYTES, &cert_bytes, -1);

		e_certificate_widget_set_der (E_CERTIFICATE_WIDGET (widget), g_bytes_get_data (cert_bytes, NULL), g_bytes_get_size (cert_bytes));

		g_clear_pointer (&cert_bytes, g_bytes_unref);
	} else {
		e_certificate_widget_set_der (E_CERTIFICATE_WIDGET (widget), NULL, 0);
	}
}

static void
cert_add_filters_for_kind (GtkFileChooser *file_chooser,
			   enum CertKind kind)
{
	GtkFileFilter *filter;

	g_return_if_fail (GTK_IS_FILE_CHOOSER (file_chooser));
	g_return_if_fail (kind == CERT_KIND_PGP || kind == CERT_KIND_X509);

	if (kind == CERT_KIND_X509) {
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("X.509 certificates"));
		gtk_file_filter_add_mime_type (filter, "application/x-x509-user-cert");
		gtk_file_chooser_add_filter (file_chooser, filter);
	} else {
		filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("PGP keys"));
		gtk_file_filter_add_mime_type (filter, "application/pgp-keys");
		gtk_file_chooser_add_filter (file_chooser, filter);
	}

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (file_chooser, filter);
}

static EContactCert *
cert_load_for_kind (EContactEditor *editor,
		    enum CertKind kind)
{
	EContactCert *cert = NULL;
	GtkWindow *parent;
	GtkFileChooserNative *native;
	GtkFileChooser *file_chooser;
	GError *error = NULL;

	g_return_val_if_fail (E_IS_CONTACT_EDITOR (editor), NULL);
	g_return_val_if_fail (kind == CERT_KIND_PGP || kind == CERT_KIND_X509, NULL);

	parent = eab_editor_get_window (EAB_EDITOR (editor));
	native = gtk_file_chooser_native_new (
		kind == CERT_KIND_PGP ? _("Open PGP key") : _("Open X.509 certificate"), parent,
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Open"), _("_Cancel"));

	file_chooser = GTK_FILE_CHOOSER (native);
	gtk_file_chooser_set_local_only (file_chooser, TRUE);
	gtk_file_chooser_set_select_multiple (file_chooser, FALSE);

	cert_add_filters_for_kind (file_chooser, kind);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;
		gchar *content = NULL;
		gsize length = 0;

		filename = gtk_file_chooser_get_filename (file_chooser);
		if (!filename) {
			g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, _("Chosen file is not a local file."));
		} else if (g_file_get_contents (filename, &content, &length, &error) && length > 0) {
			cert = e_contact_cert_new ();
			cert->length = length;
			cert->data = content;
		}

		g_free (filename);
	}

	g_object_unref (native);

	if (error) {
		e_notice (parent, GTK_MESSAGE_ERROR, _("Failed to load certificate: %s"), error->message);
		g_clear_error (&error);
	}

	return cert;
}

static void
cert_update_row_with_cert (GtkListStore *list_store,
			   GtkTreeIter *iter,
			   EContactCert *cert,
			   enum CertKind kind)
{
	GBytes *cert_bytes;
	gchar *subject = NULL;

	g_return_if_fail (GTK_IS_LIST_STORE (list_store));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (cert != NULL);
	g_return_if_fail (kind == CERT_KIND_PGP || kind == CERT_KIND_X509);

	if (kind == CERT_KIND_X509 && cert->data && cert->length) {
		#ifdef ENABLE_SMIME
		ECert *ecert;

		ecert = e_cert_new_from_der (cert->data, cert->length);
		if (ecert) {
			const gchar *ident;

			ident = e_cert_get_cn (ecert);
			if (!ident || !*ident)
				ident = e_cert_get_email (ecert);
			if (!ident || !*ident)
				ident = e_cert_get_subject_name (ecert);

			subject = g_strdup (ident);

			g_object_unref (ecert);
		}
		#else
		GTlsCertificate *tls_cert;

		tls_cert = g_tls_certificate_new_from_pem (cert->data, cert->length, NULL);
		if (!tls_cert) {
			gchar *encoded;

			encoded = g_base64_encode ((const guchar *) cert->data, cert->length);
			if (encoded) {
				GString *pem = g_string_sized_new (cert->length + 60);

				g_string_append (pem, "-----BEGIN CERTIFICATE-----\n");
				g_string_append (pem, encoded);
				g_string_append (pem, "\n-----END CERTIFICATE-----\n");

				tls_cert = g_tls_certificate_new_from_pem (pem->str, pem->len, NULL);

				g_string_free (pem, TRUE);
			}

			g_free (encoded);
		}

		if (tls_cert) {
			subject = g_tls_certificate_get_subject_name (tls_cert);

			g_clear_object (&tls_cert);
		}
		#endif
	}

	cert_bytes = g_bytes_new (cert->data, cert->length);

	gtk_list_store_set (list_store, iter,
		CERT_COLUMN_SUBJECT_STRING, subject,
		CERT_COLUMN_KIND_STRING, kind == CERT_KIND_X509 ? C_("cert-kind", "X.509") : C_("cert-kind", "PGP"),
		CERT_COLUMN_KIND_INT, kind,
		CERT_COLUMN_CERT_BYTES, cert_bytes,
		-1);

	g_clear_pointer (&cert_bytes, g_bytes_unref);
	g_free (subject);
}

static void
cert_add_kind (EContactEditor *editor,
	       enum CertKind kind)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	EContactCert *cert;

	g_return_if_fail (E_IS_CONTACT_EDITOR (editor));
	g_return_if_fail (kind == CERT_KIND_PGP || kind == CERT_KIND_X509);

	tree_view = GTK_TREE_VIEW (e_builder_get_widget (editor->priv->builder, "certs-treeview"));
	g_return_if_fail (tree_view != NULL);

	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	cert = cert_load_for_kind (editor, kind);
	if (cert) {
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		cert_update_row_with_cert (GTK_LIST_STORE (model), &iter, cert, kind);
		e_contact_cert_free (cert);

		gtk_tree_selection_select_iter (selection, &iter);

		object_changed (G_OBJECT (tree_view), editor);
	}
}

static void
cert_add_pgp_btn_clicked_cb (GtkWidget *button,
			     EContactEditor *editor)
{
	g_return_if_fail (E_IS_CONTACT_EDITOR (editor));

	cert_add_kind (editor, CERT_KIND_PGP);
}

static void
cert_add_x509_btn_clicked_cb (GtkWidget *button,
			      EContactEditor *editor)
{
	g_return_if_fail (E_IS_CONTACT_EDITOR (editor));

	cert_add_kind (editor, CERT_KIND_X509);
}

static void
cert_remove_btn_clicked_cb (GtkWidget *button,
			    EContactEditor *editor)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, select;
	gboolean have_select;

	g_return_if_fail (E_IS_CONTACT_EDITOR (editor));

	tree_view = GTK_TREE_VIEW (e_builder_get_widget (editor->priv->builder, "certs-treeview"));
	g_return_if_fail (tree_view != NULL);

	selection = gtk_tree_view_get_selection (tree_view);
	g_return_if_fail (gtk_tree_selection_get_selected (selection, &model, &iter));

	select = iter;
	have_select = gtk_tree_model_iter_next (model, &select);
	if (!have_select) {
		select = iter;
		have_select = gtk_tree_model_iter_previous (model, &select);
	}

	if (have_select)
		gtk_tree_selection_select_iter (selection, &select);

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	object_changed (G_OBJECT (tree_view), editor);
}

static void
cert_load_kind (EContactEditor *editor,
		enum CertKind kind)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	EContactCert *cert;

	g_return_if_fail (E_IS_CONTACT_EDITOR (editor));
	g_return_if_fail (kind == CERT_KIND_PGP || kind == CERT_KIND_X509);

	tree_view = GTK_TREE_VIEW (e_builder_get_widget (editor->priv->builder, "certs-treeview"));
	g_return_if_fail (tree_view != NULL);

	selection = gtk_tree_view_get_selection (tree_view);
	g_return_if_fail (gtk_tree_selection_get_selected (selection, &model, &iter));

	cert = cert_load_for_kind (editor, kind);
	if (cert) {
		cert_update_row_with_cert (GTK_LIST_STORE (model), &iter, cert, kind);
		e_contact_cert_free (cert);

		object_changed (G_OBJECT (tree_view), editor);
	}
}

static void
cert_load_pgp_btn_clicked_cb (GtkWidget *button,
			      EContactEditor *editor)
{
	g_return_if_fail (E_IS_CONTACT_EDITOR (editor));

	cert_load_kind (editor, CERT_KIND_PGP);
}

static void
cert_load_x509_btn_clicked_cb (GtkWidget *button,
			       EContactEditor *editor)
{
	g_return_if_fail (E_IS_CONTACT_EDITOR (editor));

	cert_load_kind (editor, CERT_KIND_X509);
}

static void
cert_save_btn_clicked_cb (GtkWidget *button,
			  EContactEditor *editor)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GBytes *cert_bytes = NULL;
	gint kind = -1;
	GtkWindow *parent;
	GtkFileChooserNative *native;
	GtkFileChooser *file_chooser;
	GError *error = NULL;

	g_return_if_fail (E_IS_CONTACT_EDITOR (editor));

	tree_view = GTK_TREE_VIEW (e_builder_get_widget (editor->priv->builder, "certs-treeview"));
	g_return_if_fail (tree_view != NULL);

	selection = gtk_tree_view_get_selection (tree_view);
	g_return_if_fail (gtk_tree_selection_get_selected (selection, &model, &iter));

	gtk_tree_model_get (model, &iter,
		CERT_COLUMN_KIND_INT, &kind,
		CERT_COLUMN_CERT_BYTES, &cert_bytes,
		-1);

	g_return_if_fail (kind == CERT_KIND_X509 || kind == CERT_KIND_PGP);
	g_return_if_fail (cert_bytes != NULL);

	parent = eab_editor_get_window (EAB_EDITOR (editor));
	native = gtk_file_chooser_native_new (
		kind == CERT_KIND_PGP ? _("Save PGP key") : _("Save X.509 certificate"), parent,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Save"), _("_Cancel"));

	file_chooser = GTK_FILE_CHOOSER (native);
	gtk_file_chooser_set_local_only (file_chooser, TRUE);
	gtk_file_chooser_set_select_multiple (file_chooser, FALSE);

	cert_add_filters_for_kind (file_chooser, kind);

	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG (native)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;

		filename = gtk_file_chooser_get_filename (file_chooser);
		if (!filename) {
			g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, _("Chosen file is not a local file."));
		} else {
			g_file_set_contents (filename, g_bytes_get_data (cert_bytes, NULL), g_bytes_get_size (cert_bytes), &error);
		}

		g_free (filename);
	}

	g_object_unref (native);
	g_bytes_unref (cert_bytes);

	if (error) {
		e_notice (parent, GTK_MESSAGE_ERROR, _("Failed to save certificate: %s"), error->message);
		g_clear_error (&error);
	}
}

static void
init_certs (EContactEditor *editor)
{
	GtkListStore *list_store;
	GtkTreeView *tree_view;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkWidget *certificate_widget;
	GtkWidget *widget;

	tree_view = GTK_TREE_VIEW (e_builder_get_widget (editor->priv->builder, "certs-treeview"));
	g_return_if_fail (tree_view != NULL);

	gtk_tree_view_set_headers_visible (tree_view, FALSE);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (tree_view, column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer, "text", CERT_COLUMN_KIND_STRING);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (tree_view, column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer, "text", CERT_COLUMN_SUBJECT_STRING);

	list_store = gtk_list_store_new (N_CERT_COLUMNS,
		G_TYPE_STRING,		/* CERT_COLUMN_SUBJECT_STRING */
		G_TYPE_STRING,		/* CERT_COLUMN_KIND_STRING */
		G_TYPE_INT,		/* CERT_COLUMN_KIND_INT */
		G_TYPE_BYTES);		/* CERT_COLUMN_CERT_BYTES */

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

	certificate_widget = e_certificate_widget_new ();
	gtk_widget_show (certificate_widget);
	widget = e_builder_get_widget (editor->priv->builder, "cert-preview-scw");
	gtk_container_add (GTK_CONTAINER (widget), certificate_widget);

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	g_signal_connect (selection, "changed", G_CALLBACK (cert_tab_selection_changed_cb), editor);

	widget = e_builder_get_widget (editor->priv->builder, "cert-add-pgp-btn");
	g_signal_connect (widget, "clicked", G_CALLBACK (cert_add_pgp_btn_clicked_cb), editor);

	widget = e_builder_get_widget (editor->priv->builder, "cert-add-x509-btn");
	g_signal_connect (widget, "clicked", G_CALLBACK (cert_add_x509_btn_clicked_cb), editor);

	widget = e_builder_get_widget (editor->priv->builder, "cert-remove-btn");
	g_signal_connect (widget, "clicked", G_CALLBACK (cert_remove_btn_clicked_cb), editor);

	widget = e_builder_get_widget (editor->priv->builder, "cert-load-pgp-btn");
	g_signal_connect (widget, "clicked", G_CALLBACK (cert_load_pgp_btn_clicked_cb), editor);

	widget = e_builder_get_widget (editor->priv->builder, "cert-load-x509-btn");
	g_signal_connect (widget, "clicked", G_CALLBACK (cert_load_x509_btn_clicked_cb), editor);

	widget = e_builder_get_widget (editor->priv->builder, "cert-save-btn");
	g_signal_connect (widget, "clicked", G_CALLBACK (cert_save_btn_clicked_cb), editor);
}

static void
fill_in_certs (EContactEditor *editor)
{
	GtkTreeModel *model;
	GtkListStore *list_store;
	GtkWidget *widget;
	GList *attrs, *link;
	GtkTreeIter iter;
	enum CertKind kind;

	widget = e_builder_get_widget (editor->priv->builder, "certs-treeview");
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
	list_store = GTK_LIST_STORE (model);

	/* Clear */

	gtk_list_store_clear (list_store);

	/* Fill in */

	attrs = e_vcard_get_attributes (E_VCARD (editor->priv->contact));
	for (link = attrs; link; link = g_list_next (link)) {
		EVCardAttribute *attr = link->data;
		EContactCert *cert;
		GString *value;

		if (e_vcard_attribute_has_type (attr, "X509"))
			kind = CERT_KIND_X509;
		else if (e_vcard_attribute_has_type (attr, "PGP"))
			kind = CERT_KIND_PGP;
		else
			continue;

		value = e_vcard_attribute_get_value_decoded (attr);
		if (!value || !value->len) {
			if (value)
				g_string_free (value, TRUE);
			continue;
		}

		cert = e_contact_cert_new ();
		cert->length = value->len;
		cert->data = g_malloc (cert->length);
		memcpy (cert->data, value->str, cert->length);

		gtk_list_store_append (list_store, &iter);

		cert_update_row_with_cert (list_store, &iter, cert, kind);

		e_contact_cert_free (cert);
		g_string_free (value, TRUE);
	}

	if (gtk_tree_model_get_iter_first (model, &iter)) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
		gtk_tree_selection_select_iter (selection, &iter);
	}
}

static void
extract_certs_for_kind (EContactEditor *editor,
			enum CertKind kind,
			EContactField field,
			GtkTreeModel *model)
{
	GtkTreeIter iter;
	gboolean valid;
	EVCard *vcard;
	GList *attrs = NULL, *link;

	if (is_field_supported (editor, field)) {
		valid = gtk_tree_model_get_iter_first (model, &iter);
		while (valid) {
			GBytes *cert_bytes = NULL;
			gint set_kind = -1;

			gtk_tree_model_get (model, &iter,
					    CERT_COLUMN_KIND_INT, &set_kind,
					    CERT_COLUMN_CERT_BYTES, &cert_bytes,
					   -1);

			if (cert_bytes && set_kind == kind) {
				EVCardAttribute *attr;

				attr = e_vcard_attribute_new ("", e_contact_vcard_attribute (field));
				e_vcard_attribute_add_param_with_value (
					attr, e_vcard_attribute_param_new (EVC_TYPE),
					field == E_CONTACT_X509_CERT ? "X509" : "PGP");
				e_vcard_attribute_add_param_with_value (
					attr,
					e_vcard_attribute_param_new (EVC_ENCODING),
					"b");

				e_vcard_attribute_add_value_decoded (attr, g_bytes_get_data (cert_bytes, NULL), g_bytes_get_size (cert_bytes));

				attrs = g_list_prepend (attrs, attr);
			}

			g_clear_pointer (&cert_bytes, g_bytes_unref);

			valid = gtk_tree_model_iter_next (model, &iter);
		}
	}

	attrs = g_list_reverse (attrs);

	vcard = E_VCARD (editor->priv->contact);

	for (link = attrs; link; link = g_list_next (link)) {
		/* takes ownership of the attribute */
		e_vcard_append_attribute (vcard, link->data);
	}

	g_list_free (attrs);
}

static void
extract_certs (EContactEditor *editor)
{
	GtkWidget *widget;
	GtkTreeModel *model;
	GList *attrs, *link;
	EVCard *vcard;

	widget = e_builder_get_widget (editor->priv->builder, "certs-treeview");
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	vcard = E_VCARD (editor->priv->contact);
	attrs = g_list_copy (e_vcard_get_attributes (vcard));

	for (link = attrs; link; link = g_list_next (link)) {
		EVCardAttribute *attr = link->data;

		/* Remove only those types the editor can work with. */
		if ((!e_vcard_attribute_get_name (attr) ||
		     g_ascii_strcasecmp (EVC_KEY, e_vcard_attribute_get_name (attr)) == 0) &&
		    (e_vcard_attribute_has_type (attr, "X509") ||
		     e_vcard_attribute_has_type (attr, "PGP"))) {
			e_vcard_remove_attribute (vcard, attr);
		}
	}

	g_list_free (attrs);

	/* The saved order will always be X.509 first, then PGP */
	extract_certs_for_kind (editor, CERT_KIND_X509, E_CONTACT_X509_CERT, model);
	extract_certs_for_kind (editor, CERT_KIND_PGP, E_CONTACT_PGP_CERT, model);
}

static void
sensitize_certs (EContactEditor *editor)
{
	GtkWidget *widget;

	widget = e_builder_get_widget (editor->priv->builder, "certs-grid");

	gtk_widget_set_sensitive (widget, editor->priv->target_editable && (
		is_field_supported (editor, E_CONTACT_X509_CERT) ||
		is_field_supported (editor, E_CONTACT_PGP_CERT)));

	widget = e_builder_get_widget (editor->priv->builder, "cert-add-pgp-btn");
	gtk_widget_set_sensitive (widget, is_field_supported (editor, E_CONTACT_PGP_CERT));

	widget = e_builder_get_widget (editor->priv->builder, "cert-add-x509-btn");
	gtk_widget_set_sensitive (widget, is_field_supported (editor, E_CONTACT_X509_CERT));

	widget = e_builder_get_widget (editor->priv->builder, "certs-treeview");
	cert_tab_selection_changed_cb (gtk_tree_view_get_selection (GTK_TREE_VIEW (widget)), editor);
}

static void
fill_in_all (EContactEditor *editor)
{
	GtkWidget *focused_widget;
	gpointer weak_pointer;

	/* Widget changes can cause focus widget change, thus remember the current
	   widget and restore it after the fill is done; some fill operations
	   can delete widgets, like the dyntable, thus do the weak_pointer as well.
	*/
	focused_widget = gtk_window_get_focus (eab_editor_get_window (EAB_EDITOR (editor)));
	weak_pointer = focused_widget;
	if (focused_widget)
		g_object_add_weak_pointer (G_OBJECT (focused_widget), &weak_pointer);

	fill_in_source_field (editor);
	fill_in_simple       (editor);
	fill_in_multival     (editor);
	fill_in_email        (editor);
	fill_in_phone        (editor);
	fill_in_sip          (editor);
	fill_in_im           (editor);
	fill_in_address      (editor);
	fill_in_certs        (editor);

	/* Visibility of sections and status of menuitems in the config-menu depend on data
	 * they have to be initialized here instead of init_all() and sensitize_all()
	 */
	configure_visibility (editor);
	config_sensitize_cb (NULL, editor);

	if (weak_pointer) {
		g_object_remove_weak_pointer (G_OBJECT (focused_widget), &weak_pointer);
		gtk_widget_grab_focus (focused_widget);
	}
}

static void
extract_all (EContactEditor *editor)
{
	if (editor->priv->target_client &&
	    editor->priv->contact) {
		EVCardVersion prefer_version;

		prefer_version = e_book_client_get_prefer_vcard_version (editor->priv->target_client);
		if (prefer_version != E_VCARD_VERSION_UNKNOWN) {
			EContact *converted;

			converted = e_contact_convert (editor->priv->contact, prefer_version);
			if (converted) {
				g_clear_object (&editor->priv->contact);
				editor->priv->contact = converted;
			}
		}
	}

	extract_simple  (editor);
	extract_multival(editor);
	extract_email   (editor);
	extract_phone   (editor);
	extract_sip     (editor);
	extract_im      (editor);
	extract_address (editor);
	extract_certs   (editor);
}

static void
sensitize_all (EContactEditor *editor)
{
	GtkWidget *focused_widget;
	gpointer weak_pointer;

	/* Widget changes can cause focus widget change, thus remember the current
	   widget and restore it after the fill is done; some fill operations
	   can delete widgets, like the dyntable, thus do the weak_pointer as well.
	*/
	focused_widget = gtk_window_get_focus (eab_editor_get_window (EAB_EDITOR (editor)));
	weak_pointer = focused_widget;
	if (focused_widget)
		g_object_add_weak_pointer (G_OBJECT (focused_widget), &weak_pointer);

	sensitize_ok      (editor);
	sensitize_simple  (editor);
	sensitize_multival(editor);
	sensitize_email   (editor);
	sensitize_phone   (editor);
	sensitize_sip     (editor);
	sensitize_im      (editor);
	sensitize_address (editor);
	sensitize_certs   (editor);

	if (weak_pointer) {
		g_object_remove_weak_pointer (G_OBJECT (focused_widget), &weak_pointer);
		gtk_widget_grab_focus (focused_widget);
	}
}

static void
init_personal (EContactEditor *editor)
{
	gtk_expander_set_expanded (
				GTK_EXPANDER (e_builder_get_widget (editor->priv->builder, "expander-personal-web")),
				!editor->priv->compress_ui);
	gtk_expander_set_expanded (
				GTK_EXPANDER (e_builder_get_widget (editor->priv->builder, "expander-personal-job")),
				!editor->priv->compress_ui);
	gtk_expander_set_expanded (
				GTK_EXPANDER (e_builder_get_widget (editor->priv->builder, "expander-personal-dates")),
				!editor->priv->compress_ui);
	gtk_expander_set_expanded (
				GTK_EXPANDER (e_builder_get_widget (editor->priv->builder, "expander-personal-misc")),
				!editor->priv->compress_ui);
}

static void
init_all (EContactEditor *editor)
{
	const gchar *contents[] = { "viewport1", "viewport2", "viewport3", "text-comments" };
	gint ii;
	GtkRequisition tab_req, requisition;
	GtkWidget *widget;

	init_simple   (editor);
	init_multival (editor);
	init_email    (editor);
	init_phone    (editor);
	init_sip      (editor);
	init_im       (editor);
	init_personal (editor);
	init_address  (editor);
	init_certs    (editor);
	init_config   (editor);

	/* with so many scrolled windows, we need to
	 * do some manual sizing */
	requisition.width = -1;
	requisition.height = -1;

	for (ii = 0; ii < G_N_ELEMENTS (contents); ii++) {
		widget = e_builder_get_widget (editor->priv->builder, contents[ii]);

		gtk_widget_get_preferred_size (widget, NULL, &tab_req);

		if (tab_req.width > requisition.width)
			requisition.width = tab_req.width;
		if (tab_req.height > requisition.height)
			requisition.height = tab_req.height;
	}

	if (requisition.width > 0 && requisition.height > 0) {
		GtkWidget *window;
		GdkDisplay *display;
		GdkMonitor *monitor;
		GdkRectangle monitor_area;
		gint x = 0, y = 0, width, height;

		window = editor->priv->app;

		gtk_widget_get_preferred_size (window, &tab_req, NULL);
		width = tab_req.width - 320 + 24;
		height = tab_req.height - 240 + 24;

		display = gtk_widget_get_display (window);
		gtk_window_get_position (GTK_WINDOW (window), &x, &y);

		monitor = gdk_display_get_monitor_at_point (display, x, y);
		gdk_monitor_get_workarea (monitor, &monitor_area);

		if (requisition.width > monitor_area.width - width)
			requisition.width = monitor_area.width - width;

		if (requisition.height > monitor_area.height - height)
			requisition.height = monitor_area.height - height;

		if (requisition.width > 0 && requisition.height > 0)
			gtk_window_set_default_size (
				GTK_WINDOW (window),
				width + requisition.width,
				height + requisition.height);
	}

	widget = e_builder_get_widget (editor->priv->builder, "text-comments");
	if (widget)
		e_spell_text_view_attach (GTK_TEXT_VIEW (widget));
}

static void
contact_editor_get_client_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	EClientComboBox *combo_box;
	ConnectClosure *closure = user_data;
	EClient *client;
	GError *error = NULL;

	combo_box = E_CLIENT_COMBO_BOX (source_object);

	client = e_client_combo_box_get_client_finish (
		combo_box, result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (client == NULL);
		g_error_free (error);
	} else {
		EContactEditor *editor;

		editor = g_weak_ref_get (closure->editor_weak_ref);

		if (editor) {
			if (error != NULL) {
				GtkWindow *parent;

				parent = eab_editor_get_window (EAB_EDITOR (editor));

				eab_load_error_dialog (
					GTK_WIDGET (parent), NULL,
					closure->source, error);

				e_source_combo_box_set_active (
					E_SOURCE_COMBO_BOX (combo_box),
					e_client_get_source (E_CLIENT (editor->priv->target_client)));

				g_error_free (error);
			} else {
				/* FIXME Write a private contact_editor_set_target_client(). */
				g_object_set (editor, "target_client", client, NULL);
			}
		}

		g_clear_object (&client);
		g_clear_object (&editor);
	}

	connect_closure_free (closure);
}

static void
source_changed (EClientComboBox *combo_box,
                EContactEditor *editor)
{
	ConnectClosure *closure;
	ESource *target_source;
	ESource *source_source;
	ESource *source;

	source = e_source_combo_box_ref_active (
		E_SOURCE_COMBO_BOX (combo_box));
	g_return_if_fail (source != NULL);

	if (editor->priv->cancellable != NULL) {
		g_cancellable_cancel (editor->priv->cancellable);
		g_object_unref (editor->priv->cancellable);
		editor->priv->cancellable = NULL;
	}

	target_source = e_client_get_source (E_CLIENT (editor->priv->target_client));
	source_source = e_client_get_source (E_CLIENT (editor->priv->source_client));

	if (e_source_equal (target_source, source))
		goto exit;

	if (e_source_equal (source_source, source)) {
		g_object_set (
			editor, "target_client",
			editor->priv->source_client, NULL);
		goto exit;
	}

	editor->priv->cancellable = g_cancellable_new ();

	closure = g_slice_new0 (ConnectClosure);
	closure->editor_weak_ref = e_weak_ref_new (editor);
	closure->source = g_object_ref (source);

	e_client_combo_box_get_client (
		combo_box, source,
		editor->priv->cancellable,
		contact_editor_get_client_cb,
		closure);

exit:
	g_object_unref (source);
}

static void
full_name_editor_closed_cb (GtkWidget *widget,
			    gpointer data)
{
	if (GTK_IS_WIDGET (widget))
		gtk_widget_destroy (widget);
}

static void
full_name_response (GtkDialog *dialog,
                    gint response,
                    EContactEditor *editor)
{
	EContactName *name;
	GtkWidget *fname_widget;
	gint style = 0;
	gboolean editable = FALSE;

	g_object_get (dialog, "editable", &editable, NULL);

	if (editable && response == GTK_RESPONSE_OK) {
		g_object_get (dialog, "name", &name, NULL);

		style = file_as_get_style (editor);

		fname_widget = e_builder_get_widget (
			editor->priv->builder, "entry-fullname");

		if (GTK_IS_ENTRY (fname_widget)) {
			GtkEntry *entry;
			gchar *full_name = e_contact_name_to_string (name);
			const gchar *old_full_name;

			entry = GTK_ENTRY (fname_widget);
			old_full_name = gtk_entry_get_text (entry);

			if (strcmp (full_name, old_full_name))
				gtk_entry_set_text (entry, full_name);
			g_free (full_name);
		}

		e_contact_name_free (editor->priv->name);
		editor->priv->name = name;

		file_as_set_style (editor, style);
	}

	g_signal_handlers_disconnect_by_func (editor, G_CALLBACK (full_name_editor_closed_cb), dialog);

	gtk_widget_destroy (GTK_WIDGET (dialog));
	editor->priv->fullname_dialog = NULL;
}

static void
full_name_clicked (GtkWidget *button,
                   EContactEditor *editor)
{
	GtkDialog *dialog;
	GtkWindow *parent;
	gboolean fullname_supported;

	if (editor->priv->fullname_dialog) {
		gtk_window_present (GTK_WINDOW (editor->priv->fullname_dialog));
		return;
	}

	parent = eab_editor_get_window (EAB_EDITOR (editor));
	dialog = GTK_DIALOG (e_contact_editor_fullname_new (parent, editor->priv->name));
	fullname_supported = is_field_supported (editor, E_CONTACT_FULL_NAME);

	g_object_set (
		dialog, "editable",
		fullname_supported & editor->priv->target_editable, NULL);

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (full_name_response), editor);

	/* Close the fullname dialog if the editor is closed */
	g_signal_connect_swapped (
		editor, "editor_closed",
		G_CALLBACK (full_name_editor_closed_cb), dialog);

	gtk_widget_show (GTK_WIDGET (dialog));
	editor->priv->fullname_dialog = GTK_WIDGET (dialog);
}

static void
categories_response (GtkDialog *dialog,
                     gint response,
                     EContactEditor *editor)
{
	gchar *categories;
	GtkWidget *entry;

	entry = e_builder_get_widget (editor->priv->builder, "entry-categories");

	if (response == GTK_RESPONSE_OK) {
		categories = e_categories_dialog_get_categories (
			E_CATEGORIES_DIALOG (dialog));
		if (GTK_IS_ENTRY (entry))
			gtk_entry_set_text (
				GTK_ENTRY (entry), categories);
		else
			e_contact_set (
				editor->priv->contact,
				E_CONTACT_CATEGORIES,
				categories);
		g_free (categories);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
	editor->priv->categories_dialog = NULL;
}

static void
categories_clicked (GtkWidget *button,
                    EContactEditor *editor)
{
	gchar *categories = NULL;
	GtkDialog *dialog;
	GtkWindow *window;
	GtkWidget *entry = e_builder_get_widget (editor->priv->builder, "entry-categories");

	if (entry && GTK_IS_ENTRY (entry))
		categories = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
	else if (editor->priv->contact)
		categories = e_contact_get (editor->priv->contact, E_CONTACT_CATEGORIES);

	if (editor->priv->categories_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (editor->priv->categories_dialog));
		g_free (categories);
		return;
	}else if (!(dialog = GTK_DIALOG (e_categories_dialog_new (categories)))) {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (editor->priv->app),
			"addressbook:edit-categories", NULL);
		g_free (categories);
		return;
	}

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (categories_response), editor);

	window = GTK_WINDOW (dialog);

	/* Close the category dialog if the editor is closed */
	gtk_window_set_destroy_with_parent (window, TRUE);
	gtk_window_set_modal (window, FALSE);
	gtk_window_set_transient_for (window, eab_editor_get_window (EAB_EDITOR (editor)));

	gtk_widget_show (GTK_WIDGET (dialog));
	g_free (categories);

	editor->priv->categories_dialog = GTK_WIDGET (dialog);
}

static void
image_selected (EContactEditor *editor)
{
	gchar     *file_name;
	GtkWidget *image_chooser;

	if (editor->priv->image_selector)
		file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (editor->priv->image_selector));
	else
		file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (editor->priv->image_selector_native));

	if (!file_name)
		return;

	image_chooser = e_builder_get_widget (editor->priv->builder, "image-chooser");

	g_signal_handlers_block_by_func (
		image_chooser, image_chooser_changed, editor);
	e_image_chooser_set_from_file (
		E_IMAGE_CHOOSER (image_chooser), file_name);
	g_signal_handlers_unblock_by_func (
		image_chooser, image_chooser_changed, editor);

	editor->priv->image_set = TRUE;
	editor->priv->image_changed = TRUE;
	object_changed (G_OBJECT (image_chooser), editor);
}

static void
image_cleared (EContactEditor *editor)
{
	GtkWidget *image_chooser;
	gchar     *file_name;

	image_chooser = e_builder_get_widget (
		editor->priv->builder, "image-chooser");

	file_name = e_icon_factory_get_icon_filename (
		"avatar-default", GTK_ICON_SIZE_DIALOG);

	g_signal_handlers_block_by_func (
		image_chooser, image_chooser_changed, editor);
	e_image_chooser_set_from_file (
		E_IMAGE_CHOOSER (image_chooser), file_name);
	g_signal_handlers_unblock_by_func (
		image_chooser, image_chooser_changed, editor);

	g_free (file_name);

	editor->priv->image_set = FALSE;
	editor->priv->image_changed = TRUE;
	object_changed (G_OBJECT (image_chooser), editor);
}

static void
file_chooser_response (GtkWidget *widget,
                       gint response,
                       EContactEditor *editor)
{
	if (response == GTK_RESPONSE_ACCEPT)
		image_selected (editor);
	else if (response == GTK_RESPONSE_NO)
		image_cleared (editor);
	else if (editor->priv->image_selector_native &&
		 editor->priv->image_set) {
		/* It doesn't support custom buttons, thus ask separately, which is a pita */
		if (e_alert_run_dialog_for_args (GTK_WINDOW (editor->priv->app),
			"addressbook:ask-unset-image", NULL) == GTK_RESPONSE_ACCEPT)
			image_cleared (editor);
	}

	if (editor->priv->image_selector)
		gtk_widget_hide (editor->priv->image_selector);
	else
		g_clear_object (&editor->priv->image_selector_native);
}

static gboolean
file_selector_deleted (GtkWidget *widget)
{
	gtk_widget_hide (widget);
	return TRUE;
}

static void
update_preview_cb (GtkFileChooser *file_chooser,
                   gpointer data)
{
	GtkWidget *preview;
	gchar *filename;
	GdkPixbuf *pixbuf;

	preview = GTK_WIDGET (data);
	filename = gtk_file_chooser_get_preview_filename (file_chooser);
	if (!e_util_can_preview_filename (filename)) {
		gtk_file_chooser_set_preview_widget_active (file_chooser, FALSE);
		g_free (filename);
		return;
	}

	gtk_file_chooser_set_preview_widget_active (file_chooser, TRUE);

	pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 128, 128, NULL);

	if (!pixbuf) {
		gchar *alternate_file;
		alternate_file = e_icon_factory_get_icon_filename (
			"avatar-default", GTK_ICON_SIZE_DIALOG);
		if (alternate_file) {
			pixbuf = gdk_pixbuf_new_from_file_at_size (
				alternate_file, 128, 128, NULL);
			g_free (alternate_file);
		}
	}
	g_free (filename);

	gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
	if (pixbuf)
		g_object_unref (pixbuf);
}

static void
image_clicked (GtkWidget *button,
               EContactEditor *editor)
{
	if (!editor->priv->image_selector && !editor->priv->image_selector_native) {
		GtkImage *preview;
		GtkFileFilter *filter;

		if (e_util_is_running_flatpak ()) {
			editor->priv->image_selector_native = gtk_file_chooser_native_new (
				_("Please select an image for this contact"),
				GTK_WINDOW (editor->priv->app),
				GTK_FILE_CHOOSER_ACTION_OPEN,
				_("_Open"), _("_Cancel"));
		} else {
			editor->priv->image_selector = gtk_file_chooser_dialog_new (
				_("Please select an image for this contact"),
				GTK_WINDOW (editor->priv->app),
				GTK_FILE_CHOOSER_ACTION_OPEN,
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Open"), GTK_RESPONSE_ACCEPT,
				_("_No image"), GTK_RESPONSE_NO,
				NULL);
		}

		filter = gtk_file_filter_new ();
		gtk_file_filter_add_mime_type (filter, "image/*");
		gtk_file_chooser_set_filter (
			editor->priv->image_selector ? GTK_FILE_CHOOSER (editor->priv->image_selector) : GTK_FILE_CHOOSER (editor->priv->image_selector_native),
			filter);

		if (editor->priv->image_selector) {
			preview = GTK_IMAGE (gtk_image_new ());
			gtk_file_chooser_set_preview_widget (
				GTK_FILE_CHOOSER (editor->priv->image_selector),
				GTK_WIDGET (preview));
			g_signal_connect (
				editor->priv->image_selector, "update-preview",
				G_CALLBACK (update_preview_cb), preview);

			gtk_dialog_set_default_response (
				GTK_DIALOG (editor->priv->image_selector),
				GTK_RESPONSE_ACCEPT);

			g_signal_connect (
				editor->priv->image_selector, "response",
				G_CALLBACK (file_chooser_response), editor);

			g_signal_connect_after (
				editor->priv->image_selector, "delete-event",
				G_CALLBACK (file_selector_deleted),
				editor->priv->image_selector);
		} else {
			g_signal_connect (
				editor->priv->image_selector_native, "response",
				G_CALLBACK (file_chooser_response), editor);
		}
	}

	/* Display the dialog */
	if (editor->priv->image_selector) {
		gtk_window_set_modal (GTK_WINDOW (editor->priv->image_selector), TRUE);
		gtk_window_present (GTK_WINDOW (editor->priv->image_selector));
	} else {
		gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (editor->priv->image_selector_native), TRUE);
		gtk_native_dialog_show (GTK_NATIVE_DIALOG (editor->priv->image_selector_native));
	}
}

typedef struct {
	EContactEditor *ce;
	gboolean should_close;
	gchar *new_id;
} EditorCloseStruct;

static void
editor_close_struct_free (EditorCloseStruct *ecs)
{
	if (ecs) {
		g_clear_object (&ecs->ce);
		g_free (ecs->new_id);
		g_slice_free (EditorCloseStruct, ecs);
	}
}

static void
contact_removed_cb (GObject *source_object,
                    GAsyncResult *result,
                    gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	EditorCloseStruct *ecs = user_data;
	EContactEditor *ce = ecs->ce;
	gboolean should_close = ecs->should_close;
	GError *error = NULL;

	e_book_client_remove_contact_finish (book_client, result, &error);

	gtk_widget_set_sensitive (ce->priv->app, TRUE);
	ce->priv->in_async_call = FALSE;

	e_contact_set (ce->priv->contact, E_CONTACT_UID, ecs->new_id);

	eab_editor_contact_deleted (EAB_EDITOR (ce), error, ce->priv->contact);

	ce->priv->is_new_contact = FALSE;

	if (should_close) {
		eab_editor_close (EAB_EDITOR (ce));
	} else {
		ce->priv->changed = FALSE;

		g_object_ref (ce->priv->target_client);
		g_object_unref (ce->priv->source_client);
		ce->priv->source_client = ce->priv->target_client;

		sensitize_all (ce);
	}

	g_clear_error (&error);

	editor_close_struct_free (ecs);
}

static void
contact_added_cb (EBookClient *book_client,
                  const GError *error,
                  const gchar *id,
                  gpointer closure)
{
	EditorCloseStruct *ecs = closure;
	EContactEditor *ce = ecs->ce;
	gboolean should_close = ecs->should_close;

	if (ce->priv->source_client != ce->priv->target_client && !e_client_is_readonly (E_CLIENT (ce->priv->source_client)) &&
	    !error && ce->priv->is_new_contact == FALSE) {
		ecs->new_id = g_strdup (id);
		e_book_client_remove_contact (
			ce->priv->source_client, ce->priv->contact, E_BOOK_OPERATION_FLAG_NONE, NULL, contact_removed_cb, ecs);
		return;
	}

	gtk_widget_set_sensitive (ce->priv->app, TRUE);
	ce->priv->in_async_call = FALSE;

	e_contact_set (ce->priv->contact, E_CONTACT_UID, id);

	eab_editor_contact_added (EAB_EDITOR (ce), error, ce->priv->contact);

	if (!error) {
		ce->priv->is_new_contact = FALSE;

		if (should_close) {
			eab_editor_close (EAB_EDITOR (ce));
		} else {
			ce->priv->changed = FALSE;
			sensitize_all (ce);
		}
	}

	editor_close_struct_free (ecs);
}

static void
contact_modified_cb (EBookClient *book_client,
                     const GError *error,
                     gpointer closure)
{
	EditorCloseStruct *ecs = closure;
	EContactEditor *ce = ecs->ce;
	gboolean should_close = ecs->should_close;

	gtk_widget_set_sensitive (ce->priv->app, TRUE);
	ce->priv->in_async_call = FALSE;

	eab_editor_contact_modified (EAB_EDITOR (ce), error, ce->priv->contact);

	if (!error) {
		if (should_close) {
			eab_editor_close (EAB_EDITOR (ce));
		}
		else {
			ce->priv->changed = FALSE;
			sensitize_all (ce);
		}
	}

	editor_close_struct_free (ecs);
}

static void
contact_modified_ready_cb (GObject *source_object,
                           GAsyncResult *result,
                           gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	GError *error = NULL;

	e_book_client_modify_contact_finish (book_client, result, &error);

	contact_modified_cb (book_client, error, user_data);

	if (error != NULL)
		g_error_free (error);
}

/* Emits the signal to request saving a contact */
static void
real_save_contact (EContactEditor *ce,
                   gboolean should_close)
{
	EShell *shell;
	EditorCloseStruct *ecs;
	ESourceRegistry *registry;

	shell = eab_editor_get_shell (EAB_EDITOR (ce));
	registry = e_shell_get_registry (shell);

	ecs = g_slice_new0 (EditorCloseStruct);
	ecs->ce = g_object_ref (ce);
	ecs->should_close = should_close;

	gtk_widget_set_sensitive (ce->priv->app, FALSE);
	ce->priv->in_async_call = TRUE;

	if (ce->priv->source_client != ce->priv->target_client) {
		/* Two-step move; add to target, then remove from source */
		eab_merging_book_add_contact (
			registry, ce->priv->target_client,
			ce->priv->contact, contact_added_cb, ecs, FALSE);
	} else {
		if (ce->priv->is_new_contact)
			eab_merging_book_add_contact (
				registry, ce->priv->target_client,
				ce->priv->contact, contact_added_cb, ecs, FALSE);
		else if (ce->priv->check_merge)
			eab_merging_book_modify_contact (
				registry, ce->priv->target_client,
				ce->priv->contact, contact_modified_cb, ecs);
		else
			e_book_client_modify_contact (
				ce->priv->target_client, ce->priv->contact, E_BOOK_OPERATION_FLAG_NONE, NULL,
				contact_modified_ready_cb, ecs);
	}
}

static void
save_contact (EContactEditor *ce,
              gboolean should_close)
{
	gchar *uid;
	const gchar *name_entry_string;
	const gchar *file_as_entry_string;
	const gchar *company_name_string;
	GtkWidget *entry_fullname, *entry_file_as, *company_name, *client_combo_box;
	ESource *active_source;

	if (!ce->priv->target_client)
		return;

	client_combo_box = e_builder_get_widget (ce->priv->builder, "client-combo-box");
	active_source = e_source_combo_box_ref_active (E_SOURCE_COMBO_BOX (client_combo_box));
	g_return_if_fail (active_source != NULL);

	if (!e_source_equal (e_client_get_source (E_CLIENT (ce->priv->target_client)), active_source)) {
		e_alert_run_dialog_for_args (
				GTK_WINDOW (ce->priv->app),
				"addressbook:error-still-opening",
				e_source_get_display_name (active_source),
				NULL);
		g_object_unref (active_source);
		return;
	}

	g_object_unref (active_source);

	if (ce->priv->target_editable && e_client_is_readonly (E_CLIENT (ce->priv->source_client))) {
		if (e_alert_run_dialog_for_args (
				GTK_WINDOW (ce->priv->app),
				"addressbook:prompt-move",
				NULL) == GTK_RESPONSE_NO)
			return;
	}

	entry_fullname = e_builder_get_widget (ce->priv->builder, "entry-fullname");
	entry_file_as = gtk_bin_get_child (
		GTK_BIN (e_builder_get_widget (ce->priv->builder, "combo-file-as")));
	company_name = e_builder_get_widget (ce->priv->builder, "entry-company");
	name_entry_string = gtk_entry_get_text (GTK_ENTRY (entry_fullname));
	file_as_entry_string = gtk_entry_get_text (GTK_ENTRY (entry_file_as));
	company_name_string = gtk_entry_get_text (GTK_ENTRY (company_name));

	if (strcmp (company_name_string , "")) {
		if (!strcmp (name_entry_string, ""))
			gtk_entry_set_text (
				GTK_ENTRY (entry_fullname),
				company_name_string);
		if (!strcmp (file_as_entry_string, ""))
			gtk_entry_set_text (
				GTK_ENTRY (entry_file_as),
				company_name_string);
	}

	extract_all (ce);

	if (!e_contact_editor_is_valid (EAB_EDITOR (ce))) {
		uid = e_contact_get (ce->priv->contact, E_CONTACT_UID);
		g_object_unref (ce->priv->contact);
		ce->priv->contact = eab_new_contact_for_book (ce->priv->target_client ? ce->priv->target_client : ce->priv->source_client);
		if (uid) {
			e_contact_set (ce->priv->contact, E_CONTACT_UID, uid);
			g_free (uid);
		}
		return;
	}

	real_save_contact (ce, should_close);
}

static void
e_contact_editor_save_contact (EABEditor *editor,
                               gboolean should_close)
{
	save_contact (E_CONTACT_EDITOR (editor), should_close);
}

/* Closes the dialog box and emits the appropriate signals */
static void
e_contact_editor_close (EABEditor *editor)
{
	EContactEditor *ce = E_CONTACT_EDITOR (editor);

	if (ce->priv->app != NULL) {
		gtk_widget_destroy (ce->priv->app);
		ce->priv->app = NULL;
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
	E_CONTACT_IM_TWITTER,
	E_CONTACT_IM_MATRIX,
	E_CONTACT_PHOTO,
	E_CONTACT_LOGO,
	E_CONTACT_X509_CERT,
	E_CONTACT_CATEGORY_LIST,
	E_CONTACT_BIRTH_DATE,
	E_CONTACT_ANNIVERSARY,
	E_CONTACT_DEATHDATE

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
	EVCardVersion vcard_version;
	GtkWidget *widget;
	gboolean validation_error = FALSE;
	GSList *iter;
	GString *errmsg = g_string_new (_("The contact data is invalid:\n\n"));
	guint year = 0, month = 0, day = 0;

	/* If valid, see if the birthday is a future date */
	widget = e_builder_get_widget (ce->priv->builder, "dateedit-birthday");
	e_split_date_edit_get_ymd (E_SPLIT_DATE_EDIT (widget), &year, &month, &day);
	if (year > 0 && month > 0 && day > 0) {
		GDateTime *dt = g_date_time_new_utc (year, month, day, 0, 0, 0.0);

		if (dt) {
			GDateTime *now = g_date_time_new_now_utc ();

			if (now && g_date_time_compare (dt, now) > 0) {
				g_string_append_printf (
					errmsg, _("“%s” cannot be a future date"),
					e_contact_pretty_name (E_CONTACT_BIRTH_DATE));
				validation_error = TRUE;
			}

			g_clear_pointer (&now, g_date_time_unref);
			g_clear_pointer (&dt, g_date_time_unref);
		}
	}

	if (ce->priv->target_client)
		vcard_version = e_book_client_get_prefer_vcard_version (ce->priv->target_client);
	else
		vcard_version = e_vcard_get_version (E_VCARD (ce->priv->contact));

	if (vcard_version != E_VCARD_VERSION_UNKNOWN && vcard_version <= E_VCARD_VERSION_30) {
		/* pre-vCard 4.0 cannot save partial dates */
		widget = e_builder_get_widget (ce->priv->builder, "dateedit-birthday");
		e_split_date_edit_get_ymd (E_SPLIT_DATE_EDIT (widget), &year, &month, &day);
		if ((year > 0 || month > 0 || day > 0) && (!year || !month || !day)) {
			if (validation_error)
				g_string_append_c (errmsg, '\n');
			g_string_append_printf (
				errmsg, _("“%s” cannot be a partial date"),
				e_contact_pretty_name (E_CONTACT_BIRTH_DATE));
			validation_error = TRUE;
		}

		widget = e_builder_get_widget (ce->priv->builder, "dateedit-anniversary");
		e_split_date_edit_get_ymd (E_SPLIT_DATE_EDIT (widget), &year, &month, &day);
		if ((year > 0 || month > 0 || day > 0) && (!year || !month || !day)) {
			if (validation_error)
				g_string_append_c (errmsg, '\n');
			g_string_append_printf (
				errmsg, _("“%s” cannot be a partial date"),
				e_contact_pretty_name (E_CONTACT_ANNIVERSARY));
			validation_error = TRUE;
		}
	}

	for (iter = ce->priv->required_fields; iter; iter = iter->next) {
		const gchar *field_name = iter->data;
		EContactField  field_id = e_contact_field_id (field_name);

		if (is_non_string_field (field_id)) {
			if (e_contact_get_const (ce->priv->contact, field_id) == NULL) {
				if (validation_error)
					g_string_append_c (errmsg, '\n');
				g_string_append_printf (
					errmsg, _("“%s” is empty"),
					e_contact_pretty_name (field_id));
				validation_error = TRUE;
				break;
			}

		} else {
			const gchar *text;

			text = e_contact_get_const (ce->priv->contact, field_id);

			if (STRING_IS_EMPTY (text)) {
				if (validation_error)
					g_string_append_c (errmsg, '\n');
				g_string_append_printf (
					errmsg, _("“%s” is empty"),
					e_contact_pretty_name (field_id));
				validation_error = TRUE;
				break;
			}

		}
	}

	if (validation_error) {
		e_alert_run_dialog_for_args (
			GTK_WINDOW (ce->priv->app),
			"addressbook:generic-error",
			_("Invalid contact."), errmsg->str, NULL);
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
	return E_CONTACT_EDITOR (editor)->priv->changed;
}

static GtkWindow *
e_contact_editor_get_window (EABEditor *editor)
{
	return GTK_WINDOW (E_CONTACT_EDITOR (editor)->priv->app);
}

static void
file_save_and_close_cb (GtkWidget *widget,
                        EContactEditor *ce)
{
	save_contact (ce, TRUE);
}

static void
file_cancel_cb (GtkWidget *widget,
                EContactEditor *ce)
{
	eab_editor_close (EAB_EDITOR (ce));
}

/* Callback used when the dialog box is destroyed */
static gint
app_delete_event_cb (GtkWidget *widget,
                     GdkEvent *event,
                     gpointer data)
{
	EContactEditor *ce;

	ce = E_CONTACT_EDITOR (data);

	/* if we're saving, don't allow the dialog to close */
	if (ce->priv->in_async_call)
		return TRUE;

	if (ce->priv->changed) {
		switch (eab_prompt_save_dialog (GTK_WINDOW (ce->priv->app))) {
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
show_help_cb (GtkWidget *widget,
              gpointer data)
{
	/* FIXME Pass a proper parent window. */
	e_display_help (NULL, "contacts-usage-add-contact");
}

static GList *
add_to_tab_order (GList *list,
                  GtkBuilder *builder,
                  const gchar *name)
{
	GtkWidget *widget = e_builder_get_widget (builder, name);
	return g_list_prepend (list, widget);
}

static void
setup_tab_order (GtkBuilder *builder)
{
	GtkWidget *container;
	GList *list = NULL;
/*
	container = e_builder_get_widget (builder, "table-contact-editor-general");
 *
	if (container) {
		list = add_to_tab_order (list, builder, "entry-fullname");
		list = add_to_tab_order (list, builder, "entry-jobtitle");
		list = add_to_tab_order (list, builder, "entry-company");
		list = add_to_tab_order (list, builder, "combo-file-as");
		list = add_to_tab_order (list, builder, "entry-phone-1");
		list = add_to_tab_order (list, builder, "entry-phone-2");
		list = add_to_tab_order (list, builder, "entry-phone-3");
		list = add_to_tab_order (list, builder, "entry-phone-4");
 *
		list = add_to_tab_order (list, builder, "entry-email1");
		list = add_to_tab_order (list, builder, "alignment-htmlmail");
		list = add_to_tab_order (list, builder, "entry-web");
		list = add_to_tab_order (list, builder, "entry-homepage");
		list = add_to_tab_order (list, builder, "button-fulladdr");
		list = add_to_tab_order (list, builder, "text-address");
		list = g_list_reverse (list);
		e_container_change_tab_order (GTK_CONTAINER (container), list);
		g_list_free (list);
	}
*/

	container = e_builder_get_widget (builder, "grid-home-address");
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

	container = e_builder_get_widget (builder, "grid-work-address");
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

	container = e_builder_get_widget (builder, "grid-other-address");
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
expand_dyntable (GtkExpander *expander,
		 EContactEditorDynTable *dyntable,
		 gint max_slots)
{
	if (gtk_expander_get_expanded (expander)) {
		e_contact_editor_dyntable_set_show_max (dyntable, max_slots);
	} else {
		e_contact_editor_dyntable_set_show_max (dyntable,
				SLOTS_IN_COLLAPSED_STATE);
	}
}

static void
expander_contact_mail_cb (GObject *object,
                          GParamSpec *param_spec,
                          gpointer user_data)
{
	expand_dyntable (GTK_EXPANDER (object),
			E_CONTACT_EDITOR_DYNTABLE (user_data),
			EMAIL_SLOTS);
}

static void
expander_contact_phone_cb (GObject *object,
                           GParamSpec *param_spec,
                           gpointer user_data)
{
	expand_dyntable (GTK_EXPANDER (object),
			E_CONTACT_EDITOR_DYNTABLE (user_data),
			PHONE_SLOTS);
}

static void
expander_contact_sip_cb (GObject *object,
                         GParamSpec *param_spec,
                         gpointer user_data)
{
	expand_dyntable (GTK_EXPANDER (object),
			E_CONTACT_EDITOR_DYNTABLE (user_data),
			SIP_SLOTS);
}

static void
expander_contact_im_cb (GObject *object,
                        GParamSpec *param_spec,
                        gpointer user_data)
{
	expand_dyntable (GTK_EXPANDER (object),
			E_CONTACT_EDITOR_DYNTABLE (user_data),
			IM_SLOTS);
}

static void
contact_editor_focus_widget_changed_cb (EFocusTracker *focus_tracker,
                                        GParamSpec *param,
                                        EContactEditor *editor)
{
	GtkWidget *widget;

	widget = e_focus_tracker_get_focus (focus_tracker);

	/* there is no problem to call the attach multiple times */
	if (widget)
		e_widget_undo_attach (widget, focus_tracker);
}

static void
e_contact_editor_init (EContactEditor *e_contact_editor)
{
	GtkBuilder *builder;
	EShell *shell;
	EClientCache *client_cache;
	GtkWidget *container;
	GtkWidget *widget, *label, *dyntable;
	GtkEntryCompletion *completion;
	GtkAccelGroup *accel_group;

	e_contact_editor->priv = e_contact_editor_get_instance_private (e_contact_editor);

	/* FIXME The shell should be obtained
	 *       through a constructor property. */
	shell = e_shell_get_default ();
	client_cache = e_shell_get_client_cache (shell);

	e_contact_editor->priv->name = e_contact_name_new ();

	e_contact_editor->priv->contact = NULL;
	e_contact_editor->priv->changed = FALSE;
	e_contact_editor->priv->check_merge = FALSE;
	e_contact_editor->priv->image_set = FALSE;
	e_contact_editor->priv->image_changed = FALSE;
	e_contact_editor->priv->in_async_call = FALSE;
	e_contact_editor->priv->target_editable = TRUE;
	e_contact_editor->priv->fullname_dialog = NULL;
	e_contact_editor->priv->categories_dialog = NULL;
	e_contact_editor->priv->compress_ui = e_shell_get_express_mode (shell);

	/* Make sure custom widget types are available */
	g_type_ensure (E_TYPE_IMAGE_CHOOSER);
	g_type_ensure (E_TYPE_CLIENT_COMBO_BOX);
	g_type_ensure (E_TYPE_CONTACT_EDITOR_DYNTABLE);
	g_type_ensure (E_TYPE_URL_ENTRY);
	g_type_ensure (E_TYPE_SPLIT_DATE_EDIT);

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "contact-editor.ui");

	e_contact_editor->priv->builder = builder;

	setup_tab_order (builder);

	e_contact_editor->priv->app = g_object_new (GTK_TYPE_DIALOG,
		"window-position", GTK_WIN_POS_CENTER,
		"can-focus", FALSE,
		"title", _("Contact Editor"),
		"use-header-bar", e_util_get_use_header_bar (),
		NULL);

	gtk_window_set_type_hint (GTK_WINDOW (e_contact_editor->priv->app), GDK_WINDOW_TYPE_HINT_NORMAL);
	container = gtk_dialog_get_action_area (GTK_DIALOG (e_contact_editor->priv->app));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);
	container = gtk_dialog_get_content_area (GTK_DIALOG (e_contact_editor->priv->app));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);
	widget = e_builder_get_widget (builder, "contact-editor-box");
	gtk_container_add (GTK_CONTAINER (container), widget);

	if (e_util_get_use_header_bar ()) {
		container = gtk_dialog_get_header_bar (GTK_DIALOG (e_contact_editor->priv->app));

		widget = e_builder_get_widget (builder, "button-ok");
		gtk_header_bar_pack_start (GTK_HEADER_BAR (container), widget);

		widget = e_builder_get_widget (builder, "button-config");
		gtk_header_bar_pack_end (GTK_HEADER_BAR (container), widget);

		widget = e_builder_get_widget (builder, "button-help");
		gtk_header_bar_pack_end (GTK_HEADER_BAR (container), widget);
	} else {
		container = gtk_dialog_get_action_area (GTK_DIALOG (e_contact_editor->priv->app));

		widget = e_builder_get_widget (builder, "button-config");
		gtk_box_pack_end (GTK_BOX (container), widget, FALSE, FALSE, 0);
		gtk_widget_set_valign (widget, GTK_ALIGN_FILL);

		widget = e_builder_get_widget (builder, "button-cancel");
		gtk_dialog_add_action_widget (GTK_DIALOG (e_contact_editor->priv->app), widget, GTK_RESPONSE_CANCEL);
		gtk_widget_set_valign (widget, GTK_ALIGN_FILL);

		widget = e_builder_get_widget (builder, "button-ok");
		gtk_dialog_add_action_widget (GTK_DIALOG (e_contact_editor->priv->app), widget, GTK_RESPONSE_OK);
		gtk_widget_set_valign (widget, GTK_ALIGN_FILL);

		widget = e_builder_get_widget (builder, "button-help");
		gtk_button_set_label (GTK_BUTTON (widget), _("_Help"));
		gtk_style_context_remove_class (gtk_widget_get_style_context (widget), "image-button");
		gtk_dialog_add_action_widget (GTK_DIALOG (e_contact_editor->priv->app), widget, GTK_RESPONSE_HELP);
		gtk_widget_set_valign (widget, GTK_ALIGN_FILL);
	}

	accel_group = gtk_accel_group_new ();
	widget = e_builder_get_widget (builder, "button-ok");
	gtk_widget_grab_default (widget);
	gtk_widget_add_accelerator (
		widget, "clicked", accel_group,
		's', GDK_CONTROL_MASK, 0);
	gtk_window_add_accel_group (GTK_WINDOW (e_contact_editor->priv->app), accel_group);
	g_clear_object (&accel_group);

	init_all (e_contact_editor);

	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "button-image");
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (image_clicked), e_contact_editor);
	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "button-fullname");
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (full_name_clicked), e_contact_editor);
	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "button-categories");
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (categories_clicked), e_contact_editor);
	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "client-combo-box");
	e_client_combo_box_set_client_cache (
		E_CLIENT_COMBO_BOX (widget), client_cache);
	g_signal_connect (
		widget, "changed",
		G_CALLBACK (source_changed), e_contact_editor);
	label = e_builder_get_widget (
		e_contact_editor->priv->builder, "where-label");
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "button-ok");
	if (e_util_get_use_header_bar ()) {
		/* Already set in the .ui file, but does not work */
		gtk_style_context_add_class (
			gtk_widget_get_style_context (widget),
			"suggested-action");
	}
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (file_save_and_close_cb), e_contact_editor);
	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "button-cancel");
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (file_cancel_cb), e_contact_editor);
	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "button-help");
	g_signal_connect (
		widget, "clicked",
		G_CALLBACK (show_help_cb), e_contact_editor);

	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "expander-contact-phone");
	dyntable = e_builder_get_widget (
		e_contact_editor->priv->builder, "phone-dyntable");
	g_signal_connect (widget, "notify::expanded",
	                  G_CALLBACK (expander_contact_phone_cb), dyntable);

	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "expander-contact-sip");
	dyntable = e_builder_get_widget (
		e_contact_editor->priv->builder, "sip-dyntable");
	g_signal_connect (widget, "notify::expanded",
	                  G_CALLBACK (expander_contact_sip_cb), dyntable);

	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "expander-contact-im");
	dyntable = e_builder_get_widget (
		e_contact_editor->priv->builder, "im-dyntable");
	g_signal_connect (widget, "notify::expanded",
	                  G_CALLBACK (expander_contact_im_cb), dyntable);
	e_contact_editor_dyntable_set_combo_with_entry (E_CONTACT_EDITOR_DYNTABLE (dyntable), TRUE);

	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "expander-contact-email");
	dyntable = e_builder_get_widget (
		e_contact_editor->priv->builder, "mail-dyntable");
	g_signal_connect (widget, "notify::expanded",
	                  G_CALLBACK (expander_contact_mail_cb), dyntable);

	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "entry-fullname");
	if (widget)
		gtk_widget_grab_focus (widget);

	widget = e_builder_get_widget (
		e_contact_editor->priv->builder, "entry-categories");
	completion = e_category_completion_new ();
	gtk_entry_set_completion (GTK_ENTRY (widget), completion);
	g_object_unref (completion);

	/* Connect to the deletion of the dialog */

	g_signal_connect (
		e_contact_editor->priv->app, "delete_event",
		G_CALLBACK (app_delete_event_cb), e_contact_editor);

	/* set the icon */
	gtk_window_set_icon_name (
		GTK_WINDOW (e_contact_editor->priv->app), "contact-editor");

	gtk_application_add_window (
		GTK_APPLICATION (shell),
		GTK_WINDOW (e_contact_editor->priv->app));
}

static void
e_contact_editor_constructed (GObject *object)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='undo-menubar'>"
		    "<submenu action='undo-menu'>"
		      "<item action='undo'/>"
		      "<item action='redo'/>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry undo_entries[] = {
		{ "undo-menu",
		  NULL,
		  "Undo menu",
		  NULL,
		  NULL,
		  NULL }, /* just a fake undo menu, to get shortcuts working */

		{ "undo",
		  "edit-undo",
		  N_("_Undo"),
		  "<Control>z",
		  N_("Undo"),
		  NULL }, /* Handled by EFocusTracker */

		{ "redo",
		  "edit-redo",
		  N_("_Redo"),
		  "<Control>y",
		  N_("Redo"),
		  NULL } /* Handled by EFocusTracker */
	};

	EContactEditor *editor = E_CONTACT_EDITOR (object);
	EUIAction *action;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_contact_editor_parent_class)->constructed (object);

	editor->priv->focus_tracker = e_focus_tracker_new (GTK_WINDOW (editor->priv->app));
	editor->priv->ui_manager = e_ui_manager_new (NULL);

	gtk_window_add_accel_group (
		GTK_WINDOW (editor->priv->app),
		e_ui_manager_get_accel_group (editor->priv->ui_manager));

	e_signal_connect_notify (
		editor->priv->focus_tracker, "notify::focus",
		G_CALLBACK (contact_editor_focus_widget_changed_cb), editor);

	e_ui_manager_add_actions_with_eui_data (editor->priv->ui_manager, "undo", GETTEXT_PACKAGE,
		undo_entries, G_N_ELEMENTS (undo_entries), editor, eui);

	e_ui_manager_add_action_groups_to_widget (editor->priv->ui_manager, editor->priv->app);

	action = e_ui_manager_get_action (editor->priv->ui_manager, "undo");
	e_focus_tracker_set_undo_action (editor->priv->focus_tracker, action);

	action = e_ui_manager_get_action (editor->priv->ui_manager, "redo");
	e_focus_tracker_set_redo_action (editor->priv->focus_tracker, action);
}

static void
e_contact_editor_dispose (GObject *object)
{
	EContactEditor *e_contact_editor = E_CONTACT_EDITOR (object);

	g_clear_pointer (&e_contact_editor->priv->image_selector, gtk_widget_destroy);
	g_clear_object (&e_contact_editor->priv->image_selector_native);

	g_slist_free_full (
		e_contact_editor->priv->writable_fields,
		(GDestroyNotify) g_free);
	e_contact_editor->priv->writable_fields = NULL;

	g_slist_free_full (
		e_contact_editor->priv->required_fields,
		(GDestroyNotify) g_free);
	e_contact_editor->priv->required_fields = NULL;

	if (e_contact_editor->priv->target_client) {
		g_signal_handler_disconnect (
			e_contact_editor->priv->target_client,
			e_contact_editor->priv->target_editable_id);
	}

	g_clear_pointer (&e_contact_editor->priv->name, e_contact_name_free);

	if (e_contact_editor->priv->focus_tracker) {
		g_signal_handlers_disconnect_by_data (
			e_contact_editor->priv->focus_tracker,
			e_contact_editor);
	}

	g_clear_object (&e_contact_editor->priv->contact);
	g_clear_object (&e_contact_editor->priv->source_client);
	g_clear_object (&e_contact_editor->priv->target_client);
	g_clear_object (&e_contact_editor->priv->builder);
	g_clear_object (&e_contact_editor->priv->ui_manager);
	g_clear_object (&e_contact_editor->priv->cancellable);
	g_clear_object (&e_contact_editor->priv->focus_tracker);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_contact_editor_parent_class)->dispose (object);
}

static void
supported_fields_cb (GObject *source_object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	EContactEditor *ce = user_data;
	gchar *prop_value = NULL;
	GSList *fields;
	gboolean success;
	GError *error = NULL;

	success = e_client_get_backend_property_finish (
		E_CLIENT (book_client), result, &prop_value, &error);

	if (!success)
		prop_value = NULL;

	if (error != NULL) {
		g_warning (
			"%s: Failed to get supported fields: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

	if (!g_slist_find (eab_editor_get_all_editors (), ce)) {
		g_warning (
			"supported_fields_cb called for book that's still "
			"around, but contact editor that's been destroyed.");
		g_free (prop_value);
		return;
	}

	fields = e_client_util_parse_comma_strings (prop_value);

	g_object_set (ce, "writable_fields", fields, NULL);

	g_slist_free_full (fields, (GDestroyNotify) g_free);
	g_free (prop_value);

	eab_editor_show (EAB_EDITOR (ce));

	sensitize_all (ce);
}

static void
required_fields_cb (GObject *source_object,
                    GAsyncResult *result,
                    gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	EContactEditor *ce = user_data;
	gchar *prop_value = NULL;
	GSList *fields;
	gboolean success;
	GError *error = NULL;

	success = e_client_get_backend_property_finish (
		E_CLIENT (book_client), result, &prop_value, &error);

	if (!success)
		prop_value = NULL;

	if (error != NULL) {
		g_warning (
			"%s: Failed to get supported fields: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

	if (!g_slist_find (eab_editor_get_all_editors (), ce)) {
		g_warning (
			"supported_fields_cb called for book that's still "
			"around, but contact editor that's been destroyed.");
		g_free (prop_value);
		return;
	}

	fields = e_client_util_parse_comma_strings (prop_value);

	g_object_set (ce, "required_fields", fields, NULL);

	g_slist_free_full (fields, (GDestroyNotify) g_free);
	g_free (prop_value);
}

EABEditor *
e_contact_editor_new (EShell *shell,
                      EBookClient *book_client,
                      EContact *contact,
                      gboolean is_new_contact,
                      gboolean editable)
{
	EABEditor *editor;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (E_IS_BOOK_CLIENT (book_client), NULL);
	g_return_val_if_fail (E_IS_CONTACT (contact), NULL);

	editor = g_object_new (E_TYPE_CONTACT_EDITOR, "shell", shell, NULL);

	g_object_set (
		editor,
		"source_client", book_client,
		"contact", contact,
		"is_new_contact", is_new_contact,
		"editable", editable,
		NULL);

	return editor;
}

static void
notify_readonly_cb (EBookClient *book_client,
                    GParamSpec *pspec,
                    EContactEditor *ce)
{
	EClient *client;
	gint new_target_editable;
	gboolean changed = FALSE;

	client = E_CLIENT (ce->priv->target_client);
	new_target_editable = !e_client_is_readonly (client);

	if (ce->priv->target_editable != new_target_editable)
		changed = TRUE;

	ce->priv->target_editable = new_target_editable;

	if (changed)
		sensitize_all (ce);
}

static void
e_contact_editor_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	EContactEditor *editor;

	editor = E_CONTACT_EDITOR (object);

	switch (property_id) {
	case PROP_SOURCE_CLIENT: {
		gboolean  writable;
		gboolean  changed = FALSE;
		EBookClient *source_client;

		source_client = E_BOOK_CLIENT (g_value_get_object (value));

		if (source_client == editor->priv->source_client)
			break;

		if (editor->priv->source_client)
			g_object_unref (editor->priv->source_client);

		editor->priv->source_client = source_client;
		g_object_ref (editor->priv->source_client);

		if (!editor->priv->target_client) {
			editor->priv->target_client = editor->priv->source_client;
			g_object_ref (editor->priv->target_client);

			editor->priv->target_editable_id = e_signal_connect_notify (
				editor->priv->target_client, "notify::readonly",
				G_CALLBACK (notify_readonly_cb), editor);

			e_client_get_backend_property (
				E_CLIENT (editor->priv->target_client),
				E_BOOK_BACKEND_PROPERTY_SUPPORTED_FIELDS,
				NULL, supported_fields_cb, editor);

			e_client_get_backend_property (
				E_CLIENT (editor->priv->target_client),
				E_BOOK_BACKEND_PROPERTY_REQUIRED_FIELDS,
				NULL, required_fields_cb, editor);
		}

		writable = !e_client_is_readonly (E_CLIENT (editor->priv->target_client));
		if (writable != editor->priv->target_editable) {
			editor->priv->target_editable = writable;
			changed = TRUE;
		}

		if (changed)
			sensitize_all (editor);

		break;
	}

	case PROP_TARGET_CLIENT: {
		gboolean  writable;
		gboolean  changed = FALSE;
		EBookClient *target_client;

		target_client = E_BOOK_CLIENT (g_value_get_object (value));

		if (target_client == editor->priv->target_client)
			break;

		if (editor->priv->target_client) {
			g_signal_handler_disconnect (
				editor->priv->target_client,
				editor->priv->target_editable_id);
			g_object_unref (editor->priv->target_client);
		}

		editor->priv->target_client = target_client;
		g_object_ref (editor->priv->target_client);

		editor->priv->target_editable_id = e_signal_connect_notify (
			editor->priv->target_client, "notify::readonly",
			G_CALLBACK (notify_readonly_cb), editor);

		e_client_get_backend_property (
			E_CLIENT (editor->priv->target_client),
			E_BOOK_BACKEND_PROPERTY_SUPPORTED_FIELDS,
			NULL, supported_fields_cb, editor);

		e_client_get_backend_property (
			E_CLIENT (editor->priv->target_client),
			E_BOOK_BACKEND_PROPERTY_REQUIRED_FIELDS,
			NULL, required_fields_cb, editor);

		if (!editor->priv->is_new_contact)
			editor->priv->changed = TRUE;

		writable = !e_client_is_readonly (E_CLIENT (editor->priv->target_client));

		if (writable != editor->priv->target_editable) {
			editor->priv->target_editable = writable;
			changed = TRUE;
		}

		if (changed)
			sensitize_all (editor);

		break;
	}

	case PROP_CONTACT:
		if (editor->priv->contact)
			g_object_unref (editor->priv->contact);
		editor->priv->contact = e_contact_duplicate (
			E_CONTACT (g_value_get_object (value)));
		fill_in_all (editor);
		editor->priv->changed = FALSE;
		break;

	case PROP_IS_NEW_CONTACT:
		editor->priv->is_new_contact = g_value_get_boolean (value);
		break;

	case PROP_EDITABLE: {
		gboolean new_value = g_value_get_boolean (value);
		gboolean changed = (editor->priv->target_editable != new_value);

		editor->priv->target_editable = new_value;

		if (changed)
			sensitize_all (editor);
		break;
	}

	case PROP_CHANGED: {
		gboolean new_value = g_value_get_boolean (value);
		gboolean changed = (editor->priv->changed != new_value);

		editor->priv->changed = new_value;

		if (changed)
			sensitize_ok (editor);
		break;
	}
	case PROP_WRITABLE_FIELDS:
		g_slist_free_full (
			editor->priv->writable_fields,
			(GDestroyNotify) g_free);
		editor->priv->writable_fields = g_slist_copy_deep (
			g_value_get_pointer (value),
			(GCopyFunc) g_strdup, NULL);

		sensitize_all (editor);
		break;
	case PROP_REQUIRED_FIELDS:
		g_slist_free_full (
			editor->priv->required_fields,
			(GDestroyNotify) g_free);
		editor->priv->required_fields = g_slist_copy_deep (
			g_value_get_pointer (value),
			(GCopyFunc) g_strdup, NULL);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_contact_editor_get_property (GObject *object,
                               guint property_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	EContactEditor *e_contact_editor;

	e_contact_editor = E_CONTACT_EDITOR (object);

	switch (property_id) {
	case PROP_SOURCE_CLIENT:
		g_value_set_object (value, e_contact_editor->priv->source_client);
		break;

	case PROP_TARGET_CLIENT:
		g_value_set_object (value, e_contact_editor->priv->target_client);
		break;

	case PROP_CONTACT:
		extract_all (e_contact_editor);
		g_value_set_object (value, e_contact_editor->priv->contact);
		break;

	case PROP_IS_NEW_CONTACT:
		g_value_set_boolean (
			value, e_contact_editor->priv->is_new_contact);
		break;

	case PROP_EDITABLE:
		g_value_set_boolean (
			value, e_contact_editor->priv->target_editable);
		break;

	case PROP_CHANGED:
		g_value_set_boolean (
			value, e_contact_editor->priv->changed);
		break;

	case PROP_WRITABLE_FIELDS:
		g_value_set_pointer (value, e_contact_editor->priv->writable_fields);
		break;
	case PROP_REQUIRED_FIELDS:
		g_value_set_pointer (value, e_contact_editor->priv->required_fields);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
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

	window = gtk_widget_get_window (ce->priv->app);

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
	gtk_widget_show (ce->priv->app);
}
