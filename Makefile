CC = gcc
CFLAGS = -O0 -g -Wall -Wno-unused-parameter -Wextra
LDFLAGS = -g
LIBS =
OUT = a.out

all: $(OUT)

clean:
	$(RM) *.o
	${RM} $(OUT)
$(OUT): main.o
	@$(CC) $(CFLAGS) ${LDFLAGS} $(LIBS) -o $@ main.o
	@echo "  LD     $@"

main.o: main.c
	@$(CC) -c $(CFLAGS) -o $@ $<
	@echo "  CC     $<"

