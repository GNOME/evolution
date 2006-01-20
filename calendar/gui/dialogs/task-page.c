/* Evolution calendar - Main page of the task editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <libedataserverui/e-source-option-menu.h>
#include <misc/e-dateedit.h>
#include <e-util/e-dialog-utils.h>
#include "common/authentication.h"
#include "e-util/e-popup.h"
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-categories-config.h"
#include "e-util/e-util-private.h"
#include "../e-timezone-entry.h"
#include "../calendar-config.h"
#include "comp-editor.h"
#include "comp-editor-util.h"
#include "e-send-options-utils.h"
#include "task-page.h"

#include "../e-meeting-attendee.h"
#include "../e-meeting-store.h"
#include "../e-meeting-list-view.h"
#include "../e-cal-popup.h"


/* Private part of the TaskPage structure */
struct _TaskPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Bonobo Controller for the menu/toolbar */
	BonoboUIComponent *uic;
	
	/* Widgets from the Glade file */
	GtkWidget *main;

	EAccountList *accounts;
	EMeetingAttendee *ia;	
	char *default_address;
	char *user_add;
	ECalComponent *comp;

	/* For meeting/event */
	GtkWidget *calendar_label;
	GtkWidget *org_cal_label;
	GtkWidget *attendee_box;

	/* Lists of attendees */
	GPtrArray *deleted_attendees;
	
	GtkWidget *summary;
	GtkWidget *summary_label;

	GtkWidget *due_date;
	GtkWidget *start_date;
	GtkWidget *timezone;
	GtkWidget *timezone_label;
	
	GtkWidget *description;

	ECalComponentClassification classification;

	GtkWidget *categories_btn;
	GtkWidget *categories;

	GtkWidget *source_selector;
	GtkWidget *sendoptions_frame;
	GtkWidget *sendoptions_button;

	/* Meeting related items */
	GtkWidget *list_box;
	GtkWidget *organizer_table;
	GtkWidget *organizer;
	GtkWidget *add;
	GtkWidget *remove;
	GtkWidget *edit;
	GtkWidget *invite;
	GtkWidget *attendees_label;

	/* ListView stuff */
	EMeetingStore *model;
	ECal	  *client;
	EMeetingListView *list_view;
	gint row;

	/* For handling who the organizer is */
	gboolean user_org;
	gboolean existing;
	
	gboolean updating;
	gboolean sendoptions_shown;
	gboolean is_assignment;

	ESendOptionsDialog *sod;
};

static const int classification_map[] = {
	E_CAL_COMPONENT_CLASS_PUBLIC,
	E_CAL_COMPONENT_CLASS_PRIVATE,
	E_CAL_COMPONENT_CLASS_CONFIDENTIAL,
	-1
};



static void task_page_finalize (GObject *object);

static GtkWidget *task_page_get_widget (CompEditorPage *page);
static void task_page_focus_main_widget (CompEditorPage *page);
static gboolean task_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean task_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static gboolean task_page_fill_timezones (CompEditorPage *page, GHashTable *timezones);
static void task_page_set_summary (CompEditorPage *page, const char *summary);
static void task_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);

G_DEFINE_TYPE (TaskPage, task_page, TYPE_COMP_EDITOR_PAGE);

/* Class initialization function for the task page */
static void
task_page_class_init (TaskPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GObjectClass *) class;

	editor_page_class->get_widget = task_page_get_widget;
	editor_page_class->focus_main_widget = task_page_focus_main_widget;
	editor_page_class->fill_widgets = task_page_fill_widgets;
	editor_page_class->fill_component = task_page_fill_component;
	editor_page_class->fill_timezones = task_page_fill_timezones;
	editor_page_class->set_summary = task_page_set_summary;
	editor_page_class->set_dates = task_page_set_dates;

	object_class->finalize = task_page_finalize;
}

/* Object initialization function for the task page */
static void
task_page_init (TaskPage *tpage)
{
	TaskPagePrivate *priv;

	priv = g_new0 (TaskPagePrivate, 1);
	tpage->priv = priv;

	priv->xml = NULL;
	priv->uic = NULL;

	priv->main = NULL;
	priv->summary = NULL;
	priv->summary_label = NULL;
	priv->due_date = NULL;
	priv->start_date = NULL;
	priv->timezone = NULL;
	priv->description = NULL;
	priv->classification = E_CAL_COMPONENT_CLASS_NONE;
	priv->categories_btn = NULL;
	priv->categories = NULL;
	priv->sendoptions_frame = NULL;
	priv->sendoptions_button = NULL;
	priv->sendoptions_shown = FALSE;
	priv->sod = NULL;

	priv->updating = FALSE;
	priv->sendoptions_shown = FALSE;
	priv->is_assignment = FALSE;
	
	priv->deleted_attendees = g_ptr_array_new ();

	priv->comp = NULL;

	priv->accounts = NULL;
	priv->ia = NULL;
	priv->default_address = NULL;
	priv->invite = NULL;
	
	priv->model = NULL;
	priv->list_view = NULL;	
	priv->default_address = NULL;
}

static void
cleanup_attendees (GPtrArray *attendees)
{
	int i;
	
	for (i = 0; i < attendees->len; i++)
		g_object_unref (g_ptr_array_index (attendees, i));
}

/* Destroy handler for the task page */
static void
task_page_finalize (GObject *object)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_TASK_PAGE (object));

	tpage = TASK_PAGE (object);
	priv = tpage->priv;

	if (priv->main)
		gtk_widget_unref (priv->main);

	if (priv->xml) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}
	
	if (priv->sod) {
		g_object_unref (priv->sod);
		priv->sod = NULL;
	}

	if (priv->comp != NULL)
		g_object_unref (priv->comp);
 
	cleanup_attendees (priv->deleted_attendees);
	g_ptr_array_free (priv->deleted_attendees, TRUE);

	g_free (priv);
	tpage->priv = NULL;

	if (G_OBJECT_CLASS (task_page_parent_class)->finalize)
		(* G_OBJECT_CLASS (task_page_parent_class)->finalize) (object);
}

static void
set_classification_menu (TaskPage *page, gint class)
{
	bonobo_ui_component_freeze (page->priv->uic, NULL);
	switch (class) {
		case E_CAL_COMPONENT_CLASS_PUBLIC:
			bonobo_ui_component_set_prop (
				page->priv->uic, "/commands/ActionClassPublic",
				"state", "1", NULL);
			break;
		case E_CAL_COMPONENT_CLASS_CONFIDENTIAL:
			bonobo_ui_component_set_prop (
				page->priv->uic, "/commands/ActionClassConfidential",
				"state", "1", NULL);
			break;
		case E_CAL_COMPONENT_CLASS_PRIVATE:
			bonobo_ui_component_set_prop (
				page->priv->uic, "/commands/ActionClassPrivate",
				"state", "1", NULL);
			break;
	}
	bonobo_ui_component_thaw (page->priv->uic, NULL);
}



/* get_widget handler for the task page */
static GtkWidget *
task_page_get_widget (CompEditorPage *page)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	return priv->main;
}

/* focus_main_widget handler for the task page */
static void
task_page_focus_main_widget (CompEditorPage *page)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	gtk_widget_grab_focus (priv->summary);
}

/* Fills the widgets with default values */
static void
clear_widgets (TaskPage *tpage)
{
	TaskPagePrivate *priv;

	priv = tpage->priv;

	/* Summary, description */
	e_dialog_editable_set (priv->summary, NULL);
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)), "", 0);

	/* Start, due times */
	e_date_edit_set_time (E_DATE_EDIT (priv->start_date), 0);
	e_date_edit_set_time (E_DATE_EDIT (priv->due_date), 0);

	/* Classification */
	priv->classification = E_CAL_COMPONENT_CLASS_PRIVATE;
	set_classification_menu (tpage, priv->classification);

	/* Categories */
	e_dialog_editable_set (priv->categories, NULL);
	
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->organizer)->entry), priv->default_address);
	
}

void
task_page_set_view_role (TaskPage *page, gboolean state)
{
	TaskPagePrivate *priv = page->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, "Role", state);
}

void
task_page_set_view_status (TaskPage *page, gboolean state)
{
	TaskPagePrivate *priv = page->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, "Status", state);
}

void
task_page_set_view_type (TaskPage *page, gboolean state)
{
	TaskPagePrivate *priv = page->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, "Type", state);
}

void
task_page_set_view_rsvp (TaskPage *page, gboolean state)
{
	TaskPagePrivate *priv = page->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, "RSVP", state);
}

void 
task_page_set_classification (TaskPage *page, ECalComponentClassification class)
{
	page->priv->classification = class;
}

static void
sensitize_widgets (TaskPage *tpage)
{
	gboolean read_only, sens, sensitize;
	TaskPagePrivate *priv;
	
	priv = tpage->priv;

	if (!e_cal_is_read_only (COMP_EDITOR_PAGE (tpage)->client, &read_only, NULL))
		read_only = TRUE;
	
	if (COMP_EDITOR_PAGE (tpage)->flags & COMP_EDITOR_IS_ASSIGNED)
	 	sens = COMP_EDITOR_PAGE (tpage)->flags & COMP_EDITOR_PAGE_USER_ORG;

	sensitize = (!read_only && sens);

	gtk_entry_set_editable (GTK_ENTRY (priv->summary), !read_only);
	gtk_widget_set_sensitive (priv->due_date, !read_only);
	gtk_widget_set_sensitive (priv->start_date, !read_only);
	gtk_widget_set_sensitive (priv->timezone, !read_only);
	gtk_widget_set_sensitive (priv->description, !read_only);
	gtk_widget_set_sensitive (priv->categories_btn, !read_only);
	gtk_widget_set_sensitive (priv->sendoptions_button, !read_only);
	gtk_entry_set_editable (GTK_ENTRY (priv->categories), !read_only);

	gtk_widget_set_sensitive (priv->organizer, !read_only);
	gtk_widget_set_sensitive (priv->add, (!read_only &&  sens));
	gtk_widget_set_sensitive (priv->edit, (!read_only && sens));
	e_meeting_list_view_set_editable (priv->list_view, (!read_only && sens));
	gtk_widget_set_sensitive (priv->remove, (!read_only &&  sens));
	gtk_widget_set_sensitive (priv->invite, (!read_only &&  sens));
	gtk_widget_set_sensitive (GTK_WIDGET (priv->list_view), !read_only);	

	bonobo_ui_component_set_prop (priv->uic, "/commands/InsertAttachments", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ViewTimeZone", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionClassPublic", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionClassPrivate", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionClassConfidential", "sensitive",
		       	sensitize ? "1" : "0", NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ViewCategories", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/InsertSendOptions", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/OptionStatus", "sensitive", sensitize ? "1" : "0"
			, NULL);

	
	if (!priv->is_assignment) {
		gtk_widget_hide (priv->calendar_label);
		gtk_widget_hide (priv->list_box);
		gtk_widget_hide (priv->attendee_box);
		gtk_widget_hide (priv->organizer);
		gtk_widget_hide (priv->invite);
		gtk_label_set_text_with_mnemonic ((GtkLabel *) priv->org_cal_label, _("_Group"));
	} else {
		gtk_widget_show (priv->invite);	
		gtk_widget_show (priv->calendar_label);
		gtk_widget_show (priv->list_box);
		gtk_widget_show (priv->attendee_box);
		gtk_widget_show (priv->organizer);
		gtk_label_set_text_with_mnemonic ((GtkLabel *) priv->org_cal_label, _("Or_ganizer"));		
	}
}
void
task_page_hide_options (TaskPage *page)
{
	g_return_if_fail (IS_TASK_PAGE (page));


	bonobo_ui_component_set_prop (page->priv->uic, "/commands/InsertSendOptions", "hidden", "1", NULL);
	page->priv->sendoptions_shown = FALSE;
	
}
void
task_page_show_options (TaskPage *page)
{
	g_return_if_fail (IS_TASK_PAGE (page));
	
	bonobo_ui_component_set_prop (page->priv->uic, "/commands/InsertSendOptions", "hidden", "0", NULL);
	page->priv->sendoptions_shown = TRUE;
}

void
task_page_set_assignment (TaskPage *page, gboolean set)
{
	g_return_if_fail (IS_TASK_PAGE (page));

	page->priv->is_assignment = set;
	sensitize_widgets (page);
}

static EAccount *
get_current_account (TaskPage *page)
{	
	TaskPagePrivate *priv;
	EIterator *it;
	const char *str;
	
	priv = page->priv;

	str = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->organizer)->entry));
	if (!str)
		return NULL;
	
	for (it = e_list_get_iterator((EList *)priv->accounts); e_iterator_is_valid(it); e_iterator_next(it)) {
		EAccount *a = (EAccount *)e_iterator_get(it);
		char *full = g_strdup_printf("%s <%s>", a->id->name, a->id->address);

		if (!strcmp (full, str)) {
			g_free (full);
			g_object_unref (it);

			return a;
		}
	
		g_free (full);
	}
	g_object_unref (it);
	
	return NULL;	
}

/* fill_widgets handler for the task page */
static gboolean
task_page_fill_widgets (CompEditorPage *page, ECalComponent *comp)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	ECalComponentText text;
	ECalComponentDateTime d;
	ECalComponentClassification cl;
	GSList *l;
	icalcomponent *icalcomp;
	const char *categories, *uid;
	icaltimezone *zone, *default_zone;
	ESource *source;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	priv->updating = TRUE;

	/* Clean out old data */
	if (priv->comp != NULL)
		g_object_unref (priv->comp);
	priv->comp = NULL;
	
	cleanup_attendees (priv->deleted_attendees);
	g_ptr_array_set_size (priv->deleted_attendees, 0);
	
	/* Component for cancellation */
	priv->comp = e_cal_component_clone (comp);

	/* Clean the screen */
	clear_widgets (tpage);

	priv->user_add = itip_get_comp_attendee (comp, page->client);
	
        /* Summary, description(s) */
	e_cal_component_get_summary (comp, &text);
	e_dialog_editable_set (priv->summary, text.value);

	e_cal_component_get_description_list (comp, &l);
	if (l && l->data) {
		ECalComponentText *dtext;
		
		dtext = l->data;
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)),
					  dtext->value ? dtext->value : "", -1);
	} else {
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)),
					  "", 0);
	}
	e_cal_component_free_text_list (l);

	default_zone = calendar_config_get_icaltimezone ();

	/* Due Date. */
	e_cal_component_get_due (comp, &d);
	zone = NULL;
	if (d.value) {
		struct icaltimetype *due_tt = d.value;
		e_date_edit_set_date (E_DATE_EDIT (priv->due_date),
				      due_tt->year, due_tt->month,
				      due_tt->day);
		if (due_tt->is_date) {
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->due_date),
						     -1, -1);
		} else {
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->due_date),
						     due_tt->hour,
						     due_tt->minute);
		}
	} else {
		e_date_edit_set_time (E_DATE_EDIT (priv->due_date), -1);

		/* If no time is set, we use the default timezone, so the
		   user usually doesn't have to set this when they set the
		   date. */
		zone = NULL;
	}

	/* Note that if we are creating a new task, the timezones may not be
	   on the server, so we try to get the builtin timezone with the TZID
	   first. */
	if (!zone && d.tzid) {
		if (!e_cal_get_timezone (page->client, d.tzid, &zone, NULL))
			/* FIXME: Handle error better. */
			g_warning ("Couldn't get timezone from server: %s",
				   d.tzid ? d.tzid : "");
	}
	
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->timezone),
				       zone ? zone : default_zone);
	task_page_set_show_timezone (tpage, calendar_config_get_show_timezone());

	if (!(COMP_EDITOR_PAGE (tpage)->flags & COMP_EDITOR_PAGE_NEW_ITEM) && !zone) {
			task_page_set_show_timezone (tpage, FALSE);
			bonobo_ui_component_set_prop (priv->uic, "/commands/ViewTimeZone", 
					"state", "0", NULL);
	}

	e_cal_component_free_datetime (&d);

	/* Start Date. */
	e_cal_component_get_dtstart (comp, &d);
	zone = NULL;
	if (d.value) {
		struct icaltimetype *start_tt = d.value;
		e_date_edit_set_date (E_DATE_EDIT (priv->start_date),
				      start_tt->year, start_tt->month,
				      start_tt->day);
		if (start_tt->is_date) {
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_date),
						     -1, -1);
			zone = default_zone;
		} else {
			e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_date),
						     start_tt->hour,
						     start_tt->minute);
		}
	} else {
		e_date_edit_set_time (E_DATE_EDIT (priv->start_date), -1);

		/* If no time is set, we use the default timezone, so the
		   user usually doesn't have to set this when they set the
		   date. */
		zone = default_zone;
	}

	e_cal_component_free_datetime (&d);

	/* Classification. */
	e_cal_component_get_classification (comp, &cl);

	switch (cl) {
	case E_CAL_COMPONENT_CLASS_PUBLIC:
	case E_CAL_COMPONENT_CLASS_PRIVATE:
	case E_CAL_COMPONENT_CLASS_CONFIDENTIAL:
		break;
	default:
		/* default to PUBLIC */
		cl = E_CAL_COMPONENT_CLASS_PUBLIC;
                break;
	}
	set_classification_menu (tpage, cl);

	e_cal_component_get_uid (comp, &uid);
	if (e_cal_get_object (COMP_EDITOR_PAGE (tpage)->client, uid, NULL, &icalcomp, NULL)) {
		icalcomponent_free (icalcomp);
		task_page_hide_options (tpage);
	}

	/* Categories */
	e_cal_component_get_categories (comp, &categories);
	e_dialog_editable_set (priv->categories, categories);

	/* Source */
	source = e_cal_get_source (page->client);
	e_source_option_menu_select (E_SOURCE_OPTION_MENU (priv->source_selector), source);

	if (priv->is_assignment) {
		ECalComponentOrganizer organizer;	
		
		priv->user_add = itip_get_comp_attendee (comp, COMP_EDITOR_PAGE (tpage)->client);	

		/* If there is an existing organizer show it properly */
		if (e_cal_component_has_organizer (comp)) {
			e_cal_component_get_organizer (comp, &organizer);
			if (organizer.value != NULL) {
				const gchar *strip = itip_strip_mailto (organizer.value);
				gchar *string;
				if (itip_organizer_is_user (comp, page->client)) {
					if (e_cal_get_static_capability (
								page->client,
								CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
						priv->user_org = TRUE;
				} else {
					if (e_cal_get_static_capability (
								page->client,
								CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
						gtk_widget_set_sensitive (priv->invite, FALSE);
					gtk_widget_set_sensitive (priv->add, FALSE);
					gtk_widget_set_sensitive (priv->edit, FALSE);
					gtk_widget_set_sensitive (priv->remove, FALSE);
					priv->user_org = FALSE;
				}

				if (e_cal_get_static_capability (COMP_EDITOR_PAGE (tpage)->client, CAL_STATIC_CAPABILITY_NO_ORGANIZER) && (COMP_EDITOR_PAGE (tpage)->flags & COMP_EDITOR_PAGE_DELEGATE))
					string = g_strdup (priv->user_add);
				else if ( organizer.cn != NULL)
					string = g_strdup_printf ("%s <%s>", organizer.cn, strip);
				else
					string = g_strdup (strip);

				g_free (string);
				priv->existing = TRUE;
			}
		} else {
			EAccount *a;

			a = get_current_account (tpage);
			if (a != NULL) {
				CompEditorPage *page = (CompEditorPage *) tpage;
				priv->ia = e_meeting_store_add_attendee_with_defaults (priv->model);
				g_object_ref (priv->ia);

				e_meeting_attendee_set_address (priv->ia, g_strdup_printf ("MAILTO:%s", a->id->address));
				e_meeting_attendee_set_cn (priv->ia, g_strdup (a->id->name));
				if (page->client && e_cal_get_organizer_must_accept (page->client))
					e_meeting_attendee_set_status (priv->ia, ICAL_PARTSTAT_NEEDSACTION);
				else
					e_meeting_attendee_set_status (priv->ia, ICAL_PARTSTAT_ACCEPTED);
			}
		}
	}


	priv->updating = FALSE;

	sensitize_widgets (tpage);

	return TRUE;
}

static void
set_attendees (ECalComponent *comp, const GPtrArray *attendees)
{
	GSList *comp_attendees = NULL, *l;
	int i;
	
	for (i = 0; i < attendees->len; i++) {
		EMeetingAttendee *ia = g_ptr_array_index (attendees, i);
		ECalComponentAttendee *ca;
		
		ca = e_meeting_attendee_as_e_cal_component_attendee (ia);
		
		comp_attendees = g_slist_prepend (comp_attendees, ca);
		
	}
	comp_attendees = g_slist_reverse (comp_attendees);
	
	e_cal_component_set_attendee_list (comp, comp_attendees);
	
	for (l = comp_attendees; l != NULL; l = l->next)
		g_free (l->data);	
	g_slist_free (comp_attendees);
}

/* fill_component handler for the task page */
static gboolean
task_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	ECalComponentDateTime date;
	struct icaltimetype start_tt, due_tt;
	char *cat, *str;
	gboolean start_date_set, due_date_set, time_set;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	icaltimezone *start_zone = NULL;
	icaltimezone *due_zone = NULL;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description));

	/* Summary. */

	str = e_dialog_editable_get (priv->summary);
	if (!str || strlen (str) == 0)
		e_cal_component_set_summary (comp, NULL);
	else {
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;

		e_cal_component_set_summary (comp, &text);
	}

	if (str)
		g_free (str);

	/* Description */

	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	if (!str || strlen (str) == 0)
		e_cal_component_set_description_list (comp, NULL);
	else {
		GSList l;
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		e_cal_component_set_description_list (comp, &l);
	}

	if (str)
		g_free (str);

	/* Dates */

	due_tt = icaltime_null_time ();

	date.value = &due_tt;
	date.tzid = NULL;

	/* Due Date. */
	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->due_date)) ||
	    !e_date_edit_time_is_valid (E_DATE_EDIT (priv->due_date))) {
		comp_editor_page_display_validation_error (page, _("Due date is wrong"), priv->due_date);
		return FALSE;
	}

	due_date_set = e_date_edit_get_date (E_DATE_EDIT (priv->due_date),
					 &due_tt.year,
					 &due_tt.month,
					 &due_tt.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->due_date),
						&due_tt.hour,
						&due_tt.minute);
	if (due_date_set) {
		if (time_set) {
			due_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->timezone));
			date.tzid = icaltimezone_get_tzid (due_zone);
		} else {
			due_tt.is_date = TRUE;
			date.tzid = NULL;
		}
		e_cal_component_set_due (comp, &date);
	} else {
		e_cal_component_set_due (comp, NULL);
	}

	/* Start Date. */
	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->start_date)) ||
	    !e_date_edit_time_is_valid (E_DATE_EDIT (priv->start_date))) {
		comp_editor_page_display_validation_error (page, _("Start date is wrong"), priv->start_date);
		return FALSE;
	}

	start_tt = icaltime_null_time ();
	date.value = &start_tt;
	start_date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_date),
					 &start_tt.year,
					 &start_tt.month,
					 &start_tt.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_date),
						&start_tt.hour,
						&start_tt.minute);
	if (start_date_set) {
		if (time_set) {
			start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->timezone));
			date.tzid = icaltimezone_get_tzid (start_zone);
		} else {
			start_tt.is_date = TRUE;
			date.tzid = NULL;
		}
		e_cal_component_set_dtstart (comp, &date);
	} else {
		e_cal_component_set_dtstart (comp, NULL);
	}
	
	/* Classification. */
	e_cal_component_set_classification (comp, priv->classification);
	
	/* send options */
	if (priv->sendoptions_shown && priv->sod) 
		e_sendoptions_utils_fill_component (priv->sod, comp);

	/* Categories */
	cat = e_dialog_editable_get (priv->categories);
	str = comp_editor_strip_categories (cat);
	if (cat)
		g_free (cat);

	e_cal_component_set_categories (comp, str);

	if (str)
		g_free (str);

	if (priv->is_assignment) {
		ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};

		if (!priv->existing) {
			EAccount *a;
			gchar *addr = NULL;

			/* Find the identity for the organizer or sentby field */
			a = get_current_account (tpage);

			/* Sanity Check */
			if (a == NULL) {
				e_notice (page, GTK_MESSAGE_ERROR,
						_("The organizer selected no longer has an account."));
				return FALSE;			
			}

			if (a->id->address == NULL || strlen (a->id->address) == 0) {
				e_notice (page, GTK_MESSAGE_ERROR,
						_("An organizer is required."));
				return FALSE;
			} 

			addr = g_strdup_printf ("MAILTO:%s", a->id->address);

			organizer.value = addr;
			organizer.cn = a->id->name;
			e_cal_component_set_organizer (comp, &organizer);

			g_free (addr);
		}

		if (e_meeting_store_count_actual_attendees (priv->model) < 1) {
			e_notice (page, GTK_MESSAGE_ERROR,
					_("At least one attendee is required."));
			return FALSE;
		}


		if (COMP_EDITOR_PAGE (tpage)->flags & COMP_EDITOR_PAGE_DELEGATE ) {
			GSList *attendee_list, *l;
			int i;
			const GPtrArray *attendees = e_meeting_store_get_attendees (priv->model);

			e_cal_component_get_attendee_list (priv->comp, &attendee_list);

			for (i = 0; i < attendees->len; i++) {
				EMeetingAttendee *ia = g_ptr_array_index (attendees, i);
				ECalComponentAttendee *ca;

				/* Remove the duplicate user from the component if present */
				if (e_meeting_attendee_is_set_delto (ia)) {
					for (l = attendee_list; l; l = l->next) {
						ECalComponentAttendee *a = l->data;

						if (g_str_equal (a->value, e_meeting_attendee_get_address (ia))) {
							attendee_list = g_slist_remove (attendee_list, l->data);
							break;
						}
					}
				}

				ca = e_meeting_attendee_as_e_cal_component_attendee (ia);

				attendee_list = g_slist_append (attendee_list, ca);
			}
			e_cal_component_set_attendee_list (comp, attendee_list);
			e_cal_component_free_attendee_list (attendee_list);
		} else 
			set_attendees (comp, e_meeting_store_get_attendees (priv->model));
	}

	return TRUE;
}

static void
add_clicked_cb (GtkButton *btn, TaskPage *page)
{
	EMeetingAttendee *attendee;

	attendee = e_meeting_store_add_attendee_with_defaults (page->priv->model);

	if (COMP_EDITOR_PAGE (page)->flags & COMP_EDITOR_PAGE_DELEGATE) {
		e_meeting_attendee_set_delfrom (attendee, g_strdup_printf ("MAILTO:%s", page->priv->user_add));
	}

	e_meeting_list_view_edit (page->priv->list_view, attendee);
}

edit_clicked_cb (GtkButton *btn, TaskPage *tpage)
{
	TaskPagePrivate *priv;
	EMeetingAttendee *attendee;
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_col;
	gint row = 0;

	priv = tpage->priv;

	gtk_tree_view_get_cursor (GTK_TREE_VIEW (priv->list_view), &path, NULL);
	g_return_if_fail (path != NULL);

	gtk_tree_view_get_cursor (GTK_TREE_VIEW (priv->list_view), &path, &focus_col);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->list_view), path, focus_col, TRUE);
	gtk_tree_path_free (path);
}


static gboolean
existing_attendee (EMeetingAttendee *ia, ECalComponent *comp) 
{
	GSList *attendees, *l;
	const gchar *ia_address;
	
	ia_address = itip_strip_mailto (e_meeting_attendee_get_address (ia));
	if (!ia_address)
		return FALSE;
	
	e_cal_component_get_attendee_list (comp, &attendees);

	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;
		const char *address;
		
		address = itip_strip_mailto (attendee->value);
		if (address && !g_strcasecmp (ia_address, address)) {
			e_cal_component_free_attendee_list (attendees);
			return TRUE;
		}
	}
	
	e_cal_component_free_attendee_list (attendees);
	
	return FALSE;
}

static void
remove_attendee (TaskPage *page, EMeetingAttendee *ia) 
{
	TaskPagePrivate *priv;
	int pos = 0;
	gboolean delegate = (COMP_EDITOR_PAGE (page)->flags & COMP_EDITOR_PAGE_DELEGATE);
	
	priv = page->priv;

	/* If the user deletes the organizer attendee explicitly,
	   assume they no longer want the organizer showing up */
	if (ia == priv->ia) {
		g_object_unref (priv->ia);
		priv->ia = NULL;
	}	
		
	/* If this was a delegatee, no longer delegate */
	if (e_meeting_attendee_is_set_delfrom (ia)) {
		EMeetingAttendee *ib;
		
		ib = e_meeting_store_find_attendee (priv->model, e_meeting_attendee_get_delfrom (ia), &pos);
		if (ib != NULL) {
			e_meeting_attendee_set_delto (ib, NULL);
			
			if (!delegate) 
				e_meeting_attendee_set_edit_level (ib,  E_MEETING_ATTENDEE_EDIT_FULL);
		}		
	}
	
	/* Handle deleting all attendees in the delegation chain */	
	while (ia != NULL) {
		EMeetingAttendee *ib = NULL;

		if (existing_attendee (ia, priv->comp)) {
			g_object_ref (ia);
			g_ptr_array_add (priv->deleted_attendees, ia);
		}
		
		if (e_meeting_attendee_get_delto (ia) != NULL)
			ib = e_meeting_store_find_attendee (priv->model, e_meeting_attendee_get_delto (ia), NULL);
		e_meeting_store_remove_attendee (priv->model, ia);

		ia = ib;
	}
	
	sensitize_widgets (page);
}

static void
remove_clicked_cb (GtkButton *btn, TaskPage *page)
{
	TaskPagePrivate *priv;
	EMeetingAttendee *ia;
	GtkTreeSelection *selection;
	GList *paths = NULL, *tmp;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	gboolean valid_iter;
	char *address;
	
	priv = page->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list_view));
	if (!(paths = gtk_tree_selection_get_selected_rows (selection, (GtkTreeModel **) &(priv->model)))) {
		g_warning ("Could not get a selection to delete.");
		return;
	}
	paths = g_list_reverse (paths);
	
	for (tmp = paths; tmp; tmp=tmp->next) {
		path = tmp->data;
		
		gtk_tree_model_get_iter (GTK_TREE_MODEL(priv->model), &iter, path);

		gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter, E_MEETING_STORE_ADDRESS_COL, &address, -1);
		ia = e_meeting_store_find_attendee (priv->model, address, NULL);
		g_free (address);
		if (!ia) {
			g_warning ("Cannot delete attendee\n");
			continue;
		} else if (e_meeting_attendee_get_edit_level (ia) != E_MEETING_ATTENDEE_EDIT_FULL) {
			g_warning("Not enough rights to delete attendee: %s\n", e_meeting_attendee_get_address(ia));	
			continue;
		}
		
		remove_attendee (page, ia);
	}
	
	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
	if (!valid_iter) {
		gtk_tree_path_prev (path);
		valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
	}
	
	if (valid_iter) {
		gtk_tree_selection_unselect_all (selection);
		gtk_tree_selection_select_iter (selection, &iter);
	}
	
	g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (paths);
}

static void
invite_cb (GtkWidget *widget, gpointer data) 
{
	TaskPage *page;
	TaskPagePrivate *priv;
	
	page = TASK_PAGE (data);
	priv = page->priv;

	e_meeting_list_view_invite_others_dialog (priv->list_view);
}

static void
attendee_added_cb (EMeetingListView *emlv, EMeetingAttendee *ia, gpointer user_data)
{
   TaskPage *page = TASK_PAGE (user_data);	
   TaskPagePrivate *priv;
   gboolean delegate = (COMP_EDITOR_PAGE (page)->flags & COMP_EDITOR_PAGE_DELEGATE);

   priv = page->priv;

   if (delegate) {
	   if (existing_attendee (ia, priv->comp))
		   e_meeting_store_remove_attendee (priv->model, ia);
	   else {
		   if (!e_cal_get_static_capability (COMP_EDITOR_PAGE(page)->client, 
					   CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY)) {
			   const char *delegator_id = e_meeting_attendee_get_delfrom (ia);
			   EMeetingAttendee *delegator;

			   delegator = e_meeting_store_find_attendee (priv->model, delegator_id, NULL);
			   e_meeting_attendee_set_delto (delegator, 
					   g_strdup (e_meeting_attendee_get_address (ia)));

			   gtk_widget_set_sensitive (priv->invite, FALSE);
			   gtk_widget_set_sensitive (priv->add, FALSE);
			   gtk_widget_set_sensitive (priv->edit, FALSE);
		   }
	   }
   }

}

/* Callbacks for list view*/
static void
popup_add_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	TaskPage *page = data;

	add_clicked_cb (NULL, page);
}

static void
popup_delete_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	TaskPage *page = data;
	TaskPagePrivate *priv;
	
	priv = page->priv;

	remove_clicked_cb (NULL, page);
}

enum {
	ATTENDEE_CAN_DELEGATE = 1<<1,
	ATTENDEE_CAN_DELETE = 1<<2,
	ATTENDEE_CAN_ADD = 1<<3,
	ATTENDEE_LAST = 1<<4,
};

static EPopupItem context_menu_items[] = {
	{ E_POPUP_ITEM, "10.delete", N_("_Remove"), popup_delete_cb, NULL, GTK_STOCK_REMOVE, ATTENDEE_CAN_DELETE },
	{ E_POPUP_ITEM, "15.add", N_("_Add "), popup_add_cb, NULL, GTK_STOCK_ADD, ATTENDEE_CAN_ADD },	
};

static void
context_popup_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}

static gint
button_press_event (GtkWidget *widget, GdkEventButton *event, TaskPage *page)
{
	TaskPagePrivate *priv;
	GtkMenu *menu;
	EMeetingAttendee *ia;
	GtkTreePath *path;
	GtkTreeIter iter;
	char *address;
	guint32 disable_mask = ~0;
	GSList *menus = NULL;
	ECalPopup *ep;
	int i;

	priv = page->priv;

	/* only process right-clicks */
	if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
		return FALSE;

	/* only if we right-click on an attendee */
	if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->list_view), event->x, event->y, &path, NULL, NULL, NULL)) {
		GtkTreeSelection *selection;

		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path)) {
		
			gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter, E_MEETING_STORE_ADDRESS_COL, &address, -1);
			ia = e_meeting_store_find_attendee (priv->model, address, &priv->row);
			g_free (address);

			if (ia) {
				selection = gtk_tree_view_get_selection ((GtkTreeView *) priv->list_view);
				gtk_tree_selection_unselect_all (selection);
				gtk_tree_selection_select_path (selection, path);
		
 				if (e_meeting_attendee_get_edit_level (ia) == E_MEETING_ATTENDEE_EDIT_FULL)
 					disable_mask &= ~ATTENDEE_CAN_DELETE;
			}
		}
	}

	if (GTK_WIDGET_IS_SENSITIVE(priv->add))
		disable_mask &= ~ATTENDEE_CAN_ADD;
	else if (COMP_EDITOR_PAGE (page)->flags & COMP_EDITOR_PAGE_USER_ORG)
		disable_mask &= ~ATTENDEE_CAN_ADD;

	ep = e_cal_popup_new("org.gnome.evolution.calendar.task.popup");

	for (i=0;i<sizeof(context_menu_items)/sizeof(context_menu_items[0]);i++)
		menus = g_slist_prepend(menus, &context_menu_items[i]);
	
	e_popup_add_items((EPopup *)ep, menus, NULL, context_popup_free, page);
	menu = e_popup_create_menu_once((EPopup *)ep, NULL, disable_mask);
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);

	return TRUE;
}

static gboolean
list_view_event (EMeetingListView *list_view, GdkEvent *event, TaskPage *page) {
	
	TaskPagePrivate *priv= page->priv;
	
	if (event->type == GDK_2BUTTON_PRESS && COMP_EDITOR_PAGE (page)->flags & COMP_EDITOR_PAGE_USER_ORG) {
		EMeetingAttendee *attendee;

		attendee = e_meeting_store_add_attendee_with_defaults (priv->model);

		if (COMP_EDITOR_PAGE (page)->flags & COMP_EDITOR_PAGE_DELEGATE) {
			e_meeting_attendee_set_delfrom (attendee, g_strdup_printf ("MAILTO:%s", page->priv->user_add));
		}

		e_meeting_list_view_edit (page->priv->list_view, attendee);
		return TRUE;
	}

	return FALSE;	
}


static gboolean
list_key_press (EMeetingListView *list_view, GdkEventKey *event, TaskPage *page)
{
	if (event->keyval == GDK_Delete) {
		TaskPagePrivate *priv;
	
		priv = page->priv;
		remove_clicked_cb (NULL, page);

		return TRUE;
	} else if (event->keyval == GDK_Insert) {
		add_clicked_cb (NULL, page);

		return TRUE;
	}

	return FALSE;
}

void 
task_page_set_show_timezone (TaskPage *page, gboolean state)
{
	if (state) {
		gtk_widget_show_all (page->priv->timezone);
		gtk_widget_show (page->priv->timezone_label);	
	} else {
		gtk_widget_hide (page->priv->timezone);
		gtk_widget_hide (page->priv->timezone_label);
	}
		
}

void
task_page_set_show_categories (TaskPage *page, gboolean state)
{
	if (state) {
		gtk_widget_show (page->priv->categories_btn);
		gtk_widget_show (page->priv->categories);	
	} else {
		gtk_widget_hide (page->priv->categories_btn);
		gtk_widget_hide (page->priv->categories);
	}
}

/* fill_timezones handler for the event page */
static gboolean
task_page_fill_timezones (CompEditorPage *page, GHashTable *timezones)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	icaltimezone *zone;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	/* add start date timezone */
	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->timezone));
	if (zone) {
		if (!g_hash_table_lookup (timezones, icaltimezone_get_tzid (zone)))
			g_hash_table_insert (timezones, icaltimezone_get_tzid (zone), zone);
	}

	return TRUE;
}

/* set_summary handler for the task page */
static void
task_page_set_summary (CompEditorPage *page, const char *summary)
{
	/* nothing */
}

static void
task_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (page);
	priv = tpage->priv;

	if (priv->updating)
	        return;

	priv->updating = TRUE;

	priv->updating = FALSE;
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (TaskPage *tpage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (tpage);
	TaskPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;
	GtkWidget *sw;
	GtkTreeSelection *selection;

	priv = tpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("task-page");
	if (!priv->main)
		return FALSE;

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	   it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups) {
		page->accel_group = accel_groups->data;
		gtk_accel_group_ref (page->accel_group);
	}

	gtk_widget_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);

	priv->summary = GW ("summary");
	priv->summary_label = GW ("summary-label");

	/* Glade's visibility flag doesn't seem to work for custom widgets */
	priv->due_date = GW ("due-date");
	gtk_widget_show (priv->due_date);
	priv->start_date = GW ("start-date");
	gtk_widget_show (priv->start_date);

	priv->timezone = GW ("timezone");
	priv->timezone_label = GW ("timezone-label");
	priv->attendees_label = GW ("attendees-label");
	priv->description = GW ("description");
	priv->categories_btn = GW ("categories-button");
	priv->categories = GW ("categories");
	priv->organizer = GW ("organizer");
	priv->invite = GW ("invite");
	priv->add = GW ("add-attendee");
	priv->edit = GW ("edit-attendee");
	priv->remove = GW ("remove-attendee");
	priv->list_box = GW ("list-box");
	priv->calendar_label = GW ("group-label");
	priv->attendee_box = GW ("attendee-box");
	priv->org_cal_label = GW ("org-task-label");

	priv->list_view = e_meeting_list_view_new (priv->model);
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) priv->list_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_widget_show (GTK_WIDGET (priv->list_view));

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_widget_show (sw);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (priv->list_view));
	gtk_box_pack_start (GTK_BOX (priv->list_box), sw, TRUE, TRUE, 0);

	priv->source_selector = GW ("source");
	
#undef GW

	return (priv->summary
		&& priv->summary_label
		&& priv->due_date
		&& priv->start_date
		&& priv->timezone
		&& priv->description
		&& priv->categories_btn
		&& priv->categories
		);
}

/* Callback used when the summary changes; we emit the notification signal. */
static void
summary_changed_cb (GtkEditable *editable, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	gchar *summary;
	
	tpage = TASK_PAGE (data);
	priv = tpage->priv;
	
	if (priv->updating)
		return;
	
	summary = e_dialog_editable_get (GTK_WIDGET (editable));
	comp_editor_page_notify_summary_changed (COMP_EDITOR_PAGE (tpage), 
						 summary);
	g_free (summary);
}

/* Callback used when the start or due date widgets change.  We notify the
 * other pages in the task editor, so they can update any labels. 
 */
static void
date_changed_cb (EDateEdit *dedit, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	CompEditorPageDates dates;
	gboolean date_set, time_set;
	ECalComponentDateTime start_dt, due_dt;
	struct icaltimetype start_tt = icaltime_null_time();
	struct icaltimetype due_tt = icaltime_null_time();

	tpage = TASK_PAGE (data);
	priv = tpage->priv;

	if (priv->updating)
		return;

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_date),
					 &start_tt.year,
					 &start_tt.month,
					 &start_tt.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_date),
						&start_tt.hour,
						&start_tt.minute);
	if (date_set) {
		if (time_set) {
			icaltimezone *zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->timezone));
			start_dt.tzid = icaltimezone_get_tzid (zone);
		} else {
			start_tt.is_date = TRUE;
			start_dt.tzid = NULL;
		}
	} else {
		start_tt = icaltime_null_time ();
		start_dt.tzid = NULL;
	}

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->due_date),
					 &due_tt.year,
					 &due_tt.month,
					 &due_tt.day);
	time_set = e_date_edit_get_time_of_day (E_DATE_EDIT (priv->due_date),
						&due_tt.hour,
						&due_tt.minute);
	if (date_set) {
		if (time_set) {
			icaltimezone *zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->timezone));
			due_dt.tzid = icaltimezone_get_tzid (zone);
		} else {
			due_tt.is_date = TRUE;
			due_dt.tzid = NULL;
		}
	} else {
		due_tt = icaltime_null_time ();
		due_dt.tzid = NULL;
	}

	start_dt.value = &start_tt;
	dates.start = &start_dt;
	dates.end = NULL;
	due_dt.value = &due_tt;
	dates.due = &due_dt;
	dates.complete = NULL;
	
	/* Notify upstream */
	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (tpage),
					       &dates);
}

static void
timezone_changed_cb (EDateEdit *dedit, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	
	tpage = TASK_PAGE (data);
	priv = tpage->priv;

	date_changed_cb ((EDateEdit *) priv->start_date, tpage);
	date_changed_cb ((EDateEdit *) priv->due_date, tpage);
}

/* Callback used when the categories button is clicked; we must bring up the
 * category list dialog.
 */
static void
categories_clicked_cb (GtkWidget *button, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	GtkWidget *entry;

	tpage = TASK_PAGE (data);
	priv = tpage->priv;

	entry = priv->categories;
	e_categories_config_open_dialog_for_entry (GTK_ENTRY (entry));
}
/* sets the current focused widget */
static gboolean 
widget_focus_in_cb (GtkWidget *widget,  GdkEventFocus *event, gpointer data)
{
	TaskPage *tpage;
	tpage = TASK_PAGE  (data);

	comp_editor_page_set_focused_widget (COMP_EDITOR_PAGE(tpage), widget);

	return FALSE;
}
/* unsets the current focused widget */
static gboolean
widget_focus_out_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	TaskPage *tpage;
	tpage = TASK_PAGE (data);

	comp_editor_page_unset_focused_widget (COMP_EDITOR_PAGE(tpage), widget);

	return FALSE;
}
/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;
	
	tpage = TASK_PAGE (data);
	priv = tpage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (tpage));
}

static void
source_changed_cb (GtkWidget *widget, ESource *source, gpointer data)
{
	TaskPage *tpage;
	TaskPagePrivate *priv;

	tpage = TASK_PAGE (data);
	priv = tpage->priv;

	if (!priv->updating) {
		ECal *client;

		client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_TODO);
		if (client) {
			icaltimezone *zone;
			
			zone = calendar_config_get_icaltimezone ();
			e_cal_set_default_timezone (client, zone, NULL);
		}
		
		if (!client || !e_cal_open (client, FALSE, NULL)) {
			GtkWidget *dialog;

			if (client)
				g_object_unref (client);

			e_source_option_menu_select (E_SOURCE_OPTION_MENU (priv->source_selector),
						     e_cal_get_source (COMP_EDITOR_PAGE (tpage)->client));

			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open tasks in '%s'."),
							 e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		} else {
			comp_editor_notify_client_changed (
				COMP_EDITOR (gtk_widget_get_toplevel (priv->main)),
				client);
			if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS) && priv->is_assignment)
				task_page_show_options (tpage);
			else
				task_page_hide_options (tpage);


			sensitize_widgets (tpage);
		}
	}
}

void
task_page_sendoptions_clicked_cb (TaskPage *tpage)
{
	TaskPagePrivate *priv;
	GtkWidget *toplevel;
	ESource *source;

	priv = tpage->priv;
	
	if (!priv->sod) {
		priv->sod = e_sendoptions_dialog_new ();
		priv->sod->data->initialized = TRUE;
		source = e_source_option_menu_peek_selected  (E_SOURCE_OPTION_MENU (priv->source_selector));
		e_sendoptions_utils_set_default_data (priv->sod, source, "task");
	}
	
	if (e_cal_get_static_capability (COMP_EDITOR_PAGE (tpage)->client, 
					 CAL_STATIC_CAPABILITY_NO_GEN_OPTIONS)) {
		e_sendoptions_set_need_general_options (priv->sod, FALSE);
	}
	
	toplevel = gtk_widget_get_toplevel (priv->main);
	e_sendoptions_dialog_run (priv->sod, toplevel, E_ITEM_TASK);
}

/* Hooks the widget signals */
static gboolean
init_widgets (TaskPage *tpage)
{
	TaskPagePrivate *priv;
	GtkTextBuffer *text_buffer;
	icaltimezone *zone;

	priv = tpage->priv;

	/* Make sure the EDateEdit widgets use our timezones to get the
	   current time. */
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->start_date),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   tpage, NULL);
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->due_date),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   tpage, NULL);
	
	/* Summary */
	g_signal_connect((priv->summary), "changed",
			    G_CALLBACK (summary_changed_cb), tpage);

	/* Description */
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description));

	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->description), GTK_WRAP_WORD);

	/* Dates */
	g_signal_connect((priv->start_date), "changed",
			    G_CALLBACK (date_changed_cb), tpage);
	g_signal_connect((priv->due_date), "changed",
			    G_CALLBACK (date_changed_cb), tpage);

	/* time zone changed */
	g_signal_connect (priv->timezone, "changed", G_CALLBACK(timezone_changed_cb), tpage);

	/* Categories button */
	g_signal_connect((priv->categories_btn), "clicked",
			    G_CALLBACK (categories_clicked_cb), tpage);
	
	/* Source selector */
	g_signal_connect((priv->source_selector), "source_selected",
			 G_CALLBACK (source_changed_cb), tpage);

	/* Connect the default signal handler to use to make sure the "changed"
	   field gets set whenever a field is changed. */

	/* Set the current focus entry */
	g_signal_connect (priv->summary, "focus-in-event",
			  G_CALLBACK (widget_focus_in_cb), tpage);
	g_signal_connect (priv->summary, "focus-out-event",
			  G_CALLBACK (widget_focus_out_cb), tpage);

	g_signal_connect (priv->description, "focus-in-event",
			  G_CALLBACK (widget_focus_in_cb), tpage);
	g_signal_connect (priv->description, "focus-out-event",
			  G_CALLBACK (widget_focus_out_cb), tpage);


	/* Belongs to priv->description */
	g_signal_connect ((text_buffer), "changed",
			  G_CALLBACK (field_changed_cb), tpage);

	g_signal_connect((priv->summary), "changed",
			    G_CALLBACK (field_changed_cb), tpage);
	g_signal_connect (priv->start_date, "changed",
			  G_CALLBACK (field_changed_cb), tpage);
	g_signal_connect (priv->due_date, "changed",
			  G_CALLBACK (field_changed_cb), tpage);
	g_signal_connect((priv->timezone), "changed",
			    G_CALLBACK (field_changed_cb), tpage);
	g_signal_connect((priv->categories), "changed",
			    G_CALLBACK (field_changed_cb), tpage);
	g_signal_connect (G_OBJECT (priv->list_view), "button_press_event", G_CALLBACK (button_press_event), tpage);
	g_signal_connect (G_OBJECT (priv->list_view), "event", G_CALLBACK (list_view_event), tpage);
	g_signal_connect (priv->list_view, "key_press_event", G_CALLBACK (list_key_press), tpage);	
	
	/* Add attendee button */
	g_signal_connect (priv->add, "clicked", G_CALLBACK (add_clicked_cb), tpage);

	/* Edit attendee button */
	g_signal_connect (priv->edit, "clicked", G_CALLBACK (edit_clicked_cb), tpage);

	/* Remove attendee button */
	g_signal_connect (priv->remove, "clicked", G_CALLBACK (remove_clicked_cb), tpage);

	/* Contacts button */
	g_signal_connect(priv->invite, "clicked", G_CALLBACK (invite_cb), tpage);	

	/* Meeting List View */
	g_signal_connect (priv->list_view, "attendee_added", G_CALLBACK (attendee_added_cb), tpage);	
	
	/* Set the default timezone, so the timezone entry may be hidden. */
	zone = calendar_config_get_icaltimezone ();
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->timezone), zone);
	
	task_page_set_show_timezone (tpage, calendar_config_get_show_timezone());

	e_meeting_list_view_column_set_visible (priv->list_view, "Attendee                          ", 
			TRUE);
	e_meeting_list_view_column_set_visible (priv->list_view, "Role", calendar_config_get_show_role());
	e_meeting_list_view_column_set_visible (priv->list_view, "RSVP", calendar_config_get_show_rsvp());
	e_meeting_list_view_column_set_visible (priv->list_view, "Status", calendar_config_get_show_status());
	e_meeting_list_view_column_set_visible (priv->list_view, "Type", calendar_config_get_show_type());
	
	task_page_set_show_categories (tpage, calendar_config_get_show_categories());

	return TRUE;
}



/**
 * task_page_construct:
 * @tpage: An task page.
 * 
 * Constructs an task page by loading its Glade data.
 * 
 * Return value: The same object as @tpage, or NULL if the widgets could not be
 * created.
 **/
TaskPage *
task_page_construct (TaskPage *tpage, EMeetingStore *model, ECal *client)
{
	TaskPagePrivate *priv;
	char *backend_address = NULL;
	EIterator *it;
	EAccount *def_account;
	GList *address_strings = NULL, *l;
	EAccount *a;
	char *gladefile;
	
	priv = tpage->priv;
	g_object_ref (model);
	priv->model = model;
	priv->client = client;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "task-page.glade",
				      NULL);
	priv->xml = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);

	if (!priv->xml) {
		g_message ("task_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (tpage)) {
		g_message ("task_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}
	/* Address information */
	if (!e_cal_get_cal_address (client, &backend_address, NULL))
		return NULL;

	priv->accounts = itip_addresses_get ();
	def_account = itip_addresses_get_default();
	for (it = e_list_get_iterator((EList *)priv->accounts);
	     e_iterator_is_valid(it);
	     e_iterator_next(it)) {
		a = (EAccount *)e_iterator_get(it);
		char *full;
		
		full = g_strdup_printf("%s <%s>", a->id->name, a->id->address);

		address_strings = g_list_append(address_strings, full);

		/* Note that the address specified by the backend gets
		 * precedence over the default mail address.
		 */
		if (backend_address && !strcmp (backend_address, a->id->address)) {
			if (priv->default_address)
				g_free (priv->default_address);
			
			priv->default_address = g_strdup (full);
		} else if (a == def_account && !priv->default_address) {
			priv->default_address = g_strdup (full);
		}
	}
	
	if (backend_address)
		g_free (backend_address);

	g_object_unref(it);

	if (address_strings)
		gtk_combo_set_popdown_strings (GTK_COMBO (priv->organizer), address_strings);
	else
		g_warning ("No potential organizers!");

	for (l = address_strings; l != NULL; l = l->next)
		g_free (l->data);
	g_list_free (address_strings);
	

	if (!init_widgets (tpage)) {
		g_message ("task_page_construct(): " 
			   "Could not initialize the widgets!");
		return NULL;
	}

	return tpage;
}

/**
 * task_page_new:
 * 
 * Creates a new task page.
 * 
 * Return value: A newly-created task page, or NULL if the page could
 * not be created.
 **/
TaskPage *
task_page_new (EMeetingStore *model, ECal *client, BonoboUIComponent *uic)
{
	TaskPage *tpage;

	tpage = gtk_type_new (TYPE_TASK_PAGE);
	if (!task_page_construct (tpage, model, client)) {
		g_object_unref (tpage);
		return NULL;
	}
	
	tpage->priv->uic = uic;

	return tpage;
}

ECalComponent *
task_page_get_cancel_comp (TaskPage *page)
{
	TaskPagePrivate *priv;

	g_return_val_if_fail (page != NULL, NULL);
	g_return_val_if_fail (IS_TASK_PAGE (page), NULL);

	priv = page->priv;

	if (priv->deleted_attendees->len == 0)
		return NULL;
	
	set_attendees (priv->comp, priv->deleted_attendees);
	
	return e_cal_component_clone (priv->comp);
}

GtkWidget *task_page_create_date_edit (void);

GtkWidget *
task_page_create_date_edit (void)
{
	GtkWidget *dedit;

	dedit = comp_editor_new_date_edit (TRUE, TRUE, TRUE);
	e_date_edit_set_allow_no_date_set (E_DATE_EDIT (dedit), TRUE);

	return dedit;
}

GtkWidget *task_page_create_source_option_menu (void);

GtkWidget *
task_page_create_source_option_menu (void)
{
	GtkWidget   *menu;
	GConfClient *gconf_client;
	ESourceList *source_list;

	gconf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (gconf_client, "/apps/evolution/tasks/sources");

	menu = e_source_option_menu_new (source_list);
	g_object_unref (source_list);

	gtk_widget_show (menu);
	return menu;
}
