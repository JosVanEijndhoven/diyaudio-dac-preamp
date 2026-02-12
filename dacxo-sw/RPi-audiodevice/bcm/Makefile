KDIR=/lib/modules/$(shell uname -r)/build
obj-m += dacxo_bcm.o

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	rm -f snd-soc-dacxo* *.o Module.symvers modules.order
