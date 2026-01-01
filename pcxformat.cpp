#include "pcxformat.h"
#include <QFile>
#include <QDebug>
#include <cstring>

PCXFormat::PCXFormat() : m_width(0), m_height(0), m_valid(false)
{
    memset(&m_header, 0, sizeof(m_header));
}

PCXFormat::~PCXFormat()
{
}

bool PCXFormat::load(const QString &filename)
{
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly))
    {
        m_lastError = "Could not open file: " + filename;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    return loadFromMemory(data);
}

bool PCXFormat::loadFromMemory(const QByteArray &data)
{
    m_valid = false;
    m_decodedData.clear();
    m_palette.clear();
    m_lastError.clear();
    m_rawData = data;

    if (data.size() < 128)
    {
        m_lastError = "File too small for PCX header";
        return false;
    }

    const uint8_t *ptr = reinterpret_cast<const uint8_t*>(data.constData());

    if (!parseHeader(ptr, data.size()))
    {
        return false;
    }

    if (!decodeRLE(ptr + 128, data.size() - 128))
    {
        return false;
    }

    m_valid = true;
    return true;
}

bool PCXFormat::parseHeader(const uint8_t *data, size_t size)
{
    Q_UNUSED(size);

    memcpy(&m_header, data, sizeof(PCXHeader));

    // Validate header
    if (m_header.manufacturer != 0x0A)
    {
        m_lastError = QString("Invalid PCX manufacturer byte: 0x%1").arg(m_header.manufacturer, 2, 16, QChar('0'));
        return false;
    }

    if (m_header.encoding != 1)
    {
        m_lastError = QString("Unsupported PCX encoding: %1 (only RLE supported)").arg(m_header.encoding);
        return false;
    }

    m_width = m_header.xMax - m_header.xMin + 1;
    m_height = m_header.yMax - m_header.yMin + 1;

    if (m_width <= 0 || m_height <= 0 || m_width > 65535 || m_height > 65535)
    {
        m_lastError = QString("Invalid PCX dimensions: %1x%2").arg(m_width).arg(m_height);
        return false;
    }

    qWarning() << "PCX:" << m_width << "x" << m_height
               << "bpp:" << m_header.bitsPerPixel
               << "planes:" << m_header.numPlanes
               << "bytesPerLine:" << m_header.bytesPerLine;

    // Extract 256-color palette for 8-bit images
    // The palette is at the end of the file, preceded by 0x0C
    if (m_header.bitsPerPixel == 8 && m_header.numPlanes == 1)
    {
        int paletteOffset = m_rawData.size() - 769; // 1 byte marker + 768 bytes palette

        if (paletteOffset > 128 && (uint8_t)m_rawData[paletteOffset] == 0x0C)
        {
            const uint8_t *pal = reinterpret_cast<const uint8_t*>(m_rawData.constData()) + paletteOffset + 1;
            m_palette.resize(256);
            for (int i = 0; i < 256; i++)
            {
                m_palette[i] = qRgb(pal[i*3], pal[i*3+1], pal[i*3+2]);
            }
            qWarning() << "PCX: Found 256-color palette";
        }

        else
        {
            // Use grayscale palette
            m_palette.resize(256);
            for (int i = 0; i < 256; i++)
            {
                m_palette[i] = qRgb(i, i, i);
            }

            qWarning() << "PCX: Using grayscale palette";
        }
    }
    // Extract 16-color palette from header
    else if (m_header.bitsPerPixel == 1 && m_header.numPlanes == 4)
    {
        m_palette.resize(16);
        for (int i = 0; i < 16; i++)
        {
            m_palette[i] = qRgb(m_header.palette[i*3], m_header.palette[i*3+1], m_header.palette[i*3+2]);
        }
    }

    return true;
}

bool PCXFormat::decodeRLE(const uint8_t *data, size_t size)
{
    int totalBytes = m_header.bytesPerLine * m_header.numPlanes * m_height;
    m_decodedData.resize(totalBytes);

    uint8_t *dest = reinterpret_cast<uint8_t*>(m_decodedData.data());
    const uint8_t *src = data;
    const uint8_t *srcEnd = data + size;

    // Don't read into palette area for 8-bit images
    if (m_header.bitsPerPixel == 8 && m_header.numPlanes == 1)
    {
        srcEnd = data + size - 769;
    }

    int bytesDecoded = 0;

    while (bytesDecoded < totalBytes && src < srcEnd)
    {
        uint8_t byte = *src++;

        if ((byte & 0xC0) == 0xC0)
        {
            // RLE run
            int count = byte & 0x3F;
            if (src >= srcEnd) break;
            uint8_t value = *src++;

            for (int i = 0; i < count && bytesDecoded < totalBytes; i++)
            {
                dest[bytesDecoded++] = value;
            }
        }

        else
        {
            // Single byte
            dest[bytesDecoded++] = byte;
        }
    }

    if (bytesDecoded < totalBytes)
    {
        qWarning() << "PCX: Incomplete decode, got" << bytesDecoded << "of" << totalBytes << "bytes";
        // Pad with zeros
        memset(dest + bytesDecoded, 0, totalBytes - bytesDecoded);
    }

    return true;
}

QImage PCXFormat::toQImage() const
{
    if (!m_valid)
    {
        return QImage();
    }

    QImage image;

    // 8-bit indexed color (most common)
    if (m_header.bitsPerPixel == 8 && m_header.numPlanes == 1)
    {
        image = QImage(m_width, m_height, QImage::Format_Indexed8);
        image.setColorTable(m_palette.toList());

        for (int y = 0; y < m_height; y++)
        {
            const uint8_t *src = reinterpret_cast<const uint8_t*>(m_decodedData.constData()) + y * m_header.bytesPerLine;
            uint8_t *dest = image.scanLine(y);
            memcpy(dest, src, m_width);
        }
    }
    // 24-bit RGB (3 planes)
    else if (m_header.bitsPerPixel == 8 && m_header.numPlanes == 3)
    {
        image = QImage(m_width, m_height, QImage::Format_RGB888);

        int scanlineSize = m_header.bytesPerLine * 3;

        for (int y = 0; y < m_height; y++)
        {
            const uint8_t *srcR = reinterpret_cast<const uint8_t*>(m_decodedData.constData()) + y * scanlineSize;
            const uint8_t *srcG = srcR + m_header.bytesPerLine;
            const uint8_t *srcB = srcG + m_header.bytesPerLine;
            uint8_t *dest = image.scanLine(y);

            for (int x = 0; x < m_width; x++)
            {
                dest[x*3 + 0] = srcR[x];
                dest[x*3 + 1] = srcG[x];
                dest[x*3 + 2] = srcB[x];
            }
        }
    }
    // 32-bit RGBA (4 planes)
    else if (m_header.bitsPerPixel == 8 && m_header.numPlanes == 4)
    {
        image = QImage(m_width, m_height, QImage::Format_RGBA8888);

        int scanlineSize = m_header.bytesPerLine * 4;

        for (int y = 0; y < m_height; y++)
        {
            const uint8_t *srcR = reinterpret_cast<const uint8_t*>(m_decodedData.constData()) + y * scanlineSize;
            const uint8_t *srcG = srcR + m_header.bytesPerLine;
            const uint8_t *srcB = srcG + m_header.bytesPerLine;
            const uint8_t *srcA = srcB + m_header.bytesPerLine;
            uint8_t *dest = image.scanLine(y);

            for (int x = 0; x < m_width; x++)
            {
                dest[x*4 + 0] = srcR[x];
                dest[x*4 + 1] = srcG[x];
                dest[x*4 + 2] = srcB[x];
                dest[x*4 + 3] = srcA[x];
            }
        }
    }
    // 1-bit monochrome
    else if (m_header.bitsPerPixel == 1 && m_header.numPlanes == 1)
    {
        image = QImage(m_width, m_height, QImage::Format_Mono);
        image.setColor(0, qRgb(0, 0, 0));
        image.setColor(1, qRgb(255, 255, 255));

        for (int y = 0; y < m_height; y++)
        {
            const uint8_t *src = reinterpret_cast<const uint8_t*>(m_decodedData.constData()) + y * m_header.bytesPerLine;
            uint8_t *dest = image.scanLine(y);
            memcpy(dest, src, (m_width + 7) / 8);
        }
    }
    // 4-bit 16-color (4 planes, 1 bit each)
    else if (m_header.bitsPerPixel == 1 && m_header.numPlanes == 4)
    {
        image = QImage(m_width, m_height, QImage::Format_Indexed8);
        image.setColorTable(m_palette.toList());

        int scanlineSize = m_header.bytesPerLine * 4;

        for (int y = 0; y < m_height; y++)
        {
            const uint8_t *plane0 = reinterpret_cast<const uint8_t*>(m_decodedData.constData()) + y * scanlineSize;
            const uint8_t *plane1 = plane0 + m_header.bytesPerLine;
            const uint8_t *plane2 = plane1 + m_header.bytesPerLine;
            const uint8_t *plane3 = plane2 + m_header.bytesPerLine;
            uint8_t *dest = image.scanLine(y);

            for (int x = 0; x < m_width; x++)
            {
                int byteIdx = x / 8;
                int bitIdx = 7 - (x % 8);

                uint8_t pixel = ((plane0[byteIdx] >> bitIdx) & 1) | (((plane1[byteIdx] >> bitIdx) & 1) << 1) | (((plane2[byteIdx] >> bitIdx) & 1) << 2) | (((plane3[byteIdx] >> bitIdx) & 1) << 3);
                dest[x] = pixel;
            }
        }
    }

    else
    {
        qWarning() << "PCX: Unsupported format - bpp:" << m_header.bitsPerPixel << "planes:" << m_header.numPlanes;
        return QImage();
    }

    return image;
}

QString PCXFormat::getVersionString() const
{
    switch (m_header.version)
    {
    case 0: return "2.5";
    case 2: return "2.8 with palette";
    case 3: return "2.8 without palette";
    case 4: return "Windows";
    case 5: return "3.0+";
    default: return QString("Unknown (%1)").arg(m_header.version);
    }
}

QString PCXFormat::getFormatDescription() const
{
    int bpp = m_header.bitsPerPixel * m_header.numPlanes;

    if (m_header.bitsPerPixel == 8 && m_header.numPlanes == 1)
    {
        return "8-bit indexed color (256 colors)";
    }

    else if (m_header.bitsPerPixel == 8 && m_header.numPlanes == 3)
    {
        return "24-bit RGB";
    }

    else if (m_header.bitsPerPixel == 8 && m_header.numPlanes == 4)
    {
        return "32-bit RGBA";
    }

    else if (m_header.bitsPerPixel == 1 && m_header.numPlanes == 1)
    {
        return "1-bit monochrome";
    }

    else if (m_header.bitsPerPixel == 1 && m_header.numPlanes == 4)
    {
        return "4-bit indexed color (16 colors)";
    }

    return QString("%1-bit (%2 bpp x %3 planes)").arg(bpp).arg(m_header.bitsPerPixel).arg(m_header.numPlanes);
}
