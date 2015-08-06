Building for Linux    {#flatui_guide_linux}
==================

# Version Requirements

Following are the minimum required versions of tools and libraries you
need to build [FlatUI][] for Android:

   * [CMake][]: 2.8.12.1

# Prerequisites

Prior to building, install the following components using the [Linux][]
distribution's package manager:

   * [CMake][].  You can also manually install packages from
     [cmake.org](http://cmake.org).

For example, on [Ubuntu][]:

~~~{.sh}
    sudo apt-get install cmake
~~~

# Building

   * Open a command line window.
   * Go to the [FlatUI][] project directory.
   * Generate [Makefiles][] from the [CMake][] project. <br/>
   * Execute `make` to build the library and unit tests.

For example:

~~~{.sh}
    cd flatui
    cmake -G'Unix Makefiles' .
    make
~~~

To perform a debug build:

~~~{.sh}
    cd flatui
    cmake -G'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug .
    make
~~~

Build targets can be configured using options exposed in
`flatui/CMakeLists.txt` by using [CMake]'s `-D` option.
Build configuration set using the `-D` option is sticky across subsequent
builds.

For example, if a build is performed using:

~~~{.sh}
    cd flatui
    cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug .
    make
~~~

to switch to a release build CMAKE_BUILD_TYPE must be explicitly specified:

~~~{.sh}
    cd flatui
    cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release .
    make
~~~

<br>

  [CMake]: http://www.cmake.org/
  [Linux]: http://en.wikipedia.org/wiki/Linux
  [Makefiles]: http://www.gnu.org/software/make/
  [FlatUI]: @ref flatui_overview
  [Ubuntu]: http://www.ubuntu.com
