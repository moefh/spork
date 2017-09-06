# Spork

A toy interpreter for the C language in the very early stages.

## Progress

- Translation phases 1 to 4 (most of preprocessing): mostly
  done. Largest missing piece is properly evaluating conditional
  expressions for `#if` (will probably have to wait until after
  language parsing is done). Also missing are some directives like
  `#line` and `#pragma`.

- Phases 5 and 6 (converting char constants and string literals):
  mostly done.

- Phase 7 (parsing and compilation): very early stages. Right now it
  only converts pp-tokens to tokens.

- Phase 8 (linking): not started.

## Name

Why "Spork"?

A spork is in the awkward middle-ground between a fork and a spoon:
its prongs are not sharp enough to stab, but it's not as good for soup
as a spoon.

Likewise, this project is in the awkward middle-ground between
low-level and high-level. It doesn't offer the speed and control of
native compilation, or the conveniences of high level languages like
automatic memory management.

## License

MIT License ([LICENSE](https://github.com/ricardo-massaro/spork/blob/master/LICENSE))
