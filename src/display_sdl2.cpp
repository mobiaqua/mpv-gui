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

#if defined(BUILD_SDL2)

#include "display_sdl2.h"
#include "display_base.h"
#include "logs.h"

namespace MpvGui {

#define SCREEN_WIDTH  (1920)
#define SCREEN_HEIGHT (1080)

DisplaySdl2::DisplaySdl2() :
		_window(nullptr), _renderer(nullptr), _texture(nullptr), _backBuffer(nullptr) {
}

DisplaySdl2::~DisplaySdl2() {
	deinit();
}

STATUS DisplaySdl2::init() {
	if (_initialized)
		return S_FAIL;

	if (internalInit() == S_FAIL)
		return S_FAIL;

	return S_OK;
}

STATUS DisplaySdl2::deinit() {
	if (!_initialized)
		return S_FAIL;

	internalDeinit();

	return S_OK;
}

void *DisplaySdl2::getBufferPtr() {
	if (!_initialized)
		return nullptr;

	return _backBuffer;
}

U32 DisplaySdl2::getBufferWidth() {
	if (!_initialized)
		return 0;

	return _width;
}

U32 DisplaySdl2::getBufferHeight() {
	if (!_initialized)
		return 0;

	return _height;
}

U32 DisplaySdl2::getBufferStride() {
	if (!_initialized)
		return 0;

	return _stride;
}

STATUS DisplaySdl2::internalInit() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		log->printf("DisplaySdl2::internalInit(): Failed init SDL2, %d\n", SDL_GetError());
		goto fail;
	}

	_width = SCREEN_WIDTH;
	_height = SCREEN_HEIGHT;
	_stride = _width * 4;
	_backBuffer = malloc(_stride * _height);
	if (!_backBuffer) {
		log->printf("DisplaySdl2::internalInit(): Failed alloc back buffer\n");
		goto fail;
	}

	_window = SDL_CreateWindow("MPV GUI",
                                   SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED,
                                   _width / 2, _height / 2,
                                   SDL_WINDOW_SHOWN);
	if (!_window) {
		log->printf("DisplaySdl2::internalInit(): Failed init window, %d\n", SDL_GetError());
		goto fail;
	}

	_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
	if (!_renderer) {
		log->printf("DisplaySdl2::internalInit(): Failed init renderer, %d\n", SDL_GetError());
		goto fail;
	}

	_texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, _width, _height);
	if (!_texture) {
		log->printf("DisplaySdl2::internalInit(): Failed create buffer texture, %d\n", SDL_GetError());
		goto fail;
	}

	_initialized = true;
	return S_OK;

fail:

	internalDeinit ();

	return S_FAIL;
}

void DisplaySdl2::internalDeinit() {
	if (_backBuffer)
		free(_backBuffer);
	if (_texture)
		SDL_DestroyTexture(_texture);
	if (_renderer)
		SDL_DestroyRenderer(_renderer);
	if (_window)
		SDL_DestroyWindow(_window);

	SDL_Quit();

	_initialized = false;
}

STATUS DisplaySdl2::flip() {
	if (!_initialized)
		return S_FAIL;

	void *pixels;
	int pitch;
	if (SDL_LockTexture(_texture, nullptr, &pixels, &pitch) != 0) {
		goto fail;
	}
	for (int h = 0; h < _height; h++) {
		memcpy((U8 *)pixels + (pitch * h), (U8 *)_backBuffer + (_stride * h), MIN(pitch, _stride));
	}

	SDL_UnlockTexture(_texture);

	SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);

	SDL_RenderPresent(_renderer);

	SDL_Delay(16);

	return S_OK;

fail:

	return S_FAIL;
}

void DisplaySdl2::clear() {
	memset(_backBuffer, 0, _stride * _height);
}

} // namespace

#endif
