SolveSpace
==========

This repository includes an unofficial fork of [SolveSpace][]
with enhancened Linux support and, in future, other improvements.

[solvespace]: http://solvespace.com

Installation
------------

### Debian and Ubuntu

Binary packages for Ubuntu trusty and later versions are available
in [~whitequark/solvespace PPA][ppa].

[ppa]: https://launchpad.net/~whitequark/+archive/ubuntu/solvespace

### Other systems

See below.

Building
--------

You will need cmake, libpng, zlib, json-c, fontconfig, gtkmm 2.4, pangomm 1.4,
OpenGL and OpenGL GLU.
On a Debian derivative (e.g. Ubuntu) these packages can be installed with:

    apt-get install libpng12-dev zlib1g-dev libjson-c-dev libfontconfig1-dev \
                    libgtkmm-2.4-dev libpangomm-1.4-dev libgl-dev libglu-dev

After that, build SolveSpace as following:

    mkdir build
    cd build
    cmake ..
    make
    sudo make install

Fully functional ports to other UI toolkits (GTK3 and FLTK) are available,
but not recommended for use due to bugs in these toolkits.

License
-------

SolveSpace is distributed under the terms of the [GPL3 license](COPYING.txt).