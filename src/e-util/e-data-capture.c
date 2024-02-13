/*
 * e-data-capture.c
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

/**
 * SECTION: e-data-capture
 * @include: e-util/e-util.h
 * @short_description: Capture data from streams
 *
 * #EDataCapture is a #GConverter that captures data until the end of
 * the input data is seen, then emits an #EDataCapture:finished signal
 * with the captured data in a #GBytes instance.
 *
 * When used with #GConverterInputStream or #GConverterOutputStream,
 * an #EDataCapture can discreetly capture the stream content for the
 * purpose of caching.
 **/

#include "e-data-capture.h"

#include <string.h>

typedef struct _SignalClosure SignalClosure;

struct _EDataCapturePrivate {
	GMainContext *main_context;
	GByteArray *byte_array;
	GMutex byte_array_lock;
};

struct _SignalClosure {
	GWeakRef data_capture;
	GBytes *data;
};

enum {
	PROP_0,
	PROP_MAIN_CONTEXT
};

enum {
	FINISHED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_data_capture_converter_init	(GConverterIface *iface);

G_DEFINE_TYPE_WITH_CODE (EDataCapture, e_data_capture, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EDataCapture)
	G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER, e_data_capture_converter_init))

static void
signal_closure_free (SignalClosure *signal_closure)
{
	g_weak_ref_set (&signal_closure->data_capture, NULL);
	g_bytes_unref (signal_closure->data);

	g_slice_free (SignalClosure, signal_closure);
}

static gboolean
data_capture_emit_finished_idle_cb (gpointer user_data)
{
	SignalClosure *signal_closure = user_data;
	EDataCapture *data_capture;

	data_capture = g_weak_ref_get (&signal_closure->data_capture);

	if (data_capture != NULL) {
		g_signal_emit (
			data_capture,
			signals[FINISHED], 0,
			signal_closure->data);
		g_object_unref (data_capture);
	}

	return FALSE;
}

static void
data_capture_set_main_context (EDataCapture *data_capture,
                               GMainContext *main_context)
{
	g_return_if_fail (data_capture->priv->main_context == NULL);

	if (main_context != NULL)
		g_main_context_ref (main_context);
	else
		main_context = g_main_context_ref_thread_default ();

	data_capture->priv->main_context = main_context;
}

static void
data_capture_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MAIN_CONTEXT:
			data_capture_set_main_context (
				E_DATA_CAPTURE (object),
				g_value_get_boxed (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
data_capture_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MAIN_CONTEXT:
			g_value_take_boxed (
				value,
				e_data_capture_ref_main_context (
				E_DATA_CAPTURE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
data_capture_finalize (GObject *object)
{
	EDataCapture *self = E_DATA_CAPTURE (object);

	g_main_context_unref (self->priv->main_context);

	g_byte_array_free (self->priv->byte_array, TRUE);
	g_mutex_clear (&self->priv->byte_array_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_data_capture_parent_class)->finalize (object);
}

static GConverterResult
data_capture_convert (GConverter *converter,
                      gconstpointer inbuf,
                      gsize inbuf_size,
                      gpointer outbuf,
                      gsize outbuf_size,
                      GConverterFlags flags,
                      gsize *bytes_read,
                      gsize *bytes_written,
                      GError **error)
{
	EDataCapture *data_capture;
	GConverterResult result;

	data_capture = E_DATA_CAPTURE (converter);

	/* Output buffer needs to be at least as large as the input buffer.
	 * The error message should never make it to the user interface so
	 * no need to translate. */
	if (outbuf_size < inbuf_size) {
		g_set_error_literal (
			error, G_IO_ERROR,
			G_IO_ERROR_NO_SPACE,
			"EDataCapture needs more space");
		return G_CONVERTER_ERROR;
	}

	memcpy (outbuf, inbuf, inbuf_size);
	*bytes_read = *bytes_written = inbuf_size;

	g_mutex_lock (&data_capture->priv->byte_array_lock);

	g_byte_array_append (
		data_capture->priv->byte_array, inbuf, inbuf_size);

	if ((flags & G_CONVERTER_INPUT_AT_END) != 0) {
		GSource *idle_source;
		GMainContext *main_context;
		SignalClosure *signal_closure;

		signal_closure = g_slice_new0 (SignalClosure);
		g_weak_ref_set (&signal_closure->data_capture, data_capture);
		signal_closure->data = g_bytes_new (
			data_capture->priv->byte_array->data,
			data_capture->priv->byte_array->len);

		main_context = e_data_capture_ref_main_context (data_capture);

		idle_source = g_idle_source_new ();
		g_source_set_callback (
			idle_source,
			data_capture_emit_finished_idle_cb,
			signal_closure,
			(GDestroyNotify) signal_closure_free);
		g_source_set_priority (idle_source, G_PRIORITY_HIGH_IDLE);
		g_source_attach (idle_source, main_context);
		g_source_unref (idle_source);

		g_main_context_unref (main_context);
	}

	g_mutex_unlock (&data_capture->priv->byte_array_lock);

	if ((flags & G_CONVERTER_INPUT_AT_END) != 0)
		result = G_CONVERTER_FINISHED;
	else if ((flags & G_CONVERTER_FLUSH) != 0)
		result = G_CONVERTER_FLUSHED;
	else
		result = G_CONVERTER_CONVERTED;

	return result;
}

static void
data_capture_reset (GConverter *converter)
{
	EDataCapture *data_capture;

	data_capture = E_DATA_CAPTURE (converter);

	g_mutex_lock (&data_capture->priv->byte_array_lock);

	g_byte_array_set_size (data_capture->priv->byte_array, 0);

	g_mutex_unlock (&data_capture->priv->byte_array_lock);
}

static void
e_data_capture_class_init (EDataCaptureClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = data_capture_set_property;
	object_class->get_property = data_capture_get_property;
	object_class->finalize = data_capture_finalize;

	/**
	 * EDataCapture:main-context:
	 *
	 * The #GMainContext from which to emit the #EDataCapture::finished
	 * signal.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_MAIN_CONTEXT,
		g_param_spec_boxed (
			"main-context",
			"Main Context",
			"The main loop context from "
			"which to emit the 'finished' signal",
			G_TYPE_MAIN_CONTEXT,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EDataCapture::finished:
	 * @data_capture: the #EDataCapture that received the signal
	 * @data: the captured data
	 *
	 * The ::finished signal is emitted when there is no more input
	 * data to be captured.
	 **/
	signals[FINISHED] = g_signal_new (
		"finished",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EDataCaptureClass, finished),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		G_TYPE_BYTES);
}

static void
e_data_capture_converter_init (GConverterIface *iface)
{
	iface->convert = data_capture_convert;
	iface->reset = data_capture_reset;
}

static void
e_data_capture_init (EDataCapture *data_capture)
{
	data_capture->priv = e_data_capture_get_instance_private (data_capture);

	data_capture->priv->byte_array = g_byte_array_new ();
	g_mutex_init (&data_capture->priv->byte_array_lock);
}

/**
 * e_data_capture_new:
 * @main_context: a #GMainContext, or %NULL
 *
 * Creates a new #EDataCapture.  If @main_context is %NULL, then the
 * #EDataCapture:finished signal will be emitted from the thread-default
 * #GMainContext for this thread.
 *
 * Returns: an #EDataCapture
 **/
EDataCapture *
e_data_capture_new (GMainContext *main_context)
{
	return g_object_new (
		E_TYPE_DATA_CAPTURE,
		"main-context", main_context, NULL);
}

/**
 * e_data_capture_ref_main_context:
 * @data_capture: an #EDataCapture
 *
 * Returns the #GMainContext from which the #EDataCapture:finished signal
 * is emitted.
 *
 * The returned #GMainContext is referenced for thread-safety and must be
 * unreferenced with g_main_context_unref() when finished with it.
 *
 * Returns: a #GMainContext
 **/
GMainContext *
e_data_capture_ref_main_context (EDataCapture *data_capture)
{
	g_return_val_if_fail (E_IS_DATA_CAPTURE (data_capture), NULL);

	return g_main_context_ref (data_capture->priv->main_context);
}

