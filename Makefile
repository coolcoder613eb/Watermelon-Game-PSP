# Watermelon Game Cross-Platform Makefile
# Supports: Linux, Windows, Android, PSP

PLATFORM ?= LINUX

SRC_DIR = src
BUILD_DIR = build/$(PLATFORM)
BIN_NAME = WatermelonGame
JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/graphics.c $(SRC_DIR)/physics.c $(SRC_DIR)/random.c
HEADERS = $(wildcard $(SRC_DIR)/*.h $(SRC_DIR)/android/*.h)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

RAYLIB_DIR = lib/raylib
RAYLIB_SRC = $(RAYLIB_DIR)/src
RAYLIB_INPUTS = $(shell find $(RAYLIB_SRC) -type f \( -name '*.c' -o -name '*.h' \))
RAYLIB_LINUX_LIB = $(RAYLIB_SRC)/libraylib_linux.a
RAYLIB_WIN_LIB = $(RAYLIB_SRC)/libraylib_win.a

# Android toolchain configuration. Override these for a standard SDK/NDK
# installation; the defaults also support the original local toolchain layout.
ANDROID_NDK ?= $(if $(ANDROID_NDK_HOME),$(ANDROID_NDK_HOME),$(HOME)/.cache/x/Android.ndk)
ANDROID_SDK ?= $(if $(ANDROID_SDK_ROOT),$(ANDROID_SDK_ROOT),$(if $(ANDROID_HOME),$(ANDROID_HOME),$(HOME)/.cache/x/Android.sdk))
ifeq ($(OS),Windows_NT)
ANDROID_HOST_TAG ?= windows-x86_64
else ifeq ($(shell uname -s),Darwin)
ANDROID_HOST_TAG ?= darwin-$(shell uname -m)
else
ANDROID_HOST_TAG ?= linux-$(shell uname -m)
endif
ANDROID_TOOLCHAIN = $(ANDROID_NDK)/toolchains/llvm/prebuilt/$(ANDROID_HOST_TAG)
ifneq ($(wildcard $(ANDROID_TOOLCHAIN)/sysroot),)
ANDROID_SYSROOT ?= $(ANDROID_TOOLCHAIN)/sysroot
else
ANDROID_SYSROOT ?= $(ANDROID_NDK)/usr
endif
ANDROID_SYSROOT_USR = $(if $(wildcard $(ANDROID_SYSROOT)/usr/include),$(ANDROID_SYSROOT)/usr,$(ANDROID_SYSROOT))
ANDROID_CLANG ?= $(if $(wildcard $(ANDROID_TOOLCHAIN)/bin/clang),$(ANDROID_TOOLCHAIN)/bin/clang,clang)
ANDROID_API_LEVEL ?= 33
ANDROID_SDK_JAR ?= $(ANDROID_SDK)/platforms/android-$(ANDROID_API_LEVEL)/android.jar
ANDROID_BUILD_TOOLS_VERSION ?= $(lastword $(sort $(notdir $(wildcard $(ANDROID_SDK)/build-tools/*))))
ANDROID_BUILD_TOOLS ?= $(ANDROID_SDK)/build-tools/$(ANDROID_BUILD_TOOLS_VERSION)
ANDROID_AAPT ?= $(if $(wildcard $(ANDROID_BUILD_TOOLS)/aapt),$(ANDROID_BUILD_TOOLS)/aapt,aapt)
ANDROID_ZIPALIGN ?= $(if $(wildcard $(ANDROID_BUILD_TOOLS)/zipalign),$(ANDROID_BUILD_TOOLS)/zipalign,zipalign)
ANDROID_APKSIGNER ?= $(if $(wildcard $(ANDROID_BUILD_TOOLS)/apksigner),$(ANDROID_BUILD_TOOLS)/apksigner,apksigner)
ANDROID_KEYTOOL ?= keytool

CFLAGS = -Wall -O3 -I$(SRC_DIR)
LDFLAGS =

ifeq ($(PLATFORM),LINUX)
    CC ?= gcc
    CFLAGS += -I$(RAYLIB_SRC) -DPLATFORM_DESKTOP
    LDFLAGS += -L$(RAYLIB_SRC) -lraylib_linux -lGL -lm -lpthread -ldl -lrt -lX11
    TARGET = $(BIN_NAME)
endif

ifeq ($(PLATFORM),WINDOWS)
    CC = x86_64-w64-mingw32-gcc
    WINDRES = x86_64-w64-mingw32-windres
    CFLAGS += -I$(RAYLIB_SRC) -DPLATFORM_DESKTOP
    LDFLAGS += -L$(RAYLIB_SRC) -lraylib_win -lopengl32 -lgdi32 -lwinmm -lm
    TARGET = $(BIN_NAME).exe
    WIN_RES_OBJ = $(BUILD_DIR)/icon.o
endif

ifeq ($(PLATFORM),ANDROID)
    TARGET = $(BIN_NAME).apk
endif

.PHONY: all clean run psp windows linux android apk

ifeq ($(PLATFORM),PSP)
all:
	$(MAKE) -C $(SRC_DIR) -f Makefile
clean:
	$(MAKE) -C $(SRC_DIR) -f Makefile clean
else

all: $(TARGET)

psp:
	$(MAKE) PLATFORM=PSP

windows:
	$(MAKE) PLATFORM=WINDOWS

linux:
	$(MAKE) PLATFORM=LINUX

android apk:
	$(MAKE) PLATFORM=ANDROID

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(RAYLIB_LINUX_LIB): $(RAYLIB_INPUTS)
	$(MAKE) -C $(RAYLIB_SRC) clean
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP -j$(JOBS)
	cp $(RAYLIB_SRC)/libraylib.a $(RAYLIB_LINUX_LIB)

$(RAYLIB_WIN_LIB): $(RAYLIB_INPUTS)
	$(MAKE) -C $(RAYLIB_SRC) clean
	$(MAKE) -C $(RAYLIB_SRC) OS=Windows_NT CC=x86_64-w64-mingw32-gcc PLATFORM=PLATFORM_DESKTOP -j$(JOBS)
	cp $(RAYLIB_SRC)/libraylib.a $(RAYLIB_WIN_LIB)

ifeq ($(PLATFORM),WINDOWS)
$(BUILD_DIR)/icon.o: $(SRC_DIR)/icon.rc $(SRC_DIR)/icon.ico | $(BUILD_DIR)
	$(WINDRES) $(SRC_DIR)/icon.rc -O coff -o $@

$(TARGET): $(BUILD_DIR) $(SRCS) $(RAYLIB_WIN_LIB) $(OBJS) $(WIN_RES_OBJ)
	$(CC) $(OBJS) $(WIN_RES_OBJ) -o $(TARGET) $(LDFLAGS)
	@echo "Windows build complete: $(TARGET)"
else ifeq ($(PLATFORM),ANDROID)

ANDROID_BUILD = build/ANDROID

RAYLIB_MODULES = rcore rshapes rtextures rtext rmodels raudio
GLUE_SRC = $(SRC_DIR)/android/android_native_app_glue.c

ANDROID_RESOURCES = $(shell find $(SRC_DIR)/Resources res -type f -print | sed 's/ /\\ /g')
$(TARGET): $(BUILD_DIR) $(SRCS) $(HEADERS) $(GLUE_SRC) $(ANDROID_RESOURCES) $(RAYLIB_INPUTS) AndroidManifest.xml
	@set -eu; \
	for abi in armeabi-v7a arm64-v8a; do \
		case $$abi in \
			armeabi-v7a) target=armv7a-linux-androideabi; api=19; include=arm-linux-androideabi; libdir=arm-linux-androideabi/19 ;; \
			arm64-v8a) target=aarch64-linux-android; api=21; include=aarch64-linux-android; libdir=aarch64-linux-android/21 ;; \
		esac; \
		abi_dir=$(ANDROID_BUILD)/$$abi; \
		mkdir -p $$abi_dir $(ANDROID_BUILD)/assets $(ANDROID_BUILD)/lib/$$abi; \
		echo "--- Compiling $$abi for Android API $$api ---"; \
		for mod in $(RAYLIB_MODULES); do \
			$(ANDROID_CLANG) -c $(RAYLIB_SRC)/$$mod.c -I$(RAYLIB_SRC) -I$(SRC_DIR)/android -I$(ANDROID_SYSROOT_USR)/include/$$include \
				--target=$$target$$api --sysroot=$(ANDROID_SYSROOT) -DPLATFORM_ANDROID -DGRAPHICS_API_OPENGL_ES2 -fPIC -O2 \
				-o $$abi_dir/$$mod.o; \
		done; \
		$(ANDROID_CLANG) -c $(GLUE_SRC) -I$(SRC_DIR)/android -I$(ANDROID_SYSROOT_USR)/include/$$include \
			--target=$$target$$api --sysroot=$(ANDROID_SYSROOT) -fPIC -O2 -o $$abi_dir/glue.o; \
		for src in $(SRCS); do \
			base=$$(basename $$src .c); \
			$(ANDROID_CLANG) -c $$src -I$(SRC_DIR) -I$(SRC_DIR)/android -I$(RAYLIB_SRC) -I$(ANDROID_SYSROOT_USR)/include/$$include \
				--target=$$target$$api --sysroot=$(ANDROID_SYSROOT) -DPLATFORM_ANDROID -DGRAPHICS_API_OPENGL_ES2 -fPIC -O2 \
				-o $$abi_dir/$$base.o; \
		done; \
		echo "--- Linking $$abi ---"; \
		$(ANDROID_CLANG) -shared -fuse-ld=lld $$abi_dir/*.o \
			--target=$$target$$api --sysroot=$(ANDROID_SYSROOT) \
			-B$(ANDROID_SYSROOT_USR)/lib/$$libdir -L$(ANDROID_SYSROOT_USR)/lib/$$libdir \
			-Wl,-soname,libWatermelonGame.so -Wl,--wrap=fopen -u ANativeActivity_onCreate \
			-llog -landroid -lEGL -lGLESv2 -lOpenSLES -lm -lc \
			-o $(ANDROID_BUILD)/lib/$$abi/libWatermelonGame.so; \
	done
	@echo "--- Packaging APK ---"
	@rm -rf $(ANDROID_BUILD)/assets/Resources
	@cp -r $(SRC_DIR)/Resources $(ANDROID_BUILD)/assets/
	@$(ANDROID_AAPT) package -f -M AndroidManifest.xml -S res -A $(ANDROID_BUILD)/assets -I $(ANDROID_SDK_JAR) -F $(ANDROID_BUILD)/WatermelonGame.unsigned.apk
	@(cd $(ANDROID_BUILD) && $(ANDROID_AAPT) add WatermelonGame.unsigned.apk lib/armeabi-v7a/libWatermelonGame.so lib/arm64-v8a/libWatermelonGame.so)
	@$(ANDROID_ZIPALIGN) -f 4 $(ANDROID_BUILD)/WatermelonGame.unsigned.apk $(ANDROID_BUILD)/WatermelonGame.aligned.apk
	@if [ ! -f $(ANDROID_BUILD)/debug.keystore ]; then \
		$(ANDROID_KEYTOOL) -genkeypair -validity 10000 -dname "CN=Watermelon,O=Game,C=US" \
			-keystore $(ANDROID_BUILD)/debug.keystore -storepass android -keypass android \
			-alias androiddebugkey -keyalg RSA -keysize 2048; \
	fi
	@$(ANDROID_APKSIGNER) sign --ks $(ANDROID_BUILD)/debug.keystore --ks-pass pass:android --key-pass pass:android \
		--ks-key-alias androiddebugkey --out $(TARGET) $(ANDROID_BUILD)/WatermelonGame.aligned.apk
	@echo "Android multi-ABI APK build complete: $(TARGET)"

else
$(TARGET): $(BUILD_DIR) $(SRCS) $(RAYLIB_LINUX_LIB) $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo "Linux build complete: $(TARGET)"
endif

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	cd $(SRC_DIR) && ../$(TARGET)

clean:
	rm -rf build $(BIN_NAME) $(BIN_NAME).exe $(BIN_NAME).apk $(BIN_NAME).apk.idsig

endif
