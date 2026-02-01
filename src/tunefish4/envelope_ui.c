#include "envelope_ui.h"
#include "ft2_structs.h"
#include "ft2_gui.h"
#include "ft2_mouse.h"
#include "ft2_video.h"

// TODO: Integrate FT2 instrument editor's envelope code here

// Synth envelope data structures (mirroring instrument envelope format)
typedef struct {
    int16_t points[12][2];  // [point][x,y] 
    uint8_t length;         // number of points
    uint8_t sustain;        // sustain point
    uint8_t loopStart;      // loop start point
    uint8_t loopEnd;        // loop end point
    uint8_t flags;          // ENV_ENABLED, ENV_SUSTAIN, ENV_LOOP
    uint8_t currPoint;      // currently selected point
} synthEnvelope_t;

// Two ADSR envelopes for synth
static synthEnvelope_t synthADSR1;
static synthEnvelope_t synthADSR2;
static bool updateSynthEnv1 = false;
static bool updateSynthEnv2 = false;

// Envelope flags (from ft2_structs.h)
#define ENV_ENABLED 1
#define ENV_SUSTAIN 2
#define ENV_LOOP    4

// Initialize default ADSR envelopes
static void initSynthEnvelopes(void)
{
    // ADSR1 - default volume envelope
    synthADSR1.points[0][0] = 0;   synthADSR1.points[0][1] = 0;   // start
    synthADSR1.points[1][0] = 10;  synthADSR1.points[1][1] = 64; // attack peak
    synthADSR1.points[2][0] = 30;  synthADSR1.points[2][1] = 45; // decay to sustain
    synthADSR1.points[3][0] = 100; synthADSR1.points[3][1] = 45; // sustain
    synthADSR1.points[4][0] = 150; synthADSR1.points[4][1] = 0;  // release
    synthADSR1.length = 5;
    synthADSR1.sustain = 2;
    synthADSR1.loopStart = 0;
    synthADSR1.loopEnd = 0;
    synthADSR1.flags = ENV_ENABLED | ENV_SUSTAIN;
    synthADSR1.currPoint = 0;

    // ADSR2 - default filter envelope  
    synthADSR2.points[0][0] = 0;   synthADSR2.points[0][1] = 32; // start at center
    synthADSR2.points[1][0] = 15;  synthADSR2.points[1][1] = 50; // attack
    synthADSR2.points[2][0] = 40;  synthADSR2.points[2][1] = 35; // decay
    synthADSR2.points[3][0] = 120; synthADSR2.points[3][1] = 35; // sustain
    synthADSR2.points[4][0] = 180; synthADSR2.points[4][1] = 32; // release to center
    synthADSR2.length = 5;
    synthADSR2.sustain = 2;
    synthADSR2.loopStart = 0;
    synthADSR2.loopEnd = 0;
    synthADSR2.flags = ENV_ENABLED | ENV_SUSTAIN;
    synthADSR2.currPoint = 0;
}

// Scaled envelope drawing functions (60% size)
static void envPixel(int32_t envNum, int16_t x, int16_t y, uint8_t pal)
{
    // Calculate envelope panel positions
    int panelY = 520; // ADSR panel y position
    int panelW = ((SCREEN_W * 3) / 5) / 2 - 20; // panel width
    int envW = (int)(panelW * 0.9); // envelope area width (90% of panel)
    int envH = 48; // envelope area height (60% of 80px panel height)
    
    int baseX = (envNum == 0) ? 24 : (24 + panelW + 20); // ADSR1 vs ADSR2 x position
    int baseY = panelY + 14; // offset from panel top
    
    // Scale coordinates to fit envelope area
    x = (x * envW) / 324; // scale from 324 to envW
    y = (y * envH) / 67;  // scale from 67 to envH
    
    if (x >= 0 && x < envW && y >= 0 && y < envH)
        video.frameBuffer[((baseY + y) * SCREEN_W) + (baseX + x)] = video.palette[pal];
}

static void envLine(int32_t envNum, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t pal)
{
    x1 = CLAMP(x1, 0, 324); x2 = CLAMP(x2, 0, 324);
    y1 = CLAMP(y1, 0, 67);  y2 = CLAMP(y2, 0, 67);

    const int16_t dx = x2 - x1;
    const uint16_t ax = ABS(dx) << 1;
    const int16_t sx = SGN(dx);
    const int16_t dy = y2 - y1;
    const uint16_t ay = ABS(dy) << 1;
    const int16_t sy = SGN(dy);
    int16_t x = x1;
    int16_t y = y1;

    if (ax > ay) {
        int16_t d = ay - (ax >> 1);
        while (true) {
            envPixel(envNum, x, y, pal);
            if (x == x2) break;
            if (d >= 0) { d -= ax; y += sy; }
            x += sx; d += ay;
        }
    } else {
        int16_t d = ax - (ay >> 1);
        while (true) {
            envPixel(envNum, x, y, pal);
            if (y == y2) break;
            if (d >= 0) { d -= ay; x += sx; }
            y += sy; d += ax;
        }
    }
}

static void envDot(int32_t envNum, int16_t x, int16_t y)
{
    envPixel(envNum, x-1, y-1, PAL_BLCKTXT);
    envPixel(envNum, x,   y-1, PAL_BLCKTXT);
    envPixel(envNum, x+1, y-1, PAL_BLCKTXT);
    envPixel(envNum, x-1, y,   PAL_BLCKTXT);
    envPixel(envNum, x,   y,   PAL_BLCKTXT);
    envPixel(envNum, x+1, y,   PAL_BLCKTXT);
    envPixel(envNum, x-1, y+1, PAL_BLCKTXT);
    envPixel(envNum, x,   y+1, PAL_BLCKTXT);
    envPixel(envNum, x+1, y+1, PAL_BLCKTXT);
}

static void writeSynthEnvelope(int32_t envNum)
{
    synthEnvelope_t *env = (envNum == 0) ? &synthADSR1 : &synthADSR2;
    
    // Clear envelope area (relative to panel), clamped to screen
    int panelY = 520;
    int panelW = ((SCREEN_W * 3) / 5) / 2 - 20;
    int baseX = (envNum == 0) ? 24 : (24 + panelW + 20);
    int baseY = panelY + 14;
    int envW = (int)(panelW * 0.9);
    int envH = 48;
    // clamp clear to screen height
    if (baseY < SCREEN_H)
    {
        int h0 = envH;
        if (baseY + h0 > SCREEN_H)
            h0 = SCREEN_H - baseY;
        clearRect(baseX, baseY, envW, h0);
    }
    
    // Draw dotted grid lines
    for (int i = 0; i <= 16; i++) envPixel(envNum, 2, 1 + i * 3, PAL_PATTEXT);
    for (int i = 0; i <= 4; i++) envPixel(envNum, 2, 1 + i * 12, PAL_PATTEXT);
    for (int i = 0; i <= 81; i++) envPixel(envNum, 4 + i * 4, envH-2, PAL_PATTEXT);
    for (int i = 0; i <= 3; i++) envPixel(envNum, 4 + i * 80, envH-1, PAL_PATTEXT);
    
    // Draw center line for filter envelope (ADSR2)
    if (envNum == 1)
        envLine(envNum, 4, envH/2, 320, envH/2, PAL_BLCKMRK);
    
    int16_t nd = env->length;
    if (nd > 12) nd = 12;
    
    // Draw envelope points and lines
    int16_t lx = 0, ly = 0;
    for (int i = 0; i < nd; i++) {
        int16_t x = env->points[i][0];
        int16_t y = env->points[i][1];
        
        x = CLAMP(x, 0, 324);
        if (envNum == 0) // volume envelope
            y = CLAMP(y, 0, 64);
        else // filter envelope  
            y = CLAMP(y, 0, 63);
        
        if ((uint16_t)env->points[i][0] <= 324) {
            envDot(envNum, 4 + x, envH-2 - y);
            
            // Draw selection indicator
            if (i == env->currPoint) {
                envPixel(envNum, 2 + x, envH-2 - y, PAL_BLCKTXT);
                envPixel(envNum, 6 + x, envH-2 - y, PAL_BLCKTXT);
                envPixel(envNum, 4 + x, envH-4 - y, PAL_BLCKTXT);
                envPixel(envNum, 4 + x, envH - y, PAL_BLCKTXT);
            }
            
            // Draw sustain marker
            if (i == env->sustain && (env->flags & ENV_SUSTAIN)) {
                for (int sy = 1; sy < envH-1; sy += 2)
                    envPixel(envNum, 4 + x, sy, PAL_BLCKTXT);
            }
        }
        
        // Draw connecting line
        if (i > 0 && lx < x)
            envLine(envNum, 4 + lx, envH-2 - ly, 4 + x, envH-2 - y, PAL_PATTEXT);
        
        lx = x; ly = y;
    }
}

void env_handleRedraw(void)
{
    static bool initialized = false;
    if (!initialized) {
        initSynthEnvelopes();
        initialized = true;
        updateSynthEnv1 = updateSynthEnv2 = true;
    }
    
    if (updateSynthEnv1) {
        updateSynthEnv1 = false;
        writeSynthEnvelope(0);
    }
    
    if (updateSynthEnv2) {
        updateSynthEnv2 = false; 
        writeSynthEnvelope(1);
    }
}

bool env_handleMouse(bool mouseButtonDown)
{
    if (!mouseButtonDown) return false;
    
    int panelY = 520;
    int panelW = ((SCREEN_W * 3) / 5) / 2 - 20;
    int envH = 48;
    
    // Check ADSR1 panel
    int baseX1 = 24;
    int baseY1 = panelY + 14;
    int envW = (int)(panelW * 0.9);
    
    if (mouse.x >= baseX1 && mouse.x < baseX1 + envW && 
        mouse.y >= baseY1 && mouse.y < baseY1 + envH) {
        
        // Convert screen coordinates to envelope coordinates
        int envX = ((mouse.x - baseX1 - 4) * 324) / (envW - 8);
        int envY = 64 - ((mouse.y - baseY1 - 1) * 64) / (envH - 3);
        envX = CLAMP(envX, 0, 324);
        envY = CLAMP(envY, 0, 64);
        
        // Find closest point
        int closest = 0;
        int minDist = 9999;
        for (int i = 0; i < synthADSR1.length; i++) {
            int dx = synthADSR1.points[i][0] - envX;
            int dy = synthADSR1.points[i][1] - envY;
            int dist = dx*dx + dy*dy;
            if (dist < minDist) {
                minDist = dist;
                closest = i;
            }
        }
        
        if (minDist < 400) { // within reasonable distance
            synthADSR1.currPoint = closest;
            updateSynthEnv1 = true;
        }
        return true;
    }
    
    // Check ADSR2 panel
    int baseX2 = 24 + panelW + 20;
    if (mouse.x >= baseX2 && mouse.x < baseX2 + envW &&
        mouse.y >= baseY1 && mouse.y < baseY1 + envH) {
        
        int envX = ((mouse.x - baseX2 - 4) * 324) / (envW - 8);
        int envY = 63 - ((mouse.y - baseY1 - 1) * 63) / (envH - 3);
        envX = CLAMP(envX, 0, 324);
        envY = CLAMP(envY, 0, 63);
        
        int closest = 0;
        int minDist = 9999;
        for (int i = 0; i < synthADSR2.length; i++) {
            int dx = synthADSR2.points[i][0] - envX;
            int dy = synthADSR2.points[i][1] - envY;
            int dist = dx*dx + dy*dy;
            if (dist < minDist) {
                minDist = dist;
                closest = i;
            }
        }
        
        if (minDist < 400) {
            synthADSR2.currPoint = closest;
            updateSynthEnv2 = true;
        }
        return true;
    }
    
    return false;
} 