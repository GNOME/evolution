/*
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef E_COMP_EDITOR_H
#define E_COMP_EDITOR_H

#include <gtk/gtk.h>
#include <libecal/libecal.h>

#include <e-util/e-util.h>
#include <shell/e-shell.h>

#include <calendar/gui/e-comp-editor-page.h>
#include <calendar/gui/e-comp-editor-property-part.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR \
	(e_comp_editor_get_type ())
#define E_COMP_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR, ECompEditor))
#define E_COMP_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR, ECompEditorClass))
#define E_IS_COMP_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR))
#define E_IS_COMP_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR))
#define E_COMP_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR, ECompEditorClass))

G_BEGIN_DECLS

typedef enum {
	E_COMP_EDITOR_FLAG_IS_NEW		= 1 << 0,
	E_COMP_EDITOR_FLAG_IS_ALL_DAY_EVENT	= 1 << 1,
	E_COMP_EDITOR_FLAG_WITH_ATTENDEES	= 1 << 2,
	E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER	= 1 << 3,
	E_COMP_EDITOR_FLAG_DELEGATE		= 1 << 4
} ECompEditorFlags;

typedef struct _ECompEditor ECompEditor;
typedef struct _ECompEditorClass ECompEditorClass;
typedef struct _ECompEditorPrivate ECompEditorPrivate;

struct _ECompEditor {
	GtkWindow parent;

	ECompEditorPrivate *priv;
};

struct _ECompEditorClass {
	GtkWindowClass parent_class;

	const gchar *help_section;
	const gchar *title_format_with_attendees; /* should contain only one '%s', for the component summary */
	const gchar *title_format_without_attendees; /* should contain only one '%s', for the component summary */
	const gchar *icon_name; /* to be set as window icon */

	/* Virtual functions */
	void		(* sensitize_widgets)	(ECompEditor *comp_editor,
						 gboolean force_insensitive);
	void		(* fill_widgets)	(ECompEditor *comp_editor,
						 ICalComponent *component);
	gboolean	(* fill_component)	(ECompEditor *comp_editor,
						 ICalComponent *component);

	/* Signals */
	void		(* times_changed)	(ECompEditor *comp_editor);
	void		(* object_created)	(ECompEditor *comp_editor);
	void		(* editor_closed)	(ECompEditor *comp_editor,
						 gboolean saved);
};

GType		e_comp_editor_get_type		(void) G_GNUC_CONST;
void		e_comp_editor_sensitize_widgets	(ECompEditor *comp_editor);
void		e_comp_editor_fill_widgets	(ECompEditor *comp_editor,
						 ICalComponent *component);
gboolean	e_comp_editor_fill_component	(ECompEditor *comp_editor,
						 ICalComponent *component);
void		e_comp_editor_set_validation_error
						(ECompEditor *comp_editor,
						 ECompEditorPage *error_page,
						 GtkWidget *error_widget,
						 const gchar *error_message);
EShell *	e_comp_editor_get_shell		(ECompEditor *comp_editor);
GSettings *	e_comp_editor_get_settings	(ECompEditor *comp_editor);
ESource *	e_comp_editor_get_origin_source	(ECompEditor *comp_editor);
ICalComponent *	e_comp_editor_get_component	(ECompEditor *comp_editor);
guint32		e_comp_editor_get_flags		(ECompEditor *comp_editor);
void		e_comp_editor_set_flags		(ECompEditor *comp_editor,
						 guint32 flags);
EFocusTracker *	e_comp_editor_get_focus_tracker	(ECompEditor *comp_editor);
EUIManager *	e_comp_editor_get_ui_manager	(ECompEditor *comp_editor);
EUIAction *	e_comp_editor_get_action	(ECompEditor *comp_editor,
						 const gchar *action_name);
const gchar *	e_comp_editor_get_alarm_email_address
						(ECompEditor *comp_editor);
void		e_comp_editor_set_alarm_email_address
						(ECompEditor *comp_editor,
						 const gchar *alarm_email_address);
const gchar *	e_comp_editor_get_cal_email_address
						(ECompEditor *comp_editor);
void		e_comp_editor_set_cal_email_address
						(ECompEditor *comp_editor,
						 const gchar *cal_email_address);
gboolean	e_comp_editor_get_changed	(ECompEditor *comp_editor);
void		e_comp_editor_set_changed	(ECompEditor *comp_editor,
						 gboolean changed);
void		e_comp_editor_ensure_changed	(ECompEditor *comp_editor);
gboolean	e_comp_editor_get_updating	(ECompEditor *comp_editor);
void		e_comp_editor_set_updating	(ECompEditor *comp_editor,
						 gboolean updating);
ECalClient *	e_comp_editor_get_source_client	(ECompEditor *comp_editor);
void		e_comp_editor_set_source_client	(ECompEditor *comp_editor,
						 ECalClient *client);
ECalClient *	e_comp_editor_get_target_client	(ECompEditor *comp_editor);
void		e_comp_editor_set_target_client	(ECompEditor *comp_editor,
						 ECalClient *client);
const gchar *	e_comp_editor_get_title_suffix	(ECompEditor *comp_editor);
void		e_comp_editor_set_title_suffix	(ECompEditor *comp_editor,
						 const gchar *title_suffix);
void		e_comp_editor_set_time_parts	(ECompEditor *comp_editor,
						 ECompEditorPropertyPart *dtstart_part,
						 ECompEditorPropertyPart *dtend_part);
void		e_comp_editor_get_time_parts	(ECompEditor *comp_editor,
						 ECompEditorPropertyPart **out_dtstart_part,
						 ECompEditorPropertyPart **out_dtend_part);
void		e_comp_editor_add_page		(ECompEditor *comp_editor,
						 const gchar *label,
						 ECompEditorPage *page);
void		e_comp_editor_add_encapsulated_page
						(ECompEditor *comp_editor,
						 const gchar *label,
						 ECompEditorPage *page,
						 GtkWidget *container);
ECompEditorPage *
		e_comp_editor_get_page		(ECompEditor *comp_editor,
						 GType page_type);
ECompEditorPropertyPart *
		e_comp_editor_get_property_part	(ECompEditor *comp_editor,
						 ICalPropertyKind prop_kind);
GSList *	e_comp_editor_get_pages		(ECompEditor *comp_editor);
void		e_comp_editor_select_page	(ECompEditor *comp_editor,
						 ECompEditorPage *page);
EAlert *	e_comp_editor_add_information	(ECompEditor *comp_editor,
						 const gchar *primary_text,
						 const gchar *secondary_text);
EAlert *	e_comp_editor_add_warning	(ECompEditor *comp_editor,
						 const gchar *primary_text,
						 const gchar *secondary_text);
EAlert *	e_comp_editor_add_error		(ECompEditor *comp_editor,
						 const gchar *primary_text,
						 const gchar *secondary_text);
void		e_comp_editor_ensure_start_before_end
						(ECompEditor *comp_editor,
						 ECompEditorPropertyPart *start_datetime,
						 ECompEditorPropertyPart *end_datetime,
						 gboolean change_end_datetime);
void		e_comp_editor_ensure_same_value_type
						(ECompEditor *comp_editor,
						 ECompEditorPropertyPart *src_datetime,
						 ECompEditorPropertyPart *des_datetime);
ECompEditor *	e_comp_editor_open_for_component
						(GtkWindow *parent,
						 EShell *shell,
						 ESource *origin_source,
						 const ICalComponent *component,
						 guint32 flags /* bit-or of ECompEditorFlags */);
ECompEditor *	e_comp_editor_find_existing_for	(ESource *origin_source,
						 const ICalComponent *component);
ICalTimezone *	e_comp_editor_lookup_timezone	(ECompEditor *comp_editor,
						 const gchar *tzid);
ICalTimezone *	e_comp_editor_lookup_timezone_cb(const gchar *tzid,
						 gpointer user_data, /* ECompEditor * */
						 GCancellable *cancellable,
						 GError **error);

G_END_DECLS

#endif /* E_COMP_EDITOR_H */
