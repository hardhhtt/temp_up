#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QTimer>
#include <QDataStream>
#include <QtEndian>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    this->setStyleSheet("background-color:rgb(220,226,241);");

    //按钮状态初始化
    ui->openBtn->setEnabled(true);
    ui->closeBtn->setEnabled(false);
    ui->startBtn->setEnabled(false);
    ui->stopBtn->setEnabled(false);

    QStringList serialPortName;
    foreach(const QSerialPortInfo &info,QSerialPortInfo::availablePorts()){
        serialPortName.append(info.portName()); //添加可用串口名
    }
    ui->portCom->addItems(serialPortName);
    serialPort = new QSerialPort(this); //创建串口对象

    qtimer = new QTimer;                //创建定时器对象
    qtimer->setInterval(1000);          //设置定时间隔为1s

    //初始化
    init();

    //使缩放或拖拽不会出现负值
    connect(ui->customplot->xAxis,SIGNAL(rangeChanged(QCPRange)),this,SLOT(setAxisRange(QCPRange)));
    connect(ui->customplot->yAxis,SIGNAL(rangeChanged(QCPRange)),this,SLOT(setAxisRange(QCPRange)));

    //接收串口数据
    connect(serialPort,SIGNAL(readyRead()),this,SLOT(receData()));
    connect(qtimer,SIGNAL(timeout()),this,SLOT(drawShow()));

    //按钮事件
    connect(ui->searchBtn,SIGNAL(clicked()),this,SLOT(on_searchButton()));
    connect(ui->openBtn,SIGNAL(clicked()),this,SLOT(on_openButton()));
    connect(ui->closeBtn,SIGNAL(clicked()),this,SLOT(on_closeButton()));
    connect(ui->startBtn,SIGNAL(clicked()),this,SLOT(on_startButton()));
    connect(ui->stopBtn,SIGNAL(clicked()),this,SLOT(on_stopButton()));
    connect(ui->exportBtn,SIGNAL(clicked()),this,SLOT(on_exportButton()));
    connect(ui->saveBtn,SIGNAL(clicked()),this,SLOT(on_saveButton()));
    connect(ui->clearBtn,SIGNAL(clicked()),this,SLOT(on_clearButton()));   

}

MainWindow::~MainWindow()
{
    delete ui;
}

//初始化
void MainWindow::init()
{
    //  **********绘图界面初始化**********
    //设置背景颜色
    ui->customplot->setBackground(QBrush(QColor(102,153,204)));
    //设置网格为灰色虚线
    ui->customplot->xAxis->grid()->setPen(QPen(QColor(180,180,180),1,Qt::PenStyle::DashLine));
    ui->customplot->yAxis->grid()->setPen(QPen(QColor(180,180,180),1,Qt::PenStyle::DashLine));
    //显示子网格线
    ui->customplot->xAxis->grid()->setSubGridVisible(true);
    ui->customplot->yAxis->grid()->setSubGridVisible(true);
    //设置坐标轴颜色
    ui->customplot->xAxis->setBasePen(QPen(Qt::black));
    ui->customplot->yAxis->setBasePen(QPen(Qt::black));
    //设置坐标轴终点图案为箭头
    ui->customplot->xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    ui->customplot->yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    //设置坐标轴刻度数目
    ui->customplot->xAxis->ticker()->setTickCount(5);
    ui->customplot->yAxis->ticker()->setTickCount(5);
    //设置可以缩放和拖拽
    ui->customplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    //设置x轴缩放比例为0.8,y轴缩放比例为1
    ui->customplot->axisRect()->setRangeZoomFactor(0.8,1);
    //生成时间刻度对象
    QSharedPointer<QCPAxisTickerDateTime> timeTicker(new QCPAxisTickerDateTime);
    //设置时间格式
    timeTicker->setDateTimeFormat("hh:mm:ss");
    //设置横坐标显示时间
    ui->customplot->xAxis->setTicker(timeTicker);
    //设置坐标标签
    ui->customplot->xAxis->setLabel("Time/s");
    ui->customplot->yAxis->setLabel("Temperature/℃");
    //设置坐标初始范围
    ui->customplot->xAxis->setRange(0,5);
    ui->customplot->yAxis->setRange(0,65);
    //设置画笔属性
    pen.setColor(Qt::red);
    pen.setWidthF(1.2);
    //添加曲线
    ui->customplot->addGraph();
    //设置曲线画笔
    ui->customplot->graph(0)->setPen(pen);

    //  **********LCD初始化**********
    ui->tempNumber->setDigitCount(5);                   //设置位数
    ui->tempNumber->setMode(QLCDNumber::Dec);           //十进制模式
    ui->tempNumber->setSmallDecimalPoint(true);         //显示小数点
    ui->tempNumber->setSegmentStyle(QLCDNumber::Flat);  //显示外观

    //  **********表格初始化**********
    ui->dataTable->setColumnCount(2);                   //设置为2列
    ui->dataTable->setAlternatingRowColors(true);       //设置隔行变颜色
    ui->dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);//选中整行
    //设置表头
    QStringList header;
    header << tr("时间") << tr("温度");
    ui->dataTable->setHorizontalHeaderLabels(header);
    ui->dataTable->setColumnWidth(0,80);               //设置第一列列宽
    ui->dataTable->setColumnWidth(1,80);                //设置第二列列宽
}

//表格插入数据
void MainWindow::insert(QString date, QString t)
{
    update();
    int row_count = ui->dataTable->rowCount();  //获取总行数，在此基础上插入一行
    ui->dataTable->insertRow(row_count);        //插入行
    QTableWidgetItem *item0 = new QTableWidgetItem();
    QTableWidgetItem *item1 = new QTableWidgetItem();
    item0->setText(date);
    item1->setText(t);
    ui->dataTable->setItem(row_count,0,item0);
    ui->dataTable->setItem(row_count,1,item1);
}

//x,y轴缩放或移动不会出现负值
void MainWindow::setAxisRange(QCPRange)
{
    if(ui->customplot->xAxis->range().lower < 0)  ui->customplot->xAxis->setRangeLower(0);
    if(ui->customplot->yAxis->range().lower < 0)  ui->customplot->yAxis->setRangeLower(0);
}

//接收数据槽函数
void MainWindow::receData()
{   
 // 获取温度并绘图
    QByteArray lastStr;

        QByteArray receiveBuff;
        QTime timer;
        timer.start();
        while (timer.elapsed() < 20) {
            if (serialPort->waitForReadyRead(15)) { // 2ms内有数据读取
                receiveBuff.append(serialPort->readAll());
            }
            //receiveBytes += receiveBuff.length();
        }
        qDebug() << "receive" << receiveBuff.toHex();
        if (receiveBuff.isEmpty()) {
            // 如果 receiveBuff 为空，则不进行任何操作
            receiveBuff.clear();     //清空缓存区
        } else {
        // 提取接收数据的第四字节后的两个字节
        quint8 startByte = 4;
        quint8 numBytes = 2;
        QByteArray valueByte = receiveBuff.mid(startByte, numBytes);
        qDebug() << "valueB" << valueByte.toHex(); // 以上正确
        // 将小端数据转化为大端数据
        quint16 valueBigEndian = qFromLittleEndian<quint16>(valueByte.constData());

        // 将大端数据转换为int型
        int valueB = static_cast<int>(valueBigEndian);
        // 温度值转换成浮点
        float value=0.0;
        value=valueB/100.00;
        qDebug() << "temperature:" << value;
        temperature=value;
        ui->tempNumber->display(temperature);}
        QDateTime time = QDateTime::currentDateTime();  //获取当前系统时间
        qreal key = time.toMSecsSinceEpoch()/1000;
        static qreal lastPointKey = 0;
        if(key - lastPointKey > 0.2){   //最多每2ms添加一个数据
            ui->customplot->graph(0)->addData(key,temperature);//添加数据
            lastPointKey = key;
        }

        ui->customplot->xAxis->setRange(key,10,Qt::AlignRight);//设置x轴范围，显示个数为10个

        ui->customplot->replot(QCustomPlot::rpQueuedReplot);

        QString date = time.toString("hh:mm:ss");          //设置显示格式
        QString tempe = QString("%1").arg(temperature);
        insert(date,tempe);

 //获取温度值并绘图
    receiveBuff.clear();     //清空缓存区
}

//搜索串口槽函数
void MainWindow::on_searchButton()
{
    int count = ui->portCom->count();
    if(count == 0){
        QStringList serialPortName;
        foreach(const QSerialPortInfo &info,QSerialPortInfo::availablePorts()){
            serialPortName.append(info.portName()); //添加可用串口名
        }
        ui->portCom->addItems(serialPortName);
    }
}

//打开串口按钮槽函数
void MainWindow::on_openButton()
{
    //波特率
    QSerialPort::BaudRate baudRate = QSerialPort::Baud9600;
    //数据位
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    //停止位
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    //记录所选参数
    QString baudText = ui->baudCom->currentText();
    QString dataText = ui->dataCom->currentText();
    QString stopText = ui->stopCom->currentText();

    if(baudText == "9600")    baudRate = QSerialPort::Baud9600;
    else if(baudText == "4800")    baudRate = QSerialPort::Baud4800;
    else if(baudText == "115200")    baudRate = QSerialPort::Baud115200;

    if(dataText == "8") dataBits = QSerialPort::Data8;
    else if(dataText == "5") dataBits = QSerialPort::Data5;
    else if(dataText == "6") dataBits = QSerialPort::Data6;
    else if(dataText == "7") dataBits = QSerialPort::Data7;

    if(stopText == "1") stopBits = QSerialPort::OneStop;
    else if(stopText == "1.5") stopBits = QSerialPort::OneAndHalfStop;
    else if(stopText == "2") stopBits = QSerialPort::TwoStop;

    //设置串口参数
    serialPort->setPortName(ui->portCom->currentText());
    serialPort->setBaudRate(baudRate);
    serialPort->setDataBits(dataBits);
    serialPort->setStopBits(stopBits);
    serialPort->setParity(QSerialPort::NoParity);

    if(serialPort->open(QIODevice::ReadWrite) == true){
        QMessageBox::information(this,"提示","串口打开成功！");
        //改变按钮状态
        ui->openBtn->setEnabled(false);
        ui->closeBtn->setEnabled(true);
        ui->startBtn->setEnabled(true);
        ui->stopBtn->setEnabled(true);
        // 创建定时器
        qtimer = new QTimer(this);
        qtimer->setInterval(1000); // 设置定时器时间间隔
        connect(qtimer, &QTimer::timeout, [=]() {
               QByteArray send_data = QByteArray::fromHex("FF 04 02 00 00 D0");//向串口定时发送读取命令，可以升级更改为由xml读取获得
               if (serialPort->write(send_data) == -1) {
                   qDebug() << "Failed to write data to port " << serialPort->portName() << ", error: " << serialPort->errorString();
               }
           });
        qtimer->start(); // 启动定时器

    }else{
        QMessageBox::warning(this,"提示","串口打开失败！");
    }  
}

//关闭串口按钮槽函数
void MainWindow::on_closeButton()
{
    serialPort->close();        //关闭串口
    // 停止定时器并释放资源
    qtimer->stop();
    qtimer->deleteLater();


    QMessageBox::information(this,"提示","串口已关闭！");
    ui->openBtn->setEnabled(true);
    ui->closeBtn->setEnabled(false);
    ui->startBtn->setEnabled(false);
    ui->stopBtn->setEnabled(false);
}

//开始绘图槽函数
void MainWindow::on_startButton()
{
    qtimer->start();
    ui->startBtn->setEnabled(false);
    ui->stopBtn->setEnabled(true);
}

//停止绘图槽函数
void MainWindow::on_stopButton()
{
    qtimer->stop();
    ui->startBtn->setEnabled(true);
    ui->stopBtn->setEnabled(false);
}

//导出图槽函数
void MainWindow::on_exportButton()
{
    QString filename = QFileDialog::getSaveFileName();
    if(filename == ""){
        QMessageBox::warning(this,"失败","保存失败！");
        return;
    }
    QMessageBox::information(this,"成功","保存成功！");
    ui->customplot->savePng(filename.append(".png"),ui->customplot->width(),ui->customplot->height());
}

//保存数据槽函数
void MainWindow::on_saveButton()
{
    QString filename = QFileDialog::getSaveFileName();
    //文件名
    QDateTime time = QDateTime::currentDateTime();  //获取系统当前时间
    QString date = time.toString("MM.dd.hh.mm.ss");          //设置显示格式
    filename += date;
    filename += ".txt";
    //创建文件对象
    QFile file(filename);
    if(!file.open(QFile::WriteOnly | QFile::Text)){ //只写方式
        QMessageBox::warning(this,tr("double file edit"),tr("no write").arg(filename).arg(file.errorString()));
        return;
    }
    //创建文件流对象
    QTextStream out(&file);
    out << "时间" << "\t\t" << "温度" << "\n";
    //遍历数据
    int romcount = ui->dataTable->rowCount();   //获取总行数
    for(int i = 0;i < romcount;i++){
        QString rowstring;
        for(int j = 0;j < 2;j++){
            rowstring += ui->dataTable->item(i,j)->text();
            rowstring += "      ";
        }
        rowstring += "\n";
        out << rowstring;
    }
    file.close();   //关闭文件
}

//清空文本槽函数
void MainWindow::on_clearButton()
{
    while(ui->dataTable->rowCount()){
        ui->dataTable->removeRow(0);
    }
}

//绘图并显示数据
void MainWindow::drawShow()
{
    QDateTime time = QDateTime::currentDateTime();  //获取当前系统时间
    qreal key = time.toMSecsSinceEpoch()/1000;
    static qreal lastPointKey = 0;
    if(key - lastPointKey > 0.2){   //最多每2ms添加一个数据
        ui->customplot->graph(0)->addData(key,temperature);//添加数据
        lastPointKey = key;
    }

    ui->customplot->xAxis->setRange(key,10,Qt::AlignRight);//设置x轴范围，显示个数为10个
    ui->customplot->replot(QCustomPlot::rpQueuedReplot);

    QString date = time.toString("hh:mm:ss");          //设置显示格式
    QString tempe = QString("%1").arg(temperature);
    insert(date,tempe);

}














