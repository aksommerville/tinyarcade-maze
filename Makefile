all:
.SILENT:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

# Likely to change.
PROJECT_NAME:=maze
PORT:=ttyACM0
IDEROOT:=/opt/arduino-1.8.16

# Likely constant, for Linux users.
BUILDER:=$(IDEROOT)/arduino-builder
PKGROOT:=$(wildcard ~/.arduino15/packages)
LIBSROOT:=$(wildcard ~/Arduino/libraries)
SRCFILES:=$(shell find src -type f)
TMPDIR:=mid/build
CACHEDIR:=mid/cache
TMPDIRNORMAL:=mid/buildn
CACHEDIRNORMAL:=mid/cachen
EXECARD:=out/0002.bin
EXENORM:=out/0002-normal.bin

SRCFILES:=$(filter-out src/dummy.cpp,$(SRCFILES))

clean:;rm -rf mid out

$(TMPDIR):;mkdir -p $@
$(CACHEDIR):;mkdir -p $@
$(TMPDIRNORMAL):;mkdir -p $@
$(CACHEDIRNORMAL):;mkdir -p $@

define BUILD # 1=goal, 2=tmpdir, 3=cachedir, 4=BuildOption
$1:$2 $3; \
  $(BUILDER) \
  -compile \
  -logger=machine \
  -hardware $(IDEROOT)/hardware \
  -hardware $(PKGROOT) \
  -tools $(IDEROOT)/tools-builder \
  -tools $(IDEROOT)/hardware/tools/avr \
  -tools $(PKGROOT) \
  -built-in-libraries $(IDEROOT)/libraries \
  -libraries $(LIBSROOT) \
  -fqbn=TinyCircuits:samd:tinyarcade:BuildOption=$4 \
  -ide-version=10816 \
  -build-path $2 \
  -warnings=none \
  -build-cache $3 \
  -prefs=build.warn_data_percentage=75 \
  -prefs=runtime.tools.openocd.path=$(PKGROOT)/arduino/tools/openocd/0.10.0-arduino7 \
  -prefs=runtime.tools.openocd-0.10.0-arduino7.path=$(PKGROOT)/arduino/tools/openocd/0.10.0-arduino7 \
  -prefs=runtime.tools.arm-none-eabi-gcc.path=$(PKGROOT)/arduino/tools/arm-none-eabi-gcc/7-2017q4 \
  -prefs=runtime.tools.arm-none-eabi-gcc-7-2017q4.path=$(PKGROOT)/arduino/tools/arm-none-eabi-gcc/7-2017q4 \
  -prefs=runtime.tools.bossac.path=$(PKGROOT)/arduino/tools/bossac/1.7.0-arduino3 \
  -prefs=runtime.tools.bossac-1.7.0-arduino3.path=$(PKGROOT)/arduino/tools/bossac/1.7.0-arduino3 \
  -prefs=runtime.tools.CMSIS-Atmel.path=$(PKGROOT)/arduino/tools/CMSIS-Atmel/1.2.0 \
  -prefs=runtime.tools.CMSIS-Atmel-1.2.0.path=$(PKGROOT)/arduino/tools/CMSIS-Atmel/1.2.0 \
  -prefs=runtime.tools.CMSIS.path=$(PKGROOT)/arduino/tools/CMSIS/4.5.0 \
  -prefs=runtime.tools.CMSIS-4.5.0.path=$(PKGROOT)/arduino/tools/CMSIS/4.5.0 \
  -prefs=runtime.tools.arduinoOTA.path=$(PKGROOT)/arduino/tools/arduinoOTA/1.2.1 \
  -prefs=runtime.tools.arduinoOTA-1.2.1.path=$(PKGROOT)/arduino/tools/arduinoOTA/1.2.1 \
  src/dummy.cpp $(SRCFILES) \
  2>&1 | etc/tool/reportstatus.py
  # Add -verbose to taste
endef

# For inclusion in a TinyArcade SD card.
BINFILE:=$(TMPDIR)/dummy.cpp.bin
all:$(EXECARD)
$(EXECARD):build-card;$(PRECMD) cp $(BINFILE) $@
$(eval $(call BUILD,build-card,$(TMPDIR),$(CACHEDIR),TAgame))

# For upload.
NBINFILE:=$(TMPDIRNORMAL)/dummy.cpp.bin
all:$(EXENORM)
$(EXENORM):build-normal;$(PRECMD) cp $(NBINFILE) $@
$(eval $(call BUILD,build-normal,$(TMPDIRNORMAL),$(CACHEDIRNORMAL),normal))
  
launch:$(EXENORM); \
  stty -F /dev/$(PORT) 1200 ; \
  sleep 2 ; \
  $(PKGROOT)/arduino/tools/bossac/1.7.0-arduino3/bossac -i -d --port=$(PORT) -U true -i -e -w $(EXENORM) -R
