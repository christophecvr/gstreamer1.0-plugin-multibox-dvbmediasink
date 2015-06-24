#ifndef _common_h
#define _common_h
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>

#include <time.h>
#include <sys/time.h>
#include <stdlib.h>


typedef struct queue_entry
{
	GstBuffer *buffer;
	struct queue_entry *next;
	size_t start;
	size_t end;
} queue_entry_t;

void queue_push(queue_entry_t **queue_base, GstBuffer *buffer, size_t start, size_t end);
void queue_pop(queue_entry_t **queue_base);
int queue_front(queue_entry_t **queue_base, GstBuffer **buffer, size_t *start, size_t *end);

void pes_set_pts(long long timestamp, unsigned char *pes_header);
void pes_set_payload_size(size_t size, unsigned char *pes_header);

void gst_sleepms(uint32_t msec);
void gst_sleepus(uint32_t usec);
gboolean get_servicemp3_playing();
gboolean get_servicemp3_paused();
gboolean get_servicemp3_ready();
#ifdef HAVE_DTSDOWNMIX
/* enumeration dtsdownmix_state */
typedef enum downmix_state
{
	NONE=0,
	PAUSED=1,
	PLAYING=2,
} t_dtsdownmix_state;
gboolean get_dtsdownmix_playing();
gboolean get_dtsdownmix_pause();
#endif

#endif
