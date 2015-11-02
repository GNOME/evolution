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

#ifndef E_COMP_EDITOR_PROPERTY_PART_H
#define E_COMP_EDITOR_PROPERTY_PART_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <calendar/gui/e-timezone-entry.h>

/* Standard GObject macros */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART \
	(e_comp_editor_property_part_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART, ECompEditorPropertyPart))
#define E_COMP_EDITOR_PROPERTY_PART_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART, ECompEditorPropertyPartClass))
#define E_IS_COMP_EDITOR_PROPERTY_PART(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART))
#define E_IS_COMP_EDITOR_PROPERTY_PART_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART))
#define E_COMP_EDITOR_PROPERTY_PART_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART, ECompEditorPropertyPartClass))

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING \
	(e_comp_editor_property_part_string_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_STRING(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING, ECompEditorPropertyPartString))
#define E_COMP_EDITOR_PROPERTY_PART_STRING_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING, ECompEditorPropertyPartStringClass))
#define E_IS_COMP_EDITOR_PROPERTY_PART_STRING(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING))
#define E_IS_COMP_EDITOR_PROPERTY_PART_STRING_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING))
#define E_COMP_EDITOR_PROPERTY_PART_STRING_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_STRING, ECompEditorPropertyPartStringClass))

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME \
	(e_comp_editor_property_part_datetime_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_DATETIME(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME, ECompEditorPropertyPartDatetime))
#define E_COMP_EDITOR_PROPERTY_PART_DATETIME_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME, ECompEditorPropertyPartDatetimeClass))
#define E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME))
#define E_IS_COMP_EDITOR_PROPERTY_PART_DATETIME_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME))
#define E_COMP_EDITOR_PROPERTY_PART_DATETIME_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_DATETIME, ECompEditorPropertyPartDatetimeClass))

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_SPIN \
	(e_comp_editor_property_part_spin_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_SPIN(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_SPIN, ECompEditorPropertyPartSpin))
#define E_COMP_EDITOR_PROPERTY_PART_SPIN_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_SPIN, ECompEditorPropertyPartSpinClass))
#define E_IS_COMP_EDITOR_PROPERTY_PART_SPIN(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_SPIN))
#define E_IS_COMP_EDITOR_PROPERTY_PART_SPIN_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_SPIN))
#define E_COMP_EDITOR_PROPERTY_PART_SPIN_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_SPIN, ECompEditorPropertyPartSpinClass))

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER \
	(e_comp_editor_property_part_picker_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_PICKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER, ECompEditorPropertyPartPicker))
#define E_COMP_EDITOR_PROPERTY_PART_PICKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER, ECompEditorPropertyPartPickerClass))
#define E_IS_COMP_EDITOR_PROPERTY_PART_PICKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER))
#define E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER))
#define E_COMP_EDITOR_PROPERTY_PART_PICKER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER, ECompEditorPropertyPartPickerClass))

/* ************************************************************************* */

#define E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP \
	(e_comp_editor_property_part_picker_with_map_get_type ())
#define E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP, ECompEditorPropertyPartPickerWithMap))
#define E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP, ECompEditorPropertyPartPickerWithMapClass))
#define E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP))
#define E_IS_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP))
#define E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP, ECompEditorPropertyPartPickerWithMapClass))

/* ************************************************************************* */

G_BEGIN_DECLS

typedef struct _ECompEditorPropertyPart ECompEditorPropertyPart;
typedef struct _ECompEditorPropertyPartClass ECompEditorPropertyPartClass;
typedef struct _ECompEditorPropertyPartPrivate ECompEditorPropertyPartPrivate;

struct _ECompEditorPropertyPart {
	GObject parent;

	ECompEditorPropertyPartPrivate *priv;
};

struct _ECompEditorPropertyPartClass {
	GObjectClass parent_class;

	/* Virtual functions */
	void		(* create_widgets)	(ECompEditorPropertyPart *property_part,
						 GtkWidget **out_label_widget,
						 GtkWidget **out_edit_widget);
	void		(* fill_widget)		(ECompEditorPropertyPart *property_part,
						 icalcomponent *component);
	void		(* fill_component)	(ECompEditorPropertyPart *property_part,
						 icalcomponent *component);

	/* Signals */
	void		(* changed)		(ECompEditorPropertyPart *property_part);
};

GType		e_comp_editor_property_part_get_type		(void) G_GNUC_CONST;
gboolean	e_comp_editor_property_part_get_visible		(ECompEditorPropertyPart *property_part);
void		e_comp_editor_property_part_set_visible		(ECompEditorPropertyPart *property_part,
								 gboolean visible);
void		e_comp_editor_property_part_create_widgets	(ECompEditorPropertyPart *property_part,
								 GtkWidget **out_label_widget,
								 GtkWidget **out_edit_widget);
GtkWidget *	e_comp_editor_property_part_get_label_widget	(ECompEditorPropertyPart *property_part);
GtkWidget *	e_comp_editor_property_part_get_edit_widget	(ECompEditorPropertyPart *property_part);
void		e_comp_editor_property_part_fill_widget		(ECompEditorPropertyPart *property_part,
								 icalcomponent *component);
void		e_comp_editor_property_part_fill_component	(ECompEditorPropertyPart *property_part,
								 icalcomponent *component);
void		e_comp_editor_property_part_emit_changed	(ECompEditorPropertyPart *property_part);

/* ************************************************************************* */

typedef struct _ECompEditorPropertyPartString ECompEditorPropertyPartString;
typedef struct _ECompEditorPropertyPartStringClass ECompEditorPropertyPartStringClass;
typedef struct _ECompEditorPropertyPartStringPrivate ECompEditorPropertyPartStringPrivate;

struct _ECompEditorPropertyPartString {
	ECompEditorPropertyPart parent;

	ECompEditorPropertyPartStringPrivate *priv;
};

struct _ECompEditorPropertyPartStringClass {
	ECompEditorPropertyPartClass parent_class;

	/* What entry GType (derived from GtkEntry or GtkTextView) should be used;
	   the default is the GtkEntry */
	GType entry_type;

	/* ical property kind and its manipulation functions */
	icalproperty_kind ical_prop_kind;
	icalproperty *	(* ical_new_func)	(const gchar *value);
	void		(* ical_set_func)	(icalproperty *prop,
						 const gchar *value);
	const gchar *	(* ical_get_func)	(const icalproperty *prop);
};

GType		e_comp_editor_property_part_string_get_type	(void) G_GNUC_CONST;
void		e_comp_editor_property_part_string_attach_focus_tracker
								(ECompEditorPropertyPartString *part_string,
								 EFocusTracker *focus_tracker);

/* ************************************************************************* */

typedef struct _ECompEditorPropertyPartDatetime ECompEditorPropertyPartDatetime;
typedef struct _ECompEditorPropertyPartDatetimeClass ECompEditorPropertyPartDatetimeClass;
typedef struct _ECompEditorPropertyPartDatetimePrivate ECompEditorPropertyPartDatetimePrivate;

struct _ECompEditorPropertyPartDatetime {
	ECompEditorPropertyPart parent;

	ECompEditorPropertyPartDatetimePrivate *priv;
};

struct _ECompEditorPropertyPartDatetimeClass {
	ECompEditorPropertyPartClass parent_class;

	/* ical property kind and its manipulation functions */
	icalproperty_kind ical_prop_kind;
	icalproperty *	(* ical_new_func)	(struct icaltimetype value);
	void		(* ical_set_func)	(icalproperty *prop,
						 struct icaltimetype value);
	struct icaltimetype
			(* ical_get_func)	(const icalproperty *prop);
};

GType		e_comp_editor_property_part_datetime_get_type	(void) G_GNUC_CONST;
void		e_comp_editor_property_part_datetime_attach_timezone_entry
								(ECompEditorPropertyPartDatetime *part_datetime,
								 ETimezoneEntry *timezone_entry);
void		e_comp_editor_property_part_datetime_set_date_only
								(ECompEditorPropertyPartDatetime *part_datetime,
								 gboolean date_only);
gboolean	e_comp_editor_property_part_datetime_get_date_only
								(ECompEditorPropertyPartDatetime *part_datetime);
void		e_comp_editor_property_part_datetime_set_allow_no_date_set
								(ECompEditorPropertyPartDatetime *part_datetime,
								 gboolean allow_no_date_set);
gboolean	e_comp_editor_property_part_datetime_get_allow_no_date_set
								(ECompEditorPropertyPartDatetime *part_datetime);
void		e_comp_editor_property_part_datetime_set_value	(ECompEditorPropertyPartDatetime *part_datetime,
								 struct icaltimetype value);
struct icaltimetype
		e_comp_editor_property_part_datetime_get_value	(ECompEditorPropertyPartDatetime *part_datetime);
gboolean	e_comp_editor_property_part_datetime_check_validity
								(ECompEditorPropertyPartDatetime *part_datetime,
								 gboolean *out_date_is_valid,
								 gboolean *out_time_is_valid);

/* ************************************************************************* */

typedef struct _ECompEditorPropertyPartSpin ECompEditorPropertyPartSpin;
typedef struct _ECompEditorPropertyPartSpinClass ECompEditorPropertyPartSpinClass;
typedef struct _ECompEditorPropertyPartSpinPrivate ECompEditorPropertyPartSpinPrivate;

struct _ECompEditorPropertyPartSpin {
	ECompEditorPropertyPart parent;

	ECompEditorPropertyPartSpinPrivate *priv;
};

struct _ECompEditorPropertyPartSpinClass {
	ECompEditorPropertyPartClass parent_class;

	/* ical property kind and its manipulation functions */
	icalproperty_kind ical_prop_kind;
	icalproperty *	(* ical_new_func)	(gint value);
	void		(* ical_set_func)	(icalproperty *prop,
						 gint value);
	gint		(* ical_get_func)	(const icalproperty *prop);
};

GType		e_comp_editor_property_part_spin_get_type	(void) G_GNUC_CONST;
void		e_comp_editor_property_part_spin_set_range	(ECompEditorPropertyPartSpin *part_spin,
								 gint min_value,
								 gint max_value);
void		e_comp_editor_property_part_spin_get_range	(ECompEditorPropertyPartSpin *part_spin,
								 gint *out_min_value,
								 gint *out_max_value);

/* ************************************************************************* */

typedef struct _ECompEditorPropertyPartPicker ECompEditorPropertyPartPicker;
typedef struct _ECompEditorPropertyPartPickerClass ECompEditorPropertyPartPickerClass;
typedef struct _ECompEditorPropertyPartPickerPrivate ECompEditorPropertyPartPickerPrivate;

struct _ECompEditorPropertyPartPicker {
	ECompEditorPropertyPart parent;

	ECompEditorPropertyPartPickerPrivate *priv;
};

struct _ECompEditorPropertyPartPickerClass {
	ECompEditorPropertyPartClass parent_class;

	void		(* get_values)		(ECompEditorPropertyPartPicker *part_picker,
						 GSList **out_ids,
						 GSList **out_display_names);
	gboolean	(* get_from_component)	(ECompEditorPropertyPartPicker *part_picker,
						 icalcomponent *component,
						 gchar **out_id);
	void		(* set_to_component)	(ECompEditorPropertyPartPicker *part_picker,
						 const gchar *id,
						 icalcomponent *component);
};

GType		e_comp_editor_property_part_picker_get_type	(void) G_GNUC_CONST;
void		e_comp_editor_property_part_picker_get_values	(ECompEditorPropertyPartPicker *part_picker,
								 GSList **out_ids,
								 GSList **out_display_names);
gboolean	e_comp_editor_property_part_picker_get_from_component
								(ECompEditorPropertyPartPicker *part_picker,
								 icalcomponent *component,
								 gchar **out_id);
void		e_comp_editor_property_part_picker_set_to_component
								(ECompEditorPropertyPartPicker *part_picker,
								 const gchar *id,
								 icalcomponent *component);
const gchar *	e_comp_editor_property_part_picker_get_selected_id
								(ECompEditorPropertyPartPicker *part_picker);
void		e_comp_editor_property_part_picker_set_selected_id
								(ECompEditorPropertyPartPicker *part_picker,
								 const gchar *id);

/* ************************************************************************* */

typedef struct _ECompEditorPropertyPartPickerWithMap ECompEditorPropertyPartPickerWithMap;
typedef struct _ECompEditorPropertyPartPickerWithMapClass ECompEditorPropertyPartPickerWithMapClass;
typedef struct _ECompEditorPropertyPartPickerWithMapPrivate ECompEditorPropertyPartPickerWithMapPrivate;

typedef struct _ECompEditorPropertyPartPickerMap {
	gint value; 		  /* libical enum value */
	const gchar *description; /* user visible description of the value */
	gboolean delete_prop;	  /* whether to delete property from the component when this one is selected */
	gboolean (*matches_func) (gint map_value, gint component_value); /* can be NULL, then 'equal' compare is done */
} ECompEditorPropertyPartPickerMap;

typedef icalproperty *	(* ECompEditorPropertyPartPickerMapICalNewFunc)	(gint value);
typedef void		(* ECompEditorPropertyPartPickerMapICalSetFunc)	(icalproperty *prop,
									 gint value);
typedef gint		(* ECompEditorPropertyPartPickerMapICalGetFunc)	(const icalproperty *prop);

struct _ECompEditorPropertyPartPickerWithMap {
	ECompEditorPropertyPartPicker parent;

	ECompEditorPropertyPartPickerWithMapPrivate *priv;
};

struct _ECompEditorPropertyPartPickerWithMapClass {
	ECompEditorPropertyPartPickerClass parent_class;
};

GType		e_comp_editor_property_part_picker_with_map_get_type
								(void) G_GNUC_CONST;
ECompEditorPropertyPart *
		e_comp_editor_property_part_picker_with_map_new	(const ECompEditorPropertyPartPickerMap map[],
								 gint n_map_elements,
								 const gchar *label,
								 icalproperty_kind ical_prop_kind,
								 ECompEditorPropertyPartPickerMapICalNewFunc ical_new_func,
								 ECompEditorPropertyPartPickerMapICalSetFunc ical_set_func,
								 ECompEditorPropertyPartPickerMapICalGetFunc ical_get_func);
gint		e_comp_editor_property_part_picker_with_map_get_selected
								(ECompEditorPropertyPartPickerWithMap *part_picker_with_map);
void		e_comp_editor_property_part_picker_with_map_set_selected
								(ECompEditorPropertyPartPickerWithMap *part_picker_with_map,
								 gint value);

/* ************************************************************************* */

G_END_DECLS

#endif /* E_COMP_EDITOR_PROPERTY_PART_H */
