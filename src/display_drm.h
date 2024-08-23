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

#ifndef DISPLAY_OMAPDRM_H
#define DISPLAY_OMAPDRM_H

#if !defined(BUILD_SDL2)

#include "display_base.h"
#include "basetypes.h"
#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

namespace MpvGui {

#define NUM_FB   2

class DisplayDrm : public Display {
private:

	typedef struct {
		uint32_t        handle;
		uint32_t        fbId;
		void            *ptr;
		U32             stride;
		U32             width;
		U32             height;
		U32             size;
	} FrameBuffer;

	int                         _fd;
	drmModeResPtr               _drmResources;
	drmModePlaneResPtr          _drmPlaneResources;
	drmModeCrtcPtr              _oldCrtc;
	drmModeModeInfo             _modeInfo;
	drmEventContext             _flipEvent{};
	uint32_t                    _connectorId;
	uint32_t                    _crtcId;
	int                         _planeId;

	U32                         _width;
	U32                         _height;

	FrameBuffer                 _frameBuffers[NUM_FB]{};

	int                         _currentBuffer;

public:

	bool                        _waitingForFlip;

	DisplayDrm();
	~DisplayDrm();

	STATUS init();
	STATUS deinit();
	void *getBufferPtr();
	U32 getBufferWidth();
	U32 getBufferHeight();
	U32 getBufferStride();
	STATUS flip();
	void clear();

private:

	STATUS internalInit();
	void internalDeinit();
};

} // namespace

#endif

#endif
