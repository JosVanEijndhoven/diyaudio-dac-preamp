# Makefile to build the kernel modules and devicetree overlay needed for the jedac 
# Jos van Eijndhoven, June 2016
#
# This build structure was set up according to the guidelines formulated in:
# https://www.gnu.org/software/make/manual/html_node/Makefile-Contents.html
#

PWD        := $(shell pwd)
KERNELREV  := $(shell uname -r)
LINUXHDR   := /usr/src/linux-headers-$(KERNELREV)
LINUXOVLS  := $(LINUXHDR)/arch/arm64/boot/dts/overlays
INSTALLDIR := /lib/modules/$(KERNELREV)/extra
DATE       := $(shell date --rfc-3339=date)
BACKUP     := ../jedac-rpi-$(DATE).tgz
KERNEL     ?= kernel7
export KERNEL

LOCAL_KOS  := codecs/snd-soc-jedac5_codec.ko \
              bcm/snd-soc-jedac5_bcm.ko

INSTALL_KOS := $(addprefix $(INSTALLDIR)/,$(notdir $(LOCAL_KOS)))
INSTALL_DTB := /boot/overlays/jedac5.dtbo
INSTALL_INITD := /etc/init.d/jedac

.PHONY: all tar backup dtbs modules install uninstall clean clean_all

install: $(INSTALL_KOS) $(INSTALL_DTB) $(INSTALL_INITD)
dtbs:    $(INSTALL_DTB)
modules: $(LINUXHDR) $(LOCAL_KOS)
show:
	@echo '  LINUXHDR=' $(LINUXHDR)
	@echo '  LINUXOVLS=' $(LINUXOVLS)
	@echo '  KERNEL=' $(KERNEL)

tar: $(BACKUP)

backup: $(BACKUP)
	scp $< jos@DS213:Audio/DAC5/Raspberry/Backups/

$(BACKUP): ./Makefile
	cp /boot/config.txt boot && \
	tar -czf $@ \
	--exclude=\*.o --exclude=\*.ko --exclude=\*.mod.c --exclude=\*.cmd \
	--exclude=modules.order --exclude=Module.symvers  --exclude=.tmp_versions \
	.

clean:
	cd bcm && $(MAKE) -C $(LINUXHDR) M=$$PWD clean
	cd codecs && $(MAKE) -C $(LINUXHDR) M=$$PWD clean
	rm -f overlays/*.dtbo overlays/*.dtb
	
clean_install: clean
	sudo rm -f $(INSTALL_KOS) $(INSTALL_DTB)

uninstall:
	sudo dtoverlay -v -r jedac5
	sudo modprobe -v -r snd-soc-jedac5_codec snd-soc-jedac5_bcm

$(INSTALLDIR)/snd-soc-jedac5_bcm.ko: bcm/snd-soc-jedac5_bcm.ko
	cd bcm && sudo $(MAKE) -C $(LINUXHDR) M=$$PWD modules_install

$(INSTALLDIR)/snd-soc-%.ko: codecs/snd-soc-%.ko
	cd codecs && sudo $(MAKE) -C $(LINUXHDR) M=$$PWD modules_install

$(INSTALL_INITD): etc/init.d.jedac
	sudo cp $< $@ && sudo chmod 0755 $@

bcm/snd-soc-jedac5_bcm.ko: bcm/jedac5_bcm.c codecs/jedac5.h
	cd bcm && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

codecs/snd-soc-jedac5_codec.ko: codecs/jedac5_codec.c codecs/jedac5.h
	cd codecs && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

# codecs/snd-soc-pcm1792%.ko: codecs/pcm1792%.c codecs/pcm1792a.h
# 	cd codecs && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

$(LINUXHDR):
	@echo "Missing: $(LINUXHDR)"
	@echo "Probably requires: sudo apt-get install raspberrypi-kernel-headers"
	false

# Modified output from jedac5-overlay.dtb to jedac5.dtbo, 
# in accordance with new debian naming policy as of version 4.4
$(INSTALL_DTB) : overlays/jedac5.dtbo
	sudo cp $< $@

overlays/jedac5.dtbo : overlays/jedac5-overlay.dts
	dtc -@ -I dts -O dtb -o $@ $<
