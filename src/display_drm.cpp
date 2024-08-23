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

#include "display_drm.h"

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <drm.h>
#include <poll.h>
#include "display_base.h"
#include "logs.h"

namespace MpvGui {

DisplayDrm::DisplayDrm() :
		_fd(-1), _drmResources(nullptr),
		_oldCrtc(nullptr), _drmPlaneResources(nullptr), _connectorId(-1),
		_crtcId(-1), _planeId(-1), _width(0), _height(0),
		_waitingForFlip(true),
		_currentBuffer() {
}

DisplayDrm::~DisplayDrm() {
	deinit();
}

STATUS DisplayDrm::init() {
	if (_initialized)
		return S_FAIL;

	if (internalInit() == S_FAIL)
		return S_FAIL;

	return S_OK;
}

STATUS DisplayDrm::deinit() {
	if (!_initialized)
		return S_FAIL;

	internalDeinit();

	return S_OK;
}

void *DisplayDrm::getBufferPtr() {
	if (!_initialized)
		return nullptr;

	return _frameBuffers[_currentBuffer].ptr;
}

U32 DisplayDrm::getBufferWidth() {
	if (!_initialized)
		return 0;

	return _frameBuffers[_currentBuffer].width;
}

U32 DisplayDrm::getBufferHeight() {
	if (!_initialized)
		return 0;

	return _frameBuffers[_currentBuffer].height;
}

U32 DisplayDrm::getBufferStride() {
	if (!_initialized)
		return 0;

	return _frameBuffers[_currentBuffer].stride;
}

static void drm_page_flip(int fd, unsigned int msc, unsigned int sec,
                          unsigned int usec, void *data) {
	DisplayDrm *display = (DisplayDrm *)data;

	display->_waitingForFlip = false;
}

STATUS DisplayDrm::internalInit() {
	drmDevice *devices[DRM_MAX_MINOR] = { 0 };
	uint32_t handles[4] = { 0 }, pitches[4] = { 0 }, offsets[4] = { 0 };
	struct drm_mode_create_dumb creq = { 0 };
	struct drm_mode_map_dumb mreq = { 0 };
	drmModeConnectorPtr connector = nullptr;
	drmModeObjectPropertiesPtr props;
	int crtcIndex = -1;
	int modeId = -1;
	int ret;

	int card_count = drmGetDevices2(0, devices, SIZE_OF_ARRAY(devices));
	for (int i = 0; i < card_count; i++) {
		drmDevice *dev = devices[i];
		if (!(dev->available_nodes & (1 << DRM_NODE_PRIMARY))) {
			continue;
		}
		_fd = open(dev->nodes[DRM_NODE_PRIMARY], O_RDWR | O_CLOEXEC);
		_drmResources = drmModeGetResources(_fd);
		if (!_drmResources) {
			close(_fd);
			_fd = -1;
			continue;
		}
		break;
	}
	if (_fd < 0) {
		log->printf("DisplayDrm::internalInit(): Failed open, %s\n", strerror(errno));
		goto fail;
	}

	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	if (!_drmResources) {
		log->printf("DisplayDrm::internalInit(): Failed get DRM plane resources, %s\n", strerror(errno));
		goto fail;
	}

	for (int i = 0; i < _drmResources->count_connectors; i++) {
		connector = drmModeGetConnector(_fd, _drmResources->connectors[i]);
		if (connector == nullptr)
			continue;
		if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
			_connectorId = connector->connector_id;
			break;
		}
		drmModeFreeConnector(connector);
	}
	if (_connectorId == -1) {
		log->printf("DisplayDrm::internalInit(): Failed to find connector!\n");
		goto fail;
	}

	for (int j = 0; j < connector->count_modes; j++) {
		auto mode = &connector->modes[j];
		if (mode->type & DRM_MODE_TYPE_PREFERRED) {
			modeId = j;
			break;
		}
	}

	if (modeId == -1) {
		U64 hightestArea = 0;
		for (int j = 0; j < connector->count_modes; j++) {
			auto mode = &connector->modes[j];
			const U64 area = mode->hdisplay * mode->vdisplay;
			if (area > hightestArea) {
				hightestArea = area;
				modeId = j;
			}
		}
	}

	_crtcId = -1;
	for (int i = 0; i < _drmResources->count_encoders; i++) {
		auto encoder = drmModeGetEncoder(_fd, _drmResources->encoders[i]);
		if (!encoder) {
			continue;
		}
		if (encoder->encoder_id == connector->encoder_id && encoder->crtc_id != 0) {
			_crtcId = encoder->crtc_id;
			drmModeFreeEncoder(encoder);
			break;
		}
		drmModeFreeEncoder(encoder);
	}

	if (modeId == -1 || _crtcId == -1) {
		log->printf("DisplayDrm::internalInit(): Failed to find suitable display output!\n");
		drmModeFreeConnector(connector);
		goto fail;
	}

	_modeInfo = connector->modes[modeId];

	drmModeFreeConnector(connector);

	if (drmSetClientCap(_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
		log->printf("DisplayDrm::internalInit(): Failed to set universal planes capability!\n");
		goto fail;
	}
	_drmPlaneResources = drmModeGetPlaneResources(_fd);
	if (!_drmPlaneResources) {
		log->printf("DisplayDrm::internalInit(): Failed to plane resources!\n");
		goto fail;
	}

	for (int i = 0; i < _drmResources->count_crtcs; i++) {
		if (_drmResources->crtcs[i] == _crtcId) {
			crtcIndex = i;
			break;
		}
	}

	_planeId = -1;
	for (int i = 0; i < _drmPlaneResources->count_planes; i++) {
		drmModePlane *plane = drmModeGetPlane(_fd, _drmPlaneResources->planes[i]);
		if (plane == nullptr)
			continue;
		uint32_t possible_crtcs = plane->possible_crtcs;
		if (possible_crtcs & (1 << crtcIndex)) {
			drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(_fd, plane->plane_id, DRM_MODE_OBJECT_PLANE);
			if (!props) {
				log->printf("DisplayDrm::internalInit(): Failed to find properties for plane!\n");
				drmModeFreePlane(plane);
				break;
			}
			for (int i = 0; i < props->count_props; i++) {
				drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
				if (prop != nullptr && strcmp(prop->name, "type") == 0) {
					uint64_t value = props->prop_values[i];
					if (_planeId == -1 && value == DRM_PLANE_TYPE_PRIMARY) {
						_planeId = plane->plane_id;
					}
				}
				drmModeFreeProperty(prop);
			}
			drmModeFreeObjectProperties(props);
		}
		drmModeFreePlane(plane);
	}
	if (_planeId == -1) {
		log->printf("DisplayDrm::internalInit(): Failed to find plane!\n");
		goto fail;
	}

	props = drmModeObjectGetProperties(_fd, _planeId, DRM_MODE_OBJECT_PLANE);
	if (!props) {
		log->printf("DisplayDrm::internalInit(): Failed to find properties for plane!\n");
		goto fail;
	}
	for (int i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop = drmModeGetProperty(_fd, props->props[i]);
		if (prop != nullptr && strcmp(prop->name, "zorder") == 0 && drm_property_type_is(prop, DRM_MODE_PROP_RANGE)) {
			if (drmModeObjectSetProperty(_fd, _planeId, DRM_MODE_OBJECT_PLANE, prop->prop_id, 1)) {
				log->printf("DisplayDrm::internalInit(): Failed to set zorder property for plane!\n");
				drmModeFreeProperty(prop);
				drmModeFreeObjectProperties(props);
				goto fail;
			}
		}
		drmModeFreeProperty(prop);
	}
	drmModeFreeObjectProperties(props);

	_width = _modeInfo.hdisplay;
	_height = _modeInfo.vdisplay;

	creq.height = _modeInfo.vdisplay;
	creq.width = _modeInfo.hdisplay;
	creq.bpp = 32;

	for (int i = 0; i < NUM_FB; i++) {
		if (drmIoctl(_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
			log->printf("DisplayDrm::internalInit(): Cannot create dumb buffer: %s\n", strerror(errno));
			return S_FAIL;
		}
		handles[0] = creq.handle;
		pitches[0] = creq.pitch;

		ret = drmModeAddFB2(_fd, _modeInfo.hdisplay, _modeInfo.vdisplay,
		                    DRM_FORMAT_ARGB8888,
		                    handles, pitches, offsets, &_frameBuffers[i].fbId, 0);
		if (ret < 0) {
			log->printf("DisplayDrm::internalInit(): failed add video buffer: %s\n", strerror(errno));
			goto fail;
		}

		mreq.handle = creq.handle;
		if (drmIoctl(_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
		    log->printf("DisplayDrm::internalInit(): Cannot map dumb buffer: %s\n", strerror(errno));
            goto fail;
		}
		_frameBuffers[i].ptr = mmap(nullptr, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, mreq.offset);
		if (_frameBuffers[i].ptr == MAP_FAILED) {
			log->printf("DisplayDrm::internalInit(): Cannot map dumb buffer: %s\n", strerror(errno));
			goto fail;
		}

		_frameBuffers[i].width = _modeInfo.hdisplay;
		_frameBuffers[i].height = _modeInfo.vdisplay;
		_frameBuffers[i].stride = creq.pitch;
		_frameBuffers[i].size = creq.size;

		memset(_frameBuffers[i].ptr, 0, _frameBuffers[i].size);
	}

	_oldCrtc = drmModeGetCrtc(_fd, _crtcId);
	ret = drmModeSetCrtc(_fd, _crtcId, _frameBuffers[0].fbId, 0, 0, &_connectorId, 1, &_modeInfo);
	if (ret < 0) {
		log->printf("DisplayDrm::internalInit(): failed set crtc: %s\n", strerror(errno));
		goto fail;
	}

	_flipEvent.version = DRM_EVENT_CONTEXT_VERSION;
	_flipEvent.page_flip_handler = &drm_page_flip;

	_currentBuffer = 1;
	_waitingForFlip = true;

	_initialized = true;
	return S_OK;

fail:

	internalDeinit ();

	return S_FAIL;
}

void DisplayDrm::internalDeinit() {
	if (_oldCrtc) {
		drmModeSetCrtc(_fd, _oldCrtc->crtc_id, _oldCrtc->buffer_id,
		               _oldCrtc->x, _oldCrtc->y, &_connectorId, 1, &_oldCrtc->mode);
		drmModeFreeCrtc(_oldCrtc);
		_oldCrtc = nullptr;
	}

	for (int i = 0; i < NUM_FB; i++) {
		if (_frameBuffers[i].fbId) {
			drmModeRmFB(_fd, _frameBuffers[i].fbId);
			_frameBuffers[i].fbId = 0;
		}
		if (_frameBuffers[i].ptr) {
			munmap(_frameBuffers[i].ptr, _frameBuffers[i].size);
			_frameBuffers[i].ptr = nullptr;
		}
		if (_frameBuffers[i].handle > 0) {
			struct drm_mode_destroy_dumb dreq = {
				.handle = _frameBuffers[i].handle,
			};
			drmIoctl(_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
			_frameBuffers[i].handle = 0;
		}
		_frameBuffers[i] = { 0 };
	}

	if (_drmPlaneResources != nullptr) {
		drmModeFreePlaneResources(_drmPlaneResources);
		_drmPlaneResources = nullptr;
	}

	if (_drmResources != nullptr) {
		drmModeFreeResources(_drmResources);
		_drmResources = nullptr;
	}

	if (_fd != -1) {
		drmClose(_fd);
		_fd = -1;
	}

	_initialized = false;
}


STATUS DisplayDrm::flip() {
	if (!_initialized)
		return S_FAIL;

	if (drmModePageFlip(_fd, _crtcId, _frameBuffers[_currentBuffer].fbId, DRM_MODE_PAGE_FLIP_EVENT, this) != 0) {
		log->printf("DisplayDrm::flip(): failed queue page flip: %s\n", strerror(errno));
		goto fail;
	}

	while (_waitingForFlip) {
		struct pollfd fds[1] = { { .fd = _fd, .events = POLLIN } };
		poll(fds, 1, 3000);
		if (fds[0].revents & POLLIN) {
			if (drmHandleEvent(_fd, &_flipEvent) != 0) {
				log->printf("DisplayDrm::flip(): failed handle drm event: %s\n", strerror(errno));
				goto fail;
			}
		}
	}

	if (++_currentBuffer >= NUM_FB)
		_currentBuffer = 0;

	_waitingForFlip = true;

	return S_OK;

fail:

	return S_FAIL;
}

void DisplayDrm::clear() {
	memset(_frameBuffers[_currentBuffer].ptr, 0, _frameBuffers[_currentBuffer].size);
}

} // namespace

#endif
