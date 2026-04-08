all: vmm

vmm: vmm.o
	$(CC) $(LDFLAGS) -o vmm vmm.o

vmm.o: vmm.c
	$(CC) $(CFLAGS) $(INCLUDES) -c vmm.c

.PHONY: clean
clean:
	rm -f *.o vmm