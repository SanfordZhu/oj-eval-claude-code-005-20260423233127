#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    // Write QOI header
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    const int pixel_count = width * height;
    uint8_t color_table[64][4];
    memset(color_table, 0, sizeof(color_table));

    uint8_t px_r, px_g, px_b, px_a;
    uint8_t prev_r = 0, prev_g = 0, prev_b = 0, prev_a = 255;
    int run_length = 0;

    // Add the initial previous pixel to color table
    int init_index = QoiColorHash(prev_r, prev_g, prev_b, prev_a);
    color_table[init_index][0] = prev_r;
    color_table[init_index][1] = prev_g;
    color_table[init_index][2] = prev_b;
    color_table[init_index][3] = prev_a;

    for (int i = 0; i < pixel_count; ++i) {
        px_r = QoiReadU8();
        px_g = QoiReadU8();
        px_b = QoiReadU8();
        if (channels == 4) {
            px_a = QoiReadU8();
        } else {
            px_a = 255;
        }

        int hash_index = QoiColorHash(px_r, px_g, px_b, px_a);

        // Priority 1: QOI_OP_RUN - same pixel as previous
        if (px_r == prev_r && px_g == prev_g && px_b == prev_b && px_a == prev_a) {
            run_length++;
            if (run_length == 62) {
                QoiWriteU8(QOI_OP_RUN_TAG | (run_length - 1));
                run_length = 0;
            }
        } else {
            // End any existing run
            if (run_length > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG | (run_length - 1));
                run_length = 0;
            }

            // Priority 2: QOI_OP_INDEX - pixel in color table
            if (color_table[hash_index][0] == px_r &&
                color_table[hash_index][1] == px_g &&
                color_table[hash_index][2] == px_b &&
                color_table[hash_index][3] == px_a) {
                QoiWriteU8(QOI_OP_INDEX_TAG | hash_index);
            } else {
                // Not in color table, need to encode with difference
                int dr = (int)px_r - (int)prev_r;
                int dg = (int)px_g - (int)prev_g;
                int db = (int)px_b - (int)prev_b;

                // Priority 3: QOI_OP_DIFF - small differences, alpha same
                if (px_a == prev_a && dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                    uint8_t code = QOI_OP_DIFF_TAG;
                    code |= ((dr + 2) & 0x03) << 4;
                    code |= ((dg + 2) & 0x03) << 2;
                    code |= ((db + 2) & 0x03);
                    QoiWriteU8(code);
                }
                // Priority 4: QOI_OP_LUMA - luma difference, alpha same
                else if (px_a == prev_a) {
                    int dr_dg = dr - dg;
                    int db_dg = db - dg;
                    if (dg >= -32 && dg <= 31 && dr_dg >= -8 && dr_dg <= 7 && db_dg >= -8 && db_dg <= 7) {
                        uint8_t code1 = QOI_OP_LUMA_TAG | (dg + 32);
                        uint8_t code2 = ((dr_dg + 8) << 4) | (db_dg + 8);
                        QoiWriteU8(code1);
                        QoiWriteU8(code2);
                    }
                    // Priority 5: QOI_OP_RGB - full rgb, alpha same
                    else {
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(px_r);
                        QoiWriteU8(px_g);
                        QoiWriteU8(px_b);
                    }
                }
                // Priority 5: QOI_OP_RGBA - full rgba
                else {
                    QoiWriteU8(QOI_OP_RGBA_TAG);
                    QoiWriteU8(px_r);
                    QoiWriteU8(px_g);
                    QoiWriteU8(px_b);
                    QoiWriteU8(px_a);
                }
            }
        }

        // Always update color table with current pixel
        color_table[hash_index][0] = px_r;
        color_table[hash_index][1] = px_g;
        color_table[hash_index][2] = px_b;
        color_table[hash_index][3] = px_a;

        prev_r = px_r;
        prev_g = px_g;
        prev_b = px_b;
        prev_a = px_a;
    }

    // Flush final run
    if (run_length > 0) {
        QoiWriteU8(QOI_OP_RUN_TAG | (run_length - 1));
    }

    // Write ending padding
    for (int i = 0; i < 8; i++) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    // read color space specifier
    colorspace = QoiReadU8();

    int run = 0;
    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r = 0, g = 0, b = 0, a = 255u;
    uint8_t prev_r = 0, prev_g = 0, prev_b = 0, prev_a = 255u;

    for (int i = 0; i < px_num; ++i) {

        if (run > 0) {
            run--;
            r = prev_r;
            g = prev_g;
            b = prev_b;
            a = prev_a;
        } else {
            uint8_t byte = QoiReadU8();

            if (byte == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = prev_a;
            } else if (byte == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = QoiReadU8();
            } else if ((byte & QOI_MASK_2) == QOI_OP_RUN_TAG) {
                run = (byte & 0x3f);
                r = prev_r;
                g = prev_g;
                b = prev_b;
                a = prev_a;
            } else if ((byte & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
                int index = byte & 0x3f;
                r = history[index][0];
                g = history[index][1];
                b = history[index][2];
                a = history[index][3];
            } else if ((byte & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                int dr = ((byte >> 4) & 0x03) - 2;
                int dg = ((byte >> 2) & 0x03) - 2;
                int db = (byte & 0x03) - 2;
                r = prev_r + dr;
                g = prev_g + dg;
                b = prev_b + db;
                a = prev_a;
            } else if ((byte & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                int dg = (byte & 0x3f) - 32;
                uint8_t byte2 = QoiReadU8();
                int dr_dg = ((byte2 >> 4) & 0x0f) - 8;
                int db_dg = (byte2 & 0x0f) - 8;
                int dr = dr_dg + dg;
                int db = db_dg + dg;
                r = prev_r + dr;
                g = prev_g + dg;
                b = prev_b + db;
                a = prev_a;
            }
        }

        // Add to color table
        int index = QoiColorHash(r, g, b, a);
        history[index][0] = r;
        history[index][1] = g;
        history[index][2] = b;
        history[index][3] = a;

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);

        prev_r = r;
        prev_g = g;
        prev_b = b;
        prev_a = a;
    }

    bool valid = true;
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
