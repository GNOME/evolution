/*
 * Evolution calendar - Main page of the memo editor dialog
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
#include <glade/glade.h>
#include <libedataserverui/e-category-completion.h>
#include <libedataserverui/e-source-combo-box.h>
#include <libedataserverui/e-name-selector.h>
#include <libedataserverui/e-name-selector-entry.h>
#include <libedataserverui/e-name-selector-list.h>
#include <widgets/misc/e-dateedit.h>

#include "common/authentication.h"
#include "e-util/e-dialog-widgets.h"
#include <e-util/e-dialog-utils.h>
#include "e-util/e-categories-config.h"
#include "e-util/e-util-private.h"
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
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	GtkWidget *memo_content;

	EAccountList *accounts;

	/* Generic informative messages placeholder */
	GtkWidget *info_hbox;
	GtkWidget *info_icon;
	GtkWidget *info_string;
	gchar *subscriber_info_text;

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

	GtkWidget *source_selector;

	GList *address_strings;

	ENameSelector *name_selector;
};

static void set_subscriber_info_string (MemoPage *mpage, const gchar *backend_address);
static const gchar * get_recipients (ECalComponent *comp);
static void sensitize_widgets (MemoPage *mpage);
static gboolean memo_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static void memo_page_select_organizer (MemoPage *mpage, const gchar *backend_address);

G_DEFINE_TYPE (MemoPage, memo_page, TYPE_COMP_EDITOR_PAGE)

/* Fills the widgets with default values */
static void
clear_widgets (MemoPage *mpage)
{
	GtkTextBuffer *buffer;
	GtkTextView *view;
	CompEditor *editor;

	/* Summary */
	e_dialog_editable_set (mpage->priv->summary_entry, NULL);

	/* Description */
	view = GTK_TEXT_VIEW (mpage->priv->memo_content);
	buffer = gtk_text_view_get_buffer (view);
	gtk_text_buffer_set_text (buffer, "", 0);

	/* Classification */
	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	comp_editor_set_classification (editor, E_CAL_COMPONENT_CLASS_PRIVATE);

	/* Categories */
	e_dialog_editable_set (mpage->priv->categories, NULL);
}

static void
memo_page_dispose (GObject *object)
{
	MemoPagePrivate *priv;

	priv = MEMO_PAGE_GET_PRIVATE (object);

	g_list_foreach (priv->address_strings, (GFunc) g_free, NULL);
	g_list_free (priv->address_strings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (memo_page_parent_class)->dispose (object);
}

static void
memo_page_finalize (GObject *object)
{
	MemoPagePrivate *priv;

	priv = MEMO_PAGE_GET_PRIVATE (object);

	if (priv->main != NULL) {
		g_object_unref (priv->main);
		priv->main = NULL;
	}

	if (priv->xml) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	g_free (priv->subscriber_info_text);

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
	ECal *client;
	ECalComponentClassification cl;
	ECalComponentText text;
	ECalComponentDateTime d;
	GSList *l;
	const gchar *categories;
	gchar *backend_addr = NULL;

	mpage = MEMO_PAGE (page);
	priv = mpage->priv;

	editor = comp_editor_page_get_editor (page);
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	/* Clean the screen */
	clear_widgets (mpage);

        /* Summary */
	e_cal_component_get_summary (comp, &text);
	e_dialog_editable_set (priv->summary_entry, text.value);

	e_cal_component_get_description_list (comp, &l);
	if (l && l->data) {
		ECalComponentText *dtext;

		dtext = l->data;
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content)),
					  dtext->value ? dtext->value : "", -1);
	} else {
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->memo_content)),
					  "", 0);
	}
	e_cal_component_free_text_list (l);

	/* Start Date. */
	e_cal_component_get_dtstart (comp, &d);
	if (d.value) {
		struct icaltimetype *start_tt = d.value;
		e_date_edit_set_date (E_DATE_EDIT (priv->start_date),
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
	e_dialog_editable_set (priv->categories, categories);

	e_cal_get_cal_address (client, &backend_addr, NULL);
	set_subscriber_info_string (mpage, backend_addr);

	if (e_cal_component_has_organizer (comp)) {
		ECalComponentOrganizer organizer;

		e_cal_component_get_organizer (comp, &organizer);
		if (organizer.value != NULL) {
			const gchar *strip = itip_strip_mailto (organizer.value);
			gchar *string;

			if ( organizer.cn != NULL)
				string = g_strdup_printf ("%s <%s>", organizer.cn, strip);
			else
				string = g_strdup (strip);

			if (itip_organizer_is_user (comp, client) || itip_sentby_is_user (comp, client)) {
				gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->org_combo))), string);
			} else {
				gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->org_combo))));
				gtk_combo_box_append_text (GTK_COMBO_BOX (priv->org_combo), string);
				gtk_combo_box_set_active (GTK_COMBO_BOX (priv->org_combo), 0);
				gtk_editable_set_editable (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->org_combo))), FALSE);
			}
			g_free (string);
		}
	}

	if (backend_addr)
		g_free (backend_addr);

	/* Source */
	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (priv->source_selector),
		e_cal_get_source (client));

	if (priv->to_entry && (flags & COMP_EDITOR_IS_SHARED) && !(flags & COMP_EDITOR_NEW_ITEM))
		gtk_entry_set_text (GTK_ENTRY (priv->to_entry), get_recipients (comp));

	sensitize_widgets (mpage);

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

static void
sensitize_widgets (MemoPage *mpage)
{
	GtkActionGroup *action_group;
	gboolean read_only, sens = FALSE, sensitize;
	CompEditor *editor;
	CompEditorFlags flags;
	MemoPagePrivate *priv;
	ECal *client;

	priv = mpage->priv;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	if (!e_cal_is_read_only (client, &read_only, NULL))
		read_only = TRUE;

	if (flags & COMP_EDITOR_IS_SHARED)
		sens = flags & COMP_EDITOR_USER_ORG;
	else
		sens = TRUE;

	sensitize = (!read_only && sens);

	if (read_only) {
		gchar *tmp = g_strconcat ("<b>", _("Memo cannot be edited, because the selected memo list is read only"), "</b>", NULL);
		memo_page_set_info_string (mpage, GTK_STOCK_DIALOG_INFO, tmp);
		g_free (tmp);
	} else if (!sens) {
		gchar *tmp = g_strconcat ("<b>", _("Memo cannot be fully edited, because you are not the organizer"), "</b>", NULL);
		memo_page_set_info_string (mpage, GTK_STOCK_DIALOG_INFO, tmp);
		g_free (tmp);
	} else {
		memo_page_set_info_string (mpage, priv->subscriber_info_text ? GTK_STOCK_DIALOG_INFO : NULL, priv->subscriber_info_text);
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

	action_group = comp_editor_get_action_group (editor, "individual");
	gtk_action_group_set_sensitive (action_group, sensitize);
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
fill_comp_with_recipients (ENameSelector *name_selector, ECalComponent *comp)
{
	EDestinationStore *destination_store;
	GString *str = NULL;
	GList *l, *destinations;
	ENameSelectorModel *name_selector_model = e_name_selector_peek_model (name_selector);
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	e_name_selector_model_peek_section (name_selector_model, "To",
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
				EBook *book = NULL;
				ENameSelectorDialog *dialog;
				EContactStore *c_store;
				GList *books, *l;
				gchar *uri = e_contact_get (contact, E_CONTACT_BOOK_URI);

				dialog = e_name_selector_peek_dialog (name_selector);
				c_store = dialog->name_selector_model->contact_store;
				books = e_contact_store_get_books (c_store);

				for (l = books; l; l = l->next) {
					EBook *b = l->data;
					if (g_str_equal (uri, e_book_get_uri (b))) {
						book = b;
						break;
					}
				}

				if (book) {
					GList *contacts = NULL;
					EContact *n_con = NULL;
					gchar *qu;
					EBookQuery *query;

					qu = g_strdup_printf ("(is \"full_name\" \"%s\")",
							(gchar *) e_contact_get (contact, E_CONTACT_FULL_NAME));
					query = e_book_query_from_string (qu);

					if (!e_book_get_contacts (book, query, &contacts, NULL)) {
						g_warning ("Could not get contact from the book \n");
					} else {
						des = e_destination_new ();
						n_con = contacts->data;

						e_destination_set_contact (des, n_con, 0);
						list_dests = e_destination_list_get_dests (des);

						g_list_foreach (contacts, (GFunc) g_object_unref, NULL);
						g_list_free (contacts);
					}

					e_book_query_unref (query);
					g_free (qu);
				}
				g_list_free (books);
			} else {
				card_dest.next = NULL;
				card_dest.prev = NULL;
				card_dest.data = destination;
				list_dests = &card_dest;
			}
		}

		for (l = list_dests; l; l = l->next) {
			EDestination *dest = l->data;
			const gchar *name, *attendee = NULL;

			name = e_destination_get_name (dest);

			/* If we couldn't get the attendee prior, get the email address as the default */
			if (attendee == NULL || *attendee == '\0') {
				attendee = e_destination_get_email (dest);
			}

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

static EAccount *
get_current_account (MemoPage *page)
{
	MemoPagePrivate *priv = page->priv;
	EIterator *it;
	const gchar *str;

	str = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->org_combo))));
	if (!str)
		return NULL;

	for (it = e_list_get_iterator((EList *)priv->accounts); e_iterator_is_valid(it); e_iterator_next(it)) {
		EAccount *a = (EAccount *)e_iterator_get(it);
		gchar *full = g_strdup_printf("%s <%s>", a->id->name, a->id->address);

		if (!g_ascii_strcasecmp (full, str)) {
			g_free (full);
			g_object_unref (it);

			return a;
		}

		g_free (full);
	}
	g_object_unref (it);

	return NULL;
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
	ECal *client;
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
	str = e_dialog_editable_get (priv->summary_entry);
	if (!str || strlen (str) == 0)
		e_cal_component_set_summary (comp, NULL);
	else {
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;

		e_cal_component_set_summary (comp, &text);
	}

	if (str) {
		g_free (str);
		str = NULL;
	}

	/* Memo Content */

	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	if (!str || strlen (str) == 0) {
		e_cal_component_set_description_list (comp, NULL);
	}
	else {
		gint idxToUse = 1;
		GSList l;
		ECalComponentText text, sumText;
		gchar *txt, *p;
		gunichar uc;

		for (i = 0, p = str, uc = g_utf8_get_char_validated (p, -1);
		    i < 50 && p && uc < (gunichar)-2;
		    i++, p = g_utf8_next_char (p), uc = g_utf8_get_char_validated (p, -1)) {
			idxToUse = p - str;
			if (uc == '\n' || !uc) {
				p = NULL;
				break;
			}
		}

		if (p)
			idxToUse = p - str;

		if (i == 50 && uc && uc < (gunichar)-2)
			sumText.value = txt = g_strdup_printf ("%.*s...", idxToUse, str);
		else
			sumText.value = txt = g_strndup(str, idxToUse);

		sumText.altrep = NULL;

		text.value = str;
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		e_cal_component_set_description_list (comp, &l);

		g_free(txt);
	}

	if (str)
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
	cat = e_dialog_editable_get (priv->categories);
	str = comp_editor_strip_categories (cat);
	if (cat)
		g_free (cat);

	e_cal_component_set_categories (comp, str);

	if (str)
		g_free (str);

	/* change recipients only when creating new item, after that no such action is available */
	if ((flags & COMP_EDITOR_IS_SHARED) && (flags & COMP_EDITOR_NEW_ITEM) && fill_comp_with_recipients (priv->name_selector, comp)) {
		ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};

		EAccount *a;
		gchar *backend_addr = NULL, *org_addr = NULL, *sentby = NULL;

		e_cal_get_cal_address (client, &backend_addr, NULL);

		/* Find the identity for the organizer or sentby field */
		a = get_current_account (mpage);

		/* Sanity Check */
		if (a == NULL) {
			e_notice (priv->main, GTK_MESSAGE_ERROR,
					_("The organizer selected no longer has an account."));
			return FALSE;
		}

		if (a->id->address == NULL || strlen (a->id->address) == 0) {
			e_notice (priv->main, GTK_MESSAGE_ERROR,
					_("An organizer is required."));
			return FALSE;
		}

		if (!(backend_addr && *backend_addr) || !g_ascii_strcasecmp (backend_addr, a->id->address)) {
			org_addr = g_strdup_printf ("MAILTO:%s", a->id->address);
			organizer.value = org_addr;
			organizer.cn = a->id->name;
		} else {
			org_addr = g_strdup_printf ("MAILTO:%s", backend_addr);
			sentby = g_strdup_printf ("MAILTO:%s", a->id->address);
			organizer.value = org_addr;
			organizer.sentby = sentby;
		}

		e_cal_component_set_organizer (comp, &organizer);

		if (flags & COMP_EDITOR_NEW_ITEM)
			comp_editor_set_needs_send (editor, TRUE);

		g_free (backend_addr);
		g_free (org_addr);
		g_free (sentby);
	}

	return TRUE;
}

void
memo_page_set_show_categories (MemoPage *page, gboolean state)
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
memo_page_set_info_string (MemoPage *mpage, const gchar *icon, const gchar *msg)
{
	MemoPagePrivate *priv;

	priv = mpage->priv;

	gtk_image_set_from_stock (GTK_IMAGE (priv->info_icon), icon, GTK_ICON_SIZE_BUTTON);
	gtk_label_set_markup (GTK_LABEL(priv->info_string), msg);

	if (msg && icon)
		gtk_widget_show (priv->info_hbox);
	else
		gtk_widget_hide (priv->info_hbox);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (MemoPage *mpage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (mpage);
	GtkEntryCompletion *completion;
	MemoPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;

	priv = mpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("memo-page");
	if (!priv->main) {
		g_warning("couldn't find memo-page!");
		return FALSE;
	}

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	   it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups)
		page->accel_group = g_object_ref (accel_groups->data);

	g_object_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);

	priv->info_hbox = GW ("generic-info");
	priv->info_icon = GW ("generic-info-image");
	priv->info_string = GW ("generic-info-msgs");

	priv->org_label = GW ("org-label");
	priv->org_combo = GW ("org-combo");
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->org_combo))));

	priv->to_button = GW ("to-button");
	priv->to_hbox = GW ("to-hbox");

	priv->summary_label = GW ("sum-label");
	priv->summary_entry = GW ("sum-entry");

	priv->start_label = GW ("start-label");
	priv->start_date = GW ("start-date");

	priv->memo_content = GW ("memo_content");

	priv->categories_btn = GW ("categories-button");
	priv->categories = GW ("categories");

	priv->source_selector = GW ("source");

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
source_changed_cb (ESourceComboBox *source_combo_box,
                   MemoPage *mpage)
{
	MemoPagePrivate *priv = mpage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	ESource *source;
	ECal *client;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (mpage)))
		return;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	flags = comp_editor_get_flags (editor);

	source = e_source_combo_box_get_active (source_combo_box);
	client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);

	if (!client || !e_cal_open (client, FALSE, NULL)) {
		GtkWidget *dialog;
		ECal *old_client;

		old_client = comp_editor_get_client (editor);

		if (client)
			g_object_unref (client);

		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (priv->source_selector),
			e_cal_get_source (old_client));

		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 _("Unable to open memos in '%s'."),
						 e_source_peek_name (source));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	} else {
		comp_editor_set_client (editor, client);

		if (client) {
			gchar *backend_addr = NULL;

			e_cal_get_cal_address(client, &backend_addr, NULL);

			if (flags & COMP_EDITOR_IS_SHARED)
				memo_page_select_organizer (mpage, backend_addr);

			set_subscriber_info_string (mpage, backend_addr);
			g_free (backend_addr);
		}

		sensitize_widgets (mpage);
	}
}

static void
set_subscriber_info_string (MemoPage *mpage,
                            const gchar *backend_address)
{
	CompEditor *editor;
	ECal *client;
	ESource *source;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	client = comp_editor_get_client (editor);
	source = e_cal_get_source (client);

	if (e_source_get_property (source, "subscriber")) {
		g_free (mpage->priv->subscriber_info_text);
		/* Translators: This string is used when we are creating a Memo
		   on behalf of some other user */
		mpage->priv->subscriber_info_text = g_markup_printf_escaped (_("You are acting on behalf of %s"), backend_address);
		memo_page_set_info_string (mpage, GTK_STOCK_DIALOG_INFO, mpage->priv->subscriber_info_text);
	} else {
		g_free (mpage->priv->subscriber_info_text);
		mpage->priv->subscriber_info_text = NULL;

		memo_page_set_info_string (mpage, NULL, NULL);
	}
}

static void
summary_changed_cb (GtkEditable *editable,
                    CompEditorPage *page)
{
	CompEditor *editor;
	gchar *summary;

	if (comp_editor_page_get_updating (page))
		return;

	editor = comp_editor_page_get_editor (page);
	summary = e_dialog_editable_get (GTK_WIDGET (editable));
	comp_editor_set_summary (editor, summary);
	g_free (summary);
}

static void
to_button_clicked_cb (GtkButton *button,
                      MemoPage *mpage)
{
	ENameSelectorDialog *name_selector_dialog;

	name_selector_dialog = e_name_selector_peek_dialog (
		mpage->priv->name_selector);
	gtk_widget_show (GTK_WIDGET (name_selector_dialog));
}

/* Hooks the widget signals */
static gboolean
init_widgets (MemoPage *mpage)
{
	MemoPagePrivate *priv = mpage->priv;
	GtkTextBuffer *buffer;
	GtkTextView *view;

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

	/* Categories button */
	g_signal_connect(
		priv->categories_btn, "clicked",
		G_CALLBACK (categories_clicked_cb), mpage);

	/* Source selector */
	g_signal_connect (
		priv->source_selector, "changed",
		G_CALLBACK (source_changed_cb), mpage);

	/* Connect the default signal handler to use to make sure the "changed"
	   field gets set whenever a field is changed. */

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
		priv->source_selector, "changed",
		G_CALLBACK (comp_editor_page_changed), mpage);

	g_signal_connect_swapped (
		priv->start_date, "changed",
		G_CALLBACK (comp_editor_page_changed), mpage);

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

	memo_page_set_show_categories (
		mpage, calendar_config_get_show_categories());

	return TRUE;
}

static GtkWidget *
get_to_entry (ENameSelector *name_selector)
{
	ENameSelectorModel *name_selector_model;
	ENameSelectorEntry *name_selector_entry;

	name_selector_model = e_name_selector_peek_model (name_selector);
	e_name_selector_model_add_section (name_selector_model, "To", _("To"), NULL);
	name_selector_entry = (ENameSelectorEntry *)e_name_selector_peek_section_list (name_selector, "To");

	return GTK_WIDGET (name_selector_entry);
}

static void
memo_page_select_organizer (MemoPage *mpage, const gchar *backend_address)
{
	MemoPagePrivate *priv;
	CompEditor *editor;
	CompEditorFlags flags;
	GList *l;
	ECal *client;
	EAccount *def_account;
	gchar *def_address = NULL;
	const gchar *default_address;
	gboolean subscribed_cal = FALSE;
	ESource *source = NULL;
	const gchar *user_addr = NULL;

	def_account = itip_addresses_get_default();
	if (def_account && def_account->enabled)
		def_address = g_strdup_printf("%s <%s>", def_account->id->name, def_account->id->address);

	priv = mpage->priv;
	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	if (client)
		source = e_cal_get_source (client);
	if (source)
		user_addr = e_source_get_property (source, "subscriber");

	if (user_addr)
		subscribed_cal = TRUE;
	else
		user_addr = (backend_address && *backend_address) ? backend_address : NULL;

	default_address = NULL;
	if (user_addr)
		for (l = priv->address_strings; l != NULL; l = l->next)
			if (g_strrstr ((gchar *) l->data, user_addr) != NULL) {
				default_address = (const gchar *) l->data;
				break;
			}

	if (!default_address && def_account)
		default_address = def_address;

	if (default_address) {
		if (flags & COMP_EDITOR_NEW_ITEM) {
			gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->org_combo))), default_address);
			gtk_widget_set_sensitive (priv->org_combo, !subscribed_cal);
		}
	} else
		g_warning ("No potential organizers!");

	g_free (def_address);
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
	MemoPagePrivate *priv = mpage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	EIterator *it;
	gchar *gladefile;
	EAccount *a;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (mpage));
	flags = comp_editor_get_flags (editor);

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "memo-page.glade",
				      NULL);
	priv->xml = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);

	if (!priv->xml) {
		g_message ("memo_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (mpage)) {
		g_message ("memo_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	if (flags & COMP_EDITOR_IS_SHARED) {
		priv->accounts = itip_addresses_get ();
		for (it = e_list_get_iterator((EList *)priv->accounts);
				e_iterator_is_valid(it);
				e_iterator_next(it)) {
			gchar *full = NULL;

			a = (EAccount *)e_iterator_get(it);

			/* skip disabled accounts */
			if (!a->enabled)
				continue;

			full = g_strdup_printf("%s <%s>", a->id->name, a->id->address);

			priv->address_strings = g_list_append(priv->address_strings, full);
		}

		g_object_unref(it);

		if (priv->address_strings) {
			GList *l;

			for (l = priv->address_strings; l; l = l->next)
				gtk_combo_box_append_text (GTK_COMBO_BOX (priv->org_combo), l->data);

			gtk_combo_box_set_active (GTK_COMBO_BOX (priv->org_combo), 0);
		} else
			g_warning ("No potential organizers!");

		gtk_widget_show (priv->org_label);
		gtk_widget_show (priv->org_combo);

		priv->name_selector = e_name_selector_new ();
		priv->to_entry = get_to_entry (priv->name_selector);
		gtk_container_add ((GtkContainer *)priv->to_hbox, priv->to_entry);
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

GtkWidget *memo_page_create_date_edit (void);

GtkWidget *
memo_page_create_date_edit (void)
{
	GtkWidget *widget;

	widget = comp_editor_new_date_edit (TRUE, FALSE, TRUE);
	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (widget), TRUE);
	gtk_widget_show (widget);

	return widget;
}

GtkWidget *memo_page_create_source_combo_box (void);

GtkWidget *
memo_page_create_source_combo_box (void)
{
	GtkWidget *widget;
	GConfClient *client;
	ESourceList *source_list;

	client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (
		client, "/apps/evolution/memos/sources");

	widget = e_source_combo_box_new (source_list);
	gtk_widget_show (widget);

	g_object_unref (source_list);
	g_object_unref (client);

	return widget;
}
