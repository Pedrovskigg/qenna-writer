#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct Reminder {
    QString id;
    QString text;
    qint64 createdAt = 0;
    qint64 dueAt = 0;       // 0 = sem horário
    bool   notify = false;
    qint64 completedAt = 0; // 0 = ativo
    qint64 notifiedAt  = 0; // 0 = ainda não notificado
};

class RemindersStore : public QObject {
    Q_OBJECT
public:
    explicit RemindersStore(QObject* parent = nullptr);

    void setProjectRoot(const QString& root);
    bool load();
    bool save() const;

    const QVector<Reminder>& all() const { return m_reminders; }
    QVector<Reminder> active() const;
    QVector<Reminder> completed() const;
    bool hasActive() const;

    QString add(const QString& text, qint64 dueAt = 0, bool notify = false);
    bool complete(const QString& id);
    bool remove(const QString& id);
    bool setDue(const QString& id, qint64 dueAt, bool notify);
    bool markNotified(const QString& id);
    void clearCompleted();

    const Reminder* findById(const QString& id) const;

signals:
    void changed();

private:
    QString sidecarPath() const;

    QString         m_root;
    QVector<Reminder> m_reminders;
};
