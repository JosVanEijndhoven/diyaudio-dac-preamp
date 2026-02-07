KDIR=/lib/modules/$(shell uname -r)/build
obj-m += jedac_codec.o

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	rm -f snd-soc-jedac* *.o Module.symvers modules.order
