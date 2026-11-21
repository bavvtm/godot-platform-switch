#include "os_switch.h"
#include "switch_wrap.h"

#include "main/main.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <zlib.h>

#include <cstring>

#define FB_WIDTH 1280
#define FB_HEIGHT 720

int main(int argc, char *argv[]) {
	csrngInitialize();
	socketInitializeDefault();
	if (nxlinkStdio() < 0) {
		const char *arg0 = (argc > 0 && argv) ? *argv : nullptr;
		nx::init_file_logging_from_argv(arg0);
	} else {
		setvbuf(stdout, nullptr, _IONBF, 0);
		setvbuf(stderr, nullptr, _IONBF, 0);
	}
	romfsInit();

	int apptype = appletGetAppletType();
	if (apptype != AppletType_Application && apptype != AppletType_SystemApplication) {
		NWindow *win = nwindowGetDefault();
		Framebuffer fb;
		framebufferCreate(&fb, win, FB_WIDTH, FB_HEIGHT, PIXEL_FORMAT_RGBA_8888, 2);
		framebufferMakeLinear(&fb);

		u32 stride;
		u32 *framebuf = (u32 *)framebufferBegin(&fb, &stride);

		FILE *splash = fopen("romfs:/applet_splash.rgba.z", "rb");
		if (splash) {
			fseek(splash, 0, SEEK_END);
			size_t splash_size = ftell(splash);
			rewind(splash);

			u8 *compressed_splash = (u8 *)malloc(splash_size);
			if (compressed_splash) {
				fread(compressed_splash, 1, splash_size, splash);

				memset(framebuf, 0, stride * FB_HEIGHT);

				z_stream stream;
				memset(&stream, 0, sizeof(stream));
				stream.next_in = compressed_splash;
				stream.avail_in = splash_size;
				stream.next_out = (u8 *)framebuf;
				stream.avail_out = stride * FB_HEIGHT;

				if (inflateInit2(&stream, MAX_WBITS) == Z_OK) {
					int err = inflate(&stream, Z_FINISH);
					if (err != Z_STREAM_END) {
						TRACE("Failed to decompress splash screen. Zlib error: %d\n", err);
					}
					inflateEnd(&stream);
				}
				free(compressed_splash);
			}
			fclose(splash);
		} else {
			TRACE("Splash screen romfs:/applet_splash.rgba.z not found.");
		}

		framebufferEnd(&fb);

		PadState pad;
		padConfigureInput(1, HidNpadStyleSet_NpadStandard);
		padInitializeDefault(&pad);

		while (appletMainLoop()) {
			padUpdate(&pad);
			u64 kDown = padGetButtonsDown(&pad);
			if (kDown != 0) {
				break;
			}
		}

		framebufferClose(&fb);
		romfsExit();
		socketExit();
		csrngExit();
		return 0;
	}

	int exit_code = EXIT_SUCCESS;
#if defined(DEBUG_ENABLED) || defined(ENABLE_NXLINK)
	uint64_t last_ms;
#endif
	{
#if defined(DEBUG_ENABLED) || defined(ENABLE_NXLINK)
		last_ms = nx::millis();
		TRACE("[BOOT] %llu ms: Before Main::setup", (unsigned long long)last_ms);
#endif

		OS_Switch os;
		os.set_executable_path(argv[0]);

		const Result boost = appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);
		printf("CPU-Boost (1785 MHz) at main start: %s\n",
		    R_SUCCEEDED(boost) ? "success" : "invalid");

		char **setup_argv = nullptr;
		int setup_argc = nx::build_setup_args(argc, argv, &setup_argv);

		Error err = Main::setup(argv[0], setup_argc, setup_argv);

#if defined(DEBUG_ENABLED) || defined(ENABLE_NXLINK)
		TRACE("[BOOT] After Main::setup (+%llu ms)", (unsigned long long)(nx::millis() - last_ms));
		last_ms = nx::millis();
#endif

		if (err != OK) {
			appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
			exit_code = (err == ERR_HELP) ? EXIT_SUCCESS : EXIT_FAILURE;
		} else {
			int start_ret = Main::start();
			appletSetCpuBoostMode(ApmCpuBoostMode_Normal);

			if (start_ret == EXIT_SUCCESS) {
#if defined(DEBUG_ENABLED) || defined(ENABLE_NXLINK)
				TRACE("[BOOT] After Main::start (+%llu ms)", (unsigned long long)(nx::millis() - last_ms));
#endif
				os.run();
				Main::cleanup();

				exit_code = os.get_exit_code();
			} else {
				exit_code = EXIT_FAILURE;
			}
		}

		free(setup_argv);
	}

	romfsExit();
	socketExit();
	csrngExit();
	return exit_code;
}
