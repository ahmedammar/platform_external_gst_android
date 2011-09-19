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
#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <binder/MemoryHeapPmem.h>
#include <cutils/log.h>
#include "surfaceflinger_wrap.h"
#include <gst/gst.h>
#include <asm/memory.h>

#include <utils/Log.h>


#define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "GstVideoFlingerSink"

using namespace android;

#define IS_DMABLE_BUFFER(buffer) ( (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1])) \
                                 || ( GST_IS_BUFFER(buffer) \
                                 &&  GST_BUFFER_FLAG_IS_SET((buffer),GST_BUFFER_FLAG_LAST)))
#define DMABLE_BUFFER_PHY_ADDR(buffer) (GST_IS_BUFFER_META(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]) ? \
                                        ((GstBufferMeta *)(buffer->_gst_reserved[G_N_ELEMENTS(buffer->_gst_reserved)-1]))->physical_data : \
                                        GST_BUFFER_OFFSET(buffer))

typedef struct
{
  sp < MemoryHeapPmem > frame_heap;
  sp < ISurface > isurface;
  sp < SurfaceControl > surface;
  ISurface::BufferHeap buffers;
  int32_t hor_stride;
  int32_t ver_stride;
  uint32_t width;
  uint32_t height;
  uint32_t crop_top, crop_bot, crop_right, crop_left;
  PixelFormat format;
  int buf_index;
  int yuv_size;
  int offset;
} VideoFlingerDevice;

static int videoflinger_device_create_new_surface (VideoFlingerDevice *
    videodev);

/* 
 * The only purpose of class "MediaPlayer" is to call Surface::getISurface()
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
    static sp < ISurface > getSurface (const Surface * surface)
    {
      return surface->getISurface ();
    };
  };
};


VideoFlingerDeviceHandle
videoflinger_device_create (void *isurface)
{
  VideoFlingerDevice *videodev = NULL;

  GST_INFO ("Enter\n");
  videodev = new VideoFlingerDevice;
  if (videodev == NULL) {
    return NULL;
  }
  videodev->frame_heap.clear ();
  videodev->isurface = (ISurface *) isurface;
  videodev->surface.clear ();
  videodev->format = -1;
  videodev->width = 0;
  videodev->height = 0;
  videodev->hor_stride = 0;
  videodev->ver_stride = 0;
  videodev->buf_index = 0;

  GST_INFO ("Leave\n");
  return (VideoFlingerDeviceHandle) videodev;
}



int
videoflinger_device_create_new_surface (VideoFlingerDevice * videodev)
{
  status_t state;
  int pid = getpid ();

  GST_INFO ("Enter\n");

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
  videodev->isurface.clear ();

  int width = (videodev->width) > 640 ? 640 : videodev->width;
  int height = (videodev->height) > 360 ? 360 : videodev->height;

  videodev->surface = videoClient->createSurface (pid,
      0,
      width,
      height,
      PIXEL_FORMAT_RGB_565,
      ISurfaceComposer::eFXSurfaceNormal | ISurfaceComposer::ePushBuffers);
  if (videodev->surface.get () == NULL) {
    LOGE ("Fail to create Surface\n");
    return -1;
  }

  videoClient->openTransaction ();

  /* set Surface toppest z-order, this will bypass all isurface created 
   * in java side and make sure this surface displaied in toppest */
  state = videodev->surface->setLayer (40000);//INT_MAX);
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

  /* get ISurface interface */
  videodev->isurface =
      MediaPlayer::getSurface (videodev->surface->getSurface ().get ());

  videoClient->closeTransaction ();

  /* Smart pointer videoClient shall be deleted automatically
   * when function exists
   */
  GST_INFO ("Leave\n");
  return 0;
}

int
videoflinger_device_release (VideoFlingerDeviceHandle handle)
{
  GST_INFO ("Enter");
  
  if (handle == NULL) {
    return -1;
  }

  /* unregister frame buffer */
  videoflinger_device_unregister_framebuffers (handle);

  /* release ISurface & Surface */
  VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
  videodev->isurface.clear ();
  videodev->surface.clear ();

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
  int surface_format = 0;

  GST_INFO ("Enter");
  if (handle == NULL) {
    LOGE ("videodev is NULL");
    return -1;
  }

#if 0
  /* TODO: Now, only PIXEL_FORMAT_RGB_565 is supported. Change here to support
   * more pixel type
   */
  if (format != PIXEL_FORMAT_RGB_565) {
    LOGE ("Unsupport format: %d", format);
    return -1;
  }
#endif
  surface_format = HAL_PIXEL_FORMAT_YCbCr_422_SP;

  VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
  /* unregister previous buffers */
  if (videodev->frame_heap.get ()) {
    //videoflinger_device_unregister_framebuffers (handle);
  }

  /* reset framebuffers */
  videodev->format = surface_format;
  videodev->width = w + cr + cl;
  videodev->height = h;
  
  //videodev->height *= 3;
  //videodev->height /= 2;

  videodev->crop_top = ct;
  videodev->crop_bot = cb;
  videodev->crop_right = cr;
  videodev->crop_left = cl;

  videodev->hor_stride = w;//w + cr + cl;//videodev->width;
  videodev->ver_stride = h;//h + ct + cb;//videodev->height;

  /* create isurface internally, if no ISurface interface input */
  if (videodev->isurface.get () == NULL) {
    videoflinger_device_create_new_surface (videodev);
  }

  /* create frame buffer heap and register with surfaceflinger */
  videodev->buffers = ISurface::BufferHeap(videodev->width, videodev->height,
      videodev->hor_stride, videodev->ver_stride,
      videodev->format, videodev->frame_heap);

  videodev->buffers.yuv_offsets[0] = (w+cl+cr)*(h+ct+cb);
  videodev->buffers.yuv_offsets[1] = videodev->buffers.yuv_offsets[0]  + (((w+cl+cr)*(h+ct+cb)) >>2);
  videodev->buffers.yuv_size = videodev->yuv_size; 

  if (videodev->isurface->registerBuffers (videodev->buffers) < 0) {
    LOGE ("Cannot register frame buffer!");
    videodev->frame_heap.clear ();
    return -1;
  }

  videodev->buf_index = 0;
  GST_INFO ("Leave");

  return 0;
}

static
void free_hwbuffer(gpointer data)
{
    /*GstBufferMeta * meta;
    if (meta = (GstBufferMeta*)data){
        LOGE ("free_hwbuffer buf->priv:%p", meta->priv);
        mfw_free_hw_buffer(meta->priv);
        gst_buffer_meta_free(meta);
    }*/
}

GstFlowReturn
videoflinger_alloc (VideoFlingerDeviceHandle handle, guint size, GstBuffer **buf)
{
    GstBufferMeta *bufmeta;
    gint index;
    VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;

    if (!videodev->frame_heap.get())
    {
        int success = 0;

        videodev->offset = 0;
        videodev->yuv_size = 0;

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
            }
        }
        if (!success)
        {
            LOGE ("Error creating frame buffer heap!");
            return GST_FLOW_UNEXPECTED;
        }
    }

    *buf = gst_buffer_new();
    GST_BUFFER_SIZE(*buf) = size;
    GST_BUFFER_DATA(*buf) = (guint8*) videodev->frame_heap->getBase();

    bufmeta = gst_buffer_meta_new();
    index = G_N_ELEMENTS((*buf)->_gst_reserved)-1;
    bufmeta->physical_data = (guint8*) videodev->frame_heap->getPhysAddr() + videodev->offset;
    (*buf)->_gst_reserved[index] = bufmeta;
    bufmeta->priv = handle;
    GST_BUFFER_MALLOCDATA(*buf) = (guint8*) bufmeta;
    GST_BUFFER_OFFSET(*buf) = videodev->offset;
    GST_BUFFER_FREE_FUNC(*buf) = free_hwbuffer;

    videodev->yuv_size = size;
    videodev->offset += size;

    return GST_FLOW_OK;
}

void
videoflinger_device_unregister_framebuffers (VideoFlingerDeviceHandle handle)
{
  GST_INFO ("Enter");

  if (handle == NULL) {
    return;
  }

  VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
  if (videodev->frame_heap.get ()) {
    GST_INFO ("Unregister frame buffers.  videodev->isurface = %p",
        videodev->isurface.get ());

    /* release ISurface */
    GST_INFO ("Unregister frame buffer");
    videodev->isurface->unregisterBuffers ();

    /* release MemoryHeapBase */
    GST_INFO ("Clear frame buffers.");
    videodev->frame_heap.clear ();

    videodev->format = -1;
    videodev->width = 0;
    videodev->height = 0;
    videodev->hor_stride = 0;
    videodev->ver_stride = 0;
    videodev->buf_index = 0;
    videodev->offset = 0;
    videodev->yuv_size = 0;
  }

  GST_INFO ("Leave");
}

void
videoflinger_device_post (VideoFlingerDeviceHandle handle, GstBuffer *buf)
{
  GST_INFO ("Enter");

  static int c = 0;

  if (handle == NULL) {
    return;
  }

  VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
  videodev->isurface->postBuffer (GST_BUFFER_OFFSET(buf));

  GST_INFO ("Leave");
}
