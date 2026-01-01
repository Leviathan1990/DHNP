#ifndef LTBFORMAT_H
#define LTBFORMAT_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <QVector3D>
#include <QVector2D>
#include <cstdint>
#include <cstdio>

// LTB Version
enum class LTBVersion : uint16_t
{
    Unknown = 0,
    Version1 = 1,
    Version2 = 2,
    Version3 = 3,
    Version4 = 4
};

// Geometry types
enum class LTBGeomType : uint32_t
{
    Unknown = 0,
    RigidMesh = 4,
    SkelMesh = 5,
    VAMesh = 6
};

// Vertex structure
struct LTBVertex
{
    QVector3D position;
    QVector3D normal;
    QVector2D uv;

    // Bone data (for skeletal meshes)
    uint8_t boneIndices[4] = {0, 0, 0, 0};
    float boneWeights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

// Triangle structure
struct LTBTriangle
{
    uint16_t indices[3];
};

// Piece (mesh) structure
struct LTBPiece
{
    QString name;
    LTBGeomType geomType = LTBGeomType::Unknown;

    // Material
    uint16_t textureIndex = 0;
    float specularPower = 0.0f;
    float specularScale = 0.0f;

    // Geometry
    QVector<LTBVertex> vertices;
    QVector<LTBTriangle> triangles;

    // LOD info
    uint32_t lodLevel = 0;
};

// Skeleton node
struct LTBNode
{
    QString name;
    uint16_t index = 0;
    uint16_t parentIndex = 0xFFFF;
    QVector3D position;
    QVector3D rotation;  // Euler angles
    QVector3D scale;
    float bindMatrix[16];
};

// Animation keyframe
struct LTBKeyframe
{
    uint32_t time;
    QVector3D position;
    QVector3D rotation;
};

// Animation
struct LTBAnimation
{
    QString name;
    uint32_t duration = 0;
    QVector<QVector<LTBKeyframe>> nodeKeyframes;  // Per node
};

// Main LTB class
class LTBFormat
{
public:
    LTBFormat();
    ~LTBFormat();

    // Loading
    bool load(const QString &filename);
    bool loadFromMemory(const QByteArray &data);

    // Export
    bool exportToOBJ(const QString &objPath, const QString &mtlPath = QString()) const;
    bool exportMTL(const QString &mtlPath, const QStringList &textureNames = QStringList()) const;

    // Getters
    bool isValid() const { return m_valid; }
    QString getLastError() const { return m_lastError; }

    LTBVersion getVersion() const { return m_version; }
    QString getVersionString() const;
    QString getCommandString() const { return m_commandString; }

    int getPieceCount() const { return m_pieces.size(); }
    int getNodeCount() const { return m_nodes.size(); }
    int getAnimationCount() const { return m_animations.size(); }

    const LTBPiece* getPiece(int index) const;
    const LTBNode* getNode(int index) const;
    const LTBAnimation* getAnimation(int index) const;

    // Statistics
    int getTotalVertexCount() const;
    int getTotalTriangleCount() const;

private:
    // Parsing helpers
    QString readLengthPrefixedString(const uint8_t *&ptr, const uint8_t *end);
    bool findSection(const uint8_t *&ptr, const uint8_t *end, const QString &name, uint32_t &size);

    // Section parsers
    bool parseHeader(const uint8_t *ptr, const uint8_t *end);
    bool parsePiecesSection(const uint8_t *ptr, const uint8_t *end);
    bool parseRigidMesh(const uint8_t *&ptr, const uint8_t *end, LTBPiece &piece);
    bool parseSkelMesh(const uint8_t *&ptr, const uint8_t *end, LTBPiece &piece);
    bool parseNodesSection(const uint8_t *ptr, const uint8_t *end);
    bool parseAnimationSection(const uint8_t *ptr, const uint8_t *end);

    // Read helpers
    template<typename T>
    T read(const uint8_t *&ptr)
    {
        T value = *reinterpret_cast<const T*>(ptr);
        ptr += sizeof(T);
        return value;
    }

    // Data
    QByteArray m_rawData;
    LTBVersion m_version = LTBVersion::Unknown;
    QString m_commandString;

    QVector<LTBPiece> m_pieces;
    QVector<LTBNode> m_nodes;
    QVector<LTBAnimation> m_animations;
    QStringList m_textures;
    QStringList m_childModels;

    bool m_valid = false;
    QString m_lastError;
};

#endif // LTBFORMAT_H
