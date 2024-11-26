/*
 * SPDX-FileCopyrightText: (C) 2024 Red Hat (www.redhat.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_UI_ACTION_H
#define E_UI_ACTION_H

#include <gio/gio.h>

G_BEGIN_DECLS

struct _EUIActionGroup;
struct _GtkWidget;

#define E_TYPE_UI_ACTION e_ui_action_get_type ()

G_DECLARE_FINAL_TYPE (EUIAction, e_ui_action, E, UI_ACTION, GObject)

typedef void (* EUIActionFunc)	(EUIAction *action,
				 GVariant *value,
				 gpointer user_data);

typedef struct _EUIActionEntry {
	const gchar *name;
	const gchar *icon_name;
	const gchar *label;
	const gchar *accel;
	const gchar *tooltip;
	EUIActionFunc activate;
	const gchar *parameter_type;
	const gchar *state;
	EUIActionFunc change_state;
} EUIActionEntry;

typedef struct _EUIActionEnumEntry {
	const gchar *name;
	const gchar *icon_name;
	const gchar *label;
	const gchar *accel;
	const gchar *tooltip;
	EUIActionFunc activate;
	gint state;
} EUIActionEnumEntry;

/* The EUIAction implements GAction, thus use its API to get to the name and other properties. */

EUIAction *	e_ui_action_new			(const gchar *map_name,
						 const gchar *action_name,
						 const GVariantType *parameter_type);
EUIAction *	e_ui_action_new_stateful	(const gchar *map_name,
						 const gchar *action_name,
						 const GVariantType *parameter_type,
						 GVariant *state);
EUIAction *	e_ui_action_new_from_entry	(const gchar *map_name,
						 const EUIActionEntry *entry,
						 const gchar *translation_domain);
EUIAction *	e_ui_action_new_from_enum_entry	(const gchar *map_name,
						 const EUIActionEnumEntry *entry,
						 const gchar *translation_domain);
const gchar *	e_ui_action_get_map_name	(EUIAction *self);
GVariant *	e_ui_action_ref_target		(EUIAction *self);
void		e_ui_action_set_state		(EUIAction *self,
						 GVariant *value);
void		e_ui_action_set_state_hint	(EUIAction *self,
						 GVariant *state_hint);
void		e_ui_action_set_visible		(EUIAction *self,
						 gboolean visible);
gboolean	e_ui_action_get_visible		(EUIAction *self);
void		e_ui_action_set_sensitive	(EUIAction *self,
						 gboolean sensitive);
gboolean	e_ui_action_get_sensitive	(EUIAction *self);
gboolean	e_ui_action_is_visible		(EUIAction *self);
void		e_ui_action_set_icon_name	(EUIAction *self,
						 const gchar *icon_name);
const gchar *	e_ui_action_get_icon_name	(EUIAction *self);
void		e_ui_action_set_label		(EUIAction *self,
						 const gchar *label);
const gchar *	e_ui_action_get_label		(EUIAction *self);
void		e_ui_action_set_accel		(EUIAction *self,
						 const gchar *accel);
const gchar *	e_ui_action_get_accel		(EUIAction *self);
void		e_ui_action_add_secondary_accel	(EUIAction *self,
						 const gchar *accel);
GPtrArray *	e_ui_action_get_secondary_accels(EUIAction *self); /* const gchar * */
void		e_ui_action_remove_secondary_accels
						(EUIAction *self);
void		e_ui_action_set_tooltip		(EUIAction *self,
						 const gchar *tooltip);
const gchar *	e_ui_action_get_tooltip		(EUIAction *self);
void		e_ui_action_set_radio_group	(EUIAction *self,
						 GPtrArray *radio_group);
/* const */ GPtrArray *
		e_ui_action_get_radio_group	(EUIAction *self);
void		e_ui_action_set_action_group	(EUIAction *self,
						 struct _EUIActionGroup *action_group);
struct _EUIActionGroup *
		e_ui_action_get_action_group	(EUIAction *self);
gboolean	e_ui_action_get_active		(EUIAction *self);
void		e_ui_action_set_active		(EUIAction *self,
						 gboolean active);
void		e_ui_action_emit_changed	(EUIAction *self);
guint32		e_ui_action_get_usable_for_kinds(EUIAction *self);
void		e_ui_action_set_usable_for_kinds(EUIAction *self,
						 guint32 kinds);

gboolean	e_ui_action_util_gvalue_to_enum_state
						(GBinding *binding,
						 const GValue *from_value,
						 GValue *to_value,
						 gpointer user_data);
gboolean	e_ui_action_util_enum_state_to_gvalue
						(GBinding *binding,
						 const GValue *from_value,
						 GValue *to_value,
						 gpointer user_data);
void		e_ui_action_util_assign_to_widget
						(EUIAction *action,
						 struct _GtkWidget *widget);

G_END_DECLS

#endif /* E_UI_ACTION_H */
