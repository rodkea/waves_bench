#pragma once

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include "bcprdriver.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void escanearPuertos();
    void on_btn_limpiar_clicked();
    void on_btn_conectar_clicked();



private:
    Ui::MainWindow *ui;
    QSerialPort *m_serial;
    BcprDriver *m_driver;
    void log(const QString &mensaje);
    void toggle_led(bool connected);
    QString transaccionSerial(const QString &cmd, int timeoutMs = 200);
    void actualizarUiEstado(bool conectado,
                            const QString &texto = "DESCONECTADO",
                            const QString &color = "#555555");
    void setupConnections();
};
