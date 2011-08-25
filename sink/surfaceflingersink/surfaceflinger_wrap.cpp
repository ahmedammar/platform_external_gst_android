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
//#define ENABLE_GST_PLAYER_LOG
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
  int frame_offset[3];
  int buf_index;
} VideoFlingerDevice;

/* max frame buffers */
#define  MAX_FRAME_BUFFERS     3

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
  for (int i = 0; i < MAX_FRAME_BUFFERS; i++) {
    videodev->frame_offset[i] = 0;
  }

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
    GST_ERROR ("Fail to create SurfaceComposerClient\n");
    return -1;
  }

  /* release privious surface */
  videodev->surface.clear ();
  videodev->isurface.clear ();

  int width = 1280;//(videodev->width) > 320 ? 320 : videodev->width;
  int height = 720;//(videodev->height) > 320 ? 320 : videodev->height;

  videodev->surface = videoClient->createSurface (pid,
      0,
      width,
      height,
      PIXEL_FORMAT_RGB_565,
      ISurfaceComposer::eFXSurfaceNormal | ISurfaceComposer::ePushBuffers);
  if (videodev->surface.get () == NULL) {
    GST_ERROR ("Fail to create Surface\n");
    return -1;
  }

  videoClient->openTransaction ();

  /* set Surface toppest z-order, this will bypass all isurface created 
   * in java side and make sure this surface displaied in toppest */
  state = videodev->surface->setLayer (50000);//INT_MAX);
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

ipu_lib_handle_t *ipu_handle;

int
videoflinger_device_release (VideoFlingerDeviceHandle handle)
{
  GST_INFO ("Enter");

  if (handle == NULL) {
    return -1;
  }

  if (ipu_handle)
     mxc_ipu_lib_task_uninit (ipu_handle);

  /* unregister frame buffer */
  videoflinger_device_unregister_framebuffers (handle);

  /* release ISurface & Surface */
  //VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
  //videodev->isurface.clear ();
  //videodev->surface.clear ();

  /* delete device */
  //delete videodev;

  GST_INFO ("Leave");
  return 0;
}

void
videoflinger_device_register_ipu_init (VideoFlingerDeviceHandle handle)
{
    VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
    
    ipu_handle = (ipu_lib_handle_t *) malloc (sizeof(ipu_lib_handle_t));
    memset(ipu_handle, 0, sizeof(ipu_lib_handle_t));

    ipu_lib_input_param_t *ipu_input = (ipu_lib_input_param_t *) malloc (sizeof(ipu_lib_input_param_t));
    memset(ipu_input, 0, sizeof(ipu_lib_input_param_t));

    ipu_input->width = videodev->width + videodev->crop_left + videodev->crop_right;
    ipu_input->height = videodev->height + videodev->crop_top + videodev->crop_bot;
    ipu_input->fmt = GST_MAKE_FOURCC('I','4','2','0');
    ipu_input->input_crop_win.win_w = videodev->width;
    ipu_input->input_crop_win.win_h = videodev->height;

    //ipu_input->user_def_paddr[0] = DMABLE_BUFFER_PHY_ADDR(buf);//((GstBufferMeta*)buf->malloc_data)->physical_data;

    ipu_lib_output_param_t *ipu_output = (ipu_lib_output_param_t *) malloc (sizeof(ipu_lib_output_param_t));
    memset(ipu_output, 0, sizeof(ipu_lib_output_param_t));


    ipu_output->width = videodev->width;// - videodev->crop_left - videodev->crop_right;
    ipu_output->height = videodev->height;// - videodev->crop_top - videodev->crop_bot;
    ipu_output->fmt = IPU_PIX_FMT_RGB565;//GST_MAKE_FOURCC('U','Y','V','Y'); ;
    ipu_output->show_to_fb = 0;
    //GST_INFO("++++++++++++++++++ base: %p", videodev->frame_heap->getBase());
    ipu_output->user_def_paddr[0] = videodev->frame_heap->getPhysAddr(); //(int) videodev->frame_heap->getBase();
    
    //ipu_output->fb_disp.fb_num = 2;

    mxc_ipu_lib_task_init(ipu_input, NULL, ipu_output, (TASK_PP_MODE | OP_NORMAL_MODE), ipu_handle);

    LOGI("++++++++++++++++++++ PhysAddr %p", (void*) videodev->frame_heap->getPhysAddr());
    LOGI("++++++++++++++++++++ VirtAddr %p", (void*) videodev->frame_heap->getBase());
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
    GST_ERROR ("videodev is NULL");
    return -1;
  }

  /* TODO: Now, only PIXEL_FORMAT_RGB_565 is supported. Change here to support
   * more pixel type
   */
  if (format != VIDEO_FLINGER_RGB_565) {
    GST_ERROR ("Unsupport format: %d", format);
    return -1;
  }
  surface_format = PIXEL_FORMAT_RGB_565;

  VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;
  /* unregister previous buffers */
  if (videodev->frame_heap.get ()) {
    videoflinger_device_unregister_framebuffers (handle);
  }

  /* reset framebuffers */
  videodev->format = surface_format;
  videodev->width = (w + 1) & -2;
  videodev->height = (h + 1) & -2;
  
  videodev->crop_top = ct;
  videodev->crop_bot = cb;
  videodev->crop_right = cr;
  videodev->crop_left = cl;

  videodev->hor_stride = videodev->width;
  videodev->ver_stride = videodev->height;

  /* create isurface internally, if no ISurface interface input */
  if (videodev->isurface.get () == NULL) {
    videoflinger_device_create_new_surface (videodev);
  }

  /* use double buffer in post */
  int frameSize = videodev->width * videodev->height * 2;
  GST_INFO
      ("format=%d, width=%d, height=%d, hor_stride=%d, ver_stride=%d, frameSize=%d",
      videodev->format, videodev->width, videodev->height, videodev->hor_stride,
      videodev->ver_stride, frameSize);

  /* create frame buffer heap base */
  sp<MemoryHeapBase> master = new MemoryHeapBase (pmem_adsp, frameSize * MAX_FRAME_BUFFERS);
  if (master->heapID () < 0) {
    GST_ERROR ("Error creating frame buffer heap!");
    return -1;
  }
  master->setDevice(pmem);
  videodev->frame_heap = new MemoryHeapPmem (master, 0);
  videodev->frame_heap->slap();

  videoflinger_device_register_ipu_init (handle);

#if 0
for (int i = 0; i <kPreviewBufCnt; i + +) {
mBuffers [i] = (static_cast <MemoryHeapPmem *> (mPreviewHeap.get ()))->mapMemory (i * mPreviewFrameSize_align, mPreviewFrameSize);
mBufferPhyAddr [i] = (void *) (region.offset + i * mPreviewFrameSize_align);
#endif

  //videodev->frame_heap->mapMemory(0, frameSize * MAX_FRAME_BUFFERS);

  /* create frame buffer heap and register with surfaceflinger */
  videodev->buffers = ISurface::BufferHeap(videodev->width,
      videodev->height,
      videodev->hor_stride,
      videodev->ver_stride, videodev->format, videodev->frame_heap);

  if (videodev->isurface->registerBuffers (videodev->buffers) < 0) {
    GST_ERROR ("Cannot register frame buffer!");
    videodev->frame_heap.clear ();
    return -1;
  }

  for (int i = 0; i < MAX_FRAME_BUFFERS; i++) {
    videodev->frame_offset[i] = i * frameSize;
  }
  videodev->buf_index = 0;
  GST_INFO ("Leave");

  return 0;
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

    /* reset offset */
    for (int i = 0; i < MAX_FRAME_BUFFERS; i++) {
      videodev->frame_offset[i] = 0;
    }

    videodev->format = -1;
    videodev->width = 0;
    videodev->height = 0;
    videodev->hor_stride = 0;
    videodev->ver_stride = 0;
    videodev->buf_index = 0;
  }

  GST_INFO ("Leave");
}

void ipu_callback (void * arg, int val)
{
  VideoFlingerDevice *videodev = (VideoFlingerDevice *) arg;
  videodev->isurface->postBuffer (videodev->frame_offset[videodev->buf_index]);
}

void
videoflinger_device_post (VideoFlingerDeviceHandle handle, GstBuffer *buf)
{
  GST_INFO ("Enter");

  if (handle == NULL) {
    return;
  }

  VideoFlingerDevice *videodev = (VideoFlingerDevice *) handle;

  if (++videodev->buf_index == MAX_FRAME_BUFFERS)
    videodev->buf_index = 0;

  /*memcpy (static_cast <
      unsigned char *>(videodev->frame_heap->base ()) +
      videodev->frame_offset[videodev->buf_index], buf, bufsize);

  GST_INFO ("Post buffer[%d].\n", videodev->buf_index);*/
  mxc_ipu_lib_task_buf_update(ipu_handle, /*DMABLE_BUFFER_PHY_ADDR(buf)*/ (int) ((GstBufferMeta*)buf->malloc_data)->physical_data, 0, 0, ipu_callback, (void*)videodev); 

  GST_INFO ("Leave");
}
