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
INSTALLDIR := /lib/modules/$(KERNELREV)/updates
DATE       := $(shell date --rfc-3339=date)
BACKUP     := ../jedac-rpi-$(DATE).tgz
KERNEL     ?= kernel7
export KERNEL

LOCAL_KOS  := codecs/snd-soc-jedac_codec.ko \
              bcm/snd-soc-jedac_bcm.ko

INSTALL_KOS := $(addsuffix .xz,$(addprefix $(INSTALLDIR)/,$(notdir $(LOCAL_KOS))))
INSTALL_DTB := /boot/overlays/jedac.dtbo
INSTALL_SERVICE := /etc/systemd/system/jedac.service
INSTALL_ASOUND := /etc/asound.conf
MODULES_ALIAS := /lib/modules/$(KERNELREV)/modules.alias
INSTALL_ALL := $(INSTALL_KOS) $(MODULES_ALIAS) $(INSTALL_DTB) $(INSTALL_SERVICE) $(INSTALL_ASOUND)

.PHONY: dtbs modules install uninstall clean clean_install show show_regs show_card sound test_dtoverlay

install: $(INSTALL_ALL)

life: $(INSTALL_ALL)
	sudo systemctl enable jedac.service
	sudo systemctl start jedac.service

dtbs:    $(INSTALL_DTB)
modules: $(LINUXHDR) $(LOCAL_KOS)
show:
	@echo '  LINUXHDR=' $(LINUXHDR)
	@echo '  LINUXOVLS=' $(LINUXOVLS)
	@echo '  INSTALLDIR=' $(INSTALLDIR)
	@echo '  KERNEL=' $(KERNEL)
	@echo '  MODULES_ALIAS=' $(MODULES_ALIAS)

show_regs:
	@echo 'FPGA Registers:'
	@sudo cat /sys/kernel/debug/regmap/1-0010/registers
	@echo 'Left pcm1792 Registers:'
	@sudo cat /sys/kernel/debug/regmap/1-004d/registers
	@echo 'Right pcm1792 Registers:'
	@sudo cat /sys/kernel/debug/regmap/1-004c/registers

show_card:
	amixer

sound:
	speaker-test -D hw:JEDAC -c 2 -r 96000 -F S24_LE -f 440 -t sine -l 1

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
	sudo rm -f $(INSTALL_KOS) $(INSTALL_DTB) $(NSTALL_SERVICE) $(INSTALL_ASOUND)

uninstall:
	sudo systemctl stop jedac.service ;\
	sudo dtoverlay -v -r jedac; \
	sudo modprobe -v -r snd_soc_jedac_codec snd_soc_jedac_bcm ;\
	sudo rm -f $(INSTALL_KOS) $(INSTALL_DTB) $(INSTALL_SERVICE) $(INSTALL_ASOUND)

test_dtoverlay: $(INSTALL_KOS) $(INSTALL_DTB)
	sudo sysctl -w kernel.printk=6 ;\
	sudo dtoverlay -v jedac sync_pin=27 ;\
	sleep 2 ;\
	lsmod | grep 'jedac' ;\
	dmesg | grep 'jedac' ;\
	sudo sysctl -w kernel.printk=3

dry_run.lst: dry_run.dtbo
	fdtdump $< > $@

dry_run.dtbo: $(INSTALL_DTB)
	sudo dtoverlay -D -v jedac

$(MODULES_ALIAS): $(INSTALL_KOS)
	sudo depmod -a

$(INSTALLDIR)/snd-soc-jedac_bcm.ko.xz: bcm/snd-soc-jedac_bcm.ko | $(INSTALLDIR)
	cd bcm && sudo $(MAKE) -C $(LINUXHDR) M=$$PWD modules_install

$(INSTALLDIR)/snd-soc-%.ko.xz: codecs/snd-soc-%.ko  | $(INSTALLDIR)
	cd codecs && sudo $(MAKE) -C $(LINUXHDR) M=$$PWD modules_install

$(INSTALL_INITD): etc/init.d.jedac
	sudo cp $< $@ && sudo chmod 0755 $@

$(INSTALL_SERVICE): etc/jedac.service
	sudo cp $< $@

$(INSTALL_ASOUND): etc/asound.conf
	sudo cp $< $@

bcm/snd-soc-jedac_bcm.ko: bcm/jedac_bcm.c codecs/jedac.h codecs/pcm1792a.h
	cd bcm && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

codecs/snd-soc-jedac_codec.ko: codecs/jedac_codec.c codecs/jedac.h
	cd codecs && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

# Modified output from jedac-overlay.dtb to jedac.dtbo, 
# in accordance with new debian naming policy as of version 4.4
$(INSTALL_DTB) : overlays/jedac.dtbo
	sudo cp $< $@

overlays/jedac.dtbo : overlays/jedac-overlay.dts
	dtc -@ -I dts -O dtb -o $@ $<
