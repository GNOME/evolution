/*
 * e-autosave-utils.h
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

#ifndef E_AUTOSAVE_UTILS_H
#define E_AUTOSAVE_UTILS_H

#include <shell/e-shell.h>
#include <composer/e-msg-composer.h>

G_BEGIN_DECLS

GList *		e_composer_find_orphans		(GQueue *registry,
						 GError **error);
void		e_composer_load_snapshot	(EShell *shell,
						 GFile *snapshot_file,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
EMsgComposer *	e_composer_load_snapshot_finish	(EShell *shell,
						 GAsyncResult *result,
						 GError **error);
void		e_composer_save_snapshot	(EMsgComposer *composer,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_composer_save_snapshot_finish	(EMsgComposer *composer,
						 GAsyncResult *result,
						 GError **error);
GFile *		e_composer_get_snapshot_file	(EMsgComposer *composer);
void		e_composer_prevent_snapshot_file_delete
						(EMsgComposer *composer);
void		e_composer_allow_snapshot_file_delete
						(EMsgComposer *composer);

G_END_DECLS

#endif /* E_AUTOSAVE_UTILS_H */
