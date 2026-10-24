// Host-side tests for the Switch port layer (Linux; no game data needed).
//   archive    - THA1 index and entries with TH12's cipher table, stored and LZSS
//   exe        - th12.exe tables (bullet types, sounds, camera planes) from a PE
//   anm        - ANM v7 entries, sprites, scripts and THTX formats 1/3/5/7
//   sampler    - the animation.mjs port: sprite, position, alpha fades, children
//   music      - thbgm.fmt layout and loop points; native volume curves
//   text       - CP932 decoding
//   profile    - scoreth12.dat / th12.cfg in the original formats: fresh files,
//                clears and unlocks, encode/decode round trip, foreign files
//   game       - the whole host on synthetic files: boot, sound output, the
//                title menus, stage 1, pause, stage clear -> stage 2, game
//                over -> ranking -> continue, saving and watching a replay,
//                Player Data, an ending and staff roll, Practice, Save Replay
//                and Quit, Options, Key Config, the demo, saves
//                (needs a display, e.g. Xvfb)
#include "../src/Animation.hpp"
#include "../src/Anm.hpp"
#include "../src/Archive.hpp"
#include "../src/Audio.hpp"
#include "../src/ExeTables.hpp"
#include "../src/Game.hpp"
#include "../src/Platform.hpp"
#include "../src/Profile.hpp"
#include "../src/Replay.hpp"
#include "../src/Ending.hpp"
#include "../src/TextRaster.hpp"
#include "../core/game/GameState.hpp"
#include "../core/game/StageFlow.hpp"
#include <SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

using namespace th12host;
static int failures=0;
#define CHECK(cond) do{if(!(cond)){std::printf("  FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond);++failures;}}while(0)

namespace {
using Bytes=std::vector<u8>;
void put16(Bytes& b,size_t at,u32 v){if(b.size()<at+2)b.resize(at+2);b[at]=u8(v);b[at+1]=u8(v>>8);}
void put32(Bytes& b,size_t at,u32 v){if(b.size()<at+4)b.resize(at+4);for(int i=0;i<4;++i)b[at+i]=u8(v>>(8*i));}
void putf(Bytes& b,size_t at,float f){u32 v;std::memcpy(&v,&f,4);put32(b,at,v);}
u32 bits(float f){u32 v;std::memcpy(&v,&f,4);return v;}
void append(Bytes& b,const Bytes& tail){b.insert(b.end(),tail.begin(),tail.end());}

// ANM instruction: op, size, time, mask, words.
Bytes ins(u16 op,i16 time,std::vector<u32> words={},u16 mask=0){Bytes b;put16(b,0,op);put16(b,2,u32(8+words.size()*4));put16(b,4,u16(time));put16(b,6,mask);for(size_t i=0;i<words.size();++i)put32(b,8+i*4,words[i]);return b;}
Bytes end_script(){Bytes b;put16(b,0,0xffff);put16(b,2,0);put16(b,4,0);put16(b,6,0);return b;}
struct Entry {u32 w=16,h=16,format=1;u32 argb=0xffffffff;std::vector<std::array<float,4>> sprites;std::vector<Bytes> scripts;};
// An ANM v7 file the way the original lays it out (header, sprite and script
// tables, name, sprites, scripts, THTX), one block per entry.
Bytes anm(const std::vector<Entry>& entries){
    Bytes out;
    for(size_t n=0;n<entries.size();++n){
        const auto& e=entries[n];Bytes b(64,0);
        put32(b,0,7);put16(b,4,u32(e.sprites.size()));put16(b,6,u32(e.scripts.size()));put16(b,10,e.w);put16(b,12,e.h);put16(b,14,e.format);
        const size_t tables=64+e.sprites.size()*4+e.scripts.size()*8;b.resize(tables,0);
        put32(b,16,u32(b.size()));for(char c:std::string("synthetic.png"))b.push_back(u8(c));b.push_back(0);while(b.size()%4)b.push_back(0);
        for(size_t i=0;i<e.sprites.size();++i){put32(b,64+i*4,u32(b.size()));const size_t at=b.size();put32(b,at,u32(i));for(int k=0;k<4;++k)putf(b,at+4+k*4,e.sprites[i][size_t(k)]);}
        for(size_t i=0;i<e.scripts.size();++i){put32(b,64+e.sprites.size()*4+i*8,u32(i));put32(b,64+e.sprites.size()*4+i*8+4,u32(b.size()));append(b,e.scripts[i]);append(b,end_script());}
        put32(b,28,u32(b.size()));put16(b,32,1);
        const size_t at=b.size();const u32 bpp=e.format==1?4:e.format==7?1:2;
        b.insert(b.end(),{'T','H','T','X'});put16(b,at+4,0);put16(b,at+6,e.format);put16(b,at+8,e.w);put16(b,at+10,e.h);put32(b,at+12,e.w*e.h*bpp);
        for(u32 i=0;i<e.w*e.h;++i){const u32 a=e.argb>>24,r=(e.argb>>16)&255,g=(e.argb>>8)&255,bl=e.argb&255;
            if(e.format==1){b.push_back(u8(bl));b.push_back(u8(g));b.push_back(u8(r));b.push_back(u8(a));}
            else if(e.format==3){const u32 v=(r>>3)<<11|(g>>2)<<5|(bl>>3);b.push_back(u8(v));b.push_back(u8(v>>8));}
            else if(e.format==5){const u32 v=(a>>4)<<12|(r>>4)<<8|(g>>4)<<4|(bl>>4);b.push_back(u8(v));b.push_back(u8(v>>8));}
            else b.push_back(u8(a));}
        if(n+1<entries.size())put32(b,36,u32(b.size()));
        append(out,b);
    }
    return out;
}
// A one-sprite, one-script bank: script 0 shows sprite 0 at (x, y) on `layer`.
Bytes simple_bank(u32 argb,float x,float y,u32 layer,float size=16){
    Entry e;e.argb=argb;e.sprites={{0,0,size,size}};e.w=e.h=u32(size);
    e.scripts={[&]{Bytes s=ins(68,0,{layer});append(s,ins(48,0,{bits(x),bits(y),bits(0)}));append(s,ins(3,0,{0}));append(s,ins(63,0));return s;}()};
    return anm({e});
}
// ASCII bank: 300 sprites over one white texture, enough for every font id.
Bytes ascii_bank(){Entry e;e.w=e.h=16;for(int i=0;i<300;++i)e.sprites.push_back({0,0,8,12});e.scripts={ins(63,0)};return anm({e});}
// text.anm: dialogue sprites 0..7 and the stage-surface/pause render targets.
Bytes text_bank(){Entry e;e.w=e.h=16;for(int i=0;i<8;++i)e.sprites.push_back({0,0,16,16});e.scripts={ins(63,0)};return anm({e});}
// ECL: one SCPT file with one sub (upstream tests/native/stage-flow.cpp's birth probe).
Bytes ecl(const char* sub,bool probe){
    Bytes e(220,0);put32(e,0,0x54504353);put16(e,4,1);put16(e,16,1);put32(e,36,48);std::memcpy(e.data()+40,sub,std::strlen(sub)+1);put32(e,48,0x484c4345);
    size_t at=64;
    const auto instruction=[&](int opcode,u32 operand,bool reference){put16(e,at+4,u32(opcode));put16(e,at+6,20);put16(e,at+8,reference?1:0);e[at+10]=63;e[at+11]=1;put32(e,at+16,operand);at+=20;};
    if(probe){instruction(42,u32(-9930),true);instruction(43,0,false);instruction(44,bits(-9991.f),true);instruction(45,bits(4.f),false);instruction(42,u32(-10000),true);instruction(43,8,false);}
    instruction(83,1000,false);put16(e,at+4,10);put16(e,at+6,16);e[at+10]=63;at+=16;
    e.resize(at);return e;
}
Bytes msg(){return {1,0,0,0,12,0,0,0,0,0,0,0,0,0,21,0,1,0,0,0};}
Bytes sht(){
    Bytes s(0x268,0);put16(s,0,3);put16(s,2,10);
    const float f[7]{2,5,60,4.5f,2,3.18f,1.41f};for(int i=0;i<7;++i)putf(s,4+i*4,f[i]);put32(s,32,4);
    const u32 table_end=0x268+10*8;for(int i=0;i<10;++i){put32(s,0x268+i*8,table_end);put32(s,0x268+i*8+4,0);}
    s.resize(table_end);s.push_back(0xff);return s;
}
// STD: one object with one quad (bank 10 script 0, 256x256 at the origin),
// one instance, camera at (0,0,-400) looking +z, fog far away.
Bytes std_file(){
    Bytes s(144,0);put16(s,0,1);
    put32(s,144,148);const size_t object=148;s.resize(object+28,0);s[object+2]=0;
    const size_t q=s.size();put16(s,q,0);put16(s,q+2,28);put16(s,q+4,0);putf(s,q+8,0);putf(s,q+12,0);putf(s,q+16,0);putf(s,q+20,256);putf(s,q+24,256);
    put16(s,s.size(),0xffff);put16(s,s.size(),0);
    put32(s,4,u32(s.size()));const size_t inst=s.size();put16(s,inst,0);put16(s,inst+2,0);putf(s,inst+4,0);putf(s,inst+8,0);putf(s,inst+12,0);
    put16(s,s.size(),0xffff);put16(s,s.size(),0);s.resize(s.size()+12,0);
    put32(s,8,u32(s.size()));
    const auto command=[&](u32 op,std::vector<u32> w){const size_t at=s.size();put32(s,at,0);put16(s,at+4,op);put16(s,at+6,u32(8+w.size()*4));for(size_t i=0;i<w.size();++i)put32(s,at+8+i*4,w[i]);};
    command(2,{bits(0),bits(0),bits(-400)});command(4,{bits(0),bits(0),bits(1)});command(6,{bits(0),bits(1),bits(0)});
    command(7,{bits(0.62831853f)});command(8,{0xff000000,bits(1000),bits(2000)});
    const size_t at=s.size();put32(s,at,0xffffffff);put16(s,at+4,0xffff);put16(s,at+6,0xffff);return s;
}
Bytes wav(){
    Bytes w;const u32 frames=200;w.insert(w.end(),{'R','I','F','F'});put32(w,4,36+frames*2);w.insert(w.end(),{'W','A','V','E','f','m','t',' '});
    put32(w,16,16);put16(w,20,1);put16(w,22,1);put32(w,24,22050);put32(w,28,44100);put16(w,32,2);put16(w,34,16);
    w.insert(w.end(),{'d','a','t','a'});put32(w,40,frames*2);for(u32 i=0;i<frames;++i){const i16 v=i16(std::sin(i*.3)*8000);put16(w,44+i*2,u16(v));}
    return w;
}
Bytes music_layout(){
    Bytes f;
    for(int t=0;t<3;++t){const size_t at=f.size();f.resize(at+52,0);char name[16]{};std::snprintf(name,sizeof name,"th12_%02d.wav",t);std::memcpy(&f[at],name,16);
        put32(f,at+16,16+u32(t)*4000);put32(f,at+20,4000);put32(f,at+24,400);put32(f,at+28,4000);
        put16(f,at+32,1);put16(f,at+34,2);put32(f,at+36,44100);put32(f,at+40,176400);put16(f,at+44,4);put16(f,at+46,16);}
    return f;
}
// An ending MSG (e00..e11.msg / staff.msg): load an ANM, show its script 0,
// a line of text (encrypted as 420e40 reads it), wait, then the next file.
Bytes ending_msg(const std::string& anm,const char* text,i32 frames,const char* next){
    Bytes m(12,0);put32(m,0,1);put32(m,4,12);
    const auto command=[&](u8 op,const Bytes& payload){m.push_back(0);m.push_back(0);m.push_back(op);m.push_back(u8(payload.size()));append(m,payload);};
    Bytes load(4,0);for(char c:anm)load.push_back(u8(c));load.push_back(0);while(load.size()%4)load.push_back(0);command(7,load);
    Bytes anim(12,0);command(8,anim);
    if(text){Bytes t;u8 key=0x77,step=7;for(const char* c=text;;++c){t.push_back(u8(u8(*c)^key));key=u8(key+step);step=u8(step+16);if(!*c)break;}while(t.size()%4)t.push_back(0);command(3,t);}
    Bytes wait(4,0);put32(wait,0,u32(frames));command(5,wait);
    if(next){Bytes n;for(const char* c=next;*c;++c)n.push_back(u8(*c));n.push_back(0);while(n.size()%4)n.push_back(0);command(12,n);}
    command(0,{});return m;
}
// A demo replay: stage 1, 200 idle frames (the title's first demo is demo2.rpy).
Bytes demo_replay(){
    ReplayConfig config{};ReplayRecorder r(0,0,1,false,config);ReplayInitial in;in.power=100;in.lives=2;in.bombs=2;in.point_value=10000;in.y=51200;
    r.begin_stage(1,7,in);for(int i=0;i<200;++i)r.push(0,0,0,60);return r.encode("DEMO",0,0,0,1,0);
}
// musiccmt.txt: two tracks the way the original lays them out (CP932).
Bytes music_comments(){
    std::string t;
    for(int n=1;n<=2;++n){t+="@bgm/th12_0"+std::to_string(n-1)+"\r\nNo."+std::to_string(n)+"  Test\r\n\x81\xf4Track "+std::to_string(n)+"\r\n";for(int i=0;i<7;++i)t+="line\r\n";}
    return Bytes(t.begin(),t.end());
}
// A PE image with TH12 1.00b's tables at their virtual addresses.
Bytes exe_file(){
    const u32 raw=0x400,rva=0x1000,size=0xb0000;Bytes b(raw+size,0);b[0]='M';b[1]='Z';put32(b,0x3c,0x80);
    b[0x80]='P';b[0x81]='E';put16(b,0x84,0x14c);put16(b,0x86,1);put16(b,0x94,0xe0);put16(b,0x98,0x10b);put32(b,0x98+28,0x400000);
    const size_t sec=0x98+0xe0;std::memcpy(&b[sec],".data\0\0",8);put32(b,sec+8,size);put32(b,sec+12,rva);put32(b,sec+16,size);put32(b,sec+20,raw);
    const auto at=[&](u32 va){return size_t(va-0x400000-rva+raw);};
    for(u32 i=0;i<31;++i){const size_t r=at(0x4af280+i*0xd0);put32(b,r,i==0?5:0);for(u32 j=0;j<16;++j){put32(b,r+4+j*12,j);put32(b,r+8+j*12,16+j);put32(b,r+12+j*12,32+j);}putf(b,r+0xc4,4);put32(b,r+0xc8,1);}
    const char* names[2]{"se_test00.wav","se_test01.wav"};
    for(u32 n=0;n<2;++n){const u32 va=0x4b0000+n*32;std::memcpy(&b[at(va)],names[n],std::strlen(names[n])+1);put32(b,at(0x4aea50+n*4),va);}
    for(u32 i=0;i<60;++i){const size_t r=at(0x4ae5a0+i*20);put32(b,r,59-i);put32(b,r+4,i%2);put16(b,r+8,u32(i16(-100*i32(i%5))));put32(b,r+12,i==7);}
    putf(b,at(0x4a3e14),30);putf(b,at(0x4a3e18),1800);return b;
}
}

static void test_archive(const std::string& dir){
    std::puts("archive (THA1, TH12 cipher table)");
    Bytes big(20000);for(size_t i=0;i<big.size();++i)big[i]=u8(i*7+i/300);
    for(const bool compress:{false,true}){
        const auto image=Archive::build_for_tests({{"front.anm",big},{"se_ok.wav",{1,2,3}},{"thbgm.fmt",{}},{"Stage01.std",Bytes(777,9)}},compress);
        const std::string path=dir+(compress?"/packed.dat":"/stored.dat");CHECK(host::write_file(path,image));
        Archive a;CHECK(a.open(path));CHECK(a.entries.size()==4);Bytes out;
        CHECK(a.read("front.anm",out)&&out==big);CHECK(a.read("SE_OK.WAV",out)&&out==Bytes({1,2,3}));
        CHECK(a.read("thbgm.fmt",out)&&out.empty());CHECK(a.read("stage01.std",out)&&out==Bytes(777,9));CHECK(!a.read("missing.anm",out));
    }
    std::printf("  stored and LZSS entries, case-insensitive lookup: %s\n",failures?"FAILED":"ok");
}
static void test_exe(){
    std::puts("exe (th12.exe tables)");
    ExeTables t;CHECK(t.load(exe_file()));if(!t.error.empty())std::printf("  %s\n",t.error.c_str());
    CHECK(t.bullets[0].script==5&&t.bullets[3].colors[2].sprite==2&&t.bullets[3].colors[2].spawn==18&&t.bullets[3].colors[2].cancel==34&&t.bullets[1].size==4);
    CHECK(t.sounds[0].id==0&&t.sounds[0].file=="se_test01.wav"&&t.sounds[59].file=="se_test00.wav"&&t.sounds[59].id==59);
    CHECK(t.sounds[52].loop&&!t.sounds[51].loop);CHECK(t.near_plane==30&&t.far_plane==1800);
    CHECK(t.version.empty()&&t.sha256.size()==64);
    CHECK(sha256_hex(reinterpret_cast<const u8*>("abc"),3)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    Bytes broken=exe_file();broken[0]='X';CHECK(!t.load(broken));
}
static void test_anm(){
    std::puts("anm (entries, sprites, scripts, THTX)");
    std::vector<Entry> es(4);
    es[0].format=1;es[0].argb=0x80ff4020;es[0].sprites={{0,0,8,8},{8,0,8,8}};es[0].scripts={ins(3,0,{1}),ins(48,5,{bits(1),bits(2),bits(3)},1)};
    es[1].format=3;es[1].argb=0xff00ff00;es[1].w=8;es[1].h=4;es[2].format=5;es[2].argb=0x80ffffff;es[3].format=7;es[3].argb=0x40000000;
    AnmFile f;std::string error;CHECK(parse_anm("test",anm(es),f,error));if(!error.empty())std::printf("  %s\n",error.c_str());
    CHECK(f.entries.size()==4&&f.sprites.size()==2&&f.scripts.size()==2);
    if(f.entries.size()==4){
        CHECK(f.entries[0].rgba[0]==0xff&&f.entries[0].rgba[1]==0x40&&f.entries[0].rgba[2]==0x20&&f.entries[0].rgba[3]==0x80);
        CHECK(f.entries[1].rgba[1]==255&&f.entries[1].rgba[3]==255&&f.entries[1].texture_width==8);
        CHECK(f.entries[2].rgba[3]==0x88&&f.entries[2].rgba[0]==0xff);CHECK(f.entries[3].rgba[0]==0&&f.entries[3].rgba[3]==0x40);}
    if(f.scripts.size()==2){CHECK(f.scripts[0].commands.size()==1&&f.scripts[0].commands[0].op==3&&f.scripts[0].commands[0].ints[0]==1);
        CHECK(f.scripts[1].commands[0].time==5&&f.scripts[1].commands[0].mask==1&&f.scripts[1].commands[0].floats[2]==3);}
    CHECK(f.sprite(1)&&f.sprite(1)->x==8&&!f.sprite(2));
}
static void test_sampler(){
    std::puts("sampler (animation.mjs port)");
    // Script 0: sprite 1, position (10,20), fade alpha 255 -> 0 over 10 frames
    // from time 2, child script 1 at time 4; script 1: sprite 0 offset (5,5).
    Entry e;e.sprites={{0,0,8,8},{8,0,8,8}};
    Bytes s0=ins(3,0,{1});append(s0,ins(48,0,{bits(10),bits(20),bits(0)}));append(s0,ins(55,2,{0,10}));append(s0,ins(88,4,{1}));append(s0,ins(63,20));
    Bytes s1=ins(3,0,{0});append(s1,ins(48,0,{bits(5),bits(5),bits(0)}));append(s1,ins(63,1));
    e.scripts={s0,s1};
    auto file=std::make_shared<AnmFile>();std::string error;CHECK(parse_anm("s",anm({e}),*file,error,false));
    AnimationSampler sampler;sampler.files=[&](i32){return std::shared_ptr<const AnmFile>(file);};
    std::vector<Appearance> out;sampler.begin_frame();sampler.sample_all(0,0,0,out);
    CHECK(out.size()==1&&out[0].sprite==1&&out[0].x==10&&out[0].y==20&&out[0].alpha==1);
    // The fade starts at clock 2 and its timer advances before sampling, so age 7
    // is step 6/10: trunc(255 - 255*f32(0.6)) = 101, f32 rounding included.
    out.clear();sampler.sample_all(0,0,7,out);
    CHECK(out.size()==2);if(out.size()==2){std::printf("  age 7: alpha %.3f, child at %.0f,%.0f sprite %d\n",out[0].alpha,out[1].x,out[1].y,out[1].sprite);
        CHECK(std::fabs(out[0].alpha-101/255.f)<1e-6f);CHECK(out[1].x==15&&out[1].y==25&&out[1].sprite==0);}
    out.clear();sampler.sample_all(0,0,30,out);CHECK(!out.empty()&&out[0].alpha==0);
    sampler.end_frame();
    // Going back re-binds the track from its start.
    sampler.begin_frame();out.clear();sampler.sample_all(0,0,0,out);CHECK(out.size()==1&&out[0].alpha==1);sampler.end_frame();
    // Bullet variant: op 3 n=0..2 selects sprite/spawn/cancel; interrupt 2 runs first.
    Entry b;b.sprites={{0,0,1,1},{0,0,1,1},{0,0,1,1},{0,0,1,1}};
    Bytes bs=ins(3,0,{0});append(bs,ins(63,0));append(bs,ins(64,0,{2}));append(bs,ins(3,0,{1}));append(bs,ins(63,0));b.scripts={bs};
    auto bf=std::make_shared<AnmFile>();CHECK(parse_anm("b",anm({b}),*bf,error,false));
    sampler.files=[&](i32){return std::shared_ptr<const AnmFile>(bf);};sampler.begin_frame();out.clear();
    sampler.sample_all(0,0,3,out,BulletVariant{3,2,1});CHECK(out.size()==1&&out[0].sprite==2);sampler.end_frame();
    std::printf("  easing: %.3f %.3f %.3f\n",easing(.5,1),easing(.5,4),easing(.25,9));
    CHECK(easing(.5,1)==.25&&easing(.5,4)==.75&&easing(1,15)==0);
}
static void test_music(){
    std::puts("music (thbgm.fmt, volume curves)");
    const auto tracks=parse_music_layout(music_layout());CHECK(tracks.size()==3);
    if(tracks.size()==3)CHECK(tracks[1].name=="th12_01.wav"&&tracks[1].offset==4016&&tracks[1].loop_start==400&&tracks[1].length==4000&&tracks[1].align==4);
    CHECK(native_music_gain(100)==1&&native_music_gain(0)==0);
    std::printf("  SE gain base -500 at 80%%: %.4f\n",native_sound_gain(-500,80));
    CHECK(std::fabs(native_sound_gain(-500,80)-std::pow(10.f,(std::trunc(4500*(1-std::pow(.2,3)))-5000)/2000))<1e-6f);
}
static void test_text(){
    std::puts("text (CP932)");
    const std::string s="\x93\x8c\x95\xfb" "A\xb1";const auto u=decode_cp932(s);
    CHECK(u.size()==4&&u[0]==U'東'&&u[1]==U'方'&&u[2]==U'A'&&u[3]==U'ｱ');
    // A dialogue canvas: 32px glyphs, black outline copies, then the colour.
    TextRaster raster;if(!raster.initialize()){std::puts("  raster skipped: no Japanese font");return;}
    Canvas c;c.reset(200,36);const std::u32string text=U"東方";
    for(const auto& o:std::initializer_list<std::array<int,2>>{{2,2},{-2,2},{2,-2},{-2,-2}})raster.fill(c,text,2.f+o[0],2.f+o[1],false,400,32,0x000000);
    raster.fill(c,text,2,2,false,400,32,0xf8f08f);
    u32 lit=0,black=0,coloured=0;for(size_t i=0;i<c.rgba.size();i+=4)if(c.rgba[i+3]>200){++lit;if(c.rgba[i]<20&&c.rgba[i+1]<20)++black;if(c.rgba[i]>0xe0&&c.rgba[i+2]>0x70&&c.rgba[i+2]<0xa0)++coloured;}
    std::printf("  dialogue canvas: %u opaque pixels (%u outline, %u text), width %.0f\n",lit,black,coloured,raster.measure(text,false,400,32));
    CHECK(lit>300&&black>50&&coloured>100);CHECK(std::fabs(raster.measure(text,false,400,32)-64)<1);
}

static void test_profile(const std::string& dir){
    std::puts("profile (scoreth12.dat, th12.cfg)");
    const std::string folder=dir+"/profile";std::filesystem::remove_all(folder);host::make_directory(folder);
    Profile p;p.load(folder);
    CHECK(p.bgm_volume()==100&&p.se_volume()==80);CHECK((p.joy_buttons()==std::array<i32,5>{0,1,2,4,3}));
    CHECK(!p.can_practice(0,0,1,1)&&!p.any_extra()&&!p.cleared(0,0,1));CHECK(p.spell_history().size()==1792);
    p.record_stage_clear(1,0,2,3,1234560);CHECK(p.can_practice(1,0,2,3)&&p.practice_score(1,0,2,3)==123456&&!p.can_practice(1,0,2,4));
    // A continued clear counts but unlocks no Extra; a no-continue Normal clear does.
    p.record_clear(0,1,1,2,6);CHECK(p.cleared(0,1,1)&&!p.can_extra(0,1));
    p.record_clear(0,1,1,0,6);CHECK(p.can_extra(0,1)&&p.any_extra()&&!p.can_extra(0,0));
    p.unlock_music("th12_04");CHECK(p.music_heard("th12_04")&&!p.music_heard("th12_05"));
    std::vector<i32> words(1792,0);words[5*14+3*2]=7;words[5*14+3*2+1]=2;p.set_spell_history(words);
    p.set_volumes(55,20);p.set_joy_buttons({3,0,1,6,2});CHECK(p.save());
    Profile q;q.load(folder);
    CHECK(q.bgm_volume()==55&&q.se_volume()==20);CHECK((q.joy_buttons()==std::array<i32,5>{3,0,1,6,2}));
    CHECK(q.can_practice(1,0,2,3)&&q.practice_score(1,0,2,3)==123456&&q.can_extra(0,1)&&q.cleared(0,1,1)&&q.music_heard("th12_04"));
    {const auto h=q.spell_history();CHECK(h[5*14+3*2]==7&&h[5*14+3*2+1]==2);}
    Bytes bytes;CHECK(host::read_file(folder+"/scoreth12.dat",bytes,1<<22));
    std::printf("  scoreth12.dat: %zu bytes; th12.cfg: %s\n",bytes.size(),host::exists(folder+"/th12.cfg")?"written":"missing");
    CHECK(bytes.size()>24&&bytes[0]=='T'&&bytes[1]=='H'&&bytes[2]=='2'&&bytes[3]=='1');
    {Profile r;CHECK(r.decode_score(bytes));CHECK(r.can_extra(0,1));}
    // A damaged record is rejected; a foreign file is never overwritten.
    {Bytes bad=bytes;bad[30]^=0x55;Profile r;CHECK(!r.decode_score(bad));}
    const Bytes junk{'n','o','t',' ','t','h','1','2'};CHECK(host::write_file(folder+"/scoreth12.dat",junk));
    {Profile r;r.load(folder);r.record_stage_clear(0,0,0,1,0);r.save();Bytes after;host::read_file(folder+"/scoreth12.dat",after,1<<22);CHECK(after==junk);}
}

static bool write_png(const std::string& path,const u8* rgba,u32 w,u32 h);
static void test_game(const std::string& dir){
    std::puts("game (host end to end on synthetic files)");
    if(!std::getenv("DISPLAY")&&!std::getenv("WAYLAND_DISPLAY")){std::puts("  skipped: no display");return;}
    const std::string data=dir+"/game";std::filesystem::remove_all(data);host::make_directory(data);host::paths().data=host::paths().save=data;
    std::vector<std::pair<std::string,Bytes>> files{
        {"sig.anm",simple_bank(0xff202020,320,240,22,64)},{"ascii.anm",ascii_bank()},{"text.anm",text_bank()},
        {"bullet.anm",simple_bank(0xffffffff,0,0,15)},{"enemy.anm",simple_bank(0xffffffff,0,0,15)},
        {"pl00.anm",simple_bank(0xff0000ff,0,0,12)},{"pl01.anm",simple_bank(0xff0000ff,0,0,12)},{"pl02.anm",simple_bank(0xff0000ff,0,0,12)},
        // front 0 is the HUD frame: a red 64x64 square at (520,300), above everything.
        {"front.anm",simple_bank(0xffff0000,520,300,24,64)},
        // title.anm script 0 (the menu pane) is a blue square at (100,100).
        {"title.anm",simple_bank(0xff0000ff,100,100,22,64)},{"title_v.anm",simple_bank(0xffffffff,600,460,22)},{"musiccmt.txt",music_comments()},
        {"default.ecl",ecl("unused",false)},{"demo2.rpy",demo_replay()},
        {"staff.msg",ending_msg("staff.anm",nullptr,20,nullptr)},{"staff.anm",simple_bank(0xff00ffff,320,240,22,64)},{"thbgm.fmt",music_layout()},{"se_test00.wav",wav()},{"se_test01.wav",wav()}};
    for(const char* sht_name:{"pl00a.sht","pl00b.sht","pl01a.sht","pl01b.sht","pl02a.sht","pl02b.sht"})files.push_back({sht_name,sht()});
    for(int st=1;st<=7;++st){char n[4];std::snprintf(n,sizeof n,"%02d",st);const std::string s=n;
        files.push_back({"stage"+s+".ecl",ecl("main",true)});files.push_back({"stage"+s+".std",std_file()});
        for(int i=0;i<6;++i)files.push_back({"st"+s+"_0"+std::to_string(i>>1)+(i%2?"b":"a")+".msg",msg()});
        files.push_back({"stgenm"+s+".anm",simple_bank(0xffffffff,0,0,15)});files.push_back({"st"+s+"logo.anm",simple_bank(0xffffffff,0,0,15)});
        // The stage bank: script 0 shows a green texture (the STD quad).
        files.push_back({"stage"+s+".anm",simple_bank(0xff00c000,0,0,0,256)});}
    files.push_back({"stage07boss.ecl",ecl("boss",false)});
    for(int e=0;e<12;++e){char n[16];std::snprintf(n,sizeof n,"e%02d",e);files.push_back({std::string(n)+".msg",ending_msg(std::string(n)+".anm","THE END",30,"staff.msg")});
        files.push_back({std::string(n)+".anm",simple_bank(0xffffff00,320,240,22,64)});}
    CHECK(host::write_file(data+"/th12.dat",Archive::build_for_tests(files,true)));CHECK(host::write_file(data+"/th12.exe",exe_file()));
    Bytes bgm(16+3*4000,0);std::memcpy(bgm.data(),"ZWAV",4);CHECK(host::write_file(data+"/thbgm.dat",bgm));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_Window* window=SDL_CreateWindow("th12 test",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);SDL_GLContext context=window?SDL_GL_CreateContext(window):nullptr;
    if(!context){std::printf("  skipped: no GLES 3 context (%s)\n",SDL_GetError());if(window)SDL_DestroyWindow(window);return;}
    // Sound goes to a file through SDL's disk driver, so the test can check
    // the game keeps the output queue filled.
    const std::string sound=dir+"/sound.raw";std::filesystem::remove(sound);
    setenv("SDL_AUDIODRIVER","disk",1);setenv("SDL_DISKAUDIOFILE",sound.c_str(),1);
    {
        Game game;const bool booted=game.boot();CHECK(booted);if(!booted){std::printf("  boot: %s\n",game.error.c_str());SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);return;}
        // Pad buttons by number as the Switch reports them: 0 B (shot/OK),
        // 1 A (bomb/cancel), 4 + (pause).
        constexpr u32 B=1,A=2,Plus=16;
        double t=0;const auto run=[&](u32 buttons,u32 directions,u32 frames){for(u32 i=0;i<frames;++i){t+=1000/60.0;if(!game.frame({buttons,directions},1,t))return false;}return true;};
        const auto phase=[&]{return std::string(game.phase_name());};
        const auto until=[&](const char* name,int limit){for(int i=0;i<limit&&phase()!=name;++i)if(!run(0,0,1))return false;return phase()==name;};
        // One press: a frame held, then idle frames so the menu's own waits pass.
        const auto press=[&](u32 buttons,u32 directions=0,u32 idle=40){return run(buttons,directions,1)&&run(0,0,idle);};
        const auto pixel=[&](u32 x,u32 y){const u8* p=game.gpu.read_pixels();const u32 s=game.gpu.scale();return p?std::array<u8,3>{p[(size_t(y*s)*640*s+x*s)*4],p[(size_t(y*s)*640*s+x*s)*4+1],p[(size_t(y*s)*640*s+x*s)*4+2]}:std::array<u8,3>{};};
        CHECK(run(0,0,10));CHECK(phase()=="startup");
        {const auto p=pixel(320,240);std::printf("  start-up screen centre: %02x%02x%02x\n",p[0],p[1],p[2]);CHECK(p[0]==0x20&&p[1]==0x20);}
        // Title: the intro runs to frame 191 before the menu takes input.
        CHECK(until("title",400));CHECK(run(0,0,200));
        if(const u8* p=game.gpu.read_pixels())write_png(dir+"/title.png",p,640*game.gpu.scale(),480*game.gpu.scale());
        {const auto p=pixel(110,110);std::printf("  title pane: %02x%02x%02x\n",p[0],p[1],p[2]);CHECK(p[2]==0xff&&p[0]==0);}

        // Start -> Normal -> Sanae -> B. The menu's OK sound (a synthetic sine)
        // must reach the output device.
        CHECK(press(B));CHECK(phase()=="difficulty");
        for(int i=0;i<40;++i){CHECK(run(0,0,1));SDL_Delay(15);} // the disk driver plays in real time
        {Bytes out;host::read_file(sound,out,1<<26);u32 loud=0;for(size_t i=0;i+4<=out.size();i+=4){float v;std::memcpy(&v,&out[i],4);if(std::fabs(v)>.01f)++loud;}
            std::printf("  sound output: %zu bytes, %u audible samples\n",out.size(),loud);CHECK(loud>100);}CHECK(press(B));CHECK(phase()=="character");
        CHECK(press(0,InDown,10));CHECK(press(0,InDown,10));CHECK(press(B));CHECK(phase()=="shot");CHECK(press(0,InDown,10));CHECK(press(B,0,1));
        CHECK(until("playing",300));std::printf("  error: '%s'\n",game.error.c_str());
        CHECK(run(0,0,120));CHECK(phase()=="playing");
        CHECK(th12::stageNumber==1&&th12::character==2&&th12::shot==1&&!th12::fullGameFlow.practice);
        if(const u8* p=game.gpu.read_pixels())write_png(dir+"/stage1.png",p,640*game.gpu.scale(),480*game.gpu.scale());
        {const auto field=pixel(224,100),hud=pixel(520,300),outside=pixel(4,4);
            std::printf("  field %02x%02x%02x  HUD frame %02x%02x%02x  border %02x%02x%02x\n",field[0],field[1],field[2],hud[0],hud[1],hud[2],outside[0],outside[1],outside[2]);
            CHECK(field[1]>0x80&&field[0]<0x40);   // stage quad projected over the field
            CHECK(hud[0]==0xff&&hud[1]==0);         // front 0 sampled at its script position
            CHECK(outside[1]<0x40);}
        // Pause, then + again resumes (cancel on the pause menu).
        CHECK(press(Plus,0,20));CHECK(phase()=="paused");
        if(const u8* p=game.gpu.read_pixels())write_png(dir+"/paused.png",p,640*game.gpu.scale(),480*game.gpu.scale());
        CHECK(press(Plus,0,20));CHECK(phase()=="playing");
        // Stage clear -> next stage (core flow), then the next stage's load.
        th12::ended=1;bool loaded=false;
        for(int i=0;i<2000&&!loaded;++i){run(0,0,1);loaded=th12::stageNumber==2&&phase()=="playing";}
        CHECK(loaded);std::printf("  after clear: stage %d, %s\n",th12::stageNumber,game.phase_name());
        // Game over: the in-game Score Ranking first (a low score: Next only),
        // then the continue page.
        th12::ended=2;CHECK(until("name",10));CHECK(run(0,0,30));CHECK(press(B,0,5));CHECK(phase()=="continue");
        // Save Replay (still no continue): slot 1, type "M" on the name grid, End.
        CHECK(press(0,InDown,10));CHECK(press(0,InDown,10));CHECK(press(B,0,20));CHECK(phase()=="replay-save");
        CHECK(press(B,0,20));CHECK(phase()=="name");
        CHECK(press(0,InDown,10));CHECK(press(B,0,5));CHECK(press(0,InUp,10));CHECK(press(B,0,20));CHECK(phase()=="replay-save");
        {Bytes file;Replay r;std::string why;CHECK(host::read_file(data+"/replay/th12_01.rpy",file,1<<26));CHECK(parse_replay(file,r,why));
            std::printf("  saved replay: '%s' character %d shot %d, %zu stages (%s)\n",r.info.name.c_str(),r.info.character,r.info.shot,r.stages.size(),why.c_str());
            CHECK(r.info.name=="M"&&r.info.character==2&&r.info.shot==1&&r.stages.size()==2&&r.stages[0].number==1&&r.stages[1].number==2);
            // Only the last stage ends with the all-ffff record; stage 1 runs into the transition.
            if(r.stages.size()==2)CHECK(!r.input(1,r.stages[0].frames-1)->end()&&r.input(2,r.stages[1].frames-1)->end());}
        CHECK(press(A,0,20));CHECK(until("title",60));CHECK(run(0,0,30));
        // Replay (Start -> Practice Start -> Replay): slot 1, stage 1; R plays
        // four ticks a frame. Stage 1's recording ends, stage 2 plays, then the title.
        CHECK(press(0,InDown,10));CHECK(press(0,InDown,10));CHECK(press(B,0,30));CHECK(phase()=="replay");
        CHECK(press(B,0,30));CHECK(phase()=="replay-stage");CHECK(press(B,0,1));CHECK(until("replay-playing",300));
        CHECK(th12::stageNumber==1&&th12::character==2&&th12::shot==1);
        CHECK(press(Plus,0,20));CHECK(phase()=="paused");CHECK(press(Plus,0,20));CHECK(phase()=="replay-playing");
        {const i32 before=th12::frame;CHECK(run(1u<<3,0,10));std::printf("  fast-forward: %d ticks in 10 frames\n",th12::frame-before);CHECK(th12::frame-before==40);}
        {bool second=false;for(int i=0;i<4000&&phase()!="title";++i){CHECK(run(0,0,1));second|=th12::stageNumber==2&&phase()=="replay-playing";}
            std::printf("  replay: stage 2 %s, back at %s\n",second?"played":"missing",game.phase_name());CHECK(second&&phase()=="title");}
        CHECK(run(0,0,30));
        // Player Data: difficulty down, the first card page, back.
        CHECK(press(0,InDown,10));CHECK(press(B,0,30));CHECK(phase()=="player-data");CHECK(press(0,InDown,10));CHECK(press(B,0,10));
        if(const u8* p=game.gpu.read_pixels())write_png(dir+"/player-data.png",p,640*game.gpu.scale(),480*game.gpu.scale());
        CHECK(press(A,0,30));CHECK(until("title",60));CHECK(run(0,0,30));
        // An ending (stage 6 cleared, no continue): e11 for Sanae B, the staff
        // roll, then the title's Score Ranking and replay save page.
        CHECK(press(0,InUp,10));CHECK(press(0,InUp,10));CHECK(press(0,InUp,10));CHECK(press(B));CHECK(phase()=="difficulty");
        CHECK(press(B));CHECK(press(B));CHECK(press(B,0,1));CHECK(until("playing",300));
        th12::fullGameFlow.stage=6;th12::ended=1;CHECK(until("ending",800));CHECK(run(0,0,10));
        if(const u8* p=game.gpu.read_pixels())write_png(dir+"/ending.png",p,640*game.gpu.scale(),480*game.gpu.scale());
        {const auto p=pixel(330,250);std::printf("  ending picture: %02x%02x%02x\n",p[0],p[1],p[2]);CHECK(p[0]==0xff&&p[1]==0xff&&p[2]==0);}
        CHECK(until("name",200));CHECK(run(0,0,30));CHECK(press(B,0,20));CHECK(phase()=="replay-save");CHECK(press(A,0,20));CHECK(until("title",60));
        {Profile saved;saved.load(data);CHECK(saved.ending_seen(11));CHECK(!saved.ending_seen(10));}
        CHECK(run(0,0,30));
        // The stage 1 clear unlocked Practice of stage 1 for Sanae B Normal.
        {Profile saved;saved.load(data);CHECK(saved.can_practice(2,1,1,1));CHECK(!saved.can_practice(0,0,1,1));}
        // Practice Start (Extra Start is locked and skipped) keeps the
        // remembered loadout; stage 1.
        CHECK(press(0,InDown,10));CHECK(press(B));CHECK(phase()=="difficulty");
        CHECK(press(B));CHECK(press(B));CHECK(press(B,0,20));CHECK(phase()=="practice");CHECK(press(B,0,1));CHECK(until("playing",300));
        std::printf("  practice: stage %d character %d shot %d\n",th12::stageNumber,th12::character,th12::shot);
        CHECK(th12::stageNumber==1&&th12::character==2&&th12::shot==1&&th12::fullGameFlow.practice);
        // A Practice game over goes straight to its result menu.
        CHECK(run(0,0,30));th12::ended=2;CHECK(until("result-menu",10));
        CHECK(run(0,0,20));CHECK(press(B,0,5));CHECK(until("title",60));CHECK(run(0,0,30));
        // Pause -> Save Replay and Quit -> Yes: the save page, slot 1 again, End.
        CHECK(press(B));CHECK(press(B));CHECK(press(B));CHECK(press(B,0,20));CHECK(phase()=="practice");CHECK(press(B,0,1));CHECK(until("playing",300));CHECK(run(0,0,60));
        CHECK(press(Plus,0,20));CHECK(press(0,InDown,10));CHECK(press(0,InDown,10));CHECK(press(B,0,40));CHECK(phase()=="pause-confirm");
        CHECK(press(0,InUp,10));CHECK(press(B,0,20));CHECK(phase()=="replay-save");CHECK(press(B,0,20));CHECK(phase()=="name");CHECK(press(B,0,20));CHECK(phase()=="replay-save");
        {Bytes file;Replay r;std::string why;CHECK(host::read_file(data+"/replay/th12_01.rpy",file,1<<26));CHECK(parse_replay(file,r,why));
            std::printf("  practice replay: '%s' practice %d, stage %d\n",r.info.name.c_str(),r.info.practice,r.stages.empty()?0:r.stages[0].number);CHECK(r.info.practice&&r.info.name=="M");}
        CHECK(press(A,0,20));CHECK(until("title",60));CHECK(run(0,0,30));
        // Option (Practice Start -> Replay -> Player Data -> Music Room -> Option):
        // BGM volume down 5, then Quit back to the title; th12.cfg keeps it.
        for(int i=0;i<4;++i)CHECK(press(0,InDown,10));CHECK(press(B));CHECK(phase()=="options");
        CHECK(press(0,InLeft,10));CHECK(press(A,0,40));CHECK(press(A,0,40));CHECK(until("title",60));
        {Profile saved;saved.load(data);std::printf("  saved BGM volume %d, SE volume %d\n",saved.bgm_volume(),saved.se_volume());CHECK(saved.bgm_volume()==95&&saved.se_volume()==80);}
        // Key Config: Shoot -> Y (button 6), saved on Quit; Y then confirms on the title.
        // (The title remembers Option.)
        CHECK(press(B));CHECK(phase()=="options");CHECK(press(0,InDown,10));CHECK(press(0,InDown,10));CHECK(press(B));CHECK(phase()=="key-config");
        // A jumps to Quit; OK there keeps the draft (A on Quit would drop it).
        CHECK(press(B,0,5));CHECK(press(1u<<6,0,5));CHECK(press(A,0,10));CHECK(press(B,0,40));CHECK(phase()=="options");
        {Profile saved;saved.load(data);std::printf("  pad buttons: shoot %d bomb %d\n",saved.joy_buttons()[0],saved.joy_buttons()[1]);CHECK((saved.joy_buttons()==std::array<i32,5>{6,1,2,4,3}));}
        CHECK(press(A,0,40));CHECK(press(A,0,40));CHECK(until("title",60));CHECK(run(0,0,30));
        CHECK(press(1u<<6));CHECK(phase()=="options");CHECK(press(A,0,10));CHECK(press(A,0,40));CHECK(until("title",60));
        // Half a minute idle on the title starts the demo (demo2.rpy); its
        // recording ends and the title returns.
        CHECK(until("demo",1900));CHECK(until("title",400));
        CHECK(game.snapshot());CHECK(host::exists(data+"/snapshot/th12_000.bmp"));
        game.shutdown();
    }
    SDL_GL_DeleteContext(context);SDL_DestroyWindow(window);
}

// Minimal PNG (stored deflate) so a test frame can be looked at.
static u32 crc32(const u8* d,size_t n,u32 c=0xffffffff){for(size_t i=0;i<n;++i){c^=d[i];for(int k=0;k<8;++k)c=c&1?0xedb88320^(c>>1):c>>1;}return c;}
static bool write_png(const std::string& path,const u8* rgba,u32 w,u32 h){
    Bytes raw;for(u32 y=0;y<h;++y){raw.push_back(0);raw.insert(raw.end(),rgba+size_t(y)*w*4,rgba+size_t(y+1)*w*4);}
    Bytes z{0x78,1};for(size_t at=0;at<raw.size();){const size_t n=std::min<size_t>(65535,raw.size()-at);z.push_back(at+n==raw.size());z.push_back(u8(n));z.push_back(u8(n>>8));z.push_back(u8(~n));z.push_back(u8(~n>>8));z.insert(z.end(),raw.begin()+i64(at),raw.begin()+i64(at+n));at+=n;}
    u32 a=1,b=0;for(u8 c:raw){a=(a+c)%65521;b=(b+a)%65521;}const u32 adler=b<<16|a;for(int i=3;i>=0;--i)z.push_back(u8(adler>>(8*i)));
    Bytes png{0x89,'P','N','G',13,10,26,10};
    const auto chunk=[&](const char* type,const Bytes& body){for(int i=3;i>=0;--i)png.push_back(u8(body.size()>>(8*i)));Bytes t(type,type+4);t.insert(t.end(),body.begin(),body.end());png.insert(png.end(),t.begin(),t.end());const u32 c=~crc32(t.data(),t.size());for(int i=3;i>=0;--i)png.push_back(u8(c>>(8*i)));};
    Bytes ih;for(u32 v:{w,h})for(int i=3;i>=0;--i)ih.push_back(u8(v>>(8*i)));ih.insert(ih.end(),{8,6,0,0,0});chunk("IHDR",ih);chunk("IDAT",z);chunk("IEND",{});
    return host::write_file(path,png);
}

int main(){
    const std::string dir=std::string(std::getenv("TMPDIR")?std::getenv("TMPDIR"):"/tmp")+"/th12-switch-test";
    host::make_directory(dir);host::paths().data=host::paths().save=dir;
    test_archive(dir);test_exe();test_anm();test_sampler();test_music();test_text();test_profile(dir);
    if(SDL_Init(SDL_INIT_VIDEO)==0){test_game(dir);SDL_Quit();}else std::printf("game skipped: %s\n",SDL_GetError());
    std::printf(failures?"FAILED (%d)\n":"all host tests passed\n",failures);
    return failures?1:0;
}
