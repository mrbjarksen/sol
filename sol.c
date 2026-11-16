#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <libcec/cecc.h>

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t cancel_requested;

static double monotonic_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0.0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static void on_signal(int signal_number) {
    (void)signal_number;
    stop_requested = 1;
}

static double srgb_to_linear(double value) {
    return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
}

static double linear_to_srgb(double value) {
    return value <= 0.0031308 ? value * 12.92
                              : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

static void dawn_rgb(double progress, unsigned short *r, unsigned short *g,
                     unsigned short *b) {
    static const double colors[][3] = {
        {0, 0, 0},     {4, 1, 16},     {35, 8, 45},    {150, 25, 35},
        {235, 85, 35}, {255, 185, 80}, {255, 238, 180}};
    static const double stops[] = {0.0, 0.10, 0.24, 0.43, 0.65, 0.83, 1.0};

    if (progress < 0)
        progress = 0;
    if (progress > 1)
        progress = 1;

    const int color_count = (int)(sizeof(colors) / sizeof(colors[0]));
    int index = color_count - 2;
    for (int i = 0; i < color_count - 1; ++i) {
        if (progress <= stops[i + 1]) {
            index = i;
            break;
        }
    }
    double span = stops[index + 1] - stops[index];
    double fraction = (progress - stops[index]) / span;

    double red_a = srgb_to_linear(colors[index][0] / 255.0);
    double red_b = srgb_to_linear(colors[index + 1][0] / 255.0);
    double green_a = srgb_to_linear(colors[index][1] / 255.0);
    double green_b = srgb_to_linear(colors[index + 1][1] / 255.0);
    double blue_a = srgb_to_linear(colors[index][2] / 255.0);
    double blue_b = srgb_to_linear(colors[index + 1][2] / 255.0);

    *r = (unsigned short)(linear_to_srgb(red_a * (1.0 - fraction) +
                                         red_b * fraction) *
                          65535.0);
    *g = (unsigned short)(linear_to_srgb(green_a * (1.0 - fraction) +
                                         green_b * fraction) *
                          65535.0);
    *b = (unsigned short)(linear_to_srgb(blue_a * (1.0 - fraction) +
                                         blue_b * fraction) *
                          65535.0);
}

static int show_dawn(unsigned int dur_sec) {
    Display *display = XOpenDisplay(NULL);
    double started = monotonic_seconds();

    if (!display) {
        fprintf(stderr,
                "Cannot open X display (is DISPLAY set and X running?)\n");
        return -1;
    }

    Window root = DefaultRootWindow(display);
    XWindowAttributes attributes;
    XGetWindowAttributes(display, root, &attributes);
    unsigned int width = attributes.width;
    unsigned int height = attributes.height;

    Window window =
        XCreateSimpleWindow(display, root, 0, 0, width, height, 0,
                            BlackPixel(display, 0), BlackPixel(display, 0));

    Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom wm_fullscreen =
        XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    Atom wm_bypass_compositor =
        XInternAtom(display, "_NET_WM_BYPASS_COMPOSITOR", False);

    XSetWindowAttributes window_attributes = {.override_redirect = True};
    XChangeWindowAttributes(display, window, CWOverrideRedirect,
                            &window_attributes);

    XChangeProperty(display, window,
                    XInternAtom(display, "_NET_WM_WINDOW_TYPE", False), XA_ATOM,
                    32, PropModeReplace,
                    (unsigned char *)&(Atom){XInternAtom(
                        display, "_NET_WM_WINDOW_TYPE_NORMAL", False)},
                    1);
    XChangeProperty(display, window, wm_state, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&wm_fullscreen, 1);
    XChangeProperty(display, window, wm_bypass_compositor, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&(long){1}, 1);

    XSelectInput(display, window,
                 ExposureMask | StructureNotifyMask | KeyPressMask);
    XMapRaised(display, window);
    XMoveResizeWindow(display, window, 0, 0, width, height);
    XGrabKeyboard(display, window, True, GrabModeAsync, GrabModeAsync,
                  CurrentTime);

    GC gc = XCreateGC(display, window, 0, NULL);
    Colormap colormap = DefaultColormap(display, 0);

    unsigned short red, green, blue;
    unsigned long current_pixel = 0;
    int have_current_pixel = 0;
    char name[32];

    while (!stop_requested) {
        double elapsed = monotonic_seconds() - started;
        double progress = dur_sec ? elapsed / (double)dur_sec : 1.0;

        if (elapsed >= (double)dur_sec)
            break;

        dawn_rgb(progress, &red, &green, &blue);

        XColor color = {
            .red = red,
            .green = green,
            .blue = blue,
            .flags = DoRed | DoGreen | DoBlue,
        };

        snprintf(name, sizeof(name), "#%02x%02x%02x", red / 257, green / 257,
                 blue / 257);
        if (XParseColor(display, colormap, name, &color) &&
            XAllocColor(display, colormap, &color)) {
            if (have_current_pixel)
                XFreeColors(display, colormap, &current_pixel, 1, 0);
            current_pixel = color.pixel;
            have_current_pixel = 1;
            XSetWindowBackground(display, window, current_pixel);
        }

        XClearWindow(display, window);
        XFlush(display);

        XEvent event;
        while (XPending(display)) {
            XNextEvent(display, &event);
            if (event.type == KeyPress &&
                XLookupKeysym(&event.xkey, 0) == XK_Escape) {
                stop_requested = 1;
                cancel_requested = 1;
            }
        }

        struct timespec req = {.tv_sec = 0, .tv_nsec = 16000000};
        struct timespec rem;

        nanosleep(&req, &rem);
    }

    if (have_current_pixel)
        XFreeColors(display, colormap, &current_pixel, 1, 0);
    XUngrabKeyboard(display, CurrentTime);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}

int main(int argc, char **argv) {
    unsigned int dur_min = 30;

    if (argc > 2) {
        fprintf(stderr, "Too many arguments");
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        unsigned long value = strtoul(argv[1], NULL, 10);
        dur_min = (unsigned int)value;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    libcec_configuration configuration;
    libcec_clear_configuration(&configuration);
    configuration.deviceTypes.types[0] = CEC_DEVICE_TYPE_RECORDING_DEVICE;

    libcec_connection_t adapter = libcec_initialise(&configuration);
    if (!adapter) {
        fprintf(stderr, "libcec initialisation failed\n");
        return EXIT_FAILURE;
    }

    cec_adapter adapters[10];
    int adapter_count = libcec_find_adapters(adapter, adapters, 10, NULL);
    if (adapter_count < 1 || !libcec_open(adapter, adapters[0].comm, 5000)) {
        fprintf(stderr, "No usable HDMI-CEC adapter found\n");
        libcec_destroy(adapter);
        return EXIT_FAILURE;
    }

    libcec_power_on_devices(adapter, CECDEVICE_TV);

    int exit_status = EXIT_SUCCESS;
    if (show_dawn(dur_min * 60U) != 0)
        exit_status = EXIT_FAILURE;

    if (!cancel_requested)
        libcec_standby_devices(adapter, CECDEVICE_TV);

    libcec_close(adapter);
    libcec_destroy(adapter);
    return exit_status;
}
