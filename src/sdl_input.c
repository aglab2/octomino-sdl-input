/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdl_input.h"
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_main.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include "gui.h"

FILE *logfile;
char dbpath[PATH_MAX];

int initialized = 0;
SDL_Gamepad *con = NULL;
int joy_inst = -1;

void try_init(void)
{
    if (initialized) {
        dlog("Attempted initialize, but SDL is already initialized");
        return;
    }
    dlog("Initializing");

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD))
    {
        /* deal with the unnessessary initial controller connected
           events so they don't clog up the log file */
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

        int mapcount = SDL_AddGamepadMappingsFromFile(dbpath);
        if (mapcount == -1)
            dlog("    Unable to load mappings from %s", dbpath);
        else
            dlog("    Successfully loaded %d mappings from %s", mapcount, dbpath);

        initialized = 1;
        dlog("    ...done");
    }
    else
        dlog("    SDL has failed to initialize");
}

void deinit(void)
{
    if (!initialized) {
        return;
    }

    dlog("Deinitializing");

    con_close();
    SDL_QuitSubSystem(SDL_INIT_EVENTS | SDL_INIT_GAMEPAD);
    initialized = 0;
}

void con_open(void)
{
    dlog("Attempting to open a controller");

    if (!initialized) {
        dlog("...but SDL is not initialized yet");
        try_init();
    }

    if (!initialized) {
        dlog("Failed to open a controller: SDL not initialized");
        return;
    }

    if (con) {
        if (SDL_GamepadConnected(con)) {
            dlog("Failed to open a controller: a controller is already open and connected");
            return;
        } else {
            con_close();
        }
    }

    int count;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);
	if (!joysticks) {
		dlog("    Couldn't get joysticks");
        count = 0;
	}

    dlog("    # of joysticks: %d", count);

    // open the first available controller
    for (int i = 0; i < count; ++i)
    {
        SDL_JoystickID joystick = joysticks[i];
		if (!SDL_IsGamepad(joystick))
			continue;

		con = SDL_OpenGamepad(joystick);
        if (con != NULL)
        {
            dlog("    Found a viable controller: %s (joystick %d)", SDL_GetGamepadName(con), i);

            SDL_Joystick *joy = SDL_GetGamepadJoystick(con);
    
            joy_inst = SDL_GetJoystickID(joy);
            dlog("        Joystick instance ID: %d", joy_inst);

            SDL_GUID guid = SDL_GetJoystickGUID(joy);
            char guidstr[33];
            SDL_GUIDToString(guid, guidstr, sizeof(guidstr));
            dlog("        Joystick GUID: %s", guidstr);

            char *mapping = SDL_GetGamepadMapping(con);
            if (mapping != NULL) {
                dlog("        Controller mapping: %s", mapping);
                SDL_free(mapping);
            } else {
                dlog("        This controller has no mapping! Closing it");
                // skip this controller
                con_close();
                continue;
            }

            break;
        }
        else
            dlog("    Couldn't use joystick %d", i);
    }

    if (con == NULL)
        dlog("    Couldn't find a viable controller :(");
}

void con_close(void)
{
    if (!initialized && con != NULL) con = NULL;
    if (!initialized || con == NULL) {
        return;
    }

    dlog("Closing current controller");
    SDL_CloseGamepad(con);
    con = NULL;
    joy_inst = -1;
}

int16_t threshold(int16_t val, float cutoff)
{
    if (val < 0)
        return val >= -(cutoff * 32767) ? 0 : val;
    else
        return val <= (cutoff * 32767) ? 0 : val;
}

void scale_and_limit(int16_t *x, int16_t *y, float dz, float edge)
{
    // get abs value between 0 and 1 relative to deadzone and edge
    int16_t div = edge * 32767 - dz * 32767;
    if (div == 0) {
        return;
    }
    float fx = (abs(*x) - dz * 32767) / div;
    float fy = (abs(*y) - dz * 32767) / div;

    // out of range
    if (fx > 1.f) {
        fy = fy * (1.f / fx);
        fx = 1.f;
    }
    if (fy > 1.f) {
        fx = fx * (1.f / fy);
        fy = 1.f;
    }

    float sign_x = 0;
    float sign_y = 0;

    // deadzone
    if (*y != 0) {
        if (fy <= 0.f)
            fy = 0.f;
        else
            sign_y = abs(*y) / *y;
    }

    if (*x != 0) {
        if (fx <= 0.f)
            fx = 0.f;
        else 
            sign_x = abs(*x) / *x;
    }

    *x = sign_x * fx * 32767;
    *y = sign_y * fy * 32767;
}

int16_t sclamp(int16_t val, int16_t min, int16_t max)
{
    if (val <= min) return min;
    if (val >= max) return max;
    return val;
}

int16_t smin(int16_t val, int16_t min)
{
    if (val <= min) return min;
    return val;
}

int16_t smax(int16_t val, int16_t max)
{
    if (val >= max) return max;
    return val;
}

void con_get_inputs(inputs_t *i)
{
    if (!initialized)
    {
        dlog("Attempting to get inputs but SDL is not initialized");
        try_init();
        if (!initialized) {
            return;
        }
    }

    SDL_Event e;
    while (SDL_PollEvent(&e))
        switch (e.type)
        {
        case SDL_EVENT_GAMEPAD_ADDED:
            dlog("A device has been added");
            con_open();
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            dlog("A device has been removed");
            if (e.cdevice.which == joy_inst)
            {
                dlog("    ...it was the active controller");
                con_close();
            }
            else
                dlog("    ...it was not the active controller");
            break;
        }

    if (con != NULL)
        con_write_inputs(i);
}

static inline uint8_t con_get_but(SDL_GamepadButton b)
{
    return SDL_GetGamepadButton(con, b);
}

static inline int16_t con_get_axis(SDL_GamepadAxis a)
{
    return sclamp(SDL_GetGamepadAxis(con, a), -32767, 32767);
}

void con_write_inputs(inputs_t *i)
{
    i->a      = con_get_but(SDL_GAMEPAD_BUTTON_SOUTH);
    i->b      = con_get_but(SDL_GAMEPAD_BUTTON_EAST);
    i->x      = con_get_but(SDL_GAMEPAD_BUTTON_WEST);
    i->y      = con_get_but(SDL_GAMEPAD_BUTTON_NORTH);
    i->back   = con_get_but(SDL_GAMEPAD_BUTTON_BACK);
    i->guide  = con_get_but(SDL_GAMEPAD_BUTTON_GUIDE);
    i->start  = con_get_but(SDL_GAMEPAD_BUTTON_START);
    i->lstick = con_get_but(SDL_GAMEPAD_BUTTON_LEFT_STICK);
    i->rstick = con_get_but(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    i->lshoul = con_get_but(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    i->rshoul = con_get_but(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    i->dup    = con_get_but(SDL_GAMEPAD_BUTTON_DPAD_UP);
    i->ddown  = con_get_but(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    i->dleft  = con_get_but(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    i->dright = con_get_but(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

    i->alx    = con_get_axis(SDL_GAMEPAD_AXIS_LEFTX);
    i->aly    = con_get_axis(SDL_GAMEPAD_AXIS_LEFTY);
    i->arx    = con_get_axis(SDL_GAMEPAD_AXIS_RIGHTX);
    i->ary    = con_get_axis(SDL_GAMEPAD_AXIS_RIGHTY);
    i->altrig = con_get_axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    i->artrig = con_get_axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
}

void dlog(const char *fmt, ...)
{
    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);
    char buf[1024];

    char timestr[9];
    strftime(timestr, sizeof(timestr), "%X", timeinfo);

    fprintf(logfile, "[%s] ", timestr);

    va_list args;
    va_start(args, fmt);
    vfprintf(logfile, fmt, args);
    vsnprintf(buf, sizeof(buf), fmt, args);
    write_log(buf);
    va_end(args);

    fprintf(logfile, "\n");
}
