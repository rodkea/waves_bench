#include "bcprdriver.h"
#include <QByteArray>
#include <QStringList>


BcprDriver::BcprDriver(QObject *parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
{
}

BcprDriver::~BcprDriver() {
    disconnectDevice();
}


bool BcprDriver::connectDevice(const QString &portName, qint32 baudRate, int timeoutMs) {
    if (m_serial->isOpen()) {
        disconnectDevice();
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(QString("No se pudo abrir %1: %2").arg(portName, m_serial->errorString()));
        return false;
    }

    // Ping de verificación consultando Firmware ID (registro 0x20)
    auto fw = getFirmwareId(timeoutMs);
    if (!fw.has_value()) {
        m_serial->close();
        emit errorOccurred("Puerto abierto pero el driver no respondió al handshake.");
        return false;
    }
    getBusVoltage();
    getTemperature();
    getActualVelocity();
    emit firmwareVersionSignal(QString("Versión Firmware: %1").arg(*fw));

    emit connectionChanged(true);
    return true;
}

void BcprDriver::disconnectDevice() {
    if (m_serial->isOpen()) {
        m_serial->close();
        emit connectionChanged(false);
    }
}

bool BcprDriver::isConnected() const {
    return m_serial->isOpen();
}

std::optional<int64_t> BcprDriver::readRegister(uint8_t regId, int timeoutMs) {
    QString cmd = QString("g r0x%1").arg(regId, 2, 16, QChar('0'));
    QString resp = transceive(cmd, timeoutMs);

    if (resp.startsWith("v")) {
        bool ok = false;
        int64_t val = resp.mid(1).trimmed().toLongLong(&ok);
        if (ok) {
            return val;
        }
    }
    return std::nullopt;
}

// --- GETTERS ---
std::optional<int> BcprDriver::getFirmwareId(int timeoutMs) {
    auto val = readRegister(0x20, timeoutMs);
    if (val.has_value()) {
        return static_cast<int>(*val);
    }
    return std::nullopt;
}

std::optional<double> BcprDriver::getBusVoltage(int timeoutMs) {
    auto val = readRegister(0x1e, timeoutMs);
    if (val.has_value()) {
        double volts = static_cast<double>(*val); // Tensión en V
        emit busVoltageSignal(volts);
        return volts;
    }
    return std::nullopt;
}

std::optional<double> BcprDriver::getTemperature(int timeoutMs) {
    auto val = readRegister(0x23, timeoutMs);
    if (val.has_value()) {
        double temp = static_cast<double>(*val) / 100.0;
        emit temperatureSignal(temp);
        return temp; // Escalado estándar a °C
    }
    return std::nullopt;
}

std::optional<int32_t> BcprDriver::getActualVelocity(int timeoutMs) {
    auto val = readRegister(0x18, timeoutMs);
    if (val.has_value()) {
        int32_t velocity = static_cast<int32_t>(*val);
        emit velocitySignal(velocity);
        return velocity;
    }
    return std::nullopt;
}


QString BcprDriver::transceive(const QString &cmd, int timeoutMs) {
    if (!m_serial->isOpen()) {
        return {};
    }

    m_serial->clear(QSerialPort::AllDirections);

    QByteArray outPacket = cmd.trimmed().toLatin1() + "\r";
    emit communicationLog("TX", QString::fromLatin1(outPacket).trimmed());

    m_serial->write(outPacket);
    if (!m_serial->waitForBytesWritten(timeoutMs)) {
        return {};
    }

    QByteArray buffer;
    while (m_serial->waitForReadyRead(timeoutMs)) {
        buffer.append(m_serial->readAll());
        if (buffer.contains('\r')) {
            break;
        }
    }

    QString inPacket = QString::fromLatin1(buffer).trimmed();
    emit communicationLog("RX", inPacket.isEmpty() ? "<Timeout>" : inPacket);
    return inPacket;
}