# Makefile for TIKI-100_emul V1.1
# Build with: make

#------------------------------------------------------------------------------
# Settings
#------------------------------------------------------------------------------

# Z80 debugger (uncomment to enable):
#Z80_DEBUGGER = -DDEBUG

CC = gcc
CFLAGS = -Wall -O -DLSB_FIRST -Wno-multichar -Wno-overflow -Wno-pointer-to-int-cast $(Z80_DEBUGGER)

NO_CONSOLE_ = -Wl,--subsystem,windows
LDFLAGS = -s -Wl,--subsystem,windows -lgdi32 -lcomdlg32 -lcomctl32 -lwinmm -ldwmapi

#------------------------------------------------------------------------------
# Rules
#------------------------------------------------------------------------------

BINDIR = ../bin

OBJECTS = $(BINDIR)/TIKI-100_emul.o $(BINDIR)/mem.o $(BINDIR)/video.o $(BINDIR)/sound.o $(BINDIR)/ctc.o $(BINDIR)/keyboard.o $(BINDIR)/disk.o $(BINDIR)/serial.o $(BINDIR)/parallel.o $(BINDIR)/Z80.o $(BINDIR)/Debug.o $(BINDIR)/log.o $(BINDIR)/screenshot.o $(BINDIR)/z80info.o $(BINDIR)/memview.o $(BINDIR)/diskview.o $(BINDIR)/win32.o $(BINDIR)/win32_res.res

$(BINDIR)/tikiemul.exe : $(OBJECTS) Makefile | $(BINDIR)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/TIKI-100_emul.o : TIKI-100_emul.c TIKI-100_emul.h protos.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/mem.o : mem.c TIKI-100_emul.h protos.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/video.o : video.c TIKI-100_emul.h protos.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/sound.o : sound.c TIKI-100_emul.h protos.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/ctc.o : ctc.c TIKI-100_emul.h protos.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/keyboard.o : keyboard.c TIKI-100_emul.h protos.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/disk.o : disk.c TIKI-100_emul.h protos.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/serial.o : serial.c TIKI-100_emul.h protos.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/parallel.o : parallel.c TIKI-100_emul.h protos.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/Z80.o : Z80.c Z80.h Codes.h CodesED.h CodesCB.h CodesXX.h Tables.h CodesXCB.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/Debug.o : Debug.c Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/log.o : log.c log.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/screenshot.o : screenshot.c screenshot.h log.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/z80info.o : z80info.c z80info.h TIKI-100_emul.h Z80.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/memview.o : memview.c memview.h TIKI-100_emul.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/diskview.o : diskview.c diskview.h TIKI-100_emul.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BINDIR)/win32.o : win32.c win32_res.h TIKI-100_emul.h Z80.h log.h screenshot.h z80info.h memview.h diskview.h version.h Makefile | $(BINDIR)
	$(CC) -c $< -o $@ $(CFLAGS) -fexec-charset=CP1252

$(BINDIR)/win32_res.res : win32_res.rc win32_res.h win32.ico TIKI-100_emul.h Z80.h version.h tikiemul.exe.manifest Makefile | $(BINDIR)
	windres $< $(Z80_DEBUGGER) -O coff -o $@ --codepage=65001

.PHONY : clean
clean :
	rm -f $(BINDIR)/tikiemul.exe $(BINDIR)/*.res $(BINDIR)/*.o

