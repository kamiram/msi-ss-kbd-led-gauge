#include <stdio.h>
#include <hidapi/hidapi.h>
#include <unistd.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xlibint.h>
#include <err.h>
#include <math.h>

const int VENDOR_ID = 0x1770;
const int PRODUCT_ID = 0xff00;

pthread_mutex_t usbmutex;

struct Config{
    int temp_low;
    int temp_high;
    int intensivity;
    int delay;
    char *sensors_path;
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

int set_rgb_color(enum Region region, unsigned char red, unsigned char green, unsigned char blue) {
    struct {
        unsigned char v1, v2, op;
        unsigned char region;
        unsigned char red, green, blue;
        unsigned char end;
    } kbd_operation = {1, 2, 64, 0, 0, 0, 0, 236};

    static hid_device *kbd = NULL;

    int result = 0;

    kbd_operation.region = region;
    kbd_operation.red = red;
    kbd_operation.green = green;
    kbd_operation.blue = blue;

    pthread_mutex_lock(&usbmutex);
    if (!kbd) {
        kbd = hid_open(VENDOR_ID, PRODUCT_ID, NULL);
        if (!kbd) {
            fprintf(stderr, "cannot open usb device");
             exit(1);
        }
    }

    result = hid_send_feature_report(kbd, (const unsigned char*)&kbd_operation, sizeof(kbd_operation));
    if (result < sizeof(kbd_operation)) {
        printf("reconnect usb");
        if (kbd) {
            hid_close(kbd);
            kbd = NULL;
        }
        pthread_mutex_unlock(&usbmutex);
        return -1;
    }

    pthread_mutex_unlock(&usbmutex);
    return 1;
}

void *temp_display(void *param) {
    char *src = (char *) param;
    FILE *f;
    int temp;
    double prc, old_prc = -1.0;
    unsigned char r, g, b;

    struct Config *config = param;

    while (1) {
        f = fopen(config->sensors_path, "r");
        if (!f) {
            fprintf(stderr, "cannot read temp '%s'", src);
            exit(1);
        }
        fscanf(f, "%d", &temp);
        fclose(f);
        temp = temp / 1000;
        if (temp <= config->temp_low) {
            temp = config->temp_low + 1;
        }

        if (temp > config->temp_high) {
            temp = config->temp_high;
        }

        prc = (double) (temp - config->temp_low) / (double) (config->temp_high - config->temp_low);
        if(fabs(prc - old_prc) > 0.01){
            old_prc = prc;
            r = (unsigned char) (config->intensivity * (prc));
            g = (unsigned char) (config->intensivity * (1.0 - prc));
            b = 0;
            set_rgb_color(REGION_RIGHT, r, g, b);
            // printf("temp: %5d %3.0f%% %3u %3u %3u \n", temp, prc*100, r, g, b);
        }

        usleep(config->delay);
    }
}

int main() {
    const unsigned char group_colors[3][3] ={
            {32, 16, 0},
            {127, 0, 127},
            {0, 0, 128},
    };

    struct Config config = {60, 85, 64, 1000, NULL};

    pthread_t tid;
    pthread_attr_t attr;

    Display *dpy;
    XkbEvent ev;

    int xkbEventType, xkbError, reason_rtrn, mjr, mnr;
    char *display_name = NULL;
    
    int group_index, old_group_index = -1;
    
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
    config.sensors_path = "/sys/class/hwmon/hwmon0/temp1_input";
    pthread_create(&tid, &attr, temp_display, &config);

    if(pthread_mutex_init(&usbmutex, NULL)){
        printf("Error while using pthread_mutex_init\n");
    }

    XkbSelectEventDetails(dpy, XkbUseCoreKbd, XkbStateNotify, XkbAllStateComponentsMask, XkbGroupStateMask);
    while (1) {
        XNextEvent(dpy, &ev.core);
        if (ev.any.xkb_type == XkbStateNotify) {
            group_index = ev.state.locked_group;
            if(old_group_index != group_index) {
                old_group_index = group_index;
                set_rgb_color(REGION_LEFT, group_colors[group_index][0], group_colors[group_index][1], group_colors[group_index][2]);
                set_rgb_color(REGION_MIDDLE, group_colors[group_index][0], group_colors[group_index][1], group_colors[group_index][2]);
            }

        }
    }
}
