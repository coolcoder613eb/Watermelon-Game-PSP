#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <raylib.h>
#include <stdbool.h>

#define ATTR_PSP_WIDTH 480
#define ATTR_PSP_HEIGHT 272

extern int screenWidth;
extern int screenHeight;
extern float PixelsPerUnit;
#define PPU PixelsPerUnit
extern float BoxHeight;
extern float BoxTopLineHeight;
extern float BoxDiagonalLineHeight;
extern float BoxWidth;
extern float BoxLeftMargin;
extern float BoxTopMargin;
extern float BoxFloorY;
extern float Gravity;
extern bool isPortrait;

static const float TextLeftMargin = 2.0;

static const Color BackgroundColor = {208,196,156,255};
static const Color BoxColor = {235,223,177,192};
static const Color FloorBottomColor = {146,106,82,255};
static const Color FloorTopColor = {175,131,105,255};
static const Color OutlineColor = {75,35,8,255};

static const float AnalogDeadzonePercent = 0.15;

static const float TimeToFall = 1.0f;
static const float AirResistance = 4.25f;
static const float PenetrationCorrectionPercent = 0.75f;
static const float MergePushForce = 5.0f;

static const int BallsLimit = 512;
static const int CollPasses = 4;
static const int TicksPerFrame = 4;
static const float InvTicksPerFrame = 1.0f / ((float) TicksPerFrame);

static const float BallRadius [] = {0.2f,0.25f,0.45f,0.5f,0.6f,0.75f,0.85f,1.05f,1.15f,1.45,1.7};
static const Color BallColors [] = {DARKBLUE,RED,LIME,GOLD,ORANGE,MAROON,PINK,BROWN,GREEN,YELLOW,DARKGREEN};
static const int BallTypesN = sizeof(BallRadius) / sizeof(BallRadius[0]);
static const int BallScores [] = {1,3,7,9,13,21,27,34,44,62,90};

static const int FruitTextureOffsetX [] = {0,0,0,0,0,128,128,256,256,384,384};
static const int FruitTextureOffsetY [] = {0,32,64,128,192,0,128,0,128,0,128};
static const int FruitTextureWH [] = {16,32,32,32,64,64,64,128,128,128,128};

extern float DeltaTime;
extern int Score;
extern int DispScore;
extern bool GameOver;

// #define DeltaTime (((float) GetFrameTime()) / TicksPerFrame)
// static const float DeltaTime = (0.0333f / TicksPerFrame);

#endif
