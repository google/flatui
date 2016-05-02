FlatUI    {#flatui_index}
======

# Overview    {#flatui_overview}

[FlatUI][] is an *immediate mode* C++ GUI library that aims to be a simple,
efficient and easy to use way to add menus, [HUDs][] and any kind of other UI
to your game or graphical application. It also offers unicode & i18n aware
font-rendering.

[FlatUI] is available as open source from
[GitHub](http://github.com/google/flatui) under the Apache license, v2
(see LICENSE.txt).

# Why use FlatUI?

   * **Immediate mode** - this means that rather than constructing a UI using
     objects, you do so with simple function calls that determine the structure
     of the UI each frame. This has many advantages:
     * **Less code** - simple UIs are simple, and require less setup work.
     * **Easy dynamic UIs** - no object management when the UI changes
       (add/remove). A simple if-then suffices to make (part of) a UI
       visible. No state to get out of sync, no clean-up.
     * **Handle events locally** - directly next to the code that creates the
       element, where you have all relevant data available, instead of having
       to react to an event outside the context of the code that constructed it.
     * **Compositional** - functions are easier to abstract and compose than
       objects, allowing you to combine common patterns of UI construction
       code into functions that capture your usage.
   * **Super efficient** - the memory usage by the core layouting / rendering
     code of FlatUI is on the order of a few KB, even in complex UIs, and is
     transient. Your memory usage will effectively be just whatever textures
     and fonts you use.
   * **Automatic Layout** - rather than specifying UI elements in pixels, you
     specify virtual sizes of things, making it easy to render the same UI
     on different resolutions and aspect ratios, and allowing it to
     dynamically adapt to any content. Placement still ensures pixel alignment
     for maximum crispness in font & image rendering.
   * **Style independent** - Though we supply sample widgets, it is very easy
     to make your own with specialized rendering. No complicated templating /
     skinning required.
   * **Designed for games** - supports typical game use cases.
   * **Unicode and i18n aware** - text rendering that builds on HarfBuzz and
     libunibreak.

# Supported Platforms and Dependencies.

[FlatUI][] has been tested on the following platforms:

   * [Android][]
   * [Linux][] (x86_64)
   * [OS X][]
   * [Windows][]

This library is entirely written in portable C++11 and depends on the
following libraries (included in the download / submodules):

   * [FreeType][] for font rendering.
   * [HarfBuzz][] and [Libunibreak][] for handling unicode text correctly.
   * [MathFu][] for vector math.
   * [FPLBase][] for rendering and input, which in turn uses:
     * [SDL][] as a cross-platform layer.
     * [OpenGL][] (desktop/ES).
     * [FlatBuffers][] for serialization.
     * [WebP][] for image loading.
     * [STB][] for image loading.
   * [FPLUtil][] (only for build).
   * [googletest][] (only for unit tests).

# Download

Download using git or from the
[releases page](http://github.com/google/flatui/releases).

**Important**: FlatUI uses submodules to reference other components it depends
upon so download the source using:

~~~{.sh}
    git clone --recursive https://github.com/google/flatui.git
~~~

# Feedback and Reporting Bugs

   * Discuss FlatUI with other developers and users on the
     [FlatUI Google Group][].
   * File issues on the [FlatUI Issues Tracker][].
   * Post your questions to [stackoverflow.com][] with a mention of **flatui**.

  [Android]: http://www.android.com
  [Build Configurations]: @ref flatui_build_config
  [Linux]: http://en.m.wikipedia.org/wiki/Linux
  [FlatUI Google Group]: http://group.google.com/group/flatuilib
  [FlatUI Issues Tracker]: http://github.com/google/flatui/issues
  [FlatUI]: @ref flatui_overview
  [OS X]: http://www.apple.com/osx/
  [Windows]: http://windows.microsoft.com/
  [stackoverflow.com]: http://www.stackoverflow.com
  [FreeType]: http://www.freetype.org/
  [HarfBuzz]: http://www.freedesktop.org/wiki/Software/HarfBuzz/
  [Libunibreak]: http://vimgadgets.sourceforge.net/libunibreak/
  [MathFu]: https://google.github.io/mathfu/
  [FlatBuffers]: https://google.github.io/flatbuffers/
  [FPLUtil]: https://google.github.io/fplutil/
  [FPLBase]: https://google.github.io/fplbase/
  [SDL]: https://www.libsdl.org/
  [STB]: https://github.com/nothings/stb
  [OpenGL]: https://www.opengl.org/
  [WebP]: https://developers.google.com/speed/webp/?hl=en
  [googletest]: https://code.google.com/p/googletest/
  [HUDs]: https://en.wikipedia.org/wiki/Head-up_display





