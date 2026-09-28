ISS Tracker for the Amiga
=========================

Shows where the International Space Station is right now, on a
32 colour world map with day/night shading, city lights on the
night side and the ground track of its last few orbits. Uses a FujiNet running fujinet-nio firmware.

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
station. Keys (also in the menu):

   R        refresh now
   W        who is in space right now
   S        send the astronaut on a spacewalk
   T        ground track on/off
   N        night shading on/off
   Q, Esc   quit (or click the close gadget)

Credits
-------
 Position and crew data: Open Notify, http://open-notify.org
 World map: NASA Blue Marble (public domain)
 City lights: NASA Black Marble 2016 (public domain)
 Part of the FujiNet ISS Tracker project, licensed under GPL v3.
