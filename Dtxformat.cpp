#include "Dtxformat.h"
#include <QFile>
#include <QDebug>
#include <cstring>

DTXFormat::DTXFormat() : valid(false)
{
    memset(&header, 0, sizeof(header));
}

DTXFormat::~DTXFormat()
{
}

bool DTXFormat::load(const QString &filename)
{
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly))
    {
        lastError = "Cannot open file: " + filename;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    return loadFromMemory(data);
}

bool DTXFormat::loadFromMemory(const QByteArray &data)
{
    valid = false;
    rawData = data;

    if (data.size() < (int)sizeof(DTXHeader))
    {
        lastError = "File too small for DTX header";
        return false;
    }

    if (!parseHeader(data))
    {
        return false;
    }

    if (!decodePixelData(data))
    {
        return false;
    }

    valid = true;
    return true;
}

bool DTXFormat::parseHeader(const QByteArray &data)
{
    memcpy(&header, data.constData(), sizeof(DTXHeader));

    // Validate resource type
    if (header.resourceType != DTX_RESOURCE_TYPE)
    {
        lastError = QString("Invalid resource type: %1 (expected 0)").arg(header.resourceType);
        return false;
    }

    // Validate version
    if (header.version != DTX_VERSION_LT1 &&
        header.version != DTX_VERSION_LT15 &&
        header.version != DTX_VERSION_LT2 &&
        header.version != DTX_VERSION_TALON)
    {
        lastError = QString("Unknown DTX version: %1").arg(header.version);
        // Don't fail - try to parse anyway
    }

    // Validate dimensions
    if (header.width == 0 || header.height == 0)
    {
        lastError = "Invalid texture dimensions";
        return false;
    }

    // Validate dimensions are power of 2
    if ((header.width & (header.width - 1)) != 0 || (header.height & (header.height - 1)) != 0)
    {
        // Warning only - some textures may not be power of 2
        qWarning() << "DTX: Non-power-of-2 dimensions:" << header.width << "x" << header.height;
    }

    return true;
}

bool DTXFormat::decodePixelData(const QByteArray &data)
{
    mipmapData.clear();

    int headerSize = sizeof(DTXHeader);
    int offset = headerSize;

    int width = header.width;
    int height = header.height;
    int numMipmaps = header.mipmaps;

    // Limit mipmaps to reasonable value
    if (numMipmaps <= 0) numMipmaps = 1;
    if (numMipmaps > 16) numMipmaps = 16;

    qWarning() << "decodePixelData: headerSize=" << headerSize
               << "dataSize=" << data.size()
               << "numMipmaps=" << numMipmaps
               << "bppType=" << (int)getBPPType();

    for (int mip = 0; mip < numMipmaps && offset < data.size(); mip++)
    {
        int mipWidth = qMax(1, width >> mip);
        int mipHeight = qMax(1, height >> mip);
        int mipSize = 0;

        switch (getBPPType())

        {
        case DTX_BPP::BPP_32:
        case DTX_BPP::BPP_32P:
            mipSize = mipWidth * mipHeight * 4;
            break;
        case DTX_BPP::BPP_16:
            mipSize = mipWidth * mipHeight * 2;
            break;
        case DTX_BPP::BPP_8:
        case DTX_BPP::BPP_8P:
            mipSize = mipWidth * mipHeight;
            break;
        case DTX_BPP::BPP_S3TC_DXT1:
            // DXT1: 8 bytes per 4x4 block
            mipSize = qMax(1, ((mipWidth + 3) / 4)) * qMax(1, ((mipHeight + 3) / 4)) * 8;
            break;
        case DTX_BPP::BPP_S3TC_DXT3:
        case DTX_BPP::BPP_S3TC_DXT5:
            // DXT3/5: 16 bytes per 4x4 block
            mipSize = qMax(1, ((mipWidth + 3) / 4)) * qMax(1, ((mipHeight + 3) / 4)) * 16;
            break;
        default:
            // Assume 32-bit
            mipSize = mipWidth * mipHeight * 4;
            break;
        }

        qWarning() << "  Mipmap" << mip << ": " << mipWidth << "x" << mipHeight << "size=" << mipSize << "offset=" << offset;

        if (offset + mipSize > data.size())
        {
            qWarning() << "  NOT ENOUGH DATA! need" << (offset + mipSize) << "have" << data.size();
            if (mip == 0)
            {
                lastError = QString("Not enough data for texture: need %1, have %2").arg(offset + mipSize).arg(data.size());
                return false;
            }
            break;  // Partial mipmaps is OK
        }

        mipmapData.append(data.mid(offset, mipSize));
        offset += mipSize;
    }

    qWarning() << "  Loaded" << mipmapData.size() << "mipmaps";

    return !mipmapData.isEmpty();
}

DTX_BPP DTXFormat::getBPPType() const
{
    // BPP type is stored in extra[2]
    return static_cast<DTX_BPP>(header.extra[2]);
}

QString DTXFormat::getBPPName() const
{
    switch (getBPPType()) {
    case DTX_BPP::BPP_8P:     return "8-bit Palettized";
    case DTX_BPP::BPP_8:      return "8-bit Grayscale";
    case DTX_BPP::BPP_16:     return "16-bit RGB";
    case DTX_BPP::BPP_32:     return "32-bit RGBA";
    case DTX_BPP::BPP_S3TC_DXT1: return "DXT1 (S3TC)";
    case DTX_BPP::BPP_S3TC_DXT3: return "DXT3 (S3TC)";
    case DTX_BPP::BPP_S3TC_DXT5: return "DXT5 (S3TC)";
    case DTX_BPP::BPP_32P:    return "32-bit Palettized";
    default: return QString("Unknown (%1)").arg((int)getBPPType());
    }
}

int DTXFormat::getBitsPerPixel() const
{
    switch (getBPPType())
    {
    case DTX_BPP::BPP_8P:
    case DTX_BPP::BPP_8:      return 8;
    case DTX_BPP::BPP_16:     return 16;
    case DTX_BPP::BPP_32:
    case DTX_BPP::BPP_32P:    return 32;
    case DTX_BPP::BPP_S3TC_DXT1: return 4;  // Compressed
    case DTX_BPP::BPP_S3TC_DXT3:
    case DTX_BPP::BPP_S3TC_DXT5: return 8;  // Compressed
    default: return 32;
    }
}

bool DTXFormat::isCompressed() const
{
    DTX_BPP bpp = getBPPType();
    return bpp == DTX_BPP::BPP_S3TC_DXT1 || bpp == DTX_BPP::BPP_S3TC_DXT3 || bpp == DTX_BPP::BPP_S3TC_DXT5;
}

QString DTXFormat::getCommandString() const
{
    return QString::fromLatin1(header.commandString, 128).trimmed();
}

QString DTXFormat::versionToString(int32_t version)
{
    switch (version)
    {
    case DTX_VERSION_LT1:   return "LithTech 1.0";
    case DTX_VERSION_LT15:  return "LithTech 1.5";
    case DTX_VERSION_LT2:   return "LithTech 2.0";
    case DTX_VERSION_TALON: return "Talon/FEAR";
    default: return QString("Unknown (%1)").arg(version);
    }
}

bool DTXFormat::isLithTechFormat(const QByteArray &data)
{
    if (data.size() < 8) return false;

    // Check version byte at offset 4
    uint8_t versionByte = static_cast<uint8_t>(data[4]);

    // LithTech format has 0xFB (-5) or 0xFE (-2) or 0xFD (-3)
    return (versionByte == 0xFB || versionByte == 0xFE || versionByte == 0xFD || versionByte == 0xF9);
}

QByteArray DTXFormat::getPixelData(int mipmapLevel) const
{
    if (mipmapLevel < 0 || mipmapLevel >= mipmapData.size())
    {
        return QByteArray();
    }
    return mipmapData[mipmapLevel];
}

QImage DTXFormat::toQImage(int mipmapLevel)
{
    if (!valid || mipmapLevel >= mipmapData.size())
    {
        qWarning() << "toQImage: invalid state - valid=" << valid << "mipmapLevel=" << mipmapLevel << "mipmapData.size()=" << mipmapData.size();
        return QImage();
    }

    const QByteArray &pixelData = mipmapData[mipmapLevel];

    // Calculate mipmap dimensions
    int width = header.width >> mipmapLevel;
    int height = header.height >> mipmapLevel;
    width = qMax(1, width);
    height = qMax(1, height);

    qWarning() << "toQImage: decoding" << width << "x" << height << "bppType=" << (int)getBPPType() << "pixelData.size()=" << pixelData.size();

    const uint8_t *data = reinterpret_cast<const uint8_t*>(pixelData.constData());

    QImage result;
    switch (getBPPType())
    {
    case DTX_BPP::BPP_32:
    case DTX_BPP::BPP_32P:
        result = decode32Bit(data, width, height);
        break;
    case DTX_BPP::BPP_16:
        result = decode16Bit(data, width, height);
        break;
    case DTX_BPP::BPP_8:
    case DTX_BPP::BPP_8P:
        result = decode8Bit(data, width, height);
        break;
    case DTX_BPP::BPP_S3TC_DXT1:
        result = decodeDXT1(data, width, height);
        break;
    case DTX_BPP::BPP_S3TC_DXT3:
        result = decodeDXT3(data, width, height);
        break;
    case DTX_BPP::BPP_S3TC_DXT5:
        result = decodeDXT5(data, width, height);
        break;
    default:
        lastError = "Unsupported BPP format";
        qWarning() << "toQImage: unsupported BPP format:" << (int)getBPPType();
        return QImage();
    }

    qWarning() << "toQImage: result isNull=" << result.isNull() << "size=" << result.width() << "x" << result.height();
    return result;
}

QImage DTXFormat::decode32Bit(const uint8_t *data, int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32);

    qWarning() << "decode32Bit:" << width << "x" << height;

    for (int y = 0; y < height; y++)
    {
        QRgb *line = reinterpret_cast<QRgb*>(image.scanLine(y));

        for (int x = 0; x < width; x++)
        {
            int idx = (y * width + x) * 4;
            // DTX stores as RGBA (based on game code)
            uint8_t r = data[idx + 0];
            uint8_t g = data[idx + 1];
            uint8_t b = data[idx + 2];
            // uint8_t a = data[idx + 3];  // Original alpha
            // Force alpha to 255 for preview (same as DXT)
            line[x] = qRgba(r, g, b, 255);
        }
    }

    qWarning() << "decode32Bit complete";
    return image;
}

QImage DTXFormat::decode16Bit(const uint8_t *data, int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (int y = 0; y < height; y++)
    {
        QRgb *line = reinterpret_cast<QRgb*>(image.scanLine(y));

        for (int x = 0; x < width; x++)
        {
            int idx = (y * width + x) * 2;
            uint16_t pixel = data[idx] | (data[idx + 1] << 8);

            // 5-6-5 RGB format
            uint8_t r = ((pixel >> 11) & 0x1F) << 3;
            uint8_t g = ((pixel >> 5) & 0x3F) << 2;
            uint8_t b = (pixel & 0x1F) << 3;

            line[x] = qRgb(r, g, b);
        }
    }

    return image;
}

QImage DTXFormat::decode8Bit(const uint8_t *data, int width, int height)
{
    // Grayscale
    QImage image(width, height, QImage::Format_Grayscale8);

    for (int y = 0; y < height; y++)
    {
        uint8_t *line = image.scanLine(y);
        memcpy(line, data + y * width, width);
    }

    return image;
}

// DXT1 decompression
QImage DTXFormat::decodeDXT1(const uint8_t *data, int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::black);

    int blockWidth = (width + 3) / 4;
    int blockHeight = (height + 3) / 4;

    for (int by = 0; by < blockHeight; by++)
    {
        for (int bx = 0; bx < blockWidth; bx++)
        {
            int blockIdx = (by * blockWidth + bx) * 8;

            // Read two 16-bit colors
            uint16_t c0 = data[blockIdx] | (data[blockIdx + 1] << 8);
            uint16_t c1 = data[blockIdx + 2] | (data[blockIdx + 3] << 8);

            // Decode colors (5-6-5 RGB)
            uint8_t r0 = ((c0 >> 11) & 0x1F) << 3;
            uint8_t g0 = ((c0 >> 5) & 0x3F) << 2;
            uint8_t b0 = (c0 & 0x1F) << 3;

            uint8_t r1 = ((c1 >> 11) & 0x1F) << 3;
            uint8_t g1 = ((c1 >> 5) & 0x3F) << 2;
            uint8_t b1 = (c1 & 0x1F) << 3;

            // Build color table
            QRgb colors[4];
            colors[0] = qRgba(r0, g0, b0, 255);
            colors[1] = qRgba(r1, g1, b1, 255);

            if (c0 > c1)
            {
                // 4 colors, no alpha
                colors[2] = qRgba((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
                colors[3] = qRgba((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
            }

            else
            {
                // 3 colors + transparent
                colors[2] = qRgba((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
                colors[3] = qRgba(0, 0, 0, 0);  // Transparent
            }

            // Read 4x4 indices (32 bits = 16 x 2-bit indices)
            uint32_t indices = data[blockIdx + 4] | (data[blockIdx + 5] << 8) | (data[blockIdx + 6] << 16) | (data[blockIdx + 7] << 24);

            // Apply colors to pixels
            for (int py = 0; py < 4; py++)
            {
                for (int px = 0; px < 4; px++)
                {
                    int x = bx * 4 + px;
                    int y = by * 4 + py;

                    if (x < width && y < height)
                    {
                        int idx = (indices >> ((py * 4 + px) * 2)) & 0x3;
                        image.setPixel(x, y, colors[idx]);
                    }
                }
            }
        }
    }

    return image;
}

// DXT3 decompression (explicit alpha) - based on game's sub_100052FF
QImage DTXFormat::decodeDXT3(const uint8_t *data, int width, int height)
{
    QImage image(width, height, QImage::Format_RGBA8888);  // RGBA order!
    image.fill(Qt::magenta);

    int blockCountX = (width + 3) / 4;
    int blockCountY = (height + 3) / 4;

    qWarning() << "decodeDXT3:" << width << "x" << height << "blocks:" << blockCountX << "x" << blockCountY;

    // Build lookup tables like the game does (v31 = 5-bit, v29 = 6-bit)
    uint8_t lut5[32], lut6[64];
    for (int i = 0; i < 32; i++) lut5[i] = 255 * i / 31;
    for (int i = 0; i < 64; i++) lut6[i] = 255 * i / 63;

    // Alpha lookup table for 4-bit values (byte_10358290)
    uint8_t alphaLut[16];
    for (int i = 0; i < 16; i++) alphaLut[i] = 255 * i / 15;

    for (int blockY = 0; blockY < blockCountY; blockY++)
    {
        for (int blockX = 0; blockX < blockCountX; blockX++)
        {
            const uint8_t *block = data + (blockY * blockCountX + blockX) * 16;

            // First 8 bytes: alpha data (4 bits per pixel)
            const uint8_t *alphaData = block;

            // Next 8 bytes: color data
            uint16_t c0 = block[8] | (block[9] << 8);
            uint16_t c1 = block[10] | (block[11] << 8);
            const uint8_t *indices = block + 12;

            // Extract RGB565 components
            int r0_5 = (c0 >> 11) & 0x1F;
            int g0_6 = (c0 >> 5) & 0x3F;
            int b0_5 = c0 & 0x1F;

            int r1_5 = (c1 >> 11) & 0x1F;
            int g1_6 = (c1 >> 5) & 0x3F;
            int b1_5 = c1 & 0x1F;

            // Build color palette (as 32-bit RGBA values)
            uint32_t colors[4];
            colors[0] = (lut5[r0_5] << 16) | (lut6[g0_6] << 8) | lut5[b0_5];
            colors[1] = (lut5[r1_5] << 16) | (lut6[g1_6] << 8) | lut5[b1_5];

            if (c0 <= c1)
            {
                // 2-color mode (for DXT1 with transparency, but DXT3 always uses 4-color)
                colors[2] = (lut5[(r0_5 + r1_5) / 2] << 16) | (lut6[(g0_6 + g1_6) / 2] << 8) | lut5[(b0_5 + b1_5) / 2];
                colors[3] = 0;
            }

            else
            {
                // 4-color mode
                colors[2] = (((2 * lut5[r0_5] + lut5[r1_5]) / 3) << 16) | (((2 * lut6[g0_6] + lut6[g1_6]) / 3) << 8) | ((2 * lut5[b0_5] + lut5[b1_5]) / 3);
                colors[3] = (((lut5[r0_5] + 2 * lut5[r1_5]) / 3) << 16) | (((lut6[g0_6] + 2 * lut6[g1_6]) / 3) << 8) | ((lut5[b0_5] + 2 * lut5[b1_5]) / 3);
            }

            // Decode 16 pixels
            for (int i = 0; i < 16; i++)
            {
                int px = i % 4;
                int py = i / 4;
                int x = blockX * 4 + px;
                int y = blockY * 4 + py;

                if (x < width && y < height)
                {
                    // Get color index (2 bits)
                    uint8_t indexByte = indices[py];
                    int colorIdx = (indexByte >> (px * 2)) & 0x03;
                    uint32_t color = colors[colorIdx];

                    // Get alpha (4 bits) - same as game: alphaData[i/2] >> (4*(i%2))
                    uint8_t alphaByte = alphaData[i / 2];
                    int alpha4 = (alphaByte >> (4 * (i % 2))) & 0x0F;
                    uint8_t alpha = alphaLut[alpha4];

                    // Write RGBA (game order: R, G, B, A)
                    uint8_t r = (color >> 16) & 0xFF;
                    uint8_t g = (color >> 8) & 0xFF;
                    uint8_t b = color & 0xFF;

                    // Force alpha to 255 for preview (transparent pixels invisible on white)
                    // Original alpha is still calculated correctly for export if needed
                    image.setPixel(x, y, qRgba(r, g, b, 255));
                }
            }
        }
    }

    qWarning() << "decodeDXT3 complete";
    return image;
}

// DXT5 decompression (interpolated alpha) - based on game's sub_100052FF
QImage DTXFormat::decodeDXT5(const uint8_t *data, int width, int height)
{
    QImage image(width, height, QImage::Format_RGBA8888);  // RGBA order!
    image.fill(Qt::magenta);

    int blockCountX = (width + 3) / 4;
    int blockCountY = (height + 3) / 4;

    qWarning() << "decodeDXT5:" << width << "x" << height << "blocks:" << blockCountX << "x" << blockCountY;

    // Build lookup tables like the game does
    uint8_t lut5[32], lut6[64];
    for (int i = 0; i < 32; i++) lut5[i] = 255 * i / 31;
    for (int i = 0; i < 64; i++) lut6[i] = 255 * i / 63;

    for (int blockY = 0; blockY < blockCountY; blockY++)
    {
        for (int blockX = 0; blockX < blockCountX; blockX++)
        {
            const uint8_t *block = data + (blockY * blockCountX + blockX) * 16;

            // First 2 bytes: alpha reference values
            uint8_t a0 = block[0];
            uint8_t a1 = block[1];

            // Next 6 bytes: alpha indices (48 bits = 16 * 3 bits)
            // Read as 48-bit value (stored in bytes 2-7)
            uint64_t alphaIndices = 0;

            for (int i = 0; i < 6; i++)
            {
                alphaIndices |= ((uint64_t)block[2 + i]) << (i * 8);
            }

            // Color data at offset 8
            uint16_t c0 = block[8] | (block[9] << 8);
            uint16_t c1 = block[10] | (block[11] << 8);
            const uint8_t *colorIndicesData = block + 12;

            // Extract RGB565 components
            int r0_5 = (c0 >> 11) & 0x1F;
            int g0_6 = (c0 >> 5) & 0x3F;
            int b0_5 = c0 & 0x1F;

            int r1_5 = (c1 >> 11) & 0x1F;
            int g1_6 = (c1 >> 5) & 0x3F;
            int b1_5 = c1 & 0x1F;

            // Build color palette
            uint32_t colors[4];
            colors[0] = (lut5[r0_5] << 16) | (lut6[g0_6] << 8) | lut5[b0_5];
            colors[1] = (lut5[r1_5] << 16) | (lut6[g1_6] << 8) | lut5[b1_5];

            if (c0 <= c1)
            {
                colors[2] = (lut5[(r0_5 + r1_5) / 2] << 16) | (lut6[(g0_6 + g1_6) / 2] << 8) | lut5[(b0_5 + b1_5) / 2];
                colors[3] = 0;
            }

            else
            {
                colors[2] = (((2 * lut5[r0_5] + lut5[r1_5]) / 3) << 16) | (((2 * lut6[g0_6] + lut6[g1_6]) / 3) << 8) | ((2 * lut5[b0_5] + lut5[b1_5]) / 3);
                colors[3] = (((lut5[r0_5] + 2 * lut5[r1_5]) / 3) << 16) | (((lut6[g0_6] + 2 * lut6[g1_6]) / 3) << 8) | ((lut5[b0_5] + 2 * lut5[b1_5]) / 3);
            }

            // Decode 16 pixels
            for (int i = 0; i < 16; i++)
            {
                int px = i % 4;
                int py = i / 4;
                int x = blockX * 4 + px;
                int y = blockY * 4 + py;

                if (x < width && y < height)
                {
                    // Get color index (2 bits per pixel)
                    uint8_t indexByte = colorIndicesData[py];
                    int colorIdx = (indexByte >> (px * 2)) & 0x03;
                    uint32_t color = colors[colorIdx];

                    // Get alpha index (3 bits) - game uses sub_1012ABF0 for bit extraction
                    int alphaIdx = (alphaIndices >> (i * 3)) & 0x07;

                    // Calculate alpha based on game logic
                    uint8_t alpha;

                    if (a0 <= a1)
                    {
                        // 6-value interpolation mode
                        switch (alphaIdx)
                        {
                        case 0: alpha = a0; break;
                        case 1: alpha = a1; break;
                        case 6: alpha = 0; break;
                        case 7: alpha = 255; break;
                        default:
                            alpha = ((alphaIdx - 1) * a1 + (6 - alphaIdx) * a0) / 5;
                            break;
                        }
                    }

                    else
                    {
                        // 8-value interpolation mode
                        if (alphaIdx == 0)
                        {
                            alpha = a0;
                        }

                        else if (alphaIdx == 1)
                        {
                            alpha = a1;
                        }

                        else
                        {
                            alpha = ((alphaIdx - 1) * a1 + (8 - alphaIdx) * a0) / 7;
                        }
                    }

                    // Write RGBA
                    uint8_t r = (color >> 16) & 0xFF;
                    uint8_t g = (color >> 8) & 0xFF;
                    uint8_t b = color & 0xFF;

                    // Force alpha to 255 for preview
                    image.setPixel(x, y, qRgba(r, g, b, 255));
                }
            }
        }
    }

    qWarning() << "decodeDXT5 complete";
    return image;
}

bool DTXFormat::exportToPNG(const QString &filename, int mipmapLevel)
{
    QImage img = toQImage(mipmapLevel);

    if (img.isNull())
    {
        lastError = "Failed to decode image";
        return false;
    }
    return img.save(filename, "PNG");
}

bool DTXFormat::exportToBMP(const QString &filename, int mipmapLevel)
{
    QImage img = toQImage(mipmapLevel);

    if (img.isNull())
    {
        lastError = "Failed to decode image";
        return false;
    }
    return img.save(filename, "BMP");
}

bool DTXFormat::exportToTGA(const QString &filename, int mipmapLevel)
{
    // QImage doesn't support TGA directly, use PNG instead or implement TGA writer
    QImage img = toQImage(mipmapLevel);

    if (img.isNull())
    {
        lastError = "Failed to decode image";
        return false;
    }

    // For now, save as PNG
    return img.save(filename, "PNG");
}
