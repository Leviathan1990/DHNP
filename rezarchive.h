#ifndef REZARCHIVE_H
#define REZARCHIVE_H

#include <QString>
#include <QVector>
#include <QByteArray>
#include <QMap>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QDataStream>
#include <QDateTime>
#include <QtEndian>

// REZ Archive Header Structure
struct RezHeader
{
    char description[127];      // Formatted text with CR/LF and EOF (0x1A) marker
    quint32 version;            // Must be 1
    quint32 dirOffset;          // Root directory offset in file
    quint32 dirSize;            // Root directory size in bytes
    quint32 dirTime;            // Directory modification timestamp
    quint32 nextWritePos;       // Next write position
    quint32 fileTime;           // File modification timestamp
    quint32 largestKeyArray;    // Largest key array size
    quint32 largestDirName;     // Largest directory name length
    quint32 largestRezName;     // Largest resource name length
    quint32 largestComment;     // Largest comment length
    quint32 isSorted;           // 1 = sorted, 0 = unsorted
};

// File/Directory Entry Structure
struct RezFileEntry
{
    quint32 type;               // 0 = file, 1 = directory
    quint32 offset;             // File: data offset; Dir: subdir data offset
    quint32 size;               // File: data size; Dir: subdir data size
    quint32 dateTime;           // Timestamp (Unix epoch)
    quint32 fileId;             // Unique file ID (files only)
    quint32 extensionReversed;  // Extension as reversed DWORD (files only)
    quint32 numKeys;            // Number of keys (files only)
    QString filename;           // Just the filename/dirname (no path, no extension)
    QString fullPath;           // Full path with backslashes (no extension)
    QString description;        // Description string (files only)

    // Get extension from reversed DWORD (e.g., "DTX", "WAV")
    QString getExtension() const
    {
        if (type != 0 || extensionReversed == 0)
            return QString();

        QString ext;
        quint32 val = extensionReversed;

        // Extract characters from LSB to MSB, prepend to reverse
        for (int i = 0; i < 4; i++)
        {
            char c = val & 0xFF;

            if (c != '\0' && c >= 0x20 && c <= 0x7E)
            {
                ext.prepend(QChar::fromLatin1(c));
            }
            val >>= 8;
        }

        return ext;
    }

    // Get filename WITH extension (e.g., "BENCH.DTX")
    QString getFilenameWithExt() const
    {
        QString ext = getExtension();

        if (ext.isEmpty()) {
            return filename;
        }
        return filename + "." + ext;
    }

    // Get full path WITH extension (e.g., "PROPS\MODELS\CHAIR\BENCH.DTX")
    QString getFullPathWithExt() const
    {
        QString ext = getExtension();

        if (ext.isEmpty())
        {
            return fullPath;
        }
        return fullPath + "." + ext;
    }

    bool isFile() const { return type == 0; }
    bool isDirectory() const { return type == 1; }
};

// Helper struct for recursive parsing
struct PendingDir
{
    quint32 offset;
    quint32 size;
    QString path;
};

class RezArchive
{
public:
    RezArchive();
    ~RezArchive();

    // Main operations
    bool load(const QString &filepath);
    bool save(const QString &filepath, const QMap<QString, QByteArray> &modifiedFiles);

    // File operations - accepts path with OR without extension, case-insensitive
    QByteArray extractFile(const QString &filename);
    bool replaceFile(const QString &filename, const QByteArray &data);
    bool addFile(const QString &filename, const QByteArray &data);
    bool removeFile(const QString &filename);

    // Accessors
    const QVector<RezFileEntry>& getFiles() const { return entries; }
    const RezFileEntry* getFileEntry(const QString &filename) const;
    int getFileCount() const { return fileCount; }
    QString getLastError() const { return lastError; }

    // Utility functions
    static quint32 extensionToUInt(const QString &ext);
    static QString uintToExtension(quint32 extDword);

    // Path normalization for cross-platform
    static QString normalizePath(const QString &path);

    QString getArchivePath() const { return archivePath; }

private:
    // Parsing
    bool parseDirectoryRecursive(QFile &file, quint32 offset, quint32 size, const QString &path);

    // Saving
    bool saveInternal(const QString &filepath, const QMap<QString, QByteArray> &modifiedFiles);
    QByteArray buildDirectoryRecursive(const QVector<RezFileEntry> &allEntries, const QString &path);

    // Lookup helper - case-insensitive, handles with/without extension
    int findFileIndex(const QString &path) const;

    // Data
    RezHeader header;
    QVector<RezFileEntry> entries;
    QVector<PendingDir> pendingDirs;
    QMap<QString, int> fileIndex;           // Key: lowercase fullPath (without ext)
    QMap<QString, int> fileIndexWithExt;    // Key: lowercase fullPath WITH extension
    QString archivePath;
    QString lastError;
    int fileCount;
};

#endif // REZARCHIVE_H
