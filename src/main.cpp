// Touhou 12: Undefined Fantastic Object for Nintendo Switch: native entry
// point. Replaces upstream site/game.mjs's browser loop: the original files on
// the SD card, a vsync-paced 60 Hz loop and a Switch controller as the input
// word the core reads (shot, bomb, focus, skip, pause, directions).
#include "Game.hpp"
#include "Platform.hpp"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#ifdef __SWITCH__
#include <switch.h>
#endif

using namespace th12host;
namespace {
// Controls (the same defaults as every Touhou Switch port, re-assignable in
// the game's own Key Config):
//   B shot / confirm   A bomb / cancel   L/ZL focus   R/ZR skip dialogue
//   + pause   X Enter   Y Retry shortcut   stick or d-pad: move   − snapshot
// All connected controllers act together (handheld, grip, Pro Controller).
// Buttons are reported by pad-button number, as the original's joystick:
//   0 B  1 A  2 L/ZL  3 R/ZR  4 +  5 X  6 Y
struct Pad {PadSample sample;bool minus=false;};
#ifdef __SWITCH__
PadState pad;
void initialize_pads(){padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&pad);}
Pad read_pad(){
    padUpdate(&pad);Pad p;if(!padIsConnected(&pad))return p;const u64 held=padGetButtons(&pad);const auto stick=padGetStickPos(&pad,0);
    // Upstream's gamepad path: d-pad or a stick past 0.3.
    const float x=float(stick.x)/32767.f,y=-float(stick.y)/32767.f;auto& d=p.sample.directions;
    if(held&HidNpadButton_Left||x<-.3f)d|=InLeft;if(held&HidNpadButton_Right||x>.3f)d|=InRight;
    if(held&HidNpadButton_Up||y<-.3f)d|=InUp;if(held&HidNpadButton_Down||y>.3f)d|=InDown;
    const u64 map[7]{HidNpadButton_B,HidNpadButton_A,HidNpadButton_L|HidNpadButton_ZL,HidNpadButton_R|HidNpadButton_ZR,HidNpadButton_Plus,HidNpadButton_X,HidNpadButton_Y};
    for(u32 b=0;b<7;++b)if(held&map[b])p.sample.buttons|=1u<<b;
    p.minus=held&HidNpadButton_Minus;return p;
}
#else
// Desktop test build: arrows; Z, X, Shift, Ctrl, Esc, Enter, R as buttons 0..6; F12 snapshot.
void initialize_pads(){}
Pad read_pad(){
    const auto* k=SDL_GetKeyboardState(nullptr);Pad p;auto& d=p.sample.directions;
    if(k[SDL_SCANCODE_LEFT])d|=InLeft;if(k[SDL_SCANCODE_RIGHT])d|=InRight;if(k[SDL_SCANCODE_UP])d|=InUp;if(k[SDL_SCANCODE_DOWN])d|=InDown;
    const bool keys[7]{bool(k[SDL_SCANCODE_Z]),bool(k[SDL_SCANCODE_X]),k[SDL_SCANCODE_LSHIFT]||k[SDL_SCANCODE_RSHIFT],k[SDL_SCANCODE_LCTRL]||k[SDL_SCANCODE_RCTRL],
        bool(k[SDL_SCANCODE_ESCAPE]),bool(k[SDL_SCANCODE_RETURN]),bool(k[SDL_SCANCODE_R])};
    for(u32 b=0;b<7;++b)if(keys[b])p.sample.buttons|=1u<<b;p.minus=k[SDL_SCANCODE_F12];return p;
}
#endif

void fatal(const std::string& message){
    host::log("fatal: %s",message.c_str());
#ifdef __SWITCH__
    consoleInit(nullptr);
    std::printf("\n  Touhou 12: Undefined Fantastic Object - Switch port\n\n  Error: %s\n\n",message.c_str());
    std::printf("  Data folder: %s\n  Needed: th12.exe, th12.dat (ver 1.00b). Optional: thbgm.dat (music),\n  msgothic.ttc / msmincho.ttc (dialogue fonts).\n\n  Press + to exit.\n",host::paths().data.c_str());
    consoleUpdate(nullptr);
    PadState p;padInitializeDefault(&p);
    while(appletMainLoop()){padUpdate(&p);if(padGetButtonsDown(&p)&HidNpadButton_Plus)break;consoleUpdate(nullptr);}
    consoleExit(nullptr);
#else
    std::fprintf(stderr,"%s\n",message.c_str());
#endif
}
}

int main(int argc,char** argv){
    host::locate_data(argc,argv);
#ifdef __SWITCH__
    appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleep);
#endif
    initialize_pads();
    host::log("th12-switch: data %s",host::paths().data.c_str());
    if(!host::exists(host::paths().data_file("th12.dat"))){fatal("th12.dat not found");return 1;}
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0){fatal(std::string("SDL: ")+SDL_GetError());return 1;}
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
#ifdef __SWITCH__
    SDL_Window* window=SDL_CreateWindow("Touhou 12",0,0,1280,720,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN);
#else
    SDL_Window* window=SDL_CreateWindow("Touhou 12 (Switch port test build)",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,960,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
#endif
    if(!window){fatal(std::string("window: ")+SDL_GetError());SDL_Quit();return 1;}
    SDL_GLContext context=SDL_GL_CreateContext(window);
    if(!context){fatal(std::string("OpenGL ES 3: ")+SDL_GetError());SDL_DestroyWindow(window);SDL_Quit();return 1;}
    SDL_GL_SetSwapInterval(1);
    auto game=std::make_unique<Game>();
    const double begin=host::seconds();
    if(!game->boot()){const std::string why=game->error;game.reset();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();fatal(why);return 1;}
    host::log("booted in %.2fs",host::seconds()-begin);
    // Vsync paces one tick per 60 Hz swap; measured time only adds catch-up
    // ticks after a real stall (at most 4).
    constexpr double interval=1.0/60;double debt=0,previous=host::seconds();bool running=true,ok=true,snapshot_held=false;
    while(running
#ifdef __SWITCH__
          &&appletMainLoop()
#endif
    ){
        SDL_Event e;while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT)running=false;
            else if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)game->focus_lost();
        }
        const double now=host::seconds();debt+=std::clamp(now-previous,0.,.1);previous=now;
        u32 ticks=u32(std::floor((debt+interval*.5)/interval));ticks=std::min(4u,std::max(ticks,1u));debt=std::clamp(debt-ticks*interval,-interval*.5,.1);
        const Pad p=read_pad();
        if(!game->frame(p.sample,ticks,now*1000)){ok=game->error.empty();break;}
        if(p.minus&&!snapshot_held)game->snapshot();snapshot_held=p.minus;
        i32 w=0,h=0;SDL_GL_GetDrawableSize(window,&w,&h);
        const double shown=host::seconds();game->present(w,h);SDL_GL_SwapWindow(window);game->presented((host::seconds()-shown)*1000);
    }
    const std::string why=game->error;game.reset();
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();
    if(!ok){fatal(why);return 1;}
    return 0;
}
