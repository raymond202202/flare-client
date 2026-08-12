#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

// Flare 客户端主窗口
// UI 铁律：浅色白底紫配（主色 #6d4aff），干净、轻量
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;
};

#endif // MAINWINDOW_H
