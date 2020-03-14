#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sim/sim.h>
#include <string.h>

/*
 * Clock source simulator
 */

struct context {
	unsigned long frequency;
	unsigned long delay;
};

void *create(void)
{
	struct context *ctx;
	ctx = malloc(sizeof(struct context));
	return (void *)ctx;
}

// Config interface.  Receives key and value.

void config(struct device *dev, unsigned char *key, unsigned char *value)
{
	struct context *ctx;
	ctx = (struct context *)dev->context;
	if(!strcmp(key,"frequency"))
	{
		ctx->frequency = atol(value);
		ctx->delay = (1000000000LU/ctx->frequency)/2;
		printf("%s: Frequency: %lu Delay: %lu\n",
			dev->name,
			ctx->frequency,
			ctx->delay
		);
	}
}

// This is the function that gets started in a new thread to process the
// device.
void run(struct device *dev)
{
	struct context *ctx;
	ctx = (struct context *)dev->context;

	printf("clock: running\n");
	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = ctx->delay;
	while(1)
	{
		nanosleep(&ts,NULL);
		setOutput(dev,"clkout",1);
		nanosleep(&ts,NULL);
		setOutput(dev,"clkout",0);
	}
}
