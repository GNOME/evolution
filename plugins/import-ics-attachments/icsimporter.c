/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Johnny Jacob <johnnyjacob@gmail.com>
 *
 *  Copyright 2006 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <e-util/e-config.h>
#include <e-util/e-popup.h>
#include <mail/em-popup.h>
#include <mail/em-menu.h>
#include <mail/mail-ops.h>
#include <mail/mail-mt.h>
#include <mail/em-folder-view.h>
#include <mail/em-format-html-display.h>
#include <mail/em-utils.h>
#include "e-attachment-bar.h"
#include <camel/camel-vee-folder.h>
#include "e-util/e-error.h"
#include "e-util/e-icon-factory.h"
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libedataserverui/e-source-selector.h>
#include <libecal/e-cal.h>
#include <libical/icalvcal.h>
#include <calendar/common/authentication.h>

typedef struct {
	ECal *client;
	int source_type;
	icalcomponent *icalcomp;
	GtkWidget *window;
	GtkWidget *selector;
} ICalImporterData;


static void import_ics (EPlugin *ep, EPopupTarget *t, void *data);
static icalcomponent* get_icalcomponent_from_file(char *filename);
static void prepare_events (icalcomponent *icalcomp, GList **vtodos);
static void prepare_tasks (icalcomponent *icalcomp, GList *vtodos);
static void import_items(ICalImporterData *icidata);
static gboolean update_objects (ECal *client, icalcomponent *icalcomp);
static void dialog_response_cb (GtkDialog *dialog, gint response_id, ICalImporterData *icidata);
static void dialog_close_cb (GtkDialog *dialog, ICalImporterData *icidata);
static void ical_import_done(ICalImporterData *icidata);
static void init_widgets (char *path);
static icalcomponent_kind get_menu_type (void *data);

void org_gnome_evolution_import_ics_attachments (EPlugin *ep, EMPopupTargetAttachments *t);
void org_gnome_evolution_import_ics_part (EPlugin *ep, EMPopupTargetPart *t);

static void 
popup_free (EPopup *ep, GSList *items, void *data)
{
	g_slist_free (items);
}

static EPopupItem popup_calendar_items[] = {
	{ E_POPUP_BAR, "25.display.00"},
	{ E_POPUP_ITEM, "25.display.01", N_("_Import to Calendar"), (EPopupActivateFunc)import_ics, NULL, "stock_mail-import"}
};

static EPopupItem popup_tasks_items[] = {
	{ E_POPUP_BAR, "25.display.00"},
	{ E_POPUP_ITEM, "25.display.01", N_("_Import to Tasks"), (EPopupActivateFunc)import_ics, NULL, "stock_mail-import"}
};


void org_gnome_evolution_import_ics_attachments (EPlugin *ep, EMPopupTargetAttachments *t) 
{
	GSList *menus = NULL;
	icalcomponent_kind kind;
	int len;
	int i = 0;

	len = g_slist_length(t->attachments);
	if (!camel_content_type_is(((CamelDataWrapper *) ((EAttachment *) t->attachments->data)->body)->mime_type, "text", "calendar") || len >1)
		return;
	
		kind = get_menu_type (t);

	if (kind == ICAL_VTODO_COMPONENT ) {
		for (i = 0; i < sizeof (popup_tasks_items) / sizeof (popup_tasks_items[0]); i++)
			menus = g_slist_prepend (menus, &popup_tasks_items[i]);
	} else if ( kind == ICAL_VEVENT_COMPONENT) {
		for (i = 0; i < sizeof (popup_calendar_items) / sizeof (popup_calendar_items[0]); i++)
			menus = g_slist_prepend (menus, &popup_calendar_items[i]);
	}

	e_popup_add_items (t->target.popup, menus, NULL, popup_free, t);
}

void org_gnome_evolution_import_ics_part (EPlugin*ep, EMPopupTargetPart *t) 
{
	GSList *menus = NULL;
	icalcomponent_kind kind;
	int i = 0;

	if (!camel_content_type_is(((CamelDataWrapper *) t->part)->mime_type, "text", "calendar"))
		return;

	kind = get_menu_type (t);

	if (kind == ICAL_VTODO_COMPONENT ) {
		for (i = 0; i < sizeof (popup_tasks_items) / sizeof (popup_tasks_items[0]); i++)
			menus = g_slist_prepend (menus, &popup_tasks_items[i]);
	} else if ( kind == ICAL_VEVENT_COMPONENT) {
		for (i = 0; i < sizeof (popup_calendar_items) / sizeof (popup_calendar_items[0]); i++)
			menus = g_slist_prepend (menus, &popup_calendar_items[i]);
	}

	e_popup_add_items (t->target.popup, menus, NULL, popup_free, t);
}

static icalcomponent_kind
get_menu_type (void *data)
{
	CamelMimePart *part;
	char *path;
	icalcomponent *icalcomp, *subcomp;
	icalcomponent_kind kind;
	EPopupTarget *target = (EPopupTarget *) data;	

	if (target->type == EM_POPUP_TARGET_ATTACHMENTS)
		part = ((EAttachment *) ((EMPopupTargetAttachments *) target)->attachments->data)->body;
	else
		part = ((EMPopupTargetPart *) target)->part;

	path = em_utils_temp_save_part (NULL, part);

	icalcomp = get_icalcomponent_from_file (path);

	subcomp = icalcomponent_get_inner(icalcomp);
	kind = icalcomponent_isa (subcomp);

	if (kind == ICAL_VTODO_COMPONENT ) {
		return ICAL_VTODO_COMPONENT;
	} else if ( kind == ICAL_VEVENT_COMPONENT) {
		return ICAL_VEVENT_COMPONENT;
	}
	return ICAL_NO_COMPONENT;
}

static void
import_ics (EPlugin *ep, EPopupTarget *t, void *data)
{
	CamelMimePart *part;
	char *path;
	EPopupTarget *target = (EPopupTarget *) data;	

	if (target->type == EM_POPUP_TARGET_ATTACHMENTS)
		part = ((EAttachment *) ((EMPopupTargetAttachments *) target)->attachments->data)->body;
	else
		part = ((EMPopupTargetPart *) target)->part;

	path = em_utils_temp_save_part (NULL, part);
	init_widgets(path);
}

static void
init_widgets(char *path)
{
	
	GtkWidget *vbox, *hbox, *dialog;
	icalcomponent_kind kind;
	icalcomponent *subcomp;
	GtkWidget *selector, *label;
	ESourceList *source_list;
	ESource *primary;
	GtkWidget *scrolled;
	ICalImporterData *icidata = g_malloc0(sizeof(*icidata));

	g_return_if_fail ( path != NULL);
	dialog = gtk_dialog_new_with_buttons (_("Import ICS"), 
						NULL, GTK_DIALOG_DESTROY_WITH_PARENT, 
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						NULL);
	icidata->window = dialog;
	g_signal_connect (dialog,
			  "response",
			  G_CALLBACK (dialog_response_cb),
			  icidata);
	g_signal_connect (dialog,
			  "close",
			  G_CALLBACK (dialog_close_cb),
			  icidata);

	vbox = GTK_DIALOG(dialog)->vbox;
	hbox = gtk_hbox_new (FALSE, FALSE);
	label = gtk_label_new(NULL);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 6);

	icidata->icalcomp = get_icalcomponent_from_file (path);

	subcomp = icalcomponent_get_inner(icidata->icalcomp);
	kind = icalcomponent_isa (subcomp);

	char *label_str;
	if (kind == ICAL_VTODO_COMPONENT ) {
		e_cal_get_sources (&source_list, E_CAL_SOURCE_TYPE_TODO, NULL);
		label_str = _("Select Task List");
		icidata->source_type = E_CAL_SOURCE_TYPE_TODO;
	} else if ( kind == ICAL_VEVENT_COMPONENT) {
		e_cal_get_sources (&source_list, E_CAL_SOURCE_TYPE_EVENT, NULL);
		label_str = _("Select Calendar");
		icidata->source_type = E_CAL_SOURCE_TYPE_EVENT;
	}

	char *markup;
	markup = g_markup_printf_escaped ("<b>%s</b>", label_str);
	gtk_label_set_markup (label, markup);
	hbox = gtk_hbox_new (FALSE, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 6);

	selector = e_source_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy((GtkScrolledWindow *)scrolled, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add((GtkContainer *)scrolled, selector);
	gtk_scrolled_window_set_shadow_type (scrolled, GTK_SHADOW_IN);
	hbox = gtk_hbox_new (FALSE, FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), scrolled, TRUE, TRUE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 6);
	icidata->selector = selector;
	

	/* FIXME What if no sources? */
	primary = e_source_list_peek_source_any (source_list);
	e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (selector), primary);

	g_object_unref (source_list);
	hbox = gtk_hbox_new (FALSE, FALSE);
	GtkWidget *icon = e_icon_factory_get_image ("stock_mail-import", E_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX(hbox), icon, FALSE, FALSE, 6);
	label = gtk_label_new_with_mnemonic (_("_Import"));
	gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_widget_show(label);
	GtkWidget *button = gtk_button_new ();
	gtk_container_add (button, hbox);
	gtk_dialog_add_action_widget (dialog, button, GTK_RESPONSE_OK);
	gtk_widget_grab_focus (button); 

	gtk_window_set_default_size (dialog, 210,340);
	gtk_widget_show_all (dialog);
	gtk_dialog_run (dialog);
}

static void
dialog_response_cb (GtkDialog *dialog, gint response_id, ICalImporterData *icidata)
{
	switch (response_id) {
		case GTK_RESPONSE_OK :
			import_items(icidata);
		break;

		case GTK_RESPONSE_CANCEL :
		case GTK_RESPONSE_DELETE_EVENT :
			gtk_signal_emit_by_name ((GtkObject *)dialog, "close");	
		break;
	}
}

static void 
dialog_close_cb (GtkDialog *dialog, ICalImporterData *icidata)
{
	gtk_widget_destroy ((GtkWidget *)dialog);
}

/* This removes all components except VEVENTs and VTIMEZONEs from the toplevel */
static void
prepare_events (icalcomponent *icalcomp, GList **vtodos)
{
	icalcomponent *subcomp;
	icalcompiter iter;

	if (vtodos)
		*vtodos = NULL;
	
	iter = icalcomponent_begin_component (icalcomp, ICAL_ANY_COMPONENT);
	while ((subcomp = icalcompiter_deref (&iter)) != NULL) {
		icalcomponent_kind child_kind = icalcomponent_isa (subcomp);
		if (child_kind != ICAL_VEVENT_COMPONENT
		    && child_kind != ICAL_VTIMEZONE_COMPONENT) {

			icalcompiter_next (&iter);

			icalcomponent_remove_component (icalcomp, subcomp);
			if (child_kind == ICAL_VTODO_COMPONENT && vtodos)
				*vtodos = g_list_prepend (*vtodos, subcomp);
			else
                                icalcomponent_free (subcomp);
		}

		icalcompiter_next (&iter);
	}
}

/* This removes all components except VTODOs and VTIMEZONEs from the toplevel
   icalcomponent, and adds the given list of VTODO components. The list is
   freed afterwards. */
static void
prepare_tasks (icalcomponent *icalcomp, GList *vtodos)
{
	icalcomponent *subcomp;
	GList *elem;
	icalcompiter iter;

	iter = icalcomponent_begin_component (icalcomp, ICAL_ANY_COMPONENT);
	while ((subcomp = icalcompiter_deref (&iter)) != NULL) {
		icalcomponent_kind child_kind = icalcomponent_isa (subcomp);
		if (child_kind != ICAL_VTODO_COMPONENT
		    && child_kind != ICAL_VTIMEZONE_COMPONENT) {
			icalcompiter_next (&iter);
			icalcomponent_remove_component (icalcomp, subcomp);
			icalcomponent_free (subcomp);
		}

		icalcompiter_next (&iter);
	}

	for (elem = vtodos; elem; elem = elem->next) {
		icalcomponent_add_component (icalcomp, elem->data);
	}
	g_list_free (vtodos);
}

static void 
import_items(ICalImporterData *icidata)
{
	ESource *source;
	g_return_if_fail (icidata != NULL);

	source = e_source_selector_peek_primary_selection ((ESourceSelector *)icidata->selector);
	g_return_if_fail ( source != NULL);

	icidata->client = auth_new_cal_from_source (source, icidata->source_type);
	e_cal_open (icidata->client, FALSE, NULL);

	switch (icidata->source_type) {
	case E_CAL_SOURCE_TYPE_EVENT:
		prepare_events (icidata->icalcomp, NULL);
		if (!update_objects (icidata->client, icidata->icalcomp))
			/* FIXME: e_error ... */;
		break;
	case E_CAL_SOURCE_TYPE_TODO:
		prepare_tasks (icidata->icalcomp, NULL);
		if (!update_objects (icidata->client, icidata->icalcomp))
			/* FIXME: e_error ... */;
		break;
	default:
		g_assert_not_reached ();
	}
	ical_import_done (icidata);
}

static gboolean
update_objects (ECal *client, icalcomponent *icalcomp)
{
	icalcomponent_kind kind;
	icalcomponent *vcal;
	gboolean success = TRUE;

	kind = icalcomponent_isa (icalcomp);

	if (kind == ICAL_VTODO_COMPONENT || kind == ICAL_VEVENT_COMPONENT) {
		vcal = e_cal_util_new_top_level ();
		if (icalcomponent_get_method (icalcomp) == ICAL_METHOD_CANCEL)
			icalcomponent_set_method (vcal, ICAL_METHOD_CANCEL);
		else
			icalcomponent_set_method (vcal, ICAL_METHOD_PUBLISH);
		icalcomponent_add_component (vcal, icalcomponent_new_clone (icalcomp));
	} else if (kind == ICAL_VCALENDAR_COMPONENT) {
		vcal = icalcomponent_new_clone (icalcomp);
		if (!icalcomponent_get_first_property (vcal, ICAL_METHOD_PROPERTY))
			icalcomponent_set_method (vcal, ICAL_METHOD_PUBLISH);
	} else
		return FALSE;

	if (!e_cal_receive_objects (client, vcal, NULL))
		success = FALSE;

	icalcomponent_free (vcal);

	return success;
}

static void
ical_import_done(ICalImporterData *icidata)
{
	g_object_unref (icidata->client);
	icalcomponent_free (icidata->icalcomp);
	gtk_signal_emit_by_name (GTK_DIALOG(icidata->window), "close");
	g_free (icidata);
}

static icalcomponent *
get_icalcomponent_from_file(char *filename)
{
	char *contents;
	icalcomponent *icalcomp;

	g_return_val_if_fail (filename != NULL, NULL);

	if (!g_file_get_contents (filename, &contents, NULL, NULL)) {
		g_free (filename);
		return NULL;
	}
	g_free (filename);

	icalcomp = e_cal_util_parse_ics_string (contents);
	g_free (contents);

	if (icalcomp) {
		return icalcomp;
	}
	return NULL;
}
