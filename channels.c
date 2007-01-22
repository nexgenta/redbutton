/*
 * channels.c
 */

/*
 * Copyright (C) 2007, Simon Kilvington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/dvb/frontend.h>

#include "utils.h"

/* internal functions */
static struct dvb_frontend_parameters *get_tune_params(uint16_t);

static FILE *_channels = NULL;

/*
 * if filename is NULL, it searches for:
 * ~/.tzap/channels.conf
 * /etc/channels.conf
 */

bool
init_channels_conf(char *filename)
{
	char *home;
	char pathname[PATH_MAX];

	if(_channels != NULL)
		fatal("init_channels_conf: already initialised");

	if(filename == NULL)
	{
		if((home = getenv("HOME")) != NULL)
		{
			snprintf(pathname, sizeof(pathname), "%s/.tzap/channels.conf", home);
			verbose("Trying to open %s", pathname);
			_channels = fopen(pathname, "r");
		}
		if(_channels == NULL)
		{
			verbose("Trying to open /etc/channels.conf");
			_channels = fopen("/etc/channels.conf", "r");
		}
	}
	else
	{
		verbose("Trying to open %s", filename);
		_channels = fopen(filename, "r");
	}

	return (_channels != NULL);
}

/*
 * map strings to frontend enum values
 * based on tzap utility in linuxtv dvb-apps package
 */
struct param
{
	char *name;
	int value;
};

static const struct param inversion_list[] =
{
	{ "INVERSION_OFF", INVERSION_OFF },
	{ "INVERSION_ON", INVERSION_ON },
	{ "INVERSION_AUTO", INVERSION_AUTO }
};

static const struct param bw_list[] =
{
	{ "BANDWIDTH_6_MHZ", BANDWIDTH_6_MHZ },
	{ "BANDWIDTH_7_MHZ", BANDWIDTH_7_MHZ },
	{ "BANDWIDTH_8_MHZ", BANDWIDTH_8_MHZ }
};

static const struct param fec_list[] =
{
	{ "FEC_1_2", FEC_1_2 },
	{ "FEC_2_3", FEC_2_3 },
	{ "FEC_3_4", FEC_3_4 },
	{ "FEC_4_5", FEC_4_5 },
	{ "FEC_5_6", FEC_5_6 },
	{ "FEC_6_7", FEC_6_7 },
	{ "FEC_7_8", FEC_7_8 },
	{ "FEC_8_9", FEC_8_9 },
	{ "FEC_AUTO", FEC_AUTO },
	{ "FEC_NONE", FEC_NONE }
};

static const struct param guard_list[] =
{
	{"GUARD_INTERVAL_1_16", GUARD_INTERVAL_1_16},
	{"GUARD_INTERVAL_1_32", GUARD_INTERVAL_1_32},
	{"GUARD_INTERVAL_1_4", GUARD_INTERVAL_1_4},
	{"GUARD_INTERVAL_1_8", GUARD_INTERVAL_1_8}
};

static const struct param hierarchy_list[] =
{
	{ "HIERARCHY_1", HIERARCHY_1 },
	{ "HIERARCHY_2", HIERARCHY_2 },
	{ "HIERARCHY_4", HIERARCHY_4 },
	{ "HIERARCHY_NONE", HIERARCHY_NONE }
};

static const struct param constellation_list[] =
{
	{ "QPSK", QPSK },
	{ "QAM_128", QAM_128 },
	{ "QAM_16", QAM_16 },
	{ "QAM_256", QAM_256 },
	{ "QAM_32", QAM_32 },
	{ "QAM_64", QAM_64 }
};

static const struct param transmissionmode_list[] =
{
	{ "TRANSMISSION_MODE_2K", TRANSMISSION_MODE_2K },
	{ "TRANSMISSION_MODE_8K", TRANSMISSION_MODE_8K },
};

#define LIST_SIZE(x)	(sizeof(x) / sizeof(struct param))

static int
str2enum(char *str, const struct param *map, int map_size)
{
	while(map_size >= 0)
	{
		map_size --;
		if(strcmp(str, map[map_size].name) == 0)
			return map[map_size].value;
	}

	fatal("Invalid parameter '%s' in channels.conf file", str);

	/* not reached */
	return -1;
}

/*
 * return the params needed to tune to the given service_id
 * the data is read from the channels.conf file
 * returns NULL if the service_id is not found
 */

static struct dvb_frontend_parameters _params;

static struct dvb_frontend_parameters *
get_tune_params(uint16_t service_id)
{
	char line[1024];
	unsigned int freq;
	char inv[32];
	char bw[32];
	char hp[32];
	char lp[32];
	char qam[32];
	char trans[32];
	char gi[32];
	char hier[32];
	unsigned int id;
	int len;

	bzero(&_params, sizeof(_params));

	verbose("Searching channels.conf for service_id %u", service_id);

	rewind(_channels);
	while(!feof(_channels))
	{
		if(fgets(line, sizeof(line), _channels) == NULL
		|| sscanf(line, "%*[^:]:%u:%32[^:]:%32[^:]:%32[^:]:%32[^:]:%32[^:]:%32[^:]:%32[^:]:%32[^:]:%*[^:]:%*[^:]:%u", &freq, inv, bw, hp, lp, qam, trans, gi, hier, &id) != 10
		|| id != service_id)
			continue;
		/* chop off trailing \n */
		len = strlen(line) - 1;
		while(len >= 0 && line[len] == '\n')
			line[len--] = '\0';
		verbose("%s", line);
		_params.frequency = freq;
		_params.inversion = str2enum(inv, inversion_list, LIST_SIZE(inversion_list));
		_params.u.ofdm.bandwidth = str2enum(bw, bw_list, LIST_SIZE(bw_list));
		_params.u.ofdm.code_rate_HP = str2enum(hp, fec_list, LIST_SIZE(fec_list));
		_params.u.ofdm.code_rate_LP = str2enum(lp, fec_list, LIST_SIZE(fec_list));
		_params.u.ofdm.constellation = str2enum(qam, constellation_list, LIST_SIZE(constellation_list));
		_params.u.ofdm.transmission_mode = str2enum(trans, transmissionmode_list, LIST_SIZE(transmissionmode_list));
		_params.u.ofdm.guard_interval = str2enum(gi, guard_list, LIST_SIZE(guard_list));
		_params.u.ofdm.hierarchy_information = str2enum(hier, hierarchy_list, LIST_SIZE(hierarchy_list));
		return &_params;
	}

	return NULL;
}

/*
 * retune to the frequency the given service_id is on
 */

bool
tune_service_id(unsigned int adapter, unsigned int timeout, uint16_t service_id)
{
	char fe_dev[PATH_MAX];
	struct dvb_frontend_info fe_info;
	struct dvb_frontend_parameters current_params;
	struct dvb_frontend_parameters *needed_params;
	struct dvb_frontend_event event;
	bool lock;
	/* need to keep the frontend device open to stop it untuning itself */
	/* TODO: fix this hack */
	static int fe_fd = -1;

	vverbose("Getting frontend info");

	/* see what we are currently tuned to */
	snprintf(fe_dev, sizeof(fe_dev), FE_DEVICE, adapter);
	/*
	 * need O_RDWR if you want to tune, O_RDONLY is okay for getting info
	 * if someone else is using the frontend, we can only open O_RDONLY
	 * => we can still download data, but just not retune
	 */
	if(fe_fd != -1)
		close(fe_fd);
	if((fe_fd = open(fe_dev, O_RDONLY | O_NONBLOCK)) < 0)
		fatal("open '%s': %s", fe_dev, strerror(errno));

	if(ioctl(fe_fd, FE_GET_INFO, &fe_info) < 0)
		fatal("ioctl FE_GET_INFO: %s", strerror(errno));

	if(fe_info.type != FE_OFDM)
		fatal("TODO: Only able to tune DVB-T devices at present");

	if(ioctl(fe_fd, FE_GET_FRONTEND, &current_params) < 0)
		fatal("ioctl FE_GET_FRONTEND: %s", strerror(errno));

	/* find the tuning params for the service */
	if((needed_params = get_tune_params(service_id)) == NULL)
		fatal("service_id %u not found in channels.conf file", service_id);

	/* are we already tuned to the right frequency */
	vverbose("Current frequency %u; needed %u", current_params.frequency, needed_params->frequency);
	if(current_params.frequency != needed_params->frequency)
	{
		verbose("Retuning to frequency %u", needed_params->frequency);
		close(fe_fd);
		if((fe_fd = open(fe_dev, O_RDWR | O_NONBLOCK)) < 0)
			fatal("open '%s': %s", fe_dev, strerror(errno));
		/* empty event queue */
		while(ioctl(fe_fd, FE_GET_EVENT, &event) >= 0)
			; /* do nothing */
		/* tune in */
		if(ioctl(fe_fd, FE_SET_FRONTEND, needed_params) < 0)
			fatal("ioctl FE_GET_FRONTEND: %s", strerror(errno));
		/* wait for lock */
		vverbose("Waiting for tuner to lock on");
		/* TODO: use timeout value here */
		lock = false;
		while(!lock)
		{
			if(ioctl(fe_fd, FE_GET_EVENT, &event) >= 0)
				lock = event.status & FE_HAS_LOCK;
		}
		vverbose("Retuned");
	}

	return true;
}


