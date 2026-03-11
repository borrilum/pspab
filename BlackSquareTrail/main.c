#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("BlackSquareTrail", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

// PSP screen size.
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

// The moving square is 16x16 pixels.
#define SQUARE_SIZE 16

// Simple movement speed (pixels per frame).
#define MOVE_SPEED 2.0f

// Analog stick center and deadzone.
#define ANALOG_CENTER 128
#define ANALOG_DEADZONE 30

// Keep trail storage finite and simple.
#define MAX_TRAILS 8192

// GU display list buffer.
static unsigned int guList[262144] __attribute__((aligned(16)));

// Basic 2D vertex type for GU_SPRITES.
typedef struct
{
    short x, y, z;
} Vertex;

// Trail point storage.
typedef struct
{
    int x;
    int y;
} TrailPoint;

static TrailPoint g_trails[MAX_TRAILS];
static int g_trailCount = 0;

// ------------------------------------------------------------
// Exit callback boilerplate (required for clean app exit on PSP)
// ------------------------------------------------------------
static int exitCallback(int arg1, int arg2, void *common)
{
    (void)arg1;
    (void)arg2;
    (void)common;
    sceKernelExitGame();
    return 0;
}

static int callbackThread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    int cbid = sceKernelCreateCallback("Exit Callback", exitCallback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static int setupExitCallback(void)
{
    int thid = sceKernelCreateThread("update_thread", callbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
    {
        sceKernelStartThread(thid, 0, 0);
    }
    return thid;
}

// ------------------------------------------------------------
// Very small GU setup/teardown
// ------------------------------------------------------------
static void initGu(void)
{
    sceGuInit();

    sceGuStart(GU_DIRECT, guList);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, 512);
    sceGuDispBuffer(SCREEN_WIDTH, SCREEN_HEIGHT, (void *)0x88000, 512);
    sceGuDepthBuffer((void *)0x110000, 512);

    sceGuOffset(2048 - (SCREEN_WIDTH / 2), 2048 - (SCREEN_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);

    // We are doing simple flat 2D drawing.
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);

    sceGuFinish();
    sceGuSync(0, 0);

    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

static void shutdownGu(void)
{
    sceGuTerm();
}

// Draw a filled rectangle using GU_SPRITES.
static void drawFilledRect(int x, int y, int w, int h, unsigned int color)
{
    Vertex *verts = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));

    verts[0].x = (short)x;
    verts[0].y = (short)y;
    verts[0].z = 0;

    verts[1].x = (short)(x + w);
    verts[1].y = (short)(y + h);
    verts[1].z = 0;

    sceGuColor(color);
    sceGuDrawArray(GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, verts);
}

// Keep coordinate in the valid visible area.
static float clampf(float value, float minValue, float maxValue)
{
    if (value < minValue)
        return minValue;
    if (value > maxValue)
        return maxValue;
    return value;
}

int main(void)
{
    setupExitCallback();

    // Use positive logic button reads + analog support.
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initGu();

    // Start near center.
    float squareX = (SCREEN_WIDTH - SQUARE_SIZE) * 0.5f;
    float squareY = (SCREEN_HEIGHT - SQUARE_SIZE) * 0.5f;

    unsigned int previousButtons = 0;
    int running = 1;

    while (running)
    {
        SceCtrlData pad;
        memset(&pad, 0, sizeof(pad));
        sceCtrlReadBufferPositive(&pad, 1);

        // Clean exit when START is pressed.
        if (pad.Buttons & PSP_CTRL_START)
        {
            running = 0;
        }

        // Digital D-pad movement.
        float dx = 0.0f;
        float dy = 0.0f;

        if (pad.Buttons & PSP_CTRL_LEFT)
            dx -= 1.0f;
        if (pad.Buttons & PSP_CTRL_RIGHT)
            dx += 1.0f;
        if (pad.Buttons & PSP_CTRL_UP)
            dy -= 1.0f;
        if (pad.Buttons & PSP_CTRL_DOWN)
            dy += 1.0f;

        // Analog movement (normalized to roughly -1..1 with deadzone).
        int rawAx = (int)pad.Lx - ANALOG_CENTER;
        int rawAy = (int)pad.Ly - ANALOG_CENTER;

        if (abs(rawAx) > ANALOG_DEADZONE)
        {
            dx += (float)rawAx / (float)ANALOG_CENTER;
        }
        if (abs(rawAy) > ANALOG_DEADZONE)
        {
            dy += (float)rawAy / (float)ANALOG_CENTER;
        }

        // Apply movement with a simple fixed speed.
        squareX += dx * MOVE_SPEED;
        squareY += dy * MOVE_SPEED;

        // Stay inside visible screen bounds.
        squareX = clampf(squareX, 0.0f, (float)(SCREEN_WIDTH - SQUARE_SIZE));
        squareY = clampf(squareY, 0.0f, (float)(SCREEN_HEIGHT - SQUARE_SIZE));

        // X leaves black stamps each frame while held.
        if ((pad.Buttons & PSP_CTRL_CROSS) && g_trailCount < MAX_TRAILS)
        {
            g_trails[g_trailCount].x = (int)squareX;
            g_trails[g_trailCount].y = (int)squareY;
            g_trailCount++;
        }

        // Triangle clears all trails on press edge.
        if ((pad.Buttons & PSP_CTRL_TRIANGLE) && !(previousButtons & PSP_CTRL_TRIANGLE))
        {
            g_trailCount = 0;
        }

        previousButtons = pad.Buttons;

        // ------------------- Render -------------------
        sceGuStart(GU_DIRECT, guList);

        // White background clear.
        sceGuClearColor(0xFFFFFFFF);
        sceGuClear(GU_COLOR_BUFFER_BIT);

        // Draw all saved trail stamps in black.
        for (int i = 0; i < g_trailCount; i++)
        {
            drawFilledRect(g_trails[i].x, g_trails[i].y, SQUARE_SIZE, SQUARE_SIZE, 0xFF000000);
        }

        // Draw current moving square in black.
        drawFilledRect((int)squareX, (int)squareY, SQUARE_SIZE, SQUARE_SIZE, 0xFF000000);

        sceGuFinish();
        sceGuSync(0, 0);

        // Wait one frame (simple, stable pacing).
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    shutdownGu();
    sceKernelExitGame();
    return 0;
}
