
NAME = firmware
MCU = attiny13

CC = avr-gcc
AS = avr-as
CFLAGS = -Os -mmcu=attiny13

all: $(NAME).hex

main.o: main.c

$(NAME).elf: main.o
	$(CC) $(CFLAGS) $^ -o $@

$(NAME).hex: $(NAME).elf
	avr-objcopy -O ihex $< $@

clean:
	-rm *.o
	-rm *.elf
	-rm *.hex

write: $(NAME).hex
	avrdude -c avrisp -P /dev/ttyACM0 -b 19200 -p $(MCU) -U lfuse:w:0x7a:m -U flash:w:$(NAME).hex

