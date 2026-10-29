#ifndef BCPRDRIVER_H
#define BCPRDRIVER_H

#include <QObject>
#include <QSerialPort>
#include <QString>
#include <QTimer>
#include <optional>
#include <cstdint>


class BcprDriver : public QObject
{
    Q_OBJECT
    public:
        static constexpr qint32 DEFAULT_BAUDRATE = 9600;
        static constexpr double ENCODER_PPR    = 2500.0;
        static constexpr double ENCODER_CPR    = ENCODER_PPR * 4.0;      // 10000.0 cuentas/vuelta
        static constexpr double COUNTS_TO_RPM  = 6.0 / ENCODER_CPR;      // 0.0006
        static constexpr double RPM_TO_COUNTS  = ENCODER_CPR / 6.0;      // 1666.6667
        static constexpr double COUNTS_TO_DEG  = 360.0 / ENCODER_CPR;    // 0.036 grados/cuenta
        static constexpr double DEG_TO_COUNTS  = ENCODER_CPR / 360.0;    // 27.7778 cuentas/grado
        static constexpr double MAX_MOTOR_RPM  = 3000.0;                 // Velocidad nominal de placa
        static constexpr double CURRENT_TO_AMPS = 0.01;  // De registro a Amperes
        static constexpr double AMPS_TO_CURRENT = 100.0; // De Amperes a registro
        static constexpr double NOMINAL_MOTOR_AMPS = 20.0;

        // --- Direcciones del Mapa de Registros (Evita "números mágicos" en el .cpp) ---
        struct Reg {
            static constexpr uint8_t TargetCurrent     = 0x02; // Setpoint de corriente Modo 1 (0.01 A)
            static constexpr uint8_t CurrentRamp       = 0x03; // Rampa de corriente
            static constexpr uint8_t CommandedCurrent  = 0x0b; // Corriente salida del PI (0.01 A)
            static constexpr uint8_t ActualCurrent     = 0x0c; // Corriente real medida (0.01 A)
            static constexpr uint8_t ActualPosition    = 0x17; // Posición del encoder (counts)
            static constexpr uint8_t ActualVelocity    = 0x18; // Velocidad real (0.1 counts/s)
            static constexpr uint8_t BusVoltage        = 0x1e; // Tensión del bus DC (V)
            static constexpr uint8_t Temperature       = 0x20; // Temperatura del driver (°C enteros)
            static constexpr uint8_t PeakCurrentLimit  = 0x21; // Límite de corriente pico (0.01 A) -> 6000 = 60A
            static constexpr uint8_t ContCurrentLimit  = 0x22; // Límite de corriente continua (0.01 A) -> 3000 = 30A
            static constexpr uint8_t PeakCurrentTime   = 0x23; // Tiempo máximo en pico (ms) -> 6000 = 6000 ms
            static constexpr uint8_t ControlMode       = 0x24; // Modo de operación
            static constexpr uint8_t TargetVelocity    = 0x2f; // Consigna de velocidad Modo 11 (0.1 counts/s)
            static constexpr uint8_t FirmwareVersion   = 0x7f; // String de versión de firmware
            static constexpr uint8_t StatusFaults      = 0xa0; // Máscara de fallas y estado
            static constexpr uint8_t TargetPosition    = 0xc9; // Posición objetivo Modo 31 (counts)
        };

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
        bool connectDevice(const QString &portName, int timeoutMs = 200);
        void disconnectDevice();
        bool isConnected() const;
        void startTelemetry(int intervalMs = 200);
        void stopTelemetry();


        // Getters
        std::optional<int> getFirmwareId(int timeoutMs = 150);
        std::optional<double> getBusVoltage(int timeoutMs = 150);       // Retorna voltios
        std::optional<int64_t> getActualPosition(int timeoutMs = 150);  // Cuentas de encoder
        std::optional<int32_t> getActualVelocityRaw(int timeoutMs = 150); // 0.1 counts/s
        std::optional<double> getActualVelocityRpm(int timeoutMs = 150);
        std::optional<double> getActualCurrentAmps(int timeoutMs = 150);
        std::optional<int32_t> getActualPositionRaw(int timeoutMs = 150);
        std::optional<double> getActualPositionDegrees(int timeoutMs = 150);
        std::optional<StatusFaults> getStatusFaults(int timeoutMs = 150);
        std::optional<ControlMode> getControlMode(int timeoutMs = 150);
        std::optional<double> getPeakCurrentLimitAmps(int timeoutMs = 150);
        std::optional<double> getContCurrentLimitAmps(int timeoutMs = 150);

        // Setters
        bool setControlMode(int mode, int timeoutMs = 150);
        bool setTargetCurrentAmps(double amps, int timeoutMs = 150);
        bool setTargetVelocityRpm(double rpm, int timeoutMs = 150);
        bool setTargetPositionDegrees(double degrees, int timeoutMs = 150);

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
        void rpmSignal(double rpm);
        void currentSignal(int32_t current);
        void positionSignal(int64_t position);
        void peakCurrentLimitSignal(double peakAmps);
        void contCurrentLimitSignal(double contAmps);
        void errorOccurred(const QString &errorMessage);


    private slots:
        void pollTelemetry();
        void requestStaticParameters();


    private:
        QSerialPort *m_serial;
        QString transceive(const QString &cmd, int timeoutMs);
        QTimer *m_telemetryTimer;

};


#endif // BCPRDRIVER_H
