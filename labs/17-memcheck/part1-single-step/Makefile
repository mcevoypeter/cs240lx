TESTS = tests/part1-test1 \
		tests/part1-test2 \
		tests/part1-test3 

SYS_OBJS = cp14-debug.o interrupt-asm.o check-interleave.o single-step.o

TESTS := $(patsubst %.c, %, $(TESTS))  
GREP_STR := 'TRACE:\|ERROR:\|PANIC:' 
GREP_STR := 'GC:\|ERROR:\|PANIC:' 

# ALL_ASM := $(wildcard ./*.S)
# $(patsubst %.S, %.o, $(ALL_ASM))  

ALL_SOURCES := $(wildcard ./*.c)
OBJS := $(patsubst %.c, %.o, $(ALL_SOURCES))  $(SYS_OBJS)
TEST_BINARIES := $(patsubst %, %.bin, $(TESTS))  
TEST_RUN := $(patsubst %, %.run, $(TESTS))  
TEST_EMIT := $(patsubst %, %.emit, $(TESTS))  
TEST_CHECK := $(patsubst %, %.check, $(TESTS))  

# if you want to compile everything in a directory, do something like this.
# SOURCES := $(wildcard ./*.c)

# check that your path is defined.
ifndef CS240LX_2021_PATH
$(error CS240LX_2021_PATH is not set: this should contain the absolute path to where this directory is.  Define it in your shell's initialiation.  For example, .tcshrc for tcsh or .bashrc for bash)
endif

include $(CS240LX_2021_PATH)/libpi/includes.mk

CFLAGS += -Wno-unused-function -Wno-unused-variable -fno-toplevel-reorder -Werror 

# -fno-omit-frame-pointer -mpoke

# -fno-reorder-functions

# add all the .h's as dependencies, libpi and the makefile.
DEPS := $(MEMMAP) $(LPI) $(LIBM) $(wildcard ./*.h) ./Makefile 

# i *think* order is important.
LIBS = $(LPI)  

all:  libs $(OBJS) $(TEST_BINARIES) # $(TEST_RUN) # $(TEST_CHECK)

update:
	cp part1-test*.c part1-test*.out ../code

libs: 
	make -C $(LPP) 

$(OBJS): $(DEPS)
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.S
	$(CC) -c $(CPP_ASFLAGS) $< -o $@

# rules to generate .elf->.list->.bin
%.elf: %.o $(OBJS) $(MEMMAP) $(DEPS) 
	$(LD) $(START) $*.o $(SYS_OBJS) -T $(LPP)/memmap -o $@  $(LIBS)

%.list: %.elf
	$(OD) -D   $*.elf > $*.list
%.bin: %.list
	$(OCP) $*.elf -O binary $*.bin

%.run: %.bin
	my-install ./$*.bin

# note: this will fail if output does not contain a trace.
# we pull everything into .out so that it's easier to debug.
%.emit: %.bin
	my-install ./$*.bin 2>&1 > /dev/null | cat > ./$*.out

%.check: %.bin
	my-install ./$*.bin 2>&1 > /dev/null | grep $(GREP_STR)  > ./$*.test
	grep $(GREP_STR) ./$*.out | diff ./$*.test -
	# diff ./$*.test ./$*.out
	rm ./$*.test

# not sure if there is a better way: use these fake target to run our
# checks for everything.
emitall: $(TEST_EMIT) $(TEST_BINARIES)
checkall: $(TEST_CHECK) $(TEST_BINARIES)
runall: $(TEST_RUN) 

tags: 
	ctags *.[chSs] $(LPP)/**/*.[chSs] 

clean:
	rm -rf $(BUILD_DIR) $(NAME) *~ tags $(OBJS) *.bin *.elf *.list *.img Makefile.bak  *.d *.test 
	#make -C ./tests clean

cleanall:
	make -C ./ clean
	rm *.out

.PHONY: libs all tags clean run fake-libpi  runall emitall checkall update

# stop make from deleting these suffix'd files.
.PRECIOUS: %.list %.elf
