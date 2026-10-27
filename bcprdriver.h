#ifndef BCPRDRIVER_H
#define BCPRDRIVER_H

#include <QObject>
#include <QSerialPort>
#include <QString>
#include <optional>
#include <cstdint>

class BcprDriver : public QObject
{
    Q_OBJECT
    public:
        enum class ControlMode : int {
            Disabled = 0,
            Torque = 1,
            Velocidad = 11,
            Pocision = 31
        };
        struct StatusFaults {
            uint32_t rawValue{0};
            bool softwareDisabled{false};
            bool hardwareEnableLow{false};
            bool underVoltage{false};
            bool feedbackError{false};
        };

        explicit BcprDriver(QObject *parent = nullptr);
        ~BcprDriver() override;

        // Conexión
        bool connectDevice(const QString &portName, qint32 baudRate = 9600, int timeoutMs = 250);
        void disconnectDevice();
        bool isConnected() const;


        // Getters
        std::optional<int> getFirmwareId(int timeoutMs = 150);
        std::optional<double> getBusVoltage(int timeoutMs = 150);       // Retorna voltios
        std::optional<double> getTemperature(int timeoutMs = 150);      // Retorna °C
        std::optional<int64_t> getActualPosition(int timeoutMs = 150);  // Cuentas de encoder
        std::optional<int32_t> getActualRpm(int timeoutMs = 150);  // Cuentas/segundo
        std::optional<int32_t> getActualCurrent(int timeoutMs = 150);   // Corriente real (mA o cuentas)
        std::optional<StatusFaults> getStatusFaults(int timeoutMs = 150);
        std::optional<ControlMode> getControlMode(int timeoutMs = 150);

        // Comunicación genérica de bajo nivel
        std::optional<QString> sendCommand(const QString &cmd, int timeoutMs = 150);
        std::optional<int64_t> readRegister(uint8_t regId, int timeoutMs = 150);
        bool writeRegister(uint8_t regId, int64_t value, int timeoutMs = 150);

    signals:
        void connectionChanged(bool connected);
        void communicationLog(const QString &direction, const QString &data);
        void firmwareVersionSignal(const QString &version);
        void busVoltageSignal(double voltage);
        void temperatureSignal(double tempature);
        void velocitySignal(int32_t velocity);
        void currentSignal(int32_t current);
        void positionSignal(int64_t position);
        void errorOccurred(const QString &errorMessage);

    private:
        QSerialPort *m_serial;
        QString transceive(const QString &cmd, int timeoutMs);


};


#endif // BCPRDRIVER_H
