// 3DSage - Let's program Doom - part 2
//
// Youtube: https://youtu.be/fRu8kjXvkdY
// got upto: 12m55s
//
// Implemented by Joseph21 on olc::PixelGameEngine
// February 24, 2024

#include <iostream>
#include <cmath>

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"


#define res         2                 // 1 = 160x120, 2 = 320x240, 4 = 640x480
#define SW          160*res           // screen width
#define SH          120*res           // screen height
#define SW2         (SW/2)            // half of screen width
#define SH2         (SH/2)            // half of screen height
#define pixelScale  4/res             // pixel scale

#define PI          3.1415926535f

// filename for file to load from/save to
#define LEVEL_FILE   "../textures/level.txt"

// include all texture files converted into code (see notes)
#include "../textures/T_VIEW2D.h"          // background with grid and buttons
#include "../textures/T_NUMBERS.h"         // texture containing numbers

#include "../textures/T_00.h"              // "user defined" textures
#include "../textures/T_01.h"
#include "../textures/T_02.h"
#include "../textures/T_03.h"
#include "../textures/T_04.h"
#include "../textures/T_05.h"
#include "../textures/T_06.h"
#include "../textures/T_07.h"
#include "../textures/T_08.h"
#include "../textures/T_09.h"
#include "../textures/T_10.h"
#include "../textures/T_11.h"
#include "../textures/T_12.h"
#include "../textures/T_13.h"
#include "../textures/T_14.h"
#include "../textures/T_15.h"
#include "../textures/T_16.h"
#include "../textures/T_17.h"
#include "../textures/T_18.h"
#include "../textures/T_19.h"
#include "../textures/T_20.h"
#include "../textures/T_21.h"

int numText = 21;                          // number of textures - 1 (acts as last possible index for textures)
int numSect =  0;                          // number of sectors - used by save(), set by load()
int numWall =  0;                          // number of walls   - same

//-----------------------------------------------------------------------------

// aux. struct for timing
typedef struct {
    float fr1, fr2;          // frame 1, frame 2, to create constant frame rate (in seconds)
} Timer;
Timer T;

// lookup table for conversion of degrees to sine/cosine values
typedef struct {
    float cos[360];          // save sin and cos values for [0, 359] degrees
    float sin[360];
} TrigLookup;
TrigLookup M;

// player info
typedef struct {
    int x, y, z;             // player position, z is up
    int a;                   // player angle of rotation left right
    int l;                   // variable to look up and down
} Player;
Player P;

// walls info
typedef struct {
    int x1, y1;              // bottom line point 1
    int x2, y2;              // bottom line point 2
    int c;                   // wall colour
    int wt, u, v;            // wall texture and u/v tile
    int shade;               // shade of the wall
} Wall;
Wall W[256];

// sectors info
typedef struct {
    int ws, we;              // wall number start and end
    int z1, z2;              // height of bottom and top
    int d;                   // add y distances to sort drawing order
    int c1, c2;              // bottom and top colour
    int st, ss;              // surface texture, surface scale
    int surf[SW];            // to hold points for surfaces
    int surface;             // is there a surface to draw
} Sector;
Sector S[128];

// texture map
typedef struct {
    int w, h;                // texture width/height
    const int *name = nullptr;   // texture name (could be better: "data", since it points at the data array
} TexureMaps;
TexureMaps Textures[64];     //increase for more textures

//-----------------------------------------------------------------------------

class DoomGame : public olc::PixelGameEngine {

public:
    DoomGame() {
        sAppName = "3DSage's DoomGame (by Joseph21)";
        sAppName.append( " - S:(" + std::to_string( SW         ) + ", " + std::to_string( SH         ) + ")" );
        sAppName.append(  ", P:(" + std::to_string( pixelScale ) + ", " + std::to_string( pixelScale ) + ")" );

    }

private:

    // load file data into S (sector data), W (wall data) and P (player data)
    void load() {
        std::ifstream fp( LEVEL_FILE );
        if(!fp.is_open()) {
            std::cout << "ERROR: load() --> Error opening file: " << LEVEL_FILE << std::endl;
            return;
        } else {
            fp >> numSect;                     // read # sectors
            for(int s = 0; s < numSect; s++) { // load all sectors
                fp >> S[s].ws >> S[s].we >> S[s].z1 >> S[s].z2 >> S[s].st >> S[s].ss;
            }
            fp >> numWall;                     // read # walls
            for(int w = 0; w < numWall; w++) { // load all walls
                fp >> W[w].x1 >> W[w].y1 >> W[w].x2 >> W[w].y2 >> W[w].wt >> W[w].u >> W[w].v >> W[w].shade;
            }
            fp >> P.x >> P.y >> P.z >> P.a >> P.l;
            fp.close();
        }
    }

    // draw a pixel at (x, y) with rgb
    void drawPixel( int x, int y, int r, int g, int b ) {
        // draw pixel having screen origin left *down* (and y going up)
        Draw( x, SH - 1 - y, olc::Pixel( r, g, b ) );
    }

    // adapt player variables (lookup value l, position in the world x, y or z value or orientation a)
    void movePlayer() {
        int dx = M.sin[P.a] * 10.0f;
        int dy = M.cos[P.a] * 10.0f;
        if (GetKey( olc::Key::M ).bHeld) {
            // move up, down, look up, down
            if (GetKey( olc::Key::A ).bHeld) { P.l += 1; }
            if (GetKey( olc::Key::D ).bHeld) { P.l -= 1; }
            if (GetKey( olc::Key::W ).bHeld) { P.z += 4; }
            if (GetKey( olc::Key::S ).bHeld) { P.z -= 4; }
        } else {
            // move forward, back, rotate left, right
            if (GetKey( olc::Key::A ).bHeld) { P.a += 4; if (P.a >= 360) { P.a -= 360; } }
            if (GetKey( olc::Key::D ).bHeld) { P.a -= 4; if (P.a <    0) { P.a += 360; } }
            if (GetKey( olc::Key::W ).bHeld) { P.x += dx; P.y += dy; }
            if (GetKey( olc::Key::S ).bHeld) { P.x -= dx; P.y -= dy; }
        }
        // strafe left, right
        if (GetKey( olc::Key::LEFT  ).bHeld) { P.x -= dy; P.y += dx; }
        if (GetKey( olc::Key::RIGHT ).bHeld) { P.x += dy; P.y -= dx; }
    }

    void clearBackground() {
        for (int y = 0; y < SH; y++) {
            for (int x = 0; x < SW; x++) {
                drawPixel( x, y, 0, 60, 130 );  // clear background colour
            }
        }
    }

    // clips the line from (x1, y1, z1) to (x2, y2, z2) at the view plane of the player
    // I guess the view plane is y = 0, since the points are rotated and translated before calling
    // this function so that the player is assumed at the origin
    void clipBehindPlayer( int &x1, int &y1, int &z1, int x2, int y2, int z2 ) {
        float da = y1;      // distance plane -> point a
        float db = y2;      // distance plane -> point b
        float d = da - db; if (d == 0) { d = 1; }           // prevent division by zero
        float t = da / d;         // intersection factor (between 0 and 1)
        x1 = x1 + t * (x2 - x1);
        y1 = y1 + t * (y2 - y1); if (y1 == 0) { y1 = 1; }   // prevent division by zero
        z1 = z1 + t * (z2 - z1);
    }

    // the top and bottom lines share their x values (vertical lines)
    // b1 and b2 are the bottom two y value points
    // t1 and t2 are the top    two y value points
    void drawWall( int x1, int x2, int b1, int b2, int t1, int t2, int s, int w, int frontBack ) {
        // wall texture
        int wt = W[w].wt;
        // horizontal wall texture starting and step value
        float ht = 0.0f, ht_step = float( Textures[wt].w * W[w].u ) / float( x2 - x1 );

        // hold difference in distance
        int dyb = b2 - b1;                              // y distance of bottom line
        int dyt = t2 - t1;                              // y distance of top    line
        // we will eventually divide by the x distance, so let's prevent division by zero
        int dx  = x2 - x1; if (dx == 0) { dx = 1; }     // x distance
        int xs  = x1;                                   // cache initial x1 starting position

        // clip x - clip at 1 so we can visually see that clipping is happening
        if (x1 <   0) { ht -= ht_step * x1; x1 =  0; }   // clip left
        if (x2 <   0) {                     x2 =  0; }   // clip left
        if (x1 >= SW) {                     x1 = SW; }   // clip right
        if (x2 >= SW) {                     x2 = SW; }   // clip right
        // draw x verticle lines
        for (int x = x1; x < x2; x++) {
            // the y start and end point
            // the + 0.5f is to prevent rounding issues
            int y1 = dyb * (x - xs + 0.5f) / dx + b1;   // y bottom point
            int y2 = dyt * (x - xs + 0.5f) / dx + t1;   // y top point

            // vertical wall texture starting and step value
            float vt = 0.0f, vt_step = float( Textures[wt].h * W[w].v ) / float( y2 - y1 );

            // clip y - similar to clip x
            if (y1 <   0) { vt -= vt_step * y1; y1 =      0; }    // clip bottom
            if (y2 <   0) {                     y2 =      0; }    // clip bottom
            if (y1 >= SH) {                     y1 = SH - 1; }    // clip top
            if (y2 >= SH) {                     y2 = SH - 1; }    // clip top
            // draw front wall
            if (frontBack == 0) {
                if (S[s].surface ==  1) { S[s].surf[x] = y1; }   // bottom surface save top row
                if (S[s].surface ==  2) { S[s].surf[x] = y2; }   // top    surface save bottom row

                for (int y = y1; y < y2; y++) {                  // normal wall
                    // since we save 3 values per pixel, we need to times y and x by 3 to skip to the next pixel
                    int pixel = (int)(Textures[wt].h - 1 - ((int)vt % Textures[wt].h)) * 3 * Textures[wt].w +
                                                           ((int)ht % Textures[wt].w)  * 3;
                    // shading is halved to soften the shadows
                    int r = Textures[wt].name[pixel + 0] - W[w].shade / 2; if (r < 0) { r = 0; }
                    int g = Textures[wt].name[pixel + 1] - W[w].shade / 2; if (g < 0) { g = 0; }
                    int b = Textures[wt].name[pixel + 2] - W[w].shade / 2; if (b < 0) { b = 0; }
                    drawPixel( x, y, r, g, b );
                    vt += vt_step;
                }
                ht += ht_step;
            }
            // draw back wall and surface
            if (frontBack == 1) {
                if (S[s].surface == 1) { y2 = S[s].surf[x]; }
                if (S[s].surface == 2) { y1 = S[s].surf[x]; }
                for (int y = y1; y < y2; y++) { drawPixel( x, y, 255, 0, 0 ); }   // surfaces
            }
        }
    }

    int dist( int x1, int y1, int x2, int y2 ) {
        int nDistance = sqrt( (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) );
        return nDistance;
    }

    // performs projection to draw the scene in 3D:
    //  1. world is translated so that world origin coincides with player position
    //  2. world is rotated around player with player angle
    //  3. looking up/down is implemented using the player's z coordinate
    //  4. projection from world to screen space is done
    //  5. drawing pixels only when they are really on screen
    void draw3D() {
        int wx[4], wy[4], wz[4], cycles;           // local x, y, z values for the wall
        float CS = M.cos[P.a], SN = M.sin[P.a];    // local copies of sin and cos of player angle

        // order (sort) sectors by distance - needed for painters algorithm on the sectors
        for (int s = 0; s < numSect; s++) {
            for (int w = 0; w < numSect - s - 1; w++) {
                if (S[w].d < S[w+1].d) {
                    Sector st = S[w]; S[w] = S[w+1]; S[w+1] = st;
                }
            }
        }

        for (int s = 0; s < numSect; s++) {
            S[s].d = 0;                            // clear distance
            if      (P.z < S[s].z1) { S[s].surface = 1; cycles = 2; for (int x = 0; x < SW; x++) { S[s].surf[x] = SH; } }     // bottom surface is visible
            else if (P.z > S[s].z2) { S[s].surface = 2; cycles = 2; for (int x = 0; x < SW; x++) { S[s].surf[x] =  0; } }     // top    surface is visible
            else                    { S[s].surface = 0; cycles = 1;                                                     }     // only wall surface is visible

            for (int frontBack = 0; frontBack < cycles; frontBack++) {
                for (int w = S[s].ws; w < S[s].we; w++) {

                    // offset bottom 2 points by player
                    int x1 = W[w].x1 - P.x, y1 = W[w].y1 - P.y;
                    int x2 = W[w].x2 - P.x, y2 = W[w].y2 - P.y;

                    // swap for surface
                    if (frontBack == 1) { int swp = x1; x1 = x2; x2 = swp; swp = y1; y1 = y2; y2 = swp; }

                    // world X position
                    wx[0] = x1 * CS - y1 * SN;
                    wx[1] = x2 * CS - y2 * SN;
                    wx[2] = wx[0];                             // top line has same x value as bottom line
                    wx[3] = wx[1];
                    // world Y position (depth)
                    wy[0] = y1 * CS + x1 * SN;
                    wy[1] = y2 * CS + x2 * SN;
                    wy[2] = wy[0];                             // top line has same y value as bottom line
                    wy[3] = wy[1];
                    S[s].d += dist( 0, 0, (wx[0] + wx[1]) / 2, (wy[0] + wy[1]) / 2);    // store this wall distance
                    // world Z position (height)
                    wz[0] = S[s].z1 - P.z + ((P.l * wy[0]) / 32.0f);
                    wz[1] = S[s].z1 - P.z + ((P.l * wy[1]) / 32.0f);
                    wz[2] = S[s].z2 - P.z + ((P.l * wy[0]) / 32.0f);
                    wz[3] = S[s].z2 - P.z + ((P.l * wy[1]) / 32.0f);
                    // don't draw if behind player
                    if (wy[0] < 1 && wy[1] < 1) { continue; }    // wall completely behind player - don't draw
                    // point 1 behind player, clip
                    if (wy[0] < 1) {
                        clipBehindPlayer( wx[0], wy[0], wz[0], wx[1], wy[1], wz[1] );   // bottom line
                        clipBehindPlayer( wx[2], wy[2], wz[2], wx[3], wy[3], wz[3] );   // top    line
                    }
                    // point 2 behind player, clip
                    if (wy[1] < 1) {
                        clipBehindPlayer( wx[1], wy[1], wz[1], wx[0], wy[0], wz[0] );   // bottom line
                        clipBehindPlayer( wx[3], wy[3], wz[3], wx[2], wy[2], wz[2] );   // top    line
                    }
                    // screen x, screen y position
                    wx[0] = wx[0] * 200 / wy[0] + SW2; wy[0] = wz[0] * 200 / wy[0] + SH2;
                    wx[1] = wx[1] * 200 / wy[1] + SW2; wy[1] = wz[1] * 200 / wy[1] + SH2;
                    wx[2] = wx[2] * 200 / wy[2] + SW2; wy[2] = wz[2] * 200 / wy[2] + SH2;
                    wx[3] = wx[3] * 200 / wy[3] + SW2; wy[3] = wz[3] * 200 / wy[3] + SH2;
                    // draw lines
                    drawWall( wx[0], wx[1], wy[0], wy[1], wy[2], wy[3], s, w, frontBack );
                }
                S[s].d /= (S[s].we -S[s].ws);    // find average sector distance: divide by nr of walls in sector
            }
        }
    }

    // function to test if textures are working correctly
    void testTextures() {
        int t = 4; // texture number
        for (int y = 0; y < Textures[t].h; y++) {
            for (int x = 0; x < Textures[t].w; x++) {
                // since we save 3 values per pixel, we need to times y and x by 3 to skip to the next pixel
                int pixel = (Textures[t].h - 1 - y) * 3 * Textures[t].w + x * 3;
                int r = Textures[t].name[pixel + 0];
                int g = Textures[t].name[pixel + 1];
                int b = Textures[t].name[pixel + 2];
                drawPixel( x, y, r, g, b );
            }
        }
    }

    // this function is OpenGL's analogon of OnUserUpdate() ig
    void display( float fElapsedTime ) {
        if (T.fr1 - T.fr2 >= 0.05f) {  // only draw 20 frames / second
            clearBackground();
            movePlayer();
            draw3D();

            T.fr2 = T.fr1;
        }
        T.fr1 += fElapsedTime;
    }

    // this function is OpenGL's analogon of OnUserCreate() ig
    void init() {
        // store sin & cos in degrees
        for (int v = 0; v < 360; v++) {
            M.cos[v] = cos( v / 180.0f * PI );
            M.sin[v] = sin( v / 180.0f * PI );
        }
        // init player
        P.x =   70;
        P.y = -110;
        P.z =   20;
        P.a =    0;
        P.l =    0;

        //define textures
        Textures[ 0].name = T_00; Textures[ 0].h = T_00_HEIGHT; Textures[ 0].w = T_00_WIDTH;
        Textures[ 1].name = T_01; Textures[ 1].h = T_01_HEIGHT; Textures[ 1].w = T_01_WIDTH;
        Textures[ 2].name = T_02; Textures[ 2].h = T_02_HEIGHT; Textures[ 2].w = T_02_WIDTH;
        Textures[ 3].name = T_03; Textures[ 3].h = T_03_HEIGHT; Textures[ 3].w = T_03_WIDTH;
        Textures[ 4].name = T_04; Textures[ 4].h = T_04_HEIGHT; Textures[ 4].w = T_04_WIDTH;
        Textures[ 5].name = T_05; Textures[ 5].h = T_05_HEIGHT; Textures[ 5].w = T_05_WIDTH;
        Textures[ 6].name = T_06; Textures[ 6].h = T_06_HEIGHT; Textures[ 6].w = T_06_WIDTH;
        Textures[ 7].name = T_07; Textures[ 7].h = T_07_HEIGHT; Textures[ 7].w = T_07_WIDTH;
        Textures[ 8].name = T_08; Textures[ 8].h = T_08_HEIGHT; Textures[ 8].w = T_08_WIDTH;
        Textures[ 9].name = T_09; Textures[ 9].h = T_09_HEIGHT; Textures[ 9].w = T_09_WIDTH;
        Textures[10].name = T_10; Textures[10].h = T_10_HEIGHT; Textures[10].w = T_10_WIDTH;
        Textures[11].name = T_11; Textures[11].h = T_11_HEIGHT; Textures[11].w = T_11_WIDTH;
        Textures[12].name = T_12; Textures[12].h = T_12_HEIGHT; Textures[12].w = T_12_WIDTH;
        Textures[13].name = T_13; Textures[13].h = T_13_HEIGHT; Textures[13].w = T_13_WIDTH;
        Textures[14].name = T_14; Textures[14].h = T_14_HEIGHT; Textures[14].w = T_14_WIDTH;
        Textures[15].name = T_15; Textures[15].h = T_15_HEIGHT; Textures[15].w = T_15_WIDTH;
        Textures[16].name = T_16; Textures[16].h = T_16_HEIGHT; Textures[16].w = T_16_WIDTH;
        Textures[17].name = T_17; Textures[17].h = T_17_HEIGHT; Textures[17].w = T_17_WIDTH;
        Textures[18].name = T_18; Textures[18].h = T_18_HEIGHT; Textures[18].w = T_18_WIDTH;
        Textures[19].name = T_19; Textures[19].h = T_19_HEIGHT; Textures[19].w = T_19_WIDTH;
        Textures[20].name = T_20; Textures[20].h = T_20_HEIGHT; Textures[20].w = T_20_WIDTH;
        Textures[21].name = T_21; Textures[21].h = T_21_HEIGHT; Textures[21].w = T_21_WIDTH;
    }

public:
    bool OnUserCreate() override {

        // initialize your assets here
        init();

        return true;
    }

    bool OnUserUpdate( float fElapsedTime ) override {

        if (GetKey( olc::Key::ENTER ).bPressed) {
            load();
        }

        // your update code per frame here
        display( fElapsedTime );

        // display test info on the player
        DrawString( 2, 10, "Pos: ("  + std::to_string( P.x ) + ", "         + std::to_string( P.y ) + ", " + std::to_string( P.z ) + ")" );
        DrawString( 2, 18, "Angle: " + std::to_string( P.a ) + ", lookup: " + std::to_string( P.l ) );

        return true;
    }

    bool OnUserDestroy() override {

        // your clean up code here

        return true;
    }
};

int main()
{
	DoomGame demo;
	if (demo.Construct( SW, SH, pixelScale, pixelScale ))
		demo.Start();

	return 0;
}

