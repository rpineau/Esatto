# Makefile for libPegasusUPBv2Power and libPegasusUPBv2Focuser

CC = gcc
CFLAGS = -fPIC -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -I. -I./../../
CPPFLAGS = -std=c++17 -fPIC -Wall -Wextra -O2 -g -DSB_LINUX_BUILD -I. -I./../../
LDFLAGS = -shared -lstdc++
RM = rm -f
STRIP = strip
TARGET_LIBFocuser = libEsatto.so
TARGET_LIBRotator = libArco.so

SRCS_Focuser = focuserMain.cpp Esatto.cpp x2focuser.cpp x2rotator.cpp Arco.cpp
SRCS_Rotator = rotatorMain.cpp Arco.cpp x2rotator.cpp x2focuser.cpp x2focuser.cpp

OBJS_Focuser = $(SRCS_Focuser:.cpp=.o)
OBJS_Rotator = $(SRCS_Rotator:.cpp=.o)

.PHONY: all
all: ${TARGET_LIBFocuser} ${TARGET_LIBRotator}
.PHONY: power
power : ${TARGET_LIBRotator}
.PHONY: focuser
focuser : ${TARGET_LIBFocuser}

$(TARGET_LIBFocuser): $(OBJS_Focuser)
	$(CC) ${LDFLAGS} -o $@ $^
	$(STRIP) $@ >/dev/null 2>&1  || true

$(TARGET_LIBRotator): $(OBJS_Rotator)
	$(CC) ${LDFLAGS} -o $@ $^
	$(STRIP) $@ >/dev/null 2>&1  || true

%.cpp.o: %.cpp
	$(CC) $(CFLAGS) $(CPPFLAGS) -MM $< >$@

.PHONY: clean
clean:
	${RM} ${TARGET_LIBFocuser} ${TARGET_LIBRotator} *.o *.d
