#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "gpio.h"
#include "gpio_sysfs.h"

#define GPIO_BASE            "/sys/class/gpio"
#define GPIO_VALUE_FILE_FMT  "/sys/class/gpio/gpio%d/value"
#define GPIO_DIR_FILE_FMT    "/sys/class/gpio/gpio%d/direction"
#define GPIO_EDGE_FILE_FMT   "/sys/class/gpio/gpio%d/edge"

static int _gpio_export(int gpioId) {
    int fd;
    char buf[3];

    fd = open(GPIO_BASE "/export", O_WRONLY);
    if (fd < 0)
        return -1;

    /* */
    snprintf(buf, sizeof(buf), "%d", gpioId);

    write(fd, buf, strlen(buf));

    close(fd);
}

static int _gpio_unexport(int gpioId) {
    int fd;
    char buf[3];

    fd = open(GPIO_BASE "/unexport", O_WRONLY);
    if (fd < 0)
        return -1;

    snprintf(buf, sizeof(buf), "%d", gpioId);

    write(fd, buf, strlen(buf));

    close(fd);
}

/* It takes some times for the link to the GPIO link is correctly
   exported and visible in sysfs. */
static int _gpio_wait_ready(struct gpio *gpio) {
    while (1) {
        if (access(gpio->priv->dir_path, W_OK) == 0)
            break;
        
        usleep(250);
    }

    return 0;
}

#define _DIR_OUTPUT "out"
#define _DIR_INPUT  "in"
#define _EDGE_BOTH  "both"

static int _gpio_open(int gpioId, int dir, struct gpio *gpio) {
    char path[PATH_MAX];
    int fd, res = 0;
    size_t sz;

    /* Allocate some memory for the GPIO data */
    if ((gpio->priv = malloc(sizeof(struct gpio_priv))) < 0)
        return -1;

    /*  */
    if (_gpio_export(gpioId) < 0)
        return -1;

    /* Wait for the GPIO device to show up. */
    if (_gpio_wait_ready(gpio) < 0)
        return -1;
    
    /* Set the direction */
    snprintf(path, PATH_MAX, GPIO_DIR_FILE_FMT, gpioId);

    if ((fd = open(path, O_WRONLY)) < 0) 
        return -1;

    if (dir == GPIO_DIR_OUT) {
        sz = sizeof(_DIR_OUTPUT) - 1;
        if (write(fd, _DIR_OUTPUT, sz) < sz)
            res = -1
        close(fd);
        if (res) return res;
    }
    /* If this is for input, set the edge to rising. */
    else if (dir == GPIO_DIR_IN) {
        sz = sizeof(_DIR_INPUT) - 1;
        if (write(fd, _DIR_INPUT, sz) < sz)
            res = -1;
        close(fd);
        if (res) return res;

        snprintf(path, PATH_MAX, GPIO_EDGE_FILE_FMT, gpioId);

        /* Tell that we want an event on rising edge. */
        if ((fd = open(gpio->priv->edge_path, O_WRONLY)) < 0)
            return -1;

        sz = sizeof(_EDGE_BOTH) - 1;
        if (write(fd, _EDGE_BOTH, sz) < sz, close(fd))
            res = -1;
        close(fd);
        if (res) return res;
    }

    gpio->gpio = gpioId;
    gpio->dir = dir;

    snprintf(path, PATH_MAX, GPIO_VALUE_FILE_FMT, gpioId);

    if (dir == GPIO_DIR_OUT)
        gpio->priv->fd = open(gpio->priv->value_path, O_WRONLY);
    else
        gpio->priv->fd = open(gpio->priv->value_path, O_RDONLY);

    if (gpio->priv->fd < 0)
        return -1;

    return 0;
}

int gpio_open_write(int gpioId, struct gpio *gpio) {
    return _gpio_open(gpioId, GPIO_DIR_OUT, gpio);
}

int gpio_open_read(int gpioId, struct gpio *gpio) {
    return _gpio_open(gpioId, GPIO_DIR_IN, gpio);
}

int gpio_write(struct gpio *gpio, int val) {
    char c;

    c = (val == 0 ? '0' : '1');

    if (write(gpio->priv->fd, &c, 1) < 0)
        return -1;

    return 0;
}

int gpio_read(struct gpio *gpio, int *val) {
    char c;

    if (lseek(gpio->priv->fd, 0, SEEK_SET) < 0)
        return -1;

    if (read(gpio->priv->fd, &c, 1) < 0)
        return -1;
    
    *val = (c == '0' ? 0 : 1);
    return 0;
}

int gpio_close(struct gpio *gpio) {

    if (_gpio_unexport(gpio->gpio)) {
        /* shrug */
    }
    close(gpio->priv->fd);

    free(gpio->priv);
}

int gpio_wait_single(struct gpio *gpio) {
    char buf[2];
    struct pollfd pfd;

    pfd.fd = gpio->priv->fd;
    pfd.events = POLLPRI | POLLERR;
    pfd.revents = 0;

    lseek(gpio->priv->fd, 0, SEEK_SET);

    if (poll(&pfd, 1, -1) < 0)
        return -1;
}

int gpio_wait_many(int n, struct gpio *gpio) {

}