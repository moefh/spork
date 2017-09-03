# Spork

A toy interpreter for the C language in the very early stages.

## Progress

- Translation phases 1 to 4 (most of preprocessing) are mostly done.

- Phases 5 and 6 should be easy: process escape sequences in character
constants and string literals and join adjacent strings.

- First bit of phase 7 is also east: converting preprocessing tokens to tokens.

- The rest of phase 7 is where the fun begins: parsing and compilation.

- Translation phase 8 is linking.

## Name

Why "Spork"?

A spork is in the awkward middle-ground between a fork and a spoon:
its prongs are not sharp enough to stab, and it doesn't hold soup as
well as a spoon.

Likewise, this project is in the awkward middle-ground between
low-level and high-level. It doesn't offer the speed and control of
native compilation, nor does it offer the convenience of high level
languages, like automatic memory management.

## License

MIT License ([LICENSE](https://github.com/ricardo-massaro/spork/blob/master/LICENSE))
