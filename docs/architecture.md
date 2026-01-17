# Architecture

This document describes the internal architecture and subsystems of Papyrix.

## Font System

### Pipeline

```
Storage → EpdFontLoader → FontManager → GfxRenderer → Display
```

1. **Storage**: Fonts loaded from flash (builtin) or SD card (custom)
2. **EpdFontLoader**: Parses `.epdfont` binary format, provides glyph lookup
3. **FontManager**: Manages font lifecycle, handles loading/unloading
4. **GfxRenderer**: Renders text using font glyphs
5. **Display**: Final output to e-paper

### Memory

- **Builtin fonts**: Flash (DROM), ~20 bytes RAM per wrapper
- **Custom fonts**: SD card → heap, 300-500 KB each

### `.epdfont` Format

Binary format with sections:

```
Header → Metrics → Unicode Intervals → Glyphs → Bitmap
```

- **Header**: Magic, version, font metadata
- **Metrics**: Line height, ascender, descender
- **Unicode Intervals**: Ranges of supported codepoints
- **Glyphs**: Per-character metrics and bitmap offsets
- **Bitmap**: 1-bit or 2-bit packed glyph data

### Key Files

- `lib/EpdFont/` — Format parsing, loading, glyph lookup
- `src/FontManager.h/cpp` — Font lifecycle management
- `lib/GfxRenderer/` — Text rendering with fonts
- `scripts/convert-fonts.mjs` — TTF/OTF to `.epdfont` conversion

### CJK Support

CJK fonts use binary search for glyph lookup: O(log n) complexity. Text can break at any character boundary (no word-based line breaking).
