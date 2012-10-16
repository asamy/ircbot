CC = gcc
CFLAGS = -O0 -g -Wall -Wno-unused-parameter -Wextra
OUT = a.out

all: $(OUT)

clean:
	$(RM) *.o
	${RM} *.db
	${RM} $(OUT)
$(OUT): obj.o
	@$(CC) $(CFLAGS) -o $@ $<
	@echo "  LD     $@"

obj.o: main.c
	@$(CC) -c $(CFLAGS) -o $@ $<
	@echo "  CC     $<"

