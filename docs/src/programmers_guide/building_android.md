Building for Android    {#flatui_guide_android}
====================

# Prerequisites

Set up your build environment for Android builds by following these steps.

   1. Install [Java 1.7][], required to use Android tools.
        * **Windows / OSX**: Download the [Java 1.7][] installer and run it to
          install.
        * **Linux**: on systems that support apt-get...<br/>
          `sudo apt-get install openjdk-7-jdk`

   2. Install the [Android SDK][], required to build Android applications.<br/>
      [Android Studio][] is the easiest way to install and configure the latest
      [Android SDK][]. Or you can install the [stand-alone SDK tools][].

   3. Install the [Android NDK][], required to develop Android native (C/C++)
      applications.<br/>
        * Download and unpack the latest version of the [Android NDK][] to a
          directory on your machine.
          * Tested using `android-ndk-r10e`.

   4. Add the [Android SDK][]'s `sdk/tools` directory and the [Android NDK][]
      directory to the [PATH variable][].
        * **Windows**:
          Start -> Control Panel -> System and Security -> System ->
          Advanced system settings -> Environment Variables,
          then select PATH and press `Edit`.
          Add paths similar to the following, adjusted for your install
          locations:
          `c:\Users\me\AppData\Local\Android\sdk\tools;c:\Users\me\android-ndk`
        * **Linux**: if the [Android SDK][] is installed in
          `/home/androiddev/adt` and the [Android NDK][] is
          installed in `/home/androiddev/ndk` the following line should be
          added to `~/.bashrc`.<br/>
          `export PATH="$PATH:/home/androiddev/adt/sdk/tools:/home/androiddev/ndk"`
        * **OS X**: if the [Android SDK][] is installed in
          `~/Library/Android/` and the [Android NDK][] is
          installed in `~/bin/android_ndk-r10e` the following line should be
          added to `~/.bash_profile`.<br/>
          `export PATH=$PATH:~/bin/android_ndk-r10d:~/Library/Android/sdk/tools`

   5. On **Linux**, ensure [Java 1.7][] is selected, in the event multiple Java
      versions are installed on the system.
        * on Ubunutu, run `update-java-alternatives` to select the correct
          Java version.

   6. On **Windows**, set the `JAVA_HOME` variable to the Java install location
        * see [Setting Windows Environment Variables][].
        * set `variable name` to JDK_HOME.
        * set `variable value` to your Java installation directory,
          something like `C:\Program Files\Java\jdk1.7.0_75`.


Additionally, if you'd like to use the handy tools in [fplutil][],
   * [Apache Ant][], required to build Android applications with [fplutil][].
        * **Windows / OS X**: Download the latest version of [Apache Ant][] and
          unpack it to a directory.
        * **Linux**: on systems that support `apt-get`...<br/>
          `sudo apt-get install ant`
        * Add the [Apache Ant][] install directory to the [PATH variable][].
   * [Python 2.7][], required to use [fplutil][] tools.<br/>
        * **Windows / OS X**: Download the latest package from the [Python 2.7][]
          page and run the installer.
        * **Linux**: on a systems that support `apt-get`...<br/>
          `sudo apt-get install python`


# Code Generation

By default, code is generated for devices that support the `armeabi-v7a`,
`x86`, or `armeabi` ABIs. Alternatively, you can generate a fat `.apk` that
includes code for all ABIs. To do so, override APP\_ABI on ndk-build's command
line.

Using `fplutil`:
~~~{.sh}
cd flatui
./dependencies/fplutil/bin/build_all_android.py
  --apk_keypk8 <pk8 file> --apk_keypem <pem file> -S

~~~

Using `ndk-build`:

~~~{.sh}
    cd flatui
    ndk-build -j20
~~~

<br>

  [adb]: http://developer.android.com/tools/help/adb.html
  [ADT]: http://developer.android.com/tools/sdk/eclipse-adt.html
  [Android Developer Tools]: http://developer.android.com/sdk/index.html
  [Android NDK]: http://developer.android.com/tools/sdk/ndk/index.html
  [Android SDK]: http://developer.android.com/sdk/index.html
  [Android Studio]: http://developer.android.com/sdk/index.html
  [Apache Ant]: https://www.apache.org/dist/ant/binaries/
  [apk]: http://en.wikipedia.org/wiki/Android_application_package
  [fplutil]: http://google.github.io/fplutil
  [fplutil prerequisites]: http://google.github.io/fplutil/fplutil_prerequisites.html
  [Java 1.7]: http://www.oracle.com/technetwork/java/javase/downloads/jdk7-downloads-1880260.html
  [managing avds]: http://developer.android.com/tools/devices/managing-avds.html
  [NDK Eclipse plugin]: http://developer.android.com/sdk/index.html
  [PATH variable]: http://en.wikipedia.org/wiki/PATH_(variable)
  [Python 2.7]: https://www.python.org/download/releases/2.7/
  [Setting Windows Environment Variables]: http://www.computerhope.com/issues/ch000549.htm
  [stand-alone SDK tools]: http://developer.android.com/sdk/installing/index.html?pkg=tools
  [USB debugging]: http://developer.android.com/tools/device.html
