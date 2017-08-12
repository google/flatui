Building for Linux    {#flatui_guide_linux}
==================

# Prerequisites

FlatUI depends on [FPLBase][], and has the same prerequisites.
Please install the [FPLBase prerequisites][] before building.

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
  [FPLBase]: https://github.com/google/fplbase
  [FPLBase prerequisites]: http://google.github.io/fplbase/fplbase_guide_linux.html
  [Ubuntu]: http://www.ubuntu.com
