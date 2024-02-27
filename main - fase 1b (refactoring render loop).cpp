// 3DSage - Let's program Doom - part 2
//
// Youtube: https://youtu.be/fRu8kjXvkdY
// got upto: 7m32s
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
        sAppName = "DoomGame";
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
    void myPixel( int x, int y, int c ) {
        olc::Pixel rgbPixel;
        switch (c) {
            case 0:  rgbPixel = olc::Pixel( 255, 255,   0 ); break;  // Bright yellow
            case 1:  rgbPixel = olc::Pixel( 160, 160,   0 ); break;  // Darker yellow
            case 2:  rgbPixel = olc::Pixel(   0, 255,   0 ); break;  // Bright green
            case 3:  rgbPixel = olc::Pixel(   0, 160,   0 ); break;  // Darker green
            case 4:  rgbPixel = olc::Pixel(   0, 255, 255 ); break;  // Bright cyan
            case 5:  rgbPixel = olc::Pixel(   0, 160, 160 ); break;  // Darker cyan
            case 6:  rgbPixel = olc::Pixel( 160, 100,   0 ); break;  // Bright brown
            case 7:  rgbPixel = olc::Pixel( 110,  50,   0 ); break;  // Darker brown
            case 8:  rgbPixel = olc::Pixel(   0,  60, 130 ); break;  // background colour
            default: rgbPixel = olc::Pixel( 255,   0, 255 ); break;  // error = magenta
        }
        Draw( x, SH - 1 - y, rgbPixel );   // draw pixel having screen origin left *down* (and y going up)
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
                myPixel( x, y, 8 );  // clear background colour
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
        // hold difference in distance
        int dyb = b2 - b1;                              // y distance of bottom line
        int dyt = t2 - t1;                              // y distance of top    line
        // we will eventually divide by the x distance, so let's prevent division by zero
        int dx  = x2 - x1; if (dx == 0) { dx = 1; }     // x distance
        int xs = x1;                                    // cache initial x1 starting position

        // clip x - clip at 1 so we can visually see that clipping is happening
        if (x1 <   0) { x1 =  0; }   // clip left
        if (x2 <   0) { x2 =  0; }   // clip left
        if (x1 >= SW) { x1 = SW; }   // clip right
        if (x2 >= SW) { x2 = SW; }   // clip right
        // draw x verticle lines
        for (int x = x1; x < x2; x++) {
            // the y start and end point
            // the + 0.5f is to prevent rounding issues
            int y1 = dyb * (x - xs + 0.5f) / dx + b1;   // y bottom point
            int y2 = dyt * (x - xs + 0.5f) / dx + t1;   // y top point

            // clip y - similar to clip x
            if (y1 <   0) { y1 =      0; }    // clip bottom
            if (y2 <   0) { y2 =      0; }    // clip bottom
            if (y1 >= SH) { y1 = SH - 1; }    // clip top
            if (y2 >= SH) { y2 = SH - 1; }    // clip top
            // draw front wall
            if (frontBack == 0) {
                if (S[s].surface ==  1) { S[s].surf[x] = y1; }   // bottom surface save top row
                if (S[s].surface ==  2) { S[s].surf[x] = y2; }   // top    surface save bottom row
                for (int y = y1; y < y2; y++) { myPixel( x, y, 0 ); }   // normal wall
            }
            // draw back wall and surface
            if (frontBack == 1) {
                if (S[s].surface == 1) { y2 = S[s].surf[x]; }
                if (S[s].surface == 2) { y1 = S[s].surf[x]; }
                for (int y = y1; y < y2; y++) { myPixel( x, y, 2 ); }   // surfaces
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
        DrawString( 2, SH - 18, "Pos: ("  + std::to_string( P.x ) + ", "         + std::to_string( P.y ) + ", " + std::to_string( P.z ) + ")" );
        DrawString( 2, SH - 10, "Angle: " + std::to_string( P.a ) + ", lookup: " + std::to_string( P.l ) );

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

