#define _BSD_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/uinput.h>
#include <sys/stat.h>
#include <systemd/sd-daemon.h>

#define FIFO "/var/run/acpi_fakekey"

static void fail(const char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}


static int create_fifo() {
	int fifo, n;

	n = sd_listen_fds(1);
	if (n > 1)
		fail("sd_listen_fds");
	else if (n == 1) {
		fifo = SD_LISTEN_FDS_START + 0;
		if (!sd_is_fifo(fifo, FIFO))
			fail("sd_is_fifo");
	} else {
		remove(FIFO);
		if (mkfifo(FIFO, 0200) == -1)
			fail("mkfifo");
		if ((fifo = open(FIFO, O_RDWR | O_NONBLOCK)) == -1)
			fail("open fifo");
	}

	return fifo;
}

static int create_uinputdev() {
	int udev, i;
	struct uinput_user_dev dev;

	/* Failure to open dev is not fatal, module might be loaded later */
	udev = open("/dev/uinput", O_WRONLY | O_NDELAY);
	if (udev < 0) {
		perror("failed to open /dev/uinput");
		return -1;
	}

	memset(&dev, 0, sizeof(dev));
	strncpy(dev.name, "ACPI Virtual Keyboard Device", UINPUT_MAX_NAME_SIZE);
	dev.id.version = 4;
	dev.id.bustype = BUS_USB;

	ioctl(udev, UI_SET_EVBIT, EV_KEY);
	for (i = 0; i < 256; i++)
		ioctl(udev, UI_SET_KEYBIT, i);

	if (write(udev, &dev, sizeof(dev)) == -1)
		fail("write");

	if (ioctl(udev, UI_DEV_CREATE) == -1)
		fail("create device");

	return udev;
}

int main(int argc, char **argv) {
	int do_daemonize = 1;
	int udev;
	int fifo;
	fd_set sfd;

	if (argc == 2 && !strcmp(argv[1], "-f"))
		do_daemonize = 0;
	else if (argc > 1)
		fail("invalid arguments");

	udev = create_uinputdev();

	fifo = create_fifo();

	if (do_daemonize && (daemon(0, 0) != 0))
		fail("daemon");

	FD_ZERO(&sfd);
	FD_SET(fifo, &sfd);
	while (select(fifo + 1, &sfd, 0, 0, 0) != -1) {
		int n;
		unsigned char key;
		struct input_event event;

		n = read(fifo, &key, 1);
		if (n < 0)
			break;
		if (n == 0)
			continue;

		/* Try again to create a uinput device if necessary */
		if (udev < 0) {
			udev = create_uinputdev();
			if (udev < 0)
				continue;
		}

		/* Key pressed */
		memset(&event, 0, sizeof(event));
		event.type = EV_KEY;
		event.code = key;
		event.value = 1;
		if (write(udev, &event, sizeof(event)) == -1)
			break;

		/* Key released */
		memset(&event, 0, sizeof(event));
		event.type = EV_KEY;
		event.code = key;
		event.value = 0;
		if (write(udev, &event, sizeof(event)) == -1)
			break;

		/* Sync */
		memset(&event, 0, sizeof(event));
		event.type = EV_SYN;
		event.code = SYN_REPORT;
		event.value = 0;
		if (write(udev, &event, sizeof(event)) == -1)
			break;
	}

	ioctl(udev, UI_DEV_DESTROY);
	close(udev);
	close(fifo);
	remove(FIFO);
	return EXIT_FAILURE;
}
