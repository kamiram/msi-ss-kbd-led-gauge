#include <stdio.h>
#include <hidapi/hidapi.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#include <err.h>

#define VENDOR_ID 0x1770
#define PRODUCT_ID 0xff00

#define SET_COLOR 66
#define SET_MODE 65
#define SET_RGB 64


#define BUFF_SIZE 8

enum Mode {
    MODE_NORMAL = 1,
    MODE_GAMING = 2,
    MODE_BREATHE = 3,
    MODE_DEMO = 4,
    MODE_WAVE = 5
};

enum Region {
    REGION_LEFT = 1,
    REGION_MIDDLE = 2,
    REGION_RIGHT = 3,
    LOGO = 4,
    FRL_LEFT = 5,
    FRL_RIGHT = 6,
    TOUCHPAD = 7
};

enum Color {
    COLOR_OFF = 0,
    COLOR_RED = 1,
    COLOR_ORANGE = 2,
    COLOR_YELLOW = 3,
    COLOR_GREEN = 4,
    COLOR_SKY = 5,
    COLOR_BLUE = 6,
    COLOR_PURPLE = 7,
    COLOR_WHITE = 8
};

enum Intensity {
    INTENSITY_HIGH = 0,
    INTENSITY_MEDIUM = 1,
    INTENSITY_LOW = 2,
    INTENSITY_LIGHT = 3
};

const unsigned char colors[] = {COLOR_RED, COLOR_YELLOW, COLOR_GREEN};

union {
    struct{
        unsigned char v1, v2, op;
        union{
            struct{
                unsigned char mode;
            };
            struct{
                unsigned char region, color, intensity;
            };
        };
        unsigned char v0, end;
    };
    unsigned char buff[BUFF_SIZE];
} kbd_operation;

hid_device* kbd = NULL;

int set_color(enum Mode mode, enum Region region, enum Color color, enum Intensity intensity){
    int result = 0;

    kbd_operation.op = SET_COLOR;
    kbd_operation.region = region;
    kbd_operation.color = color;
    kbd_operation.intensity = intensity;

    if(!kbd){
        kbd = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
        if(!kbd) {
            fprintf(stderr, "cannot open usb device");
            exit(1);
        }
    }

    result = hid_send_feature_report(kbd, kbd_operation.buff, BUFF_SIZE);
    if(result < BUFF_SIZE){
        printf("reconnect usb");
        if(kbd){
            hid_close(kbd);
        }
        kbd = NULL;
        return set_color(mode, region, color, intensity);
    }

    kbd_operation.op = SET_MODE;
    kbd_operation.mode = mode;
    hid_send_feature_report(kbd, kbd_operation.buff, BUFF_SIZE);

    return 1;
}

int main() {

    kbd_operation.v1 = 1;
    kbd_operation.v2 = 2;
    kbd_operation.v0 = 0;
    kbd_operation.end = 236;

    Display *dpy;
    XkbEvent ev;
    int  xkbEventType, xkbError, reason_rtrn, mjr, mnr;

    char* display_name = NULL;
    mjr = XkbMajorVersion;
    mnr = XkbMinorVersion;
    dpy = XkbOpenDisplay(display_name, &xkbEventType, &xkbError, &mjr, &mnr, &reason_rtrn);

    if (dpy == NULL) {
        warnx("Can't open display named %s", XDisplayName(display_name));
        switch (reason_rtrn) {
            case XkbOD_BadLibraryVersion :
            case XkbOD_BadServerVersion :
                warnx("xxkb was compiled with XKB version %d.%02d", XkbMajorVersion, XkbMinorVersion);
                warnx("But %s uses incompatible version %d.%02d",
                      reason_rtrn == XkbOD_BadLibraryVersion ? "Xlib" : "Xserver",
                      mjr, mnr);
                break;

            case XkbOD_ConnectionRefused :
                warnx("Connection refused");
                break;

            case XkbOD_NonXkbServer:
                warnx("XKB extension not present");
                break;

            default:
                warnx("Unknown error %d from XkbOpenDisplay", reason_rtrn);
                break;
        }
        exit(1);
    }

    set_color(MODE_NORMAL, REGION_LEFT, COLOR_RED, INTENSITY_HIGH);
    set_color(MODE_NORMAL, REGION_MIDDLE, COLOR_RED, INTENSITY_HIGH);
    set_color(MODE_NORMAL, REGION_RIGHT, COLOR_RED, INTENSITY_HIGH);

    XkbSelectEventDetails(dpy, XkbUseCoreKbd, XkbStateNotify, XkbAllStateComponentsMask, XkbGroupStateMask);
    while(1) {
        XNextEvent(dpy, &ev.core);
        if(ev.any.xkb_type == XkbStateNotify) {
            set_color(MODE_NORMAL, REGION_LEFT, colors[ev.state.locked_group], INTENSITY_HIGH);
            set_color(MODE_NORMAL, REGION_MIDDLE, colors[ev.state.locked_group], INTENSITY_HIGH);
            set_color(MODE_NORMAL, REGION_RIGHT, colors[ev.state.locked_group], INTENSITY_HIGH);
        }
    }
}
