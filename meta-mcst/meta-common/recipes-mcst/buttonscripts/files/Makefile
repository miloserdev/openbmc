BINARYNAME=server_ctl

all: $(BINARYNAME)

$(BINARYNAME): $(BINARYNAME).o
	$(CC) -o $(BINARYNAME) $(BINARYNAME).o -lgpiod $(shell pkg-config --libs dbus-1) $(LDFLAGS)

$(BINARYNAME).o: $(BINARYNAME).c
	$(CC) -o $(BINARYNAME).o $(shell pkg-config --cflags dbus-1) -c $(BINARYNAME).c

clean:
	rm -f *.o $(BINARYNAME)
