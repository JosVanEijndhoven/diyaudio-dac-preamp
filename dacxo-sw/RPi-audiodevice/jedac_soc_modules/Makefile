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
              codecs/snd-soc-jve1792a-i2c.ko \
              bcm/snd-soc-jedac5_bcm.ko

INSTALL_KOS := $(addprefix $(INSTALLDIR)/,$(notdir $(LOCAL_KOS)))
INSTALL_DTB := /boot/overlays/jedac5.dtbo
INSTALL_INITD := /etc/init.d/jedac
MODULES_ALIAS := /lib/modules/$(KERNELREV)/modules.alias

.PHONY: all tar backup dtbs modules install uninstall clean clean_all

# install: $(INSTALL_KOS) $(INSTALL_DTB) $(INSTALL_INITD)
install: $(INSTALL_KOS) $(INSTALL_DTB) $(MODULES_ALIAS)

dtbs:    $(INSTALL_DTB)
modules: $(LINUXHDR) $(LOCAL_KOS)
show:
	@echo '  LINUXHDR=' $(LINUXHDR)
	@echo '  LINUXOVLS=' $(LINUXOVLS)
	@echo '  INSTALLDIR=' $(INSTALLDIR)
	@echo '  KERNEL=' $(KERNEL)
	@echo '  MODULES_ALIAS=' $(MODULES_ALIAS)

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
	rm -f overlays/*.dtbo overlays/*.dtb dry_run.*

clean_install: clean
	sudo rm -f $(INSTALL_KOS) $(INSTALL_DTB)

uninstall:
	sudo dtoverlay -v -r jedac5
	sudo modprobe -v -r snd_soc_jedac5_codec snd_soc_jve1792a_i2c snd_soc_jedac5_bcm

test_dtoverlay: $(INSTALL_KOS) $(INSTALL_DTB)
	sudo sysctl -w kernel.printk=6 ;\
	sudo dtoverlay -v jedac5 ;\
	sleep 2 ;\
	sudo sysctl -w kernel.printk=3 ;\
	lsmod | grep 'jedac\|jve1792'
	dmesg | grep 'jedac\|jve1792'

dry_run.lst: dry_run.dtbo
	fdtdump $< > $@

dry_run.dtbo: $(INSTALL_DTB)
	sudo dtoverlay -D -v jedac5

$(MODULES_ALIAS): $(INSTALL_KOS)
	sudo depmod -a

$(INSTALLDIR)/snd-soc-jedac5_bcm.ko: bcm/snd-soc-jedac5_bcm.ko | $(INSTALLDIR)
	cd bcm && sudo $(MAKE) -C $(LINUXHDR) M=$$PWD modules_install

$(INSTALLDIR)/snd-soc-%.ko: codecs/snd-soc-%.ko  | $(INSTALLDIR)
	cd codecs && sudo $(MAKE) -C $(LINUXHDR) M=$$PWD modules_install

$(INSTALL_INITD): etc/init.d.jedac
	sudo cp $< $@ && sudo chmod 0755 $@

bcm/snd-soc-jedac5_bcm.ko: bcm/jedac5_bcm.c codecs/jedac5.h codecs/pcm1792a.h
	cd bcm && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

codecs/snd-soc-jedac5_codec.ko: codecs/jedac5_codec.c codecs/jedac5.h
	cd codecs && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

codecs/snd-soc-jve1792%.ko: codecs/jve1792%.c codecs/pcm1792a.h
	cd codecs && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

# Modified output from jedac5-overlay.dtb to jedac5.dtbo, 
# in accordance with new debian naming policy as of version 4.4
$(INSTALL_DTB) : overlays/jedac5.dtbo
	sudo cp $< $@

$(INSTALLDIR):
	sudo mkdir $@

overlays/jedac5.dtbo : overlays/jedac5-overlay.dts
	dtc -@ -I dts -O dtb -o $@ $<
