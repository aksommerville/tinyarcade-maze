# Compile-time config for linux-glx

CC+=-DSOFTARCADE_AUDIO_ENABLE=1 -include src/linux-glx/arduino_glue.h
LDPOST:=-lm -lGL -lGLX -lX11
