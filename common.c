#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gst/gst.h>


#include "common.h"
#include "gstdvbsink-marshal.h"

void queue_push(queue_entry_t **queue_base, GstBuffer *buffer, size_t start, size_t end)
{
	queue_entry_t *entry = g_malloc(sizeof(queue_entry_t));
	queue_entry_t *last = *queue_base;
#if GST_VERSION_MAJOR < 1
	gst_buffer_ref(buffer);
	entry->buffer = buffer;
#else
	entry->buffer = gst_buffer_copy(buffer);
#endif
	entry->start = start;
	entry->end = end;
	if (!last)
	{
		*queue_base = entry;
	}
	else
	{
		while (last->next) last = last->next;
		last->next = entry;
	}
	entry->next = NULL;
}

void queue_pop(queue_entry_t **queue_base)
{
	queue_entry_t *base = *queue_base;
	*queue_base = base->next;
	gst_buffer_unref(base->buffer);
	g_free(base);
}

int queue_front(queue_entry_t **queue_base, GstBuffer **buffer, size_t *start, size_t *end)
{
	if (!*queue_base)
	{
		*buffer = NULL;
		*start = 0;
		*end = 0;
		return -1;
	}
	else
	{
		queue_entry_t *entry = *queue_base;
		*buffer = entry->buffer;
		*start = entry->start;
		*end = entry->end;
		return 0;
	}
}

void pes_set_pts(long long timestamp, unsigned char *pes_header)
{
	unsigned long long pts = timestamp * 9LL / 100000; /* convert ns to 90kHz */
	pes_header[9] =  0x21 | ((pts >> 29) & 0xE);
	pes_header[10] = pts >> 22;
	pes_header[11] = 0x01 | ((pts >> 14) & 0xFE);
	pes_header[12] = pts >> 7;
	pes_header[13] = 0x01 | ((pts << 1) & 0xFE);
}

void pes_set_payload_size(size_t size, unsigned char *pes_header)
{
	if (size > 0xffff) size = 0;
	pes_header[4] = size >> 8;
	pes_header[5] = size & 0xFF;
}

void gst_sleepms(uint32_t msec)
{
	//does not interfere with signals like sleep and usleep do
	struct timespec req_ts;
	req_ts.tv_sec = msec / 1000;
	req_ts.tv_nsec = (msec % 1000) * 1000000L;
	int32_t olderrno = errno; // Some OS seem to set errno to ETIMEDOUT when sleeping
	while (1)
	{
		/* Sleep for the time specified in req_ts. If interrupted by a
		signal, place the remaining time left to sleep back into req_ts. */
		int rval = nanosleep (&req_ts, &req_ts);
		if (rval == 0)
			break; // Completed the entire sleep time; all done.
		else if (errno == EINTR)
			continue; // Interrupted by a signal. Try again.
		else 
			break; // Some other error; bail out.
	}
	errno = olderrno;
}

void gst_sleepus(uint32_t usec)
{
	//does not interfere with signals like sleep and usleep do
	struct timespec req_ts;
	req_ts.tv_sec = usec / 1000000;
	req_ts.tv_nsec = (usec % 1000000) * 1000L;
	int32_t olderrno = errno;       // Some OS seem to set errno to ETIMEDOUT when sleeping
	while (1)
	{
		/* Sleep for the time specified in req_ts. If interrupted by a
		signal, place the remaining time left to sleep back into req_ts. */
		int rval = nanosleep (&req_ts, &req_ts);
		if (rval == 0)
			break; // Completed the entire sleep time; all done.
		else if (errno == EINTR)
			continue; // Interrupted by a signal. Try again.
		else 
			break; // Some other error; bail out.
	}
	errno = olderrno;
}

#ifdef HAVE_DTSDOWNMIX

gboolean get_dtsdownmix_playing()
{
	gboolean ret = FALSE;
	FILE *f;
	char buffer[10] = {0};
	f = fopen("/tmp/dtsdownmix", "r");
	if (f)
	{
		fread(buffer, sizeof(buffer), 1, f);
		fclose(f);
	}
	ret = !strncmp(buffer, "PLAYING", 7);
	return ret;
}

gboolean get_dtsdownmix_pause()
{
	FILE *f;
	gboolean ret = FALSE;
	char buffer[10] = {0};
	f = fopen("/tmp/dtsdownmix", "r");
	if (f)
	{
		fread(buffer, sizeof(buffer), 1, f);
		fclose(f);
	}
	if(!strncmp(buffer, "PAUSE", 5))
	{
		ret = TRUE;
	}
	return ret;
}
#endif
