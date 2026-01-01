#ifndef DTXFORMAT_H
#define DTXFORMAT_H

#include <QString>
#include <QByteArray>
#include <QImage>
#include <cstdint>

// LithTech DTX Texture Format
// Based on LithTech engine documentation and reverse engineering

// DTX Resource Type (first 4 bytes)
constexpr uint32_t DTX_RESOURCE_TYPE = 0;  // Texture resource

// DTX Versions (at offset 4, as signed int32)
constexpr int32_t DTX_VERSION_LT1    = -2;   // LithTech 1.0
constexpr int32_t DTX_VERSION_LT15   = -3;   // LithTech 1.5
constexpr int32_t DTX_VERSION_LT2    = -5;   // LithTech 2.0 (NOLF, etc.)
constexpr int32_t DTX_VERSION_TALON  = -7;   // Talon engine (FEAR, etc.)

// BPP (Bits Per Pixel) Identifiers
enum class DTX_BPP : uint16_t
{
    BPP_8P     = 0,    // 8-bit palettized
    BPP_8      = 1,    // 8-bit grayscale
    BPP_16     = 2,    // 16-bit (5-6-5 RGB)
    BPP_32     = 3,    // 32-bit (8-8-8-8 RGBA)
    BPP_S3TC_DXT1 = 4, // S3TC DXT1 compression
    BPP_S3TC_DXT3 = 5, // S3TC DXT3 compression
    BPP_S3TC_DXT5 = 6, // S3TC DXT5 compression
    BPP_32P    = 7,    // 32-bit palettized
};

// DTX Flags
enum DTX_FLAGS : uint32_t
{
    DTX_FULLBRITE      = 0x0001,  // Full brightness (no lighting)
    DTX_16BITSYSCOPY   = 0x0002,  // 16-bit system copy
    DTX_PREFER4444     = 0x0004,  // Prefer 4444 format
    DTX_NOSYSCACHE     = 0x0008,  // No system cache
    DTX_PREFER16BIT    = 0x0010,  // Prefer 16-bit
    DTX_MIPSALLOCED    = 0x0020,  // Mipmaps allocated
    DTX_SECTIONSFIXED  = 0x0040,  // Sections fixed up
    DTX_PREFER5551     = 0x0080,  // Prefer 5551 format
    DTX_32BITSYSCOPY   = 0x0100,  // 32-bit system copy
    DTX_CUBEMAP        = 0x0200,  // Cube map texture
    DTX_BUMPMAP        = 0x0400,  // Bump map texture
    DTX_LUMBUMPMAP     = 0x0800,  // Luminance bump map
};

// DTX Header Structure (164 bytes for version -5)
// Based on IDA analysis of sub_428924
#pragma pack(push, 1)
struct DTXHeader
{
    uint32_t resourceType;      // 0x00: Always 0 for textures
    int32_t  version;           // 0x04: Version (-5 = 0xFFFFFFFB for LithTech 2.0)
    uint16_t width;             // 0x08: Texture width
    uint16_t height;            // 0x0A: Texture height
    uint16_t mipmaps;           // 0x0C: Number of mipmaps (1-15)
    uint16_t sections;          // 0x0E: Number of sections
    int32_t  flags;             // 0x10: DTX flags
    int32_t  userFlags;         // 0x14: User-defined flags
    uint8_t  extra[12];         // 0x18: Extra data - BPP at extra[2] (offset 0x1A)
    //       extra[2] = 0 or 3 means 32-bit RGBA
    //       extra[2] = 4 means DXT1
    //       extra[2] = 5 means DXT3
    //       extra[2] = 6 means DXT5
    char     commandString[128];// 0x24: Command string (null-terminated)
    // Total: 164 bytes (0xA4)
};
#pragma pack(pop)

// Helper class for DTX file handling
class DTXFormat
{
public:
    DTXFormat();
    ~DTXFormat();

    // Load DTX from file or memory
    bool load(const QString &filename);
    bool loadFromMemory(const QByteArray &data);

    // Get texture information
    int getWidth() const { return header.width; }
    int getHeight() const { return header.height; }
    int getMipmapCount() const { return header.mipmaps; }
    int getVersion() const { return header.version; }
    uint32_t getFlags() const { return header.flags; }
    QString getCommandString() const;

    // Get BPP type
    DTX_BPP getBPPType() const;
    QString getBPPName() const;
    int getBitsPerPixel() const;

    // Check format
    bool isValid() const { return valid; }
    bool isCompressed() const;  // DXT1/3/5
    bool isCubeMap() const { return header.flags & DTX_CUBEMAP; }
    bool isBumpMap() const { return header.flags & DTX_BUMPMAP; }

    // Convert to QImage for display
    QImage toQImage(int mipmapLevel = 0);

    // Export to standard formats
    bool exportToPNG(const QString &filename, int mipmapLevel = 0);
    bool exportToBMP(const QString &filename, int mipmapLevel = 0);
    bool exportToTGA(const QString &filename, int mipmapLevel = 0);

    // Get raw pixel data
    QByteArray getPixelData(int mipmapLevel = 0) const;

    // Error handling
    QString getLastError() const { return lastError; }

    // Static utility functions
    static QString versionToString(int32_t version);
    static bool isLithTechFormat(const QByteArray &data);

private:
    bool parseHeader(const QByteArray &data);
    bool decodePixelData(const QByteArray &data);
    QImage decodeDXT1(const uint8_t *data, int width, int height);
    QImage decodeDXT3(const uint8_t *data, int width, int height);
    QImage decodeDXT5(const uint8_t *data, int width, int height);
    QImage decode32Bit(const uint8_t *data, int width, int height);
    QImage decode16Bit(const uint8_t *data, int width, int height);
    QImage decode8Bit(const uint8_t *data, int width, int height);

    DTXHeader header;
    QByteArray rawData;
    QVector<QByteArray> mipmapData;  // Pixel data for each mipmap level
    bool valid;
    QString lastError;
};

#endif // DTXFORMAT_H
