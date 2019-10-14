#include <stdio.h>
#include <hidapi/hidapi.h>
#include <unistd.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/Xlibint.h>
#include <X11/Xatom.h>
#include <err.h>
#include <math.h>

const int VENDOR_ID = 0x1770;
const int PRODUCT_ID = 0xff00;

enum Operation {
    SET_COLOR = 66,
    SET_MODE = 65,
    SET_RGB = 64
};

const int TEMP_LOW = 62000;
const int TEMP_HIGH = 85000;

#define BUFF_SIZE 8

int group_index = 0;
unsigned char group_colors[3][3] ={
        {32, 16, 0},
         {127, 0, 127},
        {0, 0, 128},
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

int set_rgb_color(enum Region region, unsigned char red, unsigned char green, unsigned char blue) {
    static union {
        struct {
            unsigned char v1, v2, op;
            union {
                struct {
                    unsigned char mode;
                };
                struct{
                    unsigned char region;

                    union {
                        struct {
                            unsigned char color, intensity;
                        };
                        struct {
                            unsigned char red, green, blue;
                        };
                    };
                };
            };
            unsigned char end;
        };
        unsigned char buff[BUFF_SIZE];
    } kbd_operation = {1, 2, 0, 0, 236};

    static hid_device *kbd = NULL;

    int result = 0;

    kbd_operation.op = SET_RGB;
    kbd_operation.region = region;
    kbd_operation.red = red;
    kbd_operation.green = green;
    kbd_operation.blue = blue;

    if (!kbd) {
        kbd = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
        if (!kbd) {
            fprintf(stderr, "cannot open usb device");
            exit(1);
        }
    }

    result = hid_send_feature_report(kbd, kbd_operation.buff, BUFF_SIZE);
    if (result < BUFF_SIZE) {
        printf("reconnect usb");
        if (kbd) {
            hid_close(kbd);
        }
        kbd = NULL;
        return set_rgb_color(region, red, green, blue);
    }

    return 1;
}

void *temp_display(void *param) {
    char *src = (char *) param;
    FILE *f;
    int temp;

    static int old_group_index = -1;

    float prc, old_prc = 0;
    unsigned char r, g, b;

    while (1) {
        f = fopen(src, "r");
        if (!f) {
            fprintf(stderr, "cannot read temp '%s'", src);
            exit(1);
        }
        fscanf(f, "%d", &temp);
        fclose(f);

        if (temp <= TEMP_LOW) {
            temp = TEMP_LOW + 1;
        }

        if (temp > TEMP_HIGH) {
            temp = TEMP_HIGH;
        }

        prc = (float) (temp - TEMP_LOW) / (float) (TEMP_HIGH - TEMP_LOW);
        if(fabs(prc - old_prc) > 0.01){
            old_prc = prc;
            r = (unsigned char) (64.0 * (prc - 0.02));
            g = (unsigned char) (64.0 * (1.0 - prc));
            b = 0;
            set_rgb_color(REGION_RIGHT, r, g, b);
//            printf("%.3f%% %3u %3u %3u \n", prc, r, g, b);
        }

        if(old_group_index != group_index) {
            old_group_index = group_index;
            set_rgb_color(REGION_LEFT, group_colors[group_index][0], group_colors[group_index][1], group_colors[group_index][2]);
            set_rgb_color(REGION_MIDDLE, group_colors[group_index][0], group_colors[group_index][1], group_colors[group_index][2]);
        }

        usleep(500);
    }
}

int main() {
    pthread_t tid;
    pthread_attr_t attr;

    Display *dpy;
    XkbEvent ev;

    int xkbEventType, xkbError, reason_rtrn, mjr, mnr;
    char *display_name = NULL;

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

    pthread_attr_init(&attr);
    pthread_create(&tid, &attr, temp_display, "/sys/class/hwmon/hwmon0/temp1_input");

    XkbSelectEventDetails(dpy, XkbUseCoreKbd, XkbStateNotify, XkbAllStateComponentsMask, XkbGroupStateMask);
    while (1) {
        XNextEvent(dpy, &ev.core);
        if (ev.any.xkb_type == XkbStateNotify) {
            group_index = ev.state.locked_group;
        }
    }
}
