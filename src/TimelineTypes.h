#pragma once

#include <QColor>
#include <QString>

struct TimelineDef {
    QString id;
    QString name;
    QColor  color = QColor(QStringLiteral("#6c8ebf"));
};

struct TimelineEvent {
    QString id;
    QString timelineId;
    QString title;
    QString description;
    QString shape       = QStringLiteral("square"); // square | circle | triangle | diamond
    QColor  color;          // inválida = herda da timeline
    qreal   x           = 0;
    qreal   y           = 0;
    QString timeMarker; // ex: "Dia 5", "05/07/XXXX"
    QString linkedSceneId;
    QString linkedDocId;
    QString conclusion; // resumo da cena (texto livre)
};

struct TimelineConn {
    QString id;
    QString fromEventId;
    QString toEventId;
    // cor = mix RGB das timelines dos dois eventos — calculada na hora de desenhar
};
