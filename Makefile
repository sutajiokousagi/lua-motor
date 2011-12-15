PATH=/home/user/chumby-oe/meta-chumby-private/bin:/home/user/chumby-oe/meta-chumby/bin:/home/user/chumby-oe/openembedded/bin:/home/user/chumby-oe/output-angstrom-.9/sysroots/i686-linux/usr/armv5te/bin:/home/user/chumby-oe/output-angstrom-.9/sysroots/i686-linux/usr/sbin:/home/user/chumby-oe/output-angstrom-.9/sysroots/i686-linux/usr/bin:/home/user/chumby-oe/output-angstrom-.9/sysroots/i686-linux/sbin:/home/user/chumby-oe/output-angstrom-.9/sysroots/i686-linux//bin:/home/user/chumby-oe/bitbake-1.12.0/bin:/home/user/CodeSourcery/Sourcery_G++_Lite/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games

#------
# Load configuration
#
EXT=so
SOCKET_V=2.0.2
MIME_V=1.0.2
SOCKET_SO=motor.$(EXT).$(SOCKET_V)

#------
# Lua includes and libraries
#
#LUAINC=-I/usr/local/include/lua50
#LUAINC=-I/usr/local/include/lua5.1
#LUAINC=-Ilua-5.1.1/src

#------
# Compat-5.1 directory
#
#COMPAT=compat-5.1r5

#------
# Top of your Lua installation
# Relative paths will be inside the src tree
#
#INSTALL_TOP_SHARE=/usr/local/share/lua/5.0
#INSTALL_TOP_LIB=/usr/local/lib/lua/5.0
INSTALL_TOP_SHARE=/usr/local/share/lua/5.1
INSTALL_TOP_LIB=/usr/local/lib/lua/5.1

INSTALL_DATA=cp
INSTALL_EXEC=cp

#------
# Compiler and linker settings
# for Mac OS X
#
#CC=gcc
#DEF= -DLUASOCKET_DEBUG -DUNIX_HAS_SUN_LEN
#CFLAGS= $(LUAINC) -I$(COMPAT) $(DEF) -pedantic -Wall -O2 -fno-common
#LDFLAGS=-bundle -undefined dynamic_lookup
#LD=export MACOSX_DEPLOYMENT_TARGET="10.3"; gcc

#------
# Compiler and linker settings
# for Linux
CC=arm-angstrom-linux-gnueabi-gcc -march=armv5te -mtune=xscale -mthumb-interwork -mno-thumb --sysroot=/home/user/chumby-oe/output-angstrom-.9/sysroots/armv5te-angstrom-linux-gnueabi
DEF=-DLUASOCKET_DEBUG
CFLAGS=-fexpensive-optimizations -frename-registers -O0 -ggdb2 -Wall
LDFLAGS=-shared
LD=arm-none-linux-gnueabi-gcc


#------
# Hopefully no need to change anything below this line
#

#------
# Modules belonging to socket-core
#

#$(COMPAT)/compat-5.1.o \

SOCKET_OBJS:= \
	lua-motor.o 


all: $(SOCKET_SO)

$(SOCKET_SO): $(SOCKET_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(SOCKET_OBJS)

#------
# List of dependencies
#
lua-motor.o: lua-motor.c

clean:
	rm -f $(SOCKET_SO) $(SOCKET_OBJS) 

#------
# End of makefile configuration
#
