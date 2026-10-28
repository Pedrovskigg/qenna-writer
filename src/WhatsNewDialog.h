#pragma once

#include <QDialog>

class QLabel;
class QTextBrowser;

// Janela "Novidades": o patch note da versão instalada, com o logo do splash
// em cima. Aparece uma vez, na primeira abertura depois de uma atualização.
//
// O texto vem de resources/whatsnew/whatsnew_<idioma>.md (embarcado no .qrc),
// não do GitHub: abre sem internet e já no idioma do app. A cada versão, é
// só trocar o conteúdo desses arquivos. Foto entra como imagem do markdown,
// ![legenda](qrc:/whatsnew/arquivo.jpg), com o arquivo no whatsnew.qrc em 2x:
// ela ocupa a largura do texto sem passar do tamanho dela.
class WhatsNewDialog : public QDialog {
    Q_OBJECT
public:
    explicit WhatsNewDialog(QWidget* parent = nullptr);

    // Chamado no main.cpp ANTES de qualquer outra coisa gravar no QSettings.
    // Instalação nova (registro vazio) não é atualização: marca a versão atual
    // como vista, e quem acabou de instalar não recebe o patch note na cara.
    static void markFreshInstall(bool settingsWereEmpty);

    // A versão instalada é mais nova que a última cuja janela foi mostrada?
    static bool shouldShow();
    static void markSeen();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyTheme();
    QString loadNotes() const;
    void fitImages();

    QLabel* m_logo = nullptr;
    QLabel* m_title = nullptr;
    QTextBrowser* m_body = nullptr;
};
