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
#include "fs.h"

#include <algorithm>
#include <regex>

namespace MpvGui {

static size_t CurlWriteFunction(void *ptr, size_t size, size_t nmemb,
                                std::string *userdata) {
	size_t totalSize = size * nmemb;
	userdata->append((char *)ptr, totalSize);
	return totalSize;
}

Fs::Fs(std::string path) {
	if (path.compare(0, 4, "http") == 0) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteFunction);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &curlBuffer);
		if (path.back() == '/')
			path.pop_back();
		currentPath = rootPath = path;
	} else {
		currentPath = fs::canonical(fs::path(path));
		rootPath = currentPath;
	}
}

Fs::~Fs() {
	if (curl) {
		curl_easy_cleanup(curl);
		curl_global_cleanup();
	}
}

void Fs::GetMediaEntries(std::vector<FsEntry> &entries) {
	entries.clear();
	std::vector<std::string> dirs;
	std::vector<std::string> files;

	if (curl) {
		std::string url = currentPath + "/";
		std::regex space("[[:space:]]");
		url = std::regex_replace(url, space, "%20");
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curlBuffer.clear();
		CURLcode result = curl_easy_perform(curl);
		if (result != CURLE_OK) {
			return;
		}

		std::regex urlRegex(R"(<a\s+href=\"([^\"]+)\">([^<]+)<\/a>)");
		std::smatch match{};
		std::string::const_iterator searchStart(curlBuffer.cbegin());

		while (std::regex_search(searchStart, curlBuffer.cend(), match, urlRegex)) {
			std::string href = match[1].str();
			std::string name = match[2].str();
			if (href.compare(0, 1, "/") != 0 && href.compare(0, 1, "?") != 0) {
				if (href.back() == '/') {
					if (name.back() == '/')
						name.pop_back();
					dirs.push_back(name);
					searchStart = match.suffix().first;
				} else {
					if (std::count(mediaExtensions.begin(), mediaExtensions.end(), fs::path(href).extension()) != 0) {
						files.push_back(name);
					}
				}
			}
			searchStart = match.suffix().first;
		}
	} else {
		try {
			for (const auto &it : fs::directory_iterator(currentPath, fs::directory_options::skip_permission_denied)) {
				if (!it.is_directory() && !it.is_regular_file())
					continue;
				if (it.is_directory()) {
					dirs.push_back(it.path().filename());
					continue;
				}
				if (std::count(mediaExtensions.begin(), mediaExtensions.end(), it.path().extension()) == 0)
					continue;
				if (it.path().stem().u8string().compare(0, 2, "._") == 0)
					continue;
				files.push_back(it.path().filename());
			}
		} catch (const fs::filesystem_error &e) {}
	}
	std::sort(dirs.begin(), dirs.end());
	std::sort(files.begin(), files.end());
	Fs::FsEntry entry;
	for (const auto &it : dirs) {
		entry.type = FsEntryType::FsDirectory;
		entry.name = it;
		entries.push_back(entry);
	}
	for (const auto &it : files) {
		entry.type = FsEntryType::FsFile;
		entry.name = it;
		entries.push_back(entry);
	}
}

bool Fs::EnterDirectory(std::string name) {
	std::string newPath = currentPath + "/" + name;
	if (!curl) {
		if (fs::path(newPath).has_root_path()) {
			if (newPath.find(rootPath) == std::string::npos) {
				return false;
			}
		}
		if (!fs::exists(newPath))
			return false;
		if (!fs::is_directory(newPath))
			return false;
	}
	currentPath = newPath;
	return true;
}

bool Fs::ExitDirectory() {
	if (currentPath == rootPath)
		return false;
	currentPath = fs::path(currentPath).parent_path();
	return true;
}

} // namespace
