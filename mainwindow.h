#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QFileSystemModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLabel> // Tambah ini

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFolder();
    void openTemplateOptions();
    void showAbout();

    void selectAllFiles();
    void deselectAllFiles();

    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);

    void copyDirectoryTree();
    void copyFileContent();
    void copyFullContext();

    void checkUpdate();
    void onUpdateResult(QNetworkReply *reply);

    void openRecentProject();
    void clearRecentList();

private:
    Ui::MainWindow *ui;
    QString currentRootDir;
    QString contentTemplate;
    const QString defaultTemplate = "File: {name}\n```\n{code}\n```\n";

    // --- TAMBAHAN VARIABEL LABEL ---
    QLabel *statusPathLabel;
    // -------------------------------

    QStringList recentFiles;
    const int maxRecentFiles = 10;

    QNetworkAccessManager *netManager;
    const QString currentVersion = "v0.3.0";

    void populateTree(const QString &path, QTreeWidgetItem *parentItem);
    void setAllChildCheckState(QTreeWidgetItem *item, Qt::CheckState state);
    void updateFileList(QTreeWidgetItem *item);
    QString generateAsciiTree(const QString &path, const QString &prefix);

    void loadProject(const QString &path);
    void addToRecent(const QString &path);
    void updateRecentMenu();
};

#endif
