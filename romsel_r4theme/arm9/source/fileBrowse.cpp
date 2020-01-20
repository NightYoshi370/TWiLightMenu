/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/

#include "fileBrowse.h"
#include <algorithm>
#include <dirent.h>
#include <math.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <vector>

#include <nds.h>
#include "common/gl2d.h"

#include "date.h"

#include "ndsheaderbanner.h"
#include "flashcard.h"
#include "iconTitle.h"
#include "graphics/fontHandler.h"
#include "graphics/graphics.h"
#include "graphics/FontGraphic.h"
#include "graphics/TextPane.h"
#include "SwitchState.h"
#include "perGameSettings.h"
#include "errorScreen.h"

#include "gbaswitch.h"
#include "nds_loader_arm9.h"

#include "common/inifile.h"

#include "fileCopy.h"

#define SCREEN_COLS 32
#define ENTRIES_PER_SCREEN 22
#define ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10

extern bool whiteScreen;
extern bool fadeType;
extern bool fadeSpeed;

extern bool useBootstrap;
extern bool homebrewBootstrap;
extern bool useGbarunner;
extern int consoleModel;
extern bool isRegularDS;

extern bool showdialogbox;
extern int dialogboxHeight;

extern std::string romfolder[2];

extern std::string arm7DonorPath;
bool donorFound = true;

extern bool applaunch;

extern bool gotosettings;

using namespace std;

extern bool startMenu;

extern int theme;

extern bool showDirectories;
extern bool showHidden;
extern int spawnedtitleboxes;
extern int cursorPosition[2];
extern int startMenu_cursorPosition;
extern int pagenum[2];

bool settingsChanged = false;

extern void SaveSettings();

char path[PATH_MAX] = {0};

struct DirEntry {
	string name;
	bool isDirectory;
};

bool nameEndsWith (const string& name, const vector<string> extensionList) {

	if (name.substr(0,2) == "._")	return false;	// Don't show macOS's index files

	if (name.size() == 0) return false;

	if (extensionList.size() == 0) return true;

	for (int i = 0; i < (int)extensionList.size(); i++) {
		const string ext = extensionList.at(i);
		if ( strcasecmp (name.c_str() + name.size() - ext.size(), ext.c_str()) == 0) return true;
	}
	return false;
}

bool dirEntryPredicate(const DirEntry& lhs, const DirEntry& rhs)
{

	if (!lhs.isDirectory && rhs.isDirectory) {
		return false;
	}
	if (lhs.isDirectory && !rhs.isDirectory) {
		return true;
	}
	return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
}

void getDirectoryContents(vector<DirEntry>& dirContents, const vector<string> extensionList)
{
	dirContents.clear();

	struct stat st;
	DIR *pdir = opendir ("."); 
	
	if (pdir == NULL) {
		iprintf ("Unable to open the directory.\n");
	} else {
		while(true) {
			DirEntry dirEntry;

			struct dirent* pent = readdir(pdir);
			if(pent == NULL) break;
				
			stat(pent->d_name, &st);
			dirEntry.name = pent->d_name;
			dirEntry.isDirectory = (st.st_mode & S_IFDIR) ? true : false;

			if(showHidden || strncmp(dirEntry.name.c_str(), ".", 1) != 0 || dirEntry.name == "..") {
				if (showDirectories) {
					if (dirEntry.name.compare(".") && dirEntry.name.compare("_nds") && dirEntry.name.compare("saves") && (dirEntry.isDirectory || nameEndsWith(dirEntry.name, extensionList))) {
						dirContents.push_back (dirEntry);
					}
				} else {
					if (dirEntry.name.compare(".") != 0 && (nameEndsWith(dirEntry.name, extensionList))) {
						dirContents.push_back (dirEntry);
					}
				}
			}
		}

		closedir(pdir);
	}

	sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);
}

void getDirectoryContents(vector<DirEntry>& dirContents)
{
	vector<string> extensionList;
	getDirectoryContents(dirContents, extensionList);
}

void showDirectoryContents (const vector<DirEntry>& dirContents, int startRow) {
	char path[PATH_MAX];
	
	
	getcwd(path, PATH_MAX);
	
	// Clear the screen
	iprintf ("\x1b[2J");
	
	// Print the path
	if (strlen(path) < SCREEN_COLS) {
		iprintf ("%s", path);
	} else {
		iprintf ("%s", path + strlen(path) - SCREEN_COLS);
	}
	
	// Move to 2nd row
	iprintf ("\x1b[1;0H");
	// Print line of dashes
	iprintf ("--------------------------------");
	
	// Print directory listing
	for (int i = 0; i < ((int)dirContents.size() - startRow) && i < ENTRIES_PER_SCREEN; i++) {
		const DirEntry* entry = &dirContents.at(i + startRow);
		char entryName[SCREEN_COLS + 1];
		
		// Set row
		iprintf ("\x1b[%d;0H", i + ENTRIES_START_ROW);
		
		if (entry->isDirectory) {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 3] = '\0';
			iprintf (" [%s]", entryName);
		} else {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 1] = '\0';
			iprintf (" %s", entryName);
		}
	}
}

void smsWarning(void) {
	dialogboxHeight = 3;
	showdialogbox = true;
	printLargeCentered(false, 84, "Warning");
	printSmallCentered(false, 104, "When the game starts, please");
	printSmallCentered(false, 112, "touch the screen to go into");
	printSmallCentered(false, 120, "the menu, and exit out of it");
	printSmallCentered(false, 128, "for the sound to work.");
	printSmallCentered(false, 142, "A: OK");
	int pressed = 0;
	do {
		scanKeys();
		pressed = keysDown();
		checkSdEject();
		swiWaitForVBlank();
	} while (!(pressed & KEY_A));
	clearText();
	showdialogbox = false;
	dialogboxHeight = 0;
}

void mdRomTooBig(void) {
	dialogboxHeight = 3;
	showdialogbox = true;
	printLargeCentered(false, 84, "Error!");
	printSmallCentered(false, 104, "This SEGA Genesis/Mega Drive");
	printSmallCentered(false, 112, "ROM cannot be launched,");
	printSmallCentered(false, 120, "due to it surpassing the");
	printSmallCentered(false, 128, "size limit of 3 MB.");
	printSmallCentered(false, 142, "A: OK");
	int pressed = 0;
	do {
		scanKeys();
		pressed = keysDown();
		checkSdEject();
		swiWaitForVBlank();
	} while (!(pressed & KEY_A));
	clearText();
	showdialogbox = false;
	dialogboxHeight = 0;
}

void ramDiskMsg(void) {
	dialogboxHeight = 1;
	showdialogbox = true;
	printLargeCentered(false, 84, "Error!");
	printSmallCentered(false, 104, "This app requires a");
	printSmallCentered(false, 112, "RAM disk to work.");
	printSmallCentered(false, 126, "A: OK");
	int pressed = 0;
	do {
		scanKeys();
		pressed = keysDown();
		checkSdEject();
		swiWaitForVBlank();
	} while (!(pressed & KEY_A));
	clearText();
	showdialogbox = false;
	dialogboxHeight = 0;
}

void dsiBinariesMissingMsg(void) {
	dialogboxHeight = 1;
	showdialogbox = true;
	printLargeCentered(false, 84, "Error!");
	printSmallCentered(false, 104, "DSi binaries are missing.");
	printSmallCentered(false, 112, "Please start in DS mode.");
	printSmallCentered(false, 126, "A: OK");
	int pressed = 0;
	do {
		scanKeys();
		pressed = keysDown();
		checkSdEject();
		swiWaitForVBlank();
	} while (!(pressed & KEY_A));
	clearText();
	showdialogbox = false;
	dialogboxHeight = 0;
}

extern bool extention(const std::string& filename, const char* ext);

string browseForFile(const vector<string> extensionList) {
	int pressed = 0;
	int screenOffset = 0;
	int fileOffset = 0;
	vector<DirEntry> dirContents;
	
	getDirectoryContents (dirContents, extensionList);
	showDirectoryContents (dirContents, screenOffset);
	
	whiteScreen = false;
	fadeType = true;	// Fade in from white

	fileOffset = cursorPosition[secondaryDevice];
	if (pagenum[secondaryDevice] > 0) {
		fileOffset += pagenum[secondaryDevice]*40;
	}

	while (true)
	{
		if (fileOffset < 0) 	fileOffset = dirContents.size() - 1;		// Wrap around to bottom of list
		if (fileOffset > ((int)dirContents.size() - 1))		fileOffset = 0;		// Wrap around to top of list

		// Clear old cursors
		for (int i = ENTRIES_START_ROW; i < ENTRIES_PER_SCREEN + ENTRIES_START_ROW; i++) {
			iprintf ("\x1b[%d;0H ", i);
		}
		// Show cursor
		iprintf ("\x1b[%d;0H*", fileOffset - screenOffset + ENTRIES_START_ROW);

		if (dirContents.at(fileOffset).isDirectory) {
			isDirectory = true;
			bnrWirelessIcon = 0;
		} else {
			isDirectory = false;
			std::string std_romsel_filename = dirContents.at(fileOffset).name.c_str();

			if (extention(std_romsel_filename, ".nds")
			 || extention(std_romsel_filename, ".dsi")
			 || extention(std_romsel_filename, ".ids")
			 || extention(std_romsel_filename, ".srl")
			 || extention(std_romsel_filename, ".app")
			 || extention(std_romsel_filename, ".argv"))
			{
				getGameInfo(isDirectory, dirContents.at(fileOffset).name.c_str());
				bnrRomType = 0;
			} else if (extention(std_romsel_filename, ".plg") || extention(std_romsel_filename, ".rvid")) {
				bnrRomType = 9;
			} else if (extention(std_romsel_filename, ".gba")) {
				bnrRomType = 1;
			} else if (extention(std_romsel_filename, ".gb") || extention(std_romsel_filename, ".sgb")) {
				bnrRomType = 2;
			} else if (extention(std_romsel_filename, ".gbc")) {
				bnrRomType = 3;
			} else if (extention(std_romsel_filename, ".nes") || extention(std_romsel_filename, ".fds")) {
				bnrRomType = 4;
			} else if(extention(std_romsel_filename, ".sms")) {
				bnrRomType = 5;
			} else if(extention(std_romsel_filename, ".gg")) {
				bnrRomType = 6;
			} else if(extention(std_romsel_filename, ".gen")) {
				bnrRomType = 7;
			} else if(extention(std_romsel_filename, ".smc") || extention(std_romsel_filename, ".sfc")) {
				bnrRomType = 8;
			}
		}

		if (bnrRomType > 0 && bnrRomType < 10) {
			bnrWirelessIcon = 0;
			isDSiWare = false;
			isHomebrew = 0;
		}

		if (bnrRomType == 0) iconUpdate (dirContents.at(fileOffset).isDirectory,dirContents.at(fileOffset).name.c_str());
		titleUpdate (dirContents.at(fileOffset).isDirectory,dirContents.at(fileOffset).name.c_str());

		if (!isRegularDS) {
			printSmall(false, 8, 168, "Location:");
			if (secondaryDevice) {
				printSmall(false, 8, 176, "Slot-1 microSD Card");
			} else if (consoleModel < 3) {
				printSmall(false, 8, 176, "SD Card");
			} else {
				printSmall(false, 8, 176, "microSD Card");
			}
		}

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			scanKeys();
			pressed = keysDownRepeat();
			checkSdEject();
			swiWaitForVBlank();
		} while (!pressed);

		if (pressed & KEY_UP) {
			fileOffset -= 1;
			settingsChanged = true;
		}
		if (pressed & KEY_DOWN) {
			fileOffset += 1;
			settingsChanged = true;
		}
		if (pressed & KEY_LEFT) {
			fileOffset -= ENTRY_PAGE_LENGTH;
			settingsChanged = true;
		}
		if (pressed & KEY_RIGHT) {
			fileOffset += ENTRY_PAGE_LENGTH;
			settingsChanged = true;
		}

		if (fileOffset < 0) 	fileOffset = dirContents.size() - 1;		// Wrap around to bottom of list
		if (fileOffset > ((int)dirContents.size() - 1))		fileOffset = 0;		// Wrap around to top of list

		// Scroll screen if needed
		if (fileOffset < screenOffset) 	{
			screenOffset = fileOffset;
			showDirectoryContents (dirContents, screenOffset);
		}
		if (fileOffset > screenOffset + ENTRIES_PER_SCREEN - 1) {
			screenOffset = fileOffset - ENTRIES_PER_SCREEN + 1;
			showDirectoryContents (dirContents, screenOffset);
		}

		if (pressed & KEY_A) {
			DirEntry* entry = &dirContents.at(fileOffset);
			if (entry->isDirectory) {
				iprintf("Entering directory\n");
				// Enter selected directory
				chdir (entry->name.c_str());
				char buf[256];
				romfolder[secondaryDevice] = getcwd(buf, 256);
				cursorPosition[secondaryDevice] = 0;
				pagenum[secondaryDevice] = 0;
				SaveSettings();
				settingsChanged = false;
				return "null";
			}
			else if ((isDSiWare && !isDSiMode())
			      || (isDSiWare && !sdFound()))
			{
				showdialogbox = true;
				printLargeCentered(false, 84, "Error!");
				printSmallCentered(false, 104, "This game cannot be launched.");
				printSmallCentered(false, 118, "A: OK");
				pressed = 0;
				do {
					scanKeys();
					pressed = keysDown();
					checkSdEject();
					swiWaitForVBlank();
				} while (!(pressed & KEY_A));
				showdialogbox = false;
				for (int i = 0; i < 25; i++) swiWaitForVBlank();
			} else {
				int hasAP = 0;
				bool proceedToLaunch = true;
				bool useBootstrapAnyway = (useBootstrap || !secondaryDevice);
				if (useBootstrapAnyway && bnrRomType == 0 && !isDSiWare && isHomebrew == 0
				&& checkIfShowAPMsg(dirContents.at(fileOffset).name))
				{
					FILE *f_nds_file = fopen(dirContents.at(fileOffset).name.c_str(), "rb");
					hasAP = checkRomAP(f_nds_file);
					fclose(f_nds_file);
				} else if (isHomebrew == 1) {
					loadPerGameSettings(dirContents.at(fileOffset).name);
					if (requiresRamDisk && perGameSettings_ramDiskNo == -1) {
						proceedToLaunch = false;
						ramDiskMsg();
					}
				} else if (bnrRomType == 5 || bnrRomType == 6) {
					smsWarning();
					titleUpdate (dirContents.at(fileOffset).isDirectory,dirContents.at(fileOffset).name.c_str());
					if (!isRegularDS) {
						printSmall(false, 8, 168, "Location:");
						if (secondaryDevice) {
							printSmall(false, 8, 176, "Slot-1 microSD Card");
						} else if (consoleModel < 3) {
							printSmall(false, 8, 176, "SD Card");
						} else {
							printSmall(false, 8, 176, "microSD Card");
						}
					}
				}
				else if (bnrRomType == 7)
				{
					if (getFileSize(dirContents.at(fileOffset).name.c_str()) > 0x300000) {
						proceedToLaunch = false;
						mdRomTooBig();
					}
				}

				if (hasAP > 0) {
					dialogboxHeight = 3;
					showdialogbox = true;
					printLargeCentered(false, 84, "Anti-Piracy Warning");
					if (hasAP == 2) {
						printSmallCentered(false, 104, "This game has AP, and MUST");
						printSmallCentered(false, 112, "be patched using the RGF");
						printSmallCentered(false, 120, "TWiLight Menu AP patcher.");
					} else {
						printSmallCentered(false, 104, "This game has AP. Please");
						printSmallCentered(false, 112, "make sure you're using the");
						printSmallCentered(false, 120, "latest TWiLight Menu++.");
					}
					printSmallCentered(false, 142, "B: Return   A: Launch");

					pressed = 0;
					while (1) {
						scanKeys();
						pressed = keysDown();
						checkSdEject();
						swiWaitForVBlank();
						if (pressed & KEY_A) {
							pressed = 0;
							break;
						}
						if (pressed & KEY_B) {
							proceedToLaunch = false;
							pressed = 0;
							break;
						}
						if (pressed & KEY_X) {
							dontShowAPMsgAgain(dirContents.at(fileOffset).name);
							pressed = 0;
							break;
						}
					}
					clearText();
					showdialogbox = false;
					dialogboxHeight = 0;

					if (proceedToLaunch) {
						titleUpdate (dirContents.at(fileOffset).isDirectory,dirContents.at(fileOffset).name.c_str());
						if (!isRegularDS) {
							printSmall(false, 8, 168, "Location:");
							if (secondaryDevice) {
								printSmall(false, 8, 176, "Slot-1 microSD Card");
							} else if (consoleModel < 3) {
								printSmall(false, 8, 176, "SD Card");
							} else {
								printSmall(false, 8, 176, "microSD Card");
							}
						}
					}
				}

				if (proceedToLaunch && useBootstrapAnyway && bnrRomType == 0 && !isDSiWare &&
					isHomebrew == 0 &&
					checkIfDSiMode(dirContents.at(fileOffset).name)) {
					bool hasDsiBinaries = true;
					FILE *f_nds_file = fopen(dirContents.at(fileOffset).name.c_str(), "rb");
					hasDsiBinaries = checkDsiBinaries(f_nds_file);
					fclose(f_nds_file);

					if (!hasDsiBinaries) {
						dsiBinariesMissingMsg();
						proceedToLaunch = false;
					}
				}

				if (proceedToLaunch) {
					applaunch = true;

					fadeType = false;	// Fade to white
					for (int i = 0; i < 25; i++) {
						swiWaitForVBlank();
					}
					cursorPosition[secondaryDevice] = fileOffset;
					pagenum[secondaryDevice] = 0;
					for (int i = 0; i < 100; i++) {
						if (cursorPosition[secondaryDevice] > 39) {
							cursorPosition[secondaryDevice] -= 40;
							pagenum[secondaryDevice]++;
						} else {
							break;
						}
					}

					// Return the chosen file
					return entry->name;
				} else {
					for (int i = 0; i < 25; i++) swiWaitForVBlank();
				}
			}
		}

		if ((pressed & KEY_R) && bothSDandFlashcard()) {
			consoleClear();
			printf("Please wait...\n");
			cursorPosition[secondaryDevice] = fileOffset;
			pagenum[secondaryDevice] = 0;
			for (int i = 0; i < 100; i++) {
				if (cursorPosition[secondaryDevice] > 39) {
					cursorPosition[secondaryDevice] -= 40;
					pagenum[secondaryDevice]++;
				} else {
					break;
				}
			}
			secondaryDevice = !secondaryDevice;
			SaveSettings();
			settingsChanged = false;
			return "null";		
		}

		if ((pressed & KEY_B) && showDirectories) {
			// Go up a directory
			chdir ("..");
			char buf[256];
			romfolder[secondaryDevice] = getcwd(buf, 256);
			cursorPosition[secondaryDevice] = 0;
			SaveSettings();
			settingsChanged = false;
			return "null";
		}

		if ((pressed & KEY_X) && strcmp(dirContents.at(fileOffset).name.c_str(), ".."))
		{
			DirEntry *entry = &dirContents.at(fileOffset);
			bool unHide = strncmp(entry->name.c_str(), ".", 1) == 0;

			showdialogbox = true;
			dialogboxHeight = 3;

			if (isDirectory) {
				printLargeCentered(false, 84, "Folder Management options");
				printSmallCentered(false, 104, "What would you like");
				printSmallCentered(false, 112, "to do with this folder?");
			} else {
				printLargeCentered(false, 84, "ROM Management options");
				printSmallCentered(false, 104, "What would you like");
				printSmallCentered(false, 112, "to do with this ROM?");
			}

			for (int i = 0; i < 90; i++) swiWaitForVBlank();

			if (isDirectory) {
				if(unHide)	printSmallCentered(false, 142, "Y: Unhide  B: Nothing");
				else		printSmallCentered(false, 142, "Y: Hide    B: Nothing");
			} else {
				if(unHide)	printSmallCentered(false, 142, "Y: Unhide  A: Delete  B: Nothing");
				else		printSmallCentered(false, 142, "Y: Hide   A: Delete   B: Nothing");
			}

			while (1) {
				do {
					scanKeys();
					pressed = keysDown();
					checkSdEject();
					swiWaitForVBlank();
				} while (!pressed);
				
				if ((pressed & KEY_A && !isDirectory) || (pressed & KEY_Y)) {
					clearText();
					showdialogbox = false;
					consoleClear();
					printf("Please wait...\n");
					
					if (pressed & KEY_A && !isDirectory) {
						remove(dirContents.at(fileOffset).name.c_str());
					} else if (pressed & KEY_Y) {
						if (unHide) {
							rename(entry->name.c_str(), entry->name.substr(1).c_str());
						} else {
							rename(entry->name.c_str(), ("."+entry->name).c_str());
						}
					}
					
					if (settingsChanged) {
						cursorPosition[secondaryDevice] = fileOffset;
						pagenum[secondaryDevice] = 0;
						for (int i = 0; i < 100; i++) {
							if (cursorPosition[secondaryDevice] > 39) {
								cursorPosition[secondaryDevice] -= 40;
								pagenum[secondaryDevice]++;
							} else {
								break;
							}
						}
					}
					SaveSettings();
					settingsChanged = false;
					return "null";
				}

				if (pressed & KEY_B) {
					break;
				}
			}
			clearText();
			showdialogbox = false;
		}

		if (pressed & KEY_START)
		{
			if (settingsChanged) {
				cursorPosition[secondaryDevice] = fileOffset;
				pagenum[secondaryDevice] = 0;
				for (int i = 0; i < 100; i++) {
					if (cursorPosition[secondaryDevice] > 39) {
						cursorPosition[secondaryDevice] -= 40;
						pagenum[secondaryDevice]++;
					} else {
						break;
					}
				}
			}
			SaveSettings();
			settingsChanged = false;
			consoleClear();
			clearText();
			startMenu = true;
			return "null";		
		}

		if ((pressed & KEY_Y) && (isDirectory == false) && (bnrRomType == 0))
		{
			cursorPosition[secondaryDevice] = fileOffset;
			perGameSettings(dirContents.at(fileOffset).name);
			for (int i = 0; i < 25; i++) swiWaitForVBlank();
		}

	}
}
