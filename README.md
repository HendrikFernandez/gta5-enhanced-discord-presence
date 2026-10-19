# GTA V Enhanced Discord Rich Presence

A 64-bit Script Hook V ASI for **GTA V Enhanced Story Mode**. The Discord activity shows:

- Free roam, a known story or side mission, an activity, paused, wasted, or busted.
- The current map area, Story Mode completion percentage, the active protagonist's cash, and total playtime for the loaded save.
- Elapsed game-session time that stays continuous when the presence changes.

The mod samples the game every five seconds. Known mission and activity script names are editable in `GtaVEnhancedPresence.missions.ini`. An unknown active mission shows `In a mission`. Missing stats are omitted instead of being displayed as zero. The mod clears its presence if an Online session is detected; [Script Hook V](https://www.dev-c.com/gtav/scripthookv) itself does not work in GTA Online.

`Play time` adds the Story Mode playtime tracked for Michael, Franklin, and Trevor in the loaded save. The activity line shows the current status with an icon; the stat line shows the area, completion, money, and play time with icons. Discord controls how that line wraps on each screen. Its elapsed timer still measures the current game session separately.

## Install

1. Install a [Script Hook V](https://www.dev-c.com/gtav/scripthookv) version matching your **Enhanced** game build, including its ASI loader.
2. Download the release ZIP and copy `GtaVEnhancedPresence.asi`, `GtaVEnhancedPresence.ini`, and `GtaVEnhancedPresence.missions.ini` into the folder containing `GTA5_Enhanced.exe`.
3. Create a Discord application in the [Developer Portal](https://discord.com/developers/applications), copy its Application ID, and enter it after `ApplicationId=` in `GtaVEnhancedPresence.ini`.
4. Run the desktop Discord app with activity sharing enabled, then start GTA V Enhanced Story Mode.

The public download has a blank Application ID. Discord's [Developer Terms](https://support-dev.discord.com/hc/en-us/articles/8562894815383-Discord-Developer-Terms-of-Service) treat Application IDs as developer credentials and prohibit embedding them in open source projects.

If Discord keeps showing Rockstar's original GTA activity instead of this mod's mission and area details, open **Rockstar Games Launcher → Settings → General** and turn off **Discord Rich Presence**, then restart the game. Rockstar's built-in presence can compete with the mod's custom application. This setting is documented by [Rockstar Support](https://support.rockstargames.com/articles/3DE4ZKGVBkA64cEhPKFckS/managing-discord-rich-presence-for-rockstar-games-pc-titles).

Discord takes the activity title and artwork from the application you configure. To show the active Story Mode character, upload three square images to the application's **Rich Presence → Art Assets** page with the keys `michael`, `franklin`, and `trevor`, then set `MichaelImageKey=michael`, `FranklinImageKey=franklin`, and `TrevorImageKey=trevor` in the INI. The mod updates the large image within five seconds of a character switch. `LargeImageKey` remains an optional fallback for menus or custom player models; an empty fallback uses the application's icon. Character artwork is not included in the release ZIP.

You can use different uploaded asset names in the three character fields. Leaving a character key empty uses the fallback.

The release ZIP contains this mod's files. Script Hook V is a separate prerequisite and must match the installed game patch. Rockstar explains [how to disable BattlEye for Story Mode](https://support.rockstargames.com/articles/1nenwhZlVrJY6CTFeSS2Fx/grand-theft-auto-online-battleye-faq) if needed.

## Build from source

Install Visual Studio with C++ and CMake. Download the Script Hook V SDK and point `SHV_SDK_DIR` at a folder containing `inc/main.h` and `lib/ScriptHookV.lib`. CMake downloads [Discord RPC v3.4.0](https://github.com/discord/discord-rpc/releases/tag/v3.4.0) and [RapidJSON v1.1.0](https://github.com/Tencent/rapidjson/releases/tag/v1.1.0).

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DSHV_SDK_DIR="C:/path/to/ScriptHookV_SDK"
cmake --build build --config Release
```

The build copies the INI files beside the ASI. `GtaVEnhancedPresence.ini` is a local, ignored file; the checked-in example has a blank Application ID. The Script Hook V SDK and game files are not included in this repository.

## Technical notes

The code reads game state through Script Hook V natives. The completion stat `total_progress_made` comes from the game's [decompiled completion controller](https://github.com/YimMenu/GTA-V-Decompiled-Scripts/blob/master/decompiled_scripts/completionpercentage_controller.c). Save playtime sums the `SP0_TOTAL_PLAYING_TIME`, `SP1_TOTAL_PLAYING_TIME`, and `SP2_TOTAL_PLAYING_TIME` stats referenced by the game's [decompiled stats controller](https://github.com/YimMenu/GTA-V-Decompiled-Scripts/blob/master/decompiled_scripts/stats_controller.c). Cash uses the three Story Mode character stats, as in [ScriptHookVDotNet's player implementation](https://github.com/scripthookvdotnet/scripthookvdotnet/blob/main/source/scripting_v2/GTA/Player.cs). Area names follow its [zone mapping](https://github.com/scripthookvdotnet/scripthookvdotnet/blob/main/source/scripting_v2/GTA/World.cs). The editable mission list is based on [GTAV Research's script-name notes](https://github.com/Gogsi/GTAV-Research/blob/main/Scripts%20name%20detail.txt); its unverified entries were excluded.

This ASI has been compiled and its imports checked as a 64-bit Windows binary. In the local installation, `GTA5_Enhanced.exe` reports file version **1.0.1158.16**, but Script Hook V detected `VER_EN_1_0_1158_13`, initialized successfully, and launched `GtaVEnhancedPresence.asi` on 19 October 2026. A live Story Mode check confirmed that the Discord picture changed when switching from Franklin to Michael.

This project's source is under the [MIT License](LICENSE). Third-party license texts are in [third-party-licenses](third-party-licenses).
