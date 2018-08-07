#include <stdio.h>
#include <unistd.h>
#include <libudev.h>
#include <sys/select.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pwd.h>

static struct udev *s_udev;
static struct udev_monitor *s_udev_mon;
static char s_user_home[4096] = { 0 };

static void usrpath(char *p, const char *m) {
    strcpy(p, s_user_home);
    strcat(p, "/");
    strcat(p, m);
}

int usb_exec_hook(const char *name, const char *const *args, int argc) {
    char path[4096];
    usrpath(path, ".config/usbnotify/hook-");
    strcat(path, name);
    struct stat s;
    if (stat(path, &s) != 0) {
        return -1;
    }

    pid_t p = fork();

    if (p < 0) {
        return -1;
    }

    if (p == 0) {
        // Child
        const char *argv[argc + 2];
        argv[0] = strdup(path);
        argv[argc + 1] = NULL;
        for (int i = 0; i < argc; ++i) {
            argv[i + 1] = strdup(args[i]);
        }

        execv(path, (char *const *) argv);

        exit(-1);
    }

    return 0;
}

void usb_device_added(const char *name, const char *path) {
    syslog(LOG_NOTICE, "usb_device_added: name = %s, path = %s", name, path);

    const char *const args[] = {
        name,
        path
    };
    usb_exec_hook("add", args, sizeof(args) / sizeof(args[0]));
}

void usb_device_removed(const char *name, const char *path) {
    syslog(LOG_NOTICE, "usb_device_removed: name = %s, path = %s", name, path);

    const char *const args[] = {
        name,
        path
    };
    usb_exec_hook("remove", args, sizeof(args) / sizeof(args[0]));
}

int usb_event_loop(void) {
    struct udev_device *dev;
    int r;
    int fd;

    udev_monitor_enable_receiving(s_udev_mon);
    fd = udev_monitor_get_fd(s_udev_mon);

    while (1) {
        fd_set fds;
        struct timeval tv;
        
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        switch ((r = select(fd + 1, &fds, NULL, NULL, &tv))) {
        case -1:
            fprintf(stderr, "select() returned -1\n");
        case 0:
            break;
        default:
            if (FD_ISSET(fd, &fds)) {
                dev = udev_monitor_receive_device(s_udev_mon);

                if (dev) {
                    const char *action = udev_device_get_action(dev);
                    if (!strcmp(action, "add")) {
                        usb_device_added(udev_device_get_sysname(dev), udev_device_get_devpath(dev));
                    } else if (!strcmp(action, "remove")) {
                        usb_device_removed(udev_device_get_sysname(dev), udev_device_get_devpath(dev));
                    }

                    udev_device_unref(dev);
                }
            }
            break;
        }
    }

    return 0;
}

static int get_user_home(void) {
    uid_t uid = getuid();
    struct passwd pwd;
    struct passwd *res;
    char buf[16 * 1024];

    if (getpwuid_r(uid, &pwd, buf, sizeof(buf), &res) != 0 || res == NULL) {
        return -1;
    }

    strcpy(s_user_home, res->pw_dir);

    return 0;
}

void daemon_main(void) {
    setsid();

    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    freopen("/dev/null", "r", stdin);

    openlog("usbnotify", 0, LOG_USER);
    s_udev = udev_new();
    if (!s_udev) {
        fprintf(stderr, "Can't create udev\n");
        return;
    }
    s_udev_mon = udev_monitor_new_from_netlink(s_udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(s_udev_mon, "block", NULL);
    usb_event_loop();
    udev_unref(s_udev);
    closelog();
}

int main(int argc, char **argv) {
    if (get_user_home() != 0) {
        fprintf(stderr, "Failed to get user's home directory\n");
        return -1;
    }

    pid_t pid = fork();

    if (pid == 0) {
        daemon_main();
        return 0;
    }
    if (pid == -1) {
        fprintf(stderr, "Failed to fork()\n");
        return -1;
    } else {
        printf("Started usbnotify with pid %d\n", pid);
    }

    return 0;
}
