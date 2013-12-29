Arducam-Obscura
===============

An arduino, light sensor and motor controlled pinhole camera project. This is the arduino code.

Overview
---------

The project uses an arduino fio v3 witch uses a voltage of 3.3v. The fio is used because of its LiPo charging capabilities
and small form factor. Further hardware used is a decent light sensor (reliable, linear output): a TSL2561 from Adafruit,
a small servo motor that can operate with 3.3v and a standard pushbutton.

Also a box is need for loading the film and mounting the electronics.
Creating the pinhole
---------------------
A pinhole is needed as well of course. It should have a diameter of ~0.3mm with a distance of approx. 40mm from pinhole to sensitive film plane. (focal length)
The pinhole should also have a good and round shape. Using a magnifier helps. For good results use a black coated aluminum foil, the smallest stitching needle, sandpaper you can find and a good scanner.
Create a few holes with the foil lying flat on a hard surface. Press the needle with variating pressure. Don't apply too much pressure. Turn the metal foil 180° and smooth the edges with the sandpaper. Next scan the foil with the highest resolution possible and measure the holes. Select the hole with the best shape and size.
size shouldn't be more than 0.5mm and not less than 0.2mm. For the optimum diameter to use, consult material on pinhole cameras/ camera obscura. Source I used include: http://www.pinhole.cz/en/pinholecameras/pinhole_01.html , http://www.f295.org/main/showthread.php?16595-Optimum-Pinhole-Size , http://www.huecandela.com/hue-x/pin-pdf/Prober-%20Wellman.pdf . 


