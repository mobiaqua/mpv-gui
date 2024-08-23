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

#ifndef DISPLAY_OMAPSDL2_H
#define DISPLAY_OMAPSDL2_H

#if defined(BUILD_SDL2)

#include <SDL.h>

#include "display_base.h"
#include "basetypes.h"
#include <stdint.h>

namespace MpvGui {

class DisplaySdl2 : public Display {
private:

	U32                     _width;
	U32                     _height;
	U32                     _stride;

	SDL_Window              *_window;
	SDL_Renderer            *_renderer;
	SDL_Texture             *_texture;
	void                    *_backBuffer;

public:

	DisplaySdl2();
	~DisplaySdl2();

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
