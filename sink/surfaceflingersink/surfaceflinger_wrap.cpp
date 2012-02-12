/* GStreamer
 * Copyright (C) <2009> Prajnashi S <prajnashi@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#define ENABLE_GST_PLAYER_LOG
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <cutils/log.h>
#include "surfaceflinger_wrap.h"
#include <gst/gst.h>
#include <asm/memory.h>
#include "mxc_ipu_hl_lib.h"

#include <binder/MemoryHeapPmem.h>

#include "gralloc_priv.h"

#include <system/window.h>

#include <utils/Log.h>
#define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "GstVideoFlingerSink"

using namespace android;

typedef struct
{
  sp < MemoryHeapPmem > frame_heap;
  sp < SurfaceTexture > surfaceTexture;
  sp < ISurfaceTexture > iSurfaceTexture;
  sp < ANativeWindow > aNativeWindow;
  sp < SurfaceControl > surface;
  sp < SurfaceTextureClient > surfaceTextureClient;
  int32_t hor_stride;
  int32_t ver_stride;
  uint32_t width;
  uint32_t height;
  uint32_t crop_top, crop_bot, crop_right, crop_left;
  PixelFormat format;
  int offset;
} VideoFlingerDevice;

static int videoflinger_device_create_new_surface (VideoFlingerDevice *
    videodev);

/* 
 * The only purpose of class "MediaPlayer" is to call Surface::getSurface()
 * in frameworks/base/include/ui/Surface.h, which is private function and accessed
 * by friend class MediaPlayer.
 *
 * We define a fake one to cheat compiler
 */
namespace android
{
  class MediaPlayer
  {
  public:
    static sp <ISurfaceTexture> getSurfaceTexture (Surface* surface)
    {
      return surface->getSurfaceTexture ();
    };
  };
};

VideoFlingerDeviceHandle
videoflinger_device_create (void *iSurfaceTexture)
{
  VideoFlingerDevice *videodev = NULL;

  GST_INFO ("Enter\n");
  videodev = new VideoFlingerDevice;
  if (videodev == NULL) {
    return NULL;
  }

  videodev->format = -1;
  videodev->width = 0;
  videodev->height = 0;
  videodev->hor_stride = 0;
  videodev->ver_stride = 0;

  GST_INFO ("Leave\n");
  return (VideoFlingerDeviceHandle) videodev;
}



int
videoflinger_device_create_new_surface (VideoFlingerDevice * videodev)
{
/* Currently doesn't work for ICS */
#if 0
  status_t state;
  int pid = getpid ();

  GST_INFO ("Enter\n");

  sp <SurfaceComposerClient> mSession = new SurfaceComposerClient;
  LOGI("++++++++++++++++ %i", mSession->initCheck());

  // create the native surface
  videodev->surface = mSession->createSurface(
        0, 640, 360, PIXEL_FORMAT_RGB_565);

  if(videodev->surface == NULL)
      LOGE("++++++++++++++++++++++ control == NULL");

  videodev->width = 640;
  videodev->height = 360;
  videodev->format = PIXEL_FORMAT_RGB_565;

  SurfaceComposerClient::openGlobalTransaction();
  videodev->surface->setLayer(INT_MAX);
  videodev->surface->show();

  videodev->iSurfaceTexture =
      MediaPlayer::getSurfaceTexture (videodev->surface->getSurface().get ());

  SurfaceComposerClient::closeGlobalTransaction();
#endif
#if 0
  sp<Surface> s = control->getSurface();
  videodev->surfaceTexture = new SurfaceTexture(123);
  videodev->surfaceTexture->setBufferCount(15);
  videodev->surfaceTexture->setBufferCountServer(15);
  videodev->surfaceTexture->setSynchronousMode(true);
  videodev->iSurfaceTexture =  videodev->surfaceTexture;/*s->getSurfaceTexture();*/
#endif
#if 0
  videodev->surfaceTextureClient = new SurfaceTextureClient(videodev->iSurfaceTexture);
  videodev->aNativeWindow = videodev->surfaceTextureClient;
  native_window_set_buffers_geometry ( videodev->aNativeWindow.get(), videodev->width, videodev->height, HAL_PIXEL_FORMAT_RGB_565 );
  native_window_set_buffer_count (  videodev->aNativeWindow.get(), 20 );
  native_window_set_usage ( videodev->aNativeWindow.get(), GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
#endif
#if 0
  /* Create a new Surface object with 320x240
   * TODO: Create surface according to device's screen size and rotate it
   * 90, but not a pre-defined value.
   */
  sp < SurfaceComposerClient > videoClient = new SurfaceComposerClient;
  if (videoClient.get () == NULL) {
    LOGE ("Fail to create SurfaceComposerClient\n");
    return -1;
  }

  /* release privious surface */
  videodev->surface.clear ();
  videodev->iSurfaceTexture.clear ();

  int width = (videodev->width) > 1280 ? 1280 : videodev->width;
  int height = (videodev->height) > 720 ? 720 : videodev->height;

  videodev->surface = videoClient->createSurface (pid,
      0,
      width,
      height,
      PIXEL_FORMAT_RGB_565,
      iSurfaceTextureComposer::eFXSurfaceNormal | iSurfaceTextureComposer::ePushBuffers);
  if (videodev->surface.get () == NULL) {
    LOGE ("Fail to create Surface\n");
    return -1;
  }

  videoClient->openTransaction ();

  /* set Surface toppest z-order, this will bypass all iSurfaceTexture created 
   * in java side and make sure this surface displaied in toppest */
  state = videodev->surface->setLayer (INT_MAX);//40000);//INT_MAX);
  if (state != NO_ERROR) {
    GST_INFO ("videoSurface->setLayer(), state = %d", state);
    videodev->surface.clear ();
    return -1;
  }

  /* show surface */
  state = videodev->surface->show ();
  /*state =  videodev->surface->setLayer(INT_MAX);
     if (state != NO_ERROR)
     {
     GST_INFO("videoSurface->show(), state = %d", state);
     videodev->surface.clear();
     return -1;
     } */

  /* get iSurfaceTexture interface */
  videodev->isurface =
      MediaPlayer::getSurface (videodev->surface->getSurface ().get ());

  videoClient->closeTransaction ();
#endif
  /* Smart pointer videoClient shall be deleted automatically
   * when function exists
   */
  GST_INFO ("Leave\n");
  return 0;
}

ipu_lib_handle_t *ipu_handle = NULL;
ipu_lib_input_param_t *ipu_input = NULL;
ipu_lib_output_param_t *ipu_output = NULL;

int
videoflinger_device_release (VideoFlingerDeviceHandle handle)
{
  GST_INFO ("Enter");
  
  if (handle == NULL) {
    return -1;
  }

  //if (ipu_handle)
  //  mxc_ipu_lib_task_uninit (ipu_handle);
  /* unregister frame buffer */
  videoflinger_device_unregister_framebuffers (handle);
  
  /* release iSurfaceTexture & Surface */
  VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
  videodev->aNativeWindow.clear ();
  videodev->iSurfaceTexture.clear ();
  //videodev->surface.clear ();

  /* delete device */
  delete videodev;
 
  GST_INFO ("Leave");
  return 0;
}

static const char* pmem_adsp = "/dev/pmem_adsp";
static const char* pmem = "/dev/pmem";

int
videoflinger_device_register_framebuffers (VideoFlingerDeviceHandle handle,
    int w, int h, int ct, int cb, int cr, int cl, VIDEO_FLINGER_PIXEL_FORMAT format)
{
  VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
  videodev->crop_top = ct;
  videodev->crop_bot = cb;
  videodev->crop_right = cr;
  videodev->crop_left = cl;
  videodev->width = w;
  videodev->height = h;

  return 0;
}

static
void free_hwbuffer(gpointer data)
{
}

/* Attempt to set the buffers directly from SurfaceFlinger's queue/dequeue
 * architecture */
#if 0
GstFlowReturn
videoflinger_alloc (gpointer isurface, VideoFlingerDeviceHandle handle, guint width, guint height, guint size, GstBuffer **gstBuf)
{

    LOGI("%s", __func__);
    GstBufferMeta *gstBufMeta;

    android_native_buffer_t *buf = NULL;
    void *pVaddr = NULL;

    VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
    videodev->iSurfaceTexture = (ISurfaceTexture*) isurface;
    videodev->surfaceTextureClient = new SurfaceTextureClient(videodev->iSurfaceTexture);
    videodev->aNativeWindow = videodev->surfaceTextureClient;
    if (videodev->aNativeWindow.get() == NULL)
    {
        //videoflinger_device_create_new_surface (videodev);
        LOGE("aNativeWindow.get is NULL <------------------");
        return GST_FLOW_UNEXPECTED;
    }

    videodev->width = width;
    videodev->height = height;

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    native_window_set_buffers_geometry ( videodev->aNativeWindow.get(), videodev->width, videodev->height, HAL_PIXEL_FORMAT_YV12 );
    native_window_set_usage( videodev->aNativeWindow.get(), GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN );
    native_window_set_buffer_count (  videodev->aNativeWindow.get(), 16 );

    status_t err = videodev->aNativeWindow->setSwapInterval(videodev->aNativeWindow.get(), 1);
    if(err != 0) {
        LOGE("setSwapInterval failed: %s(%d)", strerror(-err), -err);
        return GST_FLOW_OK;
    }

    err = videodev->aNativeWindow->dequeueBuffer(videodev->aNativeWindow.get(), &buf);
    if((err != 0) || (buf == NULL)) {
        LOGE("dequeueBuffer failed: %s(%d)", strerror(-err), -err);
        return GST_FLOW_OK;
    }
    private_handle_t *phandle = (private_handle_t *)buf->handle;
    mapper.lock(phandle, GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN, Rect(videodev->width, videodev->height), &pVaddr);
    LOGI("phandle: %p %p %x", phandle->base, phandle->phys, phandle->size);
/*
    *gstBuf = gst_buffer_new();
    GST_BUFFER_SIZE(*gstBuf) = phandle->size;
    GST_BUFFER_DATA(*gstBuf) = (guint8*) phandle->base;

    gstBufMeta = gst_buffer_meta_new();
    gstBufMeta->physical_data = (void*) phandle->phys;
    gstBufMeta->priv = handle;
    GST_BUFFER_MALLOCDATA(*gstBuf) = (guint8*) gstBufMeta;
//    GST_BUFFER_OFFSET(*buf) = videodev->offset;
    GST_BUFFER_FREE_FUNC(*gstBuf) = free_hwbuffer;

    gint index = G_N_ELEMENTS((*gstBuf)->_gst_reserved)-1;
    (*gstBuf)->_gst_reserved[index] = gstBufMeta;
*/
    *gstBuf = gst_buffer_new();
    GST_BUFFER_SIZE(*gstBuf) = phandle->size;
    GST_BUFFER_DATA(*gstBuf) = (guint8*) phandle->base;

    gstBufMeta = gst_buffer_meta_new();
    gint index = G_N_ELEMENTS((*gstBuf)->_gst_reserved)-1;
    gstBufMeta->physical_data = (guint8*) phandle->phys;
    (*gstBuf)->_gst_reserved[index] = gstBufMeta;
    gstBufMeta->priv = buf;
    GST_BUFFER_MALLOCDATA(*gstBuf) = (guint8*) gstBufMeta;
    //GST_BUFFER_OFFSET(*gstBuf) = videodev->offset;
    GST_BUFFER_FREE_FUNC(*gstBuf) = free_hwbuffer;

    mapper.unlock(phandle);
    err = videodev->aNativeWindow->queueBuffer(videodev->aNativeWindow.get(), buf);
        if((err != 0) || (buf == NULL)) {
        LOGE("queueBuffer failed: %s(%d)", strerror(-err), -err);
        return GST_FLOW_UNEXPECTED;
    }

    /*mapper.unlock(phandle);
    videodev->aNativeWindow->queueBuffer(videodev->aNativeWindow.get(), buf);
    videodev->surfaceTexture->updateTexImage();*/

    return GST_FLOW_OK;
}
#endif

guint width, height;
gpointer isurface;

GstFlowReturn
videoflinger_alloc (gpointer surface, VideoFlingerDeviceHandle handle, guint width, guint height, guint size, GstBuffer **buf)
{
    GstBufferMeta *bufmeta;
    gint index;
    VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;

    isurface = surface;

    if (!videodev->frame_heap.get())
    {
        int success = 0;

        videodev->offset = 0;

        /* create frame buffer heap base */
        for (int i=16; i>8; i--)
        {
            sp<MemoryHeapBase> master = new MemoryHeapBase (pmem_adsp, size * i);
            if (master->heapID () >= 0){
                success = 1;
                master->setDevice(pmem);
                videodev->frame_heap = new MemoryHeapPmem (master, 0);
                videodev->frame_heap->slap();
                break;
            } else
                LOGI ("Failed to allocate frame_heap. i=%d", i);
        }
        if (!success)
        {
            LOGE ("Error creating frame buffer heap!");
            return GST_FLOW_UNEXPECTED;
        }
    }

    *buf = gst_buffer_new();
    GST_BUFFER_SIZE(*buf) = size;
    GST_BUFFER_DATA(*buf) = (guint8*) videodev->frame_heap->getBase() + videodev->offset;

    bufmeta = gst_buffer_meta_new();
    index = G_N_ELEMENTS((*buf)->_gst_reserved)-1;
    bufmeta->physical_data = (guint8*) videodev->frame_heap->getPhysAddr() + videodev->offset;
    (*buf)->_gst_reserved[index] = bufmeta;
    bufmeta->priv = handle;
    GST_BUFFER_MALLOCDATA(*buf) = (guint8*) bufmeta;
    GST_BUFFER_OFFSET(*buf) = videodev->offset;
    GST_BUFFER_FREE_FUNC(*buf) = free_hwbuffer;

    videodev->offset += size;

    videodev->iSurfaceTexture = (ISurfaceTexture*) isurface;
    videodev->surfaceTextureClient = new SurfaceTextureClient(videodev->iSurfaceTexture);
    videodev->aNativeWindow = videodev->surfaceTextureClient;
    if (videodev->aNativeWindow.get() == NULL)
    {
        return GST_FLOW_UNEXPECTED;
    }

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    native_window_set_usage( videodev->aNativeWindow.get(), GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN );
    native_window_set_buffer_count (  videodev->aNativeWindow.get(), 16 );

    return GST_FLOW_OK;
}


void
videoflinger_device_unregister_framebuffers (VideoFlingerDeviceHandle handle)
{
  GST_INFO ("Enter");

  if (handle == NULL) {
    return;
  }

  GST_INFO ("Leave");
}

#define IS_DMABLE_BUFFER(buffer) ( (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1])) \
                                 || ( GST_IS_BUFFER(buffer) \
                                 &&  GST_BUFFER_FLAG_IS_SET((buffer),GST_BUFFER_FLAG_LAST)))
#define DMABLE_BUFFER_PHY_ADDR(buffer) (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]) ? \
                                        (void*) ((GstBufferMeta *)(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]))->physical_data : \
                                        (void*) GST_BUFFER_OFFSET(buffer))

GstClockTime prev = 0, current = 0;

GstFlowReturn
videoflinger_device_post (VideoFlingerDeviceHandle handle, GstBuffer *gstBuf)
{
    VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
    android_native_buffer_t *buf = NULL;

    status_t err = videodev->aNativeWindow->dequeueBuffer(videodev->aNativeWindow.get(), &buf);
    if((err != 0) || (buf == NULL)) {
        LOGE("dequeueBuffer failed: %s(%d)", strerror(-err), -err);
        return GST_FLOW_OK;
    }
    private_handle_t *phandle = (private_handle_t *)buf->handle;

    //if (!ipu_handle)
    {
        ipu_handle = (ipu_lib_handle_t *) malloc (sizeof(ipu_lib_handle_t));
        memset(ipu_handle, 0, sizeof(ipu_lib_handle_t));

        ipu_input = (ipu_lib_input_param_t *) malloc (sizeof(ipu_lib_input_param_t));
        memset(ipu_input, 0, sizeof(ipu_lib_input_param_t));

        ipu_input->width = videodev->width + videodev->crop_right + videodev->crop_left;// 640;
        ipu_input->height = videodev->height + videodev->crop_bot + videodev->crop_top;
        ipu_input->fmt = GST_MAKE_FOURCC('I','4','2','0');
        ipu_input->input_crop_win.win_w = videodev->width;
        ipu_input->input_crop_win.win_h = videodev->height; 

        ipu_input->user_def_paddr[0] = (int) DMABLE_BUFFER_PHY_ADDR(gstBuf);

        ipu_output = (ipu_lib_output_param_t *) malloc (sizeof(ipu_lib_output_param_t));
        memset(ipu_output, 0, sizeof(ipu_lib_output_param_t));


        /* TODO: This is an alignment hack to say the least? */
        int align = (buf->width % 16);//width;
        if (align)
            ipu_output->width = ((buf->width / 16) + 2)*16;
        else
            ipu_output->width = buf->width;

        ipu_output->height = buf->height;
        ipu_output->fmt = GST_MAKE_FOURCC('R','G','B','P'); ;
        ipu_output->user_def_paddr[0] = (int) phandle->phys;

        mxc_ipu_lib_task_init(ipu_input, NULL, ipu_output, OP_NORMAL_MODE, ipu_handle);

        mxc_ipu_lib_task_buf_update(ipu_handle, (int) DMABLE_BUFFER_PHY_ADDR(gstBuf), 0, 0, NULL, NULL); 

        mxc_ipu_lib_task_uninit (ipu_handle);

        free(ipu_input);
        free(ipu_output);
        free(ipu_handle);
    }

    err = videodev->aNativeWindow->queueBuffer(videodev->aNativeWindow.get(), buf);
        if((err != 0) || (buf == NULL)) {
        LOGE("queueBuffer failed: %s(%d)", strerror(-err), -err);
        return GST_FLOW_UNEXPECTED;
    }

    return GST_FLOW_OK;
}
