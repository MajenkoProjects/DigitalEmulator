#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sim/sim.h>
#include <SDL2/SDL.h>

/*
 * RGB LED simulator
 */

struct context {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
    SDL_Window *window;
    SDL_Renderer *renderer;
};


void init_sdl(struct context *c) {
    SDL_Init(SDL_INIT_EVERYTHING);
    c->window = SDL_CreateWindow("LED",
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                64,
                                64,
                                SDL_WINDOW_SHOWN
    );

    c->renderer = SDL_CreateRenderer(c->window, -1, SDL_RENDERER_ACCELERATED);
    if (!c->renderer) {
        printf("Unable to make renderer\n");
        return;
    }
    SDL_SetRenderDrawColor(c->renderer, 0, 255, 0, 255);
    SDL_RenderClear(c->renderer);
    SDL_RenderPresent(c->renderer);

    atexit(SDL_Quit);
};

void close_sdl(struct context *c) 
{
    SDL_Quit();
}

void *create(void)
{
	struct context *ctx;
	ctx = malloc(sizeof(struct context));
	return (void *)ctx;
}

void setRed(struct device *dev, char *in, unsigned long long value)
{
	struct context *ctx = (struct context *)dev->context;
	ctx->red = value==0 ? 0 : 255;	
}

void setGreen(struct device *dev, char *in, unsigned long long value)
{
	struct context *ctx = (struct context *)dev->context;
	ctx->green = value==0 ? 0 : 255;	
}

void setBlue(struct device *dev, char *in, unsigned long long value)
{
	struct context *ctx = (struct context *)dev->context;
	ctx->blue = value==0 ? 0 : 255;	
}

void *listener(unsigned char *n)
{
	if(!strcmp(n,"red"))
	{
		return (void *)setRed;
	}
	if(!strcmp(n,"green"))
	{
		return (void *)setGreen;
	}
	if(!strcmp(n,"blue"))
	{
		return (void *)setBlue;
	}
	return NULL;
}

// Config interface.  Receives key and value.

void config(struct device *dev, unsigned char *key, unsigned char *value)
{
}

void run(struct device *dev)
{
    struct context *ctx = (struct context *)dev->context;
    init_sdl(ctx);
    printf("led: running\n");
    while (1) {
        SDL_Event e;
        if (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                exit(0);
            }
        }
        SDL_SetRenderDrawColor(ctx->renderer, ctx->red, ctx->green, ctx->blue, 255);
        SDL_RenderClear(ctx->renderer);
        SDL_RenderPresent(ctx->renderer);
        SDL_Delay(1);
    }
}

