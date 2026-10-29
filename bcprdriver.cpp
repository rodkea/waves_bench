#include "bcprdriver.h"
#include <QByteArray>
#include <QStringList>


BcprDriver::BcprDriver(QObject *parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
    , m_telemetryTimer(new QTimer(this))
{
    connect(m_telemetryTimer, &QTimer::timeout, this, &BcprDriver::pollTelemetry);
}

BcprDriver::~BcprDriver() {
    disconnectDevice();
}


bool BcprDriver::connectDevice(const QString &portName, int timeoutMs) {
    if (m_serial->isOpen()) {
        disconnectDevice();
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(DEFAULT_BAUDRATE);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(QString("No se pudo abrir %1: %2").arg(portName, m_serial->errorString()));
        return false;
    }

    m_serial->clear(QSerialPort::AllDirections);

    // Ping de verificación consultando Firmware ID (registro 0x20)
    auto fw = getFirmwareId(timeoutMs);
    if (!fw.has_value()) {
        m_serial->close();
        emit errorOccurred("Puerto abierto pero el driver no respondió al handshake.");
        return false;
    }

    emit firmwareVersionSignal(QString("Versión Firmware: %1").arg(*fw));
    emit connectionChanged(true);

    // Leemos tensión del bus una vez al conectar y arrancamos telemetría
    getBusVoltage(timeoutMs);
    startTelemetry(200);
    pollTelemetry();
    requestStaticParameters();


    return true;
}


void BcprDriver::startTelemetry(int intervalMs) {
    if (m_serial->isOpen()) {
        m_telemetryTimer->start(intervalMs);
    }
}

void BcprDriver::stopTelemetry() {
    m_telemetryTimer->stop();
}


void BcprDriver::disconnectDevice() {
    stopTelemetry();
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
    auto val = readRegister(Reg::FirmwareVersion, timeoutMs);
    if (val.has_value()) {
        return static_cast<int>(*val);
    }
    return std::nullopt;
}

std::optional<double> BcprDriver::getBusVoltage(int timeoutMs) {
    auto val = readRegister(Reg::BusVoltage, timeoutMs); // Lee 0x1e -> devuelve 222
    if (val.has_value()) {
        double volts = static_cast<double>(*val) * 0.1; // 222 * 0.1 = 22.2 V
        emit busVoltageSignal(volts);
        return volts;
    }
    return std::nullopt;
}

std::optional<int32_t> BcprDriver::getActualVelocityRaw(int timeoutMs) {
    auto val = readRegister(Reg::ActualVelocity, timeoutMs);
    if (val.has_value()) {
        return static_cast<int32_t>(*val);
    }
    return std::nullopt;
}

std::optional<double> BcprDriver::getActualVelocityRpm(int timeoutMs) {
    auto rawVel = getActualVelocityRaw(timeoutMs);
    if (rawVel.has_value()) {
        double rpm = static_cast<double>(*rawVel) * COUNTS_TO_RPM;
        emit rpmSignal(rpm);
        return rpm;
    }
    return std::nullopt;
}

std::optional<double> BcprDriver::getActualCurrentAmps(int timeoutMs) {
    auto val = readRegister(Reg::ActualCurrent, timeoutMs);
    if (val.has_value()) {
        double amps = static_cast<double>(*val) * CURRENT_TO_AMPS; // Ej: 150 -> 1.50 A
        emit currentSignal(amps);
        return amps;
    }
    return std::nullopt;
}

// 2. Leer posición cruda en cuentas de encoder (Registro 0x17)
std::optional<int32_t> BcprDriver::getActualPositionRaw(int timeoutMs) {
    auto val = readRegister(Reg::ActualPosition, timeoutMs);
    if (val.has_value()) {
        return static_cast<int32_t>(*val);
    }
    return std::nullopt;
}

// 3. Leer posición convertida a grados o ángulo (Registro 0x17)
std::optional<double> BcprDriver::getActualPositionDegrees(int timeoutMs) {
    auto rawPos = getActualPositionRaw(timeoutMs);
    if (rawPos.has_value()) {
        double degrees = static_cast<double>(*rawPos) * COUNTS_TO_DEG;
        emit positionSignal(degrees);
        return degrees;
    }
    return std::nullopt;
}

// -- SETTERS --
// 1. Cambiar el modo de control (Corriente=1, Velocidad=11, Posición=31)
bool BcprDriver::setControlMode(int mode, int timeoutMs) {
    return writeRegister(Reg::ControlMode, mode, timeoutMs);
}

// 2. Modo Corriente: Setpoint en Amperes (ej: 2.5 A -> 250)
bool BcprDriver::setTargetCurrentAmps(double amps, int timeoutMs) {
    int64_t rawValue = std::llround(amps * 100.0); // 0.01 A por cuenta
    return writeRegister(Reg::TargetCurrent, rawValue, timeoutMs);
}

// 3. Modo Velocidad: Setpoint en RPM convertidas a 0.1 counts/s
// Factor: RPM * (10000 counts / 60 s) * 10 = RPM * 1666.6667
bool BcprDriver::setTargetVelocityRpm(double rpm, int timeoutMs) {
    int64_t rawValue = std::llround(rpm * (ENCODER_CPR / 6.0) * 10.0);
    return writeRegister(Reg::TargetVelocity, rawValue, timeoutMs);
}

// 4. Modo Posición: Setpoint en Grados convertidos a cuentas de encoder
// Factor: Grados * (10000 counts / 360°) = Grados * 27.7778
bool BcprDriver::setTargetPositionDegrees(double degrees, int timeoutMs) {
    int64_t rawValue = std::llround(degrees * (ENCODER_CPR / 360.0));
    return writeRegister(Reg::TargetPosition, rawValue, timeoutMs);
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

std::optional<double> BcprDriver::getPeakCurrentLimitAmps(int timeoutMs) {
    auto val = readRegister(Reg::PeakCurrentLimit, timeoutMs);
    if (val.has_value()) {
        return static_cast<double>(*val) * CURRENT_TO_AMPS; // Ej: 6000 -> 60.00 A
    }
    return std::nullopt;
}

// 2. Obtener Límite de Corriente Continua (Registro 0x22)
std::optional<double> BcprDriver::getContCurrentLimitAmps(int timeoutMs) {
    auto val = readRegister(Reg::ContCurrentLimit, timeoutMs);
    if (val.has_value()) {
        return static_cast<double>(*val) * CURRENT_TO_AMPS; // Ej: 3000 -> 30.00 A
    }
    return std::nullopt;
}

void BcprDriver::pollTelemetry() {
    getActualVelocityRpm();
    getBusVoltage();
    getActualCurrentAmps();
    getActualPositionDegrees();
}

void BcprDriver::requestStaticParameters() {
    auto peak = getPeakCurrentLimitAmps();
    if (peak.has_value()) {
        emit peakCurrentLimitSignal(*peak);
    }

    auto cont = getContCurrentLimitAmps();
    if (cont.has_value()) {
        emit contCurrentLimitSignal(*cont);
    }
}

bool BcprDriver::writeRegister(uint8_t regId, int64_t value, int timeoutMs) {
    if (!m_serial->isOpen()) {
        return false;
    }

    // 1. Construir la trama con formato hexadecimal de 2 dígitos para el registro
    // Ejemplo: reg = 0x02, value = 250 -> "s r0x02 250\r"
    QString command = QString("s r0x%1 %2\r")
                          .arg(regId, 2, 16, QChar('0'))
                          .arg(value);

    // 2. Transmitir el comando y esperar la respuesta del driver
    QString response = transceive(command, timeoutMs);

    // 3. Validar la respuesta
    // Los drivers Copley suelen responder "ok" cuando la escritura es exitosa.
    // Algunos firmwares OEM pueden devolver una cadena vacía o "ok".
    return response.contains("ok", Qt::CaseInsensitive) || response.trimmed().isEmpty();
}