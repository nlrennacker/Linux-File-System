
ROOTNAME=fsshell
HW=
FOPTION=
RUNOPTIONS=SampleVolume 10000000 512
CC=gcc
CFLAGS= -g -I.
LIBS =pthread
DEPS = 
ADDOBJ= fsInit.o bitmap.o directoryEntry.o mfs.o fsshell.o pathparse.o b_io.o
ARCH = $(shell uname -m)

ifeq ($(ARCH), aarch64)
	ARCHOBJ=fsLowM1.o
else
	ARCHOBJ=fsLow.o
endif

OBJ = $(ROOTNAME)$(HW)$(FOPTION).o $(ADDOBJ) $(ARCHOBJ)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

$(ROOTNAME)$(HW)$(FOPTION): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -lm -l readline -l $(LIBS)

clean:
	rm $(ROOTNAME)$(HW)$(FOPTION).o $(ADDOBJ) $(ROOTNAME)$(HW)$(FOPTION)

run: $(ROOTNAME)$(HW)$(FOPTION)
	./$(ROOTNAME)$(HW)$(FOPTION) $(RUNOPTIONS)

vrun: $(ROOTNAME)$(HW)$(FOPTION)
	valgrind ./$(ROOTNAME)$(HW)$(FOPTION) $(RUNOPTIONS)


