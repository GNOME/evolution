/*
 * e-proxy-selector.h
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

#ifndef E_PROXY_SELECTOR_H
#define E_PROXY_SELECTOR_H

#include <libedataserver/libedataserver.h>

#include <e-util/e-tree-view-frame.h>

/* Standard GObject macros */
#define E_TYPE_PROXY_SELECTOR \
	(e_proxy_selector_get_type ())
#define E_PROXY_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PROXY_SELECTOR, EProxySelector))
#define E_PROXY_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PROXY_SELECTOR, EProxySelectorClass))
#define E_IS_PROXY_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PROXY_SELECTOR))
#define E_IS_PROXY_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PROXY_SELECTOR))
#define E_PROXY_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PROXY_SELECTOR, EProxySelectorClass))

G_BEGIN_DECLS

typedef struct _EProxySelector EProxySelector;
typedef struct _EProxySelectorClass EProxySelectorClass;
typedef struct _EProxySelectorPrivate EProxySelectorPrivate;

/**
 * EProxySelector:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EProxySelector {
	ETreeViewFrame parent;
	EProxySelectorPrivate *priv;
};

struct _EProxySelectorClass {
	ETreeViewFrameClass parent_class;
};

GType		e_proxy_selector_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_proxy_selector_new		(ESourceRegistry *registry);
void		e_proxy_selector_refresh	(EProxySelector *selector);
ESourceRegistry *
		e_proxy_selector_get_registry	(EProxySelector *selector);
ESource *	e_proxy_selector_ref_selected	(EProxySelector *selector);
gboolean	e_proxy_selector_set_selected	(EProxySelector *selector,
						 ESource *source);

G_END_DECLS

#endif /* E_PROXY_SELECTOR_H */

