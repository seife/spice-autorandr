/*
 * Copyright (C) 2017 Stefan Seyfried <seife+dev@b1-systems.com>
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
 * Listens to RRNotify_OutputChange events and then adjusts current mode
 * Intened to be used in KVM guests with spice drivers and virt-manager's
 * "Auto Resize VM with Window" setting ticked.
 *
 * Ideas taken from xrandr and xev source code.
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

static const char *myname;
static Display *dpy;
static Window root;
static int debug;

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
	Bool have_rr;
	int rr_event_base, rr_error_base;
	myname = argv[0];
	if (argv[1] && !strcmp(argv[1], "-d"))
		debug = 1;

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

	XRRSelectInput(dpy, root, RROutputChangeNotifyMask);

	XRandR_Mode0(0, 0); /* initial mode setting */
	while (1) {
		XEvent event;
		XNextEvent(dpy, &event);
		if (event.type == rr_event_base + RRNotify) {
			RRNotify_event(&event);
		} else {
			/* should not happen IMHO */
			prdebug("Unknown event type %d\n", event.type);
		}
		fflush(stdout);
	}

	XCloseDisplay(dpy);
	return 0;
}
