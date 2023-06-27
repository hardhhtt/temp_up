#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPen>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QFileDialog>
#include "qcustomplot.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void init();                            //初始化

    void insert(QString date,QString t);    //表格插入数据

private slots:
    void setAxisRange(QCPRange);//x,y轴缩放或移动不会出现负值

    void receData();            //接收数据槽函数

    void on_searchButton();     //搜索串口槽函数

    void on_openButton();       //打开串口槽函数

    void on_closeButton();      //关闭串口槽函数

    void on_startButton();      //开始绘图槽函数

    void on_stopButton();       //停止绘图槽函数

    void on_exportButton();     //导出图槽函数

    void on_saveButton();       //保存数据槽函数

    void on_clearButton();      //清空文本槽函数

    void drawShow();            //绘图并显示数据

private:
    Ui::MainWindow *ui;

    QPen pen;
    QSerialPort *serialPort;
    qreal temperature = 0;      //获取温度
    QTimer *qtimer;             //定时器
};
#endif // MAINWINDOW_H
