// Developer tool: plays the Linux build of the port on real TH12 1.00b files
// with scripted input and saves frames as PNG. Needs your own game files; it
// is not part of the host tests.
//   th12_play_real DATA_DIR OUT_DIR FRAMES [MODE STAGE CHARACTER SHOT DIFFICULTY] [EVERY] [TICKS] [LIVES]
//   th12_play_real DATA_DIR OUT_DIR menus   (a tour of every menu, one PNG each)
//   th12_play_real DATA_DIR OUT_DIR extras  (record and save a replay, play it
//       back and compare, Player Data, the title demo, an ending and staff roll)
//   th12_play_real DATA_DIR OUT_DIR fullgame MODE CHARACTER SHOT DIFFICULTY CONTINUE TICKS
//       a whole game through its natural flow (lives kept topped up): every
//       stage, the ending and staff roll or the Extra result, name entry, the
//       save page; CONTINUE=1 loses once on stage 1 first (a continued run)
// MODE: 0 game, 1 practice, 2 extra, started through the title menus (Practice
// and Extra are unlocked in DATA_DIR's scoreth12.dat first). Frames are saved
// every EVERY frames; TICKS logical ticks per drawn frame; LIVES=1 keeps the
// life count topped up. New games use a fixed seed and the scripted input
// follows frames of play, so a run repeats exactly (compare builds by their
// PNGs). TH12_PROFILE=1 prints CPU time per frame of play every 600 frames.
#include "../src/Archive.hpp"
#include "../src/Audio.hpp"
#include "../src/Game.hpp"
#include "../src/Platform.hpp"
#include "../src/Profile.hpp"
#include "../src/Replay.hpp"
#include "../core/game/GameState.hpp"
#include "../core/game/StageFlow.hpp"
#include "../core/wasm/Api.hpp"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

using namespace th12host;
static bool write_png(const std::string& path,const u8* rgba,u32 w,u32 h,u32 step);

int main(int argc,char** argv){
    // th12_play_real DATA audio: decode every effect and mix the stage 1 theme.
    if((argc==3||argc==4)&&std::string(argv[2])=="audio"){
        char* fake[]{argv[0],argv[1]};host::locate_data(2,fake);SDL_Init(0);
        Archive archive;std::vector<u8> exe;ExeTables tables;
        if(!archive.open(host::paths().data_file("th12.dat"))||!host::read_file(host::paths().data_file("th12.exe"),exe,1<<26)||!tables.load(exe))return 1;
        Audio audio;if(!audio.initialize(archive,tables))return 1;
        std::printf("music tracks: %d:",audio.track_count());
        for(int n=0;n<30;++n){char name[16];std::snprintf(name,sizeof name,"th12_%02d.wav",n);const int at=audio.track_index(name);if(at>=0)std::printf(" [%d]=%s",at,name);}std::printf("\n");
        for(const auto& r:tables.sounds)std::printf("%d:%s(%d%s) ",r.id,r.file.c_str(),r.volume,r.loop?",loop":"");std::printf("\n");
        if(argc==4&&std::string(argv[3]).find(',')!=std::string::npos){ // "a,b": play a for a second, then b
            const std::string list=argv[3];audio.play_music(list.substr(0,list.find(',')));std::vector<float> warm(44100*2);audio.mix(warm.data(),44100);audio.play_music(list.substr(list.find(',')+1));}
        else if(argc==4)audio.play_music(std::string(argv[3]));else audio.play_music(1);std::printf("playing %s\n",audio.music_name().c_str());std::vector<float> pcm(44100*2);double energy=0;
        for(int second=0;second<3;++second){if(!audio.mix(pcm.data(),44100))return 1;for(float v:pcm)energy+=double(v)*v;}
        std::printf("music RMS over 3 s: %.4f\n",std::sqrt(energy/(3*44100*2)));
        audio.stop_music();audio.effect(0,0,false);audio.mix(pcm.data(),4410);double e2=0;for(int i=0;i<4410*2;++i)e2+=double(pcm[size_t(i)])*pcm[size_t(i)];
        std::printf("effect 0 RMS: %.4f\n",std::sqrt(e2/(4410*2)));return 0;
    }
    if(argc<4){std::fprintf(stderr,"usage: %s DATA OUT FRAMES|menus [MODE STAGE CHARACTER SHOT DIFFICULTY] [EVERY] [TICKS] [LIVES]\n",argv[0]);return 2;}
    const std::string out=argv[2];const bool replay2=std::string(argv[3])=="replay2",tour=std::string(argv[3])=="menus",only_ending=std::string(argv[3])=="ending",extras=only_ending||std::string(argv[3])=="extras",full=std::string(argv[3])=="fullgame";
    const int frames=tour||extras||full||replay2?0:std::atoi(argv[3]);
    const int mode=argc>4?std::atoi(argv[4]):0,stage=argc>5?std::atoi(argv[5]):1,character=argc>6?std::atoi(argv[6]):0,shot=argc>7?std::atoi(argv[7]):0,difficulty=argc>8?std::atoi(argv[8]):1,every=argc>9?std::atoi(argv[9]):600,ticks=argc>10?std::atoi(argv[10]):1;const bool lives=argc>11&&std::atoi(argv[11]);
    char* fake[]{argv[0],argv[1]};host::locate_data(2,fake);host::make_directory(out);std::setvbuf(stdout,nullptr,_IOLBF,0);
    const int full_mode=full&&argc>4?std::atoi(argv[4]):0,full_character=full&&argc>5?std::atoi(argv[5]):0,full_shot=full&&argc>6?std::atoi(argv[6]):0,
        full_difficulty=full&&argc>7?std::atoi(argv[7]):1,full_continue=full&&argc>8?std::atoi(argv[8]):0,full_ticks=full&&argc>9?std::atoi(argv[9]):4;
    if(full&&full_mode==2){Profile p;p.load(host::paths().save);p.record_clear(full_character,full_shot,1,0,6);p.save();}
    if(!full&&(mode==1||mode==2)){
        Profile p;p.load(host::paths().save);
        if(mode==1)for(int st=1;st<=6;++st)p.record_stage_clear(character,shot,difficulty,st,0);else p.record_clear(character,shot,1,0,6);
        p.save();
    }
    if(SDL_Init(SDL_INIT_VIDEO)!=0)return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_Window* window=SDL_CreateWindow("th12",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);SDL_GLContext context=SDL_GL_CreateContext(window);
    if(!context){std::fprintf(stderr,"no GLES context\n");return 1;}
    Game game;if(!game.boot()){std::fprintf(stderr,"boot: %s\n",game.error.c_str());return 1;}
    if(frames)game.fixed_seed=0x1234; // scripted runs repeat exactly
    double t=0;int shot_index=0;
    const auto save=[&](const char* tag){const u8* p=game.gpu.read_pixels();if(!p)return;char name[256];std::snprintf(name,sizeof name,"%s/%03d-%s.png",out.c_str(),shot_index++,tag);
        write_png(name,p,640*game.gpu.scale(),480*game.gpu.scale(),game.gpu.scale());std::printf("saved %s (%s, stage %d, frame %d)\n",name,game.phase_name(),th12::stageNumber,th12::frame);};
    u32 per_frame=1;
    // Pad buttons by number: 0 B (shot/OK), 1 A (bomb/cancel), 2 L (focus), 4 + (pause).
    constexpr u32 B=1,A=2,L=4,Plus=16;
    const auto pad=[&](u32 buttons,u32 directions){t+=1000/60.0*per_frame;if(!game.frame({buttons,directions},per_frame,t)){std::fprintf(stderr,"stopped: %s\n",game.error.c_str());std::exit(1);}};
    // The game's input word as the default Key Config maps it.
    const auto run=[&](u32 held){pad((held&InShot?B:0)|(held&InBomb?A:0)|(held&InFocus?L:0)|(held&InPause?Plus:0),held&0xf0);};
    const auto phase=[&]{return std::string(game.phase_name());};
    const auto idle=[&](int n){for(int i=0;i<n;++i)pad(0,0);};
    const auto until=[&](const char* name,int limit){for(int i=0;i<limit&&phase()!=name;++i)pad(0,0);if(phase()!=name){std::fprintf(stderr,"expected %s, at %s\n",name,phase().c_str());save("unexpected");std::exit(1);}};
    const auto press=[&](u32 buttons,u32 directions=0,int wait=40){pad(buttons,directions);idle(wait);};
    for(int i=0;i<60;++i)pad(0,0);save("startup");
    until("title",600);idle(30);save("title-intro");idle(200);save("title");
    if(replay2){
        // Easy Reimu A, no help: record stages 1 and 2 (bombs when a bullet is
        // close), lose on purpose, save; then watch from stage 1 and from stage 2.
        press(B);press(0,InUp,10);press(B);press(B);press(B,0,1);until("playing",400);
        int f=0;i32 stage2_score=-1;
        for(;f<40000&&std::string(game.phase_name())!="name";++f){
            const std::string phase=game.phase_name();if(phase!="playing"){idle(1);continue;}
            if(th12::stageNumber==2&&stage2_score<0){stage2_score=th12::score;std::printf("stage 2 reached at %d, score %d\n",f,th12::score*10);}
            if(th12::stageNumber==2&&f%1000==0)std::printf("  stage 2 frame %d\n",f);
            u32 held=InShot;const int c=(f/90)%4;if(c==1)held|=InLeft;else if(c==3)held|=InRight;if((f/240)%2)held|=InFocus;
            if(const auto* boss=th12::primaryBoss()){held&=~u32(InLeft|InRight);const float dx=boss->x-th12::px/128.f;if(dx<-8)held|=InLeft;else if(dx>8)held|=InRight;}
            if(th12::dialogue)held=(f%4<1)?InShot:0;
            if(stage2_score>=0&&th12::stageNumber==2&&f%600==599)held|=InBomb;
            // Help only on stage 1 (that part of the replay desyncs); stage 2
            // is played as recorded and starts from its own header.
            if(th12::stageNumber==1&&th12::lives<3)th12::lives=3;
            run(held);
            if(th12::stageNumber==2&&stage2_score>=0&&std::string(game.phase_name())=="playing"&&th12::fullGameFlow.status==th12::FullGameStatus::Playing&&f>40000-2)break;
            if(stage2_score>=0&&f>=0&&th12::stageNumber==2&&th12::frame>3000)th12::ended=2;
        }
        std::printf("recording ended at frame %d, stage %d, score %d, phase %s\n",f,th12::stageNumber,th12::score*10,game.phase_name());
        if(std::string(game.phase_name())=="playing")th12::ended=2;
        until("name",60);idle(25);for(int i=0;i<200&&std::string(game.phase_name())=="name";++i)press(B,0,8);
        until("continue",120);idle(20);press(0,InDown,10);press(0,InDown,10);press(B,0,30);press(B,0,30);press(B,0,30);press(A,0,30);until("title",120);idle(30);
        Replay saved;{std::vector<u8> b;std::string why;if(!host::read_file(host::paths().save+"/replay/th12_01.rpy",b,1<<26)||!parse_replay(b,saved,why)){std::fprintf(stderr,"replay not saved: %s\n",why.c_str());return 1;}}
        std::printf("saved replay: %zu stages; final score %d\n",saved.stages.size(),saved.info.score_stored*10);
        for(const auto& st:saved.stages)std::printf("  stage %d: %u frames, starts at score %d\n",st.number,st.frames,st.initial.score*10);
        for(int from:{2,1}){
            press(0,InDown,10);press(0,InDown,10);press(B,0,40);press(B,0,40);for(int i=1;i<from;++i)press(0,InDown,10);press(B,0,1);until("replay-playing",400);
            i32 last=0,at_boundary=-1;int g=0,stage=th12::stageNumber;
            for(;g<60000&&std::string(game.phase_name())!="title";++g){
                if(std::string(game.phase_name())=="replay-playing"){if(th12::stageNumber!=stage){std::printf("  playback reached stage %d; score at the boundary %d\n",th12::stageNumber,at_boundary*10);stage=th12::stageNumber;}last=th12_score_stored();if(stage==1)at_boundary=last;}
                run(0);}
            std::printf("playback from stage %d: final score %d, recorded %d -> %s\n",from,last*10,saved.info.score_stored*10,last==saved.info.score_stored?"MATCH":"MISMATCH");
            idle(30);
            // The title remembers Replay; go back up to Start for the next pass.
            press(0,InUp,10);press(0,InUp,10);
        }
        game.shutdown();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
    }
    if(full){
        if(full_mode==2)press(0,InDown,10);press(B);until("difficulty",60);idle(10);
        if(full_mode!=2){for(int i=1;i<full_difficulty;++i)press(0,InDown,10);for(int i=full_difficulty;i<1;++i)press(0,InUp,10);}
        press(B);until("character",60);idle(10);for(int i=0;i<full_character;++i)press(0,InDown,10);press(B);until("shot",60);idle(10);
        for(int i=0;i<full_shot;++i)press(0,InDown,10);press(B,0,1);until("playing",400);
        std::string last_phase;int last_stage=0,stage_frame=0,continued=0,f=0;const double begin=host::seconds();bool done=false;
        for(;f<400000&&!done;++f){
            const std::string phase=game.phase_name();
            if(phase!=last_phase||th12::stageNumber!=last_stage){
                std::printf("[%7d] %-14s stage %d score %d lives %d continues %d (%.0fs)\n",f,phase.c_str(),th12::stageNumber,th12::score*10,th12::lives,th12::fullGameFlow.continueCount,host::seconds()-begin);
                if(th12::stageNumber!=last_stage)stage_frame=f;
                if(phase!=last_phase&&phase!="playing"&&phase!="loading"){idle(30);save(phase.c_str());}
                last_phase=phase;last_stage=th12::stageNumber;}
            if(phase=="playing"){
                per_frame=u32(full_ticks);
                u32 held=InShot;const int c=(f/90)%4;if(c==1)held|=InLeft;else if(c==3)held|=InRight;if((f/240)%2)held|=InFocus;
                if(const auto* boss=th12::primaryBoss()){held&=~u32(InLeft|InRight);const float dx=boss->x-th12::px/128.f;if(dx<-8)held|=InLeft;else if(dx>8)held|=InRight;}
                if(th12::dialogue)held=(f%4<1)?InShot:0;
                if(full_continue&&!continued&&th12::stageNumber==(full_mode==2?7:1)&&f-stage_frame==300){continued=1;th12::ended=2;}
                else if(th12::lives<3&&!(full_continue&&!continued))th12::lives=3; // testing only: keep the run going
                run(held);
                if((f-stage_frame)%3000==1500){char tag[32];std::snprintf(tag,sizeof tag,"stage%d",th12::stageNumber);save(tag);}
                per_frame=1;continue;}
            per_frame=1;
            if(phase=="loading"){idle(1);continue;}
            if(phase=="ending"){run((f%40)==0?InShot:0);if(f%1200==0)save("ending");continue;}
            if(phase=="name"){press(B,0,10);continue;}
            if(phase=="continue"){press(B,0,20);continue;}
            if(phase=="result-menu"){press(B,0,20);continue;}
            if(phase=="replay-save"){press(A,0,20);continue;}
            if(phase=="title"){done=true;break;}
            idle(1);
        }
        std::printf("finished at %s after %d frames (%.0fs)\n",game.phase_name(),f,host::seconds()-begin);
        {Profile p;p.load(host::paths().save);const auto t=p.totals(full_character,full_shot,full_mode==2?4:full_difficulty);
            std::printf("profile: clears %u, plays %u; ending flags:",t.clear_count,t.play_count);for(int e=0;e<12;++e)std::printf(" %d",int(p.ending_seen(e)));
            std::printf("; extra unlocked %d\n",int(p.can_extra(full_character,full_shot)));}
        game.shutdown();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
    }
    if(extras&&!only_ending){
        // A Reimu A run: the bot plays stage 1 (no help), then a forced game over.
        press(B);press(B);press(B);press(B,0,1);until("playing",400);
        int f=0;for(;f<4000&&std::string(game.phase_name())=="playing";++f){
            u32 held=InShot;const int c=(f/90)%4;if(c==1)held|=InLeft;else if(c==3)held|=InRight;if((f/240)%2)held|=InFocus;
            if(const auto* boss=th12::primaryBoss()){held&=~u32(InLeft|InRight);const float dx=boss->x-th12::px/128.f;if(dx<-8)held|=InLeft;else if(dx>8)held|=InRight;}
            if(th12::dialogue)held=(f%8<2)?InShot:0;
            run(held);if(f==1500)save("recording");}
        std::printf("recorded %d frames, stage %d, score %d, phase %s\n",f,th12::stageNumber,th12::score*10,game.phase_name());
        if(std::string(game.phase_name())=="playing"){th12::ended=2;}
        until("name",60);idle(25);save("score-ranking");
        // A qualifying score types "SW" on the grid (S = 18, W = 22), else Next.
        for(int i=0;i<200&&std::string(game.phase_name())=="name";++i){press(B,0,8);}
        until("continue",120);idle(20);save("continue");
        press(0,InDown,10);press(0,InDown,10);press(B,0,30);save("replay-save");press(B,0,30);save("replay-name");press(B,0,30);save("replay-saved");
        press(A,0,30);until("title",120);idle(30);
        Replay saved;{std::vector<u8> b;std::string why;if(!host::read_file(host::paths().save+"/replay/th12_01.rpy",b,1<<26)||!parse_replay(b,saved,why)){std::fprintf(stderr,"replay not saved: %s\n",why.c_str());return 1;}}
        std::printf("saved replay: %zu stages, %u frames, score %d\n",saved.stages.size(),saved.stages[0].frames,saved.info.score_stored*10);
        // Play it back: the score where the recording stops must match.
        press(0,InDown,10);press(0,InDown,10);press(B,0,40);save("replay-list");press(B,0,40);save("replay-stage");press(B,0,1);until("replay-playing",400);
        i32 last_score=0;int g=0;for(;g<20000&&std::string(game.phase_name())=="replay-playing";++g){last_score=th12_score_stored();run(0);if(g==600)save("replay-playback");
            if(g==900){press(Plus,0,25);save("replay-pause");press(Plus,0,20);}}
        std::printf("playback: %d frames; score at the end %d, recorded %d -> %s\n",g,last_score*10,saved.info.score_stored*10,last_score==saved.info.score_stored?"MATCH":"MISMATCH");
        until("title",400);idle(30);
        // Player Data: score page, then the first spell card page.
        press(0,InDown,10);press(B,0,40);save("player-data");press(B,0,20);save("player-data-cards");press(A,0,30);until("title",120);idle(30);
        // The title demo after half a minute idle (demo2.rpy, a retail replay).
        until("demo",2000);for(int i=0;i<3500&&std::string(game.phase_name())=="demo";++i){run(0);if(i==300)save("demo-a");if(i==2000)save("demo-b");}
        std::printf("demo: ended at %s, lives %d\n",game.phase_name(),th12::lives);
        until("title",400);idle(30);
        // The title still remembers Player Data: up to Practice Start, then past Extra to Start.
        press(0,InUp,10);press(0,InUp,10);press(0,InUp,10);
    }
    if(extras){
        // An ending: a Reimu A run told stage 6 is cleared.
        press(B);press(B);press(B);press(B,0,1);until("playing",400);idle(60);
        th12::score=500000; // testing only: a score that enters the ranking
        th12::fullGameFlow.stage=6;th12::ended=1;until("ending",900);
        // Real-time pacing (no vsync here), so sound can be checked.
        for(int i=0;i<12000&&std::string(game.phase_name())=="ending";++i){const double t0=host::seconds();run(i%180==170?InShot:0);const double spent=host::seconds()-t0;if(spent<1/60.)SDL_Delay(u32((1/60.-spent)*1000));if(i%600==300){char tag[32];std::snprintf(tag,sizeof tag,"ending-%05d",i);save(tag);}}
        std::printf("ending: left at %s\n",game.phase_name());idle(40);save("after-ending");
        // Name entry: S (18) and W (22) on the 13-column grid, then End (90).
        press(0,InDown,8);press(0,InDown,8);for(int i=0;i<7;++i)press(0,InLeft,8);press(B,0,8);
        for(int i=0;i<4;++i)press(0,InRight,8);press(B,0,8);save("name-entry");
        for(int i=0;i<3;++i)press(0,InRight,8);press(0,InUp,8);press(0,InUp,8);press(B,0,30);
        std::printf("after the name: %s\n",game.phase_name());save("after-name");
        press(A,0,20);until("title",120);idle(30);
        // Player Data now lists the run (the title remembers Start).
        press(0,InUp,10);press(0,InUp,10);press(0,InUp,10);press(0,InUp,10);press(B,0,40);save("player-data-ranked");
        game.shutdown();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
    }
    if(tour){
        // Start: difficulty, character, shot, then back out with A.
        press(B);save("difficulty");press(0,InDown,20);save("difficulty-hard");press(0,InUp,10);press(B);save("character");press(0,InDown,20);save("character-marisa");
        press(B);save("shot");press(A);press(A);press(A);until("title",60);idle(20);
        // Music Room (Start -> Quit -> Option -> Music Room): the stage 1 theme
        // shows the unheard-track warning first.
        for(int i=0;i<3;++i)press(0,InUp,10);press(B);until("music-room",60);idle(20);save("music-room");
        press(0,InDown,10);press(B,0,30);save("music-warning");press(B,0,120);save("music-playing");
        press(A);until("title",60);idle(20);
        // Option and Key Config.
        press(0,InDown,10);press(B);until("options",60);idle(20);save("options");press(0,InLeft,10);save("options-bgm-95");press(0,InRight,10);
        press(0,InDown,10);press(0,InDown,10);press(B);until("key-config",60);idle(20);save("key-config");
        press(A);press(A);until("options",60);press(A);press(A);until("title",60);idle(20);
        // Option -> Quit -> Start, then a Normal Reimu A run.
        press(0,InDown,10);save("title-quit-row");press(0,InDown,10);
        press(B);press(B);press(B);press(B,0,1);until("playing",400);idle(300);save("playing");
        // Pause, the Return to Title confirmation (starts on No), back, resume.
        press(Plus,0,30);save("pause");press(0,InDown,10);press(B);save("pause-confirm");press(A,0,30);save("pause-again");press(Plus,0,30);
        // Game over -> continue page -> continue.
        idle(60);th12::ended=2;until("name",30);idle(30);save("ranking");press(B,0,10);until("continue",30);idle(40);save("continue");press(B,0,60);save("continued");
        // Pause -> Return to Title -> Yes.
        press(Plus,0,30);press(0,InDown,10);press(B);press(0,InUp,10);press(B,0,5);until("title",120);idle(60);save("back-to-title");
        game.shutdown();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
    }
    // Title rows: Start, Extra Start, Practice Start, Replay, Player Data,
    // Music Room, Option, Quit; only a locked Extra Start is skipped. Extra
    // is one down from Start, Practice six up.
    if(mode==2)press(0,InDown,10);
    if(mode==1)for(int i=0;i<6;++i)press(0,InUp,10);
    save("title-row");press(B);until("difficulty",60);idle(10);
    if(mode!=2){for(int i=1;i<difficulty;++i)press(0,InDown,10);for(int i=difficulty;i<1;++i)press(0,InUp,10);}
    save("difficulty");press(B);until("character",60);idle(10);
    for(int i=0;i<character;++i)press(0,InDown,10);save("character");press(B);until("shot",60);idle(10);
    for(int i=0;i<shot;++i)press(0,InDown,10);save("shot");
    if(mode==1){press(B);until("practice",60);idle(20);for(int i=1;i<stage;++i)press(0,InDown,10);save("practice");}
    press(B,0,1);
    const auto key=[&](u32 k){run(k);run(0);};
    const double begin=host::seconds();int played=0;per_frame=u32(std::max(1,ticks));
    for(int f=0;f<frames;++f){
        const std::string phase=game.phase_name();
        if(phase=="continue"){save(phase.c_str());per_frame=1;for(int i=0;i<30;++i)run(0);key(InShot);per_frame=u32(std::max(1,ticks));continue;}
        if(phase=="result-menu"||phase=="title"){for(int i=0;i<40;++i)run(0);save(phase.c_str());break;}
        // Hold shot, sweep left and right, focus half the time; under a boss,
        // steer below it. During dialogue: advance with shot every half
        // second, and save the first line of each conversation. Timed by
        // frames of play, not loop frames (a load takes a varying number of
        // those), so a run repeats exactly.
        const int g=played/std::max(1,ticks);
        u32 held=InShot;const int c=(g/90)%4;if(c==1)held|=InLeft;else if(c==3)held|=InRight;if((g/240)%2)held|=InFocus;
        if(const auto* boss=th12::primaryBoss()){held&=~u32(InLeft|InRight);const float dx=boss->x-th12::px/128.f;if(dx<-8)held|=InLeft;else if(dx>8)held|=InRight;}
        static bool talking=false;
        if(th12::dialogue){held=(g%8<2)?InShot:0;if(!talking){talking=true;for(int i=0;i<30;++i)run(0);save("dialogue");
            // TH12_TRACE_DIALOGUE: every frame of the first conversation, with
            // one-frame presses, a held press and a held skip.
            static bool traced=false;if(std::getenv("TH12_TRACE_DIALOGUE")&&!traced){traced=true;
                for(int i=0;i<160;++i){const u32 k=(i==20||i==60)?InShot:(i>=100&&i<110)?InShot:(i>=130&&i<140)?InSkip:0;run(k);char tag[32];std::snprintf(tag,sizeof tag,"trace-%03d%s",i,k?"-key":"");save(tag);}}}}else talking=false;
        static bool seen_loading=false;if(phase=="loading"&&!seen_loading){seen_loading=true;save("loading");}
        static bool paused=false;if(!paused&&phase=="playing"&&g>=every*2+7){paused=true;per_frame=1;key(InPause);for(int i=0;i<20;++i)run(0);save("paused");key(InPause);per_frame=u32(std::max(1,ticks));}
        if(lives&&th12::lives<3)th12::lives=3; // testing only: keep the run going
        run(held);if(phase=="playing")played+=int(per_frame);
        // TH12_PROFILE: CPU ms per displayed frame of play over each 600
        // frames (mean / worst) for the core ticks, the capture and the draw
        // submission, ticks + capture percentiles, and draw calls / vertices.
        static const bool profiling=std::getenv("TH12_PROFILE")!=nullptr;
        if(profiling&&phase=="playing"){
            static double sum[3]{},worst[3]{};static u64 draws=0,vertices=0;static u32 most_draws=0,n=0,bossy=0;static std::vector<double> totals;
            for(int i=0;i<3;++i){sum[i]+=game.frame_timing[i];worst[i]=std::max(worst[i],game.frame_timing[i]);}
            totals.push_back(game.frame_timing[0]+game.frame_timing[1]);draws+=game.gpu.draws;vertices+=game.gpu.vertices;most_draws=std::max(most_draws,game.gpu.draws);bossy+=th12::primaryBoss()!=nullptr;
            if(++n==600){
                std::sort(totals.begin(),totals.end());const auto pct=[&](double q){return totals[std::min(totals.size()-1,size_t(q*double(totals.size())))];};
                std::printf("profile stage %d frame %5d boss %3u%%: tick %.2f/%.2f capture %.2f/%.2f draw %.2f/%.2f ms; tick+capture median %.2f, 95%% %.2f, 99%% %.2f; draws %llu/%u, vertices %llu\n",
                    th12::stageNumber,th12::frame,bossy*100/n,sum[0]/n,worst[0],sum[1]/n,worst[1],sum[2]/n,worst[2],pct(.5),pct(.95),pct(.99),(unsigned long long)(draws/n),most_draws,(unsigned long long)(vertices/n));
                for(int i=0;i<3;++i)sum[i]=worst[i]=0;totals.clear();draws=vertices=0;most_draws=n=bossy=0;}
        }
        static int next_shot=every;if(played/std::max(1,ticks)>=next_shot){next_shot+=every;save("play");}
    }
    std::printf("%d frames (%d playing) in %.1fs; stage %d, score %d\n",frames,played,host::seconds()-begin,th12::stageNumber,th12::score*10);
    save("end");game.shutdown();SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);SDL_Quit();return 0;
}

// Minimal PNG (stored deflate), downscaled by `step` to 640x480.
static u32 crc(const u8* d,size_t n,u32 c=0xffffffff){for(size_t i=0;i<n;++i){c^=d[i];for(int k=0;k<8;++k)c=c&1?0xedb88320^(c>>1):c>>1;}return c;}
static bool write_png(const std::string& path,const u8* rgba,u32 w,u32 h,u32 step){
    const u32 ow=w/step,oh=h/step;std::vector<u8> raw;
    for(u32 y=0;y<oh;++y){raw.push_back(0);for(u32 x=0;x<ow;++x){const u8* p=rgba+(size_t(y*step)*w+x*step)*4;raw.insert(raw.end(),{p[0],p[1],p[2]});}}
    std::vector<u8> z{0x78,1};
    for(size_t at=0;at<raw.size();){const size_t n=std::min<size_t>(65535,raw.size()-at);z.push_back(at+n==raw.size());z.push_back(u8(n));z.push_back(u8(n>>8));z.push_back(u8(~n));z.push_back(u8(~n>>8));z.insert(z.end(),raw.begin()+i64(at),raw.begin()+i64(at+n));at+=n;}
    u32 a=1,b=0;for(u8 c:raw){a=(a+c)%65521;b=(b+a)%65521;}const u32 ad=b<<16|a;for(int i=3;i>=0;--i)z.push_back(u8(ad>>(8*i)));
    std::vector<u8> png{0x89,'P','N','G',13,10,26,10};
    const auto chunk=[&](const char* type,const std::vector<u8>& body){for(int i=3;i>=0;--i)png.push_back(u8(body.size()>>(8*i)));std::vector<u8> t(type,type+4);t.insert(t.end(),body.begin(),body.end());png.insert(png.end(),t.begin(),t.end());const u32 c=~crc(t.data(),t.size());for(int i=3;i>=0;--i)png.push_back(u8(c>>(8*i)));};
    std::vector<u8> ih;for(u32 v:{ow,oh})for(int i=3;i>=0;--i)ih.push_back(u8(v>>(8*i)));ih.insert(ih.end(),{8,2,0,0,0});chunk("IHDR",ih);chunk("IDAT",z);chunk("IEND",{});
    return host::write_file(path,png);
}
