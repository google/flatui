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
  gui::StartGroup(fpl::gui::kLayoutVerticalLeft);
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

Finally, not the `40` that indicates the font height. This is not in pixels,
but in virtual coordinates, more on that later.



  [overview]: @ref flatui_overview
  [lambda]: http://en.cppreference.com/w/cpp/language/lambda
  [Build Configurations]: @ref flatui_build_config