/* Evolution calendar - Main page of the task editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <liboaf/liboaf.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkvbox.h>
#include <glade/glade.h>
#include <gal/e-table/e-cell-combo.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-table-simple.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-popup-menu.h>
#include <gal/widgets/e-gui-utils.h>
#include <widgets/misc/e-dateedit.h>
#include <e-util/e-dialog-widgets.h>
#include "../Evolution-Addressbook-SelectNames.h"
#include "../component-factory.h"
#include "../itip-utils.h"
#include "comp-editor-util.h"
#include "meeting-page.h"


#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames"

enum columns {
	MEETING_ATTENDEE_COL,
	MEETING_MEMBER_COL,
	MEETING_TYPE_COL,
	MEETING_ROLE_COL,
	MEETING_RSVP_COL,
	MEETING_DELTO_COL,
	MEETING_DELFROM_COL,
	MEETING_STATUS_COL,
	MEETING_CN_COL,
	MEETING_LANG_COL,
	MEETING_COLUMN_COUNT
};

struct attendee {
	char *address;
	char *member;

	CalComponentCUType cutype;
	CalComponentRole role;
	CalComponentPartStat status;
	gboolean rsvp;

	char *delto;
	char *delfrom;
	char *sentby;
	char *cn;
	char *language;
};

/* Private part of the MeetingPage structure */
struct _MeetingPagePrivate {
	/* List of attendees */
	GSList *attendees;

	/* List of identities */
	GList *addresses;
	GList *address_strings;
	gchar *default_address;
	
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;
	GtkWidget *organizer_table;
	GtkWidget *organizer;
	GtkWidget *organizer_lbl;
	GtkWidget *other_organizer;
	GtkWidget *other_organizer_lbl;
	GtkWidget *other_organizer_btn;
	GtkWidget *existing_organizer_table;
	GtkWidget *existing_organizer;
	GtkWidget *existing_organizer_btn;
	GtkWidget *invite;
	
	/* E Table stuff */
	ETableModel *model;
	GtkWidget *etable;
	gint row;
	
	/* For handling who the organizer is */
	gboolean other;
	gboolean existing;
	
	/* For handling the invite button */
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;

	gboolean updating;
};



static void meeting_page_class_init (MeetingPageClass *class);
static void meeting_page_init (MeetingPage *mpage);
static void meeting_page_destroy (GtkObject *object);

static GtkWidget *meeting_page_get_widget (CompEditorPage *page);
static void meeting_page_focus_main_widget (CompEditorPage *page);
static void meeting_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static void meeting_page_fill_component (CompEditorPage *page, CalComponent *comp);

static int row_count (ETableModel *etm, void *data);
static gint right_click_cb (ETable *etable, gint row, gint col, GdkEvent *event, gpointer data);

static CompEditorPageClass *parent_class = NULL;



/**
 * meeting_page_get_type:
 * 
 * Registers the #MeetingPage class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #MeetingPage class.
 **/
GtkType
meeting_page_get_type (void)
{
	static GtkType meeting_page_type;

	if (!meeting_page_type) {
		static const GtkTypeInfo meeting_page_info = {
			"MeetingPage",
			sizeof (MeetingPage),
			sizeof (MeetingPageClass),
			(GtkClassInitFunc) meeting_page_class_init,
			(GtkObjectInitFunc) meeting_page_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		meeting_page_type = 
			gtk_type_unique (TYPE_COMP_EDITOR_PAGE,
					 &meeting_page_info);
	}

	return meeting_page_type;
}

/* Class initialization function for the task page */
static void
meeting_page_class_init (MeetingPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GtkObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GtkObjectClass *) class;

	parent_class = gtk_type_class (TYPE_COMP_EDITOR_PAGE);

	editor_page_class->get_widget = meeting_page_get_widget;
	editor_page_class->focus_main_widget = meeting_page_focus_main_widget;
	editor_page_class->fill_widgets = meeting_page_fill_widgets;
	editor_page_class->fill_component = meeting_page_fill_component;
	editor_page_class->set_summary = NULL;
	editor_page_class->set_dates = NULL;

	object_class->destroy = meeting_page_destroy;
}

/* Object initialization function for the task page */
static void
meeting_page_init (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;

	priv = g_new0 (MeetingPagePrivate, 1);
	mpage->priv = priv;

	priv->xml = NULL;

	priv->main = NULL;
	priv->invite = NULL;
	
	priv->model = NULL;
	priv->etable = NULL;
	
	priv->updating = FALSE;
}

/* Destroy handler for the task page */
static void
meeting_page_destroy (GtkObject *object)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	ETable *real_table;
	char *filename;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MEETING_PAGE (object));

	mpage = MEETING_PAGE (object);
	priv = mpage->priv;

	itip_addresses_free (priv->addresses);
	g_list_free (priv->address_strings);

	filename = g_strdup_printf ("%s/config/et-header-meeting-page", 
				    evolution_dir);
	real_table = e_table_scrolled_get_table (E_TABLE_SCROLLED (priv->etable));
	e_table_save_state (real_table, filename);
	g_free (filename);
	
	if (priv->xml) {
		gtk_object_unref (GTK_OBJECT (priv->xml));
		priv->xml = NULL;
	}

	g_free (priv);
	mpage->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



/* get_widget handler for the task page */
static GtkWidget *
meeting_page_get_widget (CompEditorPage *page)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;

	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

	return priv->main;
}

/* focus_main_widget handler for the task page */
static void
meeting_page_focus_main_widget (CompEditorPage *page)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;

	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

	gtk_widget_grab_focus (priv->organizer);
}

/* Fills the widgets with default values */
static void
clear_widgets (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;

	priv = mpage->priv;

	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->organizer)->entry), "");
	gtk_entry_set_text (GTK_ENTRY (priv->other_organizer), "");
	gtk_label_set_text (GTK_LABEL (priv->existing_organizer), "None");

	gtk_widget_show (priv->organizer_table);
	gtk_widget_hide (priv->existing_organizer_table);	

	gtk_widget_hide (priv->other_organizer_lbl);
	gtk_widget_hide (priv->other_organizer);

	priv->existing = FALSE;
	priv->other = FALSE;
}

/* fill_widgets handler for the meeting page */
static void
meeting_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	CalComponentOrganizer organizer;
	GSList *attendees, *l;
	GList *l2;
	
	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

	priv->updating = TRUE;
	
	/* Clean the screen */
	clear_widgets (mpage);

	/* Organizer */
	cal_component_get_organizer (comp, &organizer);
	priv->addresses = itip_addresses_get ();
	for (l2 = priv->addresses; l2 != NULL; l2 = l2->next) {
		ItipAddress *a = l2->data;
		
		priv->address_strings = g_list_append (priv->address_strings, a->full);
		if (a->default_address)
			priv->default_address = a->full;
	}
	gtk_combo_set_popdown_strings (GTK_COMBO (priv->organizer), priv->address_strings);

	if (organizer.value != NULL) {
		const gchar *strip = itip_strip_mailto (organizer.value);
		gchar *s = e_utf8_to_gtk_string (priv->existing_organizer, strip);

		gtk_widget_hide (priv->organizer_table);
		gtk_widget_show (priv->existing_organizer_table);
		gtk_widget_hide (priv->invite);
		
		gtk_label_set_text (GTK_LABEL (priv->existing_organizer), s);
		g_free (s);
		
		priv->existing = TRUE;
	} else {
		gtk_widget_hide (priv->other_organizer_lbl);
		gtk_widget_hide (priv->other_organizer);

		e_dialog_editable_set (GTK_COMBO (priv->organizer)->entry, priv->default_address);
	}

	/* Attendees */
	cal_component_get_attendee_list (comp, &attendees);
	for (l = attendees; l != NULL; l = l->next) {
		CalComponentAttendee *att = l->data;
		struct attendee *attendee = g_new0 (struct attendee, 1);

		attendee->address = g_strdup (att->value);
		attendee->member = g_strdup (att->member);
		attendee->cutype= att->cutype;
		attendee->role = att->role;
		attendee->status = att->status;
		attendee->rsvp = att->rsvp;
		attendee->delto = g_strdup (att->delto);
		attendee->delfrom = g_strdup (att->delfrom);
		attendee->sentby = g_strdup (att->sentby);
		attendee->cn = g_strdup (att->cn);
		attendee->language = g_strdup (att->language);
		
		priv->attendees = g_slist_prepend (priv->attendees, attendee);
	
	}
	priv->attendees = g_slist_reverse (priv->attendees);
	cal_component_free_attendee_list (attendees);
	
	/* Table */
	e_table_model_rows_inserted (priv->model, 0, row_count (priv->model, mpage));

	/* So the comp editor knows we need to send if anything changes */
	if (priv->attendees != NULL)
		comp_editor_page_notify_needs_send (COMP_EDITOR_PAGE (mpage));

	priv->updating = FALSE;
}

/* fill_component handler for the meeting page */
static void
meeting_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	CalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};
	GSList *attendees = NULL, *l;
	
	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

	if (!priv->existing) {
		gchar *addr = NULL, *cn = NULL;
		GList *l;
		
		if (priv->other) {
			addr = e_dialog_editable_get (priv->other_organizer);
		} else {
			gchar *str = e_dialog_editable_get (GTK_COMBO (priv->organizer)->entry);
			for (l = priv->addresses; l != NULL; l = l->next) {
				ItipAddress *a = l->data;
				
				if (!strcmp (a->full, str)) {
					addr = g_strdup (a->address);
					cn = g_strdup (a->name);
				}
			}
			g_free (str);
		}
		
		if (addr == NULL || strlen (addr) == 0) {		
			g_free (addr);
			g_free (cn);		
			return;
		} else {
			gchar *tmp;
			
			tmp = addr;
			addr = g_strdup_printf ("MAILTO:%s", addr);
			g_free (tmp);
		}
	
		organizer.value = addr;
		organizer.cn = cn;
		cal_component_set_organizer (comp, &organizer);
		g_free (addr);
		g_free (cn);
	}
	
	for (l = priv->attendees; l != NULL; l = l->next) {
		struct attendee *attendee = l->data;
		CalComponentAttendee *att = g_new0 (CalComponentAttendee, 1);
		
		
		att->value = attendee->address;
		att->member = (attendee->member && *attendee->member) ? attendee->member : NULL;
		att->cutype= attendee->cutype;
		att->role = attendee->role;
		att->status = attendee->status;
		att->rsvp = attendee->rsvp;
		att->delto = (attendee->delto && *attendee->delto) ? attendee->delto : NULL;
		att->delfrom = (attendee->delfrom && *attendee->delfrom) ? attendee->delfrom : NULL;
		att->sentby = attendee->sentby;
		att->cn = (attendee->cn && *attendee->cn) ? attendee->cn : NULL;
		att->language = (attendee->language && *attendee->language) ? attendee->language : NULL;
		
		attendees = g_slist_prepend (attendees, att);
		
	}
	attendees = g_slist_reverse (attendees);
	cal_component_set_attendee_list (comp, attendees);
	g_slist_free (attendees);
}



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;

	priv = mpage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("meeting-page");
	if (!priv->main)
		return FALSE;

	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);

	priv->organizer_table = GW ("organizer-table");
	priv->organizer = GW ("organizer");
	priv->organizer_lbl = GW ("organizer-label");
	priv->other_organizer = GW ("other-organizer");
	priv->other_organizer_lbl = GW ("other-organizer-label");
	priv->other_organizer_btn = GW ("other-organizer-button");
	priv->existing_organizer_table = GW ("existing-organizer-table");
	priv->existing_organizer = GW ("existing-organizer");
	priv->existing_organizer_btn = GW ("existing-organizer-button");
	priv->invite = GW ("invite");
	
#undef GW

	return (priv->invite
		&& priv->organizer_table
		&& priv->organizer
		&& priv->organizer_lbl
		&& priv->other_organizer
		&& priv->other_organizer_lbl
		&& priv->other_organizer_btn
		&& priv->existing_organizer_table
		&& priv->existing_organizer
		&& priv->existing_organizer_btn);
}

static void
invite_entry_changed (BonoboListener    *listener,
		      char              *event_name,
		      CORBA_any         *arg,
		      CORBA_Environment *ev,
		      gpointer           user_data)
{
}

static void
add_section (GNOME_Evolution_Addressbook_SelectNames corba_select_names, const char *name)
{
	Bonobo_Control corba_control;
	CORBA_Environment ev;
	GtkWidget *control_widget;
	BonoboControlFrame *cf;
	Bonobo_PropertyBag pb = CORBA_OBJECT_NIL;
	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_addSection (corba_select_names,
							    name, name, &ev);

	corba_control =
		GNOME_Evolution_Addressbook_SelectNames_getEntryBySection (
			corba_select_names, name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	control_widget = bonobo_widget_new_control_from_objref (
		corba_control, CORBA_OBJECT_NIL);

	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (control_widget));
	pb = bonobo_control_frame_get_control_property_bag (cf, NULL);

	bonobo_event_source_client_add_listener (
		pb, invite_entry_changed,
		"Bonobo/Property:change:entry_changed",
		NULL, NULL);
}

static gboolean
get_select_name_dialog (MeetingPage *mpage) 
{
	MeetingPagePrivate *priv;
	CORBA_Environment ev;
	
	priv = mpage->priv;

	if (priv->corba_select_names != CORBA_OBJECT_NIL)
		return TRUE;
	
	CORBA_exception_init (&ev);

	priv->corba_select_names = oaf_activate_from_id (SELECT_NAMES_OAFID, 0, NULL, &ev);

	add_section (priv->corba_select_names, "Required Participants");
	add_section (priv->corba_select_names, "Optional Participants");
	add_section (priv->corba_select_names, "Non-Participants");

	/* OAF seems to be broken -- it can return a CORBA_OBJECT_NIL without
           raising an exception in `ev'.  */
	if (ev._major != CORBA_NO_EXCEPTION || priv->corba_select_names == CORBA_OBJECT_NIL) {
		g_warning ("Cannot activate -- %s", SELECT_NAMES_OAFID);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	
	mpage = MEETING_PAGE (data);
	priv = mpage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (mpage));
}

/* Function called to make the organizer other than the user */
static void
other_clicked_cb (GtkWidget *widget, gpointer data) 
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	
	mpage = MEETING_PAGE (data);
	priv = mpage->priv;

	gtk_widget_show (priv->other_organizer_lbl);
	gtk_widget_show (priv->other_organizer);

	gtk_label_set_text (GTK_LABEL (priv->organizer_lbl), _("Sent By:"));

	priv->other = TRUE;
}

/* Function called to change the organizer */
static void
change_clicked_cb (GtkWidget *widget, gpointer data) 
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	
	mpage = MEETING_PAGE (data);
	priv = mpage->priv;

	gtk_widget_show (priv->organizer_table);
	gtk_widget_hide (priv->existing_organizer_table);
	gtk_widget_show (priv->invite);

	gtk_combo_set_popdown_strings (GTK_COMBO (priv->organizer), priv->address_strings);
	e_dialog_editable_set (GTK_COMBO (priv->organizer)->entry, priv->default_address);

	priv->existing = FALSE;
}

/* Function called to invite more people */
static void
invite_cb (GtkWidget *widget, gpointer data) 
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	CORBA_Environment ev;
	
	mpage = MEETING_PAGE (data);
	priv = mpage->priv;
	
	if (!get_select_name_dialog (mpage))
		return;
	
	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_activateDialog (
		priv->corba_select_names, "Required Participants", &ev);

	CORBA_exception_free (&ev);
}

/* Hooks the widget signals */
static void
init_widgets (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;

	priv = mpage->priv;

	/* Organizer */
	gtk_signal_connect (GTK_OBJECT (GTK_COMBO (priv->organizer)->entry), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), mpage);

	gtk_signal_connect (GTK_OBJECT (priv->other_organizer_btn), "clicked",
			    GTK_SIGNAL_FUNC (other_clicked_cb), mpage);
	gtk_signal_connect (GTK_OBJECT (priv->existing_organizer_btn), "clicked",
			    GTK_SIGNAL_FUNC (change_clicked_cb), mpage);

	/* Invite button */
	gtk_signal_connect (GTK_OBJECT (priv->invite), "clicked", 
			    GTK_SIGNAL_FUNC (invite_cb), mpage);
}

static CalComponentCUType
text_to_type (const char *type)
{
	if (!g_strcasecmp (type, "Individual"))
		return CAL_COMPONENT_CUTYPE_INDIVIDUAL;
	else if (!g_strcasecmp (type, "Group"))
		return CAL_COMPONENT_CUTYPE_GROUP;
	else if (!g_strcasecmp (type, "Resource"))
		return CAL_COMPONENT_CUTYPE_RESOURCE;
	else if (!g_strcasecmp (type, "Room"))
		return CAL_COMPONENT_CUTYPE_ROOM;
	else
		return CAL_COMPONENT_ROLE_UNKNOWN;
}

static char *
type_to_text (CalComponentCUType type)
{
	switch (type) {
	case CAL_COMPONENT_CUTYPE_INDIVIDUAL:
		return "Individual";
	case CAL_COMPONENT_CUTYPE_GROUP:
		return "Group";
	case CAL_COMPONENT_CUTYPE_RESOURCE:
		return "Resource";
	case CAL_COMPONENT_CUTYPE_ROOM:
		return "Room";
	default:
		return "Unknown";
	}

	return NULL;

}

static CalComponentRole
text_to_role (const char *role)
{
	if (!g_strcasecmp (role, "Chair"))
		return CAL_COMPONENT_ROLE_CHAIR;
	else if (!g_strcasecmp (role, "Required Participant"))
		return CAL_COMPONENT_ROLE_REQUIRED;
	else if (!g_strcasecmp (role, "Optional Participant"))
		return CAL_COMPONENT_ROLE_OPTIONAL;
	else if (!g_strcasecmp (role, "Non-Participant"))
		return CAL_COMPONENT_ROLE_NON;
	else
		return CAL_COMPONENT_ROLE_UNKNOWN;
}

static char *
role_to_text (CalComponentRole role) 
{
	switch (role) {
	case CAL_COMPONENT_ROLE_CHAIR:
		return "Chair";
	case CAL_COMPONENT_ROLE_REQUIRED:
		return "Required Participant";
	case CAL_COMPONENT_ROLE_OPTIONAL:
		return "Optional Participant";
	case CAL_COMPONENT_ROLE_NON:
		return "Non-Participant";
	default:
		return "Unknown";
	}

	return NULL;
}

static gboolean
text_to_boolean (const char *role)
{
	if (!g_strcasecmp (role, "Yes"))
		return TRUE;
	else
		return FALSE;
}

static char *
boolean_to_text (gboolean b) 
{
	if (b)
		return "Yes";
	else
		return "No";
}

static CalComponentPartStat
text_to_partstat (const char *partstat)
{
	if (!g_strcasecmp (partstat, "Needs Action"))
		return CAL_COMPONENT_PARTSTAT_NEEDSACTION;
	else if (!g_strcasecmp (partstat, "Accepted"))
		return CAL_COMPONENT_PARTSTAT_ACCEPTED;
	else if (!g_strcasecmp (partstat, "Declined"))
		return CAL_COMPONENT_PARTSTAT_DECLINED;
	else if (!g_strcasecmp (partstat, "Tentative"))
		return CAL_COMPONENT_PARTSTAT_TENTATIVE;
	else if (!g_strcasecmp (partstat, "Delegated"))
		return CAL_COMPONENT_PARTSTAT_DELEGATED;
	else if (!g_strcasecmp (partstat, "Completed"))
		return CAL_COMPONENT_PARTSTAT_COMPLETED;
	else if (!g_strcasecmp (partstat, "In Process"))
		return CAL_COMPONENT_PARTSTAT_INPROCESS;
	else
		return CAL_COMPONENT_PARTSTAT_UNKNOWN;
}

static char *
partstat_to_text (CalComponentPartStat partstat) 
{
	switch (partstat) {
	case CAL_COMPONENT_PARTSTAT_NEEDSACTION:
		return "Needs Action";
	case CAL_COMPONENT_PARTSTAT_ACCEPTED:
		return "Accepted";
	case CAL_COMPONENT_PARTSTAT_DECLINED:
		return "Declined";
	case CAL_COMPONENT_PARTSTAT_TENTATIVE:
		return "Tentative";
	case CAL_COMPONENT_PARTSTAT_DELEGATED:
		return "Delegated";
	case CAL_COMPONENT_PARTSTAT_COMPLETED:
		return "Completed";
	case CAL_COMPONENT_PARTSTAT_INPROCESS:
		return "In Process";
	case CAL_COMPONENT_PARTSTAT_UNKNOWN:
	default:
		return "Unknown";
	}

	return NULL;
}

static int
column_count (ETableModel *etm, void *data)
{
	return MEETING_COLUMN_COUNT;
}

static int
row_count (ETableModel *etm, void *data)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;

	mpage = MEETING_PAGE (data);	
	priv = mpage->priv;

	return g_slist_length (priv->attendees);
}

static void
append_row (ETableModel *etm, ETableModel *model, int row, void *data)
{	
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	struct attendee *attendee;
	gint row_cnt;
	
	mpage = MEETING_PAGE (data);	
	priv = mpage->priv;

	attendee = g_new0 (struct attendee, 1);
	
	attendee->address = g_strdup_printf ("MAILTO:%s", e_table_model_value_at (model, MEETING_ATTENDEE_COL, row));
	attendee->member = g_strdup (e_table_model_value_at (model, MEETING_MEMBER_COL, row));
	attendee->cutype = text_to_type (e_table_model_value_at (model, MEETING_TYPE_COL, row));
	attendee->role = text_to_role (e_table_model_value_at (model, MEETING_ROLE_COL, row));
	attendee->rsvp = text_to_boolean (e_table_model_value_at (model, MEETING_RSVP_COL, row));
	attendee->delto = g_strdup (e_table_model_value_at (model, MEETING_DELTO_COL, row));
	attendee->delfrom = g_strdup (e_table_model_value_at (model, MEETING_DELFROM_COL, row));
	attendee->status = text_to_partstat (e_table_model_value_at (model, MEETING_STATUS_COL, row));
	attendee->cn = g_strdup (e_table_model_value_at (model, MEETING_CN_COL, row));
	attendee->language = g_strdup (e_table_model_value_at (model, MEETING_LANG_COL, row));

	priv->attendees = g_slist_append (priv->attendees, attendee);
	
	row_cnt = row_count (etm, data) - 1;
	e_table_model_row_inserted (E_TABLE_MODEL (etm), row_cnt);

	comp_editor_page_notify_needs_send (COMP_EDITOR_PAGE (mpage));
	comp_editor_page_notify_changed (COMP_EDITOR_PAGE (mpage));
}

static void *
value_at (ETableModel *etm, int col, int row, void *data)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	struct attendee *attendee;

	mpage = MEETING_PAGE (data);	
	priv = mpage->priv;

	attendee = g_slist_nth_data (priv->attendees, row);
	
	switch (col) {
	case MEETING_ATTENDEE_COL:
		return itip_strip_mailto (attendee->address);
	case MEETING_MEMBER_COL:
		return attendee->member;
	case MEETING_TYPE_COL:
		return type_to_text (attendee->cutype);
	case MEETING_ROLE_COL:
		return role_to_text (attendee->role);
	case MEETING_RSVP_COL:
		return boolean_to_text (attendee->rsvp);
	case MEETING_DELTO_COL:
		return attendee->delto;
	case MEETING_DELFROM_COL:
		return attendee->delfrom;
	case MEETING_STATUS_COL:
		return partstat_to_text (attendee->status);
	case MEETING_CN_COL:
		return attendee->cn;
	case MEETING_LANG_COL:
		return attendee->language;
	}
	
	return NULL;
}

static void
set_value_at (ETableModel *etm, int col, int row, const void *val, void *data)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	struct attendee *attendee;

	mpage = MEETING_PAGE (data);	
	priv = mpage->priv;
	
	attendee = g_slist_nth_data (priv->attendees, row);
	
	switch (col) {
	case MEETING_ATTENDEE_COL:
		if (attendee->address)
			g_free (attendee->address);
		attendee->address = g_strdup_printf ("MAILTO:%s", val);
		break;
	case MEETING_MEMBER_COL:
		if (attendee->member)
			g_free (attendee->member);
		attendee->member = g_strdup (val);
		break;
	case MEETING_TYPE_COL:
		attendee->cutype = text_to_type (val);
		break;
	case MEETING_ROLE_COL:
		attendee->role = text_to_role (val);
		break;
	case MEETING_RSVP_COL:
		attendee->rsvp = text_to_boolean (val);
		break;
	case MEETING_DELTO_COL:
		if (attendee->delto)
			g_free (attendee->delto);
		attendee->delto = g_strdup (val);
		break;
	case MEETING_DELFROM_COL:
		if (attendee->delfrom)
			g_free (attendee->delfrom);
		attendee->delto = g_strdup (val);
		break;
	case MEETING_STATUS_COL:
		attendee->status = text_to_partstat (val);
		break;
	case MEETING_CN_COL:
		if (attendee->cn)
			g_free (attendee->cn);
		attendee->cn = g_strdup (val);
		break;
	case MEETING_LANG_COL:
		if (attendee->language)
			g_free (attendee->language);
		attendee->language = g_strdup (val);
		break;
	}

	if (!priv->updating) {		
		comp_editor_page_notify_needs_send (COMP_EDITOR_PAGE (mpage));
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (mpage));
	}
}

static gboolean
is_cell_editable (ETableModel *etm, int col, int row, void *data)
{
	switch (col) {
	case MEETING_DELTO_COL:
	case MEETING_DELFROM_COL:
		return FALSE;

	default:
	}

	return TRUE;
}

static void *
duplicate_value (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup (val);
}

static void
free_value (ETableModel *etm, int col, void *val, void *data)
{
	g_free (val);
}

static void *
init_value (ETableModel *etm, int col, void *data)
{
	switch (col) {
	case MEETING_ATTENDEE_COL:
		return g_strdup ("");
	case MEETING_MEMBER_COL:
		return g_strdup ("");
	case MEETING_TYPE_COL:
		return g_strdup ("Individual");
	case MEETING_ROLE_COL:
		return g_strdup ("Required Participant");
	case MEETING_RSVP_COL:
		return g_strdup ("Yes");
	case MEETING_DELTO_COL:
		return g_strdup ("");
	case MEETING_DELFROM_COL:
		return g_strdup ("");
	case MEETING_STATUS_COL:
		return g_strdup ("Needs Action");
	case MEETING_CN_COL:
		return g_strdup ("");
	case MEETING_LANG_COL:
		return g_strdup ("en");
	}
	
	return g_strdup ("");
}

static gboolean
value_is_empty (ETableModel *etm, int col, const void *val, void *data)
{
	
	switch (col) {
	case MEETING_ATTENDEE_COL:
	case MEETING_MEMBER_COL:
	case MEETING_DELTO_COL:
	case MEETING_DELFROM_COL:
	case MEETING_CN_COL:
		if (val && !g_strcasecmp (val, ""))
			return TRUE;
		else
			return FALSE;
	default:
	}
	
	return TRUE;
}

static char *
value_to_string (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup (val);
}

static void
build_etable (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;
	ETable *real_table;
	ETableExtras *extras;
	GList *strings;
	ECell *popup_cell, *cell;
	
	char *filename;
	
	priv = mpage->priv;
	
	extras = e_table_extras_new ();

	/* For type */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));
	
	strings = NULL;
	strings = g_list_append (strings, "Individual");
	strings = g_list_append (strings, "Group");
	strings = g_list_append (strings, "Resource");
	strings = g_list_append (strings, "Room");
	strings = g_list_append (strings, "Unknown");

	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell), strings);
	e_table_extras_add_cell (extras, "typeedit", popup_cell);
	
	/* For role */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));
	
	strings = NULL;
	strings = g_list_append (strings, "Chair");
	strings = g_list_append (strings, "Required Participant");
	strings = g_list_append (strings, "Optional Participant");
	strings = g_list_append (strings, "Non-Participant");
	strings = g_list_append (strings, "Unknown");

	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell), strings);
	e_table_extras_add_cell (extras, "roleedit", popup_cell);

	/* For rsvp */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));

	strings = NULL;
	strings = g_list_append (strings, "Yes");
	strings = g_list_append (strings, "No");

	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell), strings);
	e_table_extras_add_cell (extras, "rsvpedit", popup_cell);

	/* For status */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	gtk_object_unref (GTK_OBJECT (cell));

	strings = NULL;
	strings = g_list_append (strings, "Needs Action");
	strings = g_list_append (strings, "Accepted");
	strings = g_list_append (strings, "Declined");
	strings = g_list_append (strings, "Tentative");
	strings = g_list_append (strings, "Delegated");

	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell), strings);
	e_table_extras_add_cell (extras, "statusedit", popup_cell);


	/* The table itself */
	priv->model = e_table_simple_new (column_count,
					  row_count,
					  value_at,
					  set_value_at,
					  is_cell_editable,
					  duplicate_value,
					  free_value,
					  init_value,
					  value_is_empty,
					  value_to_string,
					  mpage);
	gtk_object_set (GTK_OBJECT (priv->model),
			"append_row", append_row,
			NULL);
	
	priv->etable = e_table_scrolled_new_from_spec_file (priv->model,
							    extras, 
							    EVOLUTION_ETSPECDIR "/meeting-page.etspec",
							    NULL);
	filename = g_strdup_printf ("%s/config/et-header-meeting-page", 
				    evolution_dir);
	real_table = e_table_scrolled_get_table (E_TABLE_SCROLLED (priv->etable));
	e_table_load_state (real_table, filename);
	g_free (filename);

	gtk_signal_connect (GTK_OBJECT (real_table),
			    "right_click", GTK_SIGNAL_FUNC (right_click_cb), mpage);

	gtk_object_unref (GTK_OBJECT (extras));
}

static void
popup_delegate_cb (GtkWidget *widget, gpointer data) 
{
	MeetingPage *mpage = MEETING_PAGE (data);
	MeetingPagePrivate *priv;
	
	priv = mpage->priv;

	e_table_model_row_changed (priv->model, priv->row);
}

static void
popup_delete_cb (GtkWidget *widget, gpointer data) 
{
	MeetingPage *mpage = MEETING_PAGE (data);
	MeetingPagePrivate *priv;
	GSList *l;
	
	priv = mpage->priv;

	l = g_slist_nth (priv->attendees, priv->row);
	priv->attendees = g_slist_remove (priv->attendees, l->data);
	
	e_table_model_row_deleted (priv->model, priv->row);
}

enum {
	CAN_DELEGATE = 2,
	CAN_DELETE = 4
};

static EPopupMenu context_menu[] = {
	{ N_("_Delegate To..."),              NULL,
	  GTK_SIGNAL_FUNC (popup_delegate_cb),NULL,  CAN_DELEGATE },

	E_POPUP_SEPARATOR,

	{ N_("_Delete"),                      GNOME_STOCK_MENU_TRASH,
	  GTK_SIGNAL_FUNC (popup_delete_cb),  NULL,  CAN_DELETE },
	
	E_POPUP_TERMINATOR
};

/* handle context menu over message-list */
static gint
right_click_cb (ETable *etable, gint row, gint col, GdkEvent *event, gpointer data)
{
	MeetingPage *mpage = MEETING_PAGE (data);
	MeetingPagePrivate *priv;
	GtkMenu *menu;
	int enable_mask = 0, hide_mask = 0;

	priv = mpage->priv;

	priv->row = row;

	menu = e_popup_menu_create (context_menu, enable_mask, hide_mask, data);
	e_auto_kill_popup_menu_on_hide (menu);
	
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL,
			event->button.button, event->button.time);

	return TRUE;
}



/**
 * meeting_page_construct:
 * @mpage: An task details page.
 * 
 * Constructs an task page by loading its Glade data.
 * 
 * Return value: The same object as @mpage, or NULL if the widgets could not 
 * be created.
 **/
MeetingPage *
meeting_page_construct (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;
	
	priv = mpage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR 
				   "/meeting-page.glade", NULL);
	if (!priv->xml) {
		g_message ("meeting_page_construct(): "
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (mpage)) {
		g_message ("meeting_page_construct(): "
			   "Could not find all widgets in the XML file!");
		return NULL;
	}
	
	/* The etable displaying attendees and their status */
	build_etable (mpage);	
	gtk_widget_show (priv->etable);
	gtk_box_pack_start (GTK_BOX (priv->main), priv->etable, TRUE, TRUE, 2);
	
	/* Init the widget signals */
	init_widgets (mpage);

	return mpage;
}

/**
 * meeting_page_new:
 * 
 * Creates a new task details page.
 * 
 * Return value: A newly-created task details page, or NULL if the page could
 * not be created.
 **/
MeetingPage *
meeting_page_new (void)
{
	MeetingPage *mpage;

	mpage = gtk_type_new (TYPE_MEETING_PAGE);
	if (!meeting_page_construct (mpage)) {
		gtk_object_unref (GTK_OBJECT (mpage));
		return NULL;
	}

	return mpage;
}
