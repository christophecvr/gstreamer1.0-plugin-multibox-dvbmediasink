/*
 * GStreamer DVB Media Sink
 * based on code by:
 * Copyright 2006 Felix Domke <tmbinc@elitedvb.net>
 * Copyright 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

#ifndef __GST_DVBVIDEOSINK_H__
#define __GST_DVBVIDEOSINK_H__

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_DVBVIDEOSINK \
  (gst_dvbvideosink_get_type())
#define GST_DVBVIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DVBVIDEOSINK,GstDVBVideoSink))
#define GST_DVBVIDEOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DVBVIDEOSINK,GstDVBVideoSinkClass))
#define GST_DVBVIDEOSINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_DVBVIDEOSINK,GstDVBVideoSinkClass))
#define GST_IS_DVBVIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DVBVIDEOSINK))
#define GST_IS_DVBVIDEOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DVBVIDEOSINK))

typedef struct _GstDVBVideoSink		GstDVBVideoSink;
typedef struct _GstDVBVideoSinkClass	GstDVBVideoSinkClass;
typedef struct _GstDVBVideoSinkPrivate	GstDVBVideoSinkPrivate;

typedef enum {
	CT_UNKNOWN = -1,
	CT_MPEG1 = 0,
	CT_MPEG2 = 1,
	CT_H264 = 2,
	CT_DIVX311 = 3,
	CT_DIVX4 = 4,
	CT_MPEG4_PART2 = 5, 
	CT_VC1 = 6,
	CT_VC1_SM = 7,
	CT_H265 = 8,
	CT_SPARK = 9,
	CT_VP6 = 10,
	CT_VP8 = 11,
	CT_VP9 = 12
} t_codec_type;
#if defined(DREAMBOX)
typedef enum {
	STREAMTYPE_UNKNOWN = -1,
	STREAMTYPE_MPEG2 = 0,
	STREAMTYPE_MPEG4_H264 = 1,
	STREAMTYPE_H263 = 2,
	STREAMTYPE_MPEG4_Part2 = 4,
	STREAMTYPE_MPEG1 = 6,
	STREAMTYPE_MPEG4_H265 = 22,
	STREAMTYPE_XVID = 10,
	STREAMTYPE_DIVX311 = 13,
	STREAMTYPE_DIVX4 = 14,
	STREAMTYPE_DIVX5 = 15,
	STREAMTYPE_VC1 = 16,
	STREAMTYPE_VC1_SM = 17,
	STREAMTYPE_VB6 = 18,
	STREAMTYPE_VB8 = 20,
	STREAMTYPE_VB9 = 23,
	STREAMTYPE_SPARK = 21
} t_stream_type;
#endif
#if defined(TYPE2) && !defined(DREAMBOX)
typedef enum {
	STREAMTYPE_UNKNOWN = -1,
	STREAMTYPE_MPEG2 = 0,
	STREAMTYPE_MPEG4_H264 = 1,
	STREAMTYPE_H263 = 2,
	STREAMTYPE_VC1 = 3,
	STREAMTYPE_MPEG4_Part2 = 4,
	STREAMTYPE_VC1_SM = 5,
	STREAMTYPE_MPEG1 = 6,
	STREAMTYPE_MPEG4_H265 = 7,
	STREAMTYPE_XVID = 10,
	STREAMTYPE_DIVX311 = 13,
	STREAMTYPE_DIVX4 = 14,
	STREAMTYPE_DIVX5 = 15,
	STREAMTYPE_VB6 = 18,
	STREAMTYPE_VB8 = 20,
	STREAMTYPE_VB9 = 23,
	STREAMTYPE_SPARK = 21
} t_stream_type;
#endif
#if !defined(TYPE2) && !defined(DREAMBOX)
typedef enum {
	STREAMTYPE_UNKNOWN = -1,
	STREAMTYPE_MPEG2 = 0,
	STREAMTYPE_MPEG4_H264 = 1,
	STREAMTYPE_H263 = 2,
	STREAMTYPE_VC1 = 3,
	STREAMTYPE_MPEG4_Part2 = 4,
	STREAMTYPE_VC1_SM = 5,
	STREAMTYPE_MPEG1 = 6,
	STREAMTYPE_MPEG4_H265 = 7,
	STREAMTYPE_VB8 = 8,
	STREAMTYPE_VB9 = 9,
	STREAMTYPE_XVID = 10,
	STREAMTYPE_DIVX311 = 13,
	STREAMTYPE_DIVX4 = 14,
	STREAMTYPE_DIVX5 = 15,
	STREAMTYPE_VB6 = 18,
	STREAMTYPE_SPARK = 21
} t_stream_type;
#endif
struct _GstDVBVideoSink
{
	GstBaseSink element;

	int fd;
	int unlockfd[2];

	gint h264_nal_len_size;
	gboolean h264_initial_audelim_written;

	GstBuffer *pesheader_buffer;

	GstBuffer *codec_data;
	t_codec_type codec_type;
	t_stream_type stream_type;
	gboolean use_dts;

	char saved_fallback_framerate[16];

	gdouble rate;
	gboolean playing, paused, flushing, unlocking, flushed, first_paused;
	gboolean using_dts_downmix;
	gboolean pts_written, synchronized, pass_eos;
	gint64 lastpts;
	gint64 timestamp_offset;
	gboolean must_send_header, wmv_asf;
	gint8 ok_to_write;

	gboolean use_set_encoding;

	queue_entry_t *queue;
};

struct _GstDVBVideoSinkClass 
{
  GstBaseSinkClass parent_class;
  gint64 (*get_decoder_time) (GstDVBVideoSink *sink);
  gint64 (*get_video_codec) (GstDVBVideoSink *sink);
};

GType gst_dvbvideosink_get_type (void);

G_END_DECLS

#endif /* __GST_DVBVIDEOSINK_H__ */
