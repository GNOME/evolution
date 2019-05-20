/*
 * e-stock-request.h
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

#ifndef E_STOCK_REQUEST_H
#define E_STOCK_REQUEST_H

#include <e-util/e-content-request.h>

/* Standard GObject macros */
#define E_TYPE_STOCK_REQUEST \
	(e_stock_request_get_type ())
#define E_STOCK_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_STOCK_REQUEST, EStockRequest))
#define E_STOCK_REQUEST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_STOCK_REQUEST, EStockRequestClass))
#define E_IS_STOCK_REQUEST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_STOCK_REQUEST))
#define E_IS_STOCK_REQUEST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_STOCK_REQUEST))
#define E_STOCK_REQUEST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_STOCK_REQUEST, EStockRequestClass))

G_BEGIN_DECLS

typedef struct _EStockRequest EStockRequest;
typedef struct _EStockRequestClass EStockRequestClass;
typedef struct _EStockRequestPrivate EStockRequestPrivate;

struct _EStockRequest {
	GObject parent;
	EStockRequestPrivate *priv;
};

struct _EStockRequestClass {
	GObjectClass parent;
};

GType		e_stock_request_get_type	(void) G_GNUC_CONST;
EContentRequest *
		e_stock_request_new		(void);
gint		e_stock_request_get_scale_factor(EStockRequest *stock_request);
void		e_stock_request_set_scale_factor(EStockRequest *stock_request,
						 gint scale_factor);

G_END_DECLS

#endif /* E_STOCK_REQUEST_H */
