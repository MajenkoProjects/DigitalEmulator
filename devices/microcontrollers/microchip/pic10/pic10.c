#include <stdio.h>
#include <stdlib.h>
#include <sim/sim.h>
#include <string.h>

const unsigned short tmr0_ps[] = { 2,4,8,16,32,64,128,256 };

//#define DEBUG 1

#if DEBUG
#define SPRINTF(...) sprintf(__VA_ARGS__)
#else
#define SPRINTF(...)
#endif

struct context {

	// Control flags
	union {
		unsigned short flags;
		struct {
			unsigned reset:1;
		};
	};

	unsigned char W;
	unsigned short PC;
	unsigned short WatchdogTimer;
	unsigned short stack[2];
	unsigned short config;
	struct {
		unsigned long long frequency;
		unsigned long long delay;
	} intrc;
	unsigned short flash[512];
	union {
		unsigned char ram[32];
		struct {
			unsigned char INDF;
			unsigned char TMR0;
			unsigned char PCL;
			union {
				struct {
					unsigned C:1;
					unsigned DC:1;
					unsigned Z:1;
					unsigned TO:1;
					unsigned PD:1;
					unsigned NA:1;
					unsigned CWUF:1;
					unsigned GPWUF:1;
				};
				unsigned char STATUS;
			}__attribute__((packed));
			unsigned char FSR;
			unsigned char OSCCAL;
			union {
				unsigned char GPIO;
				struct {
					unsigned GPIO0:1;
					unsigned GPIO1:1;
					unsigned GPIO2:1;
					unsigned GPIO3:1;
					unsigned GPIOUnused:4;
				};
			}__attribute__((packed));
			unsigned char CMCON0;

			unsigned char general[24];
		}__attribute__((packed));
	};
	union {
		unsigned char OPTION;
		struct {
			union {
				unsigned PS:3;
				struct {
					unsigned PS0:1;
					unsigned PS1:1;
					unsigned PS2:1;
				};
			}__attribute__((packed));
			unsigned PSA:1;
			unsigned T0SE:1;
			unsigned T0CS:1;
			unsigned GPPU:1;
			unsigned GPWU:1;
		};
	}__attribute__((packed));
	unsigned char GPIOCN;
	unsigned char Tcy4;
	unsigned short PSVal;
	unsigned char TRIS;
	struct {
		unsigned long ips;
	} stats;
};

void *create(void)
{
	struct context *ctx;
	ctx = malloc(sizeof(struct context));
	ctx->reset = 0;
	return (void *)ctx;
}

void readGPIO0(struct device *dev, char *name, unsigned long long value)
{
	struct context *ctx = (struct context *)dev->context;
	
	if(ctx->TRIS & 0x0001)
		ctx->GPIO0 = value==0 ? 0 : 1;
}

void readGPIO1(struct device *dev, char *name, unsigned long long value)
{
	struct context *ctx = (struct context *)dev->context;
	
	if(ctx->TRIS & 0x0010)
		ctx->GPIO1 = value==0 ? 0 : 1;
}

void readGPIO2(struct device *dev, char *name, unsigned long long value)
{
	struct context *ctx = (struct context *)dev->context;
	
	if(ctx->TRIS & 0x0100)
	{
		if(ctx->T0CS==1)
		{
			if(ctx->T0SE==0)
			{
				if(ctx->GPIO2==0 && value!=0) // Rising
				{
					if(ctx->PSA==0)
					{
						ctx->PSVal++;
						if(ctx->PSVal==tmr0_ps[ctx->PS])
						{
							ctx->PSVal=0;
							ctx->TMR0++;
						}
					} else {
						ctx->TMR0++;
					}
				}
			} else {
				if(ctx->GPIO2==1 && value==0) // Falling
				{
					if(ctx->PSA==0)
					{
						ctx->PSVal++;
						if(ctx->PSVal==tmr0_ps[ctx->PS])
						{
							ctx->PSVal=0;
							ctx->TMR0++;
						}
					} else {
						ctx->TMR0++;
					}
				}
			}
		}
		ctx->GPIO2 = value==0 ? 0 : 1;
	}
}

void readGPIO3(struct device *dev, char *name, unsigned long long value)
{
	struct context *ctx = (struct context *)dev->context;
	
	if(ctx->TRIS & 0x1000)
		ctx->GPIO3 = value==0 ? 0 : 1;

	ctx->reset = value==0 ? 0 : 1;
}

void *listener(char *io)
{
	if(!strcmp(io,"GPIO0"))
		return (void *)readGPIO0;
	if(!strcmp(io,"GPIO1"))
		return (void *)readGPIO1;
	if(!strcmp(io,"GPIO2"))
		return (void *)readGPIO2;
	if(!strcmp(io,"GPIO3"))
		return (void *)readGPIO3;

	return NULL;
}


// Config interface.  Receives key and value.

void config(struct device *dev, unsigned char *key, unsigned char *value)
{
	struct context *ctx = (struct context *)dev->context;
	if(!strcmp(key,"flash"))
	{
		loadHex16(value,(ctx->flash),0,511);
	}
    if(!strcmp(key,"frequency"))
    {
        ctx->intrc.frequency = atol(value);
        ctx->intrc.delay = (1000000000LLU/ctx->intrc.frequency)/2;
        printf("%s: Frequency: %lluHz Delay: %lluns\n",
            dev->name,
            ctx->intrc.frequency,
            ctx->intrc.delay
        );
    }
}

void subwf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file] - ctx->W;
	ctx->Z = temp==0 ? 1 : 0;
	ctx->C = temp > ctx->W ? 1 : 0;
	ctx->DC = temp < 16 && ctx->W >= 16 ? 1 : ctx->C;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"SUBWF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"SUBWF %d,W",file);
	}
	ctx->PC++;
}

void addwf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->W + ctx->ram[file];
	ctx->Z = temp==0 ? 1 : 0;
	ctx->C = temp < ctx->W ? 1 : 0;
	ctx->DC = temp >= 16 && ctx->W < 16 ? 1 : ctx->C;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"ADDWF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"ADDWF %d,W",file);
	}
	ctx->PC++;
}

void andwf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->W & ctx->ram[file];
	ctx->Z = temp==0 ? 1 : 0;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"ANDWF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"ANDWF %d,W",file);
	}
	ctx->PC++;
}

void clrf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	ctx->Z = 1;
	ctx->ram[file]=0;
	ctx->PC++;
	SPRINTF(b,"CLRF %d",file);
}

void clrw(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	ctx->Z = 1;
	ctx->W = 0;
	ctx->PC++;
	SPRINTF(b,"CLRW");
}

void comf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ~ctx->ram[file];
	ctx->Z = temp==0 ? 1 : 0;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"COMF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"COMF %d,W",file);
	}
	ctx->PC++;
}

void decf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file]-1;
	ctx->Z = temp==0 ? 1 : 0;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"DECF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"DECF %d,W",file);
	}
	ctx->PC++;
}

void decfsz(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file]-1;
	ctx->PC += temp==0 ? 1 : 0;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"DECFSZ %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"DECFSZ %d,W",file);
	}
	ctx->PC++;
}

void incf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file]+1;
	ctx->Z = temp==0 ? 1 : 0;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"INCF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"INCF %d,W",file);
	}
	ctx->PC++;
}

void incfsz(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file]+1;
	ctx->PC += temp==0 ? 1 : 0;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"INCFSZ %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"INCFSZ %d,W",file);
	}
	ctx->PC++;
}

void xorwf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file] ^ ctx->W;
	ctx->Z = temp==0 ? 1 : 0;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"XORWF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"XORWF %d,W",file);
	}
	ctx->PC++;
}

void iorwf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file] | ctx->W;
	ctx->Z = temp==0 ? 1 : 0;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"IORWF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"IORWF %d,W",file);
	}
	ctx->PC++;
}

void movf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file];
	ctx->Z = temp==0 ? 1 : 0;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"MOVF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"MOV %d,W",file);
	}
	ctx->PC++;
}

void movwf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	ctx->ram[file] = ctx->W;
	SPRINTF(b,"MOVWF %d",file);
	ctx->PC++;
}

void nop(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	SPRINTF(b,"NOP");
	ctx->PC++;
}

void rlf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file];
	unsigned char carr = ctx->C;
	ctx->C = temp & 0b10000000 ? 1 : 0;
	temp = (temp << 1) | carr;
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"RLF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"RLF %d,W",file);
	}
	ctx->PC++;
}

void rrf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file];
	unsigned char carr = ctx->C;
	ctx->C = temp & 0b1 ? 1 : 0;
	temp = (temp >> 1) | (carr<<7);
	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"RRF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"RRF %d,W",file);
	}
	ctx->PC++;
}

void swapf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char temp = ctx->ram[file];
	unsigned char upper, lower;

	upper = temp >> 4;
	lower = temp << 4;
	temp = upper | lower;

	if(instruction & 0b000000100000)
	{
		ctx->ram[file] = temp;
		SPRINTF(b,"SWAPF %d,F",file);
	} else {
		ctx->W = temp;
		SPRINTF(b,"SWAPF %d,W",file);
	}
	ctx->PC++;
}

void bcf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char bit = (instruction & 0b000011100000) >> 5;

	ctx->ram[file] &= ~(1<<bit);
	SPRINTF(b,"BCF %d,%d",file,bit);

	ctx->PC++;
}

void bsf(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char bit = (instruction & 0b000011100000) >> 5;

	ctx->ram[file] |= (1<<bit);
	SPRINTF(b,"BSF %d,%d",file,bit);

	ctx->PC++;
}

void btfsc(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char bit = (instruction & 0b000011100000) >> 5;

	if((ctx->ram[file] & (1<<bit)) == 0)
	{
		ctx->PC++;
	}
	SPRINTF(b,"BTFSC %d,%d",file,bit);

	ctx->PC++;
}

void btfss(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000011111;
	unsigned char bit = (instruction & 0b000011100000) >> 5;

	if((ctx->ram[file] & (1<<bit)) != 0)
	{
		ctx->PC++;
	}
	SPRINTF(b,"BTFSS %d,%d",file,bit);

	ctx->PC++;
}

void andlw(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char value = instruction & 0b000011111111;

	ctx->W &= value;
	ctx->Z = ctx->W==0 ? 1 : 0;
	SPRINTF(b,"ANDLW %d",value);

	ctx->PC++;
}

void xorlw(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char value = instruction & 0b000011111111;

	ctx->W ^= value;
	ctx->Z = ctx->W==0 ? 1 : 0;
	SPRINTF(b,"XORLW %d",value);

	ctx->PC++;
}

void iorlw(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char value = instruction & 0b000011111111;

	ctx->W |= value;
	ctx->Z = ctx->W==0 ? 1 : 0;
	SPRINTF(b,"IORLW %d",value);

	ctx->PC++;
}

void movlw(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char value = instruction & 0b000011111111;

	ctx->W = value;
	SPRINTF(b,"MOVLW %d",value);

	ctx->PC++;
}

void retlw(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char value = instruction & 0b000011111111;
	
	ctx->W = value;
	ctx->PC = ctx->stack[0];
	ctx->stack[0] = ctx->stack[1];
	SPRINTF(b,"RETLW %d",value);
}

void call(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char value = instruction & 0b000011111111;
	
	ctx->stack[1] = ctx->stack[0];
	ctx->stack[0] = ctx->PC+1;
	ctx->PC = value;
	SPRINTF(b,"CALL %d",value);
}

void _goto(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned short value = instruction & 0b000111111111;
	
	ctx->PC = value;
	SPRINTF(b,"GOTO %d",value);
}

void clrwdt(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	
	ctx->WatchdogTimer = 0;
	SPRINTF(b,"CLRWDT");
	ctx->PC++;
}

void option(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;

	ctx->OPTION = ctx->W;
	SPRINTF(b,"OPTION");
	ctx->PC++;
}

void _sleep(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;

	SPRINTF(b,"SLEEP");
	while(1);
	ctx->PC++;
}

void tris(struct device *dev, unsigned short instruction, char *b)
{
        struct context *ctx = (struct context *)dev->context;
	unsigned char file = instruction & 0b000000000111;

	if(file==6)
	{
		ctx->TRIS=ctx->W;
	}
	SPRINTF(b,"TRIS %d",file);
	ctx->PC++;
}

struct opcode {
	unsigned short opcode;
	unsigned short mask;
	void (*function)(struct device *, unsigned short, char *);
};

struct opcode opcodes[] = {
	{0b000000000000, 0b111111111111, nop},
	{0b000001000000, 0b111111111111, clrw},
	{0b000000000100, 0b111111111111, clrwdt},
	{0b000000000010, 0b111111111111, option},
	{0b000000000011, 0b111111111111, _sleep},

	{0b000000000000, 0b111111111000, tris},

	{0b000001100000, 0b111111100000, clrf},
	{0b000000100000, 0b111111100000, movwf},

	{0b000111000000, 0b111111000000, addwf},
	{0b000101000000, 0b111111000000, andwf},
	{0b001001000000, 0b111111000000, comf},
	{0b000011000000, 0b111111000000, decf},
	{0b001011000000, 0b111111000000, decfsz},
	{0b001010000000, 0b111111000000, incf},
	{0b001111000000, 0b111111000000, incfsz},
	{0b000100000000, 0b111111000000, iorwf},
	{0b001000000000, 0b111111000000, movf},
	{0b001101000000, 0b111111000000, rlf},
	{0b001100000000, 0b111111000000, rrf},
	{0b000010000000, 0b111111000000, subwf},
	{0b001110000000, 0b111111000000, swapf},
	{0b000110000000, 0b111111000000, xorwf},

	{0b010000000000, 0b111100000000, bcf},
	{0b010100000000, 0b111100000000, bsf},
	{0b011000000000, 0b111100000000, btfsc}, 
	{0b011100000000, 0b111100000000, btfss}, 
	{0b111000000000, 0b111100000000, andlw},
	{0b100100000000, 0b111100000000, call},
	{0b110100000000, 0b111100000000, iorlw},
	{0b110000000000, 0b111100000000, movlw},
	{0b100000000000, 0b111100000000, retlw},
	{0b111100000000, 0b111100000000, xorlw},

	{0b101000000000, 0b111000000000, _goto},

	{0,0,0}
};

void run(struct device *dev)
{
        struct context *ctx = (struct context *)dev->context;

	unsigned short instruction;
	unsigned char temp;
	unsigned short thisPC;
	char debugBuffer[80];
	struct opcode *scan;
	unsigned char i;
	unsigned long t;
	unsigned long ic;

#if DEBUG
    printf("[2J");
#endif

        printf("%s: running\n",dev->name);
        struct timespec ts;

	ctx->PC=0;
	ctx->W=0;
	ctx->PCL	= 0b11111111;
	ctx->STATUS	= 0b00011000;
	ctx->FSR	= 0b11100000;
	ctx->OSCCAL	= 0b11111110;
	ctx->CMCON0	= 0b11111111;
	ctx->TRIS	= 0b00001111;
	ctx->OPTION	= 0b11111111;

    ts.tv_sec = 0;
    ts.tv_nsec = ctx->intrc.delay;
	t = time(NULL);
	ic=0;
    while(1)
    {
		// Stop everything if reset is low.
		if(ctx->reset==0)
		{
            printf("pic10: Entered reset state\n");
			while(ctx->reset==0)
			{
				nanosleep(&ts,NULL);
			}
			ctx->PC=0;
			ctx->W=0;
			ctx->PCL	= 0b11111111;
			ctx->STATUS	= 0b00011000;
			ctx->FSR	= 0b11100000;
			ctx->OSCCAL	= 0b11111110;
			ctx->CMCON0	= 0b11111111;
			ctx->TRIS	= 0b00001111;
			ctx->OPTION	= 0b11111111;
            printf("pic10: Reset finished - resuming operation\n");
		}

		// Tcy1
		if(ctx->intrc.delay>0)
			nanosleep(&ts,NULL);
		ic++;
		if(t!=time(NULL))
		{
			t = time(NULL);
			ctx->stats.ips = ic;
			ic=0;
		}
		thisPC = ctx->PC;
		instruction = ctx->flash[thisPC];
		debugBuffer[0]=0;

		for(scan=opcodes; scan->function; scan++)
		{
			if((instruction & scan->mask) == scan->opcode)
			{
				scan->function(dev,instruction,debugBuffer);
				break;
			}
		}
		if(!scan->function)
		{
			SPRINTF(debugBuffer,"Unknown Opcode");
			ctx->PC++;
		}
		ctx->PCL = ctx->PC&0xFF;
#if DEBUG
		printf("[1;1H%04d %04X : %s[K\n",thisPC,instruction,debugBuffer);
		for(i=0; i<32; i++)
		{
			printf(" %3d",i);
		}

		printf("   W Status\n");

		for(i=0; i<32; i++)
		{
			printf(" %3d",ctx->ram[i]);
		}

		printf(" %3d",ctx->W);
		printf(" %c%c%c%c%c%c%c%c\n",
			ctx->GPWUF ? 'G' : '-',
			ctx->CWUF ? 'U' : '-',
			'-',
			ctx->TO ? 'T' : '-',
			ctx->PD ? 'P' : '-',
			ctx->Z ? 'Z' : '-',
			ctx->DC ? 'D' : '-',
			ctx->C ? 'C' : '-'
		);
		printf("\n");
#endif
		if(ctx->intrc.delay>0)
			nanosleep(&ts,NULL);
		if(ctx->GPIO != ctx->GPIOCN)
		{
			unsigned char chg = ctx->GPIO ^ ctx->GPIOCN;
			if((chg & 0b001) & ((ctx->TRIS & 0b001)==0))
				setOutput(dev,"GPIO0",ctx->GPIO & 0b001 ? 1 : 0);
			if((chg & 0b010) & ((ctx->TRIS & 0b010)==0))
				setOutput(dev,"GPIO1",ctx->GPIO & 0b010 ? 1 : 0);
			if((chg & 0b100) & ((ctx->TRIS & 0b010)==0))
				setOutput(dev,"GPIO2",ctx->GPIO & 0b100 ? 1 : 0);
			ctx->GPIOCN = ctx->GPIO;
		}
		
		if(ctx->T0CS==0) // Tcy4
		{
			ctx->Tcy4++;
			if(ctx->Tcy4==4)
			{
				ctx->Tcy4 = 0;
				if(ctx->PSA==0)
				{
					ctx->PSVal++;
					if(ctx->PSVal==tmr0_ps[ctx->PS])
					{
						ctx->PSVal=0;
						ctx->TMR0++;
					}
				} else {
					ctx->TMR0++;
				}
				if(ctx->TMR0==0)
				{
				}
			}
		}
        }
}


