# Maze

My first attempt at a game for TinyArcade [https://tinycircuits.com/].

## Requirements

- Arduino IDE 1.8.16.
- TinyArcade hardware.
- Optional: Linux, GLX.

Builds from a Makefile that calls out to Arduino IDE.
It's configured for my installation, expect to have to change a few constants in the Makefile.

If you set `NATIVE_PLATFORM:=linux-glx` in the Makefile, it also builds a version you can run on the PC.

- `make` to build everything.
- `make launch` to build for TinyArcade and launch via USB.
- - Beware, this wipes your default program. You'll have to rebuild the arcade menu later.
- `make run` to build the native binary and launch it.

"Native" is not an emulator! It's entirely possible a thing could run Native but fail on the hardware.

## License

boooooring

Public Domain. Use at your own risk. NO WARRANTY.
