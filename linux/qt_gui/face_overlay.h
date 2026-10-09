#ifndef FACE_OVERLAY_H
#define FACE_OVERLAY_H
#include <QImage>
#include <QRectF>
#include <QVector>
// Input image and rectangles MUST describe the same captured frame.
QImage faceOverlay(const QImage &image, const QVector<QRectF> &boxes);
#endif
