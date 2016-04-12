FlatUI Version 1.0.0    {#flatui_readme}
====================

# Welcome to FlatUI!

FlatUI is a immediate mode C++ GUI library for games and graphical applications.
Go to our [landing page][] to browse our documentation.

FlatUI aims to be a simple, efficient and easy to use way to add menus,
[HUDs][] and any kind of other UI to your game or graphical application,
and provide unicode & i18n aware font-rendering.
FlatUI can be built for many different systems (Android, Windows, OS X, Linux),
see `docs/html/index.html`.

FlatUI integrates with our other game development libraries, primarily
[FPLBase][].

Discuss FlatUI with other developers and users on the
[FlatUI Google Group][]. File issues on the [FlatUI Issues Tracker][]
or post your questions to [stackoverflow.com][] with a mention of
**flatui**.

**Important**: FlatUI uses submodules to reference other components it depends
upon so download the source using:

~~~{.sh}
   git clone --recursive https://github.com/google/flatui.git
~~~

**Changes**: Please see [the release notes](./release_notes.md) for a list of changes.

For applications on Google Play that integrate this tool, usage is tracked.
This tracking is done automatically using the embedded version string
(FlatUiVersion), and helps us continue to optimize it. Aside from
consuming a few extra bytes in your application binary, it shouldn't affect
your application at all.  We use this information to let us know if FlatUI
is useful and if we should continue to invest in it. Since this is open
source, you are free to remove the version string but we would appreciate if
you would leave it in.

  [FlatUI Google Group]: http://group.google.com/group/flatuilib
  [FlatUI Issues Tracker]: http://github.com/google/flatui/issues
  [stackoverflow.com]: http://www.stackoverflow.com
  [landing page]: http://google.github.io/flatui
  [FPLBase]: https://github.com/google/fplbase
  [HUDs]: https://en.wikipedia.org/wiki/Head-up_display