RISCV_GNU ?= riscv64-unknown-elf
CC       = $(RISCV_GNU)-gcc
LD       = $(RISCV_GNU)-ld
OBJCOPY  = $(RISCV_GNU)-objcopy

CFLAGS   = -mcmodel=medany -ffreestanding -nostdlib -g -Wall -Iinclude
LDFLAGS  = -T boot/link.ld

QEMU     ?= qemu-system-riscv64
TARGET   = kernel

SRCS = \
	boot/start.S \
	src/sbi.c \
	src/main.c \
	src/uart.c \
	src/dt_parse.c \
	src/initrd_parse.c \
	src/str.c \
	src/shell.c

OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

BUILD    = build

ITS = $(BUILD)/$(TARGET).its
ELF = $(BUILD)/$(TARGET).elf
BIN = $(BUILD)/$(TARGET).bin
FIT = $(BUILD)/$(TARGET).fit

build: clean $(FIT)

$(ELF): $(OBJS)
	mkdir -p $(BUILD)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(FIT): $(BIN) $(ITS)
	mkimage -f $(ITS) $@

boot/%.o: boot/%.S
	$(CC) $(CFLAGS) -c $< -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	$(QEMU) -M virt -m 8G -kernel $(BIN) -display none -serial stdio

clean:
	rm -rf $(ELF) $(BIN) $(FIT) $(OBJS)
