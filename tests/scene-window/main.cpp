#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QTimer>
#include <QWidget>

#include <cstdio>

namespace {

QImage renderScene(int scene) {
    QImage image(1280, 720, QImage::Format_RGB32);
    QPainter painter(&image);
    painter.fillRect(image.rect(), QColor(34, 34, 40));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(70, 70, 85));
    painter.drawRect(100, 400, 300, 320);
    painter.setBrush(QColor(120, 90, 60));
    painter.drawRect(500, 300, 200, 420);
    painter.setBrush(QColor(60, 100, 120));
    painter.drawRect(900, 150, 250, 180);
    painter.setBrush(QColor(200, 200, 210));
    painter.drawEllipse(QPoint(640, 120), 50, 50);

    if (scene == 1) {
        painter.setBrush(QColor(240, 210, 60));
        painter.drawEllipse(QPoint(950, 550), 60, 60);
    } else if (scene == 2) {
        painter.setBrush(QColor(60, 220, 200));
        painter.drawRect(150, 100, 90, 90);
    }
    return image;
}

}

class SceneWidget : public QWidget {
public:
    SceneWidget(const QString &scenePath, const QString &framePath)
        : scenePath_(scenePath), framePath_(framePath), buffer_(renderScene(0)) {
        setWindowTitle(QStringLiteral("AnomalySpotter Test Scene"));
        setFixedSize(buffer_.size());
        auto *timer = new QTimer(this);
        timer->setInterval(100);
        connect(timer, &QTimer::timeout, this, [this] { tick(); });
        timer->start();
        publishFrame();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.drawImage(0, 0, buffer_);
    }

private:
    void tick() {
        QFile sceneFile(scenePath_);
        int scene = scene_;
        if (sceneFile.open(QIODevice::ReadOnly))
            scene = QString::fromUtf8(sceneFile.readAll()).trimmed().toInt();
        if (scene != scene_) {
            scene_ = scene;
            buffer_ = renderScene(scene_);
            update();
            publishFrame();
        }
    }

    void publishFrame() {
        const QString tempPath = framePath_ + QStringLiteral(".tmp");
        if (buffer_.save(tempPath, "PNG"))
            std::rename(tempPath.toLocal8Bit().constData(),
                        framePath_.toLocal8Bit().constData());
    }

    QString scenePath_;
    QString framePath_;
    QImage buffer_;
    int scene_ = 0;
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (argc < 3) {
        fprintf(stderr, "usage: test-scene <scene-control-file> <frame-output.png>\n");
        return 1;
    }
    SceneWidget widget(QString::fromLocal8Bit(argv[1]), QString::fromLocal8Bit(argv[2]));
    widget.show();
    return app.exec();
}
