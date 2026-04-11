# Makefile for TIKI-100_emul V1.1
# Asbjørn Djupdal 2001
#
# make                    - Bygger emulator
# make clean              - Sletter alle automatisk genererte filer
# make amiga_translations - kompilerer catalog-filer for Amiga-versjon

#------------------------------------------------------------------------------
# Generelle innstillinger
#------------------------------------------------------------------------------

# Velg system du skal kompilere for:
#SYSTEM = amiga
#SYSTEM = win32
SYSTEM = unix

# Ta med denne dersom Z80-debugger ønskes inkludert:
#Z80_DEBUGGER = -DDEBUG

# Velg C-kompilator du ønsker å bruke:
CC = gcc

# Generelle kompilatoropsjoner:
CFLAGS = -Wall -O

# Generelle linkeropsjoner:
LDFLAGS = -s

#------------------------------------------------------------------------------
# Dersom SYSTEM=unix må følgende settes:
#------------------------------------------------------------------------------

# Velg CPU-type - big-endian eller little-endian:
#ENDIAN = -DMSB_FIRST   # big-endian CPU, bl.a: M68k, Sun-maskiner
ENDIAN = -DLSB_FIRST   # little-endian CPU, bl.a: intel-x86

# Skriv inn stien til X11-includefiler:
X11_INCLUDE_PATH = -I/usr/X11R6/include

# Skriv inn stien til X11-linkbibliotek:
X11_LIB_PATH = -L/usr/X11R6/lib

# Skriv inn hvilke bibliotek som må linkes:
X11_LINK_LIBS = -lX11

#------------------------------------------------------------------------------
# Det skal ikke være nødvendig å forandre på resten av denne filen!
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# Systemspesifikke innstillinger 
#------------------------------------------------------------------------------

# amiga
CFLAGS_amiga = -DMSB_FIRST $(CFLAGS) $(Z80_DEBUGGER)
LDFLAGS_amiga = -lm -noixemul $(LDFLAGS)
SYS_OBJECTS_amiga = amiga.o

# win32
NO_CONSOLE_ = -mno-cygwin -Wl,--subsystem,windows
CFLAGS_win32 = -DLSB_FIRST $(CFLAGS) $(Z80_DEBUGGER)
LDFLAGS_win32 = $(NO_CONSOLE_$(Z80_DEBUGGER)) -e _mainCRTStartup -lgdi32 -lcomdlg32 -lcomctl32 $(LDFLAGS)
SYS_OBJECTS_win32 = win32.o win32_res.res

# unix
CFLAGS_unix = $(ENDIAN) $(X11_INCLUDE_PATH) $(CFLAGS) $(Z80_DEBUGGER)
LDFLAGS_unix = $(LDFLAGS) $(X11_LIB_PATH) $(X11_LINK_LIBS)
SYS_OBJECTS_unix = unix.o

#------------------------------------------------------------------------------
# Regler
#------------------------------------------------------------------------------

# Alle objektfiler som skal linkes
OBJECTS = TIKI-100_emul.o mem.o video.o sound.o ctc.o keyboard.o disk.o serial.o parallel.o Z80.o Debug.o $(SYS_OBJECTS_$(SYSTEM))

tikiemul : $(OBJECTS) Makefile
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS_$(SYSTEM))

TIKI-100_emul.o : TIKI-100_emul.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

mem.o : mem.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

video.o : video.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

sound.o : sound.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

ctc.o : ctc.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

keyboard.o : keyboard.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

disk.o : disk.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

serial.o : serial.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

parallel.o : parallel.c TIKI-100_emul.h protos.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

Z80.o : Z80.c Z80.h Codes.h CodesED.h CodesCB.h CodesXX.h Tables.h CodesXCB.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

Debug.o : Debug.c Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

.PHONY : clean
clean :
	rm -rf tikiemul tikiemul.exe amiga_strings.h *.res *.o *~ catalogs

#------
# amiga
#------

amiga.o : amiga.c amiga_strings.h TIKI-100_emul.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

amiga_strings.h : amiga.cd Makefile
	catcomp DESCRIPTOR amiga.cd CFILE amiga_strings.h

.PHONY : amiga_translations
amiga_translations :
	mkdir -p catalogs/norsk
	catcomp DESCRIPTOR amiga.cd TRANSLATION amiga_translations/norsk.ct CATALOG catalogs/norsk/tikiemul.catalog VB 0

#------
# win32
#------

win32.o : win32.c win32_res.h TIKI-100_emul.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

win32_res.res : win32_res.rc win32_res.h TIKI-100_emul.h Z80.h Makefile
	windres $< $(Z80_DEBUGGER) -O coff -o $@

#------
# unix
#------

unix.o : unix.c TIKI-100_emul.h Z80.h Makefile
	$(CC) -c $< $(CFLAGS_$(SYSTEM))

