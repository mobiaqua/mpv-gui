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

#ifndef DISPLAY_BASE_H
#define DISPLAY_BASE_H

#include "basetypes.h"

typedef enum _DISPLAY_TYPE {
    DISPLAY_NONE,
    DISPLAY_DRM,
    DISPLAY_SDL2,
} DISPLAY_TYPE;

namespace MpvGui {

class Display {
protected:

	bool           _initialized;

public:

	Display();
	virtual ~Display() {}

	virtual STATUS init() = 0;
	virtual STATUS deinit() = 0;
	virtual void *getBufferPtr() = 0;
	virtual U32 getBufferWidth() = 0;
	virtual U32 getBufferHeight() = 0;
	virtual U32 getBufferStride() = 0;
	virtual STATUS flip() = 0;
	virtual void clear() = 0;
};

Display *CreateDisplay(DISPLAY_TYPE displayType);

} // namespace

#endif
