/*
 * e-proxy-preferences.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_PROXY_PREFERENCES_H
#define E_PROXY_PREFERENCES_H

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_PROXY_PREFERENCES \
	(e_proxy_preferences_get_type ())
#define E_PROXY_PREFERENCES(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PROXY_PREFERENCES, EProxyPreferences))
#define E_PROXY_PREFERENCES_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PROXY_PREFERENCES, EProxyPreferencesClass))
#define E_IS_PROXY_PREFERENCES(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PROXY_PREFERENCES))
#define E_IS_PROXY_PREFERENCES_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PROXY_PREFERENCES))
#define E_PROXY_PREFERENCES_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PROXY_PREFERENCES, EProxyPreferencesClass))

G_BEGIN_DECLS

typedef struct _EProxyPreferences EProxyPreferences;
typedef struct _EProxyPreferencesClass EProxyPreferencesClass;
typedef struct _EProxyPreferencesPrivate EProxyPreferencesPrivate;

/**
 * EProxyPreferences:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EProxyPreferences {
	GtkBox parent;
	EProxyPreferencesPrivate *priv;
};

struct _EProxyPreferencesClass {
	GtkBoxClass parent_class;
};

GType		e_proxy_preferences_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_proxy_preferences_new		(ESourceRegistry *registry);
void		e_proxy_preferences_submit	(EProxyPreferences *preferences);
ESourceRegistry *
		e_proxy_preferences_get_registry
						(EProxyPreferences *preferences);
gboolean	e_proxy_preferences_get_show_advanced
						(EProxyPreferences *preferences);
void		e_proxy_preferences_set_show_advanced
						(EProxyPreferences *preferences,
						 gboolean show_advanced);

G_END_DECLS

#endif /* E_PROXY_PREFERENCES_H */

