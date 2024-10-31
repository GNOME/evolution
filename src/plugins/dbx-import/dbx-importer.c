/* dbx-importer.c
 *
 * Author: David Woodhouse <dwmw2@infradead.org>
 *
 * Copyright © 2010 Intel Corporation
 *
 * Evolution parts largely lifted from pst-import.c:
 *   Author: Chris Halls <chris.halls@credativ.co.uk>
 *	    Bharath Acharya <abharath@novell.com>
 *   Copyright © 2006 Chris Halls
 *
 * Some DBX bits from libdbx:
 *   Author: David Smith <Dave.S@Earthcorp.Com>
 *    Copyright © 2001 David Smith
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

#include "evolution-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>

#include <gtk/gtk.h>
#include <libecal/libecal.h>
#include <libebook/libebook.h>

#include <shell/e-shell.h>
#include <shell/e-shell-window.h>
#include <shell/e-shell-view.h>

#include <mail/e-mail-backend.h>
#include <mail/em-folder-selection-button.h>
#include <mail/em-utils.h>

#define d(x)

#ifdef WIN32
#ifdef gmtime_r
#undef gmtime_r
#endif
#define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
#endif

gboolean	org_gnome_evolution_readdbx_supported
						(EPlugin *epl,
						 EImportTarget *target);
GtkWidget *	org_gnome_evolution_readdbx_getwidget
						(EImport *ei,
						 EImportTarget *target,
						 EImportImporter *im);
void		org_gnome_evolution_readdbx_import
						(EImport *ei,
						 EImportTarget *target,
						 EImportImporter *im);
void		org_gnome_evolution_readdbx_cancel
						(EImport *ei,
						 EImportTarget *target,
						 EImportImporter *im);
gint		e_plugin_lib_enable		(EPlugin *ep,
						 gint enable);

/* em-folder-selection-button.h is private, even though other internal
 * evo plugins use it!
 * so declare the functions here
 * TODO: sort out whether this should really be private
*/

typedef struct {
	MailMsg base;

	EImport *import;
	EImportTarget *target;

	GMutex status_lock;
	gchar *status_what;
	gint status_pc;
	gint status_timeout_id;
	GCancellable *cancellable;

	guint32 *indices;
	guint32 index_count;

	gchar *uri;
	gint dbx_fd;

	CamelOperation *cancel;
	CamelFolder *folder;
	gchar *parent_uri;
	gchar *folder_name;
	gchar *folder_uri;
	gint folder_count;
	gint current_item;
} DbxImporter;

static guchar oe56_mbox_sig[16] = {
	0xcf, 0xad, 0x12, 0xfe, 0xc5, 0xfd, 0x74, 0x6f,
	0x66, 0xe3, 0xd1, 0x11, 0x9a, 0x4e, 0x00, 0xc0
};
static guchar oe56_flist_sig[16] = {
	0xcf, 0xad, 0x12, 0xfe, 0xc6, 0xfd, 0x74, 0x6f,
	0x66, 0xe3, 0xd1, 0x11, 0x9a, 0x4e, 0x00, 0xc0
};
static guchar oe4_mbox_sig[8] = {
	0x4a, 0x4d, 0x46, 0x36, 0x03, 0x00, 0x01, 0x00
};

gboolean
org_gnome_evolution_readdbx_supported (EPlugin *epl,
                                       EImportTarget *target)
{
	gchar signature[16];
	gboolean ret = FALSE;
	gint fd, n;
	EImportTargetURI *s;
	gchar *filename;

	if (target->type != E_IMPORT_TARGET_URI) {
		return FALSE;
	}

	s = (EImportTargetURI *) target;

	if (s->uri_src == NULL) {
		return TRUE;
	}

	if (strncmp (s->uri_src, "file:///", strlen ("file:///")) != 0) {
		return FALSE;
	}

	filename = g_filename_from_uri (s->uri_src, NULL, NULL);
	fd = g_open (filename, O_RDONLY, 0);
	g_free (filename);

	if (fd != -1) {
		n = read (fd, signature, sizeof (signature));
		if (n == sizeof (signature)) {
			if (!memcmp (signature, oe56_mbox_sig, sizeof (oe56_mbox_sig))) {
				ret = TRUE;
			} else if (!memcmp (signature, oe56_flist_sig, sizeof (oe56_flist_sig))) {
				d (printf ("Found DBX folder list file\n"));
			} else if (!memcmp (signature, oe4_mbox_sig, sizeof (oe4_mbox_sig))) {
				d (printf ("Found OE4 DBX file\n"));
			}
		}
		close (fd);
	}

	return ret;
}

static void
folder_selected (EMFolderSelectionButton *button,
                 EImportTargetURI *target)
{
	g_free (target->uri_dest);
	target->uri_dest = g_strdup (em_folder_selection_button_get_folder_uri (button));
}

GtkWidget *
org_gnome_evolution_readdbx_getwidget (EImport *ei,
                                       EImportTarget *target,
                                       EImportImporter *im)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailBackend *backend;
	EMailSession *session;
	GtkWidget *hbox, *w;
	GtkLabel *label;
	gchar *select_uri = NULL;

#if 1
	GtkWindow *window;
	/* preselect the folder selected in a mail view */
	window = e_shell_get_active_window (e_shell_get_default ());
	if (E_IS_SHELL_WINDOW (window)) {
		EShellWindow *shell_window;
		const gchar *view;

		shell_window = E_SHELL_WINDOW (window);
		view = e_shell_window_get_active_view (shell_window);

		if (view && g_str_equal (view, "mail")) {
			EShellView *shell_view;
			EMFolderTree *folder_tree = NULL;
			EShellSidebar *shell_sidebar;

			shell_view = e_shell_window_get_shell_view (
				shell_window, view);

			shell_sidebar = e_shell_view_get_shell_sidebar (
				shell_view);

			g_object_get (
				shell_sidebar, "folder-tree",
				&folder_tree, NULL);

			select_uri = em_folder_tree_get_selected_uri (
				folder_tree);
		}
	}
#endif

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);

	if (!select_uri) {
		const gchar *local_inbox_uri;
		local_inbox_uri =
			e_mail_session_get_local_folder_uri (
			session, E_MAIL_LOCAL_FOLDER_INBOX);
		select_uri = g_strdup (local_inbox_uri);
	}

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	w = gtk_label_new_with_mnemonic (_("_Destination folder:"));
	gtk_box_pack_start ((GtkBox *) hbox, w, FALSE, TRUE, 6);

	label = GTK_LABEL (w);

	w = em_folder_selection_button_new (
		session, _("Select folder"),
		_("Select folder to import into"));

	gtk_label_set_mnemonic_widget (label, w);
	em_folder_selection_button_set_folder_uri (
		EM_FOLDER_SELECTION_BUTTON (w), select_uri);
	folder_selected (
		EM_FOLDER_SELECTION_BUTTON (w), (EImportTargetURI *) target);
	g_signal_connect (
		w, "selected",
		G_CALLBACK (folder_selected), target);
	gtk_box_pack_start ((GtkBox *) hbox, w, FALSE, TRUE, 6);

	w = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start ((GtkBox *) w, hbox, FALSE, FALSE, 0);
	gtk_widget_show_all (w);

	g_free (select_uri);

	return w;
}

static gchar *
dbx_import_describe (DbxImporter *m,
                     gint complete)
{
	return g_strdup (_("Importing Outlook Express data"));
}

/* Types taken from libdbx and fixed */
struct _dbx_tableindexstruct {
	guint32 self;
	guint32 unknown1;
	guint32 anotherTablePtr;
	guint32 parent;
	gchar unknown2;
	gchar ptrCount;
	gchar reserve3;
	gchar reserve4;
	guint32 indexCount;
};

struct _dbx_indexstruct {
	guint32 indexptr;
	guint32 anotherTablePtr;
	guint32 indexCount;
};

#define INDEX_POINTER 0xE4
#define ITEM_COUNT 0xC4

struct _dbx_email_headerstruct {
	guint32 self;
	guint32 size;
	gushort u1;
	guchar count;
	guchar u2;
};

struct _dbx_block_hdrstruct {
	guint32 self;
	guint32 nextaddressoffset;
	gushort blocksize;
	guchar intcount;
	guchar unknown1;
	guint32 nextaddress;
};

static gint dbx_pread (gint fd, gpointer buf, guint32 count, guint32 offset)
{
	if (lseek (fd, offset, SEEK_SET) != offset)
		return -1;
	return read (fd, buf, count);
}

static gboolean dbx_load_index_table (DbxImporter *m, guint32 pos, guint32 *index_ofs)
{
	struct _dbx_tableindexstruct tindex;
	struct _dbx_indexstruct index;
	gint i;

	d (printf ("Loading index table at 0x%x\n", pos));

	if (dbx_pread (m->dbx_fd, &tindex, sizeof (tindex), pos) != sizeof (tindex)) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Failed to read table index from DBX file");
		return FALSE;
	}
	tindex.anotherTablePtr = GUINT32_FROM_LE (tindex.anotherTablePtr);
	tindex.self = GUINT32_FROM_LE (tindex.self);
	tindex.indexCount = GUINT32_FROM_LE (tindex.indexCount);

	if (tindex.self != pos) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Corrupt DBX file: Index table at 0x%x does not "
			"point to itself", pos);
		return FALSE;
	}

	d (
		printf ("Index at %x: indexCount %x, anotherTablePtr %x\n",
		pos, tindex.indexCount, tindex.anotherTablePtr));

	if (tindex.indexCount > 0) {
		if (!dbx_load_index_table (m, tindex.anotherTablePtr, index_ofs))
			return FALSE;
	}

	d (printf ("Index at %x has ptrCount %d\n", pos, tindex.ptrCount));

	pos += sizeof (tindex);

	for (i = 0; i < tindex.ptrCount; i++) {
		if (dbx_pread (m->dbx_fd, &index, sizeof (index), pos) != sizeof (index)) {
			g_set_error (
				&m->base.error,
				CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				"Failed to read index entry from DBX file");
			return FALSE;
		}
		index.indexptr = GUINT32_FROM_LE (index.indexptr);
		index.anotherTablePtr = GUINT32_FROM_LE (index.anotherTablePtr);
		index.indexCount = GUINT32_FROM_LE (index.indexCount);

		if (*index_ofs == m->index_count) {
			g_set_error (
				&m->base.error,
				CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				"Corrupt DBX file: Seems to contain more "
				"than %d entries claimed in its header",
				m->index_count);
			return FALSE;
		}
		m->indices[(*index_ofs)++] = index.indexptr;
		if (index.indexCount > 0) {
			if (!dbx_load_index_table (m, index.anotherTablePtr, index_ofs))
				return FALSE;
		}
		pos += sizeof (index);
	}
	return TRUE;
}
static gboolean dbx_load_indices (DbxImporter *m)
{
	guint indexptr, itemcount;
	guint32 index_ofs = 0;

	if (dbx_pread (m->dbx_fd, &indexptr, 4, INDEX_POINTER) != 4) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Failed to read first index pointer from DBX file");
		return FALSE;
	}

	if (dbx_pread (m->dbx_fd, &itemcount, 4, ITEM_COUNT) != 4) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Failed to read item count from DBX file");
		return FALSE;
	}

	indexptr = GUINT32_FROM_LE (indexptr);
	m->index_count = itemcount = GUINT32_FROM_LE (itemcount);
	m->indices = g_malloc (itemcount * 4);

	d (printf ("indexptr %x, itemcount %d\n", indexptr, itemcount));

	if (indexptr && !dbx_load_index_table (m, indexptr, &index_ofs))
		return FALSE;

	d (printf ("Loaded %d of %d indices\n", index_ofs, m->index_count));

	if (index_ofs < m->index_count) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Corrupt DBX file: Seems to contain fewer than %d "
			"entries claimed in its header", m->index_count);
		return FALSE;
	}
	return TRUE;
}

static gboolean
dbx_read_mail_body (DbxImporter *m,
                    guint32 offset,
                    gint bodyfd)
{
	/* FIXME: We really ought to set up CamelStream that we can feed to the
	 * MIME parser, rather than using a temporary file */

	struct _dbx_block_hdrstruct hdr;
	guint32 buflen = 0x200;
	guchar *buffer = g_malloc (buflen);

	if (ftruncate (bodyfd, 0) == -1)
		g_warning ("%s: Failed to truncate file: %s", G_STRFUNC, g_strerror (errno));
	lseek (bodyfd, 0, SEEK_SET);

	while (offset) {
		d (printf ("Reading mail data chunk from %x\n", offset));

		if (dbx_pread (m->dbx_fd, &hdr, sizeof (hdr), offset) != sizeof (hdr)) {
			g_set_error (
				&m->base.error,
				CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				"Failed to read mail data block from "
				"DBX file at offset %x", offset);
			g_free (buffer);
			return FALSE;
		}
		hdr.self = GUINT32_FROM_LE (hdr.self);
		hdr.blocksize = GUINT16_FROM_LE (hdr.blocksize);
		hdr.nextaddress = GUINT32_FROM_LE (hdr.nextaddress);

		if (hdr.self != offset) {
			g_set_error (
				&m->base.error,
				CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				"Corrupt DBX file: Mail data block at "
				"0x%x does not point to itself", offset);
			g_free (buffer);
			return FALSE;
		}

		if (hdr.blocksize > buflen) {
			g_free (buffer);
			buflen = hdr.blocksize;
			buffer = g_malloc (buflen);
		}
		if (dbx_pread (m->dbx_fd, buffer, hdr.blocksize,
			offset + sizeof (hdr)) != hdr.blocksize) {
			g_set_error (
				&m->base.error,
				CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				"Failed to read mail data from DBX file "
				"at offset %lx",
				(long)(offset + sizeof (hdr)));
			g_free (buffer);
			return FALSE;
		}
		if (write (bodyfd, buffer, hdr.blocksize) != hdr.blocksize) {
			g_set_error (
				&m->base.error,
				CAMEL_ERROR, CAMEL_ERROR_GENERIC,
				"Failed to write mail data to temporary file");
			g_free (buffer);
			return FALSE;
		}
		offset = hdr.nextaddress;
	}

	g_free (buffer);

	return TRUE;
}

static gboolean
dbx_read_email (DbxImporter *m,
                guint32 offset,
                gint bodyfd,
                gint *flags)
{
	struct _dbx_email_headerstruct hdr;
	guchar *buffer;
	guint32 dataptr = 0;
	gint i;

	if (dbx_pread (m->dbx_fd, &hdr, sizeof (hdr), offset) != sizeof (hdr)) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Failed to read mail header from DBX file at offset %x",
			offset);
		return FALSE;
	}
	hdr.self = GUINT32_FROM_LE (hdr.self);
	hdr.size = GUINT32_FROM_LE (hdr.size);

	if (hdr.self != offset) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Corrupt DBX file: Mail header at 0x%x does not "
			"point to itself", offset);
		return FALSE;
	}
	buffer = g_malloc (hdr.size);
	offset += sizeof (hdr);
	if (dbx_pread (m->dbx_fd, buffer, hdr.size, offset) != hdr.size) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Failed to read mail data block from DBX file "
			"at offset %x", offset);
		g_free (buffer);
		return FALSE;
	}

	for (i = 0; i < hdr.count; i++) {
		guchar type = buffer[i *4];
		gint val;

		val = buffer[i *4 + 1] +
			(buffer[i *4 + 2] << 8) +
			(buffer[i *4 + 3] << 16);

		switch (type) {
		case 0x01:
			*flags = buffer[hdr.count*4 + val];
			d (printf ("Got type 0x01 flags %02x\n", *flags));
			break;
		case 0x81:
			*flags = val;
			d (printf ("Got type 0x81 flags %02x\n", *flags));
			break;
		case 0x04:
			dataptr = GUINT32_FROM_LE (*(guint32 *)(buffer + hdr.count *4 + val));
			d (printf ("Got type 0x04 data pointer %x\n", dataptr));
			break;
		case 0x84:
			dataptr = val;
			d (printf ("Got type 0x84 data pointer %x\n", dataptr));
			break;
		default:
			/* We don't care about anything else */
			d (printf ("Ignoring type %02x datum\n", type));
			break;
		}
	}
	g_free (buffer);

	if (!dataptr)
		return FALSE;

	return dbx_read_mail_body (m, dataptr, bodyfd);
}

static void
dbx_import_file (DbxImporter *m)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMailSession *session;
	GCancellable *cancellable;
	gchar *filename;
	CamelFolder *folder;
	gint tmpfile;
	gint i;
	gint missing = 0;
	m->status_what = NULL;
	filename = g_filename_from_uri (
		((EImportTargetURI *) m->target)->uri_src, NULL, NULL);

	/* Destination folder, was set in our widget */
	m->parent_uri = g_strdup (((EImportTargetURI *) m->target)->uri_dest);

	cancellable = m->base.cancellable;

	/* XXX Dig up the EMailSession from the default EShell.
	 *     Since the EImport framework doesn't allow for user
	 *     data, I don't see how else to get to it. */
	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (shell, "mail");
	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	camel_operation_push_message (NULL, _("Importing “%s”"), filename);
	folder = e_mail_session_uri_to_folder_sync (
		session, m->parent_uri, CAMEL_STORE_FOLDER_CREATE,
		cancellable, &m->base.error);
	if (!folder)
		return;
	d (printf ("importing to %s\n", camel_folder_get_full_name (folder)));

	camel_folder_freeze (folder);

	filename = g_filename_from_uri (
		((EImportTargetURI *) m->target)->uri_src, NULL, NULL);
	m->dbx_fd = g_open (filename, O_RDONLY, 0);
	g_free (filename);

	if (m->dbx_fd == -1) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Failed to open import file");
		goto out;
	}

	if (!dbx_load_indices (m))
		goto out;

	tmpfile = e_mkstemp ("dbx-import-XXXXXX");
	if (tmpfile == -1) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"Failed to create temporary file for import");
		goto out;
	}

	for (i = 0; i < m->index_count; i++) {
		CamelMessageInfo *info;
		CamelMimeMessage *msg;
		CamelMimeParser *mp;
		gint dbx_flags = 0;
		gint flags = 0;
		gboolean success;

		camel_operation_progress (NULL, 100 * i / m->index_count);
		camel_operation_progress (cancellable, 100 * i / m->index_count);

		if (!dbx_read_email (m, m->indices[i], tmpfile, &dbx_flags)) {
			d (
				printf ("Cannot read email index %d at %x\n",
				i, m->indices[i]));
			if (m->base.error != NULL)
				goto out;
			missing++;
			continue;
		}
		if (dbx_flags & 0x40)
			flags |= CAMEL_MESSAGE_DELETED;
		if (dbx_flags & 0x80)
			flags |= CAMEL_MESSAGE_SEEN;
		if (dbx_flags & 0x80000)
			flags |= CAMEL_MESSAGE_ANSWERED;

		mp = camel_mime_parser_new ();

		lseek (tmpfile, 0, SEEK_SET);
		camel_mime_parser_init_with_fd (mp, tmpfile);

		msg = camel_mime_message_new ();
		if (!camel_mime_part_construct_from_parser_sync (
			(CamelMimePart *) msg, mp, NULL, NULL)) {
			/* set exception? */
			g_object_unref (msg);
			g_object_unref (mp);
			break;
		}

		info = camel_message_info_new (NULL);
		camel_message_info_set_flags (info, flags, ~0);
		success = camel_folder_append_message_sync (
			folder, msg, info, NULL,
			cancellable, &m->base.error);
		g_clear_object (&info);
		g_object_unref (msg);

		if (!success) {
			g_object_unref (mp);
			break;
		}
	}
 out:
	if (m->dbx_fd != -1)
		close (m->dbx_fd);
	g_free (m->indices);
	/* FIXME Not passing GCancellable or GError here. */
	camel_folder_synchronize_sync (folder, FALSE, NULL, NULL);
	camel_folder_thaw (folder);
	g_object_unref (folder);
	if (missing && m->base.error == NULL) {
		g_set_error (
			&m->base.error, CAMEL_ERROR, CAMEL_ERROR_GENERIC,
			"%d messages imported correctly; %d message "
			"bodies were not present in the DBX file",
			m->index_count - missing, missing);
	}
	camel_operation_pop_message (NULL);
}

static void
dbx_import_import (DbxImporter *m,
                   GCancellable *cancellable,
                   GError **error)
{
	dbx_import_file (m);
}

static void
dbx_import_imported (DbxImporter *m)
{
	e_import_complete (m->target->import, (EImportTarget *) m->target, m->base.error);
}

static void
dbx_import_free (DbxImporter *m)
{
	g_free (m->status_what);
	g_mutex_clear (&m->status_lock);

	g_source_remove (m->status_timeout_id);
	m->status_timeout_id = 0;

	g_free (m->folder_name);
	g_free (m->folder_uri);
	g_free (m->parent_uri);

	g_object_unref (m->import);
}

static MailMsgInfo dbx_import_info = {
	sizeof (DbxImporter),
	(MailMsgDescFunc) dbx_import_describe,
	(MailMsgExecFunc) dbx_import_import,
	(MailMsgDoneFunc) dbx_import_imported,
	(MailMsgFreeFunc) dbx_import_free,
};

static gboolean
dbx_status_timeout (gpointer data)
{
	DbxImporter *importer = data;
	gint pc;
	gchar *what;

	if (importer->status_what) {
		g_mutex_lock (&importer->status_lock);
		what = importer->status_what;
		importer->status_what = NULL;
		pc = importer->status_pc;
		g_mutex_unlock (&importer->status_lock);

		e_import_status (
			importer->target->import,
			(EImportTarget *) importer->target, what, pc);
	}

	return TRUE;
}

static void
dbx_status (CamelOperation *op,
            const gchar *what,
            gint pc,
            gpointer data)
{
	DbxImporter *importer = data;

	g_mutex_lock (&importer->status_lock);
	g_free (importer->status_what);
	importer->status_what = g_strdup (what);
	importer->status_pc = pc;
	g_mutex_unlock (&importer->status_lock);
}

/* Start the main import operation */
void
org_gnome_evolution_readdbx_import (EImport *ei,
                                    EImportTarget *target,
                                    EImportImporter *im)
{
	DbxImporter *m;

	m = mail_msg_new (&dbx_import_info);
	g_datalist_set_data (&target->data, "dbx-msg", m);
	m->import = ei;
	g_object_ref (m->import);
	m->target = target;

	m->parent_uri = NULL;
	m->folder_name = NULL;
	m->folder_uri = NULL;

	m->status_timeout_id =
		e_named_timeout_add (100, dbx_status_timeout, m);
	g_mutex_init (&m->status_lock);
	m->cancellable = camel_operation_new ();

	g_signal_connect (
		m->cancellable, "status",
		G_CALLBACK (dbx_status), m);

	mail_msg_unordered_push (m);
}

void
org_gnome_evolution_readdbx_cancel (EImport *ei,
                                    EImportTarget *target,
                                    EImportImporter *im)
{
	DbxImporter *m = g_datalist_get_data (&target->data, "dbx-msg");

	if (m) {
		g_cancellable_cancel (m->cancellable);
	}
}

gint
e_plugin_lib_enable (EPlugin *ep,
                     gint enable)
{
	return 0;
}
