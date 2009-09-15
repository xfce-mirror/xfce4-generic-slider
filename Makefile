CC = gcc
CFLAGS = -Wall -g
MAKE = make
INSTALL = install
SED = sed
RM = rm
CD = cd
PKG_CONFIG = pkg-config
DESTDIR =

PREFIX = /usr
PLUGINDIR = $(PREFIX)/lib/xfce4/xfce4/panel-plugins
PLUGINDESKTOPDIR = $(PREFIX)/share/xfce4/panel-plugins

LIBXFCE4PANEL_CFLAGS = `$(PKG_CONFIG) --cflags libxfce4panel-1.0`
LIBXFCE4PANEL_LIBS = `$(PKG_CONFIG) --libs libxfce4panel-1.0`

all: generic-slider.c
	$(CC) $(CFLAGS) $(LIBXFCE4PANEL_CFLAGS) -c generic-slider.c
	$(CC) $(CFLAGS) -o xfce4-generic-slider-plugin generic-slider.o $(LIBXFCE4PANEL_LIBS)

clean:
	$(RM) -f *.o
	$(RM) -f xfce4-generic-slider-plugin

install:
	$(SED) -i -e "s|@PLUGINDIR@|$(PLUGINDIR)|g" generic-slider.desktop
	$(INSTALL) -Dm644 generic-slider.desktop $(DESTDIR)$(PLUGINDESKTOPDIR)/generic-slider.desktop
	$(INSTALL) -Dm755 xfce4-generic-slider-plugin $(DESTDIR)$(PLUGINDIR)/xfce4-generic-slider-plugin

uninstall:
	$(RM) -f $(DESTDIR)$(PLUGINDESKTOPDIR)/generic-slider.desktop
	$(RM) -f $(DESTDIR)$(PLUGINDIR)/xfce4-generic-slider-plugin