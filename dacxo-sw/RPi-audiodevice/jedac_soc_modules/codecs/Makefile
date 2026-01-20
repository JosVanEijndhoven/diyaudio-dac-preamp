KDIR=/lib/modules/$(shell uname -r)/build
obj-m += jedac5_codec.o snd-soc-pcm1792a-i2c.o

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	rm -f snd-soc-jedac* snd-soc-pcm1792* *.o Module.symvers modules.order
