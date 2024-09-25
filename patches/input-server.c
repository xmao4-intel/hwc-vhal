#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <linux/uinput.h>

#define INPUT_SERVER_PORT  9900

int create_multitouch(int w, int h) {
    int fd;
    struct uinput_user_dev uidev;
    struct input_event ev;
    int x, y;

    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("Failed to open uinput");
        return -1;
    }

    // Enable multi-touch events
    if ((ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0) ||
        (ioctl(fd, UI_SET_ABSBIT, ABS_MT_SLOT) < 0) ||
        (ioctl(fd, UI_SET_ABSBIT, ABS_MT_PRESSURE) < 0) ||
        (ioctl(fd, UI_SET_ABSBIT, ABS_MT_TOUCH_MAJOR) < 0) ||
        (ioctl(fd, UI_SET_ABSBIT, ABS_MT_WIDTH_MAJOR) < 0) ||
        (ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X) < 0) ||
        (ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y) < 0) ||
        (ioctl(fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID) < 0) ||
        (ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT) < 0) ||
        (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) ||
        (ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH) < 0)
    ) {
        perror("Failed to setup touch device by ioctl");
        close(fd);
        return -1;
    }

    // enable all key events
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) {
        perror("Failed to setup touch device by ioctl");
        close(fd);
        return -1;        
    }

    // Enable all keys in the input subsystem
    for (int i = 0; i < KEY_MAX; i++) {
        if (ioctl(fd, UI_SET_KEYBIT, i) < 0) {
            perror("Failed to enable keybit");
        }
    }

    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-multitouch");
    uidev.id.bustype = BUS_VIRTUAL;
    uidev.id.vendor = 0x8086;
    uidev.id.product = 0x1111;
    uidev.id.version = 1;

    uidev.absmin[ABS_MT_SLOT] = 0;
    uidev.absmax[ABS_MT_SLOT] = 9; 
    uidev.absmin[ABS_MT_POSITION_X] = 0;
    uidev.absmax[ABS_MT_POSITION_X] = w;
    uidev.absmin[ABS_MT_POSITION_Y] = 0;
    uidev.absmax[ABS_MT_POSITION_Y] = h;
    uidev.absmin[ABS_MT_TRACKING_ID] = 0;
    uidev.absmax[ABS_MT_TRACKING_ID] = 65535;

    if (write(fd, &uidev, sizeof(uidev)) < 0) {
        perror("Failed to write uinput device properties");
        close(fd);
        return -1;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("Failed to create uinput device");
        close(fd);
        return -1;
    }
    return fd;
}

int create_touch(int w, int h) {
    int fd;
    struct uinput_user_dev uidev;
    struct input_event ev;
    int x, y;

    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("Failed to open uinput");
        return -1;
    }

    if ((ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) ||
        (ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH) < 0) ||
        (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0) ||
        (ioctl(fd, UI_SET_ABSBIT, ABS_X) < 0) ||
        (ioctl(fd, UI_SET_ABSBIT, ABS_Y) < 0)
    ) {
        perror("Failed to setup touch device by ioctl");
        close(fd);
        return -1;
    }

    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "vtouch");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x8086;
    uidev.id.product = 0x1111;
    uidev.id.version = 1;

    uidev.absmin[ABS_X] = 0;
    uidev.absmax[ABS_X] = w;
    uidev.absmin[ABS_Y] = 0;
    uidev.absmax[ABS_Y] = h;

    if (write(fd, &uidev, sizeof(uidev)) < 0) {
        perror("Failed to write uinput device properties");
        close(fd);
        return -1;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("Failed to create uinput device");
        close(fd);
        return -1;
    }
    return fd;    
}

int create_server(int port) {
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to create socket");
        return -1;
    }

    // reuse address port
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind address");
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char* argv[]) {
    int input_fd = create_multitouch(32767, 32767);
    if (input_fd < 0) {
        perror("Failed to create input");
        return -1;
    }
    printf("input device fd=%d\n", input_fd);

    int server_fd = create_server(INPUT_SERVER_PORT);
    if (server_fd < 0) {
        return -1;
    }
    printf("input server fd=%d\n", server_fd);

    if (listen(server_fd, 1) < 0) {
        perror("Failed to listen on server");
        close(server_fd);
        return -1;
    }

    struct sockaddr_in addr;
    int client_fd = -1;
    int addrlen;
    char buf[1024];
    ssize_t sz;

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&addr, (socklen_t*)&addrlen);
        printf("New connection fd = %d\n", client_fd);
        while (1) {
            sz = read(client_fd, buf, 1024);
            if (sz <= 0) {
                printf("read return %zd, disconnected\n", sz);
                close(client_fd);
                break;
            }
            printf("read %zd bytes from cleint\n", sz);
            sz = write(input_fd, buf, sz);
            if (sz < 0) {
                perror("Failed to write event to input");
            }
        }
    }

    return 0;
}