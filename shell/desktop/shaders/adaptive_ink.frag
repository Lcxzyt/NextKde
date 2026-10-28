#version 440

// Per-pixel foreground ink over KOS glass. The compositor's backdrop map
// (one stable ink cell per ~8 logical px, sampled bilinearly) decides, for
// every pixel of a neutral glyph, whether it is drawn light or dark. A glyph
// lying across a light/dark edge of the backdrop is split along that edge,
// the way Apple's glass controls re-ink their labels.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D backdropMap;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // This item's rectangle inside the shape, normalized to the map.
    vec4 mapRect;
    vec4 lightInk;
    vec4 darkInk;
    // Ink used before the first map arrives (or on an older compositor).
    vec4 fallbackInk;
    float mapEnabled;
    // Multiplies the source coverage (a mask's own alpha is always 1).
    float inkAlpha;
    // Retain the uniform layout while moving the decision into the bridge.
    float band;
} ubuf;

void main() {
    float alpha = texture(source, qt_TexCoord0).a * ubuf.inkAlpha;
    vec2 mapUv = clamp(ubuf.mapRect.xy + qt_TexCoord0 * ubuf.mapRect.zw, vec2(0.0), vec2(1.0));
    // The native bridge applies per-cell hysteresis and confirmation before
    // publishing this binary mask. Interpolation locates the spatial boundary;
    // threshold it instead of creating persistent grey ink on a grey backdrop.
    float dark = step(0.5, texture(backdropMap, mapUv).r);
    vec3 adaptive = mix(ubuf.lightInk.rgb, ubuf.darkInk.rgb, dark);
    vec3 ink = mix(ubuf.fallbackInk.rgb, adaptive, ubuf.mapEnabled);
    // Coverage (antialiasing, secondary-text alpha) is kept; only the ink
    // changes. Qt Quick expects premultiplied output.
    fragColor = vec4(ink * alpha, alpha) * ubuf.qt_Opacity;
}
