#include "keyboard_map.h"

/* there are 25 lines each of 80 columns; each element takes 2 bytes */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8e
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

extern unsigned char keyboard_map[128];
extern void keyboard_handler(void);

/* current cursor location */
unsigned int current_loc = 0;
/* video memory begins at address 0xb8000 */
char *vidptr = (char*)0xb8000;

char user_input_string[15];
int current_amount_of_characters = 0;

void idt_init(void)
{
	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	struct IDT_entry
	{
		unsigned short int offset_lowerbits;
		unsigned short int selector;
		unsigned char zero;
		unsigned char type_attr;
		unsigned short int offset_higherbits;
	};

	struct IDT_entry IDT[IDT_SIZE];

	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler; 
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = INTERRUPT_GATE;
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;
	

	/*     		Ports
	 *		PIC1	PIC2
	 *Command 	0x20	0xA0
	 *Data		0x21	0xA1
	 */

	/* ICW1 - begin initialization */
	write_port(0x20 , 0x11);
	write_port(0xA0 , 0x11);

	/* ICW2 - remap offset address of IDT */
	/*
	 * In x86 protected mode, we have to remap the PICs beyond 0x20 because
	 * Intel have designated the first 32 interrupts as "reserved" for cpu exceptions
	 */
	write_port(0x21 , 0x20);
	write_port(0xA1 , 0x28);

	/* ICW3 - setup cascading */
	write_port(0x21 , 0x00);  
	write_port(0xA1 , 0x00);  

	/* ICW4 - environment info */
	write_port(0x21 , 0x01);
	write_port(0xA1 , 0x01);
	/* Initialization finished */

	/* mask interrupts */
	write_port(0x21 , 0xff);
	write_port(0xA1 , 0xff);

	/* fill the IDT descriptor */
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}

void kb_init(void)
{
	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21 , 0xFD);
}

void kprint(const char *str)
{
	unsigned int i = 0;
	while (str[i] != '\0') 
	{
		vidptr[current_loc++] = str[i++];
		vidptr[current_loc++] = 0x07;
	}
}

void kprint_newline(void)
{
	unsigned int line_size = BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void kprint_backspace(void)
{
	unsigned int i = current_loc - BYTES_FOR_EACH_ELEMENT;
	vidptr[i++] = ' ';
	vidptr[i++] = 0x07;
	current_loc -= BYTES_FOR_EACH_ELEMENT;
}

void clear_screen(void)
{
	unsigned int i = 0;
	while (i < SCREENSIZE) 
	{
		vidptr[i++] = ' ';
		vidptr[i++] = 0x07; 
	}
	current_loc = 0;
}

void keyboard_handler_main(void) 
{
	unsigned char status;
	char keycode;

	/* write EOI */
	write_port(0x20, 0x20);

	status = read_port(KEYBOARD_STATUS_PORT);
	/* Lowest bit of status will be set if buffer is not empty */
	if (status & 0x01) 
	{
		keycode = read_port(KEYBOARD_DATA_PORT);
		if(keycode < 0)
			return;
		if(keyboard_map[keycode] == '\n')
		{
			clear_screen();
			int i;
			char *str = "You entered: ";
			kprint(str);
			kprint(user_input_string);
			kprint_newline();
			for(i = 0 ; i < current_amount_of_characters ; i++)
			{
				user_input_string[i] = '\0';
			}
			current_amount_of_characters = 0;
		}
		else if(keyboard_map[keycode] == '\b')
		{
			kprint_backspace();
		}
		else
		{
			vidptr[current_loc++] = keyboard_map[keycode];
			vidptr[current_loc++] = 0x07;
			user_input_string[current_amount_of_characters] = keyboard_map[keycode];
			current_amount_of_characters++;
		}
	}
}

void kmain(void)
{
	char *str = "Preparing Operating System";
	clear_screen();
	kprint(str);
	kprint_newline();
	kprint_newline();

	idt_init();
	kb_init();

	while(1);
}
