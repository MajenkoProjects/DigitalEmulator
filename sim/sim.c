#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include "config.h"
#include "sim.h"

struct bus *busses = NULL;
struct signal *signals = NULL;
struct device *devices = NULL;

unsigned char h2d(unsigned char hex)
{
	if(hex > 0x39) hex -= 7; // adjust for hex letters upper or lower case
	return(hex & 0xf);
}

unsigned char h2d2(unsigned char h1, unsigned char h2)
{
	return (h2d(h1)<<4) | h2d(h2);
}

unsigned short h2d4be(unsigned char h1, unsigned char h2, unsigned char h3, unsigned char h4)
{
	return (h2d(h1)<<12) | (h2d(h2)<<8) | (h2d(h3)<<4) | h2d(h4);
}

unsigned short h2d4le(unsigned char h1, unsigned char h2, unsigned char h3, unsigned char h4)
{
	return (h2d(h3)<<12) | (h2d(h4)<<8) | (h2d(h1)<<4) | h2d(h2);
}

void loadHex16(char *filename, unsigned short *store, int start, int end)
{
	FILE *hexfile;
	char buffer[1024];
	unsigned char rlen;
	unsigned short address;
	unsigned char rtype;
	unsigned char csum,exp;
	unsigned char i;
	unsigned short value;
	unsigned short slice;
	unsigned char coff;
	unsigned char words;

	hexfile = fopen(filename,"r");
	if(!hexfile)
	{
		printf("Error: unable to open %s\n",filename);
		return;
	}

	while(fgets(buffer,1023,hexfile))
	{
		while(buffer[strlen(buffer)-1]<=' ')
			buffer[strlen(buffer)-1] = 0;
		if(buffer[0]==':')
		{
			csum = 0;
			rlen = h2d2(buffer[1],buffer[2]);
			address = h2d4be(buffer[3],buffer[4],buffer[5],buffer[6]);
			rtype = h2d2(buffer[7],buffer[8]);

			for(i=1; i<strlen(buffer)-2; i+=2)
			{
				csum += h2d2(buffer[i],buffer[i+1]);
			}
			csum = 0x100 - csum;
			exp = h2d2(buffer[9+rlen*2],buffer[10+rlen*2]);
			if(exp != csum)
			{
				printf("Error: checksum error in hex file %s\n",filename);
				fclose(hexfile);
				return;
			}

			switch(rtype)
			{
				case 0:
					coff=9;
					slice = address/2;
					words = rlen/2;
					for(i=0; i<words; i++)
					{
						value = h2d4le(
							buffer[coff],
							buffer[coff+1],
							buffer[coff+2],
							buffer[coff+3]
						);
						if(slice>=start && slice<=end)
							store[slice++]=value;
						coff+=4;
					}
					break;
				case 1:
					fclose(hexfile);
					return;
			}
		}
	}
	fclose(hexfile);
}

void setBus(struct bus *bus, unsigned long long value)
{
	struct listener *scan;
	bus->value = value;
	for(scan = bus->listeners; scan; scan = scan->next)
	{
		if(scan->function)
		{
			scan->function(scan->dev,scan->input,bus->value);
		}
	}
}

void setOutput(struct device *dev, char *name, unsigned long long value)
{
	struct talker *scan;
	for(scan = dev->talkers; scan; scan = scan->next)
	{
		if(!strcmp(name,scan->output))
		{
			setBus(scan->bus,value);
		}
	}
}

unsigned long long getInput(struct device *dev, char *name)
{
	struct bus *bscan;
	struct listener *lscan;
	for(bscan=busses; bscan; bscan=bscan->next)
	{
		for(lscan=bscan->listeners; lscan; lscan=lscan->next)
		{
			if(lscan->dev == dev)
			{
				if(!strcmp(name,lscan->input))
				{
					return bscan->value;
				}
			}
		}
	}
	return 0;
}

void addBus(char *name)
{
	struct bus *new;
	struct bus *scan;

	new = malloc(sizeof(struct bus));
	new->name = strdup(name);
	new->next = NULL;
	if(!busses)
	{
		busses = new;
	} else {
		for(scan = busses; scan->next; scan = scan->next);
		scan->next = new;
	}
	printf("core: Added bus %s\n",
		name);
}

void addSignal(char *name)
{
	struct signal *new;
	struct signal *scan;

	new = malloc(sizeof(struct signal));
	new->name = strdup(name);
	new->value = 0;
	new->next = NULL;
	if(!signals)
	{
		signals = new;
	} else {
		for(scan = signals; scan->next; scan = scan->next);
		scan->next = new;
	}
	printf("core: Added signal %s\n",
		name);
}

void addDevice(char *name, char *lib)
{
	struct device *new;
	struct device *scan;

    char temp[4096];

	void *(*ctxfunc)(void);

	new = malloc(sizeof(struct device));
	new->name = strdup(name);
	new->thread = (pthread_t)NULL;

	new->handle = dlopen(lib,RTLD_LAZY|RTLD_LOCAL);

    if (!new->handle) {
        snprintf(temp, 4096, "%s/%s", LIBDIR, lib);
        new->handle = dlopen(temp, RTLD_LAZY|RTLD_LOCAL);
    }

	if(!new->handle)
	{
		printf("Error: cannot load %s\n",lib);
		exit(10);
	}
	new->next = NULL;
	ctxfunc = dlsym(new->handle,"create");
	if(!ctxfunc)
	{
		new->context = NULL;
	} else {
		new->context = ctxfunc();
	}
	
	if(!devices)
	{
		devices = new;
	} else {
		for(scan = devices; scan->next; scan = scan->next);
		scan->next = new;
	}
	printf("core: Added device %s with library %s\n",
		name,lib);
}

struct device *getDeviceByName(char *name)
{
	struct device *scan;
	for(scan=devices; scan; scan=scan->next)
	{
		if(!strcmp(name,scan->name))
		{
			return scan;
		}
	}
	return NULL;
}

struct bus *getBusByName(char *name)
{
	struct bus *scan;
	for(scan=busses; scan; scan=scan->next)
	{
		if(!strcmp(name,scan->name))
		{
			return scan;
		}
	}
	return NULL;
}

int parseLine(char *line, char *args[], int max)
{
	char *pos;
	int argc = 1;

	args[0] = line;

	pos = line;
	while(*pos && (argc<max))
	{
		while(*pos>' ')
			pos++;
		*(pos) = 0;
		pos++;
		while((*pos<=' ') && *pos)
			pos++;
		if(*pos)
		{
			args[argc]=pos;
			argc++;
		}
	}
	return argc;
}

void connectBus(struct device *dev, struct bus *bus, char *io, unsigned char dir)
{
	struct listener *newlistener;
	struct listener *listenscan;
	
	struct talker *newtalker;
	struct talker *talkerscan;

	void *(*func)(char *);

	if(dir & INPUT)
	{
		newlistener = malloc(sizeof(struct listener));
		newlistener->dev = dev;
		func = dlsym(dev->handle,"listener");
		if(func)
		{
			newlistener->function = func(io);
		} else {	
			newlistener->function = NULL;
		}
		newlistener->input = strdup(io);
		newlistener->next = NULL;
		if(!bus->listeners)
		{
			bus->listeners = newlistener;
		} else {
			for(listenscan = bus->listeners; listenscan->next; listenscan = listenscan->next);
			listenscan->next = newlistener;
		}
	}

	if(dir & OUTPUT)
	{
		newtalker = malloc(sizeof(struct talker));
		newtalker->output = strdup(io);
		newtalker->bus = bus;
		newtalker->next = NULL;

		if(!dev->talkers)
		{
			dev->talkers = newtalker;
		} else {
			for(talkerscan = dev->talkers; talkerscan->next; talkerscan = talkerscan->next);
			talkerscan->next = newtalker;
		}
	}
}

int loadConfig(char *filename)
{
	char inbuf[2048];
	char *start;
	char *end;

	char *argv[30];
	int argc;
	
	int line;

	FILE *config;

	void *(*run)(void *);
	void (*func2)(struct device *,char *, char *);

	config = fopen(filename,"r");
	if(!config)
	{
		fprintf(stderr,"Error: Unable to open %s for reading.\n",
			filename);
		return -ENOENT;
	}
	line = 0;
	while(fgets(inbuf,2047,config))
	{
		line++;
		for(start = inbuf; (*start <= ' ') && *start; start++);
		for(end = start; (*end >= ' ') && *end; end++);
		*end = 0;
		if(start == end)
			continue;
		if(*start == ';')
			continue;
		if(*start == '#')
			continue;
		argc = parseLine(start,argv,30);
		if(argc>0)
		{
			if(!strcmp(argv[0],"bus"))
			{
				if(argc!=2)
				{
					printf("Syntax error in line %d\n",line);
					return -EINVAL;
				}
				addBus(argv[1]);
			}
			if(!strcmp(argv[0],"signal"))
			{
				if(argc!=2)
				{
					printf("Syntax error in line %d\n",line);
					return -EINVAL;
				}
				addSignal(argv[1]);
			}
			if(!strcmp(argv[0],"device"))
			{
				if(argc!=3)
				{
					printf("Syntax error in line %d\n",line);
					return -EINVAL;
				}
				addDevice(argv[1], argv[2]);
			}

			if(!strcmp(argv[0],"config"))
			{
				struct device *dev;
				if(argc!=4)
				{
					printf("Syntax error in line %d\n",line);
					return -EINVAL;
				}
				dev = getDeviceByName(argv[1]);
				if(dev==NULL)
				{
					printf("Error in line %d: Cannot find device %s\n",line,argv[1]);
					return -EINVAL;
				}
				func2 = dlsym(dev->handle,"config");
				if(!func2)
				{
					printf("Error in line %d: Bad library format for device %s\n",line,argv[1]);
					return -EINVAL;
				}
				func2(dev,argv[2],argv[3]);
			}

			if(!strcmp(argv[0],"connect"))
			{
				struct device *dev;
				struct bus *bus;
				unsigned char dir = 0;

				if(argc!=5)
				{
					printf("Syntax error in line %d\n",line);
					return -EINVAL;
				}

				dev = getDeviceByName(argv[1]);
				if(dev==NULL)
				{
					printf("Error in line %d: Cannot find device %s\n",line,argv[1]);
					return -EINVAL;
				}
				bus = getBusByName(argv[3]);
				if(bus==NULL)
				{
					printf("Error in line %d: Cannot find bus %s\n",line,argv[3]);
					return -EINVAL;
				}

				if(!strcmp(argv[4],"in"))
					dir = INPUT;
				if(!strcmp(argv[4],"out"))
					dir = OUTPUT;
				if(!strcmp(argv[4],"inout"))
					dir = INPUT | OUTPUT;

				if(dir == 0)
				{
					printf("Syntax error in line %d\n",line);
					return -EINVAL;
				}

				connectBus(dev,bus,argv[2],dir);
			}

			if(!strcmp(argv[0],"set"))
			{
				struct bus *bus;
				if(argc!=3)
				{
					printf("Syntax error in line %d\n",line);
					return -EINVAL;
				}
			
				bus = getBusByName(argv[1]);
				if(bus==NULL)
				{
					printf("Error in line %d: Cannot find bus %s\n",line,argv[3]);
					return -EINVAL;
				}
				setBus(bus,atoll(argv[2]));
			}
		
			if(!strcmp(argv[0],"start"))
			{
				struct device *dev;
				if(argc!=2)
				{
					printf("Syntax error in line %d\n",line);
					return -EINVAL;
				}
				dev = getDeviceByName(argv[1]);
				if(dev==NULL)
				{
					printf("Error in line %d: Cannot find device %s\n",line,argv[1]);
					return -EINVAL;
				}
				run = dlsym(dev->handle,"run");
				if(!run)
				{
					printf("Error in line %d: Bad library format for device %s\n",line,argv[1]);
					return -EINVAL;
				}
				pthread_create(&(dev->thread),NULL,run,(void *)dev);
			}

/*
			printf(">%s<\n",argv[0]);
			for(i=1; i<argc; i++)
			{
				printf("    [%s]\n",argv[i]);
			}
			printf("\n");
*/
		}
	}
	fclose(config);
	return 0;
}

int main(int argc, char *argv[])
{
	int rc;
	struct device *dev;
	if(argc<2)
	{
		fprintf(stderr,"Usage: %s <config file>\n",argv[0]);
		return 10;
	}

	rc = loadConfig(argv[1]);
	if(rc<0)
	{
		return -rc;
	}

	for(dev = devices; dev; dev=dev->next)
	{
		if(dev->thread)
			pthread_join(dev->thread,NULL);
	}

	return 0;
}
