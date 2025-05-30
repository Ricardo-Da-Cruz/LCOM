# name of the program (Minix service)
PROG=proj

.PATH: ${.CURDIR}/devices
.PATH: ${.CURDIR}/sprites

# source code files to be compiled
SRCS = main.c game.c game.h 
SRCS += utils.c utils.h timer.c timer.h i8042.h i8254.h vbe.h gpu.c gpu.h keyboard.c keyboard.h i8042.h interrupts.c interrupts.h mouse.c mouse.h
SRCS += sprites/blinky.h sprites/clyde.h sprites/eyes.h sprites/inky.h sprites/pacman.h sprites/pacman2.h scrites/score.h 
SRCS += sprites/pinky.h sprites/pacman_death_anim.h sprites/ghost.h sprites/lettets.h sprites/numbers.h sprites/maze.h

# Set the search path for source files
VPATH = devices:sprites:.

# additional compilation flags
# "-Wall -Wextra -Werror -I . -std=c11 -Wno-unused-parameter" are already set
CFLAGS += -pedantic -O3

# list of library dependencies (for Lab 2, only LCF library)
DPADD += ${LIBLCF}
LDADD += -llcf

# include LCOM's makefile that does all the "heavy lifting"
.include <minix.lcom.mk>
