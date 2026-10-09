#include "face_overlay.h"
#include <QPainter>
#include <cmath>
QImage faceOverlay(const QImage &image, const QVector<QRectF> &boxes) {
    QImage out=image.copy();
    if(out.isNull()) return out;
    QPainter p(&out); p.setBrush(Qt::NoBrush);
    p.setPen(QPen(boxes.size()==1 ? QColor("#34d399") : QColor("#f59e0b"),3));
    for(const auto &box:boxes) {
        if(!std::isfinite(box.x()) || !std::isfinite(box.y()) || !std::isfinite(box.width()) ||
           !std::isfinite(box.height()) || box.width()<=0 || box.height()<=0) continue;
        const auto clipped=box.intersected(QRectF(0,0,image.width()-1,image.height()-1));
        if(!clipped.isEmpty()) p.drawRect(clipped);
    }
    return out;
}
