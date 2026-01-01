#include "rezarchive.h"
#include <QFileInfo>
#include <QDir>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QBuffer>
#include <algorithm>
#include <cstring>

RezArchive::RezArchive()
    : fileCount(0)
{
    memset(&header, 0, sizeof(RezHeader));

    // Build the standard 127-byte description
    memset(header.description, ' ', 127);

    // First line marker
    header.description[0] = '\r';
    header.description[1] = '\n';

    // First text line (positions 2-61, then CR LF at 62-63)
    const char* line1 = "RezMgr Version 1 Copyright (C) 1995 MONOLITH INC.";
    memcpy(&header.description[2], line1, strlen(line1));
    header.description[62] = '\r';
    header.description[63] = '\n';

    // Second text line (positions 64-123, then CR LF at 124-125, EOF at 126)
    const char* line2 = "LithTech Resource File";
    memcpy(&header.description[64], line2, strlen(line2));
    header.description[124] = '\r';
    header.description[125] = '\n';
    header.description[126] = 0x1A; // EOF marker

    header.version = 1;
    header.dirOffset = 127 + 44;
    header.dirSize = 0;
    header.isSorted = 1;
}

RezArchive::~RezArchive()
{
}

// Normalize path: convert forward slashes to backslashes, lowercase for comparison
QString RezArchive::normalizePath(const QString &path)
{
    QString normalized = path;
    normalized.replace('/', '\\');
    return normalized;
}

// Convert extension string to DWORD (reversed byte order)
// "WAV" -> 0x00564157 (stored as W A V \0 in memory, but reversed in DWORD)
quint32 RezArchive::extensionToUInt(const QString &ext)
{
    if (ext.isEmpty())
        return 0;

    QByteArray extBytes = ext.toUpper().toLatin1();
    if (extBytes.size() > 4)
        extBytes = extBytes.left(4);

    // Pad to 4 bytes with nulls
    while (extBytes.size() < 4)
        extBytes.append('\0');

    // Reverse order for DWORD
    quint32 result = 0;
    for (int i = 0; i < 4; i++)
    {
        result |= ((quint32)(unsigned char)extBytes[i]) << (8 * (3 - i));
    }

    return result;
}

// Convert DWORD back to extension string
QString RezArchive::uintToExtension(quint32 extDword)
{
    if (extDword == 0)
        return QString();

    char ext[5] = {0};
    ext[0] = (extDword >> 24) & 0xFF;
    ext[1] = (extDword >> 16) & 0xFF;
    ext[2] = (extDword >> 8) & 0xFF;
    ext[3] = extDword & 0xFF;

    QString result = QString::fromLatin1(ext).trimmed();
    result.remove(QChar('\0'));
    return result;
}

// Find file by path - case-insensitive, handles with/without extension
int RezArchive::findFileIndex(const QString &path) const
{
    QString normalized = normalizePath(path).toLower();

    // First try exact match with extension
    auto it = fileIndexWithExt.find(normalized);

    if (it != fileIndexWithExt.end())
    {
        return it.value();
    }

    // Then try without extension
    it = fileIndex.find(normalized);
    if (it != fileIndex.end())
    {
        return it.value();
    }

    // Try stripping extension from input and matching
    int lastDot = normalized.lastIndexOf('.');
    int lastSlash = normalized.lastIndexOf('\\');

    if (lastDot > lastSlash && lastDot > 0)
    {
        QString withoutExt = normalized.left(lastDot);

        it = fileIndex.find(withoutExt);
        if (it != fileIndex.end())
        {
            return it.value();
        }
    }

    return -1;
}

const RezFileEntry* RezArchive::getFileEntry(const QString &filename) const
{
    int idx = findFileIndex(filename);

    if (idx >= 0 && idx < entries.size())
    {
        return &entries[idx];
    }
    return nullptr;
}

QByteArray RezArchive::extractFile(const QString &filename)
{
    int idx = findFileIndex(filename);

    if (idx < 0)
    {
        lastError = "File not found: " + filename;
        return QByteArray();
    }

    const RezFileEntry &entry = entries[idx];

    if (!entry.isFile())
    {
        lastError = "Not a file: " + filename;
        return QByteArray();
    }

    if (entry.size == 0)
    {
        return QByteArray();  // Empty file is valid
    }

    QFile file(archivePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        lastError = "Cannot open archive";
        return QByteArray();
    }

    if (!file.seek(entry.offset))
    {
        lastError = QString("Cannot seek to offset 0x%1").arg(entry.offset, 0, 16);
        file.close();
        return QByteArray();
    }

    QByteArray data = file.read(entry.size);
    file.close();

    if (data.size() != (int)entry.size)
    {
        lastError = QString("Read size mismatch: expected %1, got %2").arg(entry.size).arg(data.size());
        return QByteArray();
    }

    return data;
}

bool RezArchive::load(const QString &filepath)
{
    QFile file(filepath);

    if (!file.open(QIODevice::ReadOnly))
    {
        lastError = "Cannot open file: " + filepath;
        return false;
    }

    archivePath = filepath;
    entries.clear();
    fileIndex.clear();
    fileIndexWithExt.clear();
    fileCount = 0;

    // Read header
    QByteArray headerData = file.read(256);
    if (headerData.size() < 171)
    {
        lastError = "File too small for header";
        file.close();
        return false;
    }

    // Validate header markers
    if (headerData[0] != '\r' || headerData[1] != '\n')
    {
        lastError = "Invalid header: missing initial CR LF";
        file.close();
        return false;
    }

    if ((unsigned char)headerData[126] != 0x1A)
    {
        lastError = "Invalid header: missing EOF marker (0x1A)";
        file.close();
        return false;
    }

    // Parse header fields starting at offset 127
    QDataStream stream(headerData);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.skipRawData(127);

    stream >> header.version;
    stream >> header.dirOffset;
    stream >> header.dirSize;
    stream >> header.dirTime;
    stream >> header.nextWritePos;
    stream >> header.fileTime;
    stream >> header.largestKeyArray;
    stream >> header.largestDirName;
    stream >> header.largestRezName;
    stream >> header.largestComment;
    stream >> header.isSorted;

    qDebug() << "\n=== REZ Archive ===";
    qDebug() << "File:" << QFileInfo(filepath).fileName();
    qDebug() << "Size:" << file.size() << "bytes";
    qDebug() << "Version:" << header.version;
    qDebug() << "Dir offset: 0x" << QString::number(header.dirOffset, 16) << "(" << header.dirOffset << ")";
    qDebug() << "Dir size:" << header.dirSize << "bytes";

    if (header.version != 1)
    {
        lastError = QString("Unsupported REZ version: %1").arg(header.version);
        file.close();
        return false;
    }

    if (header.dirOffset >= (quint32)file.size())
    {
        lastError = "Directory offset beyond file size";
        file.close();
        return false;
    }

    if (header.dirSize == 0)
    {
        qDebug() << "Empty archive";
        file.close();
        return true;
    }

    // Parse directory recursively
    pendingDirs.clear();

    if (!parseDirectoryRecursive(file, header.dirOffset, header.dirSize, ""))
    {
        file.close();
        return false;
    }

    file.close();

    // Build filename indices
    for (int i = 0; i < entries.size(); ++i)
    {
        if (entries[i].isFile())
        {
            // Index by path without extension
            QString keyNoExt = entries[i].fullPath.toLower();
            fileIndex[keyNoExt] = i;

            // Index by path WITH extension
            QString keyWithExt = entries[i].getFullPathWithExt().toLower();
            fileIndexWithExt[keyWithExt] = i;

            fileCount++;
        }
    }

    qDebug() << "=== Load Complete ===";
    qDebug() << "Total entries:" << entries.size();
    qDebug() << "Files indexed:" << fileCount;

    return true;
}

bool RezArchive::parseDirectoryRecursive(QFile &file, quint32 offset, quint32 size, const QString &path)
{
    if (size == 0)
        return true;

    if (!file.seek(offset))
    {
        lastError = QString("Failed to seek to 0x%1").arg(offset, 0, 16);
        return false;
    }

    QByteArray dirData = file.read(size);
    if (dirData.size() != (int)size)
    {
        lastError = "Failed to read directory data";
        return false;
    }

    qint64 fileSize = file.size();

    // Parse entries
    const char *ptr = dirData.constData();
    const char *end = ptr + dirData.size();

    // Collect subdirectories for later
    QVector<PendingDir> subdirs;

    while (ptr + 4 <= end)
    {
        quint32 type = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr));
        ptr += 4;

        if (type == 1)
        {
            // === DIRECTORY ENTRY ===
            // [4] type = 1 (already read)
            // [4] subdirOffset
            // [4] subdirSize
            // [4] timestamp
            // [N] dirname + null

            if (ptr + 12 > end) break;

            quint32 subdirOffset = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr)); ptr += 4;
            quint32 subdirSize = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr)); ptr += 4;
            quint32 timestamp = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr)); ptr += 4;

            // Read null-terminated name
            QString dirName;

            while (ptr < end && *ptr != '\0')
            {
                dirName.append(QLatin1Char(*ptr));
                ptr++;
            }

            if (ptr < end) ptr++; // Skip null

            RezFileEntry entry;
            entry.type = type;
            entry.offset = subdirOffset;
            entry.size = subdirSize;
            entry.dateTime = timestamp;
            entry.filename = dirName;
            entry.fileId = 0;
            entry.extensionReversed = 0;
            entry.numKeys = 0;

            QString subPath = path.isEmpty() ? dirName : (path + "\\" + dirName);
            entry.fullPath = subPath;

            entries.append(entry);

            qDebug() << "  Dir:" << dirName << "Offset: 0x" << QString::number(subdirOffset, 16) << "Size:" << subdirSize;

            // Queue subdirectory for recursive parsing
            if (subdirSize > 0 && subdirOffset > 0 && subdirOffset < fileSize && subdirOffset + subdirSize <= fileSize)
            {
                subdirs.append({subdirOffset, subdirSize, subPath});
            }

        }
        else if (type == 0)
        {
            // === FILE ENTRY ===
            // [4] type = 0 (already read)
            // [4] dataOffset
            // [4] dataSize
            // [4] timestamp
            // [4] fileId
            // [4] extensionReversed
            // [4] numKeys
            // [N] filename + null
            // [N] description + null
            // [numKeys * 4] keys

            if (ptr + 24 > end) break;

            quint32 dataOffset = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr)); ptr += 4;
            quint32 dataSize = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr)); ptr += 4;
            quint32 timestamp = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr)); ptr += 4;
            quint32 fileId = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr)); ptr += 4;
            quint32 extReversed = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr)); ptr += 4;
            quint32 numKeys = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(ptr)); ptr += 4;

            // Read filename
            QString fileName;
            while (ptr < end && *ptr != '\0')
            {
                fileName.append(QLatin1Char(*ptr));
                ptr++;
            }

            if (ptr < end) ptr++;

            // Read description
            QString desc;
            while (ptr < end && *ptr != '\0')
            {
                desc.append(QLatin1Char(*ptr));
                ptr++;
            }

            if (ptr < end) ptr++;

            // Skip keys
            if (ptr + numKeys * 4 <= end)
            {
                ptr += numKeys * 4;
            }

            RezFileEntry entry;
            entry.type = type;
            entry.offset = dataOffset;
            entry.size = dataSize;
            entry.dateTime = timestamp;
            entry.fileId = fileId;
            entry.extensionReversed = extReversed;
            entry.numKeys = numKeys;
            entry.filename = fileName;
            entry.description = desc;

            QString fullPath = path.isEmpty() ? fileName : (path + "\\" + fileName);
            entry.fullPath = fullPath;

            entries.append(entry);

            // Debug output with extension
            QString ext = entry.getExtension();
            qDebug() << "  File:" << entry.getFullPathWithExt() << "ID:" << fileId << "Size:" << dataSize << "Offset: 0x" << QString::number(dataOffset, 16) << "Ext:" << (ext.isEmpty() ? "(none)" : ext);

        }

        else
        {
            // Unknown type - stop parsing this directory
            qDebug() << "Unknown entry type:" << type << "at offset" << (ptr - dirData.constData() - 4);
            break;
        }
    }

    // Now recursively parse subdirectories
    for (const PendingDir &subdir : subdirs)
    {
        if (!parseDirectoryRecursive(file, subdir.offset, subdir.size, subdir.path))
        {
            return false;
        }
    }

    return true;
}

bool RezArchive::save(const QString &filepath, const QMap<QString, QByteArray> &modifiedFiles)
{
    if (archivePath.isEmpty())
    {
        lastError = "No archive loaded";
        return false;
    }

    // Create backup
    QString backupPath = filepath + ".backup";

    if (QFile::exists(backupPath))
    {
        QFile::remove(backupPath);
    }

    QFile::copy(filepath, backupPath);

    return saveInternal(filepath, modifiedFiles);
}

bool RezArchive::saveInternal(const QString &filepath, const QMap<QString, QByteArray> &modifiedFiles)
{
    QString tempPath = filepath + ".tmp";
    QFile outFile(tempPath);
    QFile inFile(archivePath);

    if (!outFile.open(QIODevice::WriteOnly))
    {
        lastError = "Cannot create temp file";
        return false;
    }

    if (!inFile.open(QIODevice::ReadOnly))
    {
        lastError = "Cannot open source file";
        outFile.close();
        return false;
    }

    // Build header (171 bytes, padded to 256 for compatibility)
    QByteArray headerBytes(256, 0);

    headerBytes[0] = '\r';
    headerBytes[1] = '\n';

    QByteArray line1 = "RezMgr Version 1 Copyright (C) 1995 MONOLITH INC.";
    for (int i = 2; i < 62; i++)
    {
        int idx = i - 2;
        headerBytes[i] = (idx < line1.size()) ? line1[idx] : ' ';
    }

    headerBytes[62] = '\r';
    headerBytes[63] = '\n';

    QByteArray line2 = "LithTech Resource File";

    for (int i = 64; i < 124; i++)
    {
        int idx = i - 64;
        headerBytes[i] = (idx < line2.size()) ? line2[idx] : ' ';
    }

    headerBytes[124] = '\r';
    headerBytes[125] = '\n';
    headerBytes[126] = 0x1A;

    outFile.write(headerBytes.left(127)); // Write description only first

    // Write header fields placeholder
    QByteArray headerFields(44, 0);
    outFile.write(headerFields);

    // Pad to 256 bytes if original was padded
    if (header.dirOffset >= 256)
    {
        QByteArray padding(256 - 171, 0);
        outFile.write(padding);
    }

    // Start writing file data
    quint32 currentOffset = outFile.pos();
    QVector<RezFileEntry> newEntries = entries;

    // Write file data and update offsets
    for (RezFileEntry &entry : newEntries)
    {
        if (!entry.isFile())
            continue;

        QString lookupKey = entry.fullPath.toLower();
        QString lookupKeyWithExt = entry.getFullPathWithExt().toLower();
        QByteArray data;
        bool modified = false;

        // Check both with and without extension
        for (auto it = modifiedFiles.begin(); it != modifiedFiles.end(); ++it)
        {
            QString modKey = normalizePath(it.key()).toLower();

            if (modKey == lookupKey || modKey == lookupKeyWithExt)
            {
                data = it.value();
                modified = true;
                break;
            }
        }

        if (!modified)
        {
            if (!inFile.seek(entry.offset))
            {
                lastError = "Failed to seek in source";
                goto error;
            }

            data = inFile.read(entry.size);
        }

        entry.offset = currentOffset;

        if (modified)
        {
            entry.size = data.size();
            entry.dateTime = QDateTime::currentSecsSinceEpoch();
        }

        outFile.write(data);
        currentOffset += data.size();
    }

    {
        // Build and write directory
        quint32 dirOffset = currentOffset;
        QByteArray dirData = buildDirectoryRecursive(newEntries, "");
        outFile.write(dirData);

        // Update header fields
        RezHeader newHeader = header;
        newHeader.dirOffset = dirOffset;
        newHeader.dirSize = dirData.size();
        newHeader.nextWritePos = outFile.pos();
        newHeader.fileTime = QDateTime::currentSecsSinceEpoch();

        // Write header fields at offset 127
        outFile.seek(127);
        QDataStream hdrStream(&outFile);
        hdrStream.setByteOrder(QDataStream::LittleEndian);
        hdrStream << newHeader.version;
        hdrStream << newHeader.dirOffset;
        hdrStream << newHeader.dirSize;
        hdrStream << newHeader.dirTime;
        hdrStream << newHeader.nextWritePos;
        hdrStream << newHeader.fileTime;
        hdrStream << newHeader.largestKeyArray;
        hdrStream << newHeader.largestDirName;
        hdrStream << newHeader.largestRezName;
        hdrStream << newHeader.largestComment;
        hdrStream << newHeader.isSorted;
    }

    outFile.close();
    inFile.close();

    if (QFile::exists(filepath))
    {
        QFile::remove(filepath);
    }

    if (!QFile::rename(tempPath, filepath))
    {
        lastError = "Failed to rename temp file";
        return false;
    }

    return load(filepath);

error:
    outFile.close();
    inFile.close();
    QFile::remove(tempPath);
    return false;
}

QByteArray RezArchive::buildDirectoryRecursive(const QVector<RezFileEntry> &allEntries, const QString &path)
{
    QByteArray dirData;
    QDataStream stream(&dirData, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    QString pathLower = path.toLower();

    // First write subdirectories at this level
    for (const RezFileEntry &entry : allEntries)
    {
        if (!entry.isDirectory())
            continue;

        // Check if this dir's parent matches current path
        int lastSlash = entry.fullPath.lastIndexOf('\\');
        QString parent = (lastSlash > 0) ? entry.fullPath.left(lastSlash) : "";

        if (parent.toLower() == pathLower)
        {
            // Write directory entry
            stream << quint32(1);           // type
            stream << entry.offset;         // subdirOffset
            stream << entry.size;           // subdirSize
            stream << entry.dateTime;       // timestamp

            QByteArray nameBytes = entry.filename.toLatin1();
            stream.writeRawData(nameBytes.constData(), nameBytes.size());
            stream << quint8(0);            // null terminator
        }
    }

    // Then write files at this level
    for (const RezFileEntry &entry : allEntries)
    {
        if (!entry.isFile())
            continue;

        int lastSlash = entry.fullPath.lastIndexOf('\\');
        QString parent = (lastSlash > 0) ? entry.fullPath.left(lastSlash) : "";

        if (parent.toLower() == pathLower)
        {
            // Write file entry
            stream << quint32(0);               // type
            stream << entry.offset;             // dataOffset
            stream << entry.size;               // dataSize
            stream << entry.dateTime;           // timestamp
            stream << entry.fileId;             // fileId
            stream << entry.extensionReversed;  // extension
            stream << entry.numKeys;            // numKeys

            QByteArray nameBytes = entry.filename.toLatin1();
            stream.writeRawData(nameBytes.constData(), nameBytes.size());
            stream << quint8(0);                // filename null

            // Description (empty)
            stream << quint8(0);                // description null

            // Keys
            for (quint32 i = 0; i < entry.numKeys; i++)
            {
                stream << quint32(0);
            }
        }
    }

    return dirData;
}

bool RezArchive::replaceFile(const QString &filename, const QByteArray &data)
{
    int idx = findFileIndex(filename);

    if (idx < 0)
    {
        lastError = "File not found: " + filename;
        return false;
    }

    RezFileEntry &entry = entries[idx];
    entry.size = data.size();
    entry.dateTime = QDateTime::currentSecsSinceEpoch();

    return true;
}

bool RezArchive::addFile(const QString &filename, const QByteArray &data)
{
    QString normalized = normalizePath(filename);

    if (findFileIndex(normalized) >= 0)
    {
        lastError = "File already exists";
        return false;
    }

    RezFileEntry entry;
    entry.type = 0;
    entry.offset = 0;
    entry.size = data.size();
    entry.dateTime = QDateTime::currentSecsSinceEpoch();
    entry.fileId = entries.size() + 10000000;
    entry.numKeys = 0;

    // Extract extension from filename
    int lastDot = normalized.lastIndexOf('.');
    int lastSlash = normalized.lastIndexOf('\\');

    if (lastDot > lastSlash && lastDot > 0)
    {
        QString ext = normalized.mid(lastDot + 1);
        entry.extensionReversed = extensionToUInt(ext);
        entry.fullPath = normalized.left(lastDot);  // Store without extension
    }

    else
    {
        entry.extensionReversed = 0;
        entry.fullPath = normalized;
    }

    // Extract just the filename
    if (lastSlash >= 0)
    {
        QString nameWithExt = normalized.mid(lastSlash + 1);
        int dotPos = nameWithExt.lastIndexOf('.');
        entry.filename = (dotPos > 0) ? nameWithExt.left(dotPos) : nameWithExt;
    }

    else
    {
        int dotPos = normalized.lastIndexOf('.');
        entry.filename = (dotPos > 0) ? normalized.left(dotPos) : normalized;
    }

    entries.append(entry);

    // Update indices
    fileIndex[entry.fullPath.toLower()] = entries.size() - 1;
    fileIndexWithExt[entry.getFullPathWithExt().toLower()] = entries.size() - 1;
    fileCount++;

    return true;
}

bool RezArchive::removeFile(const QString &filename)
{
    int idx = findFileIndex(filename);

    if (idx < 0)
    {
        lastError = "File not found";
        return false;
    }

    entries.removeAt(idx);

    // Rebuild indices
    fileIndex.clear();
    fileIndexWithExt.clear();
    fileCount = 0;

    for (int i = 0; i < entries.size(); i++)
    {
        if (entries[i].isFile())
        {
            fileIndex[entries[i].fullPath.toLower()] = i;
            fileIndexWithExt[entries[i].getFullPathWithExt().toLower()] = i;
            fileCount++;
        }
    }

    return true;
}
