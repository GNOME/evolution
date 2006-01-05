/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
#include <fcntl.h>
#include <stdlib.h>

#include <glib.h>

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime.h>

#include "e-cal-popup.h"
#include <libedataserverui/e-source-selector.h>

#include <camel/camel-mime-part.h>
#include <camel/camel-stream-fs.h>
#include "e-util/e-util.h"
#include "e-util/e-i18n.h"
#include "e-util/e-mktemp.h"
#include "e-util/e-dialog-utils.h"

#include "gui/e-calendar-view.h"
#include "gui/e-cal-model.h"
#include "itip-utils.h"
#include "e-attachment.h"

static GObjectClass *ecalp_parent;

static void
ecalp_init(GObject *o)
{
	/*ECalPopup *eabp = (ECalPopup *)o; */
}

static void
ecalp_finalise(GObject *o)
{
	((GObjectClass *)ecalp_parent)->finalize(o);
}

static void
ecalp_target_free(EPopup *ep, EPopupTarget *t)
{
	switch (t->type) {
	case E_CAL_POPUP_TARGET_SELECT: {
		ECalPopupTargetSelect *s = (ECalPopupTargetSelect *)t;
		int i;

		for (i=0;i<s->events->len;i++)
			e_cal_model_free_component_data(s->events->pdata[i]);
		g_ptr_array_free(s->events, TRUE);
		g_object_unref(s->model);
		break; }
	case E_CAL_POPUP_TARGET_SOURCE: {
		ECalPopupTargetSource *s = (ECalPopupTargetSource *)t;

		g_object_unref(s->selector);
		break; }
	}

	((EPopupClass *)ecalp_parent)->target_free(ep, t);
}

/* Standard menu code */

static char *
temp_save_part(CamelMimePart *part, char *path, gboolean file)
{
	const char *filename;
	char *tmpdir, *mfilename = NULL;
	CamelStream *stream;
	CamelDataWrapper *wrapper;

	if (!path) {
		tmpdir = e_mkdtemp("evolution-tmp-XXXXXX");
		if (tmpdir == NULL) {
			return NULL;
		}

		filename = camel_mime_part_get_filename (part);
		if (filename == NULL) {
			/* This is the default filename used for temporary file creation */
			filename = _("Unknown");
		} else {
			mfilename = g_strdup(filename);
			e_filename_make_safe(mfilename);
			filename = mfilename;
		}

		path = g_build_filename(tmpdir, filename, NULL);
		g_free(tmpdir);
		g_free(mfilename);
	} else if (!file) {
		tmpdir = path;
		filename = camel_mime_part_get_filename (part);
		if (filename == NULL) {
			/* This is the default filename used for temporary file creation */
			filename = _("Unknown");
		} else {
			mfilename = g_strdup(filename);
			e_filename_make_safe(mfilename);
			filename = mfilename;
		}
		
		path = g_build_filename(tmpdir, filename, NULL);
		g_free(mfilename);
	}

	wrapper = camel_medium_get_content_object (CAMEL_MEDIUM (part));
	stream = camel_stream_fs_new_with_name (path, O_RDWR|O_CREAT|O_TRUNC, 0600);

	if (!stream) {
		/* TODO handle error conditions */
		g_message ("DEBUG: could not open the file to write\n");
		return NULL;
	}

	if (camel_data_wrapper_decode_to_stream (wrapper, (CamelStream *) stream) == -1) {
		camel_stream_close (stream);
		camel_object_unref (stream);
		g_message ("DEBUG: could not write to file\n");
		return NULL;
	}

	camel_stream_close(stream);
	camel_object_unref(stream);

	return path;
}

static void
ecalp_part_popup_saveas(EPopup *ep, EPopupItem *item, void *data)
{
	EPopupTarget *t = ep->target;
	CamelMimePart *part = NULL;
	char *file, *filename, *mfilename = NULL;

	part = ((EAttachment *) ((ECalPopupTargetAttachments *) t)->attachments->data)->body;
	filename = camel_mime_part_get_filename (part);
	if (filename == NULL) {
		/* This is the default filename used for temporary file creation */
		filename = _("Unknown");
	} else {
		mfilename = g_strdup(filename);
		e_filename_make_safe(mfilename);
		filename = mfilename;
	}	
	file = e_file_dialog_save (_("Save As..."), filename);
	
	if (file)
		temp_save_part (part, file, TRUE);
		
	g_free (file);
	g_free (mfilename);
}

static void
ecalp_part_popup_save_selected(EPopup *ep, EPopupItem *item, void *data)
{
	GSList *parts;
	EPopupTarget *t = ep->target;
	char *dir, *path;
	
	dir = e_file_dialog_save_folder (_("Select folder to save selected attachments..."));
	parts = ((ECalPopupTargetAttachments *) t)->attachments;
	
	for (;parts; parts=parts->next) {
		path = temp_save_part (((EAttachment *)parts->data)->body, dir, FALSE);
		/* Probably we 'll do some reporting in next release, like listing the saved files and locations */
		g_free (path);
	}
}

static void
ecalp_part_popup_set_background(EPopup *ep, EPopupItem *item, void *data)
{
	EPopupTarget *t = ep->target;
	GConfClient *gconf;
	char *str, *filename, *path, *extension;
	unsigned int i=1;
	CamelMimePart *part = NULL;

	part = ((EAttachment *) ((ECalPopupTargetAttachments *) t)->attachments->data)->body;
	
	filename = g_strdup(camel_mime_part_get_filename(part));
	   
	/* if filename is blank, create a default filename based on MIME type */
	if (!filename || !filename[0]) {
		CamelContentType *ct;

		ct = camel_mime_part_get_content_type(part);
		g_free (filename);
		filename = g_strdup_printf (_("untitled_image.%s"), ct->subtype);
	}

	e_filename_make_safe(filename);
	
	path = g_build_filename(g_get_home_dir(), ".gnome2", "wallpapers", filename, NULL);
	
	extension = strrchr(filename, '.');
	if (extension)
		*extension++ = 0;
	
	/* if file exists, stick a (number) on the end */
	while (g_file_test(path, G_FILE_TEST_EXISTS)) {
		char *name;
		name = g_strdup_printf(extension?"%s (%d).%s":"%s (%d)", filename, i++, extension);
		g_free(path);
		path = g_build_filename(g_get_home_dir(), ".gnome2", "wallpapers", name, NULL);
		g_free(name);
	}
	
	g_free(filename);
	
	if (temp_save_part(part, path, TRUE)) {
		gconf = gconf_client_get_default();
		
		/* if the filename hasn't changed, blank the filename before 
		*  setting it so that gconf detects a change and updates it */
		if ((str = gconf_client_get_string(gconf, "/desktop/gnome/background/picture_filename", NULL)) != NULL 
		     && strcmp (str, path) == 0) {
			gconf_client_set_string(gconf, "/desktop/gnome/background/picture_filename", "", NULL);
		}
		
		g_free (str);
		gconf_client_set_string(gconf, "/desktop/gnome/background/picture_filename", path, NULL);
		
		/* if GNOME currently doesn't display a picture, set to "wallpaper"
		 * display mode, otherwise leave it alone */
		if ((str = gconf_client_get_string(gconf, "/desktop/gnome/background/picture_options", NULL)) == NULL 
		     || strcmp(str, "none") == 0) {
			gconf_client_set_string(gconf, "/desktop/gnome/background/picture_options", "wallpaper", NULL);
		}
		
		gconf_client_suggest_sync(gconf, NULL);
		
		g_free(str);
		g_object_unref(gconf);
	}
	
	g_free(path);
}

static const EPopupItem ecalp_standard_part_apps_bar = { E_POPUP_BAR, "99.object" };

static ECalPopupItem ecalp_attachment_object_popups[] = {
	{ E_POPUP_ITEM, "00.attach.00", N_("_Save As..."), ecalp_part_popup_saveas, NULL, "stock_save-as", E_CAL_POPUP_ATTACHMENTS_ONE },
	{ E_POPUP_ITEM, "00.attach.10", N_("Set as _Background"), ecalp_part_popup_set_background, NULL, NULL, E_CAL_POPUP_ATTACHMENTS_IMAGE },
	{ E_POPUP_ITEM, "00.attach.20", N_("_Save Selected"), ecalp_part_popup_save_selected, NULL, "stock_save-as", E_CAL_POPUP_ATTACHMENTS_MULTIPLE },
	{ E_POPUP_BAR, "05.attach", },
};

static void
ecalp_apps_open_in(EPopup *ep, EPopupItem *item, void *data)
{
	char *path;
	EPopupTarget *target = ep->target;
	CamelMimePart *part;

	part = ((EAttachment *) ((ECalPopupTargetAttachments *) target)->attachments->data)->body;

	path = temp_save_part(part, NULL, FALSE);
	if (path) {
		GnomeVFSMimeApplication *app = item->user_data;
		char *uri;
		GList *uris = NULL;
		
		uri = gnome_vfs_get_uri_from_local_path(path);
		uris = g_list_append(uris, uri);

		gnome_vfs_mime_application_launch(app, uris);

		g_free(uri);
		g_list_free(uris);
		g_free(path);
	}
}

static void
ecalp_apps_popup_free(EPopup *ep, GSList *free_list, void *data)
{
	while (free_list) {
		GSList *n = free_list->next;
		EPopupItem *item = free_list->data;

		g_free(item->path);
		g_free(item->label);
		g_free(item);
		g_slist_free_1(free_list);

		free_list = n;
	}
}

static void
ecalp_standard_items_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}

static void 
ecalp_standard_menu_factory (EPopup *ecalp, void *data)
{
	int i, len;
	EPopupItem *items;
	GSList *menus = NULL;
	GList *apps = NULL;
	char *mime_type = NULL;
	const char *filename = NULL;

	switch (ecalp->target->type) {
	case E_CAL_POPUP_TARGET_ATTACHMENTS: {
		ECalPopupTargetAttachments *t = (ECalPopupTargetAttachments *)ecalp->target;
		GSList *list = t->attachments;
		EAttachment *attachment;

		items = ecalp_attachment_object_popups;
		len = G_N_ELEMENTS(ecalp_attachment_object_popups);

		if (g_slist_length(list) != 1 || !((EAttachment *)list->data)->is_available_local) {
			break;
		}

		/* Only one attachment selected */
		attachment = list->data;
		mime_type = camel_data_wrapper_get_mime_type((CamelDataWrapper *)attachment->body);
		filename = camel_mime_part_get_filename(attachment->body);


		break; }		
	default:
		items = NULL;
		len = 0;	
	}

	if (mime_type) {
		apps = gnome_vfs_mime_get_all_applications(mime_type);
		
		if (apps == NULL && strcmp(mime_type, "application/octet-stream") == 0) {
			const char *name_type;
			
			if (filename) {
				/* GNOME-VFS will misidentify TNEF attachments as MPEG */
				if (!strcmp (filename, "winmail.dat"))
					name_type = "application/vnd.ms-tnef";
				else
					name_type = gnome_vfs_mime_type_from_name(filename);
				if (name_type)
					apps = gnome_vfs_mime_get_all_applications(name_type);
			}
		}
		g_free (mime_type);

		if (apps) {
			GString *label = g_string_new("");
			GSList *open_menus = NULL;
			GList *l;

			menus = g_slist_prepend(menus, (void *)&ecalp_standard_part_apps_bar);

			for (l = apps, i = 0; l; l = l->next, i++) {
				GnomeVFSMimeApplication *app = l->data;
				EPopupItem *item;

				if (app->requires_terminal)
					continue;

				item = g_malloc0(sizeof(*item));
				item->type = E_POPUP_ITEM;
				item->path = g_strdup_printf("99.object.%02d", i);
				item->label = g_strdup_printf(_("Open in %s..."), app->name);
				item->activate = ecalp_apps_open_in;
				item->user_data = app;

				open_menus = g_slist_prepend(open_menus, item);
			}

			if (open_menus)
				e_popup_add_items(ecalp, open_menus, NULL, ecalp_apps_popup_free, NULL);

			g_string_free(label, TRUE);
			g_list_free(apps);
		}
	}

	for (i=0;i<len;i++) {
		if ((items[i].visible & ecalp->target->mask) == 0)
			menus = g_slist_prepend(menus, &items[i]);
	}

	if (menus)
		e_popup_add_items(ecalp, menus, NULL, ecalp_standard_items_free, NULL);
}

static void
ecalp_class_init(GObjectClass *klass)
{
	klass->finalize = ecalp_finalise;
	((EPopupClass *)klass)->target_free = ecalp_target_free;

	e_popup_class_add_factory((EPopupClass *)klass, NULL, ecalp_standard_menu_factory, NULL);
}

GType
e_cal_popup_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(ECalPopupClass),
			NULL, NULL,
			(GClassInitFunc)ecalp_class_init,
			NULL, NULL,
			sizeof(ECalPopup), 0,
			(GInstanceInitFunc)ecalp_init
		};
		ecalp_parent = g_type_class_ref(e_popup_get_type());
		type = g_type_register_static(e_popup_get_type(), "ECalPopup", &info, 0);
	}

	return type;
}

ECalPopup *e_cal_popup_new(const char *menuid)
{
	ECalPopup *eabp = g_object_new(e_cal_popup_get_type(), 0);

	e_popup_construct(&eabp->popup, menuid);

	return eabp;
}

static icalproperty *
get_attendee_prop (icalcomponent *icalcomp, const char *address)
{
	
	icalproperty *prop;

	if (!(address && *address))
		return NULL;
	
	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
			prop;
			prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const char *attendee = icalproperty_get_attendee (prop);

		if (g_str_equal (itip_strip_mailto (attendee), address)) {
			return prop;
		}
	}
	return NULL;
}
	
static gboolean 
is_delegated (icalcomponent *icalcomp, char *user_email)
{
	icalproperty *prop;
	icalparameter *param;
	const char *delto = NULL;
	
	prop = get_attendee_prop (icalcomp, user_email);

	if (prop) {
		param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDTO_PARAMETER);
		delto = icalparameter_get_delegatedto (param);
	} else
		return FALSE;
	
	prop = get_attendee_prop (icalcomp, itip_strip_mailto (delto));	

	if (prop) {
		const char *delfrom;
		icalparameter_partstat	status;

		param = icalproperty_get_first_parameter (prop, ICAL_DELEGATEDFROM_PARAMETER);
		delfrom = icalparameter_get_delegatedfrom (param);
		param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		status = icalparameter_get_partstat (param);
		if ((delfrom && *delfrom) && g_str_equal (itip_strip_mailto (delfrom), user_email)
				&& status != ICAL_PARTSTAT_DECLINED)
			return TRUE;
	}

	return FALSE;	
}

static gboolean
needs_to_accept (icalcomponent *icalcomp, char *user_email) 
{
	icalproperty *prop;
	icalparameter *param;
	icalparameter_partstat status;
	
	prop = get_attendee_prop (icalcomp, user_email);

	/* It might be a mailing list */
	if (!prop)	
		return TRUE;
	param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
	status = icalparameter_get_partstat (param);

	if (status == ICAL_PARTSTAT_ACCEPTED || status == ICAL_PARTSTAT_TENTATIVE)
		return FALSE;

	return TRUE;
}

/**
 * e_cal_popup_target_new_select:
 * @eabp:
 * @model: The calendar model.
 * @events: An array of pointers to ECalModelComponent items.  These
 * items must be copied.  They, and the @events array will be freed by
 * the popup menu automatically.
 * 
 * Create a new selection popup target.
 * 
 * Return value: 
 **/
ECalPopupTargetSelect *
e_cal_popup_target_new_select(ECalPopup *eabp, struct _ECalModel *model, GPtrArray *events)
{
	ECalPopupTargetSelect *t = e_popup_target_new(&eabp->popup, E_CAL_POPUP_TARGET_SELECT, sizeof(*t));
	guint32 mask = ~0;
	ECal *client;
	gboolean read_only, user_org = FALSE;

	/* FIXME: This is duplicated in e-cal-menu */

	t->model = model;
	g_object_ref(t->model);
	t->events = events;
	
	if (t->events->len == 0) {
		client = e_cal_model_get_default_client(t->model);
	} else {
		ECalModelComponent *comp_data = (ECalModelComponent *)t->events->pdata[0];
		ECalComponent *comp;
		char *user_email = NULL;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
		user_email = itip_get_comp_attendee (comp, comp_data->client);

		mask &= ~E_CAL_POPUP_SELECT_ANY;
		if (t->events->len == 1)
			mask &= ~E_CAL_POPUP_SELECT_ONE;
		else {
			mask &= ~E_CAL_POPUP_SELECT_MANY;
			/* Now check for any incomplete tasks and set the flags*/
			int i=0;
			for (; i < t->events->len; i++) {
				ECalModelComponent *comp_data = (ECalModelComponent *)t->events->pdata[i];
				if (!icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY)) {
					mask &= ~E_CAL_POPUP_SELECT_NOTCOMPLETE;
					break;
				}
			}
		}

		if (icalcomponent_get_first_property (comp_data->icalcomp, ICAL_URL_PROPERTY))
			mask &= ~E_CAL_POPUP_SELECT_HASURL;

		if (e_cal_util_component_has_recurrences (comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_RECURRING;
		else if (e_cal_util_component_is_instance (comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_RECURRING;
		else
			mask &= ~E_CAL_POPUP_SELECT_NONRECURRING;

		if (e_cal_util_component_is_instance (comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_INSTANCE;

		if (e_cal_util_component_has_attendee (comp_data->icalcomp))
			mask &= ~E_CAL_POPUP_SELECT_MEETING;

		if (e_cal_util_component_has_organizer (comp_data->icalcomp)) {

			if (itip_organizer_is_user (comp, comp_data->client)) {
				mask &= ~E_CAL_POPUP_SELECT_ORGANIZER;
				user_org = TRUE;
			}

		} else {
			/* organiser is synonym for owner in this case */
			mask &= ~(E_CAL_POPUP_SELECT_ORGANIZER|E_CAL_POPUP_SELECT_NOTMEETING);
		}

		client = comp_data->client;

		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_DELEGATE_SUPPORTED)) {

			if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY))
				mask &= ~E_CAL_POPUP_SELECT_DELEGATABLE;
			else if (!user_org && !is_delegated (comp_data->icalcomp, user_email))
				mask &= ~E_CAL_POPUP_SELECT_DELEGATABLE;
		}

		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_HAS_UNACCEPTED_MEETING) &&
				needs_to_accept (comp_data->icalcomp, user_email))
			mask &= ~E_CAL_POPUP_SELECT_ACCEPTABLE;

		if (!icalcomponent_get_first_property (comp_data->icalcomp, ICAL_COMPLETED_PROPERTY))
			mask &= ~E_CAL_POPUP_SELECT_NOTCOMPLETE;

		g_object_unref (comp);
		g_free (user_email);
	}

	e_cal_is_read_only(client, &read_only, NULL);
	if (!read_only)
		mask &= ~E_CAL_POPUP_SELECT_EDITABLE;

	
		
	if (!e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT)
	    && !e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_CONV_TO_ASSIGN_TASK))
		mask &= ~E_CAL_POPUP_SELECT_ASSIGNABLE;

	/* This bit isn't implemented ... */
	mask &= ~E_CAL_POPUP_SELECT_NOTEDITING;

	t->target.mask = mask;

	return t;
}

ECalPopupTargetSource *
e_cal_popup_target_new_source(ECalPopup *eabp, ESourceSelector *selector)
{
	ECalPopupTargetSource *t = e_popup_target_new(&eabp->popup, E_CAL_POPUP_TARGET_SOURCE, sizeof(*t));
	guint32 mask = ~0;
	const char *source_uri;
	ESource *source;
	const char *offline = NULL;

	/* TODO: this is duplicated for addressbook too */

	t->selector = selector;
	g_object_ref(selector);

	/* TODO: perhaps we need to copy this so it doesn't change during the lifecycle */
	source = e_source_selector_peek_primary_selection(selector);
	if (source)
		mask &= ~E_CAL_POPUP_SOURCE_PRIMARY;

	/* FIXME Gross hack, should have a property or something */
	source_uri = e_source_peek_relative_uri(source);
	if (source_uri && !strcmp("system", source_uri))
		mask &= ~E_CAL_POPUP_SOURCE_SYSTEM;
	else
		mask &= ~E_CAL_POPUP_SOURCE_USER;


	source = e_source_selector_peek_primary_selection (selector);
	/* check for e_target_selector's offline_status property here */
	offline = e_source_get_property (source, "offline");

	if (offline  && strcmp (offline,"1") == 0) { 
		/* set the menu item to Mark Offline - */
		mask &= ~E_CAL_POPUP_SOURCE_NO_OFFLINE;
	}
	else {
		mask &= ~E_CAL_POPUP_SOURCE_OFFLINE;
	}

	t->target.mask = mask;

	return t;
}

/**
 * e_cal_popup_target_new_attachments:
 * @ecp: 
 * @attachments: A list of CalAttachment objects, reffed for
 * the list.  Will be unreff'd once finished with.
 * 
 * Owns the list @attachments and their items after they're passed in.
 * 
 * Return value: 
 **/
ECalPopupTargetAttachments *
e_cal_popup_target_new_attachments(ECalPopup *ecp, CompEditor *editor, GSList *attachments)
{
	ECalPopupTargetAttachments *t = e_popup_target_new(&ecp->popup, E_CAL_POPUP_TARGET_ATTACHMENTS, sizeof(*t));
	guint32 mask = ~0;
	int len = g_slist_length(attachments);
	ECal *client = comp_editor_get_e_cal (editor);
	CompEditorFlags flags = comp_editor_get_flags (editor);
	gboolean read_only = FALSE;
	GError *error = NULL;

	if (!e_cal_is_read_only (client, &read_only, &error)) {
		if (error->code != E_CALENDAR_STATUS_BUSY)
			read_only = TRUE;
		g_error_free (error);
	}	

	if (!read_only && (!(flags & COMP_EDITOR_MEETING) || 
				(flags & COMP_EDITOR_NEW_ITEM) || 
				(flags & COMP_EDITOR_USER_ORG)))
		mask &= ~ E_CAL_POPUP_ATTACHMENTS_MODIFY;

	t->attachments = attachments;
	if (len > 0)
		mask &= ~ E_CAL_POPUP_ATTACHMENTS_MANY;

	if (len == 1 && ((EAttachment *)attachments->data)->is_available_local) {
		if (camel_content_type_is(((CamelDataWrapper *) ((EAttachment *) attachments->data)->body)->mime_type, "image", "*"))
			mask &= ~ E_CAL_POPUP_ATTACHMENTS_IMAGE;
		mask &= ~ E_CAL_POPUP_ATTACHMENTS_ONE;
	}

	if (len > 1)
		mask &= ~ E_CAL_POPUP_ATTACHMENTS_MULTIPLE;
	
	t->target.mask = mask;

	return t;
}

/* ********************************************************************** */
/* Popup menu plugin handler */

/*
<e-plugin
  class="org.gnome.mail.plugin.popup:1.0"
  id="org.gnome.mail.plugin.popup.iteab:1.0"
  type="shlib"
  location="/opt/gnome2/lib/camel/1.0/libcamelimap.so"
  name="imap"
  description="IMAP4 and IMAP4v1 mail store">
  <hook class="org.gnome.mail.popupMenu:1.0"
        handler="HandlePopup">
  <menu id="any" target="select">
   <iteab
    type="iteab|toggle|radio|image|submenu|bar"
    active
    path="foo/bar"
    label="label"
    icon="foo"
    mask="select_one"
    activate="ecalp_view_eabacs"/>
  </menu>
  </extension>

*/

static void *ecalph_parent_class;
#define ecalph ((ECalPopupHook *)eph)

static const EPopupHookTargetMask ecalph_select_masks[] = {
	{ "one", E_CAL_POPUP_SELECT_ONE },
	{ "many", E_CAL_POPUP_SELECT_MANY },
	{ "editable", E_CAL_POPUP_SELECT_EDITABLE },
	{ "recurring", E_CAL_POPUP_SELECT_RECURRING },
	{ "non-recurring", E_CAL_POPUP_SELECT_NONRECURRING },
	{ "instance", E_CAL_POPUP_SELECT_INSTANCE },
	{ "organizer", E_CAL_POPUP_SELECT_ORGANIZER },
	{ "not-editing", E_CAL_POPUP_SELECT_NOTEDITING },
	{ "not-meeting", E_CAL_POPUP_SELECT_NOTMEETING },
	{ "meeting", E_CAL_POPUP_SELECT_MEETING },
	{ "assignable", E_CAL_POPUP_SELECT_ASSIGNABLE },
	{ "hasurl", E_CAL_POPUP_SELECT_HASURL },
	{ "delegate", E_CAL_POPUP_SELECT_DELEGATABLE }, 
	{ "accept", E_CAL_POPUP_SELECT_ACCEPTABLE },
	{ "not-complete", E_CAL_POPUP_SELECT_NOTCOMPLETE },
	{ 0 }
};

static const EPopupHookTargetMask ecalph_source_masks[] = {
	{ "primary", E_CAL_POPUP_SOURCE_PRIMARY },
	{ "system", E_CAL_POPUP_SOURCE_SYSTEM },
	{ "user", E_CAL_POPUP_SOURCE_USER },
	{ "offline", E_CAL_POPUP_SOURCE_OFFLINE},
	{ "no-offline", E_CAL_POPUP_SOURCE_NO_OFFLINE},	
	{ 0 }
};

static const EPopupHookTargetMask ecalph_attachments_masks[] = {
	{ "one", E_CAL_POPUP_ATTACHMENTS_ONE },
	{ "many", E_CAL_POPUP_ATTACHMENTS_MANY },
	{ "modify", E_CAL_POPUP_ATTACHMENTS_MODIFY },
	{ "multiple", E_CAL_POPUP_ATTACHMENTS_MULTIPLE },
	{ "image", E_CAL_POPUP_ATTACHMENTS_IMAGE },
	{ 0 }
};

static const EPopupHookTargetMap ecalph_targets[] = {
	{ "select", E_CAL_POPUP_TARGET_SELECT, ecalph_select_masks },
	{ "source", E_CAL_POPUP_TARGET_SOURCE, ecalph_source_masks },
	{ "attachments", E_CAL_POPUP_TARGET_ATTACHMENTS, ecalph_attachments_masks },
	{ 0 }
};

static void
ecalph_finalise(GObject *o)
{
	/*EPluginHook *eph = (EPluginHook *)o;*/

	((GObjectClass *)ecalph_parent_class)->finalize(o);
}

static void
ecalph_class_init(EPluginHookClass *klass)
{
	int i;

	((GObjectClass *)klass)->finalize = ecalph_finalise;
	((EPluginHookClass *)klass)->id = "org.gnome.evolution.calendar.popup:1.0";

	for (i=0;ecalph_targets[i].type;i++)
		e_popup_hook_class_add_target_map((EPopupHookClass *)klass, &ecalph_targets[i]);

	((EPopupHookClass *)klass)->popup_class = g_type_class_ref(e_cal_popup_get_type());
}

GType
e_cal_popup_hook_get_type(void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof(ECalPopupHookClass), NULL, NULL, (GClassInitFunc) ecalph_class_init, NULL, NULL,
			sizeof(ECalPopupHook), 0, (GInstanceInitFunc) NULL,
		};

		ecalph_parent_class = g_type_class_ref(e_popup_hook_get_type());
		type = g_type_register_static(e_popup_hook_get_type(), "ECalPopupHook", &info, 0);
	}
	
	return type;
}
