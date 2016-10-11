/*
 * e-settings-weekday-chooser.h
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
 */

#ifndef E_SETTINGS_WEEKDAY_CHOOSER_H
#define E_SETTINGS_WEEKDAY_CHOOSER_H

#include <libebackend/libebackend.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_WEEKDAY_CHOOSER \
	(e_settings_weekday_chooser_get_type ())
#define E_SETTINGS_WEEKDAY_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_WEEKDAY_CHOOSER, ESettingsWeekdayChooser))
#define E_SETTINGS_WEEKDAY_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_WEEKDAY_CHOOSER, ESettingsWeekdayChooserClass))
#define E_IS_SETTINGS_WEEKDAY_CHOOSER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_WEEKDAY_CHOOSER))
#define E_IS_SETTINGS_WEEKDAY_CHOOSER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_WEEKDAY_CHOOSER))
#define E_SETTINGS_WEEKDAY_CHOOSER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_WEEKDAY_CHOOSER, ESettingsWeekdayChooserClass))

G_BEGIN_DECLS

typedef struct _ESettingsWeekdayChooser ESettingsWeekdayChooser;
typedef struct _ESettingsWeekdayChooserClass ESettingsWeekdayChooserClass;
typedef struct _ESettingsWeekdayChooserPrivate ESettingsWeekdayChooserPrivate;

struct _ESettingsWeekdayChooser {
	EExtension parent;
	ESettingsWeekdayChooserPrivate *priv;
};

struct _ESettingsWeekdayChooserClass {
	EExtensionClass parent_class;
};

GType		e_settings_weekday_chooser_get_type
						(void) G_GNUC_CONST;
void		e_settings_weekday_chooser_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_WEEKDAY_CHOOSER_H */

