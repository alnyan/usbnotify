CFLAGS=
LDFLAGS=-ludev

all:
	$(CC) -o usbnotifyd $(CFLAGS) $(LDFLAGS) usbnotify.c

install-sh: hook-add hook-remove
	mkdir -p ~/.config/usbnotify
	cp hook-add ~/.config/usbnotify/hook-add
	cp hook-remove ~/.config/usbnotify/hook-remove
	chmod +x ~/.config/usbnotify/hook-add
	chmod +x ~/.config/usbnotify/hook-remove
