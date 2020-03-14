#ifndef _SIM_H
#define _SIM_H

#include <pthread.h>

struct listener {
	struct device *dev;
	char *input;
	void (*function)(struct device *, char *, unsigned long long);
	struct listener *next;
};

struct talker {
	struct bus *bus;
	char *output;
	struct talker *next;
};

struct bus {
	char *name;
	unsigned long long value;
	struct listener *listeners;
	struct bus *next;
};

struct signal {
	char *name;
	unsigned char value;
	struct signal *next;
};

struct device {
	char *name;
	void *handle;
	void *context;
	pthread_t thread;
	struct talker *talkers;
	struct device *next;
};

extern struct bus *busses;
extern struct signal *signals;
extern struct device *devices;

extern void setBus(struct bus *bus, unsigned long long value);
extern void setOutput(struct device *dev, char *name, unsigned long long value);
extern unsigned long long getInput(struct device *dev, char *name);
extern void addBus(char *name);
extern void addDevice(char *name, char *lib);
extern struct device *getDeviceByName(char *name);
extern int parseLine(char *line, char *args[], int max);
extern int loadConfig(char *filename);
extern void loadHex16(char *filename, unsigned short *store, int start, int end);

#define INPUT 1
#define OUTPUT 2

#endif
