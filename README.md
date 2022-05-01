# spice-autorandr

#### The problem
Since some time, the spice client (or the X driver? not shure) does no longer automatically resize the window if the virt-viewer window is resized. Instead, it is up to the desktop environment to receive the RandR notification and act upon it.
Unfortunately, not all desktop environments have this implemented yet (I know only of GNOME, maybe plasma/KDE has, too).
#### The solution
First I created a small script, basically doing (pseudo code, but you get the idea)

    xev --root --event randr | \
    while read e; do
        ignore_line e && continue
        xrandr --output Virtual-0 --auto
    done

with the ```ignore_line``` function (not shown) containing some magic, that avoids calling xrandr too often.

Since a bash script seemed slightly unwieldy for this, I implemented similar functionality in C.

### Building

You need (at least) the following devel-packages installed:
  * xrandr
  * x11
  * xproto
  * xorg-macros

On an openSUSE system, ```zypper install 'pkgconfig(xrandr)' 'pkgconfig(x11)' 'pkgconfig(xproto)' 'pkgconfig(xorg-macros)'``` should install all that's needed. ```gcc``` and ```make``` are of course also needed.

On a Debian-like system, ```apt install libxrandr-dev libx11-dev pkgconf``` should install the dependencies.

Building should be easy:

    ./configure
    make

If you are building directly from a git checkout, prepend these with ```autoreconf -is```.

### Technical details
This is the actual ```xrandr``` output of a VM (some resolutions omitted):

    tux@factory-vm:~> xrandr
    Screen 0: minimum 320 x 200, current 1024 x 680, maximum 8192 x 8192
    Virtual-0 connected 1024x680+0+0 0mm x 0mm
       1024x680      59.91*+
       1280x1024     59.89  
       1280x960      59.94  
       1280x800      59.81  
       1280x720      59.86  
       1024x768      59.92  
       800x600       59.86  
       640x480       59.38  
    Virtual-1 disconnected
    Virtual-2 disconnected
    Virtual-3 disconnected

Now I'm resizing the virt-viewer window:

    tux@factory-vm:~> xrandr
    Screen 0: minimum 320 x 200, current 1024 x 680, maximum 8192 x 8192
    Virtual-0 connected 1024x680+0+0 0mm x 0mm
       1024x582      59.94 +
       1280x1024     59.89
       1280x960      59.94
       1280x800      59.81
       1280x720      59.86
       1024x768      59.92
       800x600       59.86
       640x480       59.38
    Virtual-1 disconnected
    Virtual-2 disconnected
    Virtual-3 disconnected
      1024x680 (0x28c) 56.250MHz -HSync +VSync
            h: width  1024 start 1072 end 1176 total 1328 skew    0 clock  42.36KHz
            v: height  680 start  683 end  693 total  707           clock  59.91Hz

The interesting line is the

    1024x582      59.94 +

The ```+``` telling us, that it's the "preferred" mode, whatever this means.
If you now run ```xrandr --output Virtual-0 --auto``, this will do what we want, resizing the screen to 1024x582.

I then found, that the preferred mode is always the first mode in the list. This allows to use ```xrandr -s 0``` instead of specifying the output.

```xrandr -s``` uses legacy XRandR code (XRandR version less than 1.2) which in turn makes this much easier to code, this is why I went for this route :-)
