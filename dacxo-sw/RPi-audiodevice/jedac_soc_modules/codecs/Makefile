KDIR=/lib/modules/$(shell uname -r)/build
obj-m += jedac5_codec.o snd-soc-jve1792a-i2c.o

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	rm -f snd-soc-jedac* snd-soc-pcm1792* snd-soc-jve1792* *.o Module.symvers modules.order
