// City Drive DS - لعبة سيارات بمنظور علوي (أسلوب GTA 2) - مشروع أصلي
// libnds / devkitPro. كل الرسومات تُولَّد بالكود، لا حاجة لملفات خارجية.
//
// التحكم:
//   اتجاهات يمين/يسار = توجيه | فوق أو A = تسارع | تحت أو B = فرامل/رجوع
//   Y = النزول من السيارة / ركوبها (عند الاقتراب منها)
//   الهدف: اوصل إلى العلامة الصفراء بالسيارة لتكسب 100 نقطة

#include <nds.h>
#include <string.h>
#include <stdbool.h>

// إذا دارت السيارة عكس اتجاه التوجيه، غيّر -1 إلى 1
#define ROT_SIGN (-1)

#define WORLD   512
#define TILES   64

enum { T_GRASS, T_ROAD, T_VLR, T_VLL, T_HLB, T_HLT, T_SIDE, T_BUILD, T_COUNT };

static u8  tileType[TILES][TILES];
static u8  tileGfx[T_COUNT][64] __attribute__((aligned(4)));
static u16 mapRam[TILES * TILES] __attribute__((aligned(4)));
static u8  miniStatic[128 * 128];
static u8  backbuf[256 * 192] __attribute__((aligned(4)));

static u8 carGfx[256]   __attribute__((aligned(4)));
static u8 footGfx[64]   __attribute__((aligned(4)));
static u8 markerGfx[64] __attribute__((aligned(4)));

static const u8 font[10][5] = {
    {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
    {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}
};

// نقاط المهمة: تقاطعات الطرق
static const int targets[][2] = {
    {192, 64}, {320, 192}, {448, 320}, {192, 448},
    {64, 320}, {320, 448}, {448, 64}, {192, 192}
};
#define NUM_TARGETS (int)(sizeof(targets) / sizeof(targets[0]))

// ---------------------------------------------------------------- بناء العالم

static void setMapEntry(int tx, int ty, u16 v) {
    int blk = (tx >> 5) + ((ty >> 5) << 1);
    mapRam[blk * 1024 + (ty & 31) * 32 + (tx & 31)] = v;
}

static void buildWorld(void) {
    for (int ty = 0; ty < TILES; ty++) {
        for (int tx = 0; tx < TILES; tx++) {
            int rx = tx & 15, ry = ty & 15;
            int vroad = (rx == 7 || rx == 8);
            int hroad = (ry == 7 || ry == 8);
            int t;
            if (vroad && hroad)       t = T_ROAD;
            else if (vroad)           t = (rx == 7) ? T_VLR : T_VLL;
            else if (hroad)           t = (ry == 7) ? T_HLB : T_HLT;
            else if (rx == 6 || rx == 9 || ry == 6 || ry == 9) t = T_SIDE;
            else {
                int bx = (tx + 7) >> 4, by = (ty + 7) >> 4;
                t = (((bx + by) % 3) == 0) ? T_GRASS : T_BUILD;
            }
            tileType[ty][tx] = (u8)t;
            setMapEntry(tx, ty, (u16)t);
        }
    }
}

static void buildTiles(void) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int i = y * 8 + x;
            tileGfx[T_GRASS][i] = (((x * 7 + y * 13) % 5) == 0) ? 2 : 1;
            tileGfx[T_ROAD][i]  = 3;
            tileGfx[T_VLR][i]   = (x == 7 && y < 5) ? 4 : 3;
            tileGfx[T_VLL][i]   = (x == 0 && y < 5) ? 4 : 3;
            tileGfx[T_HLB][i]   = (y == 7 && x < 5) ? 4 : 3;
            tileGfx[T_HLT][i]   = (y == 0 && x < 5) ? 4 : 3;
            tileGfx[T_SIDE][i]  = 5;
            if (x == 0 || y == 0 || x == 7 || y == 7) tileGfx[T_BUILD][i] = 7;
            else tileGfx[T_BUILD][i] = ((x % 4) == 2 && (y % 4) == 2) ? 8 : 6;
        }
    }
}

static void buildSprites(void) {
    memset(carGfx, 0, sizeof(carGfx));
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            u8 c = 0;
            if (x >= 4 && x <= 11 && y >= 1 && y <= 14) {
                c = 1;
                if ((y == 1 || y == 14) && (x == 4 || x == 11)) c = 0;
                if ((y == 4 || y == 5) && x >= 5 && x <= 10) c = 2;
                if (y == 11 && x >= 5 && x <= 10) c = 2;
                if (y == 1 && (x == 5 || x == 10)) c = 4;
            }
            if ((x == 3 || x == 12) && ((y >= 3 && y <= 5) || (y >= 10 && y <= 12))) c = 3;
            carGfx[y * 16 + x] = c;
        }
    }
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int r2 = (2 * x - 7) * (2 * x - 7) + (2 * y - 7) * (2 * y - 7);
            u8 f = 0, m = 0;
            if (r2 <= 49) f = 5;
            if (r2 <= 10) f = 6;
            if (r2 <= 49 && r2 >= 16) m = 7;
            footGfx[y * 8 + x] = f;
            markerGfx[y * 8 + x] = m;
        }
    }
}

static void buildMinimap(void) {
    for (int ty = 0; ty < TILES; ty++) {
        for (int tx = 0; tx < TILES; tx++) {
            int t = tileType[ty][tx];
            u8 c = 1;
            if (t >= T_ROAD && t <= T_HLT) c = 2;
            else if (t == T_SIDE) c = 3;
            else if (t == T_BUILD) c = 4;
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++)
                    miniStatic[(ty * 2 + j) * 128 + tx * 2 + i] = c;
        }
    }
}

static int solidAt(int px, int py) {
    if (px < 0 || py < 0 || px >= WORLD || py >= WORLD) return 1;
    return tileType[py >> 3][px >> 3] == T_BUILD;
}

static int onGrass(int px, int py) {
    if (px < 0 || py < 0 || px >= WORLD || py >= WORLD) return 0;
    return tileType[py >> 3][px >> 3] == T_GRASS;
}

// ---------------------------------------------------------------- رسم الشاشة السفلية

static void putPix(int x, int y, u8 c) {
    if (x < 0 || y < 0 || x >= 256 || y >= 192) return;
    backbuf[y * 256 + x] = c;
}

static void dot(int x, int y, u8 c) {
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
            putPix(x + i, y + j, c);
}

static void drawDigit(int d, int x, int y, u8 c) {
    for (int r = 0; r < 5; r++)
        for (int b = 0; b < 3; b++)
            if (font[d][r] & (4 >> b))
                for (int j = 0; j < 4; j++)
                    for (int i = 0; i < 4; i++)
                        putPix(x + b * 4 + i, y + r * 4 + j, c);
}

static void drawNumber(int n, int x, int y, u8 c) {
    int digits[10], k = 0;
    if (n == 0) digits[k++] = 0;
    while (n > 0 && k < 10) { digits[k++] = n % 10; n /= 10; }
    for (int i = 0; i < k; i++) drawDigit(digits[k - 1 - i], x + i * 16, y, c);
}

// ---------------------------------------------------------------- البرنامج الرئيسي

int main(void) {
    powerOn(POWER_ALL_2D);
    lcdMainOnTop();

    // الشاشة العليا: اللعبة
    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_SPR_ACTIVE);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);
    // الشاشة السفلى: الخريطة المصغرة
    videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
    vramSetBankC(VRAM_C_SUB_BG);

    buildTiles();
    buildWorld();
    buildSprites();
    buildMinimap();

    // ألوان الخلفية
    BG_PALETTE[0] = RGB15(0, 0, 0);
    BG_PALETTE[1] = RGB15(6, 18, 6);
    BG_PALETTE[2] = RGB15(4, 14, 4);
    BG_PALETTE[3] = RGB15(9, 9, 10);
    BG_PALETTE[4] = RGB15(28, 28, 20);
    BG_PALETTE[5] = RGB15(20, 20, 19);
    BG_PALETTE[6] = RGB15(14, 10, 9);
    BG_PALETTE[7] = RGB15(9, 6, 6);
    BG_PALETTE[8] = RGB15(20, 16, 12);

    // ألوان السبرايت
    SPRITE_PALETTE[0] = RGB15(0, 0, 0);
    SPRITE_PALETTE[1] = RGB15(28, 3, 3);
    SPRITE_PALETTE[2] = RGB15(6, 10, 20);
    SPRITE_PALETTE[3] = RGB15(2, 2, 2);
    SPRITE_PALETTE[4] = RGB15(31, 30, 10);
    SPRITE_PALETTE[5] = RGB15(5, 12, 28);
    SPRITE_PALETTE[6] = RGB15(28, 22, 16);
    SPRITE_PALETTE[7] = RGB15(31, 24, 0);

    // ألوان الخريطة المصغرة
    BG_PALETTE_SUB[0] = RGB15(1, 1, 4);
    BG_PALETTE_SUB[1] = RGB15(5, 16, 5);
    BG_PALETTE_SUB[2] = RGB15(10, 10, 11);
    BG_PALETTE_SUB[3] = RGB15(18, 18, 17);
    BG_PALETTE_SUB[4] = RGB15(10, 6, 6);
    BG_PALETTE_SUB[5] = RGB15(31, 4, 4);
    BG_PALETTE_SUB[6] = RGB15(31, 28, 0);
    BG_PALETTE_SUB[7] = RGB15(31, 31, 31);

    // خلفية المدينة
    int bg = bgInit(0, BgType_Text8bpp, BgSize_T_512x512, 0, 1);
    DC_FlushRange(tileGfx, sizeof(tileGfx));
    DC_FlushRange(mapRam, sizeof(mapRam));
    dmaCopy(tileGfx, bgGetGfxPtr(bg), sizeof(tileGfx));
    dmaCopy(mapRam, bgGetMapPtr(bg), sizeof(mapRam));

    // الخلفية السفلية (bitmap)
    int sbg = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    u16 *subGfx = bgGetGfxPtr(sbg);

    // السبرايت
    oamInit(&oamMain, SpriteMapping_1D_32, false);
    u16 *carPtr    = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    u16 *footPtr   = oamAllocateGfx(&oamMain, SpriteSize_8x8,   SpriteColorFormat_256Color);
    u16 *markerPtr = oamAllocateGfx(&oamMain, SpriteSize_8x8,   SpriteColorFormat_256Color);
    DC_FlushRange(carGfx, sizeof(carGfx));
    DC_FlushRange(footGfx, sizeof(footGfx));
    DC_FlushRange(markerGfx, sizeof(markerGfx));
    dmaCopy(carGfx, carPtr, sizeof(carGfx));
    dmaCopy(footGfx, footPtr, sizeof(footGfx));
    dmaCopy(markerGfx, markerPtr, sizeof(markerGfx));

    // حالة اللعبة
    int carX = 64 << 8, carY = 120 << 8;   // 24.8 ثابتة
    int angle = 0;                          // 0..511، 0 = للأعلى
    int speed = 0;                          // 1/256 بكسل لكل إطار
    int footX = 64, footY = 120;
    int inCar = 1;
    int score = 0, target = 0;
    int frame = 0;

    while (1) {
        scanKeys();
        int held = keysHeld();
        int down = keysDown();
        frame++;

        s16 a16 = (s16)(angle << 6);
        int s = sinLerp(a16);
        int c = cosLerp(a16);

        if (inCar) {
            // تسارع وفرامل
            int cap = onGrass(carX >> 8, carY >> 8) ? 350 : 768;
            if (held & (KEY_UP | KEY_A)) speed += 22;
            else if (held & (KEY_DOWN | KEY_B)) speed -= (speed > 0) ? 40 : 18;
            else speed -= speed / 28;
            if (speed > cap) speed -= 20;
            if (speed < -300) speed = -300;
            if (speed > -8 && speed < 8 && !(held & (KEY_UP | KEY_A | KEY_DOWN | KEY_B))) speed = 0;

            // توجيه
            if (speed > 40 || speed < -40) {
                int turn = 0;
                if (held & KEY_LEFT)  turn = -3;
                if (held & KEY_RIGHT) turn = 3;
                if (speed < 0) turn = -turn;
                angle = (angle + turn) & 511;
                a16 = (s16)(angle << 6);
                s = sinLerp(a16);
                c = cosLerp(a16);
            }

            // حركة مع تصادم
            int dx = (s * speed) >> 12;
            int dy = -((c * speed) >> 12);
            int nx = carX + dx, ny = carY + dy;
            int dir = (speed >= 0) ? 1 : -1;
            int noseX = (nx >> 8) + dir * ((s * 7) >> 12);
            int noseY = (ny >> 8) - dir * ((c * 7) >> 12);
            if (solidAt(nx >> 8, ny >> 8) || solidAt(noseX, noseY)) {
                speed = -speed / 3;
            } else {
                carX = nx;
                carY = ny;
            }

            // النزول من السيارة
            if ((down & KEY_Y) && speed < 64 && speed > -64) {
                int fx = (carX >> 8) + ((c * 12) >> 12);
                int fy = (carY >> 8) + ((s * 12) >> 12);
                if (solidAt(fx, fy)) { fx = carX >> 8; fy = carY >> 8; }
                footX = fx; footY = fy;
                inCar = 0;
                speed = 0;
            }
        } else {
            int mx = ((held & KEY_RIGHT) ? 1 : 0) - ((held & KEY_LEFT) ? 1 : 0);
            int my = ((held & KEY_DOWN) ? 1 : 0) - ((held & KEY_UP) ? 1 : 0);
            if (!solidAt(footX + mx * 2, footY)) footX += mx * 2;
            if (!solidAt(footX, footY + my * 2)) footY += my * 2;

            int ddx = footX - (carX >> 8), ddy = footY - (carY >> 8);
            if ((down & KEY_Y) && ddx * ddx + ddy * ddy < 24 * 24) inCar = 1;
        }

        // المهمة
        int px = inCar ? (carX >> 8) : footX;
        int py = inCar ? (carY >> 8) : footY;
        {
            int tdx = px - targets[target][0], tdy = py - targets[target][1];
            if (inCar && tdx * tdx + tdy * tdy < 14 * 14) {
                score += 100;
                target = (target + 1) % NUM_TARGETS;
            }
        }

        // الكاميرا
        int camX = px - 128, camY = py - 96;
        if (camX < 0) camX = 0;
        if (camX > WORLD - 256) camX = WORLD - 256;
        if (camY < 0) camY = 0;
        if (camY > WORLD - 192) camY = WORLD - 192;
        bgSetScroll(bg, camX, camY);

        // السبرايت
        int csx = (carX >> 8) - camX, csy = (carY >> 8) - camY;
        oamRotateScale(&oamMain, 0, ROT_SIGN * (angle << 6), 256, 256);
        oamSet(&oamMain, 0, csx - 16, csy - 16, 0, 0,
               SpriteSize_16x16, SpriteColorFormat_256Color, carPtr,
               0, true, (csx < -24 || csx > 280 || csy < -24 || csy > 216),
               false, false, false);

        oamSet(&oamMain, 1, footX - camX - 4, footY - camY - 4, 0, 0,
               SpriteSize_8x8, SpriteColorFormat_256Color, footPtr,
               -1, false, inCar, false, false, false);

        int msx = targets[target][0] - camX, msy = targets[target][1] - camY;
        int blinkHide = ((frame >> 4) & 1) || msx < -8 || msx > 264 || msy < -8 || msy > 200;
        oamSet(&oamMain, 2, msx - 4, msy - 4, 0, 0,
               SpriteSize_8x8, SpriteColorFormat_256Color, markerPtr,
               -1, false, blinkHide, false, false, false);

        // الشاشة السفلية
        memset(backbuf, 0, sizeof(backbuf));
        for (int y = 0; y < 128; y++)
            memcpy(&backbuf[(32 + y) * 256 + 8], &miniStatic[y * 128], 128);
        if ((frame >> 4) & 1)
            dot(8 + targets[target][0] / 4, 32 + targets[target][1] / 4, 6);
        dot(8 + px / 4, 32 + py / 4, 5);
        drawNumber(score, 150, 40, 7);
        DC_FlushRange(backbuf, sizeof(backbuf));

        swiWaitForVBlank();
        bgUpdate();
        oamUpdate(&oamMain);
        dmaCopy(backbuf, subGfx, sizeof(backbuf));
    }
    return 0;
}
