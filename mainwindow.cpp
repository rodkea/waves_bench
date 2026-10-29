#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDateTime>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_serial(new QSerialPort(this))
{
    ui->setupUi(this);
    m_driver = new BcprDriver(this);
    setupConnections();
    escanearPuertos();
}

MainWindow::~MainWindow() {
    if (m_serial->isOpen()) {
        m_serial->close();
    }
    delete ui;
}

void MainWindow::log(const QString &mensaje) {
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->txt_log->appendPlainText(QString("[%1] %2").arg(ts, mensaje));
}

void MainWindow::escanearPuertos() {
    ui->cmb_puertos->clear();
    const auto puertos = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo &info : puertos) {
        QString etiqueta = QString("%1 - %2").arg(info.portName(), info.description());
        // Guardamos el portName limpio (ej. "COM3") en el QVariant (userData)
        ui->cmb_puertos->addItem(etiqueta, info.portName());
    }

    if (ui->cmb_puertos->count() == 0) {
        log("No se detectaron puertos serie activos.");
    } else {
        log(QString("Se detectaron %1 puerto(s) serie.").arg(ui->cmb_puertos->count()));
    }
}

void MainWindow::on_btn_limpiar_clicked() {
    ui->txt_log->clear();
}

void MainWindow::on_btn_conectar_clicked() {
    if (m_driver->isConnected()) {
        m_driver->disconnectDevice();
    } else {
        QString port = ui->cmb_puertos->currentData().toString();
        m_driver->connectDevice(port);
    }
}

void MainWindow::setupConnections() {
    connect(ui->btnRefrescar, &QPushButton::clicked, this, &MainWindow::escanearPuertos); // REEMPLAZAR CON EL on_btn_refrescar

    connect(m_driver, &BcprDriver::communicationLog, this, [this](const QString &dir, const QString &data) {
        log(QString("%1 -> %2").arg(dir, data));
    });
    connect(m_driver, &BcprDriver::connectionChanged, this, [this](bool connected) {
        actualizarUiEstado(connected, connected ? "ONLINE" : "DESCONECTADO", connected ? "#2e7d32" : "#555555");
    });

    connect(m_driver, &BcprDriver::busVoltageSignal, this, [this](double voltage) {
        ui->lb_voltage->setText(QString("Tensión Bus: %1 V").arg(voltage, 0, 'f', 1));
    });    
    connect(m_driver, &BcprDriver::rpmSignal, this, [this](double rpm) {
        ui->lb_rpm->setText(QString("Velocidad: %1 RPM").arg(rpm, 0, 'f', 1));
    });
    connect(m_driver, &BcprDriver::currentSignal, this, [this](double amps) {
        ui->lb_current->setText(QString("Corriente: %1 A").arg(amps, 0, 'f', 2));
    });
    connect(m_driver, &BcprDriver::positionSignal, this, [this](double degrees) {
        ui->lb_position->setText(QString("Posición: %1 °").arg(degrees, 0, 'f', 1));
    });
    connect(m_driver, &BcprDriver::peakCurrentLimitSignal, this, [this](double peakAmps) {
        ui->lb_max_peak_current->setText(QString("Corriente Máxima Pico: %1 A").arg(peakAmps, 0, 'f', 2));
    });

    // Conectamos la señal del límite continuo al label fijo
    connect(m_driver, &BcprDriver::contCurrentLimitSignal, this, [this](double contAmps) {
        ui->lb_max_current->setText(QString("Corriente Máxima: %1 A").arg(contAmps, 0, 'f', 2));
    });
}

QString MainWindow::transaccionSerial(const QString &cmd, int timeoutMs) {
    if (!m_serial->isOpen()) {
        return {};
    }

    m_serial->clear(QSerialPort::AllDirections);

    QByteArray trama = cmd.trimmed().toLatin1() + "\r";
    m_serial->write(trama);
    if (!m_serial->waitForBytesWritten(timeoutMs)) {
        return {};
    }

    // Leer fragmentos hasta recibir el retorno de carro '\r' del driver
    QByteArray buffer;
    while (m_serial->waitForReadyRead(timeoutMs)) {
        buffer.append(m_serial->readAll());
        if (buffer.contains('\r')) {
            break;
        }
    }

    return QString::fromLatin1(buffer).trimmed();
}

void MainWindow::toggle_led(bool connected) {
    if (connected) {
        ui->lb_led->setStyleSheet("background-color: rgb(0,255, 0); border-radius: 7px; border: 1px solid black;");
    } else {
        ui->lb_led->setStyleSheet("background-color: rgb(255,0, 0); border-radius: 7px; border: 1px solid black;");
    }
}

void MainWindow::actualizarUiEstado(bool conectado, const QString &texto, const QString &color) {
    ui->btn_conectar->setText(conectado ? "Desconectar" : "Conectar");
    ui->cmb_puertos->setEnabled(!conectado);
    ui->btnRefrescar->setEnabled(!conectado);
    toggle_led(conectado);
}

