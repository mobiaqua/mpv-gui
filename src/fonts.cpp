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

#include "basetypes.h"
#include "logs.h"
#include "fonts.h"

#include <stdlib.h>
#include <stdio.h>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace MpvGui {

static FT_Library ft;
static FT_Face face;

bool FontsInit() {
	FcResult result;
	FcChar8 *fontFile;

	if (!FcInit()) {
		log->printf("Failed init font config!\n");
		return false;
	}

	auto pattern = FcNameParse((const FcChar8 *)"sans-serif");
	FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);
	auto matched = FcFontMatch(nullptr, pattern, &result);
	if (!matched) {
		log->printf("Failed match font!\n");
		goto fail;
	}

	if (FcPatternGetString(matched, FC_FILE, 0, &fontFile) != FcResultMatch) {
		log->printf("Failed get font!\n");
		goto fail;
	}

	if (FT_Init_FreeType(&ft)) {
		log->printf("Failed init freetype!\n");
		goto fail;
	}

	if (FT_New_Face(ft, (const char *)fontFile, 0, &face)) {
		log->printf("Failed load font!\n");
		goto fail;
	}

	FcPatternDestroy(matched);
	FcPatternDestroy(pattern);
	pattern = matched = nullptr;

	FcFini();

	return true;

fail:
	if (face) {
		FT_Done_Face(face);
		face = nullptr;
	}
	if (ft) {
		FT_Done_FreeType(ft);
		ft = nullptr;
	}
	if (matched) {
		FcPatternDestroy(matched);
		matched = nullptr;
	}
	if (pattern) {
		FcPatternDestroy(pattern);
		pattern = nullptr;
	}
	FcFini();
	return false;
}

void FontsDeinit() {
	if (face) {
		FT_Done_Face(face);
		face = nullptr;
	}
	if (ft) {
		FT_Done_FreeType(ft);
		ft = nullptr;
	}
}

static U32 utf8_decode(const char *str, int &i) {
	U32 c = (unsigned char)str[i];
	if (c >= 0xF0) {
		c = ((c & 0x07) << 18) |
		    ((unsigned char)str[i + 1] & 0x3F) << 12 |
		    ((unsigned char)str[i + 2] & 0x3F) << 6 |
		    ((unsigned char)str[i + 3] & 0x3F);
	} else if (c >= 0xE0) {
		c = ((c & 0x0F) << 12) |
		    ((unsigned char)str[i + 1] & 0x3F) << 6 |
		    ((unsigned char)str[i + 2] & 0x3F);
	} else if (c >= 0xC0) {
		c = ((c & 0x1F) << 6) |
		    ((unsigned char)str[i + 1] & 0x3F);
	}
	return c;
}

static int utf8_strlen(const char *s) {
	int length = 0;

	while (*s) {
		if ((*s & 0xc0) != 0x80) {
			length++;
		}
		s++;
	}
	return length;
}

void FontsSetSize(int size) {
	FT_Set_Pixel_Sizes(face, 0, size);
}

void FontsRenderText(const char *text, U8 *buffer, U32 pos_x, U32 pos_y, U32 stride, U8 r, U8 g, U8 b) {
	int len = utf8_strlen(text);
	U32 orgPosY = pos_y;
	stride /= 4;

	for (int i = 0; i < len; i++) {
		if (FT_Load_Char(face, utf8_decode(text, i), FT_LOAD_RENDER)) {
			continue;
		}

		U32 *dst = (U32 *)buffer;
		FT_GlyphSlot f = face->glyph;
		pos_y = orgPosY - f->bitmap_top;
		for (int y = 0; y < f->bitmap.rows; y++) {
			for (int x = 0; x < f->bitmap.width; x++) {
				int offset = (pos_y + y) * stride + (pos_x + x);
				unsigned char value = f->bitmap.buffer[y * f->bitmap.width + x];
				if (value)
					dst[offset] = value << 24 | r << 16 | g << 8 | b << 0;
			}
		}

		pos_x += f->advance.x >> 6;
	}
}

} // namespace
