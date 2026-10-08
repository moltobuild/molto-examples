/* What a consumer of the recipe sees: SDL links, reports the version pinned,
   and lists the video and audio drivers it was built with. Nothing is opened,
   so it runs on a machine with no display. */
#include <SDL3/SDL.h>

#include <stdio.h>

int main(void) {
    const int version = SDL_GetVersion();
    printf("SDL %d.%d.%d on %s\n", SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version),
           SDL_VERSIONNUM_MICRO(version), SDL_GetPlatform());
    printf("video:");
    for (int i = 0; i < SDL_GetNumVideoDrivers(); i++)
        printf(" %s", SDL_GetVideoDriver(i));
    printf("\naudio:");
    for (int i = 0; i < SDL_GetNumAudioDrivers(); i++)
        printf(" %s", SDL_GetAudioDriver(i));
    printf("\ngpu:");
    for (int i = 0; i < SDL_GetNumGPUDrivers(); i++)
        printf(" %s", SDL_GetGPUDriver(i));
    printf("\n");
    return version == SDL_VERSION ? 0 : 1;
}
