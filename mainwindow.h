#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QTreeWidget>
#include <QSplitter>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QMap>
#include <QTabWidget>
#include <QScrollArea>
#include <QPushButton>
#include "rezarchive.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openArchive();
    void saveArchive();
    void closeArchive();
    void extractSelectedFile();
    void extractAllFiles();
    void searchFiles();
    void onFileDoubleClicked(QTreeWidgetItem *item, int column);
    void onFileSelectionChanged();
    void exportCurrentImage();

    //  Context menu
    void showContextMenu(const QPoint &pos);
    void exportLTBToOBJ();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void loadArchiveToTree();

    // Preview functions
    void displayTextFile(const QString &filename, const QByteArray &data);
    void displayImageFile(const QString &filename, const QByteArray &data);
    void displayLTBFile(const QString &filename, const QByteArray &data);
    void displayFileInfo(const RezFileEntry &entry);
    void clearPreviews();

    // Helper functions
    QString formatFileSize(quint32 size);
    QString formatDateTime(quint32 timestamp);
    QIcon getIconForExtension(const QString &ext);
    QString toNativePath(const QString &rezPath);

    // UI elements - Left panel
    QSplitter *mainSplitter;
    QTreeWidget *fileTree;
    QLineEdit *searchBox;

    // UI elements - Right panel (Tab Widget)
    QTabWidget *tabWidget;

    // Text tab
    QWidget *textTab;
    QPlainTextEdit *textPreview;

    // Image tab
    QWidget *imageTab;
    QScrollArea *imageScrollArea;
    QLabel *imagePreview;
    QPushButton *exportImageButton;
    QImage currentImage;  // Store current decoded image for export
    QString currentImageName;  // Store current image filename

    // Info tab
    QWidget *infoTab;
    QLabel *fileInfoLabel;

    // Progress bar
    QProgressBar *progressBar;

    // Actions
    QAction *actionOpen;
    QAction *actionSave;
    QAction *actionClose;
    QAction *actionExtractFile;
    QAction *actionExtractAll;

    // Data
    RezArchive *archive;
    QString currentFilePath;
    QMap<QString, QByteArray> modifiedFiles;
    bool isModified;

    //  LTB

    QByteArray currentLTBData;
    QString currentLTBName;
};

#endif // MAINWINDOW_H
