#pragma once

#include <QObject>
#include <QString>
#include <QVector>

// Notas soltas do projeto — a aba "Notas" do Pensário. Cada nota é cor + texto
// livre. Persistência via sidecar JSON `notes.json` no root do projeto. Salva
// imediatamente a cada mutação (ações explícitas do usuário).
class NotesStore : public QObject {
    Q_OBJECT
public:
    struct Note {
        QString id;
        QString color;     // "#RRGGBB"
        QString title;
        QString text;
        qint64 createdAt = 0; // epoch ms
    };

    explicit NotesStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QVector<Note>& notes() const { return m_notes; }

    QString addNote(const QString& color, const QString& title, const QString& text); // devolve id novo
    bool updateNote(const QString& id, const QString& color, const QString& title, const QString& text);
    bool removeNote(const QString& id);

signals:
    void notesChanged();

private:
    QString sidecarPath() const;

    QString m_root;
    QVector<Note> m_notes;
};
