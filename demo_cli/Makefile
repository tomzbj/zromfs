CC   = gcc.exe

#LINKOBJ  = $(OBJ) $(RESOBJ)
LDFLAGS =  -L. #-lcomctl32 -mwindows 
DEFS =
CFLAGS = -Os -Wall
BIN  = main.exe
RM = rm -f 
SRC = $(wildcard *.c ../*.c) 
#ASRC = $(wildcard *.s)
OBJS    = $(ASRC:.s=.o) $(SRC:.c=.o) 
LDFLAGS += -Wl,-Map=main.map,--cref,--no-warn-mismatch 


.PHONY: all clean

all: $(BIN)

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFS) -I . $(INCDIR) $< -o $@

clean: clean-custom
	${RM} $(LINKOBJ) $(BIN) *~ .*.swp

$(BIN): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(BIN)
	@strip.exe $(BIN)
	@size $(BIN)
