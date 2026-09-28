ISS Tracker for the Amiga
=========================

Shows where the International Space Station is right now, on a
32 colour world map with day/night shading, twinkling city lights
on the night side, the ground track of its last few orbits and the
country or ocean below it. Uses a FujiNet running fujinet-nio
firmware.

Requirements
------------
 - Any Amiga with Kickstart/Workbench 1.3 or later, 512K is enough
 - A FujiNet with fujinet-nio firmware, connected to WiFi
 - The FujiNet NIO drivers installed and loaded, so that
   fujinet-nio.device is resident. On Workbench 1.3 run
   Install-FujiNet-WB13 from the FujiNet NIO release disk, then add
       C:fujinet-load-resident DEVS:fujinet-nio.device fujinet-nio.device
   to S:Startup-Sequence (or S:StartupII) and reboot.

Running
-------
Double-click the ISSTracker icon, or type ISSTracker in a Shell.

The position is refreshed every minute, and every couple of
minutes an astronaut steps out for a spacewalk around the
station. Keep watching and, every so often, something else drops
by for a look around. Keys (also in the menu):

   R        refresh now
   W        who is in space right now
   S        send the astronaut on a spacewalk
   U        UFO sighting!
   T        ground track on/off
   N        night shading on/off
   V        viewing circle on/off
   M        sound on/off
   B        screen saver (any key or click returns)
   Q, Esc   quit (or click the close gadget)

After 10 idle minutes the screen saver starts by itself.

Your home
---------
Tell ISS Tracker where you live and it marks home on the map,
shows how far away the ISS is, and pings when the ISS comes over
your horizon. Select the ISSTracker icon, choose Info from the
Workbench menu and edit the Tool Types, e.g.

   HOMELAT=51.48
   HOMELON=-0.01

(decimal degrees, south and west negative), removing the
brackets from the examples. From a Shell, give the same settings
as arguments: ISSTracker HOMELAT=51.48 HOMELON=-0.01

Other Tool Types: SAVER=minutes (0 = never), SOUND=OFF,
CIRCLE=OFF (hide the viewing circle).

Credits
-------
 Position and crew data: Open Notify, http://open-notify.org
 World map: NASA Blue Marble (public domain)
 City lights: NASA Black Marble 2016 (public domain)
 Countries and oceans: Natural Earth (public domain)
 Part of the FujiNet ISS Tracker project, licensed under GPL v3.
