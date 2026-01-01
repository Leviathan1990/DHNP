#ifndef PCXFORMAT_H
#define PCXFORMAT_H

#include <QImage>
#include <QByteArray>
#include <QString>
#include <cstdint>

// PCX Header structure (128 bytes)
#pragma pack(push, 1)

struct PCXHeader
{
    uint8_t  manufacturer;    // 0x0A = ZSoft
    uint8_t  version;         // 0=v2.5, 2=v2.8 w/palette, 3=v2.8 w/o palette, 5=v3.0
    uint8_t  encoding;        // 1 = RLE
    uint8_t  bitsPerPixel;    // bits per pixel per plane
    uint16_t xMin, yMin;      // image dimensions
    uint16_t xMax, yMax;
    uint16_t hDpi, vDpi;      // resolution
    uint8_t  palette[48];     // 16-color palette (EGA)
    uint8_t  reserved1;
    uint8_t  numPlanes;       // number of color planes
    uint16_t bytesPerLine;    // bytes per scanline per plane
    uint16_t paletteType;     // 1=color, 2=grayscale
    uint16_t hScreenSize;     // horizontal screen size
    uint16_t vScreenSize;     // vertical screen size
    uint8_t  reserved2[54];   // padding to 128 bytes
};
#pragma pack(pop)

class PCXFormat
{

public:
    PCXFormat();
    ~PCXFormat();

    // Load from file or memory
    bool load(const QString &filename);
    bool loadFromMemory(const QByteArray &data);

    // Convert to QImage
    QImage toQImage() const;

    // Getters
    bool isValid() const { return m_valid; }
    QString getLastError() const { return m_lastError; }

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getBitsPerPixel() const { return m_header.bitsPerPixel * m_header.numPlanes; }
    int getNumPlanes() const { return m_header.numPlanes; }
    uint8_t getVersion() const { return m_header.version; }

    QString getVersionString() const;
    QString getFormatDescription() const;

private:
    bool parseHeader(const uint8_t *data, size_t size);
    bool decodeRLE(const uint8_t *data, size_t size);
    void applyPalette();

    PCXHeader m_header;
    QByteArray m_rawData;
    QByteArray m_decodedData;  // Decoded scanlines
    QVector<QRgb> m_palette;   // 256-color palette

    int m_width;
    int m_height;
    bool m_valid;
    QString m_lastError;
};

#endif // PCXFORMAT_H
