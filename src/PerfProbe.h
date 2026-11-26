#pragma once
// Cronômetro de desempenho (25/11/2026, teste do Pe num Pentium T3400 no Linux).
//
// Desligado por padrão. Com QENNA_PERF=1 o app mede quanto tempo cada evento
// leva para ser entregue (tecla, pintura de cada widget, timers, chamadas
// adiadas) e grava um resumo em ~/qenna-perf.txt a cada 3 segundos: os campeões
// de tempo somado e os eventos isolados mais lentos. Serve para achar o que pesa
// em máquina fraca sem chutar.

#include <QApplication>

class QennaApplication : public QApplication {
public:
    QennaApplication(int& argc, char** argv);
    bool notify(QObject* receiver, QEvent* event) override;

private:
    bool m_perf = false;
};
