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

#ifndef FS_H
#define FS_H

#include <string>
#include <vector>
#include <iostream>
#include <filesystem>

#include <curl/curl.h>

namespace fs = std::filesystem;

namespace MpvGui {

class Fs {
public:
	enum FsEntryType {
		FsFile,
		FsDirectory
	};
	struct FsEntry {
		FsEntryType type;
		std::string name;
	};

private:
	std::string rootPath;
	std::string currentPath;
	std::vector<std::string> mediaExtensions;
	CURL *curl{};
	std::string curlBuffer;

public:

	Fs() = default;
	Fs(std::string path);
	~Fs();
	std::string CurrentPath() { return currentPath; }
	void AddMediaExtension(std::string ext) { mediaExtensions.push_back(ext); }
	void GetMediaEntries(std::vector<FsEntry> &entries);
	bool EnterDirectory(std::string name);
	bool ExitDirectory();
};

} // namespace

#endif
