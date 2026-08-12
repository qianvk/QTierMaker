#include "widgets/WrappingLabel.h"

#include <QApplication>
#include <QtTest>

using namespace qtm;

class tst_WrappingLabel : public QObject {
    Q_OBJECT

private slots:
    void heightTracksAssignedWidth() {
        WrappingLabel label(
            QStringLiteral("Download the sample project to explore QTierMaker, or open one of "
                           "your own."));
        label.setTextFormat(Qt::PlainText);
        label.setAlignment(Qt::AlignCenter);
        label.setContentsMargins(6, 4, 6, 4);
        label.show();

        for (const int width : {180, 320, 520}) {
            label.resize(width, 1);
            QApplication::processEvents();
            QCOMPARE(label.minimumHeight(), label.heightForWidth(width));
            QVERIFY(label.height() >= label.heightForWidth(width));
        }
    }

    void textAndFontChangesRefreshHeight() {
        WrappingLabel label(QStringLiteral("Short text"));
        label.setContentsMargins(6, 4, 6, 4);
        label.resize(180, 1);
        label.show();
        QApplication::processEvents();
        const int shortHeight = label.minimumHeight();

        QFont larger = label.font();
        larger.setPointSize(larger.pointSize() + 4);
        label.setFont(larger);
        label.setText(QStringLiteral("A longer description that wraps over several lines at the "
                                     "assigned width without clipping its top or bottom."));
        QApplication::processEvents();

        QVERIFY(label.minimumHeight() > shortHeight);
        QCOMPARE(label.minimumHeight(), label.heightForWidth(label.width()));
    }
};

QTEST_MAIN(tst_WrappingLabel)
#include "tst_WrappingLabel.moc"
