#include "Ltbformat.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDebug>
#include <cstring>
#include <cmath>
#include <cstdio>

LTBFormat::LTBFormat()
{
}

LTBFormat::~LTBFormat()
{
}

bool LTBFormat::load(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = "Could not open file: " + filename;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    return loadFromMemory(data);
}

QString LTBFormat::readLengthPrefixedString(const uint8_t *&ptr, const uint8_t *end)
{
    if (ptr + 2 > end) return QString();

    uint16_t len = ptr[0] | (ptr[1] << 8);
    ptr += 2;

    if (len == 0 || len > 1024 || ptr + len > end) {
        return QString();
    }

    QString result = QString::fromLatin1(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    return result;
}

bool LTBFormat::loadFromMemory(const QByteArray &data)
{
    fprintf(stderr, "=== LTB loadFromMemory START === size: %d\n", data.size());
    fflush(stderr);

    m_valid = false;
    m_pieces.clear();
    m_nodes.clear();
    m_animations.clear();
    m_textures.clear();
    m_childModels.clear();
    m_lastError.clear();
    m_commandString.clear();
    m_rawData = data;

    if (data.size() < 64) {
        m_lastError = "File too small for LTB format";
        return false;
    }

    const uint8_t *start = reinterpret_cast<const uint8_t*>(data.constData());
    const uint8_t *ptr = start;
    const uint8_t *end = start + data.size();

    // Debug: show first 80 bytes
    fprintf(stderr, "First 80 bytes:\n");
    for (int i = 0; i < 80 && i < data.size(); i++) {
        fprintf(stderr, "%02x ", (uint8_t)data[i]);
        if ((i + 1) % 16 == 0) fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
    fflush(stderr);

    // Read LTBHeader section
    QString sectionName = readLengthPrefixedString(ptr, end);
    fprintf(stderr, "Section: '%s' at offset 0\n", sectionName.toUtf8().constData());
    fflush(stderr);

    if (sectionName != "LTBHeader") {
        m_lastError = "Not a valid LTB file - missing LTBHeader";
        return false;
    }

    uint32_t ltbHeaderSize = read<uint32_t>(ptr);
    fprintf(stderr, "LTBHeader data size: %u\n", ltbHeaderSize);
    fflush(stderr);

    // Read version (first 2 bytes of LTBHeader data)
    m_version = static_cast<LTBVersion>(read<uint16_t>(ptr));
    fprintf(stderr, "Version: %d\n", static_cast<int>(m_version));
    fflush(stderr);

    // Skip rest of version-related data (18 more bytes to reach offset 35)
    // Based on hexdump: version at 15, Header starts at 35
    // So skip 35 - 15 - 2 = 18 bytes
    ptr += 18;
    fprintf(stderr, "Now at offset: %ld\n", (long)(ptr - start));
    fflush(stderr);

    // Now we should be at "Header" section (offset 35)
    fprintf(stderr, "Next 20 bytes:\n");
    for (int i = 0; i < 20 && ptr + i < end; i++) {
        fprintf(stderr, "%02x ", ptr[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);

    // Read "Header" section
    sectionName = readLengthPrefixedString(ptr, end);
    fprintf(stderr, "Section: '%s' at offset %ld\n", sectionName.toUtf8().constData(), (long)(ptr - start - sectionName.length() - 2));
    fflush(stderr);

    if (sectionName == "Header") {
        uint32_t headerSize = read<uint32_t>(ptr);
        fprintf(stderr, "Header section size: %u\n", headerSize);
        fflush(stderr);
        ptr += headerSize;
        fprintf(stderr, "After Header, at offset: %ld\n", (long)(ptr - start));
        fflush(stderr);
    }

    // Show next bytes
    fprintf(stderr, "Next 20 bytes:\n");
    for (int i = 0; i < 20 && ptr + i < end; i++) {
        fprintf(stderr, "%02x ", ptr[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);

    // Read "Pieces" section
    sectionName = readLengthPrefixedString(ptr, end);
    fprintf(stderr, "Section: '%s' at offset %ld\n", sectionName.toUtf8().constData(), (long)(ptr - start - sectionName.length() - 2));
    fflush(stderr);

    if (sectionName == "Pieces") {
        uint32_t piecesSize = read<uint32_t>(ptr);
        fprintf(stderr, "Pieces section size: %u\n", piecesSize);
        fflush(stderr);

        const uint8_t *piecesEnd = ptr + piecesSize;
        if (piecesEnd > end) piecesEnd = end;

        fprintf(stderr, "Pieces data first 64 bytes:\n");
        for (int i = 0; i < 64 && ptr + i < piecesEnd; i++) {
            fprintf(stderr, "%02x ", ptr[i]);
            if ((i + 1) % 16 == 0) fprintf(stderr, "\n");
        }
        fprintf(stderr, "\n");
        fflush(stderr);

        parsePiecesSection(ptr, piecesEnd);
        ptr = piecesEnd;
    }

    // Read "Nodes" section
    if (ptr + 2 < end) {
        sectionName = readLengthPrefixedString(ptr, end);
        fprintf(stderr, "Section: '%s'\n", sectionName.toUtf8().constData());
        fflush(stderr);

        if (sectionName == "Nodes") {
            uint32_t nodesSize = read<uint32_t>(ptr);
            const uint8_t *nodesEnd = ptr + nodesSize;
            if (nodesEnd > end) nodesEnd = end;
            parseNodesSection(ptr, nodesEnd);
        }
    }

    m_valid = true;
    fprintf(stderr, "=== LTB DONE === %d pieces, %d verts, %d tris\n",
            m_pieces.size(), getTotalVertexCount(), getTotalTriangleCount());
    fflush(stderr);
    return true;
}

bool LTBFormat::parsePiecesSection(const uint8_t *ptr, const uint8_t *end)
{
    fprintf(stderr, "--- parsePiecesSection ---\n");
    fflush(stderr);

    if (ptr + 4 > end) return false;

    uint32_t numPieces = read<uint32_t>(ptr);
    fprintf(stderr, "Num pieces: %u\n", numPieces);
    fflush(stderr);

    if (numPieces > 1000 || numPieces == 0) {
        m_lastError = "Invalid piece count: " + QString::number(numPieces);
        return false;
    }

    for (uint32_t i = 0; i < numPieces && ptr < end; i++) {
        fprintf(stderr, "\n--- Piece %u ---\n", i);
        fflush(stderr);

        LTBPiece piece;

        piece.name = readLengthPrefixedString(ptr, end);
        fprintf(stderr, "  Name: '%s'\n", piece.name.toUtf8().constData());
        fflush(stderr);

        if (piece.name.isEmpty()) break;

        piece.geomType = static_cast<LTBGeomType>(read<uint32_t>(ptr));
        fprintf(stderr, "  GeomType: %d\n", static_cast<int>(piece.geomType));
        fflush(stderr);

        uint32_t geomDataSize = read<uint32_t>(ptr);
        fprintf(stderr, "  GeomDataSize: %u\n", geomDataSize);
        fflush(stderr);

        const uint8_t *geomEnd = ptr + geomDataSize;
        if (geomEnd > end) geomEnd = end;

        // Show geometry data
        fprintf(stderr, "  Geom data (first 96 bytes):\n  ");
        for (int j = 0; j < 96 && ptr + j < geomEnd; j++) {
            fprintf(stderr, "%02x ", ptr[j]);
            if ((j + 1) % 16 == 0) fprintf(stderr, "\n  ");
        }
        fprintf(stderr, "\n");
        fflush(stderr);

        bool success = false;
        switch (piece.geomType) {
        case LTBGeomType::RigidMesh:
            success = parseRigidMesh(ptr, geomEnd, piece);
            break;
        case LTBGeomType::SkelMesh:
            success = parseSkelMesh(ptr, geomEnd, piece);
            break;
        default:
            fprintf(stderr, "  Unknown geom type, skipping\n");
            success = true;
            break;
        }

        ptr = geomEnd;

        fprintf(stderr, "  Result: %s (%d verts, %d tris)\n",
                success ? "OK" : "FAIL", (int)piece.vertices.size(), (int)piece.triangles.size());
        fflush(stderr);

        m_pieces.append(piece);
    }

    return m_pieces.size() > 0;
}

bool LTBFormat::parseRigidMesh(const uint8_t *&ptr, const uint8_t *end, LTBPiece &piece)
{
    fprintf(stderr, "  --- parseRigidMesh ---\n");
    fflush(stderr);

    const uint8_t *geomStart = ptr;
    size_t geomSize = end - ptr;

    // LithTech 1.x RigidMesh format:
    // Based on hexdump analysis, try to find vertex/triangle counts

    // Skip material data (28 bytes = 7 floats)
    if (ptr + 28 > end) return false;

    piece.specularPower = read<float>(ptr);
    piece.specularScale = read<float>(ptr);
    fprintf(stderr, "  Specular: %.2f, %.2f\n", piece.specularPower, piece.specularScale);
    ptr += 20;  // Skip 5 more floats

    // Skip padding (usually 0xCD or 0x00 patterns)
    // Look for the vertex count - scan for reasonable values
    fprintf(stderr, "  Scanning for vertex count...\n");
    fflush(stderr);

    // Try multiple offsets to find vertex count
    const uint8_t *scanPtr = geomStart;
    uint32_t numVerts = 0;
    uint32_t numTris = 0;
    int foundOffset = -1;

    // Try different offsets where vertex count might be
    int offsets[] = {88, 92, 96, 100, 104, 108, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72, 76, 80, 84};

    for (int off : offsets) {
        if (geomStart + off + 8 > end) continue;

        uint32_t v = *(uint32_t*)(geomStart + off);
        uint32_t t = *(uint32_t*)(geomStart + off + 4);

        // Check if these look like valid counts
        // Vertex count should be reasonable (10-50000)
        // Triangle count should be reasonable and roughly related to vertex count
        if (v >= 3 && v <= 50000 && t >= 1 && t <= 100000) {
            // Check if data size makes sense: v*32 (vertex data) + t*6 (index data) should fit
            size_t neededSize = off + 8 + v * 32 + t * 6;
            if (neededSize <= geomSize + 100) {  // Allow some slack
                fprintf(stderr, "  Candidate at offset %d: verts=%u, tris=%u (needs ~%zu bytes, have %zu)\n",
                        off, v, t, neededSize, geomSize);
                fflush(stderr);

                if (foundOffset < 0 || (v > 10 && t > 10)) {
                    numVerts = v;
                    numTris = t;
                    foundOffset = off;
                }
            }
        }
    }

    if (foundOffset < 0) {
        fprintf(stderr, "  ERROR: Could not find valid vertex/triangle counts\n");
        fflush(stderr);
        return false;
    }

    fprintf(stderr, "  Using offset %d: %u verts, %u tris\n", foundOffset, numVerts, numTris);
    fflush(stderr);

    // Position ptr after the counts
    ptr = geomStart + foundOffset + 8;

    // Read vertices (32 bytes each: pos + norm + uv)
    if (ptr + numVerts * 32 > end) {
        fprintf(stderr, "  ERROR: Not enough data for vertices\n");
        fflush(stderr);
        return false;
    }

    piece.vertices.resize(numVerts);
    for (uint32_t v = 0; v < numVerts; v++) {
        LTBVertex &vert = piece.vertices[v];
        vert.position.setX(read<float>(ptr));
        vert.position.setY(read<float>(ptr));
        vert.position.setZ(read<float>(ptr));
        vert.normal.setX(read<float>(ptr));
        vert.normal.setY(read<float>(ptr));
        vert.normal.setZ(read<float>(ptr));
        vert.uv.setX(read<float>(ptr));
        vert.uv.setY(read<float>(ptr));
    }

    // Show first vertex
    if (numVerts > 0) {
        fprintf(stderr, "  First vertex: (%.3f, %.3f, %.3f)\n",
                piece.vertices[0].position.x(),
                piece.vertices[0].position.y(),
                piece.vertices[0].position.z());
        fflush(stderr);
    }

    // Read triangles (6 bytes each: 3 x uint16)
    if (ptr + numTris * 6 > end) {
        fprintf(stderr, "  WARNING: Not enough data for all triangles\n");
        fflush(stderr);
        numTris = (end - ptr) / 6;
    }

    piece.triangles.resize(numTris);
    for (uint32_t t = 0; t < numTris; t++) {
        piece.triangles[t].indices[0] = read<uint16_t>(ptr);
        piece.triangles[t].indices[1] = read<uint16_t>(ptr);
        piece.triangles[t].indices[2] = read<uint16_t>(ptr);
    }

    fprintf(stderr, "  parseRigidMesh SUCCESS: %d verts, %d tris\n",
            (int)piece.vertices.size(), (int)piece.triangles.size());
    fflush(stderr);
    return true;
}

bool LTBFormat::parseSkelMesh(const uint8_t *&ptr, const uint8_t *end, LTBPiece &piece)
{
    return parseRigidMesh(ptr, end, piece);
}

bool LTBFormat::parseNodesSection(const uint8_t *ptr, const uint8_t *end)
{
    if (ptr + 4 > end) return false;

    uint32_t numNodes = read<uint32_t>(ptr);
    if (numNodes > 1000) return false;

    fprintf(stderr, "Parsing %u nodes\n", numNodes);
    fflush(stderr);

    for (uint32_t i = 0; i < numNodes && ptr < end; i++) {
        LTBNode node;
        node.name = readLengthPrefixedString(ptr, end);
        if (ptr + 40 > end) break;

        node.index = read<uint16_t>(ptr);
        node.parentIndex = read<uint16_t>(ptr);
        node.position.setX(read<float>(ptr));
        node.position.setY(read<float>(ptr));
        node.position.setZ(read<float>(ptr));
        node.rotation.setX(read<float>(ptr));
        node.rotation.setY(read<float>(ptr));
        node.rotation.setZ(read<float>(ptr));

        m_nodes.append(node);
    }

    return true;
}

bool LTBFormat::parseAnimationSection(const uint8_t *ptr, const uint8_t *end)
{
    if (ptr + 4 > end) return false;
    uint32_t numAnims = read<uint32_t>(ptr);
    return true;
}

QString LTBFormat::getVersionString() const
{
    switch (m_version) {
    case LTBVersion::Version1: return "1 (LithTech 1.x)";
    case LTBVersion::Version2: return "2 (LithTech 2.x)";
    case LTBVersion::Version3: return "3 (LithTech Talon)";
    case LTBVersion::Version4: return "4 (LithTech Jupiter)";
    default: return QString("Unknown (%1)").arg(static_cast<int>(m_version));
    }
}

const LTBPiece* LTBFormat::getPiece(int index) const
{
    if (index >= 0 && index < m_pieces.size()) return &m_pieces[index];
    return nullptr;
}

const LTBNode* LTBFormat::getNode(int index) const
{
    if (index >= 0 && index < m_nodes.size()) return &m_nodes[index];
    return nullptr;
}

const LTBAnimation* LTBFormat::getAnimation(int index) const
{
    if (index >= 0 && index < m_animations.size()) return &m_animations[index];
    return nullptr;
}

int LTBFormat::getTotalVertexCount() const
{
    int total = 0;
    for (const LTBPiece &piece : m_pieces) total += piece.vertices.size();
    return total;
}

int LTBFormat::getTotalTriangleCount() const
{
    int total = 0;
    for (const LTBPiece &piece : m_pieces) total += piece.triangles.size();
    return total;
}

bool LTBFormat::exportToOBJ(const QString &objPath, const QString &mtlPath) const
{
    if (!m_valid || m_pieces.isEmpty()) return false;

    QFile objFile(objPath);
    if (!objFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&objFile);
    out.setRealNumberPrecision(6);
    out.setRealNumberNotation(QTextStream::FixedNotation);

    out << "# LTB Model exported by Die Hard: Nakatomi Plaza Modding Tools\n";
    out << "# Pieces: " << m_pieces.size() << "\n";
    out << "# Total Vertices: " << getTotalVertexCount() << "\n";
    out << "# Total Triangles: " << getTotalTriangleCount() << "\n\n";

    if (!mtlPath.isEmpty()) {
        QFileInfo mtlInfo(mtlPath);
        out << "mtllib " << mtlInfo.fileName() << "\n\n";
    }

    int vertexOffset = 1;

    for (int p = 0; p < m_pieces.size(); p++) {
        const LTBPiece &piece = m_pieces[p];

        out << "# Piece " << p << ": " << piece.name << "\n";
        out << "o " << piece.name << "\n";
        if (!mtlPath.isEmpty()) out << "usemtl " << piece.name << "\n";

        for (const LTBVertex &v : piece.vertices) {
            out << "v " << v.position.x() << " " << v.position.y() << " " << v.position.z() << "\n";
        }

        for (const LTBVertex &v : piece.vertices) {
            out << "vt " << v.uv.x() << " " << (1.0f - v.uv.y()) << "\n";
        }

        for (const LTBVertex &v : piece.vertices) {
            out << "vn " << v.normal.x() << " " << v.normal.y() << " " << v.normal.z() << "\n";
        }

        for (const LTBTriangle &tri : piece.triangles) {
            int i0 = tri.indices[0] + vertexOffset;
            int i1 = tri.indices[1] + vertexOffset;
            int i2 = tri.indices[2] + vertexOffset;
            out << "f " << i0 << "/" << i0 << "/" << i0
                << " " << i1 << "/" << i1 << "/" << i1
                << " " << i2 << "/" << i2 << "/" << i2 << "\n";
        }

        out << "\n";
        vertexOffset += piece.vertices.size();
    }

    objFile.close();
    if (!mtlPath.isEmpty()) exportMTL(mtlPath);
    return true;
}

bool LTBFormat::exportMTL(const QString &mtlPath, const QStringList &textureNames) const
{
    QFile mtlFile(mtlPath);
    if (!mtlFile.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&mtlFile);
    out << "# MTL file exported by Die Hard: Nakatomi Plaza Modding Tools\n\n";

    for (int p = 0; p < m_pieces.size(); p++) {
        const LTBPiece &piece = m_pieces[p];
        out << "newmtl " << piece.name << "\n";
        out << "Ka 0.2 0.2 0.2\n";
        out << "Kd 0.8 0.8 0.8\n";
        out << "Ks 0.1 0.1 0.1\n";
        out << "Ns 32\n";
        out << "d 1.0\n";
        out << "illum 2\n";
        if (p < textureNames.size() && !textureNames[p].isEmpty()) {
            out << "map_Kd " << textureNames[p] << "\n";
        }
        out << "\n";
    }

    mtlFile.close();
    return true;
}
