/*
 * MobiAqua MPV GUI
 *
 * Copyright (C) 2024 Pawel Kolodziejski
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined(BUILD_SDL2)

#include "basetypes.h"
#include "logs.h"
#include "remote.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <poll.h>

#include <linux/types.h>
#include <linux/input.h>

namespace MpvGui {

#define EVDEV_MAX_EVENTS 32

#define USB_VENDOR_PS3REMOTE          0x054C
#define USB_DEVICE_PS3REMOTE          0x0306
#define USB_VENDOR_R2REMOTE           0x1915
#define USB_DEVICE_R2REMOTE           0xEEEE

enum remoteType {
	REMOTE_UNKNOWN = 0,
	REMOTE_PS3_BD = 1,
	REMOTE_SATECHI_R2 = 2
};

static enum remoteType remote;

struct mapping {
	int linuxKeycode;
	int key;
};

static const struct mapping PS3RemoteMapping[] = {
	{ KEY_ENTER,         'e'  },
	{ KEY_UP,            'u'  },
	{ KEY_LEFT,          'l'  },
	{ KEY_RIGHT,         'r'  },
	{ KEY_DOWN,          'd'  },
	{ KEY_PLAY,          'p'  },
	{ -1,                 -1  }
};

static const struct mapping R2RemoteMapping[] = {
	{ KEY_VOLUMEUP,       'u' },
	{ KEY_PREVIOUSSONG,   'l' },
	{ KEY_NEXTSONG,       'r' },
	{ KEY_VOLUMEDOWN,     'd' },
	{ KEY_PLAYPAUSE,      'e' },
	{ -1,                 -1  }
};

static struct threadPriv_ {
	int fd[2];
} threadPriv;

static pthread_t threadHandle;
static int threadExit;
static int threadExited;
static int remoteFd = -1;
static int initialized;

static int lookupButtonKey(struct input_event *ev) {
	int i;

	switch (remote) {
	case REMOTE_PS3_BD: {
		for (i = 0; PS3RemoteMapping[i].linuxKeycode != -1; i++) {
			if (PS3RemoteMapping[i].linuxKeycode == ev->code) {
				return PS3RemoteMapping[i].key;
			}
		}
		break;
	}
	case REMOTE_SATECHI_R2: {
		for (i = 0; R2RemoteMapping[i].linuxKeycode != -1; i++) {
			if (R2RemoteMapping[i].linuxKeycode == ev->code) {
				return R2RemoteMapping[i].key;
			}
		}
		break;
	}
	}

	return -1;
}

static int scanRemote() {
	int i, fd;

	// look for a valid PS3 BD Remote device on system
	for (i = 0; i < EVDEV_MAX_EVENTS; i++) {
		struct input_id id;
		char file[64];
		char device_name[100];

		sprintf(file, "/dev/input/event%d", i);
		fd = open(file, O_RDONLY | O_NONBLOCK);
		if (fd < 0)
			continue;

		if (ioctl(fd, EVIOCGID, &id) != -1 && id.bustype == BUS_BLUETOOTH) {
			if (id.vendor == USB_VENDOR_PS3REMOTE && id.product == USB_DEVICE_PS3REMOTE) {
				remote = REMOTE_PS3_BD;
				return fd;
			}
			if (id.vendor == USB_VENDOR_R2REMOTE && id.product == USB_DEVICE_R2REMOTE) {
				if (ioctl(fd, EVIOCGNAME(sizeof(device_name) - 1), &device_name) > 0) {
					if (strncmp(device_name, "R2 Remote Keyboard", sizeof(device_name)) == 0) {
						remote = REMOTE_SATECHI_R2;
						return fd;
					}
				}
			}
			remote = REMOTE_UNKNOWN;
			close (fd);
		}
	}

	return -1;
}

static void *threadRemote(void *ptr) {
	struct thread_priv_ *priv = static_cast<struct thread_priv_ *>(ptr);
	int inotifyFd = -1, inotifyWd = -1;
	int inputFd, readInput, writeOutput;
	char buf[1000];
	fd_set set;
	struct timeval timeout;
	struct input_event event;

	inputFd = scanRemote();

	inotifyFd = inotify_init();
	if (inotifyFd < 0) {
		log->printf("Couldn't initialize inotify");
		goto exit;
	}

	inotifyWd = inotify_add_watch(inotifyFd, "/dev/input", IN_CREATE | IN_DELETE);
	if (inotifyWd < 0) {
		log->printf("Couldn't add watch to /dev/input\n");
		goto exit;
	}

	while (threadExit == 0) {
		FD_ZERO(&set);
		FD_SET(inotifyFd, &set);
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;

		if (select(inotifyFd + 1, &set, NULL, NULL, &timeout) > 0 && FD_ISSET(inotifyFd, &set)) {
			read(inotifyFd, buf, 1000);
			if (inputFd != -1)
				close(inputFd);
			inputFd = scanRemote();
		}

		while (inputFd != -1) {
			readInput = read(inputFd, &event, sizeof (struct input_event));
			if (readInput < 0 && errno != EAGAIN) {
				close(inputFd);
				inputFd = -1;
				break;
			}
			if (readInput < static_cast<int>(sizeof (struct input_event))) {
				break;
			}
			writeOutput = write(threadPriv.fd[1], &event, sizeof (struct input_event));
			if (writeOutput != sizeof (struct input_event)) {
				log->printf("Couldn't write to output pipe\n");
				break;
			}
		}

		usleep(10000);
	}

exit:
	if (inotifyWd != -1)
		inotify_rm_watch(inotifyFd, inotifyWd);
	if (inotifyFd != -1)
		close(inotifyFd);
	if (inputFd != -1)
		close(inputFd);

	threadExited = 1;
	return NULL;
}

int RemoteInit() {
	threadExit = 0;
	threadExited = 0;

	if (pipe(threadPriv.fd) != 0) {
		return -1;
	}

	if (pthread_create(&threadHandle, NULL, threadRemote, static_cast<void *>(&threadPriv)) != 0) {
		close(threadPriv.fd[0]);
		close(threadPriv.fd[1]);
		return -1;
	}

	remoteFd = threadPriv.fd[0];
	int flags = fcntl(remoteFd, F_GETFL, 0);
	fcntl(remoteFd, F_SETFL, flags | O_NONBLOCK);

	initialized = 1;

	return 0;
}

void RemoteClose() {
	void *result;
	int status;

	if (!initialized)
		return;

	threadExit = 1;

	while (!threadExited) { usleep(10000); }

	close(threadPriv.fd[0]);
	close(threadPriv.fd[1]);
	remoteFd = -1;

	threadExit = 0;
	threadExited = 0;
}

int RemoteRead() {
	struct input_event ev;
	int i, r;

	if (!initialized || remoteFd == -1)
		return -1;

	r = read(remoteFd, &ev, sizeof (struct input_event));
	if (r <= 0 || r < sizeof (struct input_event))
		return -1;

	// check for key press only
	if (ev.type != EV_KEY)
		return -1;

	// EvDev Key values:
	// 0: key release
	// 1: key press
	if (ev.value == 0)
		return -1;

	return lookupButtonKey(&ev);
}

} // namespace

#else

#include <SDL.h>

namespace MpvGui {

int RemoteInit() {
	return 0;
}

void RemoteClose() {
}

int RemoteRead() {
	SDL_Event event;

	if (!SDL_PollEvent(&event)) {
		return -1;
	}
	if (event.type == SDL_QUIT) {
		exit(0);
	}
	if (event.type == SDL_KEYDOWN) {
		switch (event.key.keysym.sym) {
		case SDLK_UP:
			return 'u';
		case SDLK_DOWN:
			return 'd';
		case SDLK_LEFT:
			return 'l';
		case SDLK_RIGHT:
			return 'r';
		}
	}

	return -1;
}

} // namespace

#endif
