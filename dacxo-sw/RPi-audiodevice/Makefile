# Makefile to build the kernel modules and devicetree overlay needed for the dacxo 
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
BACKUP     := ../dacxo-rpi-$(DATE).tgz
KERNEL     ?= kernel7
export KERNEL

LOCAL_KOS  := codecs/snd-soc-dacxo_codec.ko \
              bcm/snd-soc-dacxo_bcm.ko

INSTALL_KOS := $(addsuffix .xz,$(addprefix $(INSTALLDIR)/,$(notdir $(LOCAL_KOS))))
INSTALL_DTB := /boot/overlays/dacxo.dtbo
INSTALL_SERVICE := /etc/systemd/system/dacxo.service
INSTALL_ASOUND := /etc/asound.conf
MODULES_ALIAS := /lib/modules/$(KERNELREV)/modules.alias
INSTALL_ALL := $(INSTALL_KOS) $(MODULES_ALIAS) $(INSTALL_DTB) $(INSTALL_ASOUND)

.PHONY: dtbs backup modules install uninstall clean show show_regs show_card sound test_dtoverlay

install: $(INSTALL_ALL)

install_service: $(INSTALL_SERVICE)
	sudo systemctl enable dacxo.service
	sudo systemctl start dacxo.service

dtbs:    $(INSTALL_DTB)
modules: $(LINUXHDR) $(LOCAL_KOS)
backup:  $(BACKUP)

show_places:
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
	amixer -c DACXO contents

sound:
	speaker-test -D hw:DACXO -c 2 -r 96000 -F S24_LE -f 440 -t sine -l 1

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

uninstall:
	sudo systemctl stop dacxo.service ;\
	sudo dtoverlay -v -r dacxo; \
	sudo modprobe -v -r snd_soc_dacxo_codec snd_soc_dacxo_bcm ;\
	sudo rm -f $(INSTALL_KOS) $(INSTALL_DTB) $(INSTALL_SERVICE) $(INSTALL_ASOUND)

test_dtoverlay: $(INSTALL_KOS) $(INSTALL_DTB)
	sudo sysctl -w kernel.printk=6 ;\
	sudo dtoverlay -v dacxo sync_pin=27 ;\
	sleep 2 ;\
	lsmod | grep 'dacxo' ;\
	dmesg | grep 'dacxo' ;\
	sudo sysctl -w kernel.printk=3

dry_run.lst: dry_run.dtbo
	fdtdump $< > $@

dry_run.dtbo: $(INSTALL_DTB)
	sudo dtoverlay -D -v dacxo

$(MODULES_ALIAS): $(INSTALL_KOS)
	sudo depmod -a

$(INSTALLDIR)/snd-soc-dacxo_bcm.ko.xz: bcm/snd-soc-dacxo_bcm.ko | $(INSTALLDIR)
	cd bcm && sudo $(MAKE) -C $(LINUXHDR) M=$$PWD modules_install

$(INSTALLDIR)/snd-soc-%.ko.xz: codecs/snd-soc-%.ko  | $(INSTALLDIR)
	cd codecs && sudo $(MAKE) -C $(LINUXHDR) M=$$PWD modules_install

$(INSTALL_SERVICE): etc/dacxo.service
	sudo cp $< $@

$(INSTALL_ASOUND): etc/asound.conf
	sudo cp $< $@

bcm/snd-soc-dacxo_bcm.ko: bcm/dacxo_bcm.c codecs/dacxo.h codecs/pcm1792a.h
	cd bcm && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

codecs/snd-soc-dacxo_codec.ko: codecs/dacxo_codec.c codecs/dacxo.h
	cd codecs && $(MAKE) -C $(LINUXHDR) M=$$PWD modules

$(INSTALL_DTB) : overlays/dacxo.dtbo
	sudo cp $< $@

overlays/dacxo.dtbo : overlays/dacxo-overlay.dts
	dtc -@ -I dts -O dtb -o $@ $<
