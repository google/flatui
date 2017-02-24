FlatUI release notes
====================

### 1.0.1
#### New Features
*   Emoji support in the text rendering.

#### Other Changes
*   Bug fixes.
*   Added new samples.
* text_color_emoji: demonstrate color emoji feature.

### 1.0.0
#### New Features
*   3D transform support.
*   Serialization support.
*   Signed Distance Field support in font rendering.
*   Improved glyph cache behavior.
*   RTL (Right To Left) text layout and locale support.
*   Kerning scaling and line height scaling in the text rendering.\n
    Add SetTextKerningScale() and SetTextLineHeightScale() API.

#### API Modifications
* FontManager has an extra argument to specify number of glyph cache slices.
~~~
  FontManager(const mathfu::vec2i &cache_size, int32_t max_slices);
~~~

* Edit widget has an extra argument returning detailed edit status.
~~~
  Event Edit(float ysize, const mathfu::vec2 &size, const char *id,
             EditStatus *status, std::string *string);
~~~

#### Other Changes
*   Bug fixes.
*   Added new samples.
    * sample_3d: demonstrate 3D UI feature.
    * serialization: demonstrate serialization feature.
    * sdf: show signed distance field enabled font rendering feature.
    * text_layout: show misc text layout features.

### 0.9.0
#### Overview
The initial release of FlatUi.