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

#include <unistd.h>
#include <cstring>
#include <algorithm>

#include "basetypes.h"
#include "logs.h"
#include "display_base.h"
#include "fonts.h"
#include "remote.h"
#include "fs.h"

namespace MpvGui {

int GuiRun(int argc, char *argv[]) {
	int option;
	const char *dirName;
	Display *display = nullptr;
	std::string lastPath;
	int lastSelection = 0;
	int scale = 1;
	std::vector<Fs::FsEntry> entries;
	bool guiUpdate = true;
	int selection = 0;
	int parentSelection = 0;
	int offset = 0;
	int parentOffset = 0;

	if (CreateLogs() == S_FAIL) {
		return -1;
	}

	while ((option = getopt(argc, argv, ":")) != -1) {
		switch (option) {
		default:
			break;
		}
	}

	if (optind < argc) {
	    dirName = argv[optind];
	} else {
		log->printf("Missing root directory parameter!\n");
		delete log;
		return -1;
	}

	Fs fileSystem(dirName);
	fileSystem.AddMediaExtension(".mkv");
	fileSystem.AddMediaExtension(".avi");
	fileSystem.AddMediaExtension(".mp4");
	fileSystem.AddMediaExtension(".mpg");
	fileSystem.AddMediaExtension(".mpeg");
	fileSystem.AddMediaExtension(".mov");
	fileSystem.AddMediaExtension(".flv");

#if defined(BUILD_SDL2)
	display = CreateDisplay(DISPLAY_SDL2);
#else
	display = CreateDisplay(DISPLAY_DRM);
#endif
	if (display == nullptr) {
		log->printf("Failed create display!\n");
		goto end;
	}
	if (display->init() == S_FAIL) {
		log->printf("Failed init display!\n");
		goto end;
	}

	if (RemoteInit() != 0) {
		log->printf("Failed init remote controller!\n");
		goto end;
	}

	if (!FontsInit()) {
		log->printf("Failed init fonts!\n");
		goto end;
	}

	if (display->getBufferWidth() > 1920)
		scale = 2;

	if (!lastPath.empty())
		fileSystem.EnterDirectory(lastPath);

	selection = lastSelection;
	fileSystem.GetMediaEntries(entries);
	if (entries.size() == 0) {
		parentOffset = parentSelection = selection = lastSelection = -1;
	}
	do {
		int inputKey = RemoteRead();
		switch (inputKey) {
		case 'p':
		case 'r':
		case 'e': {
			if (selection < 0)
				break;
			auto &entry = entries[selection];
			if (entry.type == Fs::FsEntryType::FsDirectory && (inputKey == 'e' || inputKey == 'r')) {
				if (fileSystem.EnterDirectory(entry.name)) {
					fileSystem.GetMediaEntries(entries);
					parentSelection = selection;
					parentOffset = offset;
					offset = selection = 0;
				}
				guiUpdate = true;
				break;
			}
			if (entry.type == Fs::FsEntryType::FsFile && (inputKey == 'e' || inputKey == 'p')) {
				display->deinit();
				RemoteClose();
				std::string command = "mpv \"";
				command += (char *)(fs::path(fileSystem.CurrentPath() + "/" + entry.name + "\"").c_str());
				system(command.c_str());
				display->init();
				RemoteInit();
			}
			guiUpdate = true;
			break;
		}
		case 'l': {
			if (selection < 0) {
				fileSystem.GetMediaEntries(entries);
				if (entries.size() == 0) {
					guiUpdate = true;
					break;
				}
				offset = parentSelection = selection = 0;
			}
			if (fileSystem.ExitDirectory()) {
				fileSystem.GetMediaEntries(entries);
				selection = parentSelection;
				offset = parentOffset;
				parentOffset = parentSelection = 0;
			}
			guiUpdate = true;
			break;
		}
		case 'u': {
			if (selection < 0) {
				guiUpdate = true;
				break;
			}
			selection--;
			if (selection < 0) {
				selection = entries.size() - 1;
				if (entries.size() > 30) {
					offset = entries.size() - 30;
				}
			} else {
				if (entries.size() > 30) {
					if (entries.size() - selection > 15) {
						offset = selection - 15;
						if (offset < 0)
							offset = 0;
					}
				}
			}
			guiUpdate = true;
			break;
		}
		case 'd': {
			if (selection < 0) {
				guiUpdate = true;
				break;
			}
			selection++;
			if (selection >= entries.size()) {
				selection = 0;
				offset = 0;
			} else {
				if (entries.size() > 30) {
					if (entries.size() - offset > 30)
						offset = selection - 15;
					if (offset < 0)
						offset = 0;
				}
			}
			guiUpdate = true;
			break;
		}
		case -1:
		default:
			break;
		}

		if (!guiUpdate) {
			usleep(10000);
			continue;
		}

		display->clear();

		FontsSetSize(50 * scale);
		std::string title = "--== Media Player ==--";
		FontsRenderText(title.c_str(),
		                (U8 *)display->getBufferPtr(),
		                80 * scale,
		                80 * scale,
		                display->getBufferStride(),
		                0, 255, 0);

		FontsSetSize(30 * scale);
		std::string pathStr = "* ";
		pathStr += fileSystem.CurrentPath() + "/ *";
		FontsRenderText(pathStr.c_str(),
		                (U8 *)display->getBufferPtr(),
		                700 * scale,
		                80 * scale,
		                display->getBufferStride(),
		                255, 255, 0);

		if (offset > 0) {
			FontsRenderText("^^^",
			                (U8 *)display->getBufferPtr(),
			                80 * scale,
			                120 * scale,
			                display->getBufferStride(),
			               255, 0, 0);
		}

		int num = entries.size();
		if (num > 30)
			num = 30;
		for (int index = offset, drawIndex = 0; index < (offset + num); index++, drawIndex++) {
			auto &entry = entries[index];
			if (entry.type == Fs::FsEntryType::FsDirectory) {
				pathStr = std::string("[ ") + entry.name + " ]";
			} else {
				pathStr = fs::path(entry.name).stem();
			}
			if (selection == index)
				pathStr += " <---";
			FontsRenderText(pathStr.c_str(),
			                (U8 *)display->getBufferPtr(),
			                80 * scale,
			                150 * scale + (30 * scale * drawIndex),
			                display->getBufferStride(),
			                selection == index ? 0 : 255, 255, 255);
		}

		if (entries.size() > 30 && (entries.size() - offset) > 30) {
			FontsRenderText("v v v",
			                (U8 *)display->getBufferPtr(),
			                80 * scale,
			                150 * scale + (30 * scale * 30),
			                display->getBufferStride(),
			                255, 0, 0);
		}

		display->flip();
		guiUpdate = false;
	} while (true);

end:
	FontsDeinit();
	RemoteClose();
	delete display;
	delete log;

	return 0;
}

} // namespace

int main(int argc, char *argv[]) {
	return MpvGui::GuiRun(argc, argv);
}
