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
#include <widgets/misc/e-dateedit.h>
#include <widgets/meeting-time-sel/e-meeting-time-sel.h>
#include <e-util/e-dialog-widgets.h>
#include "../Evolution-Addressbook-SelectNames.h"
#include "comp-editor-util.h"
#include "meeting-page.h"


#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames"

#define MEETING_PAGE_TABLE_SPEC						\
	"<ETableSpecification click-to-add=\"true\" "			\
	" _click-to-add-message=\"Click here to add an attendee\" "	\
	" draw-grid=\"true\">"						\
        "  <ETableColumn model_col= \"0\" _title=\"Attendee\" "	\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"string\" compare=\"string\"/>"			                \
        "  <ETableColumn model_col= \"1\" _title=\"Role\" "	\
	"   expansion=\"1.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"roleedit\"   compare=\"string\"/>"		\
        "  <ETableColumn model_col= \"2\" _title=\"RSVP\" "	\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"rsvpedit\" compare=\"string\"/>"			                \
        "  <ETableColumn model_col= \"3\" _title=\"Status\" "		\
	"   expansion=\"2.0\" minimum_width=\"10\" resizable=\"true\" "	\
	"   cell=\"statusedit\" compare=\"string\"/>"			                \
	"  <ETableState>"						\
	"    <column source=\"0\"/>"					\
	"    <column source=\"1\"/>"					\
	"    <column source=\"2\"/>"					\
	"    <column source=\"3\"/>"					\
	"    <grouping></grouping>"					\
	"  </ETableState>"						\
	"</ETableSpecification>"

enum columns {
	MEETING_ATTENDEE_COL,
	MEETING_ROLE_COL,
	MEETING_RSVP_COL,
	MEETING_STATUS_COL,
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
	
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;
	GtkWidget *organizer;
	GtkWidget *invite;
	
	/* E Table stuff */
	ETableModel *model;
	GtkWidget *etable;

	/* For handling the invite button */
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;

	gboolean updating;
};



static void meeting_page_class_init (MeetingPageClass *class);
static void meeting_page_init (MeetingPage *mpage);
static void meeting_page_destroy (GtkObject *object);

static GtkWidget *meeting_page_get_widget (CompEditorPage *page);
static void meeting_page_fill_widgets (CompEditorPage *page, CalComponent *comp);
static void meeting_page_fill_component (CompEditorPage *page, CalComponent *comp);

static int row_count (ETableModel *etm, void *data);

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

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MEETING_PAGE (object));

	mpage = MEETING_PAGE (object);
	priv = mpage->priv;

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

/* Fills the widgets with default values */
static void
clear_widgets (MeetingPage *mpage)
{
	MeetingPagePrivate *priv;

	priv = mpage->priv;
}

/* fill_widgets handler for the task page */
static void
meeting_page_fill_widgets (CompEditorPage *page, CalComponent *comp)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	CalComponentOrganizer organizer;
	GSList *attendees, *l;
	
	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

	priv->updating = TRUE;
	
	/* Clean the screen */
	clear_widgets (mpage);

	cal_component_get_organizer (comp, &organizer);
	e_dialog_editable_set (priv->organizer, organizer.value);

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

/* fill_component handler for the task page */
static void
meeting_page_fill_component (CompEditorPage *page, CalComponent *comp)
{
	MeetingPage *mpage;
	MeetingPagePrivate *priv;
	CalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};
	GSList *attendees = NULL, *l;
	gchar *str;
	
	mpage = MEETING_PAGE (page);
	priv = mpage->priv;

	str = e_dialog_editable_get (priv->organizer);
	if (str == NULL || strlen (str) == 0) {		
		if (str != NULL)
			g_free (str);
		return;
	}
	
	organizer.value = str;
	cal_component_set_organizer (comp, &organizer);
	g_free (str);
	
	for (l = priv->attendees; l != NULL; l = l->next) {
		struct attendee *attendee = l->data;
		CalComponentAttendee *att = g_new0 (CalComponentAttendee, 1);
		
		att->value = attendee->address;
		att->member = attendee->member;
		att->cutype= attendee->cutype;
		att->role = attendee->role;
		att->status = attendee->status;
		att->rsvp = attendee->rsvp;
		att->delto = attendee->delto;
		att->delfrom = attendee->delfrom;
		att->sentby = attendee->sentby;
		att->cn = attendee->cn;
		att->language = attendee->language;
		
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
	g_assert (priv->main);
	gtk_widget_ref (priv->main);
	gtk_widget_unparent (priv->main);

	priv->organizer = GW ("organizer");
	priv->invite = GW ("invite");
	
#undef GW

	return (priv->invite
		&& priv->organizer);
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
	gtk_signal_connect (GTK_OBJECT (priv->organizer), "changed",
			    GTK_SIGNAL_FUNC (field_changed_cb), mpage);

	/* Invite button */
	gtk_signal_connect (GTK_OBJECT (priv->invite), "clicked", 
			    GTK_SIGNAL_FUNC (invite_cb), mpage);
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
	
	attendee->address = g_strdup (e_table_model_value_at (model, MEETING_ATTENDEE_COL, row));
	attendee->role = text_to_role (e_table_model_value_at (model, MEETING_ROLE_COL, row));
	attendee->rsvp = text_to_boolean (e_table_model_value_at (model, MEETING_RSVP_COL, row));
	attendee->status = text_to_partstat (e_table_model_value_at (model, MEETING_STATUS_COL, row));

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
		return attendee->address;
	case MEETING_ROLE_COL:
		return role_to_text (attendee->role);
	case MEETING_RSVP_COL:
		return boolean_to_text (attendee->rsvp);
	case MEETING_STATUS_COL:
		return partstat_to_text (attendee->status);
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
		attendee->address = g_strdup (val);
		break;
	case MEETING_ROLE_COL:
		attendee->role = text_to_role (val);
		break;
	case MEETING_RSVP_COL:
		attendee->rsvp = text_to_boolean (val);
		break;
	case MEETING_STATUS_COL:
		attendee->status = text_to_partstat (val);
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
	case MEETING_ROLE_COL:
		return g_strdup ("Required Participant");
	case MEETING_RSVP_COL:
		return g_strdup ("Yes");
	case MEETING_STATUS_COL:
		return g_strdup ("Needs Action");
	}
	
	return g_strdup ("");
}

static gboolean
value_is_empty (ETableModel *etm, int col, const void *val, void *data)
{
	if (col == 0) {
		if (val && !g_strcasecmp (val, ""))
			return TRUE;
		else
			return FALSE;
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
	ETableExtras *extras;
	GList *strings;
	ECell *popup_cell, *cell;
	
	priv = mpage->priv;
	
	extras = e_table_extras_new ();
	
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
	
	priv->etable = e_table_scrolled_new (priv->model, extras, 
					     MEETING_PAGE_TABLE_SPEC, NULL);
	gtk_object_unref (GTK_OBJECT (extras));
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
