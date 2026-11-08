#define SDL_MAIN_HANDLED

#ifdef PLATFORM_PSP
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspaudio.h>
#include <psppower.h>
#include <pspmscm.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <raylib.h>
#include <physics.h>
#include <graphics.h>
#include <random.h>
#include <constants.h>


#ifdef PLATFORM_PSP
PSP_MODULE_INFO("Watermelon Game", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

SceCtrlData Pad;

float timerStart, timerEnd;
const int quickSleep = 10000;
const int longSleep = 10000;

volatile bool SleepMode = false;
volatile bool SleepModeStart = false;
volatile bool SleepModeStarted = false;
volatile bool SleepModeResume = false;


int MyPowerCallback(int unknown, int pwrflags, void *common) {
    if (pwrflags & (PSP_POWER_CB_SUSPENDING | PSP_POWER_CB_STANDBY)) {
        SleepModeStarted = false;
        SleepModeStart = true;
        while (!SleepModeStarted) {
            sceKernelDelayThread(quickSleep);
        }
    }
    if (pwrflags & PSP_POWER_CB_RESUME_COMPLETE) {
        SleepModeResume = true;
    }
    return 0;
}


int MyCallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Power Callback", MyPowerCallback, NULL);
    int result = scePowerRegisterCallback(0, cbid);
    TraceLog(LOG_INFO, "Power registration: %08X\n", result);
    sceKernelSleepThreadCB(); 
    return 0;
}


void MySetupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", MyCallbackThread, 0x11, 0x4000, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}
#endif

int screenWidth = ATTR_PSP_WIDTH;
int screenHeight = ATTR_PSP_HEIGHT;
float PixelsPerUnit = 30.0f;
float BoxHeight = 7.6f;
float BoxTopLineHeight = 6.7f;
float BoxDiagonalLineHeight = 0.5f;
float BoxWidth = 6.5f;
float BoxLeftMargin = 4.75f;
float BoxTopMargin = 1.0f;
float BoxFloorY = 8.6f;
float Gravity = 15.2f;
bool isPortrait = false;

float DeltaTime = 0.0f;
int initialBalls = 0;
int Score = 0;
int DispScore = 0;
int MusicTrack = 0;
bool GameOver = false;
bool SelectPressed = false;

typedef enum {
    SOUND_LOUD = 0,
    SOUND_QUIET,
    SOUND_OFF
} SoundMode;

static SoundMode currentSoundMode = SOUND_LOUD;
static int highScore = 0;

#if defined(PLATFORM_ANDROID)
#include <android/native_activity.h>
#include "android/android_native_app_glue.h"
extern struct android_app *GetAndroidApp(void);
#endif

static const char* GetHighScorePath(void) {
    static char path[512] = { 0 };
    if (path[0] != '\0') return path;
#if defined(PLATFORM_ANDROID)
    // Raylib's Android fopen wrapper resolves relative write paths into the
    // activity's private data directory.
    snprintf(path, sizeof(path), "highscore.txt");
#elif defined(PLATFORM_PSP)
    snprintf(path, sizeof(path), "highscore.txt");
#else
    const char* appDir = GetApplicationDirectory();
    if (appDir && appDir[0] != '\0') {
        int len = (int)strlen(appDir);
        if (appDir[len - 1] == '/' || appDir[len - 1] == '\\') {
            snprintf(path, sizeof(path), "%shighscore.txt", appDir);
        } else {
            snprintf(path, sizeof(path), "%s/highscore.txt", appDir);
        }
    } else {
        snprintf(path, sizeof(path), "highscore.txt");
    }
#endif
    return path;
}

static int LoadHighScore(void) {
    const char* hsPath = GetHighScorePath();
    int hs = 0;
    FILE* f = fopen(hsPath, "rb");
    if (f != NULL) {
        if (fread(&hs, sizeof(hs), 1, f) != 1) {
            hs = 0;
        }
        fclose(f);
    }
    return hs;
}

static void SaveHighScore(int score) {
    const char* hsPath = GetHighScorePath();
    int currentHs = LoadHighScore();
    if (score > currentHs) {
        FILE* f = fopen(hsPath, "wb");
        if (f != NULL) {
            fwrite(&score, sizeof(score), 1, f);
            fflush(f);
            fclose(f);
        }
    }
}

int main(int argc, char *argv[]) {
    int scale = 1;
#ifdef PLATFORM_PSP
    MySetupCallbacks();
    screenWidth = ATTR_PSP_WIDTH;
    screenHeight = ATTR_PSP_HEIGHT;
    PixelsPerUnit = 30.0f;
    BoxHeight = 7.6f;
    BoxTopLineHeight = 6.7f;
    BoxDiagonalLineHeight = 0.5f;
    BoxTopMargin = 1.0f;
    BoxFloorY = 8.6f;
    BoxLeftMargin = (ATTR_PSP_WIDTH / PixelsPerUnit / 2.0f) - (BoxWidth / 2.0f);
    Gravity = 2.0f * BoxHeight / (TimeToFall * TimeToFall);
    scale = 1;
    InitWindow(screenWidth, screenHeight, "Suika");
#else
#if defined(PLATFORM_ANDROID)
    InitWindow(0, 0, "Suika");
    screenWidth = GetScreenWidth();
    screenHeight = GetScreenHeight();
#else
    int baseWidth = 640;
    int baseHeight = 480;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--480x640") == 0 || strcmp(argv[i], "--portrait") == 0) {
            baseWidth = 480; baseHeight = 640;
        } else if (strcmp(argv[i], "--640x480") == 0) {
            baseWidth = 640; baseHeight = 480;
        } else if (strcmp(argv[i], "--480x272") == 0 || strcmp(argv[i], "--psp") == 0) {
            baseWidth = 480; baseHeight = 272;
        }
    }

    screenWidth = baseWidth;
    screenHeight = baseHeight;
    InitWindow(screenWidth, screenHeight, "Suika");
#endif

    isPortrait = (screenHeight > screenWidth);

    if (isPortrait) {
        // Find the largest integer scale that fits the playfield width and
        // the playfield plus its scale-relative bottom bar in the height.
        // The old fixed 40px height deduction did not match the actual bar,
        // which is 20px per scale unit.
        scale = 1;
        for (int candidate = 1; ; candidate++) {
            float ppu = 30.0f * (float)candidate;
            float boxWidthPx = 6.5f * ppu;
            float bottomBarPx = 40.0f * ((float)candidate / 2.0f);
            float topMargin = ((float)screenHeight / ppu) - 7.6f - (bottomBarPx / ppu);
            if (topMargin < 2.0f) topMargin = 2.0f;
            float requiredHeightPx = (topMargin + 7.6f) * ppu + bottomBarPx;
            if (boxWidthPx > (float)screenWidth || requiredHeightPx > (float)screenHeight) break;
            scale = candidate;
        }

        PixelsPerUnit = 30.0f * (float)scale;
        BoxWidth = 6.5f;
        BoxHeight = 7.6f;
        BoxTopLineHeight = 6.7f;
        BoxDiagonalLineHeight = 0.5f;

        float bottomBarPx = 40.0f * ((float)scale / 2.0f);
        BoxTopMargin = ((float)screenHeight / PixelsPerUnit) - BoxHeight - (bottomBarPx / PixelsPerUnit);
        if (BoxTopMargin < 2.0f) BoxTopMargin = 2.0f;
        BoxFloorY = BoxTopMargin + BoxHeight;
        BoxLeftMargin = ((float)screenWidth / PixelsPerUnit / 2.0f) - (BoxWidth / 2.0f);
        Gravity = 2.0f * BoxHeight / (TimeToFall * TimeToFall);
    } else if (screenWidth == 640 && screenHeight == 480) {
        scale = 2;
        PixelsPerUnit = 60.0f;
        BoxWidth = 6.5f;
        BoxHeight = 7.2f;
        BoxTopLineHeight = 6.4f;
        BoxDiagonalLineHeight = 0.5f;
        BoxTopMargin = 0.4f;
        BoxFloorY = BoxTopMargin + BoxHeight;
        BoxLeftMargin = ((float)screenWidth / PixelsPerUnit / 2.0f) - (BoxWidth / 2.0f);
        Gravity = 2.0f * BoxHeight / (TimeToFall * TimeToFall);
    } else {
        scale = 1;
        PixelsPerUnit = 30.0f;
        BoxWidth = 6.5f;
        BoxHeight = 7.6f;
        BoxTopLineHeight = 6.7f;
        BoxDiagonalLineHeight = 0.5f;
        BoxTopMargin = 1.0f;
        BoxFloorY = BoxTopMargin + BoxHeight;
        BoxLeftMargin = ((float)screenWidth / PixelsPerUnit / 2.0f) - (BoxWidth / 2.0f);
        Gravity = 2.0f * BoxHeight / (TimeToFall * TimeToFall);
    }
#endif

#if !defined(PLATFORM_PSP)
    Image iconImg = LoadImage("Resources/icon.png");
    if (iconImg.data != NULL) {
        SetWindowIcon(iconImg);
        UnloadImage(iconImg);
    }
#endif

#ifdef PLATFORM_PSP
    SDL_Init(SDL_INIT_AUDIO);
    Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 2048);
    Mix_Chunk* popSfx = Mix_LoadWAV("Resources/Pop.wav");
    Mix_Chunk* whooshSfx = Mix_LoadWAV("Resources/Whoosh.wav");

    Mix_Music *firstDropMusic = Mix_LoadMUS("Resources/Watermelon Game - First Drop.mp3");
    Mix_Music *smallFruitsMusic = Mix_LoadMUS("Resources/Watermelon Game - Small Fruits.mp3");

    int initialVol = (currentSoundMode == SOUND_LOUD) ? 128 : (currentSoundMode == SOUND_QUIET ? 48 : 0);
    Mix_VolumeMusic(initialVol);
    Mix_Volume(-1, initialVol);
    Mix_PlayMusic(firstDropMusic, 0);
#else
    InitAudioDevice();
    Sound popSfx = LoadSound("Resources/Pop.wav");
    Sound whooshSfx = LoadSound("Resources/Whoosh.wav");
    Music firstDropMusic = LoadMusicStream("Resources/Watermelon Game - First Drop.mp3");
    Music smallFruitsMusic = LoadMusicStream("Resources/Watermelon Game - Small Fruits.mp3");
    firstDropMusic.looping = false;
    smallFruitsMusic.looping = false;
    float initialVol = (currentSoundMode == SOUND_LOUD) ? 1.0f : (currentSoundMode == SOUND_QUIET ? 0.375f : 0.0f);
    SetMusicVolume(firstDropMusic, initialVol);
    SetMusicVolume(smallFruitsMusic, initialVol);
    SetSoundVolume(popSfx, initialVol);
    SetSoundVolume(whooshSfx, initialVol);
    PlayMusicStream(firstDropMusic);
#endif
    MusicTrack = 1;
    
    Image fruitsImg = LoadImage("Resources/WatermelonGame.png");
    int texScale = (int)roundf(PixelsPerUnit / 30.0f);
    if (texScale < 1) texScale = 1;
    if (texScale > 1) {
        ImageResizeNN(&fruitsImg, fruitsImg.width * texScale, fruitsImg.height * texScale);
    }
    Texture2D fruitsTexture = LoadTextureFromImage(fruitsImg);
    UnloadImage(fruitsImg);
    SetTextureFilter(fruitsTexture, TEXTURE_FILTER_BILINEAR);

    float fontSize = (scale == 1) ? 20.0f : 12.0f * (float)scale;
    Font gameFont = LoadFontEx("Resources/Font.otf", (int)fontSize, NULL, 0);

    highScore = LoadHighScore();

    SetTargetFPS(60);
    initRng();

    struct Ball heldBall, nextBall;
    struct Ball balls [BallsLimit];
    float timeSinceLastBall = 0.5f;
    int BallsN = 0;

    initBall(&heldBall);
    initBall(&nextBall);
    for (int i = 0; i < BallsLimit; i++) {
        initBall(&balls[i]);
    }

    for (int i = 0; i < initialBalls; i++) {
        initBall(&balls[i]);
        balls[BallsN] = heldBall;
        balls[BallsN].x = rngf(BoxLeftMargin, BoxLeftMargin+BoxWidth);
        BallsN++;
        randomizeRadius(&heldBall);
    }

    while (!WindowShouldClose()) {
#if defined(PLATFORM_ANDROID)
        struct android_app* app = GetAndroidApp();
        if (app && app->destroyRequested) {
            break;
        }
#endif
#ifdef PLATFORM_PSP
        if (SleepModeStart) {
            scePowerLock(0);

            SleepModeStart = false;
            SleepMode = true;

            Mix_HaltMusic();
            Mix_HaltChannel(-1);

            Mix_FreeChunk(popSfx);
            Mix_FreeChunk(whooshSfx);
            Mix_FreeMusic(firstDropMusic);
            Mix_FreeMusic(smallFruitsMusic);

            popSfx = NULL;
            whooshSfx = NULL;
            firstDropMusic = NULL;
            smallFruitsMusic = NULL;

            Mix_CloseAudio();

            sceKernelDelayThreadCB(longSleep); 
            SleepModeStarted = true;

            scePowerUnlock(0);
        }

        if (SleepModeResume) {
            SleepModeResume = false;
            SleepMode = false;

            scePowerTick(0);
            sceKernelDelayThread(longSleep);
            Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 2048);

            while (!(popSfx && whooshSfx && firstDropMusic && smallFruitsMusic)) {
                popSfx = Mix_LoadWAV("Resources/Pop.wav");
                whooshSfx = Mix_LoadWAV("Resources/Whoosh.wav");

                firstDropMusic = Mix_LoadMUS("Resources/Watermelon Game - First Drop.mp3");
                smallFruitsMusic = Mix_LoadMUS("Resources/Watermelon Game - Small Fruits.mp3");
            
                if (!(popSfx && whooshSfx && firstDropMusic && smallFruitsMusic)) {
                    Mix_FreeChunk(popSfx);
                    Mix_FreeChunk(whooshSfx);
                    Mix_FreeMusic(firstDropMusic);
                    Mix_FreeMusic(smallFruitsMusic);
                    popSfx = NULL;
                    whooshSfx = NULL;
                    firstDropMusic = NULL;
                    smallFruitsMusic = NULL;
                    sceKernelDelayThread(longSleep);
                }
            }

            if (MusicTrack == 1) {
                Mix_PlayMusic(firstDropMusic, 0);
            } else if (MusicTrack == 2) {
                Mix_PlayMusic(smallFruitsMusic, 0);
            }
        }

        if (SleepMode) {
            sceKernelDelayThreadCB(quickSleep); 
            continue;
        }

        scePowerTick(0);
#endif

#ifdef PLATFORM_PSP
        if (Mix_PlayingMusic() == 0 && MusicTrack == 1) {
            Mix_PlayMusic(smallFruitsMusic, 0);
            MusicTrack = 2;
        } else if (Mix_PlayingMusic() == 0 && MusicTrack == 2) {
            Mix_PlayMusic(firstDropMusic, 0);
            MusicTrack = 1;
        }
#else
        if (MusicTrack == 1) {
            UpdateMusicStream(firstDropMusic);
            if (GetMusicTimePlayed(firstDropMusic) >= GetMusicTimeLength(firstDropMusic) || !IsMusicStreamPlaying(firstDropMusic)) {
                StopMusicStream(firstDropMusic);
                PlayMusicStream(smallFruitsMusic);
                MusicTrack = 2;
            }
        } else if (MusicTrack == 2) {
            UpdateMusicStream(smallFruitsMusic);
            if (GetMusicTimePlayed(smallFruitsMusic) >= GetMusicTimeLength(smallFruitsMusic) || !IsMusicStreamPlaying(smallFruitsMusic)) {
                StopMusicStream(smallFruitsMusic);
                PlayMusicStream(firstDropMusic);
                MusicTrack = 1;
            }
        }
#endif

        DeltaTime = ((float) GetFrameTime()) * InvTicksPerFrame;

        if (DeltaTime > (1.0f / 10.0f) * InvTicksPerFrame) {
            DeltaTime = (1.0f / 10.0f) * InvTicksPerFrame;
        }

        // Unified Input Handling
        bool btnLeft = false;
        bool btnRight = false;
        bool btnDrop = false;
        bool btnRestart = false;
        bool btnSelect = false;
        float analogPercent = 1.0f;
        bool hasDirectPointer = false;
        float pointerWorldX = 0.0f;
        bool isPointerActive = false;
#ifndef PLATFORM_PSP
        float floorTopY = BoxFloorY * PPU;
        float bottomBarCenterY = floorTopY + (screenHeight - floorTopY) * 0.5f;
        float minBarHeight = 16.0f * (float)scale;
        if (screenHeight - floorTopY < minBarHeight) {
            bottomBarCenterY = (float)screenHeight - (minBarHeight * 0.5f);
        }
        float btnSoundCenterX = isPortrait ? (30.0f * (float)scale) : 65.0f;
        float btnRestartCenterX = (float)screenWidth - (isPortrait ? (30.0f * (float)scale) : 65.0f);
        Vector2 btnSoundCenter = { btnSoundCenterX, bottomBarCenterY };
        Vector2 btnRestartCenter = { btnRestartCenterX, bottomBarCenterY };
        float btnPressRadius = isPortrait ? (17.0f * (float)scale) : 26.0f;

        Vector2 pointerScreenPos = { 0, 0 };
        static bool touchStartedInBottomBar = false;
        static Vector2 dropRipplePos = { 0, 0 };
        static float dropRippleTimer = 0.0f;

        if (dropRippleTimer > 0.0f) {
            dropRippleTimer -= GetFrameTime();
            if (dropRippleTimer < 0.0f) dropRippleTimer = 0.0f;
        }
#endif

#ifdef PLATFORM_PSP
        sceCtrlPeekBufferPositive(&Pad, 1);
        unsigned char analogX = Pad.Lx;
        btnLeft = ((Pad.Buttons & PSP_CTRL_LTRIGGER) || (Pad.Buttons & PSP_CTRL_LEFT) || analogX < 128 - (128*AnalogDeadzonePercent));
        btnRight = ((Pad.Buttons & PSP_CTRL_RTRIGGER) || (Pad.Buttons & PSP_CTRL_RIGHT) || analogX > 128 + (128*AnalogDeadzonePercent));
        analogPercent = (fabsf((float)(128-analogX))/128.0f);
        if ((Pad.Buttons & PSP_CTRL_LTRIGGER) || (Pad.Buttons & PSP_CTRL_LEFT) || (Pad.Buttons & PSP_CTRL_RTRIGGER) || (Pad.Buttons & PSP_CTRL_RIGHT)) {
            analogPercent = 1.0f;
        }
        btnDrop = ((Pad.Buttons & PSP_CTRL_LTRIGGER && Pad.Buttons & PSP_CTRL_CROSS) || (Pad.Buttons & PSP_CTRL_CIRCLE));
        btnRestart = (Pad.Buttons & PSP_CTRL_START) != 0;
        btnSelect = (Pad.Buttons & PSP_CTRL_SELECT) != 0;
#else
        // Keyboard controls
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) { btnLeft = true; analogPercent = 1.0f; }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { btnRight = true; analogPercent = 1.0f; }
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) btnDrop = true;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R)) btnRestart = true;
        if (IsKeyPressed(KEY_M) || IsKeyPressed(KEY_V)) btnSelect = true;

        // Gamepad controls
        if (IsGamepadAvailable(0)) {
            if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) { btnLeft = true; analogPercent = 1.0f; }
            if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) { btnRight = true; analogPercent = 1.0f; }
            float axisX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
            if (fabsf(axisX) > AnalogDeadzonePercent) {
                if (axisX < 0) { btnLeft = true; analogPercent = fabsf(axisX); }
                else { btnRight = true; analogPercent = fabsf(axisX); }
            }
            if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) btnDrop = true;
            if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) btnRestart = true;
            if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT)) btnSelect = true;
        }

        // Mouse & Touch controls
        if (GetTouchPointCount() > 0) {
            pointerScreenPos = GetTouchPosition(0);
            isPointerActive = true;
        } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            pointerScreenPos = GetMousePosition();
            isPointerActive = true;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 clickPos = isPointerActive ? pointerScreenPos : GetMousePosition();
            if (clickPos.y >= floorTopY) {
                touchStartedInBottomBar = true;
                if (CheckCollisionPointCircle(clickPos, btnSoundCenter, btnPressRadius)) {
                    btnSelect = true;
                } else if (CheckCollisionPointCircle(clickPos, btnRestartCenter, btnPressRadius)) {
                    btnRestart = true;
                } else {
                    // Tapped on the bottom bar outside buttons:
                    // Drop fruit in the SAME position as previous one (without moving heldBall)
                    btnDrop = true;
                    dropRipplePos = clickPos;
                    dropRippleTimer = 0.2f;
                }
            } else {
                touchStartedInBottomBar = false;
            }
        }

        // Dragging in upper play area aims the fruit
        if (isPointerActive && !touchStartedInBottomBar) {
            if (pointerScreenPos.y < floorTopY) {
                hasDirectPointer = true;
                pointerWorldX = pointerScreenPos.x / PPU;
            }
        }

        // Releasing touch or mouse drops the fruit where aimed
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (!touchStartedInBottomBar) {
                Vector2 releasePos = GetMousePosition();
                if (releasePos.y < floorTopY) {
                    btnDrop = true;
                    hasDirectPointer = true;
                    pointerWorldX = releasePos.x / PPU;
                }
            }
            touchStartedInBottomBar = false;
        }
#endif

        if (btnSelect && !SelectPressed) {
            SelectPressed = true;
            if (currentSoundMode == SOUND_LOUD) {
                currentSoundMode = SOUND_QUIET;
            } else if (currentSoundMode == SOUND_QUIET) {
                currentSoundMode = SOUND_OFF;
            } else {
                currentSoundMode = SOUND_LOUD;
            }
#ifdef PLATFORM_PSP
            int volInt = (currentSoundMode == SOUND_LOUD) ? 128 : (currentSoundMode == SOUND_QUIET ? 48 : 0);
            Mix_VolumeMusic(volInt);
            Mix_Volume(-1, volInt);
#else
            float volFloat = (currentSoundMode == SOUND_LOUD) ? 1.0f : (currentSoundMode == SOUND_QUIET ? 0.375f : 0.0f);
            SetMusicVolume(firstDropMusic, volFloat);
            SetMusicVolume(smallFruitsMusic, volFloat);
            SetSoundVolume(popSfx, volFloat);
            SetSoundVolume(whooshSfx, volFloat);
#endif
        } else if (!btnSelect) {
            SelectPressed = false;
        }

        if (GameOver) {
            if (btnRestart || btnDrop) {
                timeSinceLastBall = 0.5f;
                BallsN = 0;
                initBall(&heldBall);
                initBall(&nextBall);
                for (int i = 0; i < BallsLimit; i++) {
                    initBall(&balls[i]);
                }
                Score = 0;
                DispScore = 0;
                GameOver = false;
            }
        } else {
            if (btnRestart) {
                timeSinceLastBall = 0.5f;
                BallsN = 0;
                initBall(&heldBall);
                initBall(&nextBall);
                for (int i = 0; i < BallsLimit; i++) {
                    initBall(&balls[i]);
                }
                Score = 0;
                DispScore = 0;
                GameOver = false;
            }

            heldBall.y = BoxTopMargin;

            if (timeSinceLastBall < 0.5f) {
                timeSinceLastBall += DeltaTime * TicksPerFrame;
            } else {
                timeSinceLastBall = 0.5f;
            }

            // Always update horizontal position first
            if (hasDirectPointer) {
                heldBall.x = pointerWorldX;
            } else if (btnLeft && !btnRight) {
                heldBall.x -= analogPercent * 5 * DeltaTime * TicksPerFrame;
            } else if (!btnLeft && btnRight) {
                heldBall.x += analogPercent * 5 * DeltaTime * TicksPerFrame;
            }

            resolveLeftWallColl(&heldBall, false);
            resolveRightWallColl(&heldBall, false);

            if (timeSinceLastBall >= 0.5f && btnDrop) {
#ifdef PLATFORM_PSP
                Mix_PlayChannel(-1, whooshSfx, 0);
#else
                PlaySound(whooshSfx);
#endif
                timeSinceLastBall = 0;
                balls[BallsN] = heldBall;
                balls[BallsN].x += rngf(-0.01f, 0.01f);
                BallsN++;
                nextBall.x = heldBall.x;
                nextBall.y = heldBall.y;
                heldBall = nextBall;
                randomizeRadius(&nextBall);
            }

            if (DispScore < Score) {
                DispScore++;
            }
            if (Score > highScore) {
                highScore = Score;
                SaveHighScore(highScore);
            }

            resolveLeftWallColl(&heldBall, false);
            resolveRightWallColl(&heldBall, false);

            for (int tick = 0; tick < TicksPerFrame; tick++) {
                for (int i = 0; i < BallsN; i++) {
                    struct Ball * b = &(balls[i]);

                    struct Vector2D f = {0, 0};
                    addGravToForce(&f, b);

                    struct Vector2D a = calcAcc(f, b);
                    addAccToVel(a, b);

                    applyAirResistance(b);
                    moveBall(b);

                    resolveFloorColl(b);
                    resolveLeftWallColl(b, true);
                    resolveRightWallColl(b, true);

                    if (b->y + b->r < BoxFloorY - BoxTopLineHeight) {
                        b->timeOutside += DeltaTime;
                    } else {
                        b->timeOutside = 0;
                    }

                    if (b->timeOutside >= 3) {
                        GameOver = true;
                        SaveHighScore(Score);
                    }
                }

                for (int i = 0; i < BallsN; i++) {
                    for (int j = i+1; j < BallsN; j++) {
                        if (tryCombiningBalls(balls, i, j, &BallsN)) {
#ifdef PLATFORM_PSP
                            Mix_PlayChannel(-1, popSfx, 0);
#else
                            PlaySound(popSfx);
#endif
                        }
                    }
                }
                
                // It can improve physics at a cost of performance
                sortBalls(balls, BallsN);
            
                for (int pass = 0; pass < CollPasses; pass++) {
                    for (int i = 0; i < BallsN; i++) {
                        struct Ball * b1 = &(balls[i]);
                        for (int j = i+1; j < BallsN; j++) {
                            struct Ball * b2 = &(balls[j]);

                            resolveBallColl(b1, b2);

                            resolveFloorColl(b1);
                            resolveLeftWallColl(b1, true);
                            resolveRightWallColl(b1, true);
                            resolveFloorColl(b2);
                            resolveLeftWallColl(b2, true);
                            resolveRightWallColl(b2, true);
                        }
                    }
                }
            }
        }

        BeginDrawing();
        {
            ClearBackground(BackgroundColor);

            float floorTopY = BoxFloorY * PPU;
            int floorTopHeight = (int)(8.0f * (float)scale);
            DrawRectangle(0, (int)floorTopY, screenWidth, floorTopHeight, FloorTopColor);
            DrawRectangle(0, (int)floorTopY + floorTopHeight, screenWidth, screenHeight - (int)floorTopY - floorTopHeight, FloorBottomColor);

            drawBox();

            if (isPortrait) {
                float nextLabelX = (float)screenWidth - 45.0f * (float)scale;
                DrawTextEx(gameFont, "Next", (Vector2){ nextLabelX, 8.0f * (float)scale }, fontSize, 2, OutlineColor);
                nextBall.x = ((float)screenWidth - 27.5f * (float)scale) / PPU;
                nextBall.y = (BoxTopMargin - 1.1f) + nextBall.r;
            } else if (screenWidth == 640) {
                DrawTextEx(gameFont, "Next", (Vector2){ (BoxLeftMargin + BoxWidth + 0.35f) * PPU, 24.0f }, fontSize, 2, OutlineColor);
                nextBall.x = BoxLeftMargin + BoxWidth + 1.25f;
                nextBall.y = BoxTopMargin + 0.4f + nextBall.r;
            } else {
                DrawTextEx(gameFont, "Next", (Vector2){ 393.0f, 16.0f }, fontSize, 2, OutlineColor);
                nextBall.x = BoxLeftMargin + BoxWidth + 2.51f;
                nextBall.y = BoxTopMargin + 0.4f + nextBall.r;
            }
            drawBall(&nextBall, fruitsTexture);

            if (timeSinceLastBall >= 0.5f) {
#ifndef PLATFORM_PSP
                if (isPointerActive && !GameOver) {
                    float guideX = heldBall.x * PPU;
                    float startY = (heldBall.y + heldBall.r) * PPU;
                    float endY = BoxFloorY * PPU;
                    for (float gy = startY; gy < endY; gy += 12.0f) {
                        float segLen = (gy + 6.0f < endY) ? 6.0f : (endY - gy);
                        DrawLineEx((Vector2){ guideX, gy }, (Vector2){ guideX, gy + segLen }, 2.0f, (Color){ 180, 180, 180, 120 });
                    }
                }
#endif
                drawBall(&heldBall, fruitsTexture);
            }

            for (int i = 0; i < BallsN; i++) {
                struct Ball * b = &(balls[i]);
                drawBall(b, fruitsTexture);
            }

            if (isPortrait) {
                DrawTextEx(gameFont, TextFormat("Score: %08i", DispScore), (Vector2){ 8.0f * (float)scale, 8.0f * (float)scale }, fontSize, 2, OutlineColor);
                DrawTextEx(gameFont, TextFormat("Best:  %08i", highScore), (Vector2){ 8.0f * (float)scale, 26.0f * (float)scale }, fontSize, 2, BROWN);
            } else if (screenWidth == 640) {
                DrawTextEx(gameFont, TextFormat("Score:\n %08i", DispScore), (Vector2){ 16.0f, 24.0f }, fontSize, 2, OutlineColor);
                DrawTextEx(gameFont, TextFormat("\n\nBest:\n %08i", highScore), (Vector2){ 16.0f, 24.0f }, fontSize, 2, BROWN);
            } else {
                DrawTextEx(gameFont, TextFormat("Score:\n %08i", DispScore), (Vector2){ 16.0f, 16.0f }, fontSize, 2, OutlineColor);
                DrawTextEx(gameFont, TextFormat("\n\nBest:\n %08i", highScore), (Vector2){ 16.0f, 16.0f }, fontSize, 2, BROWN);
            }

            if (GameOver) {
                Vector2 goPos = isPortrait ? (Vector2){ (float)screenWidth * 0.15f, (float)screenHeight * 0.42f }
                                           : (screenWidth == 640 ? (Vector2){ 16.0f, 130.0f } : (Vector2){ 16.0f, 16.0f });
                DrawTextEx(gameFont, isPortrait ? "Game Over\nTap to restart" : "\n\n\n\n\nGame Over\nPress START\n(or Enter/Tap)\nto restart", goPos, fontSize, 2, RED);
            }

#ifndef PLATFORM_PSP
            // On-screen touch buttons (Sound and Restart)
            // Text-only with slightly bigger text and a press radius
            bool soundPressed = isPointerActive && CheckCollisionPointCircle(pointerScreenPos, btnSoundCenter, btnPressRadius);
            bool restartPressed = isPointerActive && CheckCollisionPointCircle(pointerScreenPos, btnRestartCenter, btnPressRadius);

            if (soundPressed) {
                DrawCircleV(btnSoundCenter, btnPressRadius, (Color){ 255, 255, 255, 45 });
            }
            const char* soundText = (currentSoundMode == SOUND_LOUD) ? "Loud" : ((currentSoundMode == SOUND_QUIET) ? "Quiet" : "Off");
            Vector2 soundTextSize = MeasureTextEx(gameFont, soundText, fontSize, 1.0f);
            DrawTextEx(gameFont, soundText, (Vector2){ btnSoundCenter.x - soundTextSize.x * 0.5f, btnSoundCenter.y - soundTextSize.y * 0.5f }, fontSize, 1.0f, soundPressed ? WHITE : OutlineColor);

            if (restartPressed) {
                DrawCircleV(btnRestartCenter, btnPressRadius, (Color){ 255, 255, 255, 45 });
            }
            Vector2 restartTextSize = MeasureTextEx(gameFont, "Restart", fontSize, 1.0f);
            DrawTextEx(gameFont, "Restart", (Vector2){ btnRestartCenter.x - restartTextSize.x * 0.5f, btnRestartCenter.y - restartTextSize.y * 0.5f }, fontSize, 1.0f, restartPressed ? WHITE : OutlineColor);

            if (dropRippleTimer > 0.0f) {
                float rippleRadius = (0.2f - dropRippleTimer) * 120.0f + 8.0f;
                unsigned char alpha = (unsigned char)((dropRippleTimer / 0.2f) * 70.0f);
                DrawCircleV(dropRipplePos, rippleRadius, (Color){ 255, 255, 255, alpha });
            }
#endif
        }
        EndDrawing();
    }

    SaveHighScore(Score);

#ifdef PLATFORM_PSP
    Mix_HaltMusic();
    Mix_HaltChannel(-1);

    Mix_FreeChunk(popSfx);
    Mix_FreeChunk(whooshSfx);
    Mix_FreeMusic(firstDropMusic);
    Mix_FreeMusic(smallFruitsMusic);

    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
#else
    StopMusicStream(firstDropMusic);
    StopMusicStream(smallFruitsMusic);
    UnloadMusicStream(firstDropMusic);
    UnloadMusicStream(smallFruitsMusic);
    UnloadSound(popSfx);
    UnloadSound(whooshSfx);
    CloseAudioDevice();
#endif

    UnloadTexture(fruitsTexture);
    UnloadFont(gameFont);
    CloseWindow();

    return 0;
}
