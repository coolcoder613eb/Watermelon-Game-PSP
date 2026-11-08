#ifdef PLATFORM_PSP
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <pspctrl.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <raylib.h>
#include <physics.h>
#include <graphics.h>
#include <constants.h> 


void drawBall(struct Ball * b, Texture2D fruitsTexture){
    float rInPixels = b->r * PPU;
    float spriteScale = PPU / 30.0f;
    float destWH = (float)FruitTextureWH[b->typeIdx] * spriteScale;
    Rectangle source = {
        (float)FruitTextureOffsetX[b->typeIdx] * spriteScale,
        (float)FruitTextureOffsetY[b->typeIdx] * spriteScale,
        destWH, destWH
    };
    Rectangle dest = { roundf(b->x * PPU), roundf(b->y * PPU), destWH, destWH };
    Vector2 origin = { roundf(rInPixels + 1.0f * spriteScale), roundf(rInPixels + 1.0f * spriteScale) };

    float rotDeg = radToDeg(b->rot);
    if (fabsf(rotDeg) < 0.05f) rotDeg = 0.0f;

    if (b->y + b->r < BoxFloorY - BoxTopLineHeight && b->timeOutside >= 1) {
        float ballTintPercent = fabsf(sinf(M_PI * b->timeOutside));
        Color ballTint = RED;
        if (b->timeOutside < 3) {
            ballTint.r = (unsigned char)(WHITE.r * (1.0f - ballTintPercent) + RED.r * (ballTintPercent));
            ballTint.g = (unsigned char)(WHITE.g * (1.0f - ballTintPercent) + RED.g * (ballTintPercent));
            ballTint.b = (unsigned char)(WHITE.b * (1.0f - ballTintPercent) + RED.b * (ballTintPercent));
        }
        DrawTexturePro(fruitsTexture, source, dest, origin, rotDeg, ballTint);
    } else {
        DrawTexturePro(fruitsTexture, source, dest, origin, rotDeg, WHITE);
    }
}


void drawBox() {
    float ppu = PPU;
    float thick = fmaxf(1.0f, roundf(ppu / 30.0f));

    float boxLeftPx = BoxLeftMargin * ppu;
    float boxWidthPx = BoxWidth * ppu;
    float boxTopLinePx = (BoxFloorY - BoxTopLineHeight) * ppu;
    float boxFloorPx = BoxFloorY * ppu;
    float boxTopMarginPx = (BoxTopMargin + BoxHeight) * ppu;
    float diagHeightPx = BoxDiagonalLineHeight * ppu;

    DrawRectangle(boxLeftPx, boxTopLinePx - diagHeightPx, boxWidthPx, (BoxTopLineHeight + BoxDiagonalLineHeight) * ppu, BoxColor);

    Vector2 v1 = { boxLeftPx + diagHeightPx, boxTopLinePx - diagHeightPx };
    Vector2 v2 = { boxLeftPx, boxTopLinePx - diagHeightPx };
    Vector2 v3 = { boxLeftPx, boxTopLinePx };
    DrawTriangle(v1, v2, v3, BackgroundColor);

    Vector2 v4 = { boxLeftPx + boxWidthPx, boxTopLinePx };
    Vector2 v5 = { boxLeftPx + boxWidthPx, boxTopLinePx - diagHeightPx };
    Vector2 v6 = { boxLeftPx + boxWidthPx - diagHeightPx, boxTopLinePx - diagHeightPx };
    DrawTriangle(v4, v5, v6, BackgroundColor);

    // Warning limit line (Red)
    DrawLineEx((Vector2){ boxLeftPx, boxTopLinePx },
               (Vector2){ boxLeftPx + boxWidthPx, boxTopLinePx },
               thick, RED);

    // Floor line
    DrawLineEx((Vector2){ boxLeftPx, boxFloorPx },
               (Vector2){ boxLeftPx + boxWidthPx, boxFloorPx },
               thick, OutlineColor);

    // Left and Right vertical walls
    DrawLineEx((Vector2){ boxLeftPx, boxTopLinePx },
               (Vector2){ boxLeftPx, boxTopMarginPx },
               thick, OutlineColor);
    DrawLineEx((Vector2){ boxLeftPx + boxWidthPx, boxTopLinePx },
               (Vector2){ boxLeftPx + boxWidthPx, boxTopMarginPx },
               thick, OutlineColor);

    // Diagonal funnel corners
    DrawLineEx((Vector2){ boxLeftPx, boxTopLinePx },
               (Vector2){ boxLeftPx + diagHeightPx, boxTopLinePx - diagHeightPx },
               thick, OutlineColor);
    DrawLineEx((Vector2){ boxLeftPx + boxWidthPx, boxTopLinePx },
               (Vector2){ boxLeftPx + boxWidthPx - diagHeightPx, boxTopLinePx - diagHeightPx },
               thick, OutlineColor);
    DrawLineEx((Vector2){ boxLeftPx + diagHeightPx, boxTopLinePx - diagHeightPx },
               (Vector2){ boxLeftPx + boxWidthPx - diagHeightPx, boxTopLinePx - diagHeightPx },
               thick, OutlineColor);
}
