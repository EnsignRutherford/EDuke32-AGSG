/* Minimal SDL2/KMSDRM HDMI isolation test for the Atari GameStation Go.
 *
 * Usage: gsg-hdmi-test [--display N] [--mode window|surface|renderer]
 *                      [--width W] [--height H] [--seconds S]
 *
 * Logs every stage to stdout. If it crashes, the last STEP line identifies
 * the failing SDL call. Press any key or wait for the timeout to exit.
 */

#include <SDL2/SDL.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char g_step[96] = "startup";

static void crash_handler(int sig)
{
    static const char prefix[] = "\nCRASH during step: ";
    static const char suffix[] = "\n";
    (void)sig;
    (void)!write(STDERR_FILENO, prefix, sizeof(prefix) - 1);
    (void)!write(STDERR_FILENO, g_step, strlen(g_step));
    (void)!write(STDERR_FILENO, suffix, sizeof(suffix) - 1);
    _exit(128 + SIGSEGV);
}

static void step(const char *name)
{
    snprintf(g_step, sizeof(g_step), "%s", name);
    printf("STEP %s\n", name);
    fflush(stdout);
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [--display N] [--mode window|surface|renderer] "
            "[--width W] [--height H] [--seconds S]\n", argv0);
}

int main(int argc, char **argv)
{
    int display_index = 1;
    int width = 0, height = 0, seconds = 15;
    const char *mode = "surface";

    setvbuf(stdout, NULL, _IOLBF, 0);

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--display") && i + 1 < argc)
            display_index = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mode") && i + 1 < argc)
            mode = argv[++i];
        else if (!strcmp(argv[i], "--width") && i + 1 < argc)
            width = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--height") && i + 1 < argc)
            height = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc)
            seconds = atoi(argv[++i]);
        else
        {
            usage(argv[0]);
            return 64;
        }
    }

    signal(SIGSEGV, crash_handler);
    signal(SIGBUS, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGILL, crash_handler);

    step("SDL_Init(SDL_INIT_VIDEO)");
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("SDL version %d.%d.%d, video driver '%s'\n",
           SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
           SDL_GetCurrentVideoDriver());

    step("enumerate displays");
    int const num_displays = SDL_GetNumVideoDisplays();
    printf("SDL_GetNumVideoDisplays() = %d\n", num_displays);

    for (int d = 0; d < num_displays; d++)
    {
        SDL_Rect bounds = { 0, 0, 0, 0 };
        SDL_DisplayMode desktop = { 0, 0, 0, 0, NULL };
        SDL_GetDisplayBounds(d, &bounds);
        SDL_GetDesktopDisplayMode(d, &desktop);
        printf("display %d: name='%s' bounds=%dx%d+%d+%d desktop=%dx%d@%dHz\n",
               d, SDL_GetDisplayName(d), bounds.w, bounds.h, bounds.x, bounds.y,
               desktop.w, desktop.h, desktop.refresh_rate);

        int const num_modes = SDL_GetNumDisplayModes(d);
        printf("display %d: %d modes:", d, num_modes);
        for (int m = 0; m < num_modes && m < 16; m++)
        {
            SDL_DisplayMode dm = { 0, 0, 0, 0, NULL };
            if (SDL_GetDisplayMode(d, m, &dm) == 0)
                printf(" %dx%d@%d", dm.w, dm.h, dm.refresh_rate);
        }
        printf("%s\n", num_modes > 16 ? " ..." : "");
    }

    if (display_index < 0 || display_index >= num_displays)
    {
        fprintf(stderr, "Requested display %d out of range (0..%d)\n",
                display_index, num_displays - 1);
        return 2;
    }

    if (width <= 0 || height <= 0)
    {
        SDL_DisplayMode desktop = { 0, 0, 0, 0, NULL };
        SDL_GetDesktopDisplayMode(display_index, &desktop);
        width = desktop.w;
        height = desktop.h;
    }
    printf("Using display %d at %dx%d, mode '%s'\n", display_index, width, height, mode);

    step("SDL_CreateWindow");
    SDL_Window *window = SDL_CreateWindow("gsg-hdmi-test",
                                          SDL_WINDOWPOS_CENTERED_DISPLAY(display_index),
                                          SDL_WINDOWPOS_CENTERED_DISPLAY(display_index),
                                          width, height, 0);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 3;
    }

    step("SDL_SetWindowFullscreen(FULLSCREEN)");
    if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) < 0)
        fprintf(stderr, "SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());

    int frame_limit = seconds * 60;
    Uint32 const start = SDL_GetTicks();
    int quit = 0;

    if (!strcmp(mode, "window"))
    {
        step("window-only idle loop (modeset only, no presentation)");
        while (!quit && SDL_GetTicks() - start < (Uint32)seconds * 1000)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                if (ev.type == SDL_QUIT || ev.type == SDL_KEYDOWN)
                    quit = 1;
            SDL_Delay(100);
        }
    }
    else if (!strcmp(mode, "surface"))
    {
        step("SDL_GetWindowSurface (EDuke32 8bpp path)");
        SDL_Surface *surface = SDL_GetWindowSurface(window);
        if (!surface)
        {
            fprintf(stderr, "SDL_GetWindowSurface failed: %s\n", SDL_GetError());
            return 4;
        }
        printf("window surface: %dx%d pitch=%d bpp=%d format=%s\n",
               surface->w, surface->h, surface->pitch, surface->format->BitsPerPixel,
               SDL_GetPixelFormatName(surface->format->format));

        for (int frame = 0; !quit && frame < frame_limit; frame++)
        {
            step("SDL_FillRect");
            Uint32 const color = (frame / 60) & 1
                ? SDL_MapRGB(surface->format, 0, 128, 0)
                : SDL_MapRGB(surface->format, 0, 0, 160);
            SDL_FillRect(surface, NULL, color);

            step("SDL_UpdateWindowSurface");
            if (SDL_UpdateWindowSurface(window) < 0)
            {
                fprintf(stderr, "SDL_UpdateWindowSurface failed: %s\n", SDL_GetError());
                break;
            }

            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                if (ev.type == SDL_QUIT || ev.type == SDL_KEYDOWN)
                    quit = 1;
        }
        printf("presented %d frames via window surface\n", frame_limit);
    }
    else if (!strcmp(mode, "renderer"))
    {
        step("SDL_CreateRenderer(SOFTWARE)");
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer)
        {
            fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
            return 5;
        }
        SDL_RendererInfo info;
        SDL_GetRendererInfo(renderer, &info);
        printf("renderer: '%s'\n", info.name);

        for (int frame = 0; !quit && frame < frame_limit; frame++)
        {
            step("SDL_RenderClear");
            if ((frame / 60) & 1)
                SDL_SetRenderDrawColor(renderer, 0, 128, 0, 255);
            else
                SDL_SetRenderDrawColor(renderer, 160, 0, 0, 255);
            SDL_RenderClear(renderer);

            step("SDL_RenderPresent");
            SDL_RenderPresent(renderer);

            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                if (ev.type == SDL_QUIT || ev.type == SDL_KEYDOWN)
                    quit = 1;
        }
        printf("presented %d frames via software renderer\n", frame_limit);
    }
    else
    {
        fprintf(stderr, "Unknown mode '%s'\n", mode);
        usage(argv[0]);
        return 64;
    }

    step("cleanup and exit");
    SDL_Quit();
    printf("OK\n");
    return 0;
}
