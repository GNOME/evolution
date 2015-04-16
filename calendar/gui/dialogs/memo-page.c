/*
 * Evolution calendar - Main page of the memo editor dialog
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
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Miguel de Icaza <miguel@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *		Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "../calendar-config.h"
#include "comp-editor.h"
#include "comp-editor-util.h"
#include "e-send-options-utils.h"
#include "memo-page.h"

#define MEMO_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_MEMO_PAGE, MemoPagePrivate))

/* Private part of the MemoPage structure */
struct _MemoPagePrivate {
	GtkBuilder *builder;

	/* Widgets from the UI file */
	GtkWidget *main;

	GtkWidget *memo_content;

	/* Generic informative messages placeholder */
	GtkWidget *info_hbox;
	GtkWidget *info_icon;
	GtkWidget *info_string;

	/* Organizer */
	GtkWidget *org_label;
	GtkWidget *org_combo;

	/* To field */
	GtkWidget *to_button;
	GtkWidget *to_hbox;
	GtkWidget *to_entry;

	/* Summary */
	GtkWidget *summary_label;
	GtkWidget *summary_entry;

	/* Start date */
	GtkWidget *start_label;
	GtkWidget *start_date;

	GtkWidget *categories_btn;
	GtkWidget *categories;

	GtkWidget *client_combo_box;

	gchar **address_strings;
	gchar *fallback_address;

	ENameSelector *name_selector;

	GCancellable *connect_cancellable;
};

static void set_subscriber_info_string (MemoPage *mpage, const gchar *backend_address);
static const gchar * get_recipients (ECalComponent *comp);
static void sensitize_widgets (MemoPage *mpage);
static gboolean memo_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static void memo_page_select_organizer (MemoPage *mpage, const gchar *backend_address);

G_DEFINE_TYPE (MemoPage, memo_page, TYPE_COMP_EDITOR_PAGE)

static gboolean
get_current_identity (MemoPage *page,
                      gchar **name,
                      gchar **mailto)
{
	EShell *shell;
	CompEditor *editor;
	ESourceRegistry *registry;
	GList *list, *iter;
	GtkWidget *entry;
	const gchar *extension_name;
	const gchar *text;
	gboolean match = FALSE;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	shell = comp_editor_get_shell (editor);

	entry = gtk_bin_get_child (GTK_BIN (page->priv->org_combo));
	text = gtk_entry_get_text (GTK_ENTRY (entry));

	if (text == NULL || *text == '\0')
		return FALSE;

	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;

	list = e_source_registry_list_sources (registry, extension_name);

	for (iter = list; !match && iter != NULL; iter = g_list_next (iter)) {
		ESource *source = E_SOURCE (iter->data);
		ESourceMailIdentity *extension;
		const gchar *id_name;
		const gchar *id_address;
		gchar *identity;

		extension = e_source_get_extension (source, extension_name);

		id_name = e_source_mail_identity_get_name (extension);
		id_address = e_source_mail_identity_get_address (extension);

		if (id_name == NULL || id_address == NULL)
			continue;

		identity = g_strdup_printf ("%s <%s>", id_name, id_address);
		match = (g_ascii_strcasecmp (text, identity) == 0);
		g_free (identity);

		if (match && name != NULL)
			*name = g_strdup (id_name);

		if (match && mailto != NULL)
			*mailto = g_strdup_printf ("MAILTO:%s", id_address);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return match;
}

/* Fills the widgets with default values */
static void
clear_widgets (MemoPage *mpage)
{
	GtkTextBuffer *buffer;
	GtkTextView *view;
	CompEditor *editor;

	/* Summary */
	gtk_entry_set_text (GTK_ENTRY (mpage->priv->summary_entry), "");

	/* Description */
	view = GTK_TEXT_VIEW (mpage->priv->memo_content);
	buffer = gtk_text_view_get_buffer (view);
	gtk_text_buffer_set_text (buffer, "", 0);
	e_buffer_tagger_update_tags (view);

	/* Classification */
	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	comp_editor_set_classification (editor, E_CAL_COMPONENT_CLASS_PRIVATE);

	/* Categories */
	gtk_entry_set_text (GTK_ENTRY (mpage->priv->categories), "");
}

static void
memo_page_dispose (GObject *object)
{
	MemoPagePrivate *priv;

	priv = MEMO_PAGE_GET_PRIVATE (object);

	if (priv->connect_cancellable != NULL) {
		g_cancellable_cancel (priv->connect_cancellable);
		g_object_unref (priv->connect_cancellable);
		priv->connect_cancellable = NULL;
	}

	g_strfreev (priv->address_strings);
	priv->address_strings = NULL;

	g_free (priv->fallback_address);
	priv->fallback_address = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (memo_page_parent_class)->dispose (object);
}

static void
memo_page_finalize (GObject *object)
{
	MemoPagePrivate *priv;

	priv = MEMO_PAGE_GET_PRIVATE (object);

	if (priv->name_selector) {
		e_name_selector_cancel_loading (priv->name_selector);
		g_object_unref (priv->name_selector);
		priv->name_selector = NULL;
	}

	if (priv->main != NULL) {
		g_object_unref (priv->main);
		priv->main = NULL;
	}

	if (priv->builder) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (memo_page_parent_class)->finalize (object);
}

static GtkWidget *
memo_page_get_widget (CompEditorPage *page)
{
	MemoPagePrivate *priv = MEMO_PAGE_GET_PRIVATE (page);

	return priv->main;
}

static void
memo_page_focus_main_widget (CompEditorPage *page)
{
	MemoPagePrivate *priv = MEMO_PAGE_GET_PRIVATE (page);

	gtk_widget_grab_focus (priv->summary_entry);
}

static gboolean
memo_page_fill_widgets (CompEditorPage *page,
                        ECalComponent *comp)
{
	MemoPage *mpage;
	MemoPagePrivate *priv;
	CompEditor *editor;
	CompEditorFlags flags;
	ECalClient *client;
	ECalComponentClassification cl;
	ECalComponentText text;
	ECalComponentDateTime d;
	ESourceRegistry *registry;
	EShell *shell;
	GSList *l;
	const gchar *categories;
	gchar *backend_addr = NULL;

	mpage = MEMO_PAGE (page);
	priv = mpage->priv;

	editor = comp_editor_page_get_editor (page);
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);
	shell = comp_editor_get_shell (editor);

	registry = e_shell_get_registry (shell);

	/* Clean the screen */
	clear_widgets (mpage);

        /* Summary */
	e_cal_component_get_summary (comp, &text);
	if (text.value != NULL)
		gtk_entry_set_text (GTK_ENTRY (priv->summary_entry), text.value);
	else
		gtk_entry_set_text (GTK_ENTRY (priv->summary_entry), "");

	e_cal_component_get_description_list (comp, &l);
	if (l && l->data) {
		ECalComponentText *dtext;

		dtext = l->data;
		gtk_text_buffer_set_text (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content)),
			dtext->value ? dtext->value : "", -1);
	} else {
		gtk_text_buffer_set_text (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content)),
			"", 0);
	}
	e_cal_component_free_text_list (l);
	e_buffer_tagger_update_tags (GTK_TEXT_VIEW (priv->memo_content));

	/* Start Date. */
	e_cal_component_get_dtstart (comp, &d);
	if (d.value) {
		struct icaltimetype *start_tt = d.value;
		e_date_edit_set_date (
			E_DATE_EDIT (priv->start_date),
			start_tt->year, start_tt->month,
			start_tt->day);
	} else if (!(flags & COMP_EDITOR_NEW_ITEM))
		e_date_edit_set_time (E_DATE_EDIT (priv->start_date), -1);
	e_cal_component_free_datetime (&d);

	/* Classification. */
	e_cal_component_get_classification (comp, &cl);
	comp_editor_set_classification (editor, cl);

	/* Categories */
	e_cal_component_get_categories (comp, &categories);
	if (categories != NULL)
		gtk_entry_set_text (GTK_ENTRY (priv->categories), categories);
	else
		gtk_entry_set_text (GTK_ENTRY (priv->categories), "");

	e_client_get_backend_property_sync (E_CLIENT (client), CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &backend_addr, NULL, NULL);
	set_subscriber_info_string (mpage, backend_addr);

	if (e_cal_component_has_organizer (comp)) {
		ECalComponentOrganizer organizer;

		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value != NULL) {
			const gchar *strip = itip_strip_mailto (organizer.value);
			gchar *string;

			if (organizer.cn != NULL)
				string = g_strdup_printf ("%s <%s>", organizer.cn, strip);
			else
				string = g_strdup (strip);

			if (itip_organizer_is_user (registry, comp, client) ||
			    itip_sentby_is_user (registry, comp, client)) {
				gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->org_combo))), string);
			} else {
				GtkComboBox *combo_box;
				GtkListStore *list_store;
				GtkTreeModel *model;
				GtkTreeIter iter;

				combo_box = GTK_COMBO_BOX (priv->org_combo);
				model = gtk_combo_box_get_model (combo_box);
				list_store = GTK_LIST_STORE (model);

				gtk_list_store_clear (list_store);
				gtk_list_store_append (list_store, &iter);
				gtk_list_store_set (list_store, &iter, 0, string, -1);
				gtk_combo_box_set_active (combo_box, 0);
				gtk_editable_set_editable (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->org_combo))), FALSE);
			}
			g_free (string);
		}
	}

	if (backend_addr)
		g_free (backend_addr);

	/* Source */
	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (priv->client_combo_box),
		e_client_get_source (E_CLIENT (client)));

	if (priv->to_entry && (flags & COMP_EDITOR_IS_SHARED) && !(flags & COMP_EDITOR_NEW_ITEM))
		gtk_entry_set_text (GTK_ENTRY (priv->to_entry), get_recipients (comp));

	sensitize_widgets (mpage);

	e_widget_undo_reset (priv->summary_entry);
	e_widget_undo_reset (priv->categories);
	e_widget_undo_reset (priv->memo_content);

	return TRUE;
}

static void
memo_page_class_init (MemoPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (MemoPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = memo_page_dispose;
	object_class->finalize = memo_page_finalize;

	editor_page_class = COMP_EDITOR_PAGE_CLASS (class);
	editor_page_class->get_widget = memo_page_get_widget;
	editor_page_class->focus_main_widget = memo_page_focus_main_widget;
	editor_page_class->fill_widgets = memo_page_fill_widgets;
	editor_page_class->fill_component = memo_page_fill_component;
}

static void
memo_page_init (MemoPage *mpage)
{
	mpage->priv = MEMO_PAGE_GET_PRIVATE (mpage);
}

/* returns whether changed info text */
static gboolean
check_starts_in_the_past (MemoPage *mpage)
{
	MemoPagePrivate *priv;
	struct icaltimetype start_tt = icaltime_null_time ();

	if ((comp_editor_get_flags (comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage))) & COMP_EDITOR_NEW_ITEM) == 0)
		return FALSE;

	priv = mpage->priv;

	start_tt.is_date = TRUE;
	if (e_date_edit_get_date (E_DATE_EDIT (priv->start_date), &start_tt.year, &start_tt.month, &start_tt.day) &&
	    comp_editor_test_time_in_the_past (start_tt)) {
		gchar *tmp = g_strconcat ("<b>", _("Memo's start date is in the past"), "</b>", NULL);
		memo_page_set_info_string (mpage, "dialog-warning", tmp);
		g_free (tmp);
	} else {
		memo_page_set_info_string (mpage, NULL, NULL);
	}

	return TRUE;
}

static void
sensitize_widgets (MemoPage *mpage)
{
	GtkActionGroup *action_group;
	gboolean read_only, sens = FALSE, sensitize;
	CompEditor *editor;
	CompEditorFlags flags;
	MemoPagePrivate *priv;
	ECalClient *client;

	priv = mpage->priv;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	read_only = e_client_is_readonly (E_CLIENT (client));

	if (flags & COMP_EDITOR_IS_SHARED)
		sens = flags & COMP_EDITOR_USER_ORG;
	else
		sens = TRUE;

	sensitize = (!read_only && sens);

	if (read_only) {
		gchar *tmp = g_strconcat ("<b>", _("Memo cannot be edited, because the selected memo list is read only"), "</b>", NULL);
		memo_page_set_info_string (mpage, "dialog-information", tmp);
		g_free (tmp);
	} else if (!sens) {
		gchar *tmp = g_strconcat ("<b>", _("Memo cannot be fully edited, because you are not the organizer"), "</b>", NULL);
		memo_page_set_info_string (mpage, "dialog-information", tmp);
		g_free (tmp);
	} else if (!check_starts_in_the_past (mpage)) {
		memo_page_set_info_string (mpage, NULL, NULL);
	}

	/* The list of organizers is set to be non-editable. Otherwise any
	* change in the displayed list causes an 'Account not found' error.
	*/
	gtk_editable_set_editable (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->org_combo))), FALSE);

	gtk_text_view_set_editable (GTK_TEXT_VIEW (priv->memo_content), sensitize);
	gtk_widget_set_sensitive (priv->start_date, sensitize);
	gtk_widget_set_sensitive (priv->categories_btn, !read_only);
	gtk_editable_set_editable (GTK_EDITABLE (priv->categories), !read_only);
	gtk_editable_set_editable (GTK_EDITABLE (priv->summary_entry), sensitize);

	if (flags & COMP_EDITOR_IS_SHARED) {
		if (priv->to_entry) {
			gtk_editable_set_editable (GTK_EDITABLE (priv->to_entry), !read_only);
			gtk_widget_grab_focus (priv->to_entry);
		}
	}

	action_group = comp_editor_get_action_group (editor, "editable");
	gtk_action_group_set_sensitive (action_group, !read_only);

	action_group = comp_editor_get_action_group (editor, "individual");
	gtk_action_group_set_sensitive (action_group, sensitize);

	if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_NO_MEMO_START_DATE)) {
		gtk_widget_hide (priv->start_label);
		gtk_widget_hide (priv->start_date);
	}
}

/* returns empty string rather than NULL because of simplicity of usage */
static const gchar *
get_recipients (ECalComponent *comp)
{
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	g_return_val_if_fail (comp != NULL, "");

	icalcomp = e_cal_component_get_icalcomponent (comp);

	/* first look if we have there such property */
	for (icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	     icalprop;
	     icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY)) {
		const gchar *xname = icalproperty_get_x_name (icalprop);

		if (xname && strcmp (xname, "X-EVOLUTION-RECIPIENTS") == 0)
			break;
	}

	if (icalprop)
		return icalproperty_get_x (icalprop);

	return "";
}

static gboolean
fill_comp_with_recipients (ENameSelector *name_selector,
                           ECalComponent *comp)
{
	EDestinationStore *destination_store;
	GString *str = NULL;
	GList *l, *destinations;
	ENameSelectorModel *name_selector_model = e_name_selector_peek_model (name_selector);
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	e_name_selector_model_peek_section (
		name_selector_model, "To",
		NULL, &destination_store);

	destinations = e_destination_store_list_destinations (destination_store);
	for (l = destinations; l; l = g_list_next (l)) {
		EDestination *destination = l->data, *des = NULL;
		const GList *list_dests = NULL, *l;
		GList card_dest;

		if (e_destination_is_evolution_list (destination)) {
			list_dests = e_destination_list_get_dests (destination);
		} else {
			EContact *contact = e_destination_get_contact (destination);
			/* check if the contact is contact list which is not expanded yet */
			/* we expand it by getting the list again from the server forming the query */
			if (contact && e_contact_get (contact , E_CONTACT_IS_LIST)) {
				EBookClient *book_client = NULL;
				ENameSelectorDialog *dialog;
				ENameSelectorModel *model;
				EContactStore *c_store;
				GSList *clients, *l;
				gchar *uid = e_contact_get (contact, E_CONTACT_BOOK_UID);

				dialog = e_name_selector_peek_dialog (name_selector);
				model = e_name_selector_dialog_peek_model (dialog);
				c_store = e_name_selector_model_peek_contact_store (model);
				clients = e_contact_store_get_clients (c_store);

				for (l = clients; l; l = l->next) {
					EBookClient *b = l->data;
					ESource *source;

					source = e_client_get_source (E_CLIENT (b));

					if (g_strcmp0 (uid, e_source_get_uid (source)) == 0) {
						book_client = b;
						break;
					}
				}

				if (book_client) {
					GSList *contacts = NULL;
					EContact *n_con = NULL;
					gchar *query;

					query = g_strdup_printf (
						"(is \"full_name\" \"%s\")",
						(gchar *) e_contact_get (contact, E_CONTACT_FULL_NAME));

					if (!e_book_client_get_contacts_sync (book_client, query, &contacts, NULL, NULL)) {
						g_warning ("Could not get contact from the book \n");
					} else {
						des = e_destination_new ();
						n_con = contacts->data;

						e_destination_set_contact (des, n_con, 0);
						e_destination_set_client (des, book_client);
						list_dests = e_destination_list_get_dests (des);

						g_slist_foreach (contacts, (GFunc) g_object_unref, NULL);
						g_slist_free (contacts);
					}

					g_free (query);
				}
				g_slist_free (clients);
			} else {
				card_dest.next = NULL;
				card_dest.prev = NULL;
				card_dest.data = destination;
				list_dests = &card_dest;
			}
		}

		for (l = list_dests; l; l = l->next) {
			EDestination *dest = l->data;
			const gchar *attendee;

			/* Get the email address as the default. */
			attendee = e_destination_get_email (dest);

			if (attendee == NULL || *attendee == '\0')
				continue;

			if (!str) {
				str = g_string_new (NULL);
				g_string_prepend (str, attendee);
				continue;
			}
			g_string_prepend_c (str, ';');
			g_string_prepend (str, attendee);
		}
	}

	g_list_free (destinations);

	if (str && *str->str) {
		icalcomp = e_cal_component_get_icalcomponent (comp);
		icalprop = icalproperty_new_x (str->str);
		icalproperty_set_x_name (icalprop, "X-EVOLUTION-RECIPIENTS");
		icalcomponent_add_property (icalcomp, icalprop);

		g_string_free (str, FALSE);
		return TRUE;
	} else
		return FALSE;
}

/* fill_component handler for the memo page */
static gboolean
memo_page_fill_component (CompEditorPage *page,
                          ECalComponent *comp)
{
	MemoPage *mpage;
	MemoPagePrivate *priv;
	CompEditor *editor;
	CompEditorFlags flags;
	ECalClient *client;
	ECalComponentClassification classification;
	ECalComponentDateTime start_date;
	struct icaltimetype start_tt;
	gchar *cat, *str;
	gint i;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;

	mpage = MEMO_PAGE (page);
	priv = mpage->priv;

	editor = comp_editor_page_get_editor (page);
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content));

	/* Summary */
	str = gtk_editable_get_chars (GTK_EDITABLE (priv->summary_entry), 0, -1);
	if (str == NULL || *str == '\0')
		e_cal_component_set_summary (comp, NULL);
	else {
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;

		e_cal_component_set_summary (comp, &text);
	}

	g_free (str);
	str = NULL;

	/* Memo Content */

	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	if (!str || strlen (str) == 0) {
		e_cal_component_set_description_list (comp, NULL);
	}
	else {
		GSList l;
		ECalComponentText text;
		gchar *p;
		gunichar uc;

		for (i = 0, p = str, uc = g_utf8_get_char_validated (p, -1);
		    i < 50 && p && uc < (gunichar) - 2;
		    i++, p = g_utf8_next_char (p), uc = g_utf8_get_char_validated (p, -1)) {
			if (uc == '\n' || !uc) {
				p = NULL;
				break;
			}
		}

		text.value = str;
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		e_cal_component_set_description_list (comp, &l);
	}

	g_free (str);

	/* Dates */
	start_tt = icaltime_null_time ();
	start_tt.is_date = 1;
	start_date.value = &start_tt;
	start_date.tzid = NULL;

	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->start_date))) {
		comp_editor_page_display_validation_error (page, _("Start date is wrong"), priv->start_date);
		return FALSE;
	}
	if (e_date_edit_get_date (E_DATE_EDIT (priv->start_date),
					       &start_tt.year,
					       &start_tt.month,
					       &start_tt.day))
		e_cal_component_set_dtstart (comp, &start_date);
	else
		e_cal_component_set_dtstart (comp, NULL);

	/* Classification. */
	classification = comp_editor_get_classification (editor);
	e_cal_component_set_classification (comp, classification);

	/* Categories */
	cat = gtk_editable_get_chars (GTK_EDITABLE (priv->categories), 0, -1);
	str = comp_editor_strip_categories (cat);
	g_free (cat);

	e_cal_component_set_categories (comp, str);

	g_free (str);

	/* change recipients only when creating new item, after that no such action is available */
	if ((flags & COMP_EDITOR_IS_SHARED) && (flags & COMP_EDITOR_NEW_ITEM) && fill_comp_with_recipients (priv->name_selector, comp)) {
		ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};

		gchar *backend_addr = NULL;
		gchar *backend_mailto = NULL;
		gchar *name = NULL;
		gchar *mailto = NULL;

		e_client_get_backend_property_sync (E_CLIENT (client), CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &backend_addr, NULL, NULL);

		/* Find the identity for the organizer or sentby field */
		if (!get_current_identity (mpage, &name, &mailto)) {
			e_notice (
				priv->main, GTK_MESSAGE_ERROR,
				_("An organizer is required."));
			return FALSE;
		}

		/* Prefer the backend addres if we have one. */
		if (backend_addr != NULL && *backend_addr != '\0') {
			backend_mailto = g_strdup_printf (
				"MAILTO:%s", backend_addr);
			if (g_ascii_strcasecmp (backend_mailto, mailto) == 0) {
				g_free (backend_mailto);
				backend_mailto = NULL;
			}
		}

		if (backend_mailto == NULL) {
			organizer.cn = name;
			organizer.value = mailto;
			name = mailto = NULL;
		} else {
			organizer.value = backend_mailto;
			organizer.sentby = mailto;
			backend_mailto = mailto = NULL;
		}

		e_cal_component_set_organizer (comp, &organizer);

		if (flags & COMP_EDITOR_NEW_ITEM)
			comp_editor_set_needs_send (editor, TRUE);

		g_free (backend_addr);
		g_free (backend_mailto);
		g_free (name);
		g_free (mailto);
	}

	comp_editor_set_needs_send (editor, (flags & COMP_EDITOR_IS_SHARED) != 0 &&
		itip_organizer_is_user (e_shell_get_registry (comp_editor_get_shell (editor)), comp, client));

	return TRUE;
}

void
memo_page_set_show_categories (MemoPage *page,
                               gboolean state)
{
	if (state) {
		gtk_widget_show (page->priv->categories_btn);
		gtk_widget_show (page->priv->categories);
	} else {
		gtk_widget_hide (page->priv->categories_btn);
		gtk_widget_hide (page->priv->categories);
	}
}

/*If the msg has some value set, the icon should always be set */
void
memo_page_set_info_string (MemoPage *mpage,
                           const gchar *icon,
                           const gchar *msg)
{
	MemoPagePrivate *priv;

	priv = mpage->priv;

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->info_icon), icon, GTK_ICON_SIZE_BUTTON);
	gtk_label_set_markup (GTK_LABEL (priv->info_string), msg);

	if (msg && icon)
		gtk_widget_show (priv->info_hbox);
	else
		gtk_widget_hide (priv->info_hbox);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (MemoPage *mpage)
{
	EShell *shell;
	EClientCache *client_cache;
	CompEditor *editor;
	CompEditorPage *page = COMP_EDITOR_PAGE (mpage);
	GtkEntryCompletion *completion;
	MemoPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;
	GtkWidget *parent;

	priv = mpage->priv;

#define GW(name) e_builder_get_widget (priv->builder, name)

	editor = comp_editor_page_get_editor (page);
	shell = comp_editor_get_shell (editor);
	client_cache = e_shell_get_client_cache (shell);

	priv->main = GW ("memo-page");
	if (!priv->main) {
		g_warning ("couldn't find memo-page!");
		return FALSE;
	}

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	 * it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups)
		page->accel_group = g_object_ref (accel_groups->data);

	g_object_ref (priv->main);
	parent = gtk_widget_get_parent (priv->main);
	gtk_container_remove (GTK_CONTAINER (parent), priv->main);

	priv->info_hbox = GW ("generic-info");
	priv->info_icon = GW ("generic-info-image");
	priv->info_string = GW ("generic-info-msgs");

	priv->org_label = GW ("org-label");
	priv->org_combo = GW ("org-combo");
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->org_combo))));
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->org_combo), 0);

	priv->to_button = GW ("to-button");
	priv->to_hbox = GW ("to-hbox");

	priv->summary_label = GW ("sum-label");
	priv->summary_entry = GW ("sum-entry");

	priv->start_label = GW ("start-label");
	priv->start_date = GW ("start-date");

	priv->memo_content = GW ("memo_content");

	priv->categories_btn = GW ("categories-button");
	priv->categories = GW ("categories");

	priv->client_combo_box = GW ("client-combo-box");
	e_client_combo_box_set_client_cache (
		E_CLIENT_COMBO_BOX (priv->client_combo_box), client_cache);
#undef GW

	completion = e_category_completion_new ();
	gtk_entry_set_completion (GTK_ENTRY (priv->categories), completion);
	g_object_unref (completion);

	return (priv->memo_content
		&& priv->categories_btn
		&& priv->categories
		&& priv->start_date);
}

/* Callback used when the categories button is clicked; we must bring up the
 * category list dialog.
 */
static void
categories_clicked_cb (GtkWidget *button,
                       MemoPage *mpage)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (mpage->priv->categories);
	e_categories_config_open_dialog_for_entry (entry);
}

static void
mpage_get_client_cb (GObject *source_object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	EClient *client;
	EClientComboBox *combo_box;
	MemoPage *mpage = user_data;
	CompEditor *editor;
	GError *error = NULL;

	combo_box = E_CLIENT_COMBO_BOX (source_object);

	client = e_client_combo_box_get_client_finish (
		combo_box, result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
		return;
	}

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));

	if (error != NULL) {
		GtkWidget *dialog;
		ECalClient *old_client;

		old_client = comp_editor_get_client (editor);

		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (combo_box),
			e_client_get_source (E_CLIENT (old_client)));

		dialog = gtk_message_dialog_new (
			NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
			"%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_clear_error (&error);
	} else {
		icaltimezone *zone;
		CompEditorFlags flags;
		ECalClient *cal_client = E_CAL_CLIENT (client);

		g_return_if_fail (cal_client != NULL);

		flags = comp_editor_get_flags (editor);
		zone = comp_editor_get_timezone (editor);
		e_cal_client_set_default_timezone (cal_client, zone);

		comp_editor_set_client (editor, cal_client);

		if (client) {
			gchar *backend_addr = NULL;

			e_client_get_backend_property_sync (client, CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &backend_addr, NULL, NULL);

			if (flags & COMP_EDITOR_IS_SHARED)
				memo_page_select_organizer (mpage, backend_addr);

			set_subscriber_info_string (mpage, backend_addr);
			g_free (backend_addr);
		}

		sensitize_widgets (mpage);
	}
}

static void
source_changed_cb (ESourceComboBox *combo_box,
                   MemoPage *mpage)
{
	MemoPagePrivate *priv = mpage->priv;
	ESource *source;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (mpage)))
		return;

	source = e_source_combo_box_ref_active (combo_box);
	/* This is valid when the 'combo_box' is rebuilding its content. */
	if (!source)
		return;

	if (priv->connect_cancellable != NULL) {
		g_cancellable_cancel (priv->connect_cancellable);
		g_object_unref (priv->connect_cancellable);
	}
	priv->connect_cancellable = g_cancellable_new ();

	e_client_combo_box_get_client (
		E_CLIENT_COMBO_BOX (combo_box),
		source, priv->connect_cancellable,
		mpage_get_client_cb, mpage);

	g_object_unref (source);
}

static void
set_subscriber_info_string (MemoPage *mpage,
                            const gchar *backend_address)
{
	if (!check_starts_in_the_past (mpage))
		memo_page_set_info_string (mpage, NULL, NULL);
}

static void
summary_changed_cb (GtkEntry *entry,
                    CompEditorPage *page)
{
	CompEditor *editor;
	const gchar *text;

	if (comp_editor_page_get_updating (page))
		return;

	editor = comp_editor_page_get_editor (page);
	text = gtk_entry_get_text (entry);
	comp_editor_set_summary (editor, text);
}

static void
to_button_clicked_cb (GtkButton *button,
                      MemoPage *mpage)
{
	e_name_selector_show_dialog (
		mpage->priv->name_selector,
		mpage->priv->main);
}

static void
memo_page_start_date_changed_cb (MemoPage *mpage)
{
	check_starts_in_the_past (mpage);
	comp_editor_page_changed (COMP_EDITOR_PAGE (mpage));
}

/* Hooks the widget signals */
static gboolean
init_widgets (MemoPage *mpage)
{
	CompEditor *editor;
	MemoPagePrivate *priv = mpage->priv;
	GtkTextBuffer *buffer;
	GtkTextView *view;
	GtkAction *action;
	gboolean active;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));

	/* Generic informative messages */
	gtk_widget_hide (priv->info_hbox);

	/* Summary */
	g_signal_connect (
		priv->summary_entry, "changed",
		G_CALLBACK (summary_changed_cb), mpage);

	/* Memo Content */
	view = GTK_TEXT_VIEW (priv->memo_content);
	buffer = gtk_text_view_get_buffer (view);
	gtk_text_view_set_wrap_mode (view, GTK_WRAP_WORD);
	e_buffer_tagger_connect (view);

	/* Categories button */
	g_signal_connect (
		priv->categories_btn, "clicked",
		G_CALLBACK (categories_clicked_cb), mpage);

	/* Source selector */
	g_signal_connect (
		priv->client_combo_box, "changed",
		G_CALLBACK (source_changed_cb), mpage);

	/* Connect the default signal handler to use to make sure the "changed"
	 * field gets set whenever a field is changed. */

	/* Belongs to priv->memo_content */
	g_signal_connect_swapped (
		buffer, "changed",
		G_CALLBACK (comp_editor_page_changed), mpage);

	g_signal_connect_swapped (
		priv->categories, "changed",
		G_CALLBACK (comp_editor_page_changed), mpage);

	g_signal_connect_swapped (
		priv->summary_entry, "changed",
		G_CALLBACK (comp_editor_page_changed), mpage);

	g_signal_connect_swapped (
		priv->client_combo_box, "changed",
		G_CALLBACK (comp_editor_page_changed), mpage);

	g_signal_connect_swapped (
		priv->start_date, "changed",
		G_CALLBACK (memo_page_start_date_changed_cb), mpage);

	if (priv->name_selector) {
		ENameSelectorDialog *name_selector_dialog;

		name_selector_dialog = e_name_selector_peek_dialog (priv->name_selector);

		g_signal_connect (
			name_selector_dialog, "response",
			G_CALLBACK (gtk_widget_hide), NULL);
		g_signal_connect (
			priv->to_button, "clicked",
			G_CALLBACK (to_button_clicked_cb), mpage);
		g_signal_connect_swapped (
			priv->to_entry, "changed",
			G_CALLBACK (comp_editor_page_changed), mpage);
	}

	action = comp_editor_get_action (editor, "view-categories");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	memo_page_set_show_categories (mpage, active);

	return TRUE;
}

static GtkWidget *
get_to_entry (ENameSelector *name_selector)
{
	ENameSelectorModel *name_selector_model;
	ENameSelectorEntry *name_selector_entry;

	name_selector_model = e_name_selector_peek_model (name_selector);
	e_name_selector_model_add_section (name_selector_model, "To", _("To"), NULL);
	name_selector_entry = (ENameSelectorEntry *) e_name_selector_peek_section_list (name_selector, "To");

	return GTK_WIDGET (name_selector_entry);
}

static void
memo_page_select_organizer (MemoPage *mpage,
                            const gchar *backend_address)
{
	MemoPagePrivate *priv = mpage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	const gchar *default_address;
	gint ii;

	/* Treat an empty backend address as NULL. */
	if (backend_address != NULL && *backend_address == '\0')
		backend_address = NULL;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	flags = comp_editor_get_flags (editor);

	default_address = priv->fallback_address;

	if (backend_address != NULL) {
		for (ii = 0; priv->address_strings[ii] != NULL; ii++) {
			if (g_strrstr (priv->address_strings[ii], backend_address) != NULL) {
				default_address = priv->address_strings[ii];
				break;
			}
		}
	}

	if (default_address != NULL) {
		if (flags & COMP_EDITOR_NEW_ITEM) {
			gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->org_combo))), default_address);
		}
	} else
		g_warning ("No potential organizers!");
}

/**
 * memo_page_construct:
 * @mpage: An memo page.
 *
 * Constructs an memo page by loading its Glade data.
 *
 * Return value: The same object as @mpage, or NULL if the widgets could not be
 * created.
 **/
MemoPage *
memo_page_construct (MemoPage *mpage)
{
	MemoPagePrivate *priv;
	EShell *shell;
	CompEditor *editor;
	CompEditorFlags flags;
	ESourceRegistry *registry;
	EFocusTracker *focus_tracker;
	EClientCache *client_cache;

	priv = mpage->priv;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	focus_tracker = comp_editor_get_focus_tracker (editor);

	flags = comp_editor_get_flags (editor);
	shell = comp_editor_get_shell (editor);

	registry = e_shell_get_registry (shell);
	client_cache = e_shell_get_client_cache (shell);

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_CLIENT_COMBO_BOX);
	g_type_ensure (E_TYPE_DATE_EDIT);
	g_type_ensure (E_TYPE_SPELL_ENTRY);

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "memo-page.ui");

	if (!get_widgets (mpage)) {
		g_message (
			"memo_page_construct(): "
			"Could not find all widgets in the XML file!");
		return NULL;
	}

	e_spell_text_view_attach (GTK_TEXT_VIEW (priv->memo_content));
	e_widget_undo_attach (priv->summary_entry, focus_tracker);
	e_widget_undo_attach (priv->categories, focus_tracker);
	e_widget_undo_attach (priv->memo_content, focus_tracker);

	if (flags & COMP_EDITOR_IS_SHARED) {
		GtkComboBox *combo_box;
		GtkListStore *list_store;
		GtkTreeModel *model;
		GtkTreeIter iter;
		gint ii;

		combo_box = GTK_COMBO_BOX (priv->org_combo);
		model = gtk_combo_box_get_model (combo_box);
		list_store = GTK_LIST_STORE (model);

		priv->address_strings = itip_get_user_identities (registry);
		priv->fallback_address = itip_get_fallback_identity (registry);

		/* FIXME Could we just use a GtkComboBoxText? */
		for (ii = 0; priv->address_strings[ii] != NULL; ii++) {
			gtk_list_store_append (list_store, &iter);
			gtk_list_store_set (
				list_store, &iter,
				0, priv->address_strings[ii], -1);
		}

		gtk_combo_box_set_active (combo_box, 0);

		gtk_widget_show (priv->org_label);
		gtk_widget_show (priv->org_combo);

		priv->name_selector = e_name_selector_new (client_cache);
		priv->to_entry = get_to_entry (priv->name_selector);
		gtk_container_add ((GtkContainer *) priv->to_hbox, priv->to_entry);
		gtk_widget_show (priv->to_hbox);
		gtk_widget_show (priv->to_entry);
		gtk_widget_show (priv->to_button);

		if (!(flags & COMP_EDITOR_NEW_ITEM)) {
			gtk_widget_set_sensitive (priv->to_button, FALSE);
			gtk_widget_set_sensitive (priv->to_entry, FALSE);
		}
	}

	if (!init_widgets (mpage)) {
		g_message ("memo_page_construct(): "
			"Could not initialize the widgets!");
		return NULL;
	}

	return mpage;
}

/**
 * memo_page_new:
 *
 * Creates a new memo page.
 *
 * Return value: A newly-created task page, or NULL if the page could
 * not be created.
 **/
MemoPage *
memo_page_new (CompEditor *editor)
{
	MemoPage *mpage;

	g_return_val_if_fail (IS_COMP_EDITOR (editor), NULL);

	mpage = g_object_new (TYPE_MEMO_PAGE, "editor", editor, NULL);

	if (!memo_page_construct (mpage)) {
		g_object_unref (mpage);
		g_return_val_if_reached (NULL);
	}

	return mpage;
}
