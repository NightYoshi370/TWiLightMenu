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

#include "perGameSettings.h"
#include "buttontext.h"
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <string>
#include <sstream>
#include <stdio.h>
#include <dirent.h>

#include <nds.h>
#include <maxmod9.h>
#include "common/gl2d.h"

#include "date.h"

#include "ndsheaderbanner.h"
#include "iconTitle.h"
#include "graphics/fontHandler.h"
#include "graphics/graphics.h"
#include "graphics/FontGraphic.h"
#include "graphics/TextPane.h"
#include "graphics/ThemeTextures.h"
#include "sound.h"
#include "SwitchState.h"
#include "cheat.h"
#include "errorScreen.h"

#include "gbaswitch.h"
#include "nds_loader_arm9.h"

#include "common/inifile.h"
#include "common/flashcard.h"
#include "common/dsimenusettings.h"

#define SCREEN_COLS 32
#define ENTRIES_PER_SCREEN 15
#define ENTRIES_START_ROW 3
#define ENTRY_PAGE_LENGTH 10


extern int currentBg;
extern bool displayGameIcons;

const char* SDKnumbertext;

extern bool fadeType;
extern bool showdialogbox;
extern bool dbox_showIcon;

bool perGameSettingsChanged = false;

int perGameSettings_cursorPosition = 0;
bool perGameSettings_directBoot = false;	// Homebrew only
int perGameSettings_dsiMode = -1;
int perGameSettings_language = -2;
int perGameSettings_saveNo = 0;
int perGameSettings_ramDiskNo = -1;
int perGameSettings_boostCpu = -1;
int perGameSettings_boostVram = -1;
int perGameSettings_heapShrink = -1;
int perGameSettings_bootstrapFile = -1;

extern int file_count;

char pergamefilepath[256];

extern mm_sound_effect snd_launch;
extern mm_sound_effect snd_select;
extern mm_sound_effect snd_stop;
extern mm_sound_effect snd_wrong;
extern mm_sound_effect snd_back;
extern mm_sound_effect snd_switch;

extern char usernameRendered[11];
extern bool usernameRenderedDone;

char fileCounter[8];
char gameTIDText[16];
char saveNoDisplay[8];

int firstPerGameOpShown = 0;
int perGameOps = -1;
int perGameOp[8] = {-1};

void loadPerGameSettings (std::string filename) {
	snprintf(pergamefilepath, sizeof(pergamefilepath), "%s/_nds/TWiLightMenu/gamesettings/%s.ini", (ms().secondaryDevice ? "fat:" : "sd:"), filename.c_str());
	CIniFile pergameini( pergamefilepath );
	perGameSettings_directBoot = pergameini.GetInt("GAMESETTINGS", "DIRECT_BOOT", (isModernHomebrew[CURPOS] || ms().secondaryDevice));	// Homebrew only
	if ((isDSiMode() && ms().useBootstrap) || !ms().secondaryDevice) {
		perGameSettings_dsiMode = pergameini.GetInt("GAMESETTINGS", "DSI_MODE", (isModernHomebrew[CURPOS] ? true : -1));
	} else {
		perGameSettings_dsiMode = -1;
	}
	perGameSettings_language = pergameini.GetInt("GAMESETTINGS", "LANGUAGE", -2);
	perGameSettings_saveNo = pergameini.GetInt("GAMESETTINGS", "SAVE_NUMBER", 0);
	perGameSettings_ramDiskNo = pergameini.GetInt("GAMESETTINGS", "RAM_DISK", -1);
	perGameSettings_boostCpu = pergameini.GetInt("GAMESETTINGS", "BOOST_CPU", -1);
	perGameSettings_boostVram = pergameini.GetInt("GAMESETTINGS", "BOOST_VRAM", -1);
	perGameSettings_heapShrink = pergameini.GetInt("GAMESETTINGS", "HEAP_SHRINK", -1);
	perGameSettings_bootstrapFile = pergameini.GetInt("GAMESETTINGS", "BOOTSTRAP_FILE", -1);
}

void savePerGameSettings (std::string filename) {
	snprintf(pergamefilepath, sizeof(pergamefilepath), "%s/_nds/TWiLightMenu/gamesettings/%s.ini", (ms().secondaryDevice ? "fat:" : "sd:"), filename.c_str());
	CIniFile pergameini( pergamefilepath );
	if (isHomebrew[CURPOS]) {
		if (!ms().secondaryDevice) pergameini.SetInt("GAMESETTINGS", "LANGUAGE", perGameSettings_language);
		pergameini.SetInt("GAMESETTINGS", "DIRECT_BOOT", perGameSettings_directBoot);
		if (isDSiMode()) {
			pergameini.SetInt("GAMESETTINGS", "DSI_MODE", perGameSettings_dsiMode);
		}
		if (!ms().secondaryDevice) pergameini.SetInt("GAMESETTINGS", "RAM_DISK", perGameSettings_ramDiskNo);
		if (REG_SCFG_EXT != 0) {
			pergameini.SetInt("GAMESETTINGS", "BOOST_CPU", perGameSettings_boostCpu);
			pergameini.SetInt("GAMESETTINGS", "BOOST_VRAM", perGameSettings_boostVram);
		}
	} else {
		if (ms().useBootstrap || !ms().secondaryDevice) {
			pergameini.SetInt("GAMESETTINGS", "LANGUAGE", perGameSettings_language);
			pergameini.SetInt("GAMESETTINGS", "SAVE_NUMBER", perGameSettings_saveNo);
			pergameini.SetInt("GAMESETTINGS", "HEAP_SHRINK", perGameSettings_heapShrink);
			pergameini.SetInt("GAMESETTINGS", "BOOTSTRAP_FILE", perGameSettings_bootstrapFile);
		}

		if ((isDSiMode() && ms().useBootstrap) || !ms().secondaryDevice)
			pergameini.SetInt("GAMESETTINGS", "DSI_MODE", perGameSettings_dsiMode);

		if (REG_SCFG_EXT != 0) {
			pergameini.SetInt("GAMESETTINGS", "BOOST_CPU", perGameSettings_boostCpu);
			pergameini.SetInt("GAMESETTINGS", "BOOST_VRAM", perGameSettings_boostVram);
		}
	}
	pergameini.SaveIniFileModified();
}

bool checkIfShowAPMsg (std::string filename) {
	snprintf(pergamefilepath, sizeof(pergamefilepath), "%s/_nds/TWiLightMenu/gamesettings/%s.ini", (ms().secondaryDevice ? "fat:" : "sd:"), filename.c_str());
	CIniFile pergameini(pergamefilepath);
	return (pergameini.GetInt("GAMESETTINGS", "NO_SHOW_AP_MSG", 0) == 0);
}

void dontShowAPMsgAgain (std::string filename) {
	snprintf(pergamefilepath, sizeof(pergamefilepath), "%s/_nds/TWiLightMenu/gamesettings/%s.ini", (ms().secondaryDevice ? "fat:" : "sd:"), filename.c_str());
	CIniFile pergameini(pergamefilepath);
	pergameini.SetInt("GAMESETTINGS", "NO_SHOW_AP_MSG", 1);
	pergameini.SaveIniFileModified();
}

bool checkIfDSiMode (std::string filename) {
	if (!isDSiMode() || (!ms().useBootstrap && ms().secondaryDevice))
		return false;

	snprintf(pergamefilepath, sizeof(pergamefilepath), "%s/_nds/TWiLightMenu/gamesettings/%s.ini", (ms().secondaryDevice ? "fat:" : "sd:"), filename.c_str());
	CIniFile pergameini(pergamefilepath);
	perGameSettings_dsiMode = pergameini.GetInt("GAMESETTINGS", "DSI_MODE", (isModernHomebrew[CURPOS] ? true : -1));
	return perGameSettings_dsiMode == -1 ? ms().bstrap_dsiMode : perGameSettings_dsiMode;
}

void perGameSettings (std::string filename) {
	int pressed = 0;

	if (ms().theme == 4) {
		snd().playStartup();
		fadeType = false;	   // Fade to black
		for (int i = 0; i < 25; i++) {
			swiWaitForVBlank();
		}
		currentBg = 1;
		displayGameIcons = false;
		fadeType = true;
	} else {
		dbox_showIcon = true;
		showdialogbox = true;
	}
	clearText();
	
	snprintf (fileCounter, sizeof(fileCounter), "%i/%i", (CURPOS+1)+PAGENUM*40, file_count);
	
	perGameSettings_cursorPosition = 0;
	loadPerGameSettings(filename);

	std::string filenameForInfo = filename;
	if((filenameForInfo.substr(filenameForInfo.find_last_of(".") + 1) == "argv")
	|| (filenameForInfo.substr(filenameForInfo.find_last_of(".") + 1) == "ARGV"))
	{

		std::vector<char*> argarray;

		FILE *argfile = fopen(filenameForInfo.c_str(),"rb");
			char str[PATH_MAX], *pstr;
		const char seps[]= "\n\r\t ";

		while( fgets(str, PATH_MAX, argfile) ) {
			// Find comment and end string there
			if( (pstr = strchr(str, '#')) )
				*pstr= '\0';

			// Tokenize arguments
			pstr= strtok(str, seps);

			while( pstr != NULL ) {
				argarray.push_back(strdup(pstr));
				pstr= strtok(NULL, seps);
			}
		}
		fclose(argfile);
		filenameForInfo = argarray.at(0);
	}

	FILE *f_nds_file = fopen(filenameForInfo.c_str(), "rb");

	bool showSDKVersion = false;
	u32 SDKVersion = 0;
	if (memcmp(gameTid[CURPOS], "HND", 3) == 0 || memcmp(gameTid[CURPOS], "HNE", 3) == 0 || !isHomebrew[CURPOS]) {
		SDKVersion = getSDKVersion(f_nds_file);
		showSDKVersion = true;
	}
	fclose(f_nds_file);
	
	bool showPerGameSettings =
		(!isDSiWare[CURPOS]
		&& memcmp(gameTid[CURPOS], "HND", 3) != 0
		&& memcmp(gameTid[CURPOS], "HNE", 3) != 0);
	if (!ms().useBootstrap && REG_SCFG_EXT == 0) {
		showPerGameSettings = false;
	}

	firstPerGameOpShown = 0;
	perGameOps = -1;
	if (isHomebrew[CURPOS]) {		// Per-game settings for homebrew
		if (!ms().secondaryDevice) {
			perGameOps++;
			perGameOp[perGameOps] = 0;	// Language
			perGameOps++;
			perGameOp[perGameOps] = 1;	// RAM disk number
		}
		perGameOps++;
		perGameOp[perGameOps] = 6;	// Direct boot
		if (isDSiMode() || !ms().secondaryDevice) {
			perGameOps++;
			perGameOp[perGameOps] = 2;	// Run in
		}
		if (REG_SCFG_EXT != 0) {
			perGameOps++;
			perGameOp[perGameOps] = 3;	// ARM9 CPU Speed
			perGameOps++;
			perGameOp[perGameOps] = 4;	// VRAM Boost
		}
		if (!ms().secondaryDevice) {
			perGameOps++;
			perGameOp[perGameOps] = 7;	// Bootstrap
		}
	} else if (!showPerGameSettings) {
		for (int i = 0; i < 8; i++) {
			perGameOp[i] = -1;
		}
	} else {	// Per-game settings for retail/commercial games with nds-bootstrap/B4DS
		if (ms().useBootstrap || !ms().secondaryDevice) {
			perGameOps++;
			perGameOp[perGameOps] = 0;	// Language
			perGameOps++;
			perGameOp[perGameOps] = 1;	// Save number
			if (isDSiMode()) {
				perGameOps++;
				perGameOp[perGameOps] = 2;	// Run in
			}
		}
		if (REG_SCFG_EXT != 0) {
			perGameOps++;
			perGameOp[perGameOps] = 3;	// ARM9 CPU Speed
			perGameOps++;
			perGameOp[perGameOps] = 4;	// VRAM Boost
		}
		if (ms().useBootstrap || !ms().secondaryDevice) {
			if (SDKVersion < 0x5000000 || !isDSiMode()) {
				perGameOps++;
				perGameOp[perGameOps] = 5;	// Heap shrink
			}
			perGameOps++;
			perGameOp[perGameOps] = 7;	// Bootstrap
		}
	}

	snprintf (gameTIDText, sizeof(gameTIDText), "TID: %s", gameTid[CURPOS]);

	if((SDKVersion > 0x1000000) && (SDKVersion < 0x2000000)) {
		SDKnumbertext = "SDK ver: 1";
	} else if((SDKVersion > 0x2000000) && (SDKVersion < 0x3000000)) {
		SDKnumbertext = "SDK ver: 2";
	} else if((SDKVersion > 0x3000000) && (SDKVersion < 0x4000000)) {
		SDKnumbertext = "SDK ver: 3";
	} else if((SDKVersion > 0x4000000) && (SDKVersion < 0x5000000)) {
		SDKnumbertext = "SDK ver: 4";
	} else if((SDKVersion > 0x5000000) && (SDKVersion < 0x6000000)) {
		SDKnumbertext = "SDK ver: 5 (TWLSDK)";
	} else {
		SDKnumbertext = "SDK ver: ?";
	}
	if (ms().theme == 4) {
		while (!screenFadedIn()) { swiWaitForVBlank(); }
		dbox_showIcon = true;
	} else {
		for (int i = 0; i < 30; i++) { snd().updateStream(); swiWaitForVBlank(); }
	}

	// About 38 characters fit in the box.
	std::string displayFilename = filename;
	if (strlen(displayFilename.c_str()) > 35) {
		// Truncate to 35, 35 + 3 = 38 (because we append "...").
		displayFilename.resize(32, ' ');
		size_t first = displayFilename.find_first_not_of(' ');
		size_t last = displayFilename.find_last_not_of(' ');
		displayFilename = displayFilename.substr(first, (last - first + 1));
		displayFilename.append("...");
	}

	while (1) {
		clearText();
		titleUpdate(isDirectory[CURPOS], filename.c_str(), CURPOS);

		printSmall(false, 16, 66, displayFilename.c_str());
		if (showSDKVersion) printSmall(false, 16, 80, SDKnumbertext);
		printSmall(false, 176, 80, gameTIDText);
		printSmall(false, 16, 160, fileCounter);

		int perGameOpYpos = ms().theme == 2 ? 112 : 98;

		if (showPerGameSettings) {
			printSmall(false, 16, 98+(perGameSettings_cursorPosition*14)-(firstPerGameOpShown*14), ">");
		}
		for (int i = firstPerGameOpShown; i < firstPerGameOpShown+4; i++) {
			if (!showPerGameSettings || perGameOp[i] == -1)
				break;

			int xPos;
			switch (perGameOp[i]) {
				case 0: {
					printSmall(false, 24, perGameOpYpos, "Language:");

					std::string languageText;
					xPos = 188 - (ms().theme == 2 ? 8 : 0);
					switch (perGameSettings_language) {
						case -2:
							languageText = "Default";
							break;
						case -1:
							languageText = "System";
							break;
						case 0:
							languageText = "Japanese";

							if (ms().theme == 2)
								xPos = 172;
							break;
						case 1:
							languageText = "English";
							break;
						case 2:
							languageText = "French";
							break;
						case 3:
							languageText = "German";
							break;
						case 4:
							languageText = "Italian";
							break;
						case 5:
							languageText = "Spanish";
							break;
					}

					printSmall(false, xPos, perGameOpYpos, languageText.c_str());
					break;
				} case 1:
					xPos = 24 + (ms().theme == 2 ? 8 : 0);
					if (isHomebrew[CURPOS]) {
						printSmall(false, xPos, perGameOpYpos, "RAM disk:");
						snprintf (saveNoDisplay, sizeof(saveNoDisplay), "%i", perGameSettings_ramDiskNo);
					} else {
						printSmall(false, xPos, perGameOpYpos, "Save number:");
						snprintf (saveNoDisplay, sizeof(saveNoDisplay), "%i", perGameSettings_saveNo);
					}

					if (isHomebrew[CURPOS] && perGameSettings_ramDiskNo == -1) {
						printSmall(false, 200, perGameOpYpos, "None");
					} else {
						printSmall(false, 220, perGameOpYpos, saveNoDisplay);
					}
					break;
				case 2:
					printSmall(false, 24, perGameOpYpos, "Run in:");
					if (perGameSettings_dsiMode == -1) {
						printSmall(false, 188, perGameOpYpos, "Default");
					} else if (perGameSettings_dsiMode == 2) {
						printSmall(false, 128, perGameOpYpos, "DSi mode (Forced)");
					} else if (perGameSettings_dsiMode == 1) {
						printSmall(false, 184, perGameOpYpos, "DSi mode");
					} else {
						printSmall(false, 184, perGameOpYpos, "DS mode");
					}
					break;
				case 3:
					printSmall(false, 24, perGameOpYpos, "ARM9 CPU Speed:");
					if (perGameSettings_dsiMode > 0 && isDSiMode()) {
						printSmall(false, 146, perGameOpYpos, "133mhz (TWL)");
					} else {
						if (perGameSettings_boostCpu == -1) {
							printSmall(false, 188, perGameOpYpos, "Default");
						} else if (perGameSettings_boostCpu == 1) {
							printSmall(false, 146, perGameOpYpos, "133mhz (TWL)");
						} else {
							printSmall(false, 156, perGameOpYpos, "67mhz (NTR)");
						}
					}
					break;
				case 4:
					printSmall(false, 24, perGameOpYpos, "VRAM boost:");
					if (perGameSettings_dsiMode > 0 && isDSiMode()) {
						printSmall(false, 188, perGameOpYpos, "On");
					} else {
						if (perGameSettings_boostVram == -1) {
							printSmall(false, 188, perGameOpYpos, "Default");
						} else if (perGameSettings_boostVram == 1) {
							printSmall(false, 188, perGameOpYpos, "On");
						} else {
							printSmall(false, 188, perGameOpYpos, "Off");
						}
					}
					break;
				case 5:
					printSmall(false, 24, perGameOpYpos, "Heap shrink:");
					if (perGameSettings_heapShrink == -1) {
						printSmall(false, 200, perGameOpYpos, "Auto");
					} else if (perGameSettings_heapShrink == 1) {
						printSmall(false, 200, perGameOpYpos, "On");
					} else {
						printSmall(false, 200, perGameOpYpos, "Off");
					}
					break;
				case 6:
					printSmall(false, 24, perGameOpYpos, "Direct boot:");
					if (perGameSettings_directBoot) {
						printSmall(false, 200, perGameOpYpos, "Yes");
					} else {
						printSmall(false, 200, perGameOpYpos, "No");
					}
					break;
				case 7:
					printSmall(false, 24, perGameOpYpos, "Bootstrap:");
					if (perGameSettings_bootstrapFile == -1) {
						printSmall(false, 188, perGameOpYpos, "Default");
					} else if (perGameSettings_bootstrapFile == 1) {
						printSmall(false, 188, perGameOpYpos, "Nightly");
					} else {
						printSmall(false, 188, perGameOpYpos, "Release");
					}
					break;
			}
			perGameOpYpos += 14;
		}
		if (isHomebrew[CURPOS]) {		// Per-game settings for homebrew
			printSmall(false, 194, 160, BUTTON_B" Back");
		} else if (!showPerGameSettings) {
			printSmall(false, 208, 160, BUTTON_A" OK");
		} else {	// Per-game settings for retail/commercial games
			if ((isDSiMode() && ms().useBootstrap) || !ms().secondaryDevice) {
				printSmall(false, 128, 160, BUTTON_X " Cheats  " BUTTON_B" Back");
			} else {
				printSmall(false, 194, 160, BUTTON_B" Back");
			}
		}
		do {
			scanKeys();
			pressed = keysDown();
			checkSdEject();
			tex().drawVolumeImageCached();
			tex().drawBatteryImageCached();
			drawCurrentTime();
			drawCurrentDate();
			drawClockColon();
			snd().updateStream();
			swiWaitForVBlank();
		} while (!pressed);

		if (!showPerGameSettings && ((pressed & KEY_A) || (pressed & KEY_B))) {
			snd().playBack();
			break;
		} else {
			if (pressed & KEY_UP) {
				snd().playSelect();
				perGameSettings_cursorPosition--;
				if (perGameSettings_cursorPosition < 0) {
					perGameSettings_cursorPosition = perGameOps;
					firstPerGameOpShown = (perGameOps>2 ? perGameOps-3 : 0);
				} else if (perGameSettings_cursorPosition < firstPerGameOpShown) {
					firstPerGameOpShown--;
				}
			} else if (pressed & KEY_DOWN) {
				snd().playSelect();
				perGameSettings_cursorPosition++;
				if (perGameSettings_cursorPosition > perGameOps) {
					perGameSettings_cursorPosition = 0;
					firstPerGameOpShown = 0;
				} else if (perGameSettings_cursorPosition > firstPerGameOpShown+3) {
					firstPerGameOpShown++;
				}
			}

			if (pressed & KEY_LEFT) {
				switch (perGameOp[perGameSettings_cursorPosition]) {
					case 0:
						perGameSettings_language--;
						if (perGameSettings_language < -2) perGameSettings_language = 5;
						break;
					case 1:
						if (isHomebrew[CURPOS]) {
							if (!perGameSettings_directBoot) {
								perGameSettings_ramDiskNo--;
								if (perGameSettings_ramDiskNo < -1) perGameSettings_ramDiskNo = 9;
							}
						} else {
							perGameSettings_saveNo--;
							if (perGameSettings_saveNo < 0) perGameSettings_saveNo = 9;
						}
						break;
					case 2:
						perGameSettings_dsiMode--;
						if (perGameSettings_dsiMode < -1) perGameSettings_dsiMode = 2;
						break;
					case 3:
						if (perGameSettings_dsiMode < 1) {
							perGameSettings_boostCpu--;
							if (perGameSettings_boostCpu < -1) perGameSettings_boostCpu = 1;
						}
						break;
					case 4:
						if (perGameSettings_dsiMode < 1) {
							perGameSettings_boostVram--;
							if (perGameSettings_boostVram < -1) perGameSettings_boostVram = 1;
						}
						break;
					case 5:
						perGameSettings_heapShrink--;
						if (perGameSettings_heapShrink < -1) perGameSettings_heapShrink = 1;
						break;
					case 6:
						perGameSettings_directBoot = !perGameSettings_directBoot;
						break;
					case 7:
						perGameSettings_bootstrapFile--;
						if (perGameSettings_bootstrapFile < -1) perGameSettings_bootstrapFile = 1;
						break;
				}
				(ms().theme == 4) ? snd().playLaunch() : snd().playSelect();
				perGameSettingsChanged = true;
			} else if ((pressed & KEY_A) || (pressed & KEY_RIGHT)) {
				switch (perGameOp[perGameSettings_cursorPosition]) {
					case 0:
						perGameSettings_language++;
						if (perGameSettings_language > 5) perGameSettings_language = -2;
						break;
					case 1:
						if (isHomebrew[CURPOS]) {
							if (!perGameSettings_directBoot) {
								perGameSettings_ramDiskNo++;
								if (perGameSettings_ramDiskNo > 9) perGameSettings_ramDiskNo = -1;
							}
						} else {
							perGameSettings_saveNo++;
							if (perGameSettings_saveNo > 9) perGameSettings_saveNo = 0;
						}
						break;
					case 2:
						perGameSettings_dsiMode++;
						if (perGameSettings_dsiMode > 2-isHomebrew[CURPOS]) perGameSettings_dsiMode = -1;
						break;
					case 3:
						if (perGameSettings_dsiMode < 1) {
							perGameSettings_boostCpu++;
							if (perGameSettings_boostCpu > 1) perGameSettings_boostCpu = -1;
						}
						break;
					case 4:
						if (perGameSettings_dsiMode < 1) {
							perGameSettings_boostVram++;
							if (perGameSettings_boostVram > 1) perGameSettings_boostVram = -1;
						}
						break;
					case 5:
						perGameSettings_heapShrink++;
						if (perGameSettings_heapShrink > 1) perGameSettings_heapShrink = -1;
						break;
					case 6:
						perGameSettings_directBoot = !perGameSettings_directBoot;
						break;
					case 7:
						perGameSettings_bootstrapFile++;
						if (perGameSettings_bootstrapFile > 1) perGameSettings_bootstrapFile = -1;
						break;
				}
				(ms().theme == 4) ? snd().playLaunch() : snd().playSelect();
				perGameSettingsChanged = true;
			}

			if (pressed & KEY_B) {
				snd().playBack();
				if (perGameSettingsChanged) {
					savePerGameSettings(filename);
					perGameSettingsChanged = false;
				}
				break;
			}
			if ((pressed & KEY_X) && !isHomebrew[CURPOS]) {
			  if ((isDSiMode() && ms().useBootstrap) || !ms().secondaryDevice) {
				(ms().theme == 4) ? snd().playLaunch() : snd().playSelect();
				CheatCodelist codelist;
				codelist.selectCheats(filename);
			  }
			}
		}
	}
	showdialogbox = false;
	if (ms().theme == 4) {
		fadeType = false;	   // Fade to black
		for (int i = 0; i < 25; i++) {
			swiWaitForVBlank();
		}
		clearText();
		currentBg = 0;
		displayGameIcons = true;
		fadeType = true;
		snd().playStartup();
	} else {
		clearText();
	}
}

std::string getSavExtension(void) {
	switch (perGameSettings_saveNo) {
		case 0:
		default:
			return ".sav";
			break;
		case 1:
			return ".sav1";
			break;
		case 2:
			return ".sav2";
			break;
		case 3:
			return ".sav3";
			break;
		case 4:
			return ".sav4";
			break;
		case 5:
			return ".sav5";
			break;
		case 6:
			return ".sav6";
			break;
		case 7:
			return ".sav7";
			break;
		case 8:
			return ".sav8";
			break;
		case 9:
			return ".sav9";
			break;
	}
}

std::string getImgExtension(int number) {
	switch (number) {
		case 0:
		default:
			return ".img";
			break;
		case 1:
			return ".img1";
			break;
		case 2:
			return ".img2";
			break;
		case 3:
			return ".img3";
			break;
		case 4:
			return ".img4";
			break;
		case 5:
			return ".img5";
			break;
		case 6:
			return ".img6";
			break;
		case 7:
			return ".img7";
			break;
		case 8:
			return ".img8";
			break;
		case 9:
			return ".img9";
			break;
	}
}
