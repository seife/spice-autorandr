/*
 * Copyright (C) 2017-2024 Stefan Seyfried <seife+dev@b1-systems.com>
 *
 * License: MIT
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * spice-autorandr
 * Listens to RRNotify_OutputChange events and then adjusts current mode.
 * Also listens to udev for "drm" subsystem events and then adjusts current mode.
 * => udev mode was inspired by https://github.com/seife/spice-autorandr/issues/1
 *
 * Intened to be used in KVM guests with spice drivers and virt-manager's
 * "Auto Resize VM with Window" setting ticked.
 *
 * Ideas taken from xrandr and xev source code.
 * XNextEvent timeout code inspired by
 *   https://nrk.neocities.org/articles/x11-timeout-with-xsyncalarm
 * Udev code inspired by https://github.com/robertalks/udev-examples
 *
 * CAVEAT: this will fail to work, if the "SPICE changed mode" one day is
 * no longer the first on the modes list seen with "xrandr -q".
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>
#include <poll.h>
#include <libudev.h>

static const char *myname;
static Display *dpy;
static Window root;
static int debug;

static const char *portdev = "/dev/virtio-ports/com.redhat.spice.0";

#define prdebug(fmt, args...) do { if (debug) { printf("%s: " fmt, myname, ##args); }} while(0)
#define prerror(fmt, args...) do { fprintf(stderr, "%s: " fmt, myname, ##args); } while (0)

/*
 * should be roughly equivalent to "xrandr -s 0"
 * - just plain old xrandr 1.0 code
 * - no fancy extensions needed
 * - will explode badly if mode0 is no longer the one set by qxl driver
 */
static int XRandR_Mode0(int width, int height)
{
	XRRScreenConfiguration *sc;
	XRRScreenSize *sizes;
	int nsize;
	Status status = RRSetConfigFailed;
	int ret = 0;
	sc = XRRGetScreenInfo(dpy, root);

	if (sc == NULL) {/* ??? */
		prerror("XRRGetScreenInfo returned NULL!\n");
		ret = 1;
		goto out;
	}
	sizes = XRRConfigSizes(sc, &nsize);
	if (nsize < 1) {
		prerror("XRRConfigSizes returned less than 1 mode :-(\n");
		ret = 1;
		goto out;
	}
	if (sizes[0].width == width && sizes[0].height == height) {
		prdebug("%s nothing changed. skipping\n", __func__);
		goto out; /* no need to change identical size */
	}
	prdebug("%s changing from %dx%d to %dx%d\n", __func__, width, height, sizes[0].width, sizes[0].height);

	status = XRRSetScreenConfig(dpy, sc, root, (SizeID) 0,
			(Rotation) 1,	/* AFAICT spice driver does not support rotation anyway */
			CurrentTime);
	if (status == RRSetConfigFailed) {
		prerror("Failed to change the screen configuration!\n");
		ret = 1;
	}

out:
	XRRFreeScreenConfigInfo(sc);
	return(ret);
}


static void RRNotify_event(XEvent *eventp)
{
	XRRNotifyEvent *e = (XRRNotifyEvent *)eventp;
	XRRScreenResources *screen_r;

	XRRUpdateConfiguration(eventp);
	screen_r = XRRGetScreenResources(dpy, e->window);

	if (e->subtype != RRNotify_OutputChange) {
		prdebug("RRNotify event with subtype %d (!= RRNotify_OutputChange)\n", e->subtype);
		goto out;
	}
	if (! screen_r) {
		prerror("RRNotify event but no screen_r\n");
	} else {
		XRROutputChangeNotifyEvent *c = (XRROutputChangeNotifyEvent *)e;
		XRROutputInfo *output = NULL;
		XRRModeInfo *mode = NULL;
		int i;
		output = XRRGetOutputInfo(dpy, screen_r, c->output);
		for (i = 0; i < screen_r->nmode; i++) {
			if (screen_r->modes[i].id == c->mode) {
				mode = &screen_r->modes[i];
				break;
			}
		}
		prdebug("RRNotify event, subtype RRNotify_OutputChange ");
		if (mode) {
			if (debug) printf("mode %s (%dx%d)\n", mode->name, mode->width, mode->height);
			XRandR_Mode0(mode->width, mode->height);
		} else
			if (debug) printf("no mode => no RandR setting\n");
		XRRFreeOutputInfo(output);
	}
out:
	XRRFreeScreenResources(screen_r);
}

int main (int argc, char **argv)
{
	char *displayname = NULL;
	int screen;
	struct udev *udev;
	struct udev_device *dev;
	struct udev_monitor *mon;
	int udev_fd;
	Bool have_rr;
	int rr_event_base, rr_error_base;
	myname = argv[0];
	if (argv[1] && !strcmp(argv[1], "-d"))
		debug = 1;

	if (access(portdev, F_OK) != 0) {
		prdebug("%s does not exist, exiting\n", portdev);
		return 0;
	}
	dpy = XOpenDisplay(displayname);
	if (!dpy) {
		prerror("unable to open display '%s'\n", XDisplayName(displayname));
		exit (1);
	}

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	have_rr = XRRQueryExtension(dpy, &rr_event_base, &rr_error_base);
	if (! have_rr) {
		prerror("no RandR Extension?\n");
		exit(1);
	}

	udev = udev_new();
	if (!udev) {
		prerror("unable to open libudev\n");
		exit (2);
	}
	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "drm", NULL);
	udev_monitor_enable_receiving(mon);
	udev_fd = udev_monitor_get_fd(mon);

	XRRSelectInput(dpy, root, RROutputChangeNotifyMask);

	XRandR_Mode0(0, 0); /* initial mode setting */
	while (1) {
		fd_set fds;
		struct timeval tv;
		struct pollfd pfd = {
			.fd = ConnectionNumber(dpy),
			.events = POLLIN,
		};
		int ret;
		Bool delivered = False;
		Bool pending = XPending(dpy) > 0 || poll(&pfd, 1, 1000) > 0;
		if (pending) {
			XEvent event;
			XNextEvent(dpy, &event);
			if (event.type == rr_event_base + RRNotify) {
				RRNotify_event(&event);
				delivered = True;
			} else {
				/* should not happen IMHO */
				prdebug("Unknown event type %d\n", event.type);
			}
		} else
			prdebug("XNextEvent timeout\n");
		FD_ZERO(&fds);
		FD_SET(udev_fd, &fds);
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		ret = select(udev_fd + 1, &fds, NULL, NULL, &tv);
		if (ret > 0 && FD_ISSET(udev_fd, &fds)) {
			dev = udev_monitor_receive_device(mon);
			if (dev) {
				if (delivered)
					prdebug("udev event received, but xevent already processed\n");
				else {
					prdebug("udev-event => XRandR_Mode0(0, 0)\n");
					XRandR_Mode0(0, 0);
				}
				udev_device_unref(dev);
			}
		} else
			prdebug("udev timeout\n");
		fflush(stdout);
	}

	XCloseDisplay(dpy);
	udev_monitor_unref(mon);
	udev_unref(udev);
	return 0;
}
