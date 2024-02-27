// 3DSage - Let's program Doom - part 2
//
// Youtube: https://youtu.be/fRu8kjXvkdY
// got upto:
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

#define numSect     4                 // number of sectors
#define numWall    16                 // number of walls (total)

//-----------------------------------------------------------------------------

// aux. struct for timing
typedef struct {
    float fr1, fr2;          // frame 1, frame 2, to create constant frame rate (in seconds)
} myTime;
myTime T;

// lookup table for conversion of degrees to sine/cosine values
typedef struct {
    float cos[360];          // save sin and cos values for [0, 359] degrees
    float sin[360];
} myMath;
myMath M;

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
} Walls;
Walls W[30];

// sectors info
typedef struct {
    int ws, we;              // wall number start and end
    int z1, z2;              // height of bottom and top
    int d;                   // add y distances to sort drawing order
    int c1, c2;              // bottom and top colour
    int surf[SW];            // to hold points for surfaces
    int surface;             // is there a surface to draw
} Sectors;
Sectors S[30];

//-----------------------------------------------------------------------------

class DoomGame : public olc::PixelGameEngine {

public:
    DoomGame() {
        sAppName = "DoomGame";
    }

private:

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
            if (GetKey( olc::Key::A ).bHeld) { P.a -= 4; if (P.a <   0) { P.a += 360; } }
            if (GetKey( olc::Key::D ).bHeld) { P.a += 4; if (P.a > 359) { P.a -= 360; } }
            if (GetKey( olc::Key::W ).bHeld) { P.x += dx; P.y += dy; }
            if (GetKey( olc::Key::S ).bHeld) { P.x -= dx; P.y -= dy; }
        }
        // strafe left, right
        if (GetKey( olc::Key::LEFT  ).bHeld) { P.x += dy; P.y += dx; }
        if (GetKey( olc::Key::RIGHT ).bHeld) { P.x -= dy; P.y -= dx; }
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
    void drawWall( int x1, int x2, int b1, int b2, int t1, int t2, int c, int s ) {
        // hold difference in distance
        int dyb = b2 - b1;                              // y distance of bottom line
        int dyt = t2 - t1;                              // y distance of top    line
        // we will eventually divide by the x distance, so let's prevent division by zero
        int dx  = x2 - x1; if (dx == 0) { dx = 1; }     // x distance
        int xs = x1;                                    // cache initial x1 starting position

        // clip x - clip at 1 so we can visually see that clipping is happening
        if (x1 <       1) { x1 =      1; }   // clip left
        if (x2 <       1) { x2 =      1; }   // clip left
        if (x1 >= SW - 1) { x1 = SW - 2; }   // clip right
        if (x2 >= SW - 1) { x2 = SW - 2; }   // clip right
        // draw x verticle lines
        for (int x = x1; x < x2; x++) {
            // the y start and end point
            // the + 0.5f is to prevent rounding issues
            int y1 = dyb * (x - xs + 0.5f) / dx + b1;   // y bottom point
            int y2 = dyt * (x - xs + 0.5f) / dx + t1;   // y top point

            // clip y - similar to clip x
            if (y1 <       1) { y1 =      1; }    // clip bottom
            if (y2 <       1) { y2 =      1; }    // clip bottom
            if (y1 >= SH - 1) { y1 = SH - 2; }    // clip top
            if (y2 >= SH - 1) { y2 = SH - 2; }    // clip top
            // surface
            if (S[s].surface ==  1) { S[s].surf[x] = y1; continue; }   // save bottom points
            if (S[s].surface ==  2) { S[s].surf[x] = y2; continue; }   // save top    points
            if (S[s].surface == -1) { for (int y = S[s].surf[x]; y < y1; y++) { myPixel( x, y, S[s].c1 ); }}   // bottom
            if (S[s].surface == -2) { for (int y = y2; y < S[s].surf[x]; y++) { myPixel( x, y, S[s].c2 ); }}   // top
            for (int y = y1; y < y2; y++) { myPixel( x, y, c ); }   // normal wall
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
        int wx[4], wy[4], wz[4];                   // local x, y, z values for the wall
        float CS = M.cos[P.a], SN = M.sin[P.a];    // local copies of sin and cos of player angle

        // order (sort) sectors by distance - needed for painters algorithm on the sectors
        for (int s = 0; s < numSect; s++) {
            for (int w = 0; w < numSect - s - 1; w++) {
                if (S[w].d < S[w+1].d) {
                    Sectors st = S[w]; S[w] = S[w+1]; S[w+1] = st;
                }
            }
        }

        for (int s = 0; s < numSect; s++) {
            S[s].d = 0;                            // clear distance
            if      (P.z < S[s].z1) { S[s].surface = 1; }     // bottom surface is visible
            else if (P.z > S[s].z2) { S[s].surface = 2; }     // top    surface is visible
            else                    { S[s].surface = 0; }     // only wall surface is visible

            for (int loop = 0; loop < 2; loop++) {
                for (int w = S[s].ws; w < S[s].we; w++) {

                    // offset bottom 2 points by player
                    int x1 = W[w].x1 - P.x, y1 = W[w].y1 - P.y;
                    int x2 = W[w].x2 - P.x, y2 = W[w].y2 - P.y;

                    // swap for surface
                    if (loop == 0) { int swp = x1; x1 = x2; x2 = swp; swp = y1; y1 = y2; y2 = swp; }

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
                    wz[2] = wz[0] + S[s].z2;                        // top line has elevated z w.r.t. bottom line
                    wz[3] = wz[1] + S[s].z2;
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
                    drawWall( wx[0], wx[1], wy[0], wy[1], wy[2], wy[3], W[w].c, s );
                }
                S[s].d /= (S[s].we -S[s].ws);    // find average sector distance: divide by nr of walls in sector
                S[s].surface *= -1;
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

    std::vector<int> loadSectors = {
        // wall start, wall end, z1 height, z2 height, bottom colour, top colour
         0,  4,  0, 40, 2, 3,  // sector 1
         4,  8,  0, 40, 4, 5,  // sector 2
         8, 12,  0, 40, 6, 7,  // sector 3
        12, 16,  0, 40, 0, 1,  // sector 4
    };

    std::vector<int> loadWalls = {
        // x1, y1, x2, y2, color index
         0,  0, 32,  0, 0,
        32,  0, 32, 32, 1,
        32, 32,  0, 32, 0,
         0, 32,  0,  0, 1,

        64,  0, 96,  0, 2,
        96,  0, 96, 32, 3,
        96, 32, 64, 32, 2,
        64, 32, 64,  0, 3,

        64, 64, 96, 64, 4,
        96, 64, 96, 96, 5,
        96, 96, 64, 96, 4,
        64, 96, 64, 64, 5,

         0, 64, 32, 64, 6,
        32, 64, 32, 96, 7,
        32, 96,  0, 96, 6,
         0, 96,  0, 64, 7,
    };

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
        // load sectors
        int v1 = 0, v2 = 0;
        for (int s = 0; s < numSect; s++) {
            S[s].ws = loadSectors[ v1 + 0 ];                           // wall start number
            S[s].we = loadSectors[ v1 + 1 ];                           // wall end   number
            S[s].z1 = loadSectors[ v1 + 2 ];                           // sector bottom height
            S[s].z2 = loadSectors[ v1 + 3 ] - loadSectors[ v1 + 2 ];   // sector top    height
            S[s].c1 = loadSectors[ v1 + 4 ];                           // sector top    colour
            S[s].c2 = loadSectors[ v1 + 5 ];                           // sector bottom colour
            v1 += 6;
            // load walls into sectors
            for (int w = S[s].ws; w < S[s].we; w++) {
                W[w].x1 = loadWalls[ v2 + 0 ];   // bottom x1
                W[w].y1 = loadWalls[ v2 + 1 ];   // bottom y1
                W[w].x2 = loadWalls[ v2 + 2 ];   // top    x2
                W[w].y2 = loadWalls[ v2 + 3 ];   // top    y2
                W[w].c  = loadWalls[ v2 + 4 ];   // wall colour
                v2 += 5;
            }
        }
    }

public:
    bool OnUserCreate() override {

        // initialize your assets here
        init();

        return true;
    }

    bool OnUserUpdate( float fElapsedTime ) override {

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

