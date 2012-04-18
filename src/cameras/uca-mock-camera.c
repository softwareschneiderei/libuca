/* Copyright (C) 2011, 2012 Matthias Vogelgesang <matthias.vogelgesang@kit.edu>
   (Karlsruhe Institute of Technology)

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by the
   Free Software Foundation; either version 2.1 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
   details.

   You should have received a copy of the GNU Lesser General Public License along
   with this library; if not, write to the Free Software Foundation, Inc., 51
   Franklin St, Fifth Floor, Boston, MA 02110, USA */

#include <string.h>
#include "uca-mock-camera.h"

#define UCA_MOCK_CAMERA_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), UCA_TYPE_MOCK_CAMERA, UcaMockCameraPrivate))

G_DEFINE_TYPE(UcaMockCamera, uca_mock_camera, UCA_TYPE_CAMERA)

enum {
    PROP_0,
    PROP_SENSOR_WIDTH,
    PROP_SENSOR_HEIGHT,
    PROP_SENSOR_BITDEPTH,
    PROP_SENSOR_HORIZONTAL_BINNING,
    PROP_SENSOR_HORIZONTAL_BINNINGS,
    PROP_SENSOR_VERTICAL_BINNING,
    PROP_SENSOR_VERTICAL_BINNINGS,
    PROP_EXPOSURE_TIME,
    PROP_ROI_X,
    PROP_ROI_Y,
    PROP_ROI_WIDTH,
    PROP_ROI_HEIGHT,
    PROP_SENSOR_MAX_FRAME_RATE,
    PROP_HAS_STREAMING,
    PROP_HAS_CAMRAM_RECORDING,
    N_INTERFACE_PROPERTIES,

    PROP_FRAMERATE,
    N_PROPERTIES
};

static const gchar *mock_overrideables[N_PROPERTIES] = {
    "sensor-width",
    "sensor-height",
    "sensor-bitdepth",
    "sensor-horizontal-binning",
    "sensor-horizontal-binnings",
    "sensor-vertical-binning",
    "sensor-vertical-binnings",
    "exposure-time",
    "roi-x",
    "roi-y",
    "roi-width",
    "roi-height",
    "max-frame-rate",
    "has-streaming",
    "has-camram-recording"
};

static GParamSpec *mock_properties[N_PROPERTIES] = { NULL, };

struct _UcaMockCameraPrivate {
    guint width;
    guint height;
    gfloat frame_rate;
    gfloat max_frame_rate;
    guint8 *dummy_data;
    guint current_frame;

    gboolean thread_running;

    GThread *grab_thread;
    GValueArray *binnings;
};

static const char g_digits[10][20] = {
    /* 0 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 1 */
    { 0x00, 0x00, 0xff, 0x00,
      0x00, 0xff, 0xff, 0x00,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0x00, 0xff, 0x00 },
    /* 2 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0xff, 0x00, 0x00,
      0xff, 0xff, 0xff, 0xff },
    /* 3 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 4 */
    { 0xff, 0x00, 0x00, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0xff,
      0x00, 0x00, 0x00, 0xff },
    /* 5 */
    { 0xff, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0x00, 0xff, 0xff, 0x00,
      0x00, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0x00 },
    /* 6 */
    { 0x00, 0xff, 0xff, 0xff,
      0xff, 0x00, 0x00, 0x00,
      0xff, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 7 */
    { 0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00,
      0x00, 0xff, 0x00, 0x00,
      0xff, 0x00, 0x00, 0x00 },
    /* 8 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0x00 },
    /* 9 */
    { 0x00, 0xff, 0xff, 0x00,
      0xff, 0x00, 0x00, 0xff,
      0x00, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0xff,
      0xff, 0xff, 0xff, 0x00 }
};

const guint DIGIT_WIDTH = 4;
const guint DIGIT_HEIGHT = 5;

static void print_number(gchar *buffer, guint number, guint x, guint y, guint width)
{
    for (int i = 0; i < DIGIT_WIDTH; i++) {
        for (int j = 0; j < DIGIT_HEIGHT; j++) {
            buffer[(y+j)*width + (x+i)] = g_digits[number][j*DIGIT_WIDTH+i];
        }
    }
}

static void print_current_frame(UcaMockCameraPrivate *priv, gchar *buffer)
{
    guint number = priv->current_frame;
    guint divisor = 100000000;
    int x = 10;
    while (divisor > 1) {
        print_number(buffer, number / divisor, x, 10, priv->width);
        number = number % divisor;
        divisor = divisor / 10;
        x += DIGIT_WIDTH + 1;
    }
}

/**
 * uca_mock_camera_new:
 * @error: Location for error
 *
 * Create a new #UcaMockCamera object.
 *
 * Returns: A newly created #UcaMockCamera object
 */
UcaMockCamera *uca_mock_camera_new(GError **error)
{
    UcaMockCamera *camera = g_object_new(UCA_TYPE_MOCK_CAMERA, NULL);
    return camera;
}

static gpointer mock_grab_func(gpointer data)
{
    UcaMockCamera *mock_camera = UCA_MOCK_CAMERA(data);
    g_return_val_if_fail(UCA_IS_MOCK_CAMERA(mock_camera), NULL);

    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(mock_camera);
    UcaCamera *camera = UCA_CAMERA(mock_camera);
    const gulong sleep_time = (gulong) G_USEC_PER_SEC / priv->frame_rate;

    while (priv->thread_running) {
        camera->grab_func(priv->dummy_data, camera->user_data);
        g_usleep(sleep_time);
    }

    return NULL;
}

static void uca_mock_camera_start_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));

    /*
     * In case asynchronous transfer is requested, we start a new thread that
     * invokes the grab callback, otherwise nothing will be done here.
     */

    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(camera);
    gboolean transfer_async = FALSE;
    g_object_get(G_OBJECT(camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    if (transfer_async) {
        GError *tmp_error = NULL;
        priv->thread_running = TRUE;
        priv->grab_thread = g_thread_create(mock_grab_func, camera, TRUE, &tmp_error); 

        if (tmp_error != NULL) {
            priv->thread_running = FALSE;
            g_propagate_error(error, tmp_error);
        }
    }
}

static void uca_mock_camera_stop_recording(UcaCamera *camera, GError **error)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));

    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(camera);
    gboolean transfer_async = FALSE;
    g_object_get(G_OBJECT(camera),
            "transfer-asynchronously", &transfer_async,
            NULL);

    if (transfer_async) {
        priv->thread_running = FALSE;    
        g_thread_join(priv->grab_thread);
    }
}

static void uca_mock_camera_grab(UcaCamera *camera, gpointer *data, GError **error)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(camera));
    g_return_if_fail(data != NULL);

    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(camera);

    if (*data == NULL)
        *data = g_malloc0(priv->width * priv->height);

    g_memmove(*data, priv->dummy_data, priv->width * priv->height);
    print_current_frame(priv, *data);
    priv->current_frame++;
}

static void uca_mock_camera_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail(UCA_IS_MOCK_CAMERA(object));
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_FRAMERATE:
            priv->frame_rate = g_value_get_float(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            return;
    }
}

static void uca_mock_camera_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_SENSOR_WIDTH:
            g_value_set_uint(value, priv->width);
            break;
        case PROP_SENSOR_HEIGHT:
            g_value_set_uint(value, priv->height);
            break;
        case PROP_SENSOR_BITDEPTH:
            g_value_set_uint(value, 8);
            break;
        case PROP_SENSOR_HORIZONTAL_BINNING:
            g_value_set_uint(value, 1);
            break;
        case PROP_SENSOR_HORIZONTAL_BINNINGS:
            g_value_set_boxed(value, priv->binnings);
            break;
        case PROP_SENSOR_VERTICAL_BINNING:
            g_value_set_uint(value, 1);
            break;
        case PROP_SENSOR_VERTICAL_BINNINGS:
            g_value_set_boxed(value, priv->binnings);
            break;
        case PROP_EXPOSURE_TIME:
            g_value_set_double(value, 0.001);
            break;
        case PROP_ROI_X:
            g_value_set_uint(value, 0);
            break;
        case PROP_ROI_Y:
            g_value_set_uint(value, 0);
            break;
        case PROP_ROI_WIDTH:
            g_value_set_uint(value, priv->width);
            break;
        case PROP_ROI_HEIGHT:
            g_value_set_uint(value, priv->height);
            break;
        case PROP_SENSOR_MAX_FRAME_RATE:
            g_value_set_float(value, priv->max_frame_rate);
            break;
        case PROP_HAS_STREAMING:
            g_value_set_boolean(value, TRUE);
            break;
        case PROP_HAS_CAMRAM_RECORDING:
            g_value_set_boolean(value, FALSE);
            break;
        case PROP_FRAMERATE:
            g_value_set_float(value, priv->frame_rate);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void uca_mock_camera_finalize(GObject *object)
{
    UcaMockCameraPrivate *priv = UCA_MOCK_CAMERA_GET_PRIVATE(object);

    if (priv->thread_running) {
        priv->thread_running = FALSE;    
        g_thread_join(priv->grab_thread);
    }

    g_free(priv->dummy_data);
    g_value_array_free(priv->binnings);

    G_OBJECT_CLASS(uca_mock_camera_parent_class)->finalize(object);
}

static void uca_mock_camera_class_init(UcaMockCameraClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = uca_mock_camera_set_property;
    gobject_class->get_property = uca_mock_camera_get_property;
    gobject_class->finalize = uca_mock_camera_finalize;

    UcaCameraClass *camera_class = UCA_CAMERA_CLASS(klass);
    camera_class->start_recording = uca_mock_camera_start_recording;
    camera_class->stop_recording = uca_mock_camera_stop_recording;
    camera_class->grab = uca_mock_camera_grab;

    for (guint id = PROP_0 + 1; id < N_INTERFACE_PROPERTIES; id++)
        g_object_class_override_property(gobject_class, id, mock_overrideables[id-1]);

    mock_properties[PROP_FRAMERATE] = 
        g_param_spec_float("frame-rate",
                "Frame rate",
                "Number of frames per second that are taken",
                1.0f, 100.0f, 100.0f,
                G_PARAM_READWRITE);

    for (guint id = N_INTERFACE_PROPERTIES + 1; id < N_PROPERTIES; id++)
        g_object_class_install_property(gobject_class, id, mock_properties[id]);

    g_type_class_add_private(klass, sizeof(UcaMockCameraPrivate));
}

static void uca_mock_camera_init(UcaMockCamera *self)
{
    self->priv = UCA_MOCK_CAMERA_GET_PRIVATE(self);
    self->priv->width = 640;
    self->priv->height = 480;
    self->priv->frame_rate = self->priv->max_frame_rate = 100000.0f;
    self->priv->dummy_data = (guint8 *) g_malloc0(self->priv->width * self->priv->height);
    self->priv->grab_thread = NULL;
    self->priv->current_frame = 0;

    self->priv->binnings = g_value_array_new(1);
    GValue val = {0};
    g_value_init(&val, G_TYPE_UINT);
    g_value_set_uint(&val, 1);
    g_value_array_append(self->priv->binnings, &val);
}
