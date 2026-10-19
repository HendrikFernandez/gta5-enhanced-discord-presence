GTA V Enhanced Discord Rich Presence

1. Install Script Hook V for your exact GTA V Enhanced game version, with its ASI loader:
   https://www.dev-c.com/gtav/scripthookv
2. Copy GtaVEnhancedPresence.asi, GtaVEnhancedPresence.ini, and
   GtaVEnhancedPresence.missions.ini into the folder containing GTA5_Enhanced.exe.
3. Create an application at https://discord.com/developers/applications and
   put its Application ID after ApplicationId= in GtaVEnhancedPresence.ini.
4. Open desktop Discord and start GTA V Enhanced Story Mode.

The public download has a blank Application ID; use your own application.

The activity shows Play time for the loaded Story Mode save, along with
status, area, completion, and money. Icons mark each item in Discord.
For character portraits, upload square images to your application's Rich
Presence Art Assets with keys michael, franklin, and trevor. Then set
MichaelImageKey=michael, FranklinImageKey=franklin, and
TrevorImageKey=trevor in the INI. Leave these blank to use the app icon.
Do not install or use this ASI for GTA Online.

Local check (19 October 2026): Script Hook V detected the tested game as
VER_EN_1_0_1158_13 and launched GtaVEnhancedPresence.asi successfully.
If Discord shows Rockstar's built-in GTA activity instead of the mod's
mission/area details, turn off Discord Rich Presence in Rockstar Games
Launcher > Settings > General, then restart the game.
