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
LDFLAGS = -s -Wl,--subsystem,windows -lgdi32 -lcomdlg32 -lcomctl32

#------------------------------------------------------------------------------
# Rules
#------------------------------------------------------------------------------

OBJECTS = TIKI-100_emul.o mem.o video.o sound.o ctc.o keyboard.o disk.o serial.o parallel.o Z80.o Debug.o log.o win32.o win32_res.res

tikiemul.exe : $(OBJECTS) Makefile
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

TIKI-100_emul.o : TIKI-100_emul.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

mem.o : mem.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

video.o : video.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

sound.o : sound.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

ctc.o : ctc.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

keyboard.o : keyboard.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

disk.o : disk.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

serial.o : serial.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

parallel.o : parallel.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

Z80.o : Z80.c Z80.h Codes.h CodesED.h CodesCB.h CodesXX.h Tables.h CodesXCB.h Makefile
	$(CC) -c $< $(CFLAGS)

Debug.o : Debug.c Z80.h Makefile
	$(CC) -c $< $(CFLAGS)

log.o : log.c log.h Makefile
	$(CC) -c $< $(CFLAGS)

win32.o : win32.c win32_res.h TIKI-100_emul.h Z80.h log.h Makefile
	$(CC) -c $< $(CFLAGS) -fexec-charset=CP1252

win32_res.res : win32_res.rc win32_res.h TIKI-100_emul.h Z80.h Makefile
	windres $< $(Z80_DEBUGGER) -O coff -o $@ --codepage=65001

.PHONY : clean
clean :
	rm -f tikiemul.exe *.res *.o

