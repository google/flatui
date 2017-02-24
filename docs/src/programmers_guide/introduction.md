Introduction    {#flatui_guide_introduction}
============

# Immediate mode.

As mentioned in the [overview][], FlatUI is not your average GUI library.
Instead of maintaining a tree of widgets and getting callbacks from them
(like is customary in most UI toolkits),
you specify the UI in terms of functions that create, display and handle
events all in one. There are no objects to hold on to. In the absolute simplest
case, it could look like this (pseudo code):

~~~{.cpp}
if (button("Click me!")) printf("I was clicked!\n");
~~~

The reason this can work is because games render the entire display at
(ideally) 60 frames per second, so even though the code above creates, renders
tests and then throws away a button, it will be back the next frame.

Because we don't hold on to anything, making a UI appear dependent on what's
going on elsewhere in the game is real easy too:

~~~{.cpp}
if (show_button && button("Click me!")) printf("I was clicked!\n");
~~~

Depending on the `show_button` condition, this (part of) the UI will show up
or not. Note that there's no need to check when to create a widget, when to
delete it, to install or remove event handlers, and any bugs related to the
widget not being properly removed due to game state changes we hadn't
foreseen.

Dealing with state is also nice:

~~~{.cpp}
label("value: " + x);
if (button("increase")) x++;
~~~

Note that we can stick this code wherever we want, like in the class that owns
`x`, and keep the data really local. Since the "event handler" is local too,
it is really easy to see that this code is correct, unlike with a callback that
may get executed when this object has already changed state (or has
been deleted, etc). Also note that we don't have to write code to ensure
the label is updated to reflect the new value, this is all explicit in the code
(because the GUI library doesn't own the state, the caller does).


# Layout and groups.

Now the above pseudo code is maybe a bit too simple, since we conveniently
ignored that we were going to do automatic widget layout, which is pretty
much impossible to do without knowing the full set of widgets. So let's look
at the real code for the last example, and how it differs:

~~~{.cpp}
gui::Run([&]() {
  gui::StartGroup(gui::kLayoutVerticalLeft, 5);
    gui::Label("value: " + x, 40);
    if (gui::TextButton("increase", 40) == kEventWentUp) x++;
  gui::EndGroup();
});
~~~

Our gui code now sits inside a C++ [lambda][] passed to `gui::Run()`.
The reason for this is that `Run` actually executes that block of code twice,
once to gather information on the sizes of things in the ui, and the second
time to actually render it and fire off events.

Also new here is `StartGroup` and `EndGroup`. These mark a set of widgets
that should be grouped together, in this case our label and button. It also
specifies how they should be layed out: vertically, i.e. the button goes under
the label, and with child elements to the left, meaning if the two elements
don't have the same width, they will align to the left.

You can nest groups however deep you need, and at each level choose between
horizontal, vertical and overlay for direction (x, y and z axis), as well as
left/right/center alignment for children of that group.

There are many different kinds of events you can listen for on UI elements,
`kEventWentUp` is most typical, since you want to only register a click if
if the mouse/finger both went down and up on the same button (the system makes
sure that an up event only fires on the corresponding UI element that received
the down event, see "identity" below).

Finally, note the `40` that indicates the font height. This is not in pixels,
but in virtual coordinates, more on that later. Similarly, `5` indicates
spacing between group elements.


# Initialization.

FlatUI currently relies on FPLBase (or an engine that provides a similar
interface) to work. Before you can call `Run`, your app needs to have
initialized FPLBase (its `InputManager`, `Renderer` and `AssetManager`), and
typically you will want to have loaded any textures you want to use in the UI.
See the FPLBase documentation for more details on this.

Additionally, FlatUI comes with a font rendering system encapsulating
functionality from [FreeType][], [HarfBuzz][] and [Libunibreak][]. You constuct
a `fpl::FontManager` object, and then call `Open` on it to specify the font
you'd like to use, and pass it the renderer you created above, e.g.

~~~{.cpp}
fontman.Open("fonts/NotoSansCJKjp-Bold.otf");
~~~

Now you're ready to use FlatUI. You pass the above as arguments to `Run`:

~~~{.cpp}
gui::Run(assetman, fontman, inputman, [&]() { ...UI goes here... });
~~~


# Virtual coordinates.

FlatUI uses "virtual coordinates" for sizes of everything, making it easy to
scale your UI to fit devices with varying resolution screens.

The virtual resolution is indicated in terms of the amount of virtual units
that fit into the smallest dimension of the display (i.e. the Y height for
a landscape display, and the X width for portrait). It is by default 1000.
If you'd like to change that, you call:

~~~{.cpp}
gui::SetVirtualResolution(1080);
~~~

As the first thing inside `Run`. For example, the setting above would mean that
you'd be addressing pixels on a 1080p screen, and a scaled version on any other
resolution.

Note that aspect ratio is not taken into account, i.e. units are always square.
For example, if you use the example virtual resolution of 1080 on a 16:9
aspect ratio device, the other dimension will come out to 1920 as you'd
expect. But if you run the same code on a 4:3 aspect ratio device, your
other dimension will be 1440 instead.

~~~{.txt}
+------1920------+                    +-----1440-----+
|                |                    |              |
|     16:9     1080 <- specified -> 1080     4:3     |
|                |                    |              |
+----------------+                    |              |
                                      +--------------+
~~~
If your application needs to work on a variety of aspect ratios, be sure to
leave plenty of space around your UI, and test on multiple devices. For example,
In the above case you can lay out your menu such that it fits neatly in the Y
direction, but you shouldn't be using all of the X direction, or things will
fall off the screen on a 4:3 device. Similarly, in portrait mode, your virtual
resolution would specify the width, which means you have to be careful about
available space in height.

UI element sizes are always rounded to nearest pixel sizes even on a scaled
display. For example, we specified `40` as a font height above, which would
amount to a non-integer amount of pixels on some displays. The actual size
gets rounded to the nearest pixel size, guaranteeing that rasterized glyphs
match pixels 1:1 and thus look crisp. Similarly, lines and other thin elements
will always use whole pixels and thus look great on all devices.

If for some reason you want to render a UI using native device pixels instead,
this is still possible by simply setting the virtual resolution to be
equal to pixels, e.g.:

~~~{.cpp}
gui::SetVirtualResolution(min(renderer.window_size().x,
                              renderer.window_size().x));
~~~


# Basic elements.

Basic elements you can render as part of a UI are images and text:

~~~{.cpp}
gui::Image(tex_about, 60);
gui::Label("The quick brown fox jumps over the lazy dog.", 40);
~~~

The image refers to a texture resource, that must have already been loaded
at initialization time using the `AssetManager` (see `LoadTexture`).

The text will be rendered using the font set in the FontManager. The text
may contain unicode characters (UTF-8). Text will be formatted using
[HarfBuzz][].

The virtual sizes `60` and `40` are the vertical sizes of these elements,
the horizontal size will be derived from the image ratio and the text size
respectively.

You can call `Label` with a 3rd argument indicating a 2D size to get a
multi-line label that renders inside that area. You can leave the Y
coordinate at 0 to allow the area to extend in the Y direction as big as it
needs to be. Breaking of text will be done in a way to honors unicode rules
courtesy of [Libunibreak][].

Use `SetTextColor` to change the color used for rendering text (default: white).

# Identity and interactivity.

Labels and images are not by themselves interactive, they are merely elements
that can make up an interactive element. Interactive elements are groups that
check for events.

Since in immediate mode the UI gets effectively created and destroyed each
frame, we need a mechanism to identify elements across frames, to ensure that
as elements dynamically get added and removed, we don't fire events on the
wrong elements. We do this using "ids" which can be any text string and
passed as an optional 3rd argument to `StartGroup`:

~~~{.cpp}
StartGroup(gui::kLayoutVerticalLeft, 5, "my_button_1");
~~~

These ids can be constants, or they can also be generated on the fly, as
FlatUI only computes and stores a hash of it.

Ideally all interactive elements have a unique id, but nothing particularly bad
will happen if two elements happen to have the same id. Two such buttons for
example allow a user to press down on one button and to release on the second,
and have it register as a click, which you generally don't want.

Typically, you wouldn't call `StartGroup` directly to create an interactive
element, but you'd use `TextButton`, `ImageButton`, or something else made
specifically for you app. But to see how to create one of these yourself,
let's look at the implementation of the example `ImageButton`:

~~~{.cpp}
Event ImageButton(const char *texture_name, float size, const char *id) {
  StartGroup(kLayoutVerticalLeft, size, id);
  SetMargin(Margin(10));
  auto event = CheckEvent();
  if (event & kEventIsDown) ColorBackground(vec4(1.0f, 1.0f, 1.0f, 0.5f));
  else if (event & kEventHover) ColorBackground(vec4(0.5f, 0.5f, 0.5f, 0.5f));
  Image(texture_name, size);
  EndGroup();
  return event;
}
~~~

As you can see, you can put any kind of functionality inside `StartGroup` and
`EndGroup` to define an interactive element. Since these are all simple
function calls, it is easy to compose whatever custom elements you may need.

Here, we don't just use an Image, but we also change the background depending
on the current event, to signal the user that she's hovering or clicking. Again,
you can decide to make this look however you want.

`SetMargin` creates a margin around all the elements contained in this group.
The `Margin` helper object allows you to specify all sides equally (as here),
or specify individual sizes.

Note that it is the presence of `CheckEvent` that makes this group
"interactive". This matters whenever you want to use this UI with gamepad or
keyboard navigation.

Besides `ColorBackground`, `ImageBackground` is a convenient way to change
the look of a button, and `ImageBackgroundNinePatch` allows you to use a
[ninepatch][] as background.

# Overlays and positioning.

We briefly mentioned overlays above. These are the mechanism to have elements
layed out independently and/or on top of eachother. They can be used for
independent parts of a UI placed on different parts of the screen (like parts
of the HUD or "windows") or to place elements on top (modal dialogs, popup
menus, hover help).

To use them, create a top-level group of type `kLayoutOverlay`. Then, for
each group inside of that group, call `PositionGroup` right after `StartGroup`:

~~~{.cpp}
gui::PositionGroup(kAlignCenter, kAlignCenter, mathfu::kZeros2f);
~~~

This sets the position of this sub-group. The first
two arguments allow you to specify horizontal and vertical placement, choosing
from 9 possible areas on screen. Then a 3rd offset parameter allows you to
place the UI relative to that, for more fine grained control. For example,
to place all sub-groups as "windows", you'd use top-left positioning with
the offset denoting their position.

By default, all these groups can receive events, which assumes they are not
overlapping. If they do overlap, multiple elements could receive an event,
which may be undesirable. If a group denotes something like a modal popup,
you can shut down events to earlier groups simply by calling `ModalGroup`
inside the group that is meant to receive the events exclusively:

~~~{.cpp}
ModalGroup();
~~~

# Text edit widgets.

You can take text input similarly to rendering a `Label`:

~~~{.cpp}
gui::Edit(30, vec2(0, 30), "edit1", &str);
~~~
`30` is the font height, `vec2(0, 30)` indicates we want an edit box that
expands in X as the user types, but is limited in Y to just the font height (use
large numbers to allow a multi-line edit box). `"edit1"` is our id, and `str`
is a `std::string` that holds the current string contents.


# Scrolling areas.

You can create a scrolling area (a group of elements that may be smaller
than the on-screen area it is rendered inside of) simply by calling
`StartScroll` inside a `StartGroup` (and similarly, `EndScroll` before
the corresponding `EndGroup`):

~~~{.cpp}
StartScroll(vec2(200, 100), &scroll_offset);
// elements go here..
EndScroll();
~~~

This causes all elements to be renderen in a 200x100 area regardless of how
big they actually are. If they are larger, parts of these elements outside
the area will be clipped and not shown. The area can be scrolled by various
means, e.g. the mouse scroll wheel or touch screen dragging. The speed at
which this happens can be changed using `SetScrollSpeed`.

`scroll_offset` is the 2D offset that stores how far the area has been
scrolled, and typically starts at (0, 0).

Events function like normal for any visible parts of elements.

Note that scrolling areas currently don't work with custom projections, see
below.


# Custom projections.

By default, FlatUI renders on top of your entire display. You can

~~~{.cpp}
UseExistingProjection(vec2i(1024, 1024));
~~~

Which stops FlatUI from creating its own projection, and instead simply
uses whatever projection is currently set on the `Renderer`. This may be
an alternative 2D projection (e.g. using only part of the screen) or even
a 3D projection, showing the UI in 3D space.

The size passed serves as the pixel area FlatUI will render to. This
should ideally correspond to the actual physical pixels this will be rendered
to. In 3D it matters less, of course.


  [overview]: @ref flatui_overview
  [lambda]: http://en.cppreference.com/w/cpp/language/lambda
  [Build Configurations]: @ref flatui_build_config
  [FreeType]: http://www.freetype.org/
  [HarfBuzz]: http://www.freedesktop.org/wiki/Software/HarfBuzz/
  [Libunibreak]: http://vimgadgets.sourceforge.net/libunibreak/
  [ninepatch]: http://developer.android.com/guide/topics/graphics/2d-graphics.html#nine-patch
