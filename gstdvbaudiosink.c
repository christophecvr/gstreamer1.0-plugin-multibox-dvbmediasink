/*
 * GStreamer DVB Media Sink
 * 
 * Copyright 2011 <slashdev@gmx.net>
 *
 * based on code by:
 * Copyright 2006 Felix Domke <tmbinc@elitedvb.net>
 * Copyright 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files(the "Software"),
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
 * GNU Lesser General Public License Version 2.1(the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-plugin
 *
 * <refsect2>
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch -v -m audiotestsrc ! plugin ! fakesink silent=TRUE
 * </programlisting>
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __sh__
#include <linux/dvb/stm_ioctls.h>
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/base/gstbasesink.h>
#include <gst/audio/gstaudiodecoder.h>

#include "common.h"
#include "gstdvbaudiosink.h"
#include "gstdvbsink-marshal.h"

GST_DEBUG_CATEGORY_STATIC(dvbaudiosink_debug);
#define GST_CAT_DEFAULT dvbaudiosink_debug

enum
{
	PROP_0,
	PROP_SYNC,
	PROP_ASYNC,
	PROP_SYNC_E2PLAYER,
	PROP_ASYNC_E2PLAYER,
	PROP_RENDER_DELAY,
	PROP_LAST
};


enum
{
	SIGNAL_GET_DECODER_TIME,
	LAST_SIGNAL
};

static guint gst_dvbaudiosink_signals[LAST_SIGNAL] = { 0 };


#ifdef HAVE_MP3
#define MPEGCAPS \
		"audio/mpeg, " \
		"mpegversion = (int) 1, " \
		"layer = (int) [ 1, 3 ], " \
		"parsed = (boolean) true; " \
		"audio/mpeg, " \
		"mpegversion = (int) { 2, 4 }, " \
		"profile = (string) lc, " \
		"stream-format = (string) { raw, adts, adif }, " \
		"framed = (boolean) true; " \
		"audio/mpeg, " \
		"mpegversion = (int) { 2, 4 }, " \
		"stream-format = (string) loas, " \
		"framed = (boolean) true; "
#else
#define MPEGCAPS \
		"audio/mpeg, " \
		"mpegversion = (int) 1, " \
		"layer = (int) [ 1, 2 ], " \
		"parsed = (boolean) true; "
#endif

#define AC3CAPS \
		"audio/x-ac3, " \
		"framed =(boolean) true; " \
		"audio/x-private1-ac3, " \
		"framed =(boolean) true; "

#define EAC3CAPS \
		"audio/x-eac3, " \
		"framed =(boolean) true; " \
		"audio/x-private1-eac3, " \
		"framed =(boolean) true; "

#define LPCMCAPS \
		"audio/x-private1-lpcm; "

/* DTSCAPS SUPPORTED IN THIS SINK ARE ONLY THAT FROM
* standard DVD's or blurays or older base DTS 5.1.
* They all have endianness 4321. p.s. the dvd has his
* own x-private1-dts cap and they are normally all suported
* Request to stb manufacturers about the cd audio cap which has
* a depth of 14(two bits are set to zero to save speakers and you're ears)
* but also it has 1024 blok-size instead of 512
* and 4096 frame-size instead of 2012 has been send.
* Strictly speaking it should work but maybe there are some pess_header
* changes needed for this cd dts audio wav media type 
* For now we made the use off cd audio cap not possible.
* So if You want dts_audio_cd support just install gstreamer1.0-plugins-bad-dtsdec.
* Only by stb's who have been build with option --with-dtsdownmix do not and may not !!
* install the plugin from gstreamer.
* One some stb's also a frame-size off 2013 is not supported max is 2012 like mutant51
* For those stb's we limit to 2012 frame-size, higher is trough gst-libav
*/
#ifdef MAX_DTS_FRAMESIZE_2012
#define DTSCAPS \
		"audio/x-dts, " \
		"framed =(boolean) true, " \
		"endianness = (int) 4321, " \
		"frame-size = (int) [ 1, 2012 ]; " \
		"audio/x-private1-dts, " \
		"framed =(boolean) true; "
#else
#define DTSCAPS \
		"audio/x-dts, " \
		"framed =(boolean) true, " \
		"endianness = (int) 4321; " \
		"audio/x-private1-dts, " \
		"framed =(boolean) true; "
#endif

#define WMACAPS \
		"audio/x-wma; " \

#define AMRCAPS \
		"audio/AMR, " \
		"rate = (int) {8000, 16000}, channels = (int) 1; "

#define XRAW "audio/x-raw"
#if defined(DREAMBOX) || defined(MAX_PCMRATE_48K)
#define PCMCAPS \
		"audio/x-raw, " \
		"format = (string) { "GST_AUDIO_NE(S32)", "GST_AUDIO_NE(S24)", "GST_AUDIO_NE(S16)", S8, "GST_AUDIO_NE(U32)", "GST_AUDIO_NE(U24)", "GST_AUDIO_NE(U16)", U8 }, " \
		"layout = (string) { interleaved, non-interleaved }, " \
		"rate = (int) [ 1, 48000 ], " "channels = (int) [ 1, 2 ]; "
#else
#define PCMCAPS \
		"audio/x-raw, " \
		"format = (string) { "GST_AUDIO_NE(S32)", "GST_AUDIO_NE(S24)", "GST_AUDIO_NE(S16)", S8, "GST_AUDIO_NE(U32)", "GST_AUDIO_NE(U24)", "GST_AUDIO_NE(U16)", U8 }, " \
		"layout = (string) { interleaved, non-interleaved }, " \
		"rate = (int) [ 1, MAX ], " "channels = (int) [ 1, 2 ]; "
#endif

static GstStaticPadTemplate sink_factory =
GST_STATIC_PAD_TEMPLATE(
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS(
		MPEGCAPS 
		AC3CAPS
#ifdef HAVE_EAC3
		EAC3CAPS
#endif
#ifdef HAVE_DTS
		DTSCAPS
#endif
#ifdef HAVE_LPCM
		LPCMCAPS
#endif
#ifdef HAVE_WMA
		WMACAPS
#endif
#ifdef HAVE_AMR
		AMRCAPS
#endif
#ifdef HAVE_PCM
		PCMCAPS
#endif
	)
);

#define AUDIO_ENCODING_UNKNOWN  0xFF

t_audio_type bypass_to_encoding (t_audio_type bypass)
{
#ifdef AUDIO_SET_ENCODING
	switch(bypass)
	{
	case AUDIOTYPE_AC3:
	case AUDIOTYPE_AC3_PLUS:
		return AUDIO_ENCODING_AC3;
	case AUDIOTYPE_MPEG:
		return AUDIO_ENCODING_MPEG1;
	case AUDIOTYPE_DTS:
		return AUDIO_ENCODING_DTS;
	case AUDIOTYPE_LPCM:
		return AUDIO_ENCODING_LPCMA;
	case AUDIOTYPE_MP3:
		return AUDIO_ENCODING_MP3;
	case AUDIOTYPE_AAC_PLUS:
		return AUDIO_ENCODING_AAC;
	case AUDIOTYPE_WMA:
	case AUDIOTYPE_WMA_PRO:
		return AUDIO_ENCODING_WMA;
	default:
		return AUDIO_ENCODING_UNKNOWN;
	}
#endif
	return AUDIO_ENCODING_UNKNOWN;
}

static void gst_dvbaudiosink_init(GstDVBAudioSink *self);
static void gst_dvbaudiosink_dispose(GObject *obj);
static void gst_dvbaudiosink_reset(GObject *obj);
static void gst_dvbaudiosink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dvbaudiosink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

#define DEBUG_INIT \
	GST_DEBUG_CATEGORY_INIT(dvbaudiosink_debug, "dvbaudiosink", 0, "dvbaudiosink element");

static GstBaseSinkClass *parent_class = NULL;
G_DEFINE_TYPE_WITH_CODE(GstDVBAudioSink, gst_dvbaudiosink, GST_TYPE_BASE_SINK, DEBUG_INIT);

static gboolean gst_dvbaudiosink_start(GstBaseSink * sink);
static gboolean gst_dvbaudiosink_stop(GstBaseSink * sink);
static gboolean gst_dvbaudiosink_event(GstBaseSink * sink, GstEvent * event);
static GstFlowReturn gst_dvbaudiosink_render(GstBaseSink * sink, GstBuffer * buffer);
static gboolean gst_dvbaudiosink_unlock(GstBaseSink * basesink);
static gboolean gst_dvbaudiosink_unlock_stop(GstBaseSink * basesink);
static gboolean gst_dvbaudiosink_set_caps(GstBaseSink * sink, GstCaps * caps);
static GstCaps *gst_dvbaudiosink_get_caps(GstBaseSink *basesink, GstCaps *filter);
static GstStateChangeReturn gst_dvbaudiosink_change_state(GstElement * element, GstStateChange transition);
static gint64 gst_dvbaudiosink_get_decoder_time(GstDVBAudioSink *self);

/* initialize the plugin's class */
static void gst_dvbaudiosink_class_init(GstDVBAudioSinkClass *self)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(self);
	GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS(self);
	GstElementClass *element_class = GST_ELEMENT_CLASS(self);

	parent_class = g_type_class_peek_parent(self);

	gobject_class->finalize = gst_dvbaudiosink_reset;
	gobject_class->dispose = gst_dvbaudiosink_dispose;

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
	gst_element_class_set_static_metadata(element_class,
		"DVB audio sink",
		"Generic/DVBAudioSink",
		"Outputs PES into a linuxtv dvb audio device",
		"PLi team");

	gobject_class->set_property = gst_dvbaudiosink_set_property;
	gobject_class->get_property = gst_dvbaudiosink_get_property;

	g_object_class_install_property (gobject_class, PROP_SYNC,
			g_param_spec_boolean ("sync", "Sync", "Sync on the clock", FALSE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_ASYNC,
			g_param_spec_boolean ("async", "Async", "preroll", FALSE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_SYNC_E2PLAYER,
			g_param_spec_boolean ("e2-sync", "E2-Sync", "Sync on the clock", FALSE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_ASYNC_E2PLAYER,
			g_param_spec_boolean ("e2-async", "E2-Async", "preroll", FALSE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class, PROP_RENDER_DELAY,
			g_param_spec_uint64 ("render-delay", "Renderdelay", "Render-delay increase latency",
				0, G_MAXUINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_start);
	gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_stop);
	gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_render);
	gstbasesink_class->event = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_event);
	gstbasesink_class->unlock = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_unlock);
	gstbasesink_class->unlock_stop = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_unlock_stop);
	gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_set_caps);
	gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_get_caps);

	element_class->change_state = GST_DEBUG_FUNCPTR(gst_dvbaudiosink_change_state);

	gst_dvbaudiosink_signals[SIGNAL_GET_DECODER_TIME] =
		g_signal_new("get-decoder-time",
		G_TYPE_FROM_CLASS(self),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET(GstDVBAudioSinkClass, get_decoder_time),
		NULL, NULL, gst_dvbsink_marshal_INT64__VOID, G_TYPE_INT64, 0);

	self->get_decoder_time = gst_dvbaudiosink_get_decoder_time;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void gst_dvbaudiosink_init(GstDVBAudioSink *self)
{
	self->codec_data = NULL;
	self->bypass = AUDIOTYPE_UNKNOWN;
	self->fixed_buffersize = 0;
	self->fixed_bufferduration = GST_CLOCK_TIME_NONE;
	self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;
	self->aac_adts_header_valid = self->pass_eos = FALSE;
	self->pesheader_buffer = NULL;
	self->cache = NULL;
	self->audio_stream_type = NULL;
	self->playing = self->flushing = self->unlocking = self->paused = self->first_paused = FALSE;
	self->pts_written = self->using_dts_downmix = self->synchronized = self->dts_cd = FALSE;
	self->lastpts = 0;
	self->timestamp_offset = 0;
	self->queue = NULL;
	self->fd = -1;
	self->unlockfd[0] = self->unlockfd[1] = -1;
	self->rate = 1.0;
	self->timestamp = GST_CLOCK_TIME_NONE;
#ifdef AUDIO_SET_ENCODING
	self->use_set_encoding = TRUE;
#else
	self->use_set_encoding = FALSE;
#endif
	// this machine selection is there for now it is just for me now
	// during test and dev fase.
	// The goal is to do machine depended difs from out of e2 players in future.
	gst_base_sink_set_sync(GST_BASE_SINK(self), FALSE);
	gst_base_sink_set_async_enabled(GST_BASE_SINK(self), FALSE);

}

static void gst_dvbaudiosink_dispose(GObject *obj)
{
	G_OBJECT_CLASS(parent_class)->dispose(obj);
	GST_DEBUG("GstDVBAudioSink DISPOSED");
}

static void gst_dvbaudiosink_reset(GObject *obj)
{
	G_OBJECT_CLASS(parent_class)->finalize(obj);
	GST_DEBUG("GstDVBAudioSink RESET");
}

static void gst_dvbaudiosink_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK (object);

	switch (prop_id)
	{
		/* sink should only work with sync turned off, ignore all attempts to change it *
		 * looks like no element tries to change this setting so leave it allowed for now *
		 * subject to changes in future Most stb do support sync settings *
		 * exception on this are the old dreamboxes and vuplus boxes and maybe some other ol ones */
		case PROP_SYNC:
			GST_INFO_OBJECT(self, "ignoring attempt to change 'sync' to %s by unknown element", g_value_get_boolean(value) ? "TRUE" : "FALSE");
			break;
		case PROP_ASYNC:
			GST_INFO_OBJECT(self, "ignoring attempt to change by 'async' to %s by unknown element", g_value_get_boolean(value) ? "TRUE" : "FALSE");
			break;
		case PROP_SYNC_E2PLAYER:
			gst_base_sink_set_sync(GST_BASE_SINK(object), g_value_get_boolean(value));
			GST_DEBUG_OBJECT(self, "CHANGE sync setting to %s", g_value_get_boolean(value) ? "TRUE" : "FALSE");
			if (gst_base_sink_get_sync(GST_BASE_SINK(object)))
			{
				GST_INFO_OBJECT(self, "Gstreamer sync succesfully set to TRUE by e2Player");
				// the driver should(if the driver support that setting) only synchronize if gstreamer runs sync false mode
				if (self->fd >= 0)
				{
					if(ioctl(self->fd, AUDIO_SET_AV_SYNC, FALSE) >= 0)
						GST_INFO_OBJECT(self," AUDIO_SET_AV_SYNC FALSE accepted by driver");
				}
				self->synchronized = TRUE;
			}
			else
			{
				// when gstreamer runs in sync false mode we try to let the driver synchronize the media
				GST_INFO_OBJECT(self, "Gstreamer sync succesfully set to FALSE by e2Player");
				if (self->fd >= 0)
				{
					if(ioctl(self->fd, AUDIO_SET_AV_SYNC, TRUE) >= 0)
						GST_INFO_OBJECT(self," AUDIO_SET_AV_SYNC TRUE accepted by driver");
				}
				self->synchronized = FALSE;
			}
			break;
		case PROP_ASYNC_E2PLAYER:
			gst_base_sink_set_async_enabled(GST_BASE_SINK(object), g_value_get_boolean(value));
			GST_DEBUG_OBJECT(self, "CHANGE async setting to %s  source ", g_value_get_boolean(value) ? "TRUE" : "FALSE");
			if (gst_base_sink_is_async_enabled(GST_BASE_SINK(object)))
			{
				GST_INFO_OBJECT(self, "Gstreamer async succesfully set to TRUE by e2Player");
			}
			else
			{
				GST_INFO_OBJECT(self, "Gstreamer async succesfully set to FALSE by e2Player");
			}
			break;
		case PROP_RENDER_DELAY:
			gst_base_sink_set_render_delay(GST_BASE_SINK(object), g_value_get_uint64(value));
			GST_INFO_OBJECT(self, "Change renderdelay to  = %" G_GUINT64_FORMAT , g_value_get_uint64(value));
			if (gst_base_sink_get_render_delay(GST_BASE_SINK(object)) == g_value_get_uint64(value))
				GST_INFO_OBJECT(self, "Renderdelay changed to  %" G_GUINT64_FORMAT , g_value_get_uint64(value));
			else
				GST_WARNING_OBJECT(self, "Renderdelay change to  %" G_GUINT64_FORMAT " FAILURE", g_value_get_uint64(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void gst_dvbaudiosink_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK (object);

	switch (prop_id)
	{
		case PROP_SYNC:
			g_value_set_boolean(value, gst_base_sink_get_sync(GST_BASE_SINK(object)));
			GST_INFO_OBJECT(self, "Requested by other element SYNC VALUE = %s", g_value_get_boolean(value) ? "TRUE" : "FALSE");
			break;
		case PROP_ASYNC:
			g_value_set_boolean(value, gst_base_sink_is_async_enabled(GST_BASE_SINK(object)));
			GST_INFO_OBJECT(self, "Requested by other element ASYNC VALUE = %s", g_value_get_boolean(value) ? "TRUE" : "FALSE");
			break;
		case PROP_RENDER_DELAY:
			g_value_set_uint64(value, gst_base_sink_get_render_delay(GST_BASE_SINK(object)));
			GST_INFO_OBJECT(self, "Requested by other element RENDER DELAY = %" G_GUINT64_FORMAT , g_value_get_uint64(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static gint64 gst_dvbaudiosink_get_decoder_time(GstDVBAudioSink *self)
{
	gint64 cur = 0;
	gint res = -1;
	if (self->fd < 0 || !self->playing || !self->pts_written)
		return GST_CLOCK_TIME_NONE;

	res = ioctl(self->fd, AUDIO_GET_PTS, &cur);
	if (cur && res >= 0)
	{
		self->lastpts = cur;
	}
	else
	{
		cur = self->lastpts;
	}
	cur *= 11111;
	// timestamp_offset is a gstreamer nanoseconds var
	cur -= self->timestamp_offset;

	return cur;
}

static gboolean gst_dvbaudiosink_unlock(GstBaseSink *basesink)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);
	self->unlocking = TRUE;
	/* wakeup the poll */
	write(self->unlockfd[1], "\x01", 1);
	GST_DEBUG_OBJECT(basesink, "unlock");
	return TRUE;
}

static gboolean gst_dvbaudiosink_unlock_stop(GstBaseSink *basesink)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);
	self->unlocking = FALSE;
	GST_DEBUG_OBJECT(basesink, "unlock_stop");
	return TRUE;
}

static GstCaps *gst_dvbaudiosink_get_caps(GstBaseSink *basesink, GstCaps *filter)
{
	GstCaps *caps = gst_caps_from_string(
		MPEGCAPS 
		AC3CAPS
#ifdef HAVE_EAC3
		EAC3CAPS
#endif
#ifdef HAVE_LPCM
		LPCMCAPS
#endif
#ifdef HAVE_WMA
		WMACAPS
#endif
#ifdef HAVE_AMR
		AMRCAPS
#endif
#ifdef HAVE_PCM
		PCMCAPS
#endif
	);

#if defined(HAVE_DTS) && ((!defined(DREAMBOX) && !defined(VUPLUS) && !defined(OSMIO4K)) || (defined(VUPLUS) && !defined(HAVE_DTSDOWNMIX)))
	/* for the time the static cap has been limited to not be used in case of dts_audio_cd media */
	gst_caps_append(caps, gst_caps_from_string(DTSCAPS));
#endif
#if defined(HAVE_DTSDOWNMIX) && (defined(DREAMBOX) || defined(VUPLUS) || defined(OSMIO4K))
	if (!get_ac3_downmix_setting())
	{
		gst_caps_append(caps, gst_caps_from_string(DTSCAPS));
	}
#endif

	if (filter)
	{
		GstCaps *intersection = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
		gst_caps_unref(caps);
		caps = intersection;
	}
	return caps;
}

static gboolean gst_dvbaudiosink_set_caps(GstBaseSink *basesink, GstCaps *caps)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);
	GstStructure *structure = gst_caps_get_structure(caps, 0);
	const char *type = gst_structure_get_name(structure);
	t_audio_type previous_bypass = self->bypass;
	self->bypass = AUDIOTYPE_UNKNOWN;

	self->skip = 0;
	self->aac_adts_header_valid = FALSE;
	self->fixed_buffersize = 0;
	self->fixed_bufferduration = GST_CLOCK_TIME_NONE;
	self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;

	GST_INFO_OBJECT (self, "caps = %" GST_PTR_FORMAT, caps);

	if (self->codec_data)
	{
		gst_buffer_unref(self->codec_data);
		self->codec_data = NULL;
	}

	if (!strcmp(type, "audio/mpeg"))
	{
		gint mpegversion;
		gst_structure_get_int(structure, "mpegversion", &mpegversion);
		switch (mpegversion)
		{
			case 1:
			{
				gint layer;
				gst_structure_get_int(structure, "layer", &layer);
				if (layer == 3)
				{
					self->bypass = AUDIOTYPE_MP3;
				}
				else
				{
					self->bypass = AUDIOTYPE_MPEG;
				}
				GST_INFO_OBJECT(self, "MIMETYPE %s version %d layer %d", type, mpegversion, layer);
				break;
			}
			case 2:
			case 4:
			{
				/* hack on sometimes wrong caps reparse by hls stream */
				if(!self->audio_stream_type)
					self->audio_stream_type = gst_structure_get_string(structure, "stream-type");
				if (!self->audio_stream_type)
				{
					self->audio_stream_type = gst_structure_get_string(structure, "stream-format");
				}
				if (self->audio_stream_type && !strcmp(self->audio_stream_type, "adts"))
				{
					GST_INFO_OBJECT(self, "MIMETYPE %s version %d(AAC-ADTS)", type, mpegversion);
				}
				else if (self->audio_stream_type && !strcmp(self->audio_stream_type, "loas"))
				{
					self->bypass = AUDIOTYPE_AAC_HE;
					break;
				}
				else
				{
					const GValue *codec_data = gst_structure_get_value(structure, "codec_data");
					GST_INFO_OBJECT(self, "MIMETYPE %s version %d(AAC-RAW)", type, mpegversion);
					if (codec_data)
					{
						guint8 h[2];
						gst_buffer_extract(gst_value_get_buffer(codec_data), 0, h, sizeof(h));
						guint8 obj_type =h[0] >> 3;
						guint8 rate_idx =((h[0] & 0x7) << 1) |((h[1] & 0x80) >> 7);
						guint8 channels =(h[1] & 0x78) >> 3;
						GST_INFO_OBJECT(self, "have codec data -> obj_type = %d, rate_idx = %d, channels = %d\n",
							obj_type, rate_idx, channels);
						/* Sync point over a full byte */
						self->aac_adts_header[0] = 0xFF;
						/* Sync point continued over first 4 bits + static 4 bits
						 *(ID, layer, protection)*/
						self->aac_adts_header[1] = 0xF1;
						if (mpegversion == 2)
							self->aac_adts_header[1] |= 8;
						/* Object type over first 2 bits */
						self->aac_adts_header[2] = (obj_type - 1) << 6;
						/* rate index over next 4 bits */
						self->aac_adts_header[2] |= rate_idx << 2;
						/* channels over last 2 bits */
						self->aac_adts_header[2] |= (channels & 0x4) >> 2;
						/* channels continued over next 2 bits + 4 bits at zero */
						self->aac_adts_header[3] = (channels & 0x3) << 6;
						self->aac_adts_header_valid = TRUE;
					}
					else
					{
						gint rate, channels, rate_idx = 0, obj_type = 1; // hardcoded yet.. hopefully this works every time ;)
						GST_INFO_OBJECT(self, "no codec data");
						if (gst_structure_get_int(structure, "rate", &rate) && gst_structure_get_int(structure, "channels", &channels))
						{
							guint samplingrates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0 };
							do
							{
								if (samplingrates[rate_idx] == rate) break;
								++rate_idx;
							} while (samplingrates[rate_idx]);
							if (samplingrates[rate_idx])
							{
								GST_INFO_OBJECT(self, "mpegversion %d, channels %d, rate %d, rate_idx %d\n", mpegversion, channels, rate, rate_idx);
								/* Sync point over a full byte */
								self->aac_adts_header[0] = 0xFF;
								/* Sync point continued over first 4 bits + static 4 bits
								 *(ID, layer, protection)*/
								self->aac_adts_header[1] = 0xF1;
								if (mpegversion == 2) self->aac_adts_header[1] |= 8;
								/* Object type over first 2 bits */
								self->aac_adts_header[2] = obj_type << 6;
								/* rate index over next 4 bits */
								self->aac_adts_header[2] |= rate_idx << 2;
								/* channels over last 2 bits */
								self->aac_adts_header[2] |=(channels & 0x4) >> 2;
								/* channels continued over next 2 bits + 4 bits at zero */
								self->aac_adts_header[3] =(channels & 0x3) << 6;
								self->aac_adts_header_valid = TRUE;
							}
						}
					}
				}
				self->bypass = AUDIOTYPE_AAC_PLUS; // always use AAC+ ADTS yet..
				break;
			}
			default:
				GST_ELEMENT_ERROR(self, STREAM, FORMAT,(NULL),("unhandled mpeg version %i", mpegversion));
				break;
		}
	}
	else if (!strcmp(type, "audio/x-ac3"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
		self->bypass = AUDIOTYPE_AC3;
	}
	else if (!strcmp(type, "audio/x-eac3"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
		self->bypass = AUDIOTYPE_AC3_PLUS;
	}
	else if (!strcmp(type, "audio/x-private1-dts"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s(DVD Audio - 2 byte skipping)",type);
		self->bypass = AUDIOTYPE_DTS;
		self->skip = 2;
	}
	else if (!strcmp(type, "audio/x-private1-ac3"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s(DVD Audio - 2 byte skipping)",type);
		self->bypass = AUDIOTYPE_AC3;
		self->skip = 2;
	}
	else if (!strcmp(type, "audio/x-private1-eac3"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s(DVD Audio - 2 byte skipping)",type);
		self->bypass = AUDIOTYPE_AC3_PLUS;
		self->skip = 2;
	}
	else if (!strcmp(type, "audio/x-private1-lpcm"))
	{
		GST_INFO_OBJECT(self, "MIMETYPE %s(DVD Audio)",type);
		self->bypass = AUDIOTYPE_LPCM;
	}
	else if (!strcmp(type, "audio/x-dts"))
	{
		/* waiting on manufacturers answer about this type of dts but it is already prepared to be used */
		gint endianness = 0; 
		gint framesize = 0;
		gboolean str_framesize = gst_structure_get_int(structure, "frame-size", &framesize);
		gboolean str_endianness = gst_structure_get_int(structure, "endianness", &endianness);
		if(str_endianness && endianness == 1234)
		{
			GST_INFO_OBJECT (self, "MEDIA IS DTS_AUDIO_CD");
			self->dts_cd = TRUE;
			self->bypass = AUDIOTYPE_DTS;
		}
		else if(str_framesize && framesize == 2013)
		{
			GST_INFO_OBJECT (self, "MEDIA IS dtshd96");
			self->bypass = AUDIOTYPE_DTS;
		}
		else
			self->bypass = AUDIOTYPE_DTS;

		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
	}
	else if (!strcmp(type, "audio/x-wma"))
	{
		const GValue *codec_data = gst_structure_get_value(structure, "codec_data");
		gint wmaversion, bitrate, depth, rate, channels, block_align;
		gst_structure_get_int(structure, "wmaversion", &wmaversion);
		gst_structure_get_int(structure, "bitrate", &bitrate);
		gst_structure_get_int(structure, "depth", &depth);
		gst_structure_get_int(structure, "rate", &rate);
		gst_structure_get_int(structure, "channels", &channels);
		gst_structure_get_int(structure, "block_align", &block_align);
		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
		self->bypass = (wmaversion > 2) ? AUDIOTYPE_WMA_PRO : AUDIOTYPE_WMA;
		if (codec_data)
		{
			guint8 *data;
			guint8 *codec_data_pointer;
			gint codec_data_size;
			gint codecid = 0x160 + wmaversion - 1;
			GstMapInfo map, codecdatamap;
			gst_buffer_map(gst_value_get_buffer(codec_data), &codecdatamap, GST_MAP_READ);
			codec_data_pointer = codecdatamap.data;
			codec_data_size = codecdatamap.size;
			self->codec_data = gst_buffer_new_and_alloc(18 + codec_data_size);
			gst_buffer_map(self->codec_data, &map, GST_MAP_WRITE);
			data = map.data;
			/* codec tag */
			*(data++) = codecid & 0xff;
			*(data++) = (codecid >> 8) & 0xff;
			/* channels */
			*(data++) = channels & 0xff;
			*(data++) = (channels >> 8) & 0xff;
			/* sample rate */
			*(data++) = rate & 0xff;
			*(data++) = (rate >> 8) & 0xff;
			*(data++) = (rate >> 16) & 0xff;
			*(data++) = (rate >> 24) & 0xff;
			/* byte rate */
			bitrate /= 8;
			*(data++) = bitrate & 0xff;
			*(data++) = (bitrate >> 8) & 0xff;
			*(data++) = (bitrate >> 16) & 0xff;
			*(data++) = (bitrate >> 24) & 0xff;
			/* block align */
			*(data++) = block_align & 0xff;
			*(data++) = (block_align >> 8) & 0xff;
			/* word size */
			*(data++) = depth & 0xff;
			*(data++) = (depth >> 8) & 0xff;
			/* codec data size */
			*(data++) = codec_data_size & 0xff;
			*(data++) = (codec_data_size >> 8) & 0xff;
			memcpy(data, codec_data_pointer, codec_data_size);
			gst_buffer_unmap(self->codec_data, &map);
			gst_buffer_unmap(gst_value_get_buffer(codec_data), &codecdatamap);
		}
	}
	else if (!strcmp(type, "audio/AMR"))
	{
		const GValue *codec_data = gst_structure_get_value(structure, "codec_data");
		if (codec_data)
		{
			self->codec_data = gst_buffer_copy(gst_value_get_buffer(codec_data));
		}
		GST_INFO_OBJECT(self, "MIMETYPE %s",type);
		self->bypass = AUDIOTYPE_AMR;
	}
	else if (!strcmp(type, XRAW))
	{
		guint8 *data;
		gint size;
		gint format = 0x01;
		const gchar *formatstring = NULL;
		gint width = 0, depth = 0, rate = 0, channels, block_align, byterate;
		self->codec_data = gst_buffer_new_and_alloc(18);
		GstMapInfo map;
		gst_buffer_map(self->codec_data, &map, GST_MAP_WRITE);
		data = map.data;
		size = map.size;
		formatstring = gst_structure_get_string(structure, "format");
		if (formatstring)
		{
			if (!strncmp(&formatstring[1], "32", 2))
			{
				width = depth = 32;
			}
			else if (!strncmp(&formatstring[1], "24", 2))
			{
				width = depth = 24;
			}
			else if (!strncmp(&formatstring[1], "16", 2))
			{
				width = depth = 16;
			}
			else if (!strncmp(&formatstring[1], "8", 1))
			{
				width = depth = 8;
			}
		}
		gst_structure_get_int(structure, "rate", &rate);
		gst_structure_get_int(structure, "channels", &channels);
		byterate = channels * rate * width / 8;
		block_align = channels * width / 8;
		memset(data, 0, size);
		/* format tag */
		*(data++) = format & 0xff;
		*(data++) = (format >> 8) & 0xff;
		/* channels */
		*(data++) = channels & 0xff;
		*(data++) = (channels >> 8) & 0xff;
		/* sample rate */
		*(data++) = rate & 0xff;
		*(data++) = (rate >> 8) & 0xff;
		*(data++) = (rate >> 16) & 0xff;
		*(data++) = (rate >> 24) & 0xff;
		/* byte rate */
		*(data++) = byterate & 0xff;
		*(data++) = (byterate >> 8) & 0xff;
		*(data++) = (byterate >> 16) & 0xff;
		*(data++) = (byterate >> 24) & 0xff;
		/* block align */
		*(data++) = block_align & 0xff;
		*(data++) = (block_align >> 8) & 0xff;
		/* word size */
		*(data++) = depth & 0xff;
		*(data++) = (depth >> 8) & 0xff;
		self->fixed_buffersize = rate * 30 / 1000;
		self->fixed_buffersize *= channels * depth / 8;
		self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;
		self->fixed_bufferduration = GST_SECOND * (GstClockTime)self->fixed_buffersize / (GstClockTime)byterate;
		GST_INFO_OBJECT(self, "MIMETYPE %s", type);
		self->bypass = AUDIOTYPE_RAW;
		gst_buffer_unmap(self->codec_data, &map);
	}
	else
	{
		GST_ELEMENT_ERROR(self, STREAM, TYPE_NOT_FOUND,(NULL),("unimplemented stream type %s", type));
		return FALSE;
	}
	if (!self->playing)
		GST_INFO_OBJECT(self, "set bypass 0x%02x", self->bypass);

	if (self->playing && self->bypass != previous_bypass)
	{
		if (self->fd >= 0)
		{
			GST_INFO_OBJECT(self,"SAME MEDIA set new bypass 0x%02x", self->bypass);
			ioctl(self->fd, AUDIO_STOP, 0);
			if (ioctl(self->fd, AUDIO_CLEAR_BUFFER) >= 0)
				GST_DEBUG_OBJECT(self, "NEW_bypass AUDIO BUFFER FLUSHED");
		}
		self->playing = FALSE;
	}
#ifdef AUDIO_SET_ENCODING
	if (self->use_set_encoding)
	{
		unsigned int encoding = bypass_to_encoding(self->bypass);
		if (self->fd < 0 || ioctl(self->fd, AUDIO_SET_ENCODING, encoding) < 0)
		{
			GST_ELEMENT_WARNING(self, STREAM, DECODE,(NULL),("hardware decoder can't be set to encoding %i", encoding));
		}
	}
	else
	{
		if (self->fd < 0 || ioctl(self->fd, AUDIO_SET_BYPASS_MODE, self->bypass) < 0)
		{
			GST_ELEMENT_ERROR(self, STREAM, TYPE_NOT_FOUND,(NULL),("hardware decoder can't be set to bypass mode type %s", type));
			return FALSE;
		}
	}
#else
	if (self->fd < 0 || ioctl(self->fd, AUDIO_SET_BYPASS_MODE, self->bypass) < 0)
	{
		GST_ELEMENT_ERROR(self, STREAM, TYPE_NOT_FOUND,(NULL),("hardware decoder can't be set to bypass mode type %s", type));
		return FALSE;
	}
#endif
		if(!self->playing && self->fd >= 0)
			ioctl(self->fd, AUDIO_PLAY);
		self->playing = TRUE;

	return TRUE;
}

static gboolean gst_dvbaudiosink_event(GstBaseSink *sink, GstEvent *event)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(sink);
	GST_DEBUG_OBJECT(self, "EVENT %s", gst_event_type_get_name(GST_EVENT_TYPE(event)));
	gboolean ret = TRUE, wait = FALSE;

	switch (GST_EVENT_TYPE(event))
	{
	case GST_EVENT_FLUSH_START:
		GST_INFO_OBJECT (self,"GST_EVENT_FLUSH_START");
		if(self->flushed && !self->playing && self->using_dts_downmix && (!self->paused || self->first_paused))
		{ 
			self->playing = TRUE;
			self->ok_to_write = 1;
		}
		self->flushed = FALSE;
		self->flushing = TRUE;
		/* wakeup the poll */
		write(self->unlockfd[1], "\x01", 1);
		break;
	case GST_EVENT_FLUSH_STOP:
		GST_INFO_OBJECT (self,"GST_EVENT_FLUSH_STOP");
		if (self->fd >= 0) ioctl(self->fd, AUDIO_CLEAR_BUFFER);
		GST_OBJECT_LOCK(self);
		while (self->queue)
		{
			queue_pop(&self->queue);
		}
		self->flushing = FALSE;
		self->timestamp = GST_CLOCK_TIME_NONE;
		self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;
		if (self->cache)
		{
			gst_buffer_unref(self->cache);
			self->cache = NULL;
		}
		GST_OBJECT_UNLOCK(self);
		/* flush while media is playing requires a delay before rendering */
		if(self->using_dts_downmix && (!self->paused || self->first_paused))
		{
			self->playing = FALSE;
			self->ok_to_write = 0;
		}
		self->flushed = TRUE;
		break;
	case GST_EVENT_STREAM_GROUP_DONE:
		self->pass_eos = TRUE;
		break;
	case GST_EVENT_EOS:
	{
		GST_INFO_OBJECT (self, "GST_EVENT_EOS");
#ifdef AUDIO_FLUSH
		if (self->fd >= 0) ioctl(self->fd, AUDIO_FLUSH, 1/*NONBLOCK*/); //Notify the player that no addionional data will be injected
#endif
		struct pollfd pfd[2];
		pfd[0].fd = self->unlockfd[0];
		pfd[0].events = POLLIN;
		pfd[1].fd = self->fd;
		pfd[1].events = POLLIN;

		int x = 0;
		int retval = 0;
		gint64 previous_pts = 0;
		gint64 current_pts = 0;
		gboolean first_loop_done = FALSE;
		GST_BASE_SINK_PREROLL_UNLOCK(sink);
		while (1)
		{
			retval = poll(pfd, 2, 250);
			if (retval < 0)
			{
				GST_INFO_OBJECT(self,"poll in EVENT_EOS");
				ret = FALSE;
				break;
			}
			else if ((pfd[0].revents & POLLIN) == POLLIN)
			{
				GST_INFO_OBJECT(self, "wait EOS aborted!! media is not ended");
				wait = TRUE;
				ret = FALSE;
				break;
			}
			else if ((pfd[1].revents & POLLIN) == POLLIN && first_loop_done)
			{
					GST_INFO_OBJECT(self, "got buffer empty from driver!");
					break;
			}
			else if (sink->flushing)
			{
				GST_INFO_OBJECT(self, "wait EOS flushing!!");
				wait = TRUE;
				ret = FALSE;
				break;
			}
			else
			{
				
				/* max 500 ms needed for 4K stb's for the first loop detection.
				 * note streamed live media may have an eternal position of 0
				 * We only will react on empty buffer event for streamed media which remains at zero
				 * Like usual this is not the case for all live streamed media */
				current_pts = gst_dvbaudiosink_get_decoder_time(self);

				if(current_pts > 0 || x >= 1)
				{
					if(previous_pts == current_pts && current_pts > 0)
					{
						GST_INFO_OBJECT(self,"Media ended push eos to basesink current_pts %" G_GINT64_FORMAT " previous_pts %" G_GINT64_FORMAT,
							current_pts, previous_pts);
						break;
					}
					else
					{
						if(previous_pts == 0 && x < 1)
						{
							gst_sleepms(500);
						}						
						else
							first_loop_done = TRUE;
						GST_DEBUG_OBJECT(self,"poll out current_pts %" G_GINT64_FORMAT " previous_pts %" G_GINT64_FORMAT,
							current_pts, previous_pts);
						previous_pts = current_pts;
						if(x < 1)
							x++;
					}
				}
				else if (x < 1)
				{
					gst_sleepms(500);
					x++;
				}
				else
					first_loop_done = TRUE;
			}
		}
		GST_BASE_SINK_PREROLL_LOCK(sink);
		break;
	}
	case GST_EVENT_SEGMENT:
	{
		const GstSegment *segment;
		gst_event_parse_segment(event, &segment);
		GstFormat format;
		gdouble rate;
		guint64 start, end, pos;
		gint64 start_dvb;
		format = segment->format;
		rate = segment->rate;
		start = segment->start;
		end = segment->stop;
		pos = segment->position;
		start_dvb = start / 11111LL;
		GST_INFO_OBJECT(self, "GST_EVENT_SEGMENT rate=%f format=%d start=%"G_GUINT64_FORMAT " position=%"G_GUINT64_FORMAT, rate, format, start, pos);
		GST_INFO_OBJECT(self, "SEGMENT DVB TIMESTAMP=%"G_GINT64_FORMAT " HEX=%#"G_GINT64_MODIFIER "x", start_dvb, start_dvb);
 		if (format == GST_FORMAT_TIME)
		{
			self->timestamp_offset = start - pos;
			self->rate = rate;
		}
		break;
	}
	case GST_EVENT_CAPS:
	{
		GstCaps *caps;
		gst_event_parse_caps(event, &caps);
		if (caps)
		{
			GST_INFO_OBJECT(self,"CAP %"GST_PTR_FORMAT, caps);
		}
		else
			ret = FALSE;
		break;
	}
	case GST_EVENT_TAG:
	{
		GstTagList *taglist;
		gst_event_parse_tag(event, &taglist);
		GST_DEBUG_OBJECT(self,"TAG %"GST_PTR_FORMAT, taglist);
		break;
	}
	default:
		break;
	}
	if (ret)
		ret = GST_BASE_SINK_CLASS(parent_class)->event(sink, event);
	else if (!wait)
		gst_event_unref(event);

	return ret;
}

static int audio_write(GstDVBAudioSink *self, GstBuffer *buffer, size_t start, size_t end)
{
	size_t written = start;
	size_t len = end;
	struct pollfd pfd[2];
	guint8 *data;
	int retval = 0;
	GstMapInfo map;
	gst_buffer_map(buffer, &map, GST_MAP_READ);
	data = map.data;

	pfd[0].fd = self->unlockfd[0];
	pfd[0].events = POLLIN;
	pfd[1].fd = self->fd;
	pfd[1].events = POLLOUT;

	do
	{
		if (self->flushing)
		{
			GST_INFO_OBJECT(self, "flushing, skip %d bytes", len - written);
			break;
		}
		else if (self->paused || self->unlocking)
		{
			GST_OBJECT_LOCK(self);
			queue_push(&self->queue, buffer, written, end);
			GST_OBJECT_UNLOCK(self);
			GST_INFO_OBJECT(self, "pushed %d bytes to queue", len - written);
			break;
		}
		else
		{
			GST_TRACE_OBJECT(self, "going into poll, have %d bytes to write", len - written);
		}
#if defined(__sh__) && !defined(CHECK_DRAIN)
		pfd[1].revents = POLLOUT;
#else
		if (poll(pfd, 2, -1) < 0)
		{
			GST_INFO_OBJECT(self,"poll(pfd, 2, -1) < 0");
			if (errno == EINTR) continue;
			retval = -1;
			break;
		}
#endif
		if (pfd[0].revents & POLLIN)
		{
			/* read all stop commands */
			while (1)
			{
				GST_INFO_OBJECT(self, "Read all stop commands");
				gchar command;
				int res = read(self->unlockfd[0], &command, 1);
				if (res < 0)
				{
					GST_INFO_OBJECT(self, "no more commands");
					/* no more commands */
					break;
				}
			}
			continue;
		}
		if (pfd[1].revents & POLLOUT)
		{
			size_t queuestart, queueend;
			GstBuffer *queuebuffer;
			GST_OBJECT_LOCK(self);
			if (queue_front(&self->queue, &queuebuffer, &queuestart, &queueend) >= 0)
			{
				guint8 *queuedata;
				GstMapInfo queuemap;
				gst_buffer_map(queuebuffer, &queuemap, GST_MAP_READ);
				queuedata = queuemap.data;
				int wr = write(self->fd, queuedata + queuestart, queueend - queuestart);
				gst_buffer_unmap(queuebuffer, &queuemap);
				if (wr < 0)
				{
					switch(errno)
					{
						case EINTR:
						case EAGAIN:
							break;
						default:
							GST_OBJECT_UNLOCK(self);
							retval = -3;
							break;
					}
					if (retval < 0) break;
				}
				else if (wr >= queueend - queuestart)
				{
					queue_pop(&self->queue);
					GST_INFO_OBJECT(self, "written %d queue bytes... pop entry", wr);
				}
				else
				{
					self->queue->start += wr;
					GST_INFO_OBJECT(self, "written %d queue bytes... update offset", wr);
				}
				GST_OBJECT_UNLOCK(self);
				continue;
			}
			GST_OBJECT_UNLOCK(self);
			int wr = write(self->fd, data + written, len - written);
			if (wr < 0)
			{
				switch(errno)
				{
					case EINTR:
					case EAGAIN:
						continue;
					default:
						retval = -3;
						break;
				}
				if (retval < 0) break;
			}
			written += wr;
		}
	} while (written < len);

	gst_buffer_unmap(buffer, &map);
	return retval;
}

GstFlowReturn gst_dvbaudiosink_push_buffer(GstDVBAudioSink *self, GstBuffer *buffer)
{
	guint8 *pes_header;
	gsize pes_header_len = 0;
	gsize size;
	guint8 *data, *original_data;
	guint8 *codec_data = NULL;
	gsize codec_data_size = 0;
	GstClockTime timestamp = self->timestamp;
	GstClockTime duration = GST_BUFFER_DURATION(buffer);
	GstMapInfo map, pesheadermap, codecdatamap;
	gst_buffer_map(buffer, &map, GST_MAP_READ);
	original_data = data = map.data;
	size = map.size;
	gst_buffer_map(self->pesheader_buffer, &pesheadermap, GST_MAP_WRITE);
	pes_header = pesheadermap.data;

	if (self->codec_data)
	{
		gst_buffer_map(self->codec_data, &codecdatamap, GST_MAP_READ);
		codec_data = codecdatamap.data;
		codec_data_size = codecdatamap.size;
	}
	/* 
	 * Some audioformats have incorrect timestamps, 
	 * so if we have both a timestamp and a duration, 
	 * keep extrapolating from the first timestamp instead
	 */
	if (timestamp == GST_CLOCK_TIME_NONE)
	{
		timestamp = GST_BUFFER_PTS(buffer);
		if (timestamp != GST_CLOCK_TIME_NONE && duration != GST_CLOCK_TIME_NONE)
		{
			self->timestamp = timestamp + duration;
		}
	}
	else
	{
		if (duration != GST_CLOCK_TIME_NONE)
		{
			self->timestamp += duration;
		}
		else
		{
			timestamp = GST_BUFFER_PTS(buffer);
			self->timestamp = GST_CLOCK_TIME_NONE;
		}
	}

	pes_header[0] = 0;
	pes_header[1] = 0;
	pes_header[2] = 1;
	pes_header[3] = 0xc0;

	pes_header[6] = 0x81;
	pes_header[7] = 0; /* no pts */
	pes_header[8] = 0;
	pes_header_len = 9;
	
	// looks that this does is never true for the time commented
	/*if (self->bypass == AUDIOTYPE_DTS)
	{
		int pos = 0;
		while ((pos + 4) <= size)
		{
			//check for DTS-HD
			if (!strcmp((char*)(data + pos), "\x64\x58\x20\x25"))
			{
				//GST_INFO_OBJECT(self," DTS-HD FOUND %x", (char*)(data + pos));
				size = pos;
				break;
			}
			++pos;
		}
	}*/

	if (timestamp != GST_CLOCK_TIME_NONE)
	{
		//GST_INFO_OBJECT(self,"timestamp = %" G_GUINT64_FORMAT , (GstClockTime)timestamp);
		pes_header[7] = 0x80; /* pts */
		pes_header[8] = 5; /* pts size */
		pes_header_len += 5;
		pes_set_pts(timestamp, pes_header);
	}


	if (self->aac_adts_header_valid)
	{
		size_t payload_len = size + 7;
		self->aac_adts_header[3] &= 0xC0;
		/* frame size over last 2 bits */
		self->aac_adts_header[3] |= (payload_len & 0x1800) >> 11;
		/* frame size continued over full byte */
		self->aac_adts_header[4] = (payload_len & 0x1FF8) >> 3;
		/* frame size continued first 3 bits */
		self->aac_adts_header[5] = (payload_len & 7) << 5;
		/* buffer fullness(0x7FF for VBR) over 5 last bits */
		self->aac_adts_header[5] |= 0x1F;
		/* buffer fullness(0x7FF for VBR) continued over 6 first bits + 2 zeros for
		 * number of raw data blocks */
		self->aac_adts_header[6] = 0xFC;
		memcpy(pes_header + pes_header_len, self->aac_adts_header, 7);
		pes_header_len += 7;
	}

	if (self->bypass == AUDIOTYPE_LPCM && (data[0] < 0xa0 || data[0] > 0xaf))
	{
		/*
		 * gstmpegdemux removes the streamid and the number of frames
		 * for certain lpcm streams, so we need to reconstruct them.
		 * Fortunately, the number of frames is ignored.
		 */
		pes_header[pes_header_len++] = 0xa0;
		pes_header[pes_header_len++] = 0x01;
	}
	else if (self->bypass == AUDIOTYPE_WMA || self->bypass == AUDIOTYPE_WMA_PRO)
	{
		if (self->codec_data)
		{
			size_t payload_len = size;
#if defined(DREAMBOX) || defined(DAGS)
			pes_header[pes_header_len++] = 0x42; // B
			pes_header[pes_header_len++] = 0x43; // C
			pes_header[pes_header_len++] = 0x4D; // M
			pes_header[pes_header_len++] = 0x41; // A
#endif
			pes_header[pes_header_len++] = (payload_len >> 24) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 16) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 8) & 0xff;
			pes_header[pes_header_len++] = payload_len & 0xff;
			memcpy(&pes_header[pes_header_len], codec_data, codec_data_size);
			pes_header_len += codec_data_size;
		}
	}
	else if (self->bypass == AUDIOTYPE_AMR)
	{
		if (self->codec_data && codec_data_size >= 17)
		{
			size_t payload_len = size + 17;
			pes_header[pes_header_len++] = (payload_len >> 24) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 16) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 8) & 0xff;
			pes_header[pes_header_len++] = payload_len & 0xff;
			memcpy(&pes_header[pes_header_len], codec_data + 8, 9);
			pes_header_len += 9;
		}
	}
	else if (self->bypass == AUDIOTYPE_RAW)
	{
		if (self->codec_data && codec_data_size >= 18)
		{
			size_t payload_len = size;
#if defined(DREAMBOX) || defined(DAGS)
			pes_header[pes_header_len++] = 0x42; // B
			pes_header[pes_header_len++] = 0x43; // C
			pes_header[pes_header_len++] = 0x4D; // M
			pes_header[pes_header_len++] = 0x41; // A
#endif
			pes_header[pes_header_len++] = (payload_len >> 24) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 16) & 0xff;
			pes_header[pes_header_len++] = (payload_len >> 8) & 0xff;
			pes_header[pes_header_len++] = payload_len & 0xff;
			memcpy(&pes_header[pes_header_len], codec_data, codec_data_size);
			pes_header_len += codec_data_size;
		}
	}

	pes_set_payload_size(size + pes_header_len - 6, pes_header);
	if (audio_write(self, self->pesheader_buffer, 0, pes_header_len) < 0) goto error;
	if (audio_write(self, buffer, data - original_data, data - original_data + size) < 0) goto error;
	if (timestamp != GST_CLOCK_TIME_NONE)
	{
		self->pts_written = TRUE;
	}
	gst_buffer_unmap(self->pesheader_buffer, &pesheadermap);
	if (self->codec_data)
	{
		gst_buffer_unmap(self->codec_data, &codecdatamap);
	}
	gst_buffer_unmap(buffer, &map);

	return GST_FLOW_OK;
error:
	gst_buffer_unmap(self->pesheader_buffer, &pesheadermap);
	if (self->codec_data)
	{
		gst_buffer_unmap(self->codec_data, &codecdatamap);
	}
	gst_buffer_unmap(buffer, &map);
	{
		GST_ELEMENT_ERROR(self, RESOURCE, READ,(NULL),
				("audio write: %s", g_strerror(errno)));
		GST_WARNING_OBJECT(self, "Audio write error");
		return GST_FLOW_ERROR;
	}
}

static GstFlowReturn gst_dvbaudiosink_render(GstBaseSink *sink, GstBuffer *buffer)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(sink);
	GstBuffer *disposebuffer = NULL;
	GstFlowReturn retval = GST_FLOW_OK;
	GstClockTime duration = GST_BUFFER_DURATION(buffer);
	gsize buffersize;
	buffersize = gst_buffer_get_size(buffer);
	GstClockTime timestamp = GST_BUFFER_PTS(buffer);
	gint i = 0;
	if (self->ok_to_write == 0)
	{
		// wait 1 seconds after flush and new segment 
		self->flushed = FALSE;
		self->ok_to_write = 1;
		self->playing = TRUE;
		gst_sleepms(1200);
		GST_INFO_OBJECT(self,"RESUME PLAY AFTER FLUSH + 1,2 SECOND");
	}
	if (self->bypass <= AUDIOTYPE_UNKNOWN)
	{
		GST_ELEMENT_ERROR(self, STREAM, FORMAT,(NULL), ("hardware decoder not setup (no caps in pipeline?)"));
		return GST_FLOW_ERROR;
	}

	if (self->fd < 0) return GST_FLOW_ERROR;

	if (GST_BUFFER_IS_DISCONT(buffer)) 
	{
		if (self->cache) 
		{
			gst_buffer_unref(self->cache);
			self->cache = NULL;
		}
		self->timestamp = GST_CLOCK_TIME_NONE;
		self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;
	}

	disposebuffer = buffer;
	/* grab an additional ref, because we need to return the buffer with the same refcount as we got it */
	gst_buffer_ref(buffer);

	if (self->skip)
	{
		GstBuffer *newbuffer;
		newbuffer = gst_buffer_copy_region(buffer, GST_BUFFER_COPY_ALL, self->skip, buffersize - self->skip);
		GST_BUFFER_PTS(newbuffer) = timestamp;
		GST_BUFFER_DURATION(newbuffer) = duration;
		if (disposebuffer) gst_buffer_unref(disposebuffer);
		buffer = disposebuffer = newbuffer;
		buffersize = gst_buffer_get_size(buffer);
	}

	if (self->cache)
	{
		/* join unrefs both buffers */
		buffer = gst_buffer_append(self->cache, buffer);
		buffersize = gst_buffer_get_size(buffer);
		GST_BUFFER_PTS(buffer) = timestamp;
		GST_BUFFER_DURATION(buffer) = duration;
		disposebuffer = buffer;
		self->cache = NULL;
	}

	if (buffer)
	{
		if (self->fixed_buffersize)
		{
			if (self->fixed_buffertimestamp == GST_CLOCK_TIME_NONE)
			{
				self->fixed_buffertimestamp = timestamp;
			}
			if (buffersize < self->fixed_buffersize)
			{
				self->cache = gst_buffer_copy(buffer);
				retval = GST_FLOW_OK;
			}
			else if (buffersize > self->fixed_buffersize)
			{
				int index = 0;
				while (index <= buffersize - self->fixed_buffersize)
				{
					GstBuffer *block;
					block = gst_buffer_copy_region(buffer, GST_BUFFER_COPY_ALL, index, self->fixed_buffersize);
					/* only the first buffer needs the correct timestamp, next buffer timestamps will be ignored (and extrapolated) */
					GST_BUFFER_PTS(block) = self->fixed_buffertimestamp;
					GST_BUFFER_DURATION(block) = self->fixed_bufferduration;
					self->fixed_buffertimestamp += self->fixed_bufferduration;
					gst_dvbaudiosink_push_buffer(self, block);
					gst_buffer_unref(block);
					index += self->fixed_buffersize;
				}
				if (index < buffersize)
				{
					self->cache = gst_buffer_copy_region(buffer, GST_BUFFER_COPY_ALL, index, buffersize - index);
				}
				retval = GST_FLOW_OK;
			}
			else
			{
				/* could still be the original buffer, make sure we can write metadata */
				if (!gst_buffer_is_writable(buffer))
				{
					GstBuffer *tmpbuf = gst_buffer_copy(buffer);
					GST_BUFFER_DURATION(tmpbuf) = self->fixed_bufferduration;
					retval = gst_dvbaudiosink_push_buffer(self, tmpbuf);
					gst_buffer_unref(tmpbuf);
				}
				else
				{
					GST_BUFFER_DURATION(buffer) = self->fixed_bufferduration;
					retval = gst_dvbaudiosink_push_buffer(self, buffer);
				}
			}
		}
		else
		{
			retval = gst_dvbaudiosink_push_buffer(self, buffer);
		}
	}

	if (disposebuffer) gst_buffer_unref(disposebuffer);
	return retval;
}

static gboolean gst_dvbaudiosink_start(GstBaseSink * basesink)
{
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);

	GST_DEBUG_OBJECT(self, "start");

	if (socketpair(PF_UNIX, SOCK_STREAM, 0, self->unlockfd) < 0)
	{
		perror("socketpair");
		goto error;
	}

	fcntl(self->unlockfd[0], F_SETFL, O_NONBLOCK);
	fcntl(self->unlockfd[1], F_SETFL, O_NONBLOCK);

	self->pesheader_buffer = gst_buffer_new_and_alloc(256);

	self->fd = open("/dev/dvb/adapter0/audio0", O_RDWR | O_NONBLOCK);

	self->pts_written = FALSE;
	self->lastpts = 0;

	return TRUE;
error:
	{
		GST_ELEMENT_ERROR(self, RESOURCE, OPEN_READ_WRITE,(NULL),
				GST_ERROR_SYSTEM);
		return FALSE;
	}
}

static gboolean gst_dvbaudiosink_stop(GstBaseSink * basesink)
{
	/* stop will reset the sink like in init fase init fase */
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(basesink);

	GST_INFO_OBJECT(self, "stop");

	if (self->fd >= 0)
	{
		if (self->playing)
			ioctl(self->fd, AUDIO_STOP);
		ioctl(self->fd, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_DEMUX);
		if (ioctl(self->fd, AUDIO_CLEAR_BUFFER) >= 0)
			GST_INFO_OBJECT(self, "STOP AUDIO BUFFER FLUSHED");
		close(self->fd);
	}
	if (self->codec_data)
		gst_buffer_unref(self->codec_data);
	if (self->pesheader_buffer)
		gst_buffer_unref(self->pesheader_buffer);
	if (self->cache)
		gst_buffer_unref(self->cache);
	while (self->queue)
	{
		queue_pop(&self->queue);
	}

	GST_INFO_OBJECT(self, "stop if self->unlockfd[1] >= 0");
	/* close write end first */
	if (self->unlockfd[1] >= 0)
	{
		close(self->unlockfd[1]);
		self->unlockfd[1] = -1;
	}
	GST_INFO_OBJECT(self, "stop if self->unlockfd[0] >= 0");
	if (self->unlockfd[0] >= 0)
	{
		close(self->unlockfd[0]);
		self->unlockfd[0] = -1;
	}
	self->codec_data = NULL;
	self->bypass = AUDIOTYPE_UNKNOWN;
	self->fixed_buffersize = 0;
	self->fixed_bufferduration = GST_CLOCK_TIME_NONE;
	self->fixed_buffertimestamp = GST_CLOCK_TIME_NONE;
	self->aac_adts_header_valid = self->pass_eos = FALSE;
	self->pesheader_buffer = NULL;
	self->cache = NULL;
	self->playing = self->flushing = self->unlocking = self->paused = self->first_paused = FALSE;
	self->pts_written = self->using_dts_downmix = self->synchronized = self->dts_cd = FALSE;
	self->lastpts = 0;
	self->timestamp_offset = 0;
	self->queue = NULL;
	self->fd = -1;
	self->unlockfd[0] = self->unlockfd[1] = -1;
	self->rate = 1.0;
	self->timestamp = GST_CLOCK_TIME_NONE;
	self->audio_stream_type = NULL;
#ifdef AUDIO_SET_ENCODING
	self->use_set_encoding = TRUE;
#else
	self->use_set_encoding = FALSE;
#endif
	gst_base_sink_set_sync(GST_BASE_SINK(self), FALSE);
	gst_base_sink_set_async_enabled(GST_BASE_SINK(self), FALSE);
	GST_INFO_OBJECT(self, "STOP MEDIA COMPLETED");
	return TRUE;
}

static GstStateChangeReturn gst_dvbaudiosink_change_state(GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstDVBAudioSink *self = GST_DVBAUDIOSINK(element);

	switch(transition)
	{
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_INFO_OBJECT(self,"GST_STATE_CHANGE_NULL_TO_READY");
// special debug added to check correct machinebuild during development phase
#ifdef DREAMBOX
		GST_INFO_OBJECT(self,"BUILD FOR DREAMBOX");
#endif
#ifdef VUPLUS
		GST_INFO_OBJECT(self,"BUILD FOR VUPLUS");
#endif
/* 	This debug added to check that sink was build for right boxtype
	In openatv the DVBMEDIASINK_CONFIG will in future be based on ${MACHINE}
	Depending on that other defines and or specific machine code can be set.
	Some extra defines will be added to configur.ac file and then be used to limit
	code lines or change some codelines at compile time, then the final build dvbmediasink
	can be kept small and small into memory also, cause some older stb'so have very few memory.
	But machine is also added as a defined var containing the stb box group for mediasink , this can then be used :
	in a if (!strcmp(machine, "<stbgroup>")) where stbgroup comes from ${MACHINE} at compile time.
	example for a dreambox 8000 it will be dm8000 for vuplus duo2 it will be vuduo2 for mutant51 it will be hd51. */
#ifdef machine
		GST_INFO_OBJECT(self,"BUILD FOR STB BOXTYPE %s", machine);
#endif
		self->ok_to_write = 1;
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_INFO_OBJECT(self,"GST_STATE_CHANGE_READY_TO_PAUSED");
		self->paused = TRUE;
		self->first_paused = TRUE;
		if (self->fd >= 0)
		{
			ioctl(self->fd, AUDIO_SELECT_SOURCE, AUDIO_SOURCE_MEMORY);
			ioctl(self->fd, AUDIO_PAUSE);
			/* the driver should(if the driver support that setting) only synchronize if gstreamer runs sync false mode */
			if(self->synchronized)
			{
				if(ioctl(self->fd, AUDIO_SET_AV_SYNC, FALSE) >= 0)
					GST_INFO_OBJECT(self," AUDIO_SET_AV_SYNC FALSE accepted by driver");
				else
					GST_ERROR_OBJECT(self,"AUDIO_SET_AV_SYNC FALSE ***NOT*** accepted by driver critical ioctl error");
			}
			else
			{
				if(ioctl(self->fd, AUDIO_SET_AV_SYNC, TRUE) >= 0)
					GST_INFO_OBJECT(self," AUDIO_SET_AV_SYNC TRUE accepted by driver");
				else
					GST_ERROR_OBJECT(self,"AUDIO_SET_AV_SYNC TRUE ***NOT*** accepted by driver critical ioctl error");
			}
		}
// dreambox driver issue patch
#ifdef DREAMBOX
		if(get_downmix_ready())
			self->using_dts_downmix = TRUE;
#endif
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		GST_INFO_OBJECT(self,"GST_STATE_CHANGE_PAUSED_TO_PLAYING");
#ifdef DREAMBOX
		if(self->using_dts_downmix && self->first_paused)
		{
			self->first_paused = FALSE;
			gst_sleepms(1800);
			GST_INFO_OBJECT(self, "USING DTSDOWMIX DELAY START 1800 ms");
		}
#endif
		if (self->fd >= 0)
		{
			ioctl(self->fd, AUDIO_CONTINUE);
		}
		self->first_paused = FALSE;
		self->paused = FALSE;
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

	switch(transition)
	{
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_INFO_OBJECT(self,"GST_STATE_CHANGE_PLAYING_TO_PAUSED");
		self->paused = TRUE;
		if (self->fd >= 0)
			ioctl(self->fd, AUDIO_PAUSE);
		/* wakeup the poll */
		write(self->unlockfd[1], "\x01", 1);
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_INFO_OBJECT(self,"GST_STATE_CHANGE_PAUSED_TO_READY");
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_INFO_OBJECT(self,"GST_STATE_CHANGE_READY_TO_NULL");
		break;
	default:
		break;
	}

	return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 *
 * exchange the string 'plugin' with your elemnt name
 */
static gboolean plugin_init(GstPlugin *plugin)
{
	gst_debug_set_colored(GST_DEBUG_COLOR_MODE_OFF);
	if (!gst_element_register(plugin, "dvbaudiosink",
						 GST_RANK_PRIMARY + 1,
						 GST_TYPE_DVBAUDIOSINK))
	return FALSE;

	return TRUE;
}

/* this is the structure that gstreamer looks for to register plugins
 *
 * exchange the strings 'plugin' and 'Template plugin' with you plugin name and
 * description
 */
GST_PLUGIN_DEFINE(
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	dvbaudiosink,
	"DVB Audio Output",
	plugin_init,
	VERSION,
	"LGPL",
	"GStreamer",
	"http://gstreamer.net/"
)
