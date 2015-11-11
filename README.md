AccordionTest
=============
Laptop Accordion Support Code

### WTF
To get AccordionTest running, create an empty OpenFrameworks project
using the standard add-ons ofxGui and ofxOpenCv, in addition to the
custom add-on ofxCv. Then copy the `/src` directory of this repository
to your project's `/src` directory.

### FluidSynth
Go [here](http://sourceforge.net/projects/fluidsynth/files/) and download
the latest version of FluidSynth. On Linux or Mac OSX, follow the directions
in the INSTALL file to install FluidSynth as a library on your system. On
Windows, your best and easiest bet is probably to grab the pre-built DLL
[here](http://www.zdoom.org/files/fluidsynth.7z).

Now create a new folder to hold a custom copy of FluidSynth. On Windows, start
by copying the pre-built DLL to this folder. On all platforms, then, go to where
you downloaded FluidSynth, and copy `include/fluidsynth.h` and `include/fluidsynth`
to your new folder. In your OpenFrameworks IDE of choice, add your new folder to
the compiler search path; Windows users should also add it to the linker search
path.

If you are on OSX or Linux, you're done. Otherwise, finally copy the DLL to the
`/bin` directory in your OpenFrameworks project. You should now have the DLL in
two places: first, in your custom folder for FluidSynth, and second, in the `/bin`
folder of your OpenFrameworks project. Good luck!
