# Spork

A toy interpreter for the C language in the very early stages.

## Progress

- Translation phases 1 to 4 (most of preprocessing): mostly done. Largest missing piece is properly evaluating conditional expressions for `#if` (will probably have to wait until after language parsing is done). Also missing are some directives like `#line` and `#pragma` and some small details in others, like performing macro expansion on `#include` file names.

- Phases 5 and 6 (converting char constants and string literals): about half way done. Most escape sequences are handled and string literals are joined. Joining wide strings with normal strings is not done, and neither are universal character escapes (`\u` and `\U`).

- First bit of phase 7 (converting preprocessing tokens to tokens): in progress. Keywords, identifiers, inetegers, floats and strings are being converted. Missing character constants.

- The rest of phase 7 (parsing and compilation) and phase 8 (linking): not started.

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
