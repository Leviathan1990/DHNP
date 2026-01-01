#include "mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QPixmap>
#include <QDateTime>
#include <QApplication>
#include <QStyle>
#include <QFileInfo>
#include <QFont>
#include <QMenu>
#include <QDir>
#include <cstdio>

//  Custom headers
#include "Dtxformat.h"
#include "Ltbformat.h"
#include "pcxformat.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), archive(nullptr), isModified(false)
{
    setupUI();
    setupMenuBar();
    setupToolBar();

    setWindowTitle("Die Hard: Nakatomi Plaza - Modding Tools V1.0");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    delete archive;
}

void MainWindow::setupUI()
{
    // Central widget with splitter
    mainSplitter = new QSplitter(Qt::Horizontal, this);

    // ========== LEFT PANEL ==========
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // Search box
    searchBox = new QLineEdit();
    searchBox->setPlaceholderText("Search files...");
    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::searchFiles);

    // File tree
    fileTree = new QTreeWidget();
    fileTree->setHeaderLabels({"Name", "Type", "Size", "Date"});
    fileTree->setColumnWidth(0, 300);
    fileTree->setColumnWidth(1, 80);
    fileTree->setColumnWidth(2, 100);
    fileTree->setSortingEnabled(true);
    fileTree->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(fileTree, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onFileDoubleClicked);
    connect(fileTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onFileSelectionChanged);

    leftLayout->addWidget(searchBox);
    leftLayout->addWidget(fileTree);

    //  Context menu
    connect(fileTree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);

    // ========== RIGHT PANEL (Tab Widget) ==========
    tabWidget = new QTabWidget();

    // --- Text Tab ---
    textTab = new QWidget();
    QVBoxLayout *textLayout = new QVBoxLayout(textTab);
    textLayout->setContentsMargins(4, 4, 4, 4);

    textPreview = new QPlainTextEdit();
    textPreview->setReadOnly(true);
    textPreview->setFont(QFont("Monospace", 10));
    textPreview->setLineWrapMode(QPlainTextEdit::NoWrap);
    textPreview->setPlaceholderText("Select a text file to view its contents...");

    textLayout->addWidget(textPreview);
    tabWidget->addTab(textTab, "Text");

    // --- Image Tab ---
    imageTab = new QWidget();
    QVBoxLayout *imageLayout = new QVBoxLayout(imageTab);
    imageLayout->setContentsMargins(4, 4, 4, 4);

    // Export button at top
    QHBoxLayout *imageToolbar = new QHBoxLayout();
    exportImageButton = new QPushButton("Export Image...");
    exportImageButton->setEnabled(false);
    exportImageButton->setIcon(QIcon::fromTheme("document-save-as"));
    connect(exportImageButton, &QPushButton::clicked, this, &MainWindow::exportCurrentImage);
    imageToolbar->addWidget(exportImageButton);
    imageToolbar->addStretch();
    imageLayout->addLayout(imageToolbar);

    imageScrollArea = new QScrollArea();
    imageScrollArea->setWidgetResizable(true);
    imageScrollArea->setAlignment(Qt::AlignCenter);

    imagePreview = new QLabel();
    imagePreview->setAlignment(Qt::AlignCenter);
    imagePreview->setText("Select an image file to view...");
    imagePreview->setStyleSheet("QLabel { background-color: #2d2d2d; color: #888; }");

    imageScrollArea->setWidget(imagePreview);
    imageLayout->addWidget(imageScrollArea);
    tabWidget->addTab(imageTab, "Image");

    // --- Info Tab ---
    infoTab = new QWidget();
    QVBoxLayout *infoLayout = new QVBoxLayout(infoTab);
    infoLayout->setContentsMargins(4, 4, 4, 4);

    fileInfoLabel = new QLabel();
    fileInfoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    fileInfoLabel->setWordWrap(true);
    fileInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileInfoLabel->setText("Select a file to view information...");
    fileInfoLabel->setStyleSheet("QLabel { padding: 10px; }");

    infoLayout->addWidget(fileInfoLabel);
    infoLayout->addStretch();
    tabWidget->addTab(infoTab, "Info");

    // ========== ADD TO SPLITTER ==========
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(tabWidget);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({500, 700});

    setCentralWidget(mainSplitter);

    // Status bar
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    statusBar()->addPermanentWidget(progressBar);
    statusBar()->showMessage("Ready");
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");

    actionOpen = fileMenu->addAction("&Open REZ Archive...");
    actionOpen->setShortcut(QKeySequence::Open);
    connect(actionOpen, &QAction::triggered, this, &MainWindow::openArchive);

    actionSave = fileMenu->addAction("&Save Archive");
    actionSave->setShortcut(QKeySequence::Save);
    actionSave->setEnabled(false);
    connect(actionSave, &QAction::triggered, this, &MainWindow::saveArchive);

    actionClose = fileMenu->addAction("&Close Archive");
    actionClose->setEnabled(false);
    connect(actionClose, &QAction::triggered, this, &MainWindow::closeArchive);

    fileMenu->addSeparator();

    QAction *actionExit = fileMenu->addAction("E&xit");
    actionExit->setShortcut(QKeySequence::Quit);
    connect(actionExit, &QAction::triggered, this, &QMainWindow::close);

    QMenu *extractMenu = menuBar()->addMenu("&Extract");

    actionExtractFile = extractMenu->addAction("Extract &Selected File...");
    actionExtractFile->setShortcut(Qt::CTRL | Qt::Key_E);
    actionExtractFile->setEnabled(false);
    connect(actionExtractFile, &QAction::triggered, this, &MainWindow::extractSelectedFile);

    actionExtractAll = extractMenu->addAction("Extract &All Files...");
    actionExtractAll->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_E);
    actionExtractAll->setEnabled(false);
    connect(actionExtractAll, &QAction::triggered, this, &MainWindow::extractAllFiles);

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *actionAbout = helpMenu->addAction("&About");
    connect(actionAbout, &QAction::triggered, this, [this]()
            {
                QMessageBox::about(this, tr("About DHNP tool."),
                                   tr("<h3>Die Hard: Nakatomi Plaza - Modding Tools V1.0 </h3>"
                                      "<p>A complete modding tool.</p>"
                                      "<p><b>Features:</p></b>"

                                      "<ul>"

                                      "<li>Rez archive manager - Browse all assets</li>"
                                      "<li>Image Inspector with DTX, PCX Viewer - converter - Exporter</li>"
                                      "<li>Nakatomi script reader</li>"
                                      "<li>LTB parser -> .obj, .mtl, textures exporter</li>"

                                      "</ul>"

                                      "<p>Developer: Krisztian Kispeti</p>"
                                      "<p>Company: K's Interactive. </p>"
                                      ));
            });
}

void MainWindow::setupToolBar()
{
    QToolBar *toolbar = addToolBar("Main Toolbar");
    toolbar->setMovable(false);

    toolbar->addAction(actionOpen);
    toolbar->addAction(actionSave);
    toolbar->addSeparator();
    toolbar->addAction(actionExtractFile);
    toolbar->addAction(actionExtractAll);
}

QString MainWindow::toNativePath(const QString &rezPath)
{
    QString path = rezPath;
    path.replace('\\', '/');
    return path;
}

QIcon MainWindow::getIconForExtension(const QString &ext)
{
    QString e = ext.toLower();

    if (e == "dtx" || e == "bmp" || e == "tga" || e == "png" || e == "jpg" || e == "jpeg" || e == "pcx") {
        return style()->standardIcon(QStyle::SP_FileIcon);
    } else if (e == "wav" || e == "mp3" || e == "ogg") {
        return style()->standardIcon(QStyle::SP_MediaVolume);
    } else if (e == "txt" || e == "cfg" || e == "ini" || e == "dat") {
        return style()->standardIcon(QStyle::SP_FileDialogDetailedView);
    } else {
        return style()->standardIcon(QStyle::SP_FileIcon);
    }
}

void MainWindow::openArchive()
{
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Open REZ Archive",
        "",
        "REZ Archives (*.rez);;All Files (*.*)"
        );

    if (filename.isEmpty())
        return;

    if (archive) {
        closeArchive();
    }

    archive = new RezArchive();

    statusBar()->showMessage("Loading archive...");
    progressBar->setVisible(true);
    progressBar->setRange(0, 0);

    if (!archive->load(filename)) {
        QMessageBox::critical(this, "Error", "Failed to load REZ archive:\n" + archive->getLastError());
        delete archive;
        archive = nullptr;
        progressBar->setVisible(false);
        statusBar()->showMessage("Failed to load archive", 3000);
        return;
    }

    currentFilePath = filename;
    loadArchiveToTree();

    actionSave->setEnabled(true);
    actionClose->setEnabled(true);
    actionExtractAll->setEnabled(true);

    progressBar->setVisible(false);
    statusBar()->showMessage(QString("Loaded: %1 (%2 files)")
                                 .arg(QFileInfo(filename).fileName())
                                 .arg(archive->getFileCount()), 5000);
}

void MainWindow::loadArchiveToTree()
{
    fileTree->clear();

    if (!archive)
        return;

    QMap<QString, QTreeWidgetItem*> dirItems;
    QTreeWidgetItem *root = fileTree->invisibleRootItem();

    const QVector<RezFileEntry> &files = archive->getFiles();

    for (const RezFileEntry &entry : files) {
        if (!entry.isFile())
            continue;

        if (entry.filename.isEmpty())
            continue;

        QString pathWithExt = entry.getFullPathWithExt().toUpper();
        QStringList parts = pathWithExt.split('\\', Qt::SkipEmptyParts);

        if (parts.isEmpty())
            continue;

        QTreeWidgetItem *parent = root;
        QString currentPath;

        for (int i = 0; i < parts.size() - 1; ++i) {
            currentPath += parts[i] + "\\";
            QString lowerPath = currentPath.toLower();

            if (!dirItems.contains(lowerPath)) {
                QTreeWidgetItem *dirItem = new QTreeWidgetItem(parent);
                dirItem->setText(0, parts[i]);
                dirItem->setText(1, "Folder");
                dirItem->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
                dirItems[lowerPath] = dirItem;
                parent = dirItem;
            } else {
                parent = dirItems[lowerPath];
            }
        }

        QTreeWidgetItem *fileItem = new QTreeWidgetItem(parent);
        QString displayName = parts.last();

        fileItem->setText(0, displayName);

        QString ext = entry.getExtension().toUpper();
        fileItem->setText(1, ext.isEmpty() ? "FILE" : ext);
        fileItem->setText(2, formatFileSize(entry.size));
        fileItem->setText(3, formatDateTime(entry.dateTime));

        fileItem->setData(0, Qt::UserRole, entry.fullPath);
        fileItem->setIcon(0, getIconForExtension(ext));
    }

    fileTree->expandToDepth(0);
}

void MainWindow::onFileDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    if (!archive || !item)
        return;

    QString filename = item->data(0, Qt::UserRole).toString();

    if (filename.isEmpty())
        return;

    const RezFileEntry *entry = archive->getFileEntry(filename);
    if (!entry)
        return;

    // Extract file data directly
    QFile archiveFile(archive->getArchivePath());
    QByteArray data;

    if (archiveFile.open(QIODevice::ReadOnly)) {
        if (archiveFile.seek(entry->offset)) {
            data = archiveFile.read(entry->size);
        }
        archiveFile.close();
    }

    QString ext = entry->getExtension().toLower();

    // Text files - show in Text tab
    if (ext == "txt" || ext == "cfg" || ext == "ini" || ext == "dat" ||
        ext == "csv" || ext == "log" || ext == "xml" || ext == "html" ||
        ext == "dep" || ext == "lyt") {
        displayTextFile(filename, data);
    }
    // Image files - show in Image tab
    else if (ext == "dtx" || ext == "bmp" || ext == "png" || ext == "jpg" ||
             ext == "jpeg" || ext == "tga" || ext == "pcx") {
        displayImageFile(filename, data);
    }
    // LTB model files - show info in Text tab
    else if (ext == "ltb") {
        displayLTBFile(filename, data);
    }
    // Other files - just show info
    else {
        displayFileInfo(*entry);
        tabWidget->setCurrentWidget(infoTab);
    }
}

void MainWindow::displayTextFile(const QString &filename, const QByteArray &data)
{
    // Convert to text (handle different encodings)
    QString text = QString::fromUtf8(data);
    if (text.contains(QChar::ReplacementCharacter)) {
        // Try Latin1 if UTF-8 fails
        text = QString::fromLatin1(data);
    }

    textPreview->setPlainText(text);
    tabWidget->setCurrentWidget(textTab);

    statusBar()->showMessage(QString("Viewing: %1 (%2 bytes)")
                                 .arg(filename)
                                 .arg(data.size()), 3000);
}

// Add this to mainwindow.cpp - replace the existing displayLTBFile function

void MainWindow::displayLTBFile(const QString &filename, const QByteArray &data)
{
    // DEBUG
    fprintf(stderr, "=== displayLTBFile === file: %s size: %d\n",
            filename.toUtf8().constData(), data.size());
    fflush(stderr);

    const RezFileEntry *entry = archive->getFileEntry(filename);

    LTBFormat ltb;

    fprintf(stderr, "Calling ltb.loadFromMemory...\n");
    fflush(stderr);

    if (ltb.loadFromMemory(data)) {
        fprintf(stderr, "LTB loaded! Pieces: %d Verts: %d Tris: %d\n",
                ltb.getPieceCount(), ltb.getTotalVertexCount(), ltb.getTotalTriangleCount());
        fflush(stderr);

        // Display model info in text preview
        QString info;
        info += QString("=== LTB Model: %1 ===\n\n").arg(filename);
        info += QString("Version: %1\n").arg(ltb.getVersionString());
        info += QString("Pieces: %1\n").arg(ltb.getPieceCount());
        info += QString("Nodes: %1\n").arg(ltb.getNodeCount());
        info += QString("Animations: %1\n").arg(ltb.getAnimationCount());
        info += QString("Total Vertices: %1\n").arg(ltb.getTotalVertexCount());
        info += QString("Total Triangles: %1\n").arg(ltb.getTotalTriangleCount());
        info += QString("File Size: %1 bytes\n\n").arg(data.size());

        if (ltb.getPieceCount() > 0) {
            info += "=== Pieces (Meshes) ===\n";
            for (int i = 0; i < ltb.getPieceCount(); i++) {
                const LTBPiece *piece = ltb.getPiece(i);
                if (piece) {
                    info += QString("  [%1] %2 - %3 verts, %4 tris\n")
                    .arg(i)
                        .arg(piece->name)
                        .arg(piece->vertices.size())
                        .arg(piece->triangles.size());
                }
            }
        }

        if (ltb.getNodeCount() > 0) {
            info += "\n=== Skeleton Nodes ===\n";
            for (int i = 0; i < ltb.getNodeCount(); i++) {
                const LTBNode *node = ltb.getNode(i);
                if (node) {
                    info += QString("  [%1] %2 (parent: %3)\n")
                    .arg(node->index)
                        .arg(node->name)
                        .arg(node->parentIndex == 0xFFFF ? "none" : QString::number(node->parentIndex));
                }
            }
        }

        info += "\n=== Export ===\n";
        info += "Right-click and select 'Export to OBJ...' to export the model.\n";

        textPreview->setPlainText(info);

        // Update info tab
        if (entry) {
            QString htmlInfo;
            htmlInfo += QString("<h3>%1</h3>").arg(entry->getFilenameWithExt().toUpper());
            htmlInfo += "<hr>";
            htmlInfo += QString("<b>Format:</b> LTB (LithTech Binary Model)<br>");
            htmlInfo += QString("<b>Version:</b> %1<br>").arg(ltb.getVersionString());
            htmlInfo += QString("<b>Pieces:</b> %1<br>").arg(ltb.getPieceCount());
            htmlInfo += QString("<b>Total Vertices:</b> %1<br>").arg(ltb.getTotalVertexCount());
            htmlInfo += QString("<b>Total Triangles:</b> %1<br>").arg(ltb.getTotalTriangleCount());
            htmlInfo += QString("<b>Skeleton Nodes:</b> %1<br>").arg(ltb.getNodeCount());
            htmlInfo += QString("<b>Animations:</b> %1<br>").arg(ltb.getAnimationCount());
            htmlInfo += QString("<br><b>File Size:</b> %1 bytes<br>").arg(data.size());

            fileInfoLabel->setText(htmlInfo);
        }

        statusBar()->showMessage(QString("LTB: %1 (%2 pieces, %3 verts, %4 tris)")
                                     .arg(filename)
                                     .arg(ltb.getPieceCount())
                                     .arg(ltb.getTotalVertexCount())
                                     .arg(ltb.getTotalTriangleCount()), 5000);

        tabWidget->setCurrentWidget(textTab);
    } else {
        fprintf(stderr, "LTB load FAILED: %s\n", ltb.getLastError().toUtf8().constData());
        fflush(stderr);

        textPreview->setPlainText(QString("Failed to load LTB: %1\n\nError: %2")
                                      .arg(filename)
                                      .arg(ltb.getLastError()));
        tabWidget->setCurrentWidget(textTab);
    }
}

void MainWindow::displayImageFile(const QString &filename, const QByteArray &data)
{
    const RezFileEntry *entry = archive->getFileEntry(filename);
    QString ext = entry ? entry->getExtension().toLower() : "";

    // For DTX files - use DTXFormat class
    if (ext == "dtx") {
        // Debug: show raw header bytes
        if (data.size() >= 32) {
            qWarning() << "=== DTX DEBUG ===" << filename;
            qWarning() << "File size:" << data.size();
            qWarning() << "First 32 bytes (hex):";
            QString hexDump;
            for (int i = 0; i < 32; i++) {
                hexDump += QString("%1 ").arg((uint8_t)data[i], 2, 16, QChar('0'));
                if (i == 15) hexDump += "\n";
            }
            qWarning().noquote() << hexDump;

            // Parse header manually for debug
            uint32_t resType = *reinterpret_cast<const uint32_t*>(data.constData());
            int32_t version = *reinterpret_cast<const int32_t*>(data.constData() + 4);
            uint16_t width = *reinterpret_cast<const uint16_t*>(data.constData() + 8);
            uint16_t height = *reinterpret_cast<const uint16_t*>(data.constData() + 10);
            uint16_t mipmaps = *reinterpret_cast<const uint16_t*>(data.constData() + 12);
            uint16_t sections = *reinterpret_cast<const uint16_t*>(data.constData() + 14);
            uint32_t flags = *reinterpret_cast<const uint32_t*>(data.constData() + 16);
            uint8_t bppType = (uint8_t)data[0x1A];  // extra[2]

            qWarning() << "  resType:" << resType;
            qWarning() << "  version:" << version << QString("(0x%1)").arg((uint32_t)version, 8, 16, QChar('0'));
            qWarning() << "  width:" << width;
            qWarning() << "  height:" << height;
            qWarning() << "  mipmaps:" << mipmaps;
            qWarning() << "  sections:" << sections;
            qWarning() << "  flags:" << QString("0x%1").arg(flags, 8, 16, QChar('0'));
            qWarning() << "  bppType:" << bppType;
            qWarning() << "================";
        }

        DTXFormat dtx;
        if (dtx.loadFromMemory(data)) {
            QImage img = dtx.toQImage(0);  // Mipmap level 0

            if (!img.isNull()) {
                // Store image for export
                currentImage = img;
                currentImageName = entry->getFilenameWithExt();
                exportImageButton->setEnabled(true);

                QPixmap pixmap = QPixmap::fromImage(img);

                // Display the image
                imagePreview->clear();
                imagePreview->setPixmap(pixmap);
                imagePreview->setMinimumSize(pixmap.size());
                imagePreview->adjustSize();

                // Update info tab with DTX details
                QString info;
                info += QString("<h3>%1</h3>").arg(entry->getFilenameWithExt().toUpper());
                info += "<hr>";
                info += QString("<b>Format:</b> DTX (LithTech Texture)<br>");
                info += QString("<b>Version:</b> %1<br>").arg(DTXFormat::versionToString(dtx.getVersion()));
                info += QString("<b>Dimensions:</b> %1 x %2<br>").arg(dtx.getWidth()).arg(dtx.getHeight());
                info += QString("<b>BPP Type:</b> %1<br>").arg(dtx.getBPPName());
                info += QString("<b>Mipmaps:</b> %1<br>").arg(dtx.getMipmapCount());
                info += QString("<b>Compressed:</b> %1<br>").arg(dtx.isCompressed() ? "Yes" : "No");
                info += QString("<b>Command:</b> %1<br>").arg(dtx.getCommandString());
                info += QString("<br><b>File Size:</b> %1 bytes<br>").arg(data.size());

                fileInfoLabel->setText(info);

                statusBar()->showMessage(QString("DTX: %1 (%2x%3, %4)")
                                             .arg(filename)
                                             .arg(dtx.getWidth())
                                             .arg(dtx.getHeight())
                                             .arg(dtx.getBPPName()), 5000);
            } else {
                exportImageButton->setEnabled(false);
                imagePreview->setText(QString("Failed to decode DTX: %1\n\nError: %2\n\nBPP Type: %3")
                                          .arg(filename)
                                          .arg(dtx.getLastError())
                                          .arg(dtx.getBPPName()));
            }
        } else {
            exportImageButton->setEnabled(false);
            imagePreview->setText(QString("Failed to load DTX: %1\n\nError: %2")
                                      .arg(filename)
                                      .arg(dtx.getLastError()));
        }

        tabWidget->setCurrentWidget(imageTab);
        return;
    }

    // For PCX files - use PCXFormat class
    if (ext == "pcx") {
        PCXFormat pcx;
        if (pcx.loadFromMemory(data)) {
            QImage img = pcx.toQImage();

            if (!img.isNull()) {
                // Store image for export
                currentImage = img;
                currentImageName = entry->getFilenameWithExt();
                exportImageButton->setEnabled(true);

                QPixmap pixmap = QPixmap::fromImage(img);

                // Display the image
                imagePreview->clear();
                imagePreview->setPixmap(pixmap);
                imagePreview->setMinimumSize(pixmap.size());
                imagePreview->adjustSize();

                // Update info tab with PCX details
                QString info;
                info += QString("<h3>%1</h3>").arg(entry->getFilenameWithExt().toUpper());
                info += "<hr>";
                info += QString("<b>Format:</b> PCX (ZSoft Paintbrush)<br>");
                info += QString("<b>Version:</b> %1<br>").arg(pcx.getVersionString());
                info += QString("<b>Dimensions:</b> %1 x %2<br>").arg(pcx.getWidth()).arg(pcx.getHeight());
                info += QString("<b>Color Depth:</b> %1<br>").arg(pcx.getFormatDescription());
                info += QString("<b>Bits/Pixel:</b> %1<br>").arg(pcx.getBitsPerPixel());
                info += QString("<b>Planes:</b> %1<br>").arg(pcx.getNumPlanes());
                info += QString("<br><b>File Size:</b> %1 bytes<br>").arg(data.size());

                fileInfoLabel->setText(info);

                statusBar()->showMessage(QString("PCX: %1 (%2x%3, %4)")
                                             .arg(filename)
                                             .arg(pcx.getWidth())
                                             .arg(pcx.getHeight())
                                             .arg(pcx.getFormatDescription()), 5000);
            } else {
                exportImageButton->setEnabled(false);
                imagePreview->setText(QString("Failed to decode PCX: %1\n\nError: %2")
                                          .arg(filename)
                                          .arg(pcx.getLastError()));
            }
        } else {
            exportImageButton->setEnabled(false);
            imagePreview->setText(QString("Failed to load PCX: %1\n\nError: %2")
                                      .arg(filename)
                                      .arg(pcx.getLastError()));
        }

        tabWidget->setCurrentWidget(imageTab);
        return;
    }

    // For standard image formats (BMP, PNG, JPG, etc.)
    QPixmap pixmap;
    if (pixmap.loadFromData(data))
    {
        // Store image for export
        currentImage = pixmap.toImage();
        currentImageName = filename;
        exportImageButton->setEnabled(true);

        imagePreview->setPixmap(pixmap);
        statusBar()->showMessage(QString("Image: %1 (%2x%3)").arg(filename).arg(pixmap.width()).arg(pixmap.height()), 3000);
    }

    else
    {
        imagePreview->setText(QString("Cannot display image: %1\n\nFormat: %2\nSize: %3 bytes").arg(filename).arg(ext.toUpper()).arg(data.size()));
    }

    tabWidget->setCurrentWidget(imageTab);
}

void MainWindow::displayFileInfo(const RezFileEntry &entry)
{
    QString ext = entry.getExtension().toUpper();

    QString info;
    info += QString("<h3>%1</h3>").arg(entry.getFilenameWithExt().toUpper());
    info += "<hr>";
    info += QString("<b>Full Path:</b> %1<br><br>").arg(entry.getFullPathWithExt().toUpper());
    info += QString("<b>Extension:</b> %1<br>").arg(ext.isEmpty() ? "(none)" : ext);
    info += QString("<b>Size:</b> %1 (%2 bytes)<br>").arg(formatFileSize(entry.size)).arg(entry.size);
    info += QString("<b>Offset:</b> 0x%1<br>").arg(entry.offset, 8, 16, QChar('0')).toUpper();
    info += QString("<b>Date/Time:</b> %1<br>").arg(formatDateTime(entry.dateTime));
    info += QString("<b>File ID:</b> %1<br>").arg(entry.fileId);
    info += QString("<b>Extension DWORD:</b> 0x%1<br>").arg(entry.extensionReversed, 8, 16, QChar('0')).toUpper();

    fileInfoLabel->setText(info);
}

void MainWindow::clearPreviews()
{
    textPreview->clear();
    textPreview->setPlaceholderText("Select a text file to view its contents...");
    imagePreview->clear();
    imagePreview->setText("Select an image file to view...");
    fileInfoLabel->setText("Select a file to view information...");
}

void MainWindow::onFileSelectionChanged()
{
    QList<QTreeWidgetItem*> selected = fileTree->selectedItems();

    if (selected.isEmpty()) {
        actionExtractFile->setEnabled(false);
        fileInfoLabel->setText("Select a file to view information...");
        return;
    }

    QString filename = selected.first()->data(0, Qt::UserRole).toString();

    if (filename.isEmpty()) {
        actionExtractFile->setEnabled(false);
        fileInfoLabel->setText("Directory selected");
        return;
    }

    actionExtractFile->setEnabled(true);

    const RezFileEntry *entry = archive->getFileEntry(filename);
    if (entry)
    {
        displayFileInfo(*entry);
    }
}

void MainWindow::saveArchive()
{
    if (!archive || !isModified)
        return;

    QString backupPath = currentFilePath + ".backup";

    if (QFile::exists(backupPath))
    {
        auto reply = QMessageBox::question(this, "Backup Exists","A backup file already exists. Overwrite it?", QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No)
            return;
    }

    statusBar()->showMessage("Saving archive...");
    progressBar->setVisible(true);
    progressBar->setRange(0, 0);

    if (!archive->save(currentFilePath, modifiedFiles))
    {
        QMessageBox::critical(this, "Error", "Failed to save archive:\n" + archive->getLastError());
        progressBar->setVisible(false);
        statusBar()->showMessage("Save failed", 3000);
        return;
    }

    isModified = false;
    modifiedFiles.clear();
    setWindowTitle("REZ Archive Manager - " + QFileInfo(currentFilePath).fileName());

    progressBar->setVisible(false);
    statusBar()->showMessage("Archive saved successfully", 5000);

    QMessageBox::information(this, "Success", "Archive saved successfully!");
}

void MainWindow::extractSelectedFile()
{
    QList<QTreeWidgetItem*> selected = fileTree->selectedItems();
    if (selected.isEmpty() || !archive)
        return;

    QString filename = selected.first()->data(0, Qt::UserRole).toString();
    if (filename.isEmpty())
        return;

    const RezFileEntry *entry = archive->getFileEntry(filename);
    if (!entry)
    {
        QMessageBox::critical(this, "Error", "File not found in archive");
        return;
    }

    QString suggestedName = entry->getFilenameWithExt().toUpper();

    QString savePath = QFileDialog::getSaveFileName(this, "Extract File", suggestedName, "All Files (*.*)");

    if (savePath.isEmpty())
        return;

    // Extract directly using offset and size
    QFile archiveFile(archive->getArchivePath());
    QByteArray data;

    if (archiveFile.open(QIODevice::ReadOnly))
    {
        if (archiveFile.seek(entry->offset))
        {
            data = archiveFile.read(entry->size);
        }
        archiveFile.close();
    }

    if (data.isEmpty() && entry->size > 0)
    {
        QMessageBox::critical(this, "Error", "Failed to extract file");
        return;
    }

    QFile outFile(savePath);
    if (!outFile.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, "Error", "Failed to write file: " + savePath);
        return;
    }

    outFile.write(data);
    outFile.close();

    statusBar()->showMessage("File extracted: " + entry->getFullPathWithExt().toUpper(), 3000);
}

void MainWindow::extractAllFiles()
{
    if (!archive)
        return;

    QString dir = QFileDialog::getExistingDirectory(this, "Extract All Files To");

    if (dir.isEmpty())
        return;

    const QVector<RezFileEntry> &files = archive->getFiles();

    progressBar->setVisible(true);
    progressBar->setRange(0, files.size());

    int successCount = 0;
    int skipCount = 0;
    int failCount = 0;
    int dirCount = 0;
    int fileIndex = 0;
    quint64 totalBytesWritten = 0;

    for (const RezFileEntry &entry : files)
    {
        progressBar->setValue(fileIndex);

        // Handle DIRECTORIES
        if (entry.isDirectory())
        {
            QString dirPath = entry.fullPath.toUpper();
            dirPath.replace('\\', '/');
            QString outputDirPath = dir + "/" + dirPath;

            if (QDir().mkpath(outputDirPath))
            {
                dirCount++;
            }
            fileIndex++;
            continue;
        }

        // Handle FILES
        if (entry.filename.isEmpty())
        {
            QString dirPath = entry.fullPath.toUpper();

            if (dirPath.endsWith('\\'))
            {
                dirPath.chop(1);
            }

            dirPath.replace('\\', '/');
            QString outputDirPath = dir + "/" + dirPath;
            QDir().mkpath(outputDirPath);
            dirCount++;
            skipCount++;
            fileIndex++;
            continue;
        }

        QString fullPathWithExt = entry.getFullPathWithExt().toUpper();

        if (fullPathWithExt.isEmpty())
        {
            skipCount++;
            fileIndex++;
            continue;
        }

        while (fullPathWithExt.endsWith('\\') || fullPathWithExt.endsWith('/'))
        {
            fullPathWithExt.chop(1);
        }

        statusBar()->showMessage(QString("Extracting %1/%2: %3").arg(fileIndex + 1).arg(files.size()).arg(fullPathWithExt));

        QString relativePath = toNativePath(fullPathWithExt);
        QString outputPath = dir + "/" + relativePath;

        if (outputPath.endsWith('/'))
        {
            skipCount++;
            fileIndex++;
            continue;
        }

        QFileInfo fileInfo(outputPath);
        QDir().mkpath(fileInfo.absolutePath());

        // Extract directly
        QFile archiveFile(archive->getArchivePath());
        QByteArray data;

        if (archiveFile.open(QIODevice::ReadOnly))
        {
            if (archiveFile.seek(entry.offset))
            {
                data = archiveFile.read(entry.size);
            }
            archiveFile.close();
        }

        if (!data.isEmpty() || entry.size == 0)
        {
            QFile outFile(outputPath);
            if (outFile.open(QIODevice::WriteOnly))
            {
                qint64 written = outFile.write(data);
                outFile.close();
                totalBytesWritten += written;
                successCount++;
            }

            else
            {
                failCount++;
            }
        }

        else
        {
            failCount++;
        }

        fileIndex++;
        QApplication::processEvents();
    }

    progressBar->setVisible(false);

    statusBar()->showMessage(QString("Extracted %1 files, %2 directories").arg(successCount).arg(dirCount), 5000);

    QString message = QString("Files extracted: %1\nDirectories created: %2\nSkipped: %3\nFailed: %4\n\nBytes written: %5\n\nOutput: %6")
                          .arg(successCount)
                          .arg(dirCount)
                          .arg(skipCount)
                          .arg(failCount)
                          .arg(totalBytesWritten)
                          .arg(dir);

    QMessageBox::information(this, "Complete", message);
}

void MainWindow::closeArchive()
{
    if (isModified) {
        auto reply = QMessageBox::question(this, "Unsaved Changes",
                                           "You have unsaved changes. Save before closing?",
                                           QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Save) {
            saveArchive();
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    fileTree->clear();
    clearPreviews();

    delete archive;
    archive = nullptr;

    currentFilePath.clear();
    modifiedFiles.clear();
    isModified = false;

    actionSave->setEnabled(false);
    actionClose->setEnabled(false);
    actionExtractAll->setEnabled(false);
    actionExtractFile->setEnabled(false);

    setWindowTitle("REZ Archive Manager");
    statusBar()->showMessage("Archive closed", 3000);
}

void MainWindow::searchFiles()
{
    QString searchText = searchBox->text().toLower().trimmed();

    if (searchText.isEmpty()) {
        // Show all items when search is empty
        QTreeWidgetItemIterator it(fileTree);
        while (*it) {
            (*it)->setHidden(false);
            ++it;
        }
        return;
    }

    // First pass: hide all items
    QTreeWidgetItemIterator it(fileTree);
    while (*it) {
        (*it)->setHidden(true);
        ++it;
    }

    // Second pass: show matching items and their parents
    QTreeWidgetItemIterator it2(fileTree);
    while (*it2) {
        QTreeWidgetItem *item = *it2;
        QString itemText = item->text(0).toLower();

        // Check if this item matches
        if (itemText.contains(searchText)) {
            // Show this item
            item->setHidden(false);

            // Show all parent folders
            QTreeWidgetItem *parent = item->parent();
            while (parent) {
                parent->setHidden(false);
                parent->setExpanded(true);  // Expand to show the match
                parent = parent->parent();
            }
        }
        ++it2;
    }
}

QString MainWindow::formatFileSize(quint32 size)
{
    if (size < 1024)
        return QString::number(size) + " B";
    else if (size < 1024 * 1024)
        return QString::number(size / 1024.0, 'f', 2) + " KB";
    else
        return QString::number(size / (1024.0 * 1024.0), 'f', 2) + " MB";
}

QString MainWindow::formatDateTime(quint32 timestamp)
{
    if (timestamp == 0)
        return "N/A";

    QDateTime dt = QDateTime::fromSecsSinceEpoch(timestamp);
    return dt.toString("yyyy-MM-dd HH:mm:ss");
}

void MainWindow::exportCurrentImage()
{
    if (currentImage.isNull())
    {
        QMessageBox::warning(this, "Export Error", "No image to export.");
        return;
    }

    // Get base filename without extension
    QString baseName = currentImageName;
    int dotPos = baseName.lastIndexOf('.');

    if (dotPos > 0)
    {
        baseName = baseName.left(dotPos);
    }

    // Show save dialog
    QString filter = "PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;BMP Image (*.bmp);;All Files (*)";
    QString defaultPath = QDir::homePath() + "/" + baseName + ".png";

    QString filePath = QFileDialog::getSaveFileName(this, "Export Image", defaultPath, filter);

    if (filePath.isEmpty())
    {
        return;  // User cancelled
    }

    // Determine format from extension
    QString ext = QFileInfo(filePath).suffix().toLower();
    const char *format = nullptr;
    int quality = -1;

    if (ext == "png")
    {
        format = "PNG";
    }

    else if (ext == "jpg" || ext == "jpeg")
    {
        format = "JPEG";
        quality = 95;  // High quality JPEG
    }

    else if (ext == "bmp")
    {
        format = "BMP";
    }

    else
    {
        // Default to PNG if unknown extension
        format = "PNG";
        if (!filePath.endsWith(".png", Qt::CaseInsensitive))
        {
            filePath += ".png";
        }
    }

    // Save the image
    bool success;
    if (quality > 0)
    {
        success = currentImage.save(filePath, format, quality);
    }

    else
    {
        success = currentImage.save(filePath, format);
    }

    if (success)
    {
        statusBar()->showMessage(QString("Image exported to: %1").arg(filePath), 5000);
        QMessageBox::information(this, "Export Successful", QString("Image exported successfully to:\n%1").arg(filePath));
    }

    else
    {
        QMessageBox::critical(this, "Export Failed", QString("Failed to export image to:\n%1").arg(filePath));
    }
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = fileTree->itemAt(pos);
    if (!item || !archive)
        return;

    QString filename = item->data(0, Qt::UserRole).toString();
    if (filename.isEmpty())
        return;

    const RezFileEntry *entry = archive->getFileEntry(filename);
    if (!entry)
        return;

    QString ext = entry->getExtension().toLower();

    QMenu contextMenu(this);

    // Extract action (always available)
    QAction *extractAction = contextMenu.addAction("Extract File...");
    connect(extractAction, &QAction::triggered, this, &MainWindow::extractSelectedFile);

    // LTB-specific actions
    if (ext == "ltb") {
        contextMenu.addSeparator();
        QAction *exportOBJAction = contextMenu.addAction("Export to OBJ...");

        // Store LTB data for export
        QFile archiveFile(archive->getArchivePath());
        if (archiveFile.open(QIODevice::ReadOnly)) {
            if (archiveFile.seek(entry->offset)) {
                currentLTBData = archiveFile.read(entry->size);
                currentLTBName = entry->getFilenameWithExt();
            }
            archiveFile.close();
        }

        connect(exportOBJAction, &QAction::triggered, this, &MainWindow::exportLTBToOBJ);
    }

    contextMenu.exec(fileTree->viewport()->mapToGlobal(pos));
}

void MainWindow::exportLTBToOBJ()
{
    if (currentLTBData.isEmpty()) {
        QMessageBox::warning(this, "Export Error", "No LTB data loaded.");
        return;
    }

    LTBFormat ltb;
    if (!ltb.loadFromMemory(currentLTBData)) {
        QMessageBox::critical(this, "Export Error",
                              QString("Failed to parse LTB: %1").arg(ltb.getLastError()));
        return;
    }

    // Get base filename
    QString baseName = currentLTBName;
    int dotPos = baseName.lastIndexOf('.');
    if (dotPos > 0) {
        baseName = baseName.left(dotPos);
    }

    // Show save dialog
    QString defaultPath = QDir::homePath() + "/" + baseName + ".obj";
    QString objPath = QFileDialog::getSaveFileName(this, "Export to OBJ", defaultPath,
                                                   "Wavefront OBJ (*.obj);;All Files (*)");

    if (objPath.isEmpty())
        return;

    // Generate MTL path
    QString mtlPath = objPath;
    if (mtlPath.endsWith(".obj", Qt::CaseInsensitive)) {
        mtlPath = mtlPath.left(mtlPath.length() - 4) + ".mtl";
    } else {
        mtlPath += ".mtl";
    }

    // Export
    if (ltb.exportToOBJ(objPath, mtlPath)) {
        statusBar()->showMessage(QString("Exported: %1").arg(objPath), 5000);
        QMessageBox::information(this, "Export Successful",
                                 QString("Model exported successfully!\n\nOBJ: %1\nMTL: %2\n\nVertices: %3\nTriangles: %4")
                                     .arg(objPath)
                                     .arg(mtlPath)
                                     .arg(ltb.getTotalVertexCount())
                                     .arg(ltb.getTotalTriangleCount()));
    } else {
        QMessageBox::critical(this, "Export Failed", "Failed to export OBJ file.");
    }
}
