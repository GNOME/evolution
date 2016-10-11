/*
 * e-source-util.h
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

/* These functions combine asynchronous ESource and ESourceRegistry methods
 * with Evolution's EActivity and EAlert facilities to offer an easy-to-use,
 * "fire-and-forget" API for ESource operations.  Use these in situations
 * where it's sufficient to just display an error message if the operation
 * fails, and you don't need to operate on the result. */

#ifndef E_SOURCE_UTIL_H
#define E_SOURCE_UTIL_H

#include <libedataserver/libedataserver.h>

#include <e-util/e-activity.h>
#include <e-util/e-alert-sink.h>

G_BEGIN_DECLS

EActivity *	e_source_util_remove		(ESource *source,
						 EAlertSink *alert_sink);
EActivity *	e_source_util_write		(ESource *source,
						 EAlertSink *alert_sink);
EActivity *	e_source_util_remote_delete	(ESource *source,
						 EAlertSink *alert_sink);

G_END_DECLS

#endif /* E_SOURCE_UTIL_H */
